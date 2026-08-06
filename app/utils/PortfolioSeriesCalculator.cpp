// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "PortfolioSeriesCalculator.h"
#include "ShareCalculator.h"

#include <algorithm>

namespace {

/// Rounding is delegated to ShareCalculator so the whole project shares one
/// cent semantics (half away from zero, 2 decimals).
inline double round2(double value)
{
    return ShareCalculator::roundAway(value);
}

/// A still-open buy position. Sales consume these in insertion order, which
/// is buy-date order after sorting — that is the FIFO rule.
struct Lot
{
    double remaining = 0.0;
    double price     = 0.0;
};

/**
 * Drops every entry without a valid date and reports how many were removed.
 *
 * An invalid QDate compares LESS than any valid one, so leaving such entries
 * in would make the "date <= stichtag" loops book them all at the very first
 * grid point (Bugfix 06.08.2026, see header).
 */
template <typename T>
QList<T> dropInvalidDates(const QList<T>& source, int& invalidCounter)
{
    QList<T> result;
    result.reserve(source.size());
    for (const T& entry : source) {
        if (entry.date.isValid())
            result.append(entry);
        else
            ++invalidCounter;
    }
    return result;
}

/// True when @p date lies inside the (possibly open) window [from, to].
bool inWindow(const QDate& date, const QDate& from, const QDate& to)
{
    if (!date.isValid())
        return false;
    if (from.isValid() && date < from)
        return false;
    if (to.isValid() && date > to)
        return false;
    return true;
}

} // namespace

// -- buildDateGrid -------------------------------------------------------------

QList<QDate> PortfolioSeriesCalculator::buildDateGrid(
    const QList<PortfolioShareSeriesInput>& shares,
    const QDate& from,
    const QDate& to)
{
    QList<QDate> dates;

    for (const PortfolioShareSeriesInput& share : shares) {
        // A share without any price history is excluded entirely (see header),
        // so it must not contribute grid dates either.
        if (share.prices.isEmpty())
            continue;

        // inWindow() liefert für ungültige Datumsangaben bereits false, sie
        // können also gar nicht erst ins Raster gelangen.
        for (const PortfolioPriceEvent& price : share.prices)
            if (inWindow(price.date, from, to))
                dates.append(price.date);

        for (const PortfolioBuyEvent& buy : share.buys)
            if (inWindow(buy.date, from, to))
                dates.append(buy.date);

        for (const PortfolioSaleEvent& sale : share.sales)
            if (inWindow(sale.date, from, to))
                dates.append(sale.date);

        for (const PortfolioDividendEvent& dividend : share.dividends)
            if (inWindow(dividend.date, from, to))
                dates.append(dividend.date);

        for (const PortfolioCostEvent& cost : share.costs)
            if (inWindow(cost.date, from, to))
                dates.append(cost.date);
    }

    std::sort(dates.begin(), dates.end());
    dates.erase(std::unique(dates.begin(), dates.end()), dates.end());
    return dates;
}

// -- closingPriceAt ------------------------------------------------------------

double PortfolioSeriesCalculator::closingPriceAt(const QList<PortfolioPriceEvent>& prices,
                                                 const QDate& date)
{
    double result = 0.0;
    for (const PortfolioPriceEvent& price : prices) {
        if (price.date > date)
            break; // list is sorted ascending — nothing later can qualify
        result = price.closingPrice;
    }
    return result;
}

// -- compute -------------------------------------------------------------------

PortfolioSeriesResult PortfolioSeriesCalculator::compute(
    const QList<PortfolioShareSeriesInput>& shares,
    const QDate& from,
    const QDate& to,
    bool withPerShareDetail)
{
    PortfolioSeriesResult result;

    const QList<QDate> grid = buildDateGrid(shares, from, to);

    // Prepare one point per grid date; the per-share sweep below accumulates
    // into these.
    result.points.reserve(grid.size());
    for (const QDate& date : grid) {
        PortfolioSeriesPoint point;
        point.date = date;
        result.points.append(point);
    }

    for (const PortfolioShareSeriesInput& share : shares) {

        PortfolioShareDiagnostics diag;
        diag.name = share.name.isEmpty() ? share.shareGuid : share.name;

        // Working copies without invalid dates — callers may also pass
        // unsorted lists, sorting follows below.
        QList<PortfolioBuyEvent>      buys      = dropInvalidDates(share.buys,      diag.invalidDates);
        QList<PortfolioSaleEvent>     sales     = dropInvalidDates(share.sales,     diag.invalidDates);
        QList<PortfolioDividendEvent> dividends = dropInvalidDates(share.dividends, diag.invalidDates);
        QList<PortfolioCostEvent>     costs     = dropInvalidDates(share.costs,     diag.invalidDates);
        QList<PortfolioPriceEvent>    prices    = dropInvalidDates(share.prices,    diag.invalidDates);

        diag.buys      = buys.size();
        diag.sales     = sales.size();
        diag.dividends = dividends.size();
        diag.costs     = costs.size();
        diag.prices    = prices.size();

        // Shares without usable price history cannot be valued on any date.
        // Reporting them and skipping them completely is the only consistent
        // option — counting their purchase value without their holdings value
        // would show a loss that does not exist (Nessies Vorgabe 05.08.2026).
        if (prices.isEmpty()) {
            diag.excluded = true;
            result.sharesWithoutHistory.append(diag.name);
            result.diagnostics.append(diag);
            continue;
        }

        const auto byDate = [](const auto& a, const auto& b) { return a.date < b.date; };
        std::sort(buys.begin(),      buys.end(),      byDate);
        std::sort(sales.begin(),     sales.end(),     byDate);
        std::sort(dividends.begin(), dividends.end(), byDate);
        std::sort(costs.begin(),     costs.end(),     byDate);
        std::sort(prices.begin(),    prices.end(),    byDate);

        const QDate firstPriceDate = prices.constFirst().date;

        diag.firstPrice = firstPriceDate;
        diag.lastPrice  = prices.constLast().date;
        if (!buys.isEmpty())
            diag.firstBuy = buys.constFirst().date;
        result.diagnostics.append(diag);

        // Running state, advanced monotonically as the sweep walks the grid.
        QList<Lot> lots;
        int buyIdx = 0, saleIdx = 0, divIdx = 0, costIdx = 0, priceIdx = 0;

        double heldVolume         = 0.0;
        double purchaseValueHeld  = 0.0; // sum over lots of round(remaining x price)
        double purchaseValueTotal = 0.0; // sum over all buys of round(volume x price)
        double realizedGain       = 0.0;
        double dividendSum        = 0.0;
        double costSum            = 0.0;
        double lastPrice          = 0.0;

        for (int gridIdx = 0; gridIdx < grid.size(); ++gridIdx) {
            const QDate stichtag = grid.at(gridIdx);

            // -- Buys up to and including the current date -----------------
            while (buyIdx < buys.size() && buys.at(buyIdx).date <= stichtag) {
                const PortfolioBuyEvent& buy = buys.at(buyIdx);
                const double buyValue = round2(buy.volume * buy.price);

                lots.append(Lot{ buy.volume, buy.price });
                heldVolume         += buy.volume;
                purchaseValueHeld  += buyValue;
                purchaseValueTotal += buyValue;
                ++buyIdx;
            }

            // -- Sales up to and including the current date -----------------
            while (saleIdx < sales.size() && sales.at(saleIdx).date <= stichtag) {
                const PortfolioSaleEvent& sale = sales.at(saleIdx);

                double open     = sale.volume; // still to be assigned to lots
                double soldCost = 0.0;         // buy cost of the sold shares

                for (int lotIdx = 0; lotIdx < lots.size() && open > 0.0; ++lotIdx) {
                    Lot& lot = lots[lotIdx];
                    if (lot.remaining <= 0.0)
                        continue;

                    const double take = std::min(lot.remaining, open);

                    // Keep purchaseValueHeld exactly equal to the sum of the
                    // rounded per-lot contributions by replacing this lot's
                    // contribution rather than subtracting an unrounded delta.
                    const double before = round2(lot.remaining * lot.price);
                    lot.remaining -= take;
                    const double after = round2(lot.remaining * lot.price);
                    purchaseValueHeld -= (before - after);

                    soldCost += round2(take * lot.price);
                    open     -= take;
                }

                // `open` is normally 0. Anything left over means the sale had
                // no matching buy volume; the held volume is then reduced only
                // by what could actually be assigned, so the state stays
                // consistent instead of going negative.
                heldVolume -= (sale.volume - open);

                const double netProceeds = round2(round2(sale.volume * sale.price)
                                                  - sale.taxSum);
                realizedGain += netProceeds - soldCost;
                ++saleIdx;
            }

            // -- Dividends up to and including the current date -------------
            while (divIdx < dividends.size() && dividends.at(divIdx).date <= stichtag) {
                dividendSum += dividends.at(divIdx).payoutWithTaxes;
                ++divIdx;
            }

            // -- Costs up to and including the current date -----------------
            while (costIdx < costs.size() && costs.at(costIdx).date <= stichtag) {
                costSum += costs.at(costIdx).amount;
                ++costIdx;
            }

            // -- Forward-filled closing price ------------------------------
            while (priceIdx < prices.size() && prices.at(priceIdx).date <= stichtag) {
                lastPrice = prices.at(priceIdx).closingPrice;
                ++priceIdx;
            }

            // Before the share's first price date it contributes nothing at
            // all — not even its purchase value. Events above are still
            // processed so the running state is correct once the share does
            // start contributing.
            if (stichtag < firstPriceDate)
                continue;

            if (withPerShareDetail) {
                PortfolioSharePoint detail;
                detail.date              = stichtag;
                detail.name              = diag.name;
                detail.volume            = heldVolume;
                detail.price             = lastPrice;
                detail.holdingsValue     = round2(heldVolume * lastPrice);
                detail.purchaseValueHeld = purchaseValueHeld;
                detail.realizedGain      = realizedGain;
                detail.dividends         = dividendSum;
                detail.costs             = costSum;
                result.sharePoints.append(detail);
            }

            PortfolioSeriesPoint& point = result.points[gridIdx];
            point.holdingsValue      += round2(heldVolume * lastPrice);
            point.realizedGain       += realizedGain;
            point.dividends          += dividendSum;
            point.costs              += costSum;
            point.purchaseValueHeld  += purchaseValueHeld;
            point.purchaseValueTotal += purchaseValueTotal;
        }
    }

    // -- Derive the displayed line from its components -------------------------
    for (PortfolioSeriesPoint& point : result.points) {
        point.development = round2(point.holdingsValue
                                   + point.realizedGain
                                   + point.dividends
                                   - point.costs
                                   - point.purchaseValueHeld);

        // Denominator is the cumulative buy value of ALL buys up to the
        // stichtag, not just the held ones (Nessies Vorgabe 05.08.2026): after
        // a complete sale the held value is 0, which would force the percentage
        // to 0 even though the line still shows the realized gain. Same guard
        // pattern as ShareCalculator (`> 0.0` before dividing).
        point.developmentPct = (point.purchaseValueTotal > 0.0)
                               ? (point.development / point.purchaseValueTotal * 100.0)
                               : 0.0;
    }

    return result;
}

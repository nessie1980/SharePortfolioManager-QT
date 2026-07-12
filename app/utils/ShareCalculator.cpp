// MIT License -- spm-qt
#include "ShareCalculator.h"
#include "../repositories/BuyRepository.h"
#include "../repositories/SaleRepository.h"
#include "../repositories/DividendRepository.h"
#include "../repositories/BrokerageRepository.h"
#include "../models/BuyObject.h"
#include "../models/SaleObject.h"
#include "../models/BrokerageObject.h"

#include <QList>
#include <QHash>
#include <cmath>

// -- roundAway -----------------------------------------------------------------

double ShareCalculator::roundAway(double value, int digits)
{
    const double factor = std::pow(10.0, digits);
    const double scaled = value * factor;
    const double sign   = (scaled < 0.0) ? -1.0 : 1.0;
    // +1e-9 neutralises binary representation error at exact half-cent
    // boundaries so the result matches C# decimal half-away-from-zero rounding.
    return sign * std::floor(std::abs(scaled) + 0.5 + 1e-9) / factor;
}

// -- compute -------------------------------------------------------------------

ShareValues ShareCalculator::compute(const QString& guid,
                                     double curPrice,
                                     double prevDayPrice)
{
    ShareValues v;
    v.curPrice     = curPrice;
    v.prevDayPrice = prevDayPrice;

    BuyRepository       buyRepo;
    SaleRepository      saleRepo;
    DividendRepository  divRepo;
    BrokerageRepository brokerageRepo;

    // -- Previous day ------------------------------------------------------
    v.prevDayDiff = curPrice - prevDayPrice;
    v.prevDayPct  = (prevDayPrice > 0.0)
                    ? (v.prevDayDiff / prevDayPrice * 100.0)
                    : 0.0;

    // -- Load all buys and sales -------------------------------------------
    const QList<BuyObject>  buys  = buyRepo.findByShare(guid);
    const QList<SaleObject> sales = saleRepo.findByShare(guid);

    // -- Held cost basis (current columns) ---------------------------------
    //
    // Both bases use the REMAINING volume of each buy. The Marktwert basis
    // omits brokerage; the Depotwert basis includes it. Reduction (Rabatt) is
    // included in both. Brokerage/reduction are attributed proportionally to
    // the still-held fraction of each buy.

    double purchaseValueMarket = 0.0; // held basis, no brokerage, no reduction
    double purchaseValueFinal  = 0.0; // held basis, with brokerage
    double completePurchase    = 0.0; // ALL buys, with brokerage - reduction
    double completePurchaseMarket = 0.0; // ALL buys, no brokerage, no reduction
    double netVolume           = 0.0;

    for (const BuyObject& buy : buys) {
        const BrokerageObject brk = brokerageRepo.findByBuyGuid(buy.guid());
        const double buyBrokerage = brk.brokerage();   // 0 if none linked
        const double buyReduction = brk.reduction();

        const double remVol  = buy.volume() - buy.volumeSold();
        const double frac    = (buy.volume() > 0.0) ? remVol / buy.volume() : 0.0;

        // Complete (Kpl.) Einzahlung: full buy, with brokerage - reduction
        const double fullBuyValue = roundAway(buy.volume() * buy.price());
        completePurchase       += fullBuyValue + buyBrokerage - buyReduction;
        // Marktwert: no brokerage AND no reduction — Rabatt is a discount on
        // brokerage costs, so it belongs together with "Kosten" and is
        // excluded from the Marktwert figures just like the brokerage itself
        // (confirmed 10.07.2026 — previously reduction was subtracted here,
        // which was inconsistent with excluding brokerage but not its discount).
        completePurchaseMarket += fullBuyValue;

        if (remVol > 0.0) {
            const double heldBuyValue  = roundAway(remVol * buy.price());
            const double heldBrokerage = roundAway(buyBrokerage * frac);
            const double heldReduction = roundAway(buyReduction * frac);

            purchaseValueMarket += heldBuyValue; // no brokerage, no reduction
            purchaseValueFinal  += heldBuyValue + heldBrokerage - heldReduction;
            netVolume           += remVol;
        }
    }

    const double curValue = roundAway(netVolume * curPrice);

    // -- Realized sale figures --------------------------------------------
    //
    // Derived purely from sale-side aggregates and the buy-side cost basis
    // (full buys minus still-held buys). This does NOT depend on the per-sale
    // SaleBuyDetail records, which may be stored without their proportional
    // brokerage/reduction parts (or empty). Relying on them would leave the
    // sold shares' buy cost unsubtracted and massively overstate the gain.
    //
    // salePayoutFinal:  net cash from sales WITH brokerage (- tax)
    // salePayoutMarket: net cash from sales WITHOUT brokerage AND WITHOUT
    //                    reduction (- tax) — see reduction comment above.

    double salePayoutFinal  = 0.0;
    double salePayoutMarket = 0.0;

    for (const SaleObject& sale : sales) {
        const double saleValue = roundAway(sale.volume() * sale.salePrice());

        salePayoutFinal += roundAway(saleValue
                                     - sale.brokerage()
                                     + sale.reduction()
                                     - sale.taxSum());

        salePayoutMarket += roundAway(saleValue
                                      - sale.taxSum());
    }

    // Cost basis of the SOLD shares = all buys minus still-held buys.
    const double soldCostFinal  = completePurchase       - purchaseValueFinal;  // with brokerage
    const double soldCostMarket = completePurchaseMarket - purchaseValueMarket; // no brokerage

    // Realized P/L with brokerage (= C# profitLossBrokerageReduction) and
    // without brokerage (Marktwert footer aggregate).
    const double realizedProfitLossWithFees = salePayoutFinal  - soldCostFinal;
    const double saleProfitLossMarket       = salePayoutMarket - soldCostMarket;

    // -- Aggregates from repositories --------------------------------------
    v.totalBrokerage = roundAway(brokerageRepo.totalBrokerage(guid));
    v.totalDividend  = roundAway(divRepo.totalPayoutWithTaxes(guid));

    // -- Marktwert-Tab -----------------------------------------------------
    v.volume          = netVolume;
    v.curValue        = curValue;
    v.purchaseValue   = purchaseValueMarket;
    v.profitLoss      = roundAway(curValue - purchaseValueMarket);
    v.profitLossPct   = (purchaseValueMarket > 0.0)
                        ? (v.profitLoss / purchaseValueMarket * 100.0)
                        : 0.0;
    v.saleProfitLoss  = roundAway(saleProfitLossMarket);
    v.marketValue     = roundAway(curValue + saleProfitLossMarket);
    v.marketValuePct  = (purchaseValueMarket > 0.0)
                        ? (v.marketValue / purchaseValueMarket * 100.0 - 100.0)
                        : 0.0;
    v.salePayoutMarket = roundAway(salePayoutMarket);

    // Marktwert complete columns.
    v.completePurchaseMarket   = roundAway(completePurchaseMarket);
    // Komplette Entwicklung = unrealized development (held, no brokerage)
    // + realized P/L WITH brokerage (real net gain/loss on closed positions).
    v.completeProfitLossMarket = roundAway((curValue - purchaseValueMarket)
                                           + realizedProfitLossWithFees);
    // Kpl. Marktwert = Kpl. Einzahlung + Komplette Entwicklung, so the column
    // stays consistent (Kpl. Entwicklung = Kpl. Marktwert - Kpl. Einzahlung).
    v.completeCurValueMarket   = roundAway(v.completePurchaseMarket
                                           + v.completeProfitLossMarket);
    v.completeProfitPctMarket  = (v.completePurchaseMarket > 0.0)
                                 ? (v.completeProfitLossMarket / v.completePurchaseMarket * 100.0)
                                 : 0.0;

    // -- Depotwert-Tab -----------------------------------------------------
    v.purchaseValueFinal = purchaseValueFinal;
    v.profitLossFinal    = roundAway(curValue - purchaseValueFinal);
    v.profitLossPctFinal = (purchaseValueFinal > 0.0)
                           ? (v.profitLossFinal / purchaseValueFinal * 100.0)
                           : 0.0;

    v.completePurchase   = roundAway(completePurchase);
    v.completeCurValue   = roundAway(curValue + salePayoutFinal + v.totalDividend);
    v.completeProfitLoss = roundAway(v.completeCurValue - v.completePurchase);
    v.completeProfitPct  = (v.completePurchase > 0.0)
                           ? (v.completeProfitLoss / v.completePurchase * 100.0)
                           : 0.0;

    // ShareDetailsForm "Komplette Depotbewertung" box needs the raw sale
    // proceeds and the realized brokerage-inclusive P/L as their own fields
    // (both already computed above as locals for completeCurValue /
    // realizedProfitLossWithFees).
    v.salePayoutFinal     = roundAway(salePayoutFinal);
    v.saleProfitLossFinal = roundAway(realizedProfitLossWithFees);

    return v;
}

// -- portfolioTotalsMarket -----------------------------------------------------

void ShareCalculator::portfolioTotalsMarket(const QList<ShareValues>& values,
                                             double& totalPurchase,
                                             double& totalCurValue,
                                             double& totalMarketValue,
                                             double& totalProfitLoss,
                                             double& totalProfitPct,
                                             double& totalMarketValuePct)
{
    totalPurchase       = 0.0;
    totalCurValue       = 0.0;
    totalMarketValue    = 0.0;
    totalProfitLoss     = 0.0;
    totalProfitPct      = 0.0;
    totalMarketValuePct = 0.0;

    for (const ShareValues& v : values) {
        totalPurchase    += v.purchaseValue;
        totalCurValue    += v.curValue;
        totalMarketValue += v.marketValue;
        totalProfitLoss  += v.profitLoss;
    }

    totalProfitPct      = (totalPurchase > 0.0)
                          ? (totalProfitLoss / totalPurchase * 100.0)
                          : 0.0;
    totalMarketValuePct = (totalPurchase > 0.0)
                          ? (totalMarketValue / totalPurchase * 100.0 - 100.0)
                          : 0.0;
}

// -- portfolioCompleteTotalsMarket ---------------------------------------------

void ShareCalculator::portfolioCompleteTotalsMarket(const QList<ShareValues>& values,
                                                    double& completePurchase,
                                                    double& completeCurValue,
                                                    double& completeProfitLoss,
                                                    double& completeProfitPct)
{
    completePurchase   = 0.0;
    completeCurValue   = 0.0;
    completeProfitLoss = 0.0;
    completeProfitPct  = 0.0;

    for (const ShareValues& v : values) {
        completePurchase += v.completePurchaseMarket;
        completeCurValue += v.completeCurValueMarket;
    }

    completeProfitLoss = completeCurValue - completePurchase;
    completeProfitPct  = (completePurchase > 0.0)
                         ? (completeProfitLoss / completePurchase * 100.0)
                         : 0.0;
}

// -- portfolioTotalsFinal ------------------------------------------------------

void ShareCalculator::portfolioTotalsFinal(const QList<ShareValues>& values,
                                            double& totalBrokDividend,
                                            double& totalPurchase,
                                            double& totalCurValue,
                                            double& totalProfitLoss,
                                            double& totalProfitPct,
                                            double& completePurchase,
                                            double& completeCurValue,
                                            double& completeProfitLoss,
                                            double& completeProfitPct)
{
    totalBrokDividend  = 0.0;
    totalPurchase      = 0.0;
    totalCurValue      = 0.0;
    totalProfitLoss    = 0.0;
    totalProfitPct     = 0.0;
    completePurchase   = 0.0;
    completeCurValue   = 0.0;
    completeProfitLoss = 0.0;
    completeProfitPct  = 0.0;

    for (const ShareValues& v : values) {
        totalBrokDividend += v.totalBrokerage + v.totalDividend;
        totalPurchase     += v.purchaseValueFinal; // Depotwert Einzahlung (with brokerage)
        totalCurValue     += v.curValue;
        completePurchase  += v.completePurchase;
        completeCurValue  += v.completeCurValue;
    }

    totalProfitLoss    = totalCurValue - totalPurchase;
    totalProfitPct     = (totalPurchase > 0.0)
                         ? (totalProfitLoss / totalPurchase * 100.0)
                         : 0.0;
    completeProfitLoss = completeCurValue - completePurchase;
    completeProfitPct  = (completePurchase > 0.0)
                         ? (completeProfitLoss / completePurchase * 100.0)
                         : 0.0;
}

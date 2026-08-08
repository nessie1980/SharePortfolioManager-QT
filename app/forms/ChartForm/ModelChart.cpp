// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ModelChart.h"

#include "../../models/BuyObject.h"
#include "../../models/SaleObject.h"
#include "../../models/ShareSplitObject.h"
#include "../../utils/ShareSplitAdjuster.h"

#include <algorithm>

// ── loadDailyValues ─────────────────────────────────────────────────────────
//
// Aktiensplit-Behandlung, Phase 2b (07.08.2026, siehe ARCHITECTURE.md "Offene
// Punkte"): OHLC-Werte werden vor der Rückgabe auf die heutige Skala
// umgerechnet — genau der ursprüngliche Alphabet-Fall (Kurssprung am Splittag
// im Chart). Handelsvolumen (daily_values.volume, Serie "Gehandelte
// Anteile") wird mit demselben Faktor umgerechnet wie die Kurse — Annahme:
// es stammt vom selben Datenfeed und hat denselben Bereinigungszustand (kein
// eigenes prices_adjusted-Flag für Handelsvolumen). Ohne gespeicherte Splits
// liefert ShareSplitAdjuster überall den Faktor 1,0 — bitgenau identisch zum
// bisherigen Verhalten.

QList<DailyValuesObject> ModelChart::loadDailyValues(const QString& shareGuid,
                                                      const QDate& from,
                                                      const QDate& to) const
{
    const QList<DailyValuesObject> raw =
        m_dailyValuesRepo.findByShareAndDateRange(shareGuid, from, to);

    const QList<ShareSplitObject> splits = m_splitRepo.findByShare(shareGuid);

    QList<DailyValuesObject> adjusted;
    adjusted.reserve(raw.size());
    for (const DailyValuesObject& dv : raw) {
        adjusted.append(DailyValuesObject(
            dv.shareGuid(), dv.date(),
            ShareSplitAdjuster::adjustedHistoryPrice(dv.openingPrice(), splits, dv.date()),
            ShareSplitAdjuster::adjustedHistoryPrice(dv.closingPrice(), splits, dv.date()),
            ShareSplitAdjuster::adjustedHistoryPrice(dv.top(),          splits, dv.date()),
            ShareSplitAdjuster::adjustedHistoryPrice(dv.bottom(),       splits, dv.date()),
            ShareSplitAdjuster::adjustedHistoryPrice(dv.volume(),       splits, dv.date())));
    }
    return adjusted;
}

// ── latestDailyValueDate ─────────────────────────────────────────────────────

QDate ModelChart::latestDailyValueDate(const QString& shareGuid) const
{
    return m_dailyValuesRepo.latestDate(shareGuid);
}

// ── earliestDailyValueDate ─────────────────────────────────────────────────────

QDate ModelChart::earliestDailyValueDate(const QString& shareGuid) const
{
    return m_dailyValuesRepo.earliestDate(shareGuid);
}

// ── heldVolumeSeries ──────────────────────────────────────────────────────────

QMap<QDate, double> ModelChart::heldVolumeSeries(const QString& shareGuid,
                                                 const QList<QDate>& dates) const
{
    QMap<QDate, double> result;
    if (dates.isEmpty())
        return result;

    const auto buys  = m_buyRepo.findByShare(shareGuid);   // ascending by date
    const auto sales = m_saleRepo.findByShare(shareGuid);  // ascending by date

    // Aktiensplit-Behandlung, Phase 2b: jeder Kauf/Verkauf wird vor der
    // Summierung auf die heutige Skala umgerechnet — sonst springt der
    // Bestand am Splittag exakt wie im ursprünglichen Alphabet-Fall.
    const QList<ShareSplitObject> splits = m_splitRepo.findByShare(shareGuid);

    // dates arrives already sorted ascending (DailyValuesRepository orders by
    // date ASC and PresenterChart passes it through unchanged) — a single
    // forward sweep over buys/sales keeps this O(dates + buys + sales)
    // instead of re-summing from scratch for every date.
    int buyIdx = 0, saleIdx = 0;
    double held = 0.0;

    for (const QDate& d : dates) {
        while (buyIdx < buys.size() && buys[buyIdx].date() <= d) {
            held += ShareSplitAdjuster::adjustedVolume(
                buys[buyIdx].volume(), splits, buys[buyIdx].date());
            ++buyIdx;
        }
        while (saleIdx < sales.size() && sales[saleIdx].date() <= d) {
            held -= ShareSplitAdjuster::adjustedVolume(
                sales[saleIdx].volume(), splits, sales[saleIdx].date());
            ++saleIdx;
        }
        result.insert(d, held);
    }
    return result;
}

// ── latestBuy / latestSale ────────────────────────────────────────────────────

ChartReferenceInfo ModelChart::latestBuy(const QString& shareGuid) const
{
    const auto buys = m_buyRepo.findByShare(shareGuid); // ascending by date
    if (buys.isEmpty())
        return ChartReferenceInfo{};

    const auto& last = buys.constLast();
    const QList<ShareSplitObject> splits = m_splitRepo.findByShare(shareGuid);
    return ChartReferenceInfo{ /*valid=*/true, last.date(),
        ShareSplitAdjuster::adjustedTransactionPrice(last.price(), splits, last.date()) };
}

ChartReferenceInfo ModelChart::latestSale(const QString& shareGuid) const
{
    const auto sales = m_saleRepo.findByShare(shareGuid); // ascending by date
    if (sales.isEmpty())
        return ChartReferenceInfo{};

    const auto& last = sales.constLast();
    const QList<ShareSplitObject> splits = m_splitRepo.findByShare(shareGuid);
    return ChartReferenceInfo{ /*valid=*/true, last.date(),
        ShareSplitAdjuster::adjustedTransactionPrice(last.salePrice(), splits, last.date()) };
}

// ── buysInRange / salesInRange ────────────────────────────────────────────────

QList<ChartReferenceInfo> ModelChart::buysInRange(const QString& shareGuid,
                                                  const QDate& from, const QDate& to) const
{
    QList<ChartReferenceInfo> result;
    const QList<ShareSplitObject> splits = m_splitRepo.findByShare(shareGuid);
    for (const auto& buy : m_buyRepo.findByShare(shareGuid)) { // ascending by date
        if (buy.date() >= from && buy.date() <= to)
            result.append(ChartReferenceInfo{ /*valid=*/true, buy.date(),
                ShareSplitAdjuster::adjustedTransactionPrice(buy.price(), splits, buy.date()),
                ShareSplitAdjuster::adjustedVolume(buy.volume(), splits, buy.date()) });
    }
    return result;
}

QList<ChartReferenceInfo> ModelChart::salesInRange(const QString& shareGuid,
                                                   const QDate& from, const QDate& to) const
{
    QList<ChartReferenceInfo> result;
    const QList<ShareSplitObject> splits = m_splitRepo.findByShare(shareGuid);
    for (const auto& sale : m_saleRepo.findByShare(shareGuid)) { // ascending by date
        if (sale.date() >= from && sale.date() <= to)
            result.append(ChartReferenceInfo{ /*valid=*/true, sale.date(),
                ShareSplitAdjuster::adjustedTransactionPrice(sale.salePrice(), splits, sale.date()),
                ShareSplitAdjuster::adjustedVolume(sale.volume(), splits, sale.date()) });
    }
    return result;
}

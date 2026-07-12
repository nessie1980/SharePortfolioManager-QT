// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ModelChart.h"

#include "../../models/BuyObject.h"
#include "../../models/SaleObject.h"

#include <algorithm>

// ── loadDailyValues ─────────────────────────────────────────────────────────

QList<DailyValuesObject> ModelChart::loadDailyValues(const QString& shareGuid,
                                                      const QDate& from,
                                                      const QDate& to) const
{
    return m_dailyValuesRepo.findByShareAndDateRange(shareGuid, from, to);
}

// ── latestDailyValueDate ─────────────────────────────────────────────────────

QDate ModelChart::latestDailyValueDate(const QString& shareGuid) const
{
    return m_dailyValuesRepo.latestDate(shareGuid);
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

    // dates arrives already sorted ascending (DailyValuesRepository orders by
    // date ASC and PresenterChart passes it through unchanged) — a single
    // forward sweep over buys/sales keeps this O(dates + buys + sales)
    // instead of re-summing from scratch for every date.
    int buyIdx = 0, saleIdx = 0;
    double held = 0.0;

    for (const QDate& d : dates) {
        while (buyIdx < buys.size() && buys[buyIdx].date() <= d) {
            held += buys[buyIdx].volume();
            ++buyIdx;
        }
        while (saleIdx < sales.size() && sales[saleIdx].date() <= d) {
            held -= sales[saleIdx].volume();
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
    return ChartReferenceInfo{ /*valid=*/true, last.date(), last.price() };
}

ChartReferenceInfo ModelChart::latestSale(const QString& shareGuid) const
{
    const auto sales = m_saleRepo.findByShare(shareGuid); // ascending by date
    if (sales.isEmpty())
        return ChartReferenceInfo{};

    const auto& last = sales.constLast();
    return ChartReferenceInfo{ /*valid=*/true, last.date(), last.salePrice() };
}

// ── buysInRange / salesInRange ────────────────────────────────────────────────

QList<ChartReferenceInfo> ModelChart::buysInRange(const QString& shareGuid,
                                                  const QDate& from, const QDate& to) const
{
    QList<ChartReferenceInfo> result;
    for (const auto& buy : m_buyRepo.findByShare(shareGuid)) // ascending by date
        if (buy.date() >= from && buy.date() <= to)
            result.append(ChartReferenceInfo{ /*valid=*/true, buy.date(), buy.price(), buy.volume() });
    return result;
}

QList<ChartReferenceInfo> ModelChart::salesInRange(const QString& shareGuid,
                                                   const QDate& from, const QDate& to) const
{
    QList<ChartReferenceInfo> result;
    for (const auto& sale : m_saleRepo.findByShare(shareGuid)) // ascending by date
        if (sale.date() >= from && sale.date() <= to)
            result.append(ChartReferenceInfo{ /*valid=*/true, sale.date(), sale.salePrice(), sale.volume() });
    return result;
}

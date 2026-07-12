// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "ChartTypes.h"

#include <QString>
#include <QDate>
#include <QList>
#include <QMap>

#include "../../models/DailyValuesObject.h"

/**
 * @brief Read-only model interface for the "Aktien-Chart" tab.
 *
 * Deliberately minimal, same convention as IModelShareDetails: only what the
 * chart needs, nothing that belongs to the other ShareDetailsForm tabs.
 */
class IModelChart
{
public:
    virtual ~IModelChart() = default;

    /** Daily OHLCV values in [from, to], ordered ascending by date. */
    virtual QList<DailyValuesObject> loadDailyValues(const QString& shareGuid,
                                                      const QDate& from,
                                                      const QDate& to) const = 0;

    /** Most recent date with a daily value for this share (invalid QDate if none). */
    virtual QDate latestDailyValueDate(const QString& shareGuid) const = 0;

    /**
     * @brief Cumulative held volume ("Anteile") as of each requested date.
     *
     * For each date in @p dates: sum of all buy volumes with dateTime <= that
     * date, minus all sale volumes with dateTime <= that date (no FIFO lot
     * tracking needed here — this is a simple running total, not a per-lot figure).
     *
     * @return Map date -> held volume, same keys as @p dates.
     */
    virtual QMap<QDate, double> heldVolumeSeries(const QString& shareGuid,
                                                 const QList<QDate>& dates) const = 0;

    /** Most recent buy for this share (invalid ChartReferenceInfo if none). */
    virtual ChartReferenceInfo latestBuy(const QString& shareGuid) const = 0;

    /** Most recent sale for this share (invalid ChartReferenceInfo if none). */
    virtual ChartReferenceInfo latestSale(const QString& shareGuid) const = 0;

    /** All buys for this share within [from, to], ascending by date — Datum,
     *  Preis und Stückzahl für die vertikalen "Kauf"-Markerlinien im Chart
     *  inkl. deren Hover-Tooltip. */
    virtual QList<ChartReferenceInfo> buysInRange(const QString& shareGuid,
                                                  const QDate& from, const QDate& to) const = 0;

    /** All sales for this share within [from, to], ascending by date — Datum,
     *  Preis und Stückzahl für die vertikalen "Verkauf"-Markerlinien im Chart
     *  inkl. deren Hover-Tooltip. */
    virtual QList<ChartReferenceInfo> salesInRange(const QString& shareGuid,
                                                   const QDate& from, const QDate& to) const = 0;
};

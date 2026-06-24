// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QString>
#include <QDate>
#include <QLocale>

/**
 * @brief Represents a single day's OHLCV data for a share.
 *
 * Stores the opening, closing, high (top), low (bottom) prices and
 * trading volume for one trading day. Used by DailyValuesRepository
 * to persist historical price data in the `daily_values` table.
 *
 * The primary key in the database is the composite (shareGuid, date).
 *
 * ### Example
 * @code
 * DailyValuesObject entry(
 *     shareGuid,
 *     QDate(2024, 6, 15),
 *     142.50,  // opening
 *     144.80,  // closing
 *     145.20,  // top
 *     141.90,  // bottom
 *     1250000  // volume
 * );
 * @endcode
 */
class DailyValuesObject
{
public:
    DailyValuesObject() = default;

    /**
     * @brief Full constructor.
     * @param shareGuid    GUID of the parent share.
     * @param date         Trading date.
     * @param openingPrice Opening price of the day.
     * @param closingPrice Closing price of the day.
     * @param top          Highest price of the day.
     * @param bottom       Lowest price of the day.
     * @param volume       Trading volume of the day.
     */
    DailyValuesObject(const QString& shareGuid,
                      const QDate&   date,
                      double openingPrice = 0.0,
                      double closingPrice = 0.0,
                      double top          = 0.0,
                      double bottom       = 0.0,
                      double volume       = 0.0);

    // ── Identity ──────────────────────────────────────────────────────────
    QString shareGuid() const { return m_shareGuid; } ///< Parent share GUID

    // ── Date ──────────────────────────────────────────────────────────────
    QDate   date()      const { return m_date; }                                           ///< Trading date
    QString dateAsStr() const { return QLocale().toString(m_date, QLocale::ShortFormat); } ///< Date formatted for display

    // ── OHLCV values ──────────────────────────────────────────────────────
    double openingPrice() const { return m_openingPrice; } ///< Opening price
    double closingPrice() const { return m_closingPrice; } ///< Closing price
    double top()          const { return m_top; }          ///< Highest price of the day
    double bottom()       const { return m_bottom; }       ///< Lowest price of the day
    double volume()       const { return m_volume; }       ///< Trading volume

    // ── Setters ───────────────────────────────────────────────────────────
    /**
     * @brief Update all OHLCV values at once.
     * @param openingPrice New opening price.
     * @param closingPrice New closing price.
     * @param top          New highest price.
     * @param bottom       New lowest price.
     * @param volume       New trading volume.
     */
    void setValues(double openingPrice, double closingPrice,
                   double top, double bottom, double volume);

    // ── Validity ──────────────────────────────────────────────────────────
    /**
     * @brief Returns true if the object contains valid data.
     *
     * A DailyValuesObject is valid when shareGuid is set and date is valid.
     * @return true if valid.
     */
    bool isValid() const { return !m_shareGuid.isEmpty() && m_date.isValid(); }

private:
    QString m_shareGuid;
    QDate   m_date;

    double  m_openingPrice = 0.0;
    double  m_closingPrice = 0.0;
    double  m_top          = 0.0;
    double  m_bottom       = 0.0;
    double  m_volume       = 0.0;
};

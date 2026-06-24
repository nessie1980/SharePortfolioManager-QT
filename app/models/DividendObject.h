// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QString>
#include <QDateTime>
#include <QLocale>

/**
 * @brief Represents a single dividend payment for a share.
 *
 * Supports both domestic and foreign currency payouts via `enableForeignCurrency`
 * and `exchangeRatio`. All derived values are calculated automatically.
 *
 * ### Calculated values
 * | Property | Formula |
 * |----------|---------|
 * | `taxSum()` | taxAtSource + capitalGainsTax + solidarityTax |
 * | `dividendPayout()` | volume × rate (domestic) or payoutFc / exchangeRatio |
 * | `dividendPayoutFc()` | volume × rate (foreign currency, if enabled) |
 * | `dividendPayoutWithTaxes()` | dividendPayout - taxSum |
 * | `yield()` | rate / priceAtPayday × 100 |
 *
 * ### Example
 * @code
 * DividendObject div(
 *     QUuid::createUuid().toString(QUuid::WithoutBraces),
 *     shareGuid, "2024-05-15T00:00:00",
 *     1.50,   // rate per share
 *     100.0,  // volume
 *     5.0,    // taxAtSource
 *     3.0,    // capitalGainsTax
 *     0.5,    // solidarityTax
 *     45.00,  // priceAtPayday
 *     false,  // no foreign currency
 *     1.0     // exchangeRatio
 * );
 * qDebug() << div.dividendPayoutWithTaxes(); // 1.5 * 100 - 5.0 - 3.0 - 0.5 = 141.5
 * @endcode
 */
class DividendObject
{
public:
    DividendObject() = default;

    /**
     * @brief Full constructor — calculates all derived values.
     * @param guid                 Unique identifier
     * @param shareGuid            Parent share GUID
     * @param dateTime             Payment date (ISO 8601)
     * @param rate                 Dividend rate per share
     * @param volume               Number of shares held at payment date
     * @param taxAtSource          Withholding tax
     * @param capitalGainsTax      Capital gains tax
     * @param solidarityTax        Solidarity surcharge
     * @param priceAtPayday        Share price at payment date (for yield calculation)
     * @param enableForeignCurrency  True if payout is in a foreign currency
     * @param exchangeRatio        Exchange rate FC→domestic (default 1.0)
     * @param currency             Currency code (e.g. "EUR")
     * @param document             Path to dividend document (optional)
     */
    DividendObject(const QString& guid,
                   const QString& shareGuid,
                   const QString& dateTime,
                   double rate,
                   double volume,
                   double taxAtSource       = 0.0,
                   double capitalGainsTax   = 0.0,
                   double solidarityTax     = 0.0,
                   double priceAtPayday     = 0.0,
                   bool   enableForeignCurrency = false,
                   double exchangeRatio     = 1.0,
                   const QString& currency  = QStringLiteral("EUR"),
                   const QString& document  = QString());

    // ── Identity ──────────────────────────────────────────────────────────
    QString guid()      const { return m_guid; }      ///< Unique identifier
    QString shareGuid() const { return m_shareGuid; } ///< Parent share GUID

    // ── Date / Time ───────────────────────────────────────────────────────
    QString dateTime()  const { return m_dateTime; }                                            ///< ISO 8601 datetime string
    QDate   date()      const { return QDateTime::fromString(m_dateTime, Qt::ISODate).date(); } ///< Payment date as QDate
    QString dateAsStr() const { return QLocale().toString(date(), QLocale::ShortFormat); }      ///< Date formatted for display
    int     year()      const { return date().year(); }                                         ///< Year of the dividend payment

    // ── Core values ───────────────────────────────────────────────────────
    double  rate()              const { return m_rate; }              ///< Dividend per share
    double  volume()            const { return m_volume; }            ///< Shares held
    double  priceAtPayday()     const { return m_priceAtPayday; }     ///< Share price at payment
    bool    enableForeignCurrency() const { return m_enableFc; }      ///< Foreign currency mode
    double  exchangeRatio()     const { return m_exchangeRatio; }     ///< FC exchange ratio
    QString currency()          const { return m_currency; }          ///< Currency code

    // ── Tax values ────────────────────────────────────────────────────────
    double taxAtSource()     const { return m_taxAtSource; }     ///< Withholding tax
    double capitalGainsTax() const { return m_capitalGainsTax; } ///< Capital gains tax
    double solidarityTax()   const { return m_solidarityTax; }   ///< Solidarity surcharge
    double taxSum()          const { return m_taxSum; }          ///< Total taxes

    // ── Calculated payout values ──────────────────────────────────────────
    double dividendPayout()          const { return m_dividendPayout; }         ///< Gross payout (domestic)
    double dividendPayoutFc()        const { return m_dividendPayoutFc; }       ///< Gross payout (foreign currency)
    double dividendPayoutWithTaxes() const { return m_dividendPayoutWithTaxes;}  ///< Net payout after taxes
    double yield()                   const { return m_yield; }                   ///< Yield in percent

    // ── Document ──────────────────────────────────────────────────────────
    QString document()                            const { return m_document; }    ///< Path to dividend document
    void    setDocument(const QString& document)        { m_document = document; } ///< Update document path

    // ── Validity ──────────────────────────────────────────────────────────
    /// Returns true if guid is set, rate > 0 and volume > 0.
    bool isValid() const { return !m_guid.isEmpty() && m_rate > 0 && m_volume > 0; }

private:
    void calculateValues();

    QString m_guid;
    QString m_shareGuid;
    QString m_dateTime;

    double  m_rate          = 0.0;
    double  m_volume        = 0.0;
    double  m_priceAtPayday = 0.0;
    bool    m_enableFc      = false;
    double  m_exchangeRatio = 1.0;
    QString m_currency      = QStringLiteral("EUR");

    double  m_taxAtSource     = 0.0;
    double  m_capitalGainsTax = 0.0;
    double  m_solidarityTax   = 0.0;
    double  m_taxSum          = 0.0;

    double  m_dividendPayout          = 0.0;
    double  m_dividendPayoutFc        = 0.0;
    double  m_dividendPayoutWithTaxes = 0.0;
    double  m_yield                   = 0.0;

    QString m_document;
};

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
 *
 * ### Ex-Tag und Depotnummer (seit 21.08.2026)
 *
 * `exDate`/`depotNumber` sind die Datengrundlage für die Plausibilitätsprüfung
 * der Dividenden-Stückzahl — siehe ARCHITECTURE.md,
 * "Plausibilitätsprüfung der Dividenden-Stückzahl". Beide sind auf
 * Modell-/DB-Ebene bewusst optional (leerer String = nicht gesetzt): eine
 * bestehende Dividende ohne diese Angaben muss weiterhin ladbar und anzeigbar
 * bleiben. Verbindlich ("Muss") sind sie erst in der Formularvalidierung von
 * `PresenterDividendEdit` (Phase 2) — `isValid()` bleibt daher unverändert an
 * `guid`/`rate`/`volume` geknüpft und prüft `exDate`/`depotNumber` NICHT mit,
 * sonst würde eine bereits gespeicherte Alt-Dividende beim Laden fälschlich
 * als "nicht gefunden" erscheinen.
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
     * @param exDate               Ex-date (ISO 8601 date, e.g. "2024-05-13"), optional —
     *        empty for existing dividends that predate this field
     * @param depotNumber          Depot number the dividend was paid into, optional —
     *        empty for existing dividends that predate this field
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
                   const QString& document  = QString(),
                   const QString& exDate      = QString(),
                   const QString& depotNumber = QString());

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

    // ── Ex-date / Depot (siehe Klassendoku "Ex-Tag und Depotnummer") ───────
    QString exDate()      const { return m_exDate; }      ///< Ex-date, ISO 8601 date string, empty if unset
    QDate   exDateAsDate() const { return QDate::fromString(m_exDate, Qt::ISODate); } ///< Ex-date as QDate, invalid if unset
    QString exDateAsStr() const { return hasExDate()
                                        ? QLocale().toString(exDateAsDate(), QLocale::ShortFormat)
                                        : QStringLiteral("-"); }                   ///< Ex-date formatted for display, "-" if unset
    bool    hasExDate()   const { return exDateAsDate().isValid(); }               ///< True if a (parseable) ex-date is set

    QString depotNumber()    const { return m_depotNumber; }        ///< Depot number, empty if unset
    bool    hasDepotNumber() const { return !m_depotNumber.isEmpty(); } ///< True if a depot number is set

    /**
     * @brief Das Datum, auf das sich volume() bezieht: der Ex-Tag, wenn
     *        gesetzt — sonst der Zahltag als Rückfall.
     *
     * Phase 4 der Ex-Tag-Behandlung (21.08.2026). Die Bank schüttet auf den
     * Bestand am Ex-Tag aus, nicht auf den am Zahltag; für die Frage "war
     * diese Stückzahl schon nach dem Split gezählt" ist deshalb der Ex-Tag
     * der richtige Massstab. Die Übersichtstabellen benutzen diesen Wert für
     * den Split-Marker und dessen Tooltip.
     *
     * Der Rückfall auf date() ist für Dividenden gedacht, die vor dem
     * 21.08.2026 erfasst wurden und noch keinen Ex-Tag haben. Er bildet exakt
     * das bisherige Verhalten ab — eine solche Zeile sieht also aus wie
     * vorher, statt mangels Ex-Tag gar keinen Marker mehr zu bekommen.
     *
     * @note Bewusst NICHT von `DividendVolumeChecker` (Phase 3) verwendet.
     * Dort ist der Ex-Tag Pflichtfeld und garantiert vorhanden; ein
     * stillschweigender Rückfall auf den Zahltag würde die Stückzahl gegen
     * den falschen Stichtag prüfen und wäre schlimmer als gar keine Prüfung.
     */
    QDate volumeReferenceDate() const { return hasExDate() ? exDateAsDate() : date(); }

    // ── Validity ──────────────────────────────────────────────────────────
    /// Returns true if guid is set, rate > 0 and volume > 0.
    /// Deliberately does NOT require exDate()/depotNumber() — those are
    /// optional at model/DB level (see class doc) so that pre-21.08.2026
    /// dividends without them keep loading and displaying correctly.
    /// Enforcing them as mandatory for saving is the presenter/UI layer's job.
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

    QString m_exDate;      ///< ISO 8601 date string, empty if unset
    QString m_depotNumber; ///< Depot number, empty if unset
};

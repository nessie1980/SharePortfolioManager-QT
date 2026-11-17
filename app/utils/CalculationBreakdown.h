// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "CalculationReport.h"

#include <QDate>
#include <QList>
#include <QString>
#include <QStringList>

/**
 * @brief Eine Zeile der Gegenüberstellung Marktwert / Depotwert.
 *
 * Gemeinsames Zeilenmodell von Berechnungsbericht und Tab
 * "Depot- vs. Marktwert" (ergänzt 27.10.2026). Der Bericht rendert daraus
 * seine Texttabelle, der Tab seine Qt-Tabelle — beide zeigen damit
 * zwangsläufig dieselben Werte und Rechenwege.
 */
struct CalculationBreakdownRow
{
    QString label;              ///< Bezeichnung, z. B. "Kurswert gehaltener Stücke"
    bool    hasMarket = false;  ///< false = in der Marktwert-Sicht nicht enthalten ("–")
    double  market    = 0.0;
    bool    hasDepot  = false;  ///< false = in der Depotwert-Sicht nicht enthalten ("–")
    double  depot     = 0.0;
    bool    percent   = false;  ///< Prozentwert statt Euro-Betrag
    QString marketWay;          ///< Rechenweg der Marktwert-Sicht
    QString depotWay;           ///< Rechenweg der Depotwert-Sicht

    /// Rechnung mit den eingesetzten Zahlen, z. B. "1.234,00 − 1.000,00 =
    /// 234,00"; leer, wo der Wert direkt aus den Transaktionen summiert wird.
    /// Nur im Tab (Tooltip), nicht im Bericht.
    QString marketFigures;
    QString depotFigures;

    /// Wo der Wert in den Tabs "Kompletter Depotwert" / "Kompletter
    /// Marktwert", ihren Footern und den Charts erscheint (Nessies
    /// Entscheidung 27.10.2026, Variante a: nur Text). Nur im Tab, nicht im
    /// Bericht (Variante b für den Bericht).
    QStringList usedIn;
};

/// Ergebnis eines Abgleichs Tabelle gegen Chart.
enum class CalculationCheckResult
{
    Ok,         ///< Differenz unter der Toleranz
    Explained,  ///< Differenz geht genau in bekannten Ursachen auf (Kurs, Aktien ohne Kursdaten)
    Hint,       ///< Differenz erwartbar (aktueller Kurs gegen letzten Tageswert)
    Deviation   ///< echte Abweichung
};

/**
 * @brief Eine Zeile "Abgleich Tabelle gegen Chart" samt Befund.
 */
struct CalculationCheck
{
    QString label;              ///< z. B. "Bestand (Stück)"
    double  tableValue = 0.0;
    double  chartValue = 0.0;
    double  diff       = 0.0;   ///< tableValue − chartValue
    bool    isVolume   = false; ///< Stückzahl (vier Nachkommastellen) statt Euro
    CalculationCheckResult result = CalculationCheckResult::Ok;
    QString resultText;         ///< "ok", "Hinweis", "ABWEICHUNG", "durch Kurs erklärt", …
    QString finding;            ///< Zeile für die Auffälligkeiten; leer bei Ok
};

/// Abweichungen und Hinweise, getrennt — wie Teil 3 des Berichts.
struct CalculationFindings
{
    QStringList deviations; ///< echte Abweichungen
    QStringList hints;      ///< erwartbar oder durch bekannte Ursachen erklärt
};

/**
 * @brief Die Footer-Werte beider Tabs, genau wie MainWindow sie berechnet.
 *
 * Über ShareCalculator::portfolioTotalsMarket(),
 * portfolioCompleteTotalsMarket() und portfolioTotalsFinal() — dieselben
 * Aufrufe wie in MainWindow::updatePortfolioFooters().
 */
struct CalculationFooterTotals
{
    // ── Footer "Kompletter Marktwert" ─────────────────────────────────────
    double marketPurchase         = 0.0; ///< Einzahlung (gesamt)
    double marketCurValue         = 0.0; ///< Aktueller Depotstand
    double marketProfit           = 0.0; ///< Entwicklung (gesamt)
    double marketProfitPct        = 0.0;
    double marketCompletePurchase = 0.0; ///< Kpl. Einzahlung
    double marketCompleteValue    = 0.0; ///< Kpl. Marktwert
    double marketCompleteProfit   = 0.0; ///< Komplette Entwicklung
    double marketCompleteProfitPct= 0.0;

    // ── Footer "Kompletter Depotwert" ─────────────────────────────────────
    double depotPurchase          = 0.0; ///< Einzahlung (gesamt)
    double depotBrokerage         = 0.0; ///< Kosten (ges.)
    double depotDividend          = 0.0; ///< Dividenden (ges.)
    double depotCurValue          = 0.0; ///< Aktueller Depotstand
    double depotProfit            = 0.0; ///< Entwicklung (gesamt)
    double depotProfitPct         = 0.0;
    double depotCompletePurchase  = 0.0; ///< Kpl. Einzahlung
    double depotCompleteValue     = 0.0; ///< Kpl. Wert
    double depotCompleteProfit    = 0.0; ///< Komplette Entwicklung
    double depotCompleteProfitPct = 0.0;
};

/**
 * @brief Abgleich Footer gegen Chart samt der bekannten Erklärungen.
 */
struct CalculationTotalChecks
{
    QList<CalculationCheck> checks;  ///< leer, wenn es keinen Chartpunkt gibt
    double byPrice          = 0.0;   ///< Σ Kurseffekt je Aktie
    double excludedCurValue = 0.0;   ///< Kurswert der Aktien ohne Kursdaten (nicht im Chart)
    double excludedProfit   = 0.0;   ///< Kompl. Entwicklung der Aktien ohne Kursdaten (nicht im Chart)
    double excludedHeldProfit = 0.0;   ///< Aktuelle Entwicklung (Depotwert) dieser Aktien (01.11.2026)
    double excludedCompleteMarket = 0.0; ///< Kpl. Marktwert dieser Aktien (01.11.2026)
};

// ── Transaktionstabellen je Aktie (ergänzt 17.11.2026, v1.30.1) ─────────────
//
// Reine Zahlen ohne Formatierung. Bis v1.30.0 rechnete
// CalculationReport::buildShareSection() diese Werte direkt beim Erzeugen
// des Textes; seit v1.30.1 holt der Bericht sie von hier, damit der Tab
// "Depot- vs. Marktwert" dieselben Zahlen zeigen kann, ohne sie ein zweites
// Mal zu rechnen. Stückzahlen und Kurse der Zeilen stehen in Beleg-Skala,
// die Felder "...Today" in heutiger Skala (siehe ShareSplitAdjuster).

/**
 * @brief Ein Kauf mit Kosten und FIFO-Nachrechnung.
 */
struct CalculationBuyRow
{
    QDate   date;
    QString depotNumber;
    double  volume         = 0.0;  ///< Stück, Beleg-Skala
    double  price          = 0.0;  ///< Kurs, Beleg-Skala
    double  value          = 0.0;  ///< Stück × Kurs, kaufmännisch gerundet
    double  costs          = 0.0;  ///< zugeordnete Kosten: Gebühren − Rabatt
    double  valueWithCosts = 0.0;  ///< value + costs
    double  soldStored     = 0.0;  ///< BuyObject::volumeSold(), Beleg-Skala
    double  soldFifo       = 0.0;  ///< nach FIFO neu zugeteilt, Beleg-Skala
    double  remainingFifo  = 0.0;  ///< volume − soldFifo, Beleg-Skala
    double  volumeToday    = 0.0;  ///< volume in heutiger Skala
    double  priceToday     = 0.0;  ///< price in heutiger Skala
    double  remainingFifoToday = 0.0; ///< remainingFifo in heutiger Skala
};

/**
 * @brief Alle Käufe einer Aktie samt Summen.
 *
 * Mit Splits liegen die Stückzahlen der Käufe in verschiedenen Beleg-Skalen
 * und lassen sich nicht addieren; maßgeblich ist dann sumVolumeToday. Die
 * verkauften Mengen werden für die Summen immer in heutige Skala umgerechnet.
 */
struct CalculationBuyTable
{
    QList<CalculationBuyRow> rows;      ///< Reihenfolge wie CalculationReportShare::buys
    bool   withSplits         = false;  ///< die Aktie hat mindestens einen Split
    double sumVolume          = 0.0;    ///< Σ Stück in Beleg-Skala (nur ohne Splits aussagekräftig)
    double sumVolumeToday     = 0.0;
    double sumValue           = 0.0;
    double sumCosts           = 0.0;
    double sumValueWithCosts  = 0.0;    ///< sumValue + sumCosts
    double sumSoldStoredToday = 0.0;
    double sumSoldFifoToday   = 0.0;
};

/**
 * @brief Ein Verkauf mit Auszahlung.
 */
struct CalculationSaleRow
{
    QDate   date;
    QString depotNumber;
    double  volume    = 0.0;  ///< Stück, Beleg-Skala
    double  price     = 0.0;  ///< Verkaufskurs, Beleg-Skala
    double  value     = 0.0;  ///< Stück × Kurs, kaufmännisch gerundet
    double  fees      = 0.0;  ///< Gebühren
    double  reduction = 0.0;  ///< Rabatt
    double  taxes     = 0.0;  ///< Steuern gesamt
    double  payout    = 0.0;  ///< value − fees + reduction − taxes
};

/// Alle Verkäufe einer Aktie samt Summen.
struct CalculationSaleTable
{
    QList<CalculationSaleRow> rows;   ///< Reihenfolge wie CalculationReportShare::sales
    double sumVolumeToday = 0.0;      ///< Σ Stück in heutiger Skala
    double sumValue       = 0.0;
    double sumFees        = 0.0;
    double sumReduction   = 0.0;
    double sumTaxes       = 0.0;
    double sumPayout      = 0.0;
};

/**
 * @brief Eine Dividendenzahlung.
 */
struct CalculationDividendRow
{
    QDate   date;              ///< Zahltag
    double  volume   = 0.0;    ///< Stück am Zahltag
    double  rate     = 0.0;    ///< je Stück, in der Währung des Belegs
    QString currency;          ///< "€" oder die Fremdwährung des Belegs, z. B. "USD"
    double  gross    = 0.0;    ///< Brutto in Euro
    double  taxAtSource     = 0.0;
    double  capitalGainsTax = 0.0;
    double  solidarityTax   = 0.0;
    double  taxes    = 0.0;    ///< Steuern gesamt
    double  net      = 0.0;    ///< Brutto − Steuern gesamt
};

/**
 * @brief Alle Dividenden einer Aktie samt Summen.
 *
 * Stückzahl und Satz je Stück lassen sich über mehrere Zahltage nicht
 * sinnvoll addieren und haben deshalb keine Summe.
 */
struct CalculationDividendTable
{
    QList<CalculationDividendRow> rows;  ///< Reihenfolge wie CalculationReportShare::dividends
    double sumGross           = 0.0;
    double sumTaxAtSource     = 0.0;
    double sumCapitalGainsTax = 0.0;
    double sumSolidarityTax   = 0.0;
    double sumTaxes           = 0.0;
    double sumNet             = 0.0;
};

/// Wozu ein Kosteneintrag gehört.
enum class CalculationBrokerageAssignment
{
    Buy,         ///< BrokerageObject::buyGuid() gesetzt
    Sale,        ///< saleGuid() gesetzt
    Standalone   ///< keins von beiden
};

/**
 * @brief Ein Kosteneintrag.
 */
struct CalculationBrokerageRow
{
    QDate  date;
    CalculationBrokerageAssignment assignment = CalculationBrokerageAssignment::Standalone;
    double provision = 0.0;
    double brokerFee = 0.0;   ///< Makler
    double traderFee = 0.0;   ///< Börse
    double gross     = 0.0;   ///< Provision + Makler + Börse
    double reduction = 0.0;   ///< Rabatt
    double net       = 0.0;   ///< Brutto − Rabatt
};

/// Alle Kosteneinträge einer Aktie samt Summen.
struct CalculationBrokerageTable
{
    QList<CalculationBrokerageRow> rows;  ///< Reihenfolge wie CalculationReportShare::brokerages
    double sumProvision = 0.0;
    double sumBrokerFee = 0.0;
    double sumTraderFee = 0.0;
    double sumGross     = 0.0;
    double sumReduction = 0.0;
    double sumNet       = 0.0;
};

/**
 * @brief Der Block "Bestand (heutige Skala)" des Berichts.
 */
struct CalculationStockSummary
{
    double bought     = 0.0;  ///< gekauft gesamt
    double sold       = 0.0;  ///< verkauft gesamt
    double computed   = 0.0;  ///< bought − sold
    double fifo       = 0.0;  ///< Σ Rest je Kauf nach FIFO-Nachrechnung
    double table      = 0.0;  ///< Bestand der Tabelle (ShareValues::volume)
};

/**
 * @brief Gemeinsames Zeilenmodell von Berechnungsbericht und Tab
 * "Depot- vs. Marktwert" (ergänzt 27.10.2026).
 *
 * Herausgelöst aus CalculationReport, damit der neue Tab die Zeilen nicht
 * ein zweites Mal zusammenbaut — sonst liefen Bericht und Tab früher oder
 * später auseinander, wie früher die parseDouble()-Kopien. Der Bericht
 * rendert die Zeilen als Text (unverändert gegenüber v1.27.0), der Tab als
 * Qt-Tabellen.
 *
 * Datenbankfrei und zustandslos: alle Funktionen arbeiten nur auf den von
 * CalculationReport::collect() gelieferten Eingangsdaten.
 *
 * Die Texte laufen wie im Bericht nicht über tr().
 */
class CalculationBreakdown
{
public:
    CalculationBreakdown() = delete;

    // ── Formatierung (vom Bericht übernommen) ─────────────────────────────

    /// Werte, die gerundet null ergeben, als exakt null — sonst erscheint
    /// eine Rechendifferenz wie −0,000000001 als "-0,00" (Bugfix 24.10.2026).
    static double clean(double value, double tolerance);

    static QString money(double value);   ///< "1.234,56"
    static QString euro(double value);    ///< "1.234,56 €"
    static QString volume(double value);  ///< "12,0000"
    static QString price(double value);   ///< "62,0000"
    static QString percent(double value); ///< "12,34 %"

    // ── Je Aktie ──────────────────────────────────────────────────────────

    /**
     * @brief Die 14 Zeilen der Gegenüberstellung "Kompletter Marktwert" /
     * "Kompletter Depotwert" einer Aktie, in Berichtsreihenfolge.
     */
    static QList<CalculationBreakdownRow> shareRows(const ShareValues& values);

    /**
     * @brief Teil einer Differenz Tabelle − Chart, der allein aus dem
     * Kursunterschied folgt: der Chart bewertet seinen Bestand mit dem
     * letzten Tageswert, die Tabelle denselben Bestand mit dem aktuellen Kurs.
     * @return 0, wenn die Aktie keinen Chartpunkt hat.
     */
    static double priceEffect(const CalculationReportShare& share);

    /**
     * @brief Entwicklung einer Aktie im Depotwert-Chart, aus ihren
     * Bestandteilen — dieselbe Formel wie PortfolioSeriesCalculator::compute()
     * für die Summe.
     */
    static double chartDevelopment(const PortfolioSharePoint& point);

    /**
     * @brief Entwicklung einer Aktie auf der Depotwert-Linie "Aktuell (im
     * Depot)" (ergänzt 01.11.2026): Bestandswert minus Einzahlung der
     * gehaltenen Stücke inklusive anteiliger Kaufkosten — dieselbe Formel
     * wie PortfolioSeriesCalculator::compute() für die Summe.
     */
    static double chartDevelopmentHeld(const PortfolioSharePoint& point);

    /**
     * @brief Abgleich Tabelle gegen Chart einer Aktie (sieben Zeilen, seit
     * 01.11.2026 mit den zweiten Linien beider Charts).
     * @return Leer, wenn die Aktie keine Tageswerte oder keinen Chartpunkt hat.
     */
    static QList<CalculationCheck> shareChecks(const CalculationReportShare& share);

    /// Auffälligkeiten einer Aktie, in der Reihenfolge des Berichts.
    static CalculationFindings shareFindings(const CalculationReportShare& share);

    /// Anzeigename: Name, ersatzweise die GUID.
    static QString shareTitle(const CalculationReportShare& share);

    // ── Transaktionen je Aktie (ergänzt 17.11.2026, v1.30.1) ─────────────
    /**
     * @brief Alle Käufe mit zugeordneten Kosten, gespeicherter und nach FIFO
     * nachgerechneter Verkaufsmenge, Werten in heutiger Skala und Summen.
     *
     * Kosten je Kauf: alle Kosteneinträge mit BrokerageObject::buyGuid() des
     * Kaufs oder der Kosteneintrag aus BuyObject::brokerageGuid(), jeweils
     * Gebühren abzüglich Rabatt. FIFO über replayFifoSold().
     */
    static CalculationBuyTable buyTable(const CalculationReportShare& share);
    /**
     * @brief Spielt alle Verkäufe einer Aktie FIFO gegen ihre Käufe nach und
     * liefert je Kauf die verkaufte Menge in dessen Beleg-Skala.
     *
     * Wie die Verkaufsmaske (ModelSaleEdit/SaleFifoAllocator): je Verkauf nur
     * Käufe desselben Depots (leere Depotnummer = alle) mit Kaufdatum bis
     * einschließlich Verkaufsdatum, älteste zuerst. Bis v1.30.0 in
     * CalculationReport; dort bleibt replayFifoSold() als Weiterleitung.
     *
     * @return Gleiche Reihenfolge wie @p buys.
     */
    static QList<double> replayFifoSold(const QList<BuyObject>& buys,
                                        const QList<SaleObject>& sales,
                                        const QList<ShareSplitObject>& splits);
    /// Alle Verkäufe mit Auszahlung und Summen.
    static CalculationSaleTable saleTable(const CalculationReportShare& share);
    /// Alle Dividenden mit aufgeschlüsselten Steuern und Summen.
    static CalculationDividendTable dividendTable(const CalculationReportShare& share);
    /// Alle Kosteneinträge mit Zuordnung und Summen.
    static CalculationBrokerageTable brokerageTable(const CalculationReportShare& share);
    /// Bestand in heutiger Skala: gekauft, verkauft, rechnerisch, nach FIFO, Tabelle.
    static CalculationStockSummary stockSummary(const CalculationReportShare& share);

    // ── Gesamt ────────────────────────────────────────────────────────────

    /// Footer-Werte beider Tabs über alle Aktien.
    static CalculationFooterTotals footerTotals(const QList<CalculationReportShare>& shares);

    /**
     * @brief Gegenüberstellung der beiden Footer (zehn Zeilen). Nur im Tab —
     * der Bericht listet die Footer weiterhin getrennt.
     */
    static QList<CalculationBreakdownRow> footerRows(const CalculationFooterTotals& totals);

    /// Abgleich Footer gegen Chart (fünf Zeilen seit 01.11.2026), siehe
    /// CalculationTotalChecks.
    static CalculationTotalChecks totalChecks(const CalculationReportInput& input,
                                              const CalculationFooterTotals& totals);

    /// Alle Auffälligkeiten: je Aktie, danach Footer gegen Chart — genau die
    /// Liste von Teil 3 des Berichts.
    static CalculationFindings findings(const CalculationReportInput& input);

    // ── Baustein ──────────────────────────────────────────────────────────

    /**
     * @brief Vergleicht einen Tabellen- mit einem Chartwert.
     *
     * Zwei bekannte Ursachen gelten als Erklärung, wenn die Differenz genau
     * in ihnen aufgeht: der Kursunterschied (@p priceExplained, siehe
     * priceEffect()) und Aktien ohne Kursdaten, die in der Tabelle zählen,
     * im Chart aber fehlen (@p excludedExplained, ergänzt 26.10.2026). Bleibt
     * ein Rest, ist es eine Abweichung, und beide Anteile werden neben dem
     * Rest genannt.
     *
     * @param subject   Aktienname oder "GESAMT" für die Befundzeile.
     * @param hintOnly  Eine Differenz ist erwartbar und kein Fehler (aktueller
     *                  Kurs gegen letzten Tageswert) — sie wird als Hinweis
     *                  geführt.
     */
    static CalculationCheck check(const QString& subject, const QString& label,
                                  double tableValue, double chartValue, bool isVolume,
                                  bool hintOnly = false, double priceExplained = 0.0,
                                  double excludedExplained = 0.0);
};

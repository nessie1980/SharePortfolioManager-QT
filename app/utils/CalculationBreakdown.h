// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "CalculationReport.h"

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
    Explained,  ///< Differenz geht genau in bekannten Ursachen auf (Kurs, Aktien ohne Tageswerte)
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
    double excludedCurValue = 0.0;   ///< Kurswert der Aktien ohne Tageswerte
    double excludedProfit   = 0.0;   ///< Kompl. Entwicklung der Aktien ohne Tageswerte
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
     * @brief Abgleich Tabelle gegen Chart einer Aktie (fünf Zeilen).
     * @return Leer, wenn die Aktie keine Tageswerte oder keinen Chartpunkt hat.
     */
    static QList<CalculationCheck> shareChecks(const CalculationReportShare& share);

    /// Auffälligkeiten einer Aktie, in der Reihenfolge des Berichts.
    static CalculationFindings shareFindings(const CalculationReportShare& share);

    /// Anzeigename: Name, ersatzweise die GUID.
    static QString shareTitle(const CalculationReportShare& share);

    // ── Gesamt ────────────────────────────────────────────────────────────

    /// Footer-Werte beider Tabs über alle Aktien.
    static CalculationFooterTotals footerTotals(const QList<CalculationReportShare>& shares);

    /**
     * @brief Gegenüberstellung der beiden Footer (zehn Zeilen). Nur im Tab —
     * der Bericht listet die Footer weiterhin getrennt.
     */
    static QList<CalculationBreakdownRow> footerRows(const CalculationFooterTotals& totals);

    /// Abgleich Footer gegen Chart (drei Zeilen), siehe CalculationTotalChecks.
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
     * priceEffect()) und Aktien ohne Tageswerte, die in der Tabelle zählen,
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

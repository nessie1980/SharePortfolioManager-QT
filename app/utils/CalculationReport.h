// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "ShareCalculator.h"
#include "PortfolioSeriesCalculator.h"

#include "../models/ShareObject.h"
#include "../models/BuyObject.h"
#include "../models/SaleObject.h"
#include "../models/DividendObject.h"
#include "../models/BrokerageObject.h"
#include "../models/ShareSplitObject.h"

#include <QDateTime>
#include <QList>
#include <QString>

/**
 * @brief Alles, was der Berechnungsbericht über eine einzelne Aktie wissen muss.
 *
 * Gefüllt von CalculationReport::collect(); in Tests auch direkt befüllbar,
 * damit CalculationReport::build() ohne Datenbank prüfbar bleibt.
 */
struct CalculationReportShare
{
    ShareObject             share;       ///< Stammdaten, Kurs der Tabelle (curPrice)
    QList<BuyObject>        buys;        ///< Alle Käufe, Beleg-Skala
    QList<SaleObject>       sales;       ///< Alle Verkäufe, Beleg-Skala
    QList<DividendObject>   dividends;   ///< Alle Dividenden
    QList<BrokerageObject>  brokerages;  ///< Alle Kosteneinträge (kauf-, verkaufsgebunden, freistehend)
    QList<ShareSplitObject> splits;      ///< Alle Splits

    /// Exakt das, woraus MainWindow die Tabellenzeilen und Footer füllt:
    /// ShareCalculator::compute(guid, curPrice, prevDayPrice).
    ShareValues values;

    /// Werte der Aktie am letzten Stichtag des Charts (PortfolioSeriesCalculator
    /// nur über diese Aktie, gesamte Historie). Ungültiges Datum = kein Punkt.
    PortfolioSharePoint chart;

    /// true, wenn die Aktie mangels Tageswerten aus beiden Charts fällt.
    bool chartExcluded = false;
};

/**
 * @brief Eingangsdaten des Berechnungsberichts.
 */
struct CalculationReportInput
{
    QDateTime created;                     ///< Zeitpunkt der Erstellung
    QString   portfolioPath;               ///< Pfad der Portfolio-Datenbank
    QList<CalculationReportShare> shares;  ///< Alle Aktien des Portfolios, auch verkaufte

    /// Letzter Punkt der portfolioweiten Chart-Berechnung — genau der Wert,
    /// auf dem Depotwert- und Marktwert-Chart am rechten Rand enden, wenn
    /// der Zeitraum bis heute reicht. Ungültiges Datum = keine Kurve.
    PortfolioSeriesPoint chartTotal;

    /// true für die anonymisierte Fassung (siehe CalculationReport::anonymize()):
    /// der Kopf weist dann darauf hin, was ersetzt wurde und was nicht.
    bool anonymized = false;
};

/**
 * @brief Berechnungsbericht als Textdatei (ergänzt 23.10.2026).
 *
 * Diagnose-Werkzeug, angestoßen von Nessies Abnahme des Marktwert-Charts
 * (19.10.2026): Die Summe im Tab "Kompletter Marktwert" passte nicht zum
 * Endwert des Marktwert-Charts. Der Bericht legt jede Rechnung offen, damit
 * sich solche Abweichungen einer Aktie und einem Term zuordnen lassen —
 * siehe ARCHITECTURE.md, "Berechnungsbericht".
 *
 * Aufbau (Nessies Vorgaben vom 19./23.10.2026):
 * - Teil 1, je Aktie (auch vollständig verkaufte): Transaktionen, Splits,
 *   Bestandsabgleich mit FIFO-Nachrechnung, alle Werte der Tabellen
 *   "Kompletter Marktwert" und "Kompletter Depotwert" mit Rechenweg, die
 *   Chart-Werte am letzten Stichtag und der Abgleich Tabelle gegen Chart.
 * - Teil 2, Gesamtübersicht: je Aktie eine Zeile, Summen, die Footer-Werte
 *   genau so, wie MainWindow sie berechnet, die Chart-Endwerte und die
 *   Differenzen.
 * - Teil 3: Liste aller Auffälligkeiten.
 *
 * Bewusst reine Diagnose — der Bericht korrigiert nichts, er rechnet nur mit
 * denselben Funktionen nach, die Tabellen, Footer und Charts verwenden.
 *
 * Zweiteilig wie PresenterPortfolioChart/PortfolioSeriesCalculator:
 * collect() liest die Datenbank, build() formatiert datenbankfrei und ist
 * dadurch direkt testbar.
 *
 * Die Texte sind bewusst nicht über tr() geführt: der Bericht ist eine
 * Diagnose-Datei für die Fehlersuche, keine Oberfläche.
 */
class CalculationReport
{
public:
    CalculationReport() = delete;

    /// Geldbeträge gelten ab dieser Differenz als abweichend (halber Cent).
    static constexpr double kMoneyTolerance  = 0.005;
    /// Stückzahlen gelten ab dieser Differenz als abweichend.
    static constexpr double kVolumeTolerance = 0.00005;
    /// Nach so vielen Datenzeilen wiederholt jede Tabelle ihre Titelleiste
    /// (Überschrift und Linie) — Nessies Wunsch 23.10.2026.
    static constexpr int kHeaderRepeatRows = 40;

    /**
     * @brief Liest alle Aktien samt Transaktionen aus der geöffneten
     * Datenbank und rechnet Tabellen- und Chart-Werte nach.
     *
     * Tabellenwerte über ShareCalculator::compute() mit Kurs und Vortageskurs
     * der Aktie (wie MainWindow), Chart-Werte über ModelPortfolioChart und
     * PortfolioSeriesCalculator über die gesamte Historie (wie die Charts).
     *
     * @param created        Zeitstempel für den Kopf.
     * @param portfolioPath  Pfad für den Kopf.
     */
    static CalculationReportInput collect(const QDateTime& created,
                                          const QString& portfolioPath);

    /**
     * @brief Baut den vollständigen Berichtstext.
     * @param input  Siehe CalculationReportInput.
     * @return Text mit Zeilenumbrüchen "\n", Zahlen im aktuellen Locale.
     */
    static QString build(const CalculationReportInput& input);

    /**
     * @brief Schreibt @p text als UTF-8 mit BOM nach @p path.
     *
     * Mit BOM, damit ältere Windows-Editoren die Umlaute richtig erkennen —
     * gleiche Begründung wie beim Diagnose-Export der Charts.
     *
     * @param errorMessage  Bei Fehlschlag die Fehlermeldung der Datei, sonst
     *                      unverändert; darf nullptr sein.
     * @return true bei Erfolg.
     */
    static bool writeToFile(const QString& path, const QString& text,
                            QString* errorMessage = nullptr);

    /**
     * @brief Anonymisierte Kopie der Eingangsdaten (ergänzt 24.10.2026).
     *
     * Nessies Vorgabe: ein Menüeintrag schreibt zwei Dateien, eine mit
     * Realwerten und eine anonymisierte. Ersetzt werden
     * - Aktienname → "Aktie 1", "Aktie 2", … in der Reihenfolge des Berichts,
     * - WKN → "WKN-1", … und ISIN → "ISIN-1", … (leere bleiben leer),
     * - Depotnummer → "Depot 1", "Depot 2", … in der Reihenfolge des ersten
     *   Auftretens; gleiche Nummer ergibt gleiche Bezeichnung, auch über
     *   Aktien hinweg,
     * - Portfolio-Pfad → "(anonymisiert)".
     * Beträge, Kurse, Stückzahlen und Daten bleiben unverändert — sie sind
     * der Inhalt der Analyse. Ordernummern und Dokumentpfade erscheinen im
     * Bericht nicht und werden nur zur Sicherheit geleert.
     */
    static CalculationReportInput anonymize(const CalculationReportInput& input);

    /**
     * @brief Dateiname der anonymisierten Fassung neben @p path:
     * "bericht.txt" → "bericht-anonymisiert.txt"; ohne Endung wird ".txt"
     * angehängt.
     */
    static QString anonymizedFilePath(const QString& path);

    /**
     * @brief Spielt alle Verkäufe einer Aktie FIFO gegen ihre Käufe nach und
     * liefert je Kauf die verkaufte Menge in dessen Beleg-Skala.
     *
     * Wie die Verkaufsmaske (ModelSaleEdit/SaleFifoAllocator): je Verkauf nur
     * Käufe desselben Depots (leere Depotnummer = alle) mit Kaufdatum bis
     * einschließlich Verkaufsdatum, älteste zuerst. Dient dem Vergleich mit
     * dem gespeicherten BuyObject::volumeSold(), aus dem ShareCalculator den
     * Bestand ableitet.
     *
     * @return Gleiche Reihenfolge wie @p buys.
     */
    static QList<double> replayFifoSold(const QList<BuyObject>& buys,
                                        const QList<SaleObject>& sales,
                                        const QList<ShareSplitObject>& splits);
};

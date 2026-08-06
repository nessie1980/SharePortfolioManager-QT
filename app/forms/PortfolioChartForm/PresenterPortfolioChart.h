// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IViewPortfolioChart.h"
#include "IModelPortfolioChart.h"

#include "../../utils/PortfolioSeriesCalculator.h"

#include <QObject>
#include <QDate>
#include <QList>
#include <QString>
#include <QStringList>

/**
 * @brief Presenter des Depotwert-Charts (MVP).
 *
 * Interaktiv wie PresenterChart: jede Änderung an Start-Datum, Interval oder
 * Anzahl liest die aktuellen Werte über die Getter neu aus und baut die Kurve
 * neu auf — dieselbe "ein Refresh-Slot liest alle Getter neu"-Konvention.
 *
 * "Start-Datum" ist zugleich das ENDE des dargestellten Zeitraums, genau wie
 * in ChartForm: Start-Datum = 05.08.2026, Interval = Jahr, Anzahl = 1 ergibt
 * den Zeitraum 05.08.2025 bis 05.08.2026.
 *
 * @note Die Portfoliodaten werden einmalig in loadAndDisplay() geladen und
 * gehalten (m_input). Ein Refresh der Steuerelemente rechnet nur neu, ohne
 * die Datenbank erneut zu lesen — bei vielen Aktien mit langer Historie ist
 * das der teure Teil. Nach einer Kursaktualisierung oder einem
 * Portfoliowechsel ruft MainWindow reload() auf, was den Cache verwirft.
 */
class PresenterPortfolioChart : public QObject
{
    Q_OBJECT

public:
    explicit PresenterPortfolioChart(IViewPortfolioChart* view,
                                     IModelPortfolioChart* model,
                                     QObject* parent = nullptr);

    /** Lädt die Portfoliodaten, setzt das Vorgabe-Start-Datum und zeichnet. */
    void loadAndDisplay();

    /**
     * @brief Verwirft den Datencache und lädt neu.
     *
     * Aufgerufen nach einer Kursaktualisierung oder einem Portfoliowechsel.
     * Die aktuelle Zeitraum-Einstellung bleibt erhalten — nur die Daten
     * darunter werden erneuert.
     */
    void reload();

    // ── Für Tests direkt erreichbar (public static, siehe ARCHITECTURE.md) ──

    /**
     * @brief Beginn des Zeitraums, rückwärts von @p rangeEnd gerechnet.
     * @param rangeEnd  Das Start-Datum aus der View (= Ende des Zeitraums).
     * @param unit      Interval-Einheit.
     * @param count     Anzahl der Einheiten, mindestens 1.
     */
    static QDate computeRangeStart(const QDate& rangeEnd, IntervalUnit unit, int count);

    /**
     * @brief Grösste zulässige "Anzahl", bei der das Fenster gerade noch bis
     * zum ältesten vorhandenen Tageswert zurückreicht.
     *
     * Schleife statt geschlossenem Ausdruck, weil addMonths()/addYears() den
     * Tag clampen (31.01. minus 1 Monat = 28./29.02.) — gleiche Begründung
     * und gleiche zwei Sicherheitsbremsen wie in
     * PresenterChart::computeMaxIntervalCount().
     *
     * @param rangeEnd      Aktuelles Start-Datum.
     * @param unit          Aktuelle Interval-Einheit.
     * @param earliestDate  Ältester vorhandener Tageswert, ggf. ungültig.
     * @return Mindestens 1.
     */
    static int computeMaxIntervalCount(const QDate& rangeEnd, IntervalUnit unit,
                                       const QDate& earliestDate);

    /**
     * @brief Warnzeile für Aktien ohne Tageswert-Historie.
     * @param shareNames  Namen der ausgeschlossenen Aktien.
     * @return Fertiger Text, oder ein leerer String bei leerer Liste.
     */
    static QString buildWarningText(const QStringList& shareNames);

    /**
     * @brief "Zeitraum: ... / Entwicklung: X € (Y %)" für die Kopfzeile.
     * @param from    Beginn des Zeitraums.
     * @param to      Ende des Zeitraums.
     * @param points  Berechnete Kurvenpunkte; leer ergibt nur die Zeitraum-Angabe.
     */
    static QString buildRangeInfo(const QDate& from, const QDate& to,
                                  const QList<PortfolioChartPoint>& points);

    /**
     * @brief Baut den Diagnose-Export als CSV-Text.
     *
     * Ergänzt 06.08.2026 zur Fehlersuche an realen Portfolios: aus dem
     * gezeichneten Chart allein liess sich nicht ablesen, welcher Term eine
     * Auffälligkeit verursacht. Der Export enthält zwei Blöcke — je Aktie die
     * Anzahl geladener Käufe, Verkäufe, Dividenden, Kosteneinträge und
     * Tageswerte samt der ungültigen Datumsangaben, und je Stichtag alle
     * sechs Bestandteile der Formel.
     *
     * Rechnet mit dem aktuell eingestellten Zeitraum. Das Schreiben der Datei
     * übernimmt die View — der Presenter formatiert nur.
     *
     * @return Vollständiger CSV-Text, Semikolon als Trennzeichen.
     */
    QString buildDiagnosticsCsv() const;

public slots:
    /** Reagiert auf jede Änderung an Start-Datum, Interval oder Anzahl. */
    void onControlsChanged();

private:
    void refresh();

    static QString formatEuro(double value);
    static QString formatPercent(double value);

    /** Absolute Notbremse gegen die Schleife in computeMaxIntervalCount(),
     *  greift nur bei korrupten Datumsdaten — gleiche Konstante und gleiche
     *  Begründung wie in PresenterChart. */
    static constexpr int kAbsoluteSafetyCeiling = 1000000;

    IViewPortfolioChart*  m_view;
    IModelPortfolioChart* m_model;

    /** Zwischengehaltene Portfoliodaten, siehe Klassendoku. */
    QList<PortfolioShareSeriesInput> m_input;

    /** True, sobald mindestens eine Aktie mit Tageswert-Historie geladen ist.
     *  refresh() macht davor nichts, damit ein während des View-Aufbaus
     *  ausgelöstes onControlsChanged() nicht gegen leere Daten läuft. */
    bool m_hasData = false;
};

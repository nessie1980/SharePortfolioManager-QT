// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QColor>
#include <QDate>
#include <QList>
#include <QString>
#include <QStringList>

// IntervalUnit wird bewusst aus ChartTypes.h wiederverwendet statt neu
// definiert: die Zeitraumsteuerung (Start-Datum / Interval / Anzahl) ist in
// beiden Charts fachlich dieselbe, und eine zweite Aufzählung mit denselben
// vier Werten würde bei jeder Änderung an einer Stelle vergessen.
#include "../ChartForm/ChartTypes.h"

/**
 * @brief Welche Linie ein Portfolio-Chart zeigt (ergänzt 17.10.2026).
 *
 * Depotwert-Chart und Marktwert-Chart teilen sich Model, Presenter und View;
 * der Modus wird beim Anlegen festgelegt und ändert sich danach nicht —
 * gleiche Bauweise wie der Depotwert-/Marktwert-Modus von ViewShareDetails.
 * Siehe ARCHITECTURE.md, "PortfolioChartForm-Details", Abschnitt
 * "Marktwert-Chart".
 */
enum class PortfolioChartMode
{
    Development, ///< Depotwert-Chart: Entwicklung in Euro und Prozent
    MarketValue  ///< Marktwert-Chart: Wert der gehaltenen Stücke in Euro
};

/**
 * @brief Ein Punkt der Portfolio-Kurve.
 *
 * Bereits fertig gerechnet von PresenterPortfolioChart über
 * PortfolioSeriesCalculator — die View formatiert nur noch. Der Presenter
 * füllt immer alle Werte; welche Linie gezeichnet wird, entscheidet der
 * PortfolioChartMode der View.
 */
struct PortfolioChartPoint
{
    QDate  date;                  ///< Stichtag
    double development    = 0.0;  ///< Entwicklung in Euro
    double developmentPct = 0.0;  ///< Dieselbe Entwicklung in Prozent
    double marketValue    = 0.0;  ///< Marktwert in Euro (ergänzt 17.10.2026)
    /// Mindestens eine Aktie ist an diesem Stichtag mit ihrem aktuellen Kurs
    /// statt eines Schlusskurses bewertet (ergänzt 30.10.2026); die View
    /// kennzeichnet den Punkt im Tooltip mit "(aktueller Kurs)".
    bool   currentPrice   = false;

    // ── Zweite Linie je Chart (ergänzt 01.11.2026) ──────────────────────
    /// Depotwert-Chart, Linie "Aktuell (im Depot)": wie die Spalte
    /// "Aktuelle Entwicklung" — Bestandswert minus Einzahlung der gehaltenen
    /// Stücke inklusive anteiliger Kaufkosten, ohne Dividenden.
    double developmentHeld     = 0.0;
    double developmentHeldPct  = 0.0;  ///< developmentHeld in Prozent der Einzahlung
    /// Marktwert-Chart, Linie "Komplett (alle Aktien)": wie "Kpl. Marktwert"
    /// — marketValue plus alle Verkäufe brutto bis zum Stichtag.
    double marketValueComplete = 0.0;
};

/**
 * @brief Alles, was die View zum Zeichnen braucht.
 */
struct PortfolioChartData
{
    QList<PortfolioChartPoint> points; ///< Kurvenpunkte, aufsteigend nach Datum
};

/**
 * @brief Farben der Entwicklungskurve.
 *
 * Die Kurve wird abschnittsweise nach Vorzeichen eingefärbt (Nessies Vorgabe
 * 05.08.2026), analog zur Einfärbung der Entwicklungs-Spalten im
 * Portfolio-Grid. Als benannte Konstanten statt Literale im Zeichencode —
 * gleiche Konvention wie kOlderBuyColor / kOlderSaleColor in ChartForm.
 */
inline const QColor kPortfolioGainColor = QColor(0, 140, 0);    ///< Entwicklung >= 0
inline const QColor kPortfolioLossColor = QColor(200, 0, 0);    ///< Entwicklung < 0
inline const QColor kPortfolioZeroLineColor = QColor(140, 140, 140); ///< Null-Linie

/** Farbe der Marktwert-Linie (ergänzt 17.10.2026). Eine einzige Farbe statt
 *  der Gewinn-/Verlust-Einfärbung: der Marktwert ist nie negativ, eine
 *  Einfärbung nach Vorzeichen wäre also inhaltsleer. */
inline const QColor kPortfolioMarketValueColor = QColor(0, 90, 170);

/** Farben der zweiten Linien (ergänzt 01.11.2026, Nessies Vorgabe).
 *  Depotwert-Chart "Aktuell (im Depot)": nach Vorzeichen dunkelblau (>= 0)
 *  bzw. hellblau (< 0), damit sie sich von der grün/roten Linie "Komplett"
 *  abhebt und das Vorzeichen trotzdem erkennbar bleibt. Marktwert-Chart
 *  "Komplett (alle Aktien)": einfarbig grau-grün, ein Marktwert ist nie
 *  negativ. */
inline const QColor kPortfolioHeldGainColor       = QColor(20, 50, 140);
inline const QColor kPortfolioHeldLossColor       = QColor(110, 170, 235);
inline const QColor kPortfolioMarketCompleteColor = QColor(95, 140, 120);

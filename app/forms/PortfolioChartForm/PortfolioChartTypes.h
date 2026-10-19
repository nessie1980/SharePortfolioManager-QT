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

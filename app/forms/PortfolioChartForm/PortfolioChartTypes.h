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
 * @brief Ein Punkt der Portfolio-Entwicklungskurve.
 *
 * Bereits fertig gerechnet von PresenterPortfolioChart über
 * PortfolioSeriesCalculator — die View formatiert nur noch.
 */
struct PortfolioChartPoint
{
    QDate  date;                  ///< Stichtag
    double development    = 0.0;  ///< Entwicklung in Euro
    double developmentPct = 0.0;  ///< Dieselbe Entwicklung in Prozent
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

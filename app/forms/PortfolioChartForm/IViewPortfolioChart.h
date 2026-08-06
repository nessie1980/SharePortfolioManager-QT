// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "PortfolioChartTypes.h"

#include <QDate>
#include <QString>
#include <QStringList>

/**
 * @brief Passives View-Interface des Depotwert-Charts.
 *
 * Implementiert von ViewPortfolioChart (Produktion, einbettbares QWidget —
 * kein QDialog, da MainWindow es als eigenen Tab aufnimmt) und von einem Fake
 * in tst_portfoliochartform.cpp.
 *
 * Aufteilung in zwei Richtungen, gleiche Konvention wie IViewChart:
 * Getter liest der Presenter bei jeder Bedienung neu aus, Setter schreibt er
 * fertig formatiert hinein. Die View formatiert nichts selbst.
 */
class IViewPortfolioChart
{
public:
    virtual ~IViewPortfolioChart() = default;

    // ── Steuerelemente (vom Presenter gelesen) ────────────────────────────
    virtual QDate        startDate() const = 0;
    virtual IntervalUnit intervalUnit() const = 0;
    virtual int          intervalCount() const = 0;

    // ── Anzeige (vom Presenter geschrieben) ───────────────────────────────
    /** Setzt das Start-Datum einmalig beim ersten Laden (Vorgabe: heute). */
    virtual void setDefaultStartDate(const QDate& date) = 0;

    /** Obergrenze für "Anzahl", damit das Fenster nicht über den ältesten
     *  vorhandenen Tageswert hinaus wachsen kann. */
    virtual void setMaxIntervalCount(int maxCount) = 0;

    /** Ersetzt die dargestellte Kurve vollständig. */
    virtual void setChartData(const PortfolioChartData& data) = 0;

    /** Hinweis anstelle des Charts, z.B. ohne Daten im gewählten Zeitraum. */
    virtual void showEmptyChart(const QString& message) = 0;

    /**
     * @brief Zwischenanzeige während der Aggregation (Nessies Vorgabe
     * 05.08.2026). Der Presenter ruft dies vor dem Rechnen auf; der nächste
     * setChartData()- oder showEmptyChart()-Aufruf löst sie wieder ab.
     */
    virtual void showCalculating(const QString& message) = 0;

    /**
     * @brief Warnzeile unterhalb des Charts für Aktien ohne
     * Tageswert-Historie. Leerer Text blendet die Zeile aus.
     */
    virtual void setWarning(const QString& message) = 0;

    /** "Zeitraum: dd.MM.yyyy - dd.MM.yyyy / Entwicklung: X € (Y %)". */
    virtual void setRangeInfo(const QString& infoText) = 0;

    virtual void showError(const QString& message) = 0;
};

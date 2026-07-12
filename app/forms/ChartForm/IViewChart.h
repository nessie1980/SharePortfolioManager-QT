// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "ChartTypes.h"

#include <QString>
#include <QDate>

/**
 * @brief Passive view interface for the "Aktien-Chart" tab (ported from the
 * C# reference's Chart tab in FrmShareDetails).
 *
 * Implemented by ViewChart (production, embeddable QWidget — not a QDialog,
 * since it lives inside ViewShareDetails' QTabWidget) and by a fake in
 * tst_chartform.cpp for isolated PresenterChart tests.
 *
 * Split into two directions, same convention as IViewBrokerageEdit etc.:
 * - Getters: PresenterChart reads the current control values (Start-Datum/
 *   Interval/Anzahl/Selektion-Checkboxen) when the user changes any of them.
 * - Setters: PresenterChart pushes fully formatted display data.
 */
class IViewChart
{
public:
    virtual ~IViewChart() = default;

    // ── Control values (read by the Presenter) ─────────────────────────────
    virtual QDate       startDate() const = 0;
    virtual IntervalUnit intervalUnit() const = 0;
    virtual int         intervalCount() const = 0;
    virtual bool        isSeriesSelected(SeriesKind kind) const = 0;

    // ── Display data (written by the Presenter) ────────────────────────────
    /** Sets the default Start-Datum once, at initial load (e.g. latest DailyValue date). */
    virtual void setDefaultStartDate(const QDate& date) = 0;

    /**
     * @brief Upper bound for "Anzahl" — the largest value for which the
     * displayed window still reaches back to, but not past, the oldest
     * available daily value. Called on every refresh(), independent of
     * whether the bound actually changed since the last call (ergänzt
     * 12.07.2026, auf Nessies Vorgabe: weiteres Erhöhen soll gestoppt
     * werden, sobald der älteste vorhandene Wert bereits dargestellt wird).
     */
    virtual void setMaxIntervalCount(int maxCount) = 0;

    /**
     * @brief Replaces the plotted series entirely (all currently visible
     * series at once, since QtCharts axis ranges depend on all of them together).
     */
    virtual void setChartData(const QList<ChartSeriesData>& series) = 0;

    /** Empty-state message shown in place of the chart (e.g. no daily values in range). */
    virtual void showEmptyChart(const QString& message) = 0;

    virtual void setLegendEntries(const LegendEntries& entries) = 0;

    /**
     * @brief Vertical marker lines for buys/sales within the currently
     * displayed date range (ergänzt 12.07.2026, portiert vom C#-Referenz-
     * Verhalten). Called after setChartData() in the same refresh, so the
     * View can anchor the lines to the just-rebuilt price axis range.
     */
    virtual void setReferenceLines(const QList<ChartReferenceLine>& lines) = 0;

    /**
     * @brief "Zeitraum: dd.MM.yyyy - dd.MM.yyyy / Entwicklung: X€ (Y %)" —
     * mirrors the C# reference's window-title suffix. ViewShareDetails
     * appends this to its own window title (share name); empty string means
     * "no range info yet" (e.g. no data at all for this share).
     */
    virtual void setRangeInfo(const QString& infoText) = 0;

    virtual void showError(const QString& message) = 0;
};

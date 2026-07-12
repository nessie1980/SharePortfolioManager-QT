// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IViewChart.h"
#include "ModelChart.h"
#include "PresenterChart.h"

#include <QWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QDateEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QMap>

#include <QChart>
#include <QChartView>
#include <QValueAxis>
#include <QDateTimeAxis>
#include <QPointF>

class QLineSeries;

/**
 * @brief Embeddable "Aktien-Chart" widget — ported from the C# reference's
 * Chart tab in FrmShareDetails (see ARCHITECTURE.md, "ChartForm-Details").
 *
 * Deliberately a QWidget, not a QDialog: unlike the other *Edit dialogs, this
 * form has no independent life cycle of its own — ViewShareDetails embeds it
 * directly as tab 1 of its QTabWidget (setupChartTab()), same relationship
 * the C# reference has between FrmShareDetails and its Chart tab.
 *
 * Layout: chart (QChartView) on the left, "Legende" + "Selektion" boxes on
 * the right — matches the C# reference screenshot. QtCharts' own legend is
 * hidden; the "Legende" box on the right is populated manually from
 * PresenterChart-formatted LegendEntry rows (color swatch + text), since it
 * needs to show more than series names (Min/Max, Letzter Kauf/Verkauf).
 *
 * Pure MVP View: no repository access, no formatting/business logic. All
 * data arrives already formatted via IViewChart, computed by PresenterChart
 * from ModelChart.
 */
class ViewChart : public QWidget, public IViewChart
{
    Q_OBJECT

public:
    explicit ViewChart(const QString& shareGuid, QWidget* parent = nullptr);
    ~ViewChart() override = default;

    // ── IViewChart: getters (read by the Presenter) ─────────────────────────
    QDate        startDate() const override;
    IntervalUnit intervalUnit() const override;
    int          intervalCount() const override;
    bool         isSeriesSelected(SeriesKind kind) const override;

    // ── IViewChart: setters (written by the Presenter) ──────────────────────
    void setDefaultStartDate(const QDate& date) override;
    void setChartData(const QList<ChartSeriesData>& series) override;
    void showEmptyChart(const QString& message) override;
    void setLegendEntries(const LegendEntries& entries) override;
    void setReferenceLines(const QList<ChartReferenceLine>& lines) override;
    void setRangeInfo(const QString& infoText) override;
    void showError(const QString& message) override;

signals:
    /**
     * @brief Forwards setRangeInfo() to ViewShareDetails so it can restore
     * the C# reference's full window title ("{Name} - Zeitraum: ... /
     * Entwicklung: ..."). Empty string means "no range info yet"
     * (e.g. no daily values at all for this share) — ViewShareDetails then
     * falls back to just the share name.
     */
    void titleInfoChanged(const QString& infoText);

private:
    void setupUi();
    QGroupBox* setupSelektionBox();
    QGroupBox* setupLegendeBox();

    void clearLegendLayout();
    void rebuildAxes(const QList<ChartSeriesData>& series);

    /**
     * @brief Shows/hides a QToolTip with "{Serie}\n{Datum}: {Wert}" when the
     * mouse hovers a QLineSeries — ported from the C# reference, which shows
     * the same info via native chart tooltips. Connected once per series in
     * setChartData() (QLineSeries::hovered), since Qt Charts has no
     * chart-wide hover signal that also identifies which series was hit.
     * @param kind   Which series was hovered (for the label + unit).
     * @param point  Nearest data point to the cursor, in axis coordinates
     *               (x = msecsSinceEpoch, y = the plotted value).
     * @param state  true = entering hover, false = leaving it.
     */
    void onSeriesHovered(SeriesKind kind, const QPointF& point, bool state);

    /**
     * @brief Shows/hides a QToolTip with "{Kauf/Verkauf}\n{Datum}: {Preis}\n
     * {Stückzahl} Stk." when the mouse hovers a Kauf-/Verkauf-Markerlinie —
     * ergänzt 12.07.2026 auf Nessies Vorgabe, analog zu onSeriesHovered() für
     * die Daten-Serien. Eigener Handler statt Wiederverwendung von
     * onSeriesHovered(), da Markerlinien kein SeriesKind haben und Datum/
     * Preis/Stückzahl bereits in der ChartReferenceLine stecken (keine
     * Rückrechnung aus den Achsen-Koordinaten nötig).
     * @param line   Die gehoverte Markerlinie (Wertkopie, siehe setReferenceLines()).
     * @param state  true = entering hover, false = leaving it.
     */
    void onReferenceLineHovered(const ChartReferenceLine& line, bool state);

    // ── MVP wiring ────────────────────────────────────────────────────────
    ModelChart     m_model;
    PresenterChart m_presenter;

    // ── Chart ─────────────────────────────────────────────────────────────
    QStackedWidget* m_stack     = nullptr; ///< index 0 = chart, index 1 = empty-state label
    QChartView*     m_chartView = nullptr;
    QChart*         m_chart     = nullptr;
    QLabel*         m_emptyLabel = nullptr;
    QDateTimeAxis*  m_xAxis              = nullptr;
    QValueAxis*     m_yAxisPrice         = nullptr;
    QValueAxis*     m_yAxisHeldVolume    = nullptr; ///< "Anteile" (Depotbestand) — eigene Skala
    QValueAxis*     m_yAxisTradedVolume  = nullptr; ///< "Gehandelte Anteile" (Börsenvolumen) — eigene Skala

    /** Vertikale Kauf-/Verkauf-Markerlinien — separat von den Daten-Serien
     *  getrackt, damit setReferenceLines() sie gezielt entfernen kann, ohne
     *  die gerade erst per setChartData() gezeichneten Serien anzufassen. */
    QList<QLineSeries*> m_referenceLineSeries;

    // ── Legende box (rebuilt on every refresh) ───────────────────────────────
    QVBoxLayout* m_legendLayout = nullptr;

    // ── Selektion box ─────────────────────────────────────────────────────
    QMap<SeriesKind, QCheckBox*> m_seriesCheckBoxes;
    QDateEdit* m_startDateEdit = nullptr;
    QComboBox* m_intervalCombo = nullptr;
    QSpinBox*  m_countSpin     = nullptr;
};

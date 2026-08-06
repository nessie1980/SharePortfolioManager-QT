// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IViewPortfolioChart.h"
#include "ModelPortfolioChart.h"
#include "PresenterPortfolioChart.h"

#include <QWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QDateEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QGroupBox>
#include <QPushButton>

#include <QChart>
#include <QChartView>
#include <QValueAxis>
#include <QDateTimeAxis>
#include <QPointF>

class QLineSeries;
class QWheelEvent;

/**
 * @brief Einbettbares Widget für den Depotwert-Chart (Feature 05.08.2026).
 *
 * Bewusst ein QWidget und kein QDialog: MainWindow hängt es direkt als
 * eigenen Tab ein, es hat keinen eigenen Lebenszyklus — dieselbe Beziehung,
 * die ViewChart zu ViewShareDetails hat.
 *
 * Layout: Chart links, rechts nur der Zeitraum-Block (Start-Datum, Interval,
 * Anzahl). Eine Legende gibt es bewusst nicht — es wird genau eine Linie
 * dargestellt, eine Farbzuordnung wäre also inhaltsleer (Nessies Vorgabe
 * 05.08.2026). Unterhalb des Charts liegt eine Warnzeile, die nur erscheint,
 * wenn Aktien ohne Tageswert-Historie ausgeschlossen wurden.
 *
 * Die Kurve wird abschnittsweise nach Vorzeichen eingefärbt: Abschnitte über
 * der Null-Linie grün, darunter rot. An jedem Vorzeichenwechsel wird der
 * Schnittpunkt mit der Null-Linie linear interpoliert und in beide Abschnitte
 * aufgenommen, damit die Linie optisch geschlossen bleibt.
 *
 * Reine MVP-View: kein Repository-Zugriff, keine Formatierung, keine
 * Fachlogik. Alle Daten kommen fertig gerechnet über IViewPortfolioChart.
 */
class ViewPortfolioChart : public QWidget, public IViewPortfolioChart
{
    Q_OBJECT

public:
    explicit ViewPortfolioChart(QWidget* parent = nullptr);
    ~ViewPortfolioChart() override = default;

    // ── IViewPortfolioChart: Getter ───────────────────────────────────────
    QDate        startDate() const override;
    IntervalUnit intervalUnit() const override;
    int          intervalCount() const override;

    // ── IViewPortfolioChart: Setter ───────────────────────────────────────
    void setDefaultStartDate(const QDate& date) override;
    void setMaxIntervalCount(int maxCount) override;
    void setChartData(const PortfolioChartData& data) override;
    void showEmptyChart(const QString& message) override;
    void showCalculating(const QString& message) override;
    void setWarning(const QString& message) override;
    void setRangeInfo(const QString& infoText) override;
    void showError(const QString& message) override;

    /** Letzter per setRangeInfo() gesetzter Text — MainWindow hängt ihn an
     *  die Tab-Kopfzeile an, gleiche Rolle wie ViewChart::rangeInfo(). */
    QString rangeInfo() const { return m_lastRangeInfo; }

    /** Verwirft den Datencache des Presenters und zeichnet neu. Von
     *  MainWindow nach einer Kursaktualisierung oder einem Portfoliowechsel
     *  aufgerufen. */
    void reload() { m_presenter.reload(); }

    /** Mausrad auf "Anzahl" umleiten — auf m_countSpin unabhängig vom Fokus,
     *  auf dem Chart-Viewport nur über der Zeichenfläche. Gleiche Mechanik
     *  wie ViewChart::eventFilter(). */
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    /** Reicht setRangeInfo() an MainWindow weiter. */
    void titleInfoChanged(const QString& infoText);

private:
    void setupUi();
    QGroupBox* setupZeitraumBox();
    void rebuildAxes(const QList<PortfolioChartPoint>& points);
    void applyWheelStep(QWheelEvent* event);

private slots:
    /**
     * @brief Tooltip "Datum / Entwicklung € / Entwicklung %" beim Hovern der
     * Kurve. Als private slot deklariert, damit Tests die Formatierung per
     * QMetaObject::invokeMethod() prüfen können, ohne ein Maus-Hover über die
     * im Offscreen-Testlauf nicht verlässlich vermessbare Zeichenfläche zu
     * simulieren — gleiche Begründung wie bei ViewChart::onSeriesHovered().
     * @param point  Nächstgelegener Datenpunkt in Achsenkoordinaten.
     * @param state  true beim Betreten, false beim Verlassen.
     */
    void onSeriesHovered(const QPointF& point, bool state);

    /**
     * @brief Schreibt den Diagnose-Export als CSV-Datei.
     *
     * Ergänzt 06.08.2026 zur Fehlersuche an realen Portfolios. Den Inhalt
     * baut PresenterPortfolioChart::buildDiagnosticsCsv(); die View fragt nur
     * den Zielpfad ab und schreibt. Als private slot deklariert, damit Tests
     * ihn per QMetaObject::invokeMethod() erreichen können.
     */
    void onExportDiagnostics();

private:
    // ── MVP-Verdrahtung ───────────────────────────────────────────────────
    ModelPortfolioChart     m_model;
    PresenterPortfolioChart m_presenter;

    // ── Chart ─────────────────────────────────────────────────────────────
    QStackedWidget* m_stack             = nullptr; ///< 0 = Chart, 1 = Hinweis, 2 = Berechnung
    QChartView*     m_chartView         = nullptr;
    QChart*         m_chart             = nullptr;
    QLabel*         m_emptyLabel        = nullptr;
    QLabel*         m_calculatingLabel  = nullptr;
    QLabel*         m_warningLabel      = nullptr;
    QDateTimeAxis*  m_xAxis             = nullptr;
    QValueAxis*     m_yAxis             = nullptr;

    /**
     * Die dargestellten Datenpunkte, aufsteigend nach Datum, plus ihre
     * X-Koordinaten in Millisekunden für die Suche.
     *
     * Nötig, weil QLineSeries::hovered() NICHT den Datenpunkt liefert,
     * sondern die Cursorposition in Achsenkoordinaten (Bugfix 06.08.2026).
     * Der Tooltip rastet deshalb auf den nächstgelegenen echten Datenpunkt
     * ein, statt die Mausposition anzuzeigen — sonst änderte sich der Wert
     * bei gleichem Datum, je nachdem wie hoch der Zeiger steht.
     *
     * Die interpolierten Nulldurchgänge stehen bewusst nicht darin: sie sind
     * keine Datenpunkte und haben keinen eigenen Prozentwert.
     */
    QList<PortfolioChartPoint> m_points;
    QList<qint64>              m_pointsX;

    QString m_lastRangeInfo;

    // ── Zeitraum-Block ────────────────────────────────────────────────────
    QDateEdit* m_startDateEdit = nullptr;
    QComboBox* m_intervalCombo = nullptr;
    QSpinBox*  m_countSpin     = nullptr;
    QPushButton* m_exportButton = nullptr; ///< siehe onExportDiagnostics()
};

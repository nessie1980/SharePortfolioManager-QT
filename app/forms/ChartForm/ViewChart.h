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
class QWheelEvent;
class QGraphicsLineItem;

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
 *
 * @note **Compact-Modus (ergänzt 31.07.2026, für ChartPopup — siehe
 * ARCHITECTURE.md, "ChartPopup — Rechtsklick-Popup-Chart"):** Der optionale
 * @p compact-Konstruktor-Parameter blendet die "Selektion:"-Box (Serien-
 * Checkboxen + Start-Datum/Interval/Anzahl-Formular) aus dem sichtbaren
 * Layout aus — nur Chart + "Legende"-Box bleiben sichtbar, wie im
 * C#-Referenz-Popup (`FrmChart`). Die ausgeblendeten Widgets werden trotzdem
 * ganz normal angelegt (nur eben nie ins Layout eingehängt) — alle
 * IViewChart-Getter sowie die bestehende Mausrad-Steuerung auf m_countSpin
 * (siehe eventFilter()/applyWheelStep()) funktionieren dadurch unverändert,
 * ganz ohne Sonderfall-Code in PresenterChart. Da im Compact-Modus ohnehin
 * nur die Default-Checkbox (ClosingPrice) angehakt ist und nie umgeschaltet
 * werden kann, zeigt der Chart wie in der C#-Referenz immer nur die
 * Schluss-Kurs-Serie (+ Kauf-/Verkauf-Markerlinien).
 */
class ViewChart : public QWidget, public IViewChart
{
    Q_OBJECT

public:
    explicit ViewChart(const QString& shareGuid, bool compact = false, QWidget* parent = nullptr);
    /** Speichert Interval und gewünschte Anzahl in AppSettings (ergänzt
     *  20.09.2026, siehe AppSettings Abschnitt "Charts"). */
    ~ViewChart() override;

    // ── IViewChart: getters (read by the Presenter) ─────────────────────────
    QDate        startDate() const override;
    IntervalUnit intervalUnit() const override;
    int          intervalCount() const override;
    bool         isSeriesSelected(SeriesKind kind) const override;

    // ── IViewChart: setters (written by the Presenter) ──────────────────────
    void setDefaultStartDate(const QDate& date) override;
    void setMaxIntervalCount(int maxCount) override;
    void setChartData(const QList<ChartSeriesData>& series) override;
    void showEmptyChart(const QString& message) override;
    void setLegendEntries(const LegendEntries& entries) override;
    void setReferenceLines(const QList<ChartReferenceLine>& lines) override;
    void setRangeInfo(const QString& infoText) override;
    void showError(const QString& message) override;

    /**
     * @brief Letzter per setRangeInfo() gesetzter Text ("Zeitraum: ... /
     * Entwicklung: ..." bzw. leerer String ohne Daten) — ergänzt 31.07.2026
     * für ChartPopup. Kein Teil von IViewChart (reiner ViewChart-Getter):
     * ChartPopup verbindet sich erst NACH der Konstruktion mit
     * titleInfoChanged() (siehe ChartPopup.cpp) und würde die bereits im
     * Konstruktor (PresenterChart::loadAndDisplay()) gefeuerte erste
     * Emission sonst verpassen — derselbe Effekt, den ViewShareDetails über
     * setHeaderName() umgeht (siehe ARCHITECTURE.md, "ChartPopup —
     * Rechtsklick-Popup-Chart").
     */
    QString rangeInfo() const { return m_lastRangeInfo; }

    /**
     * @brief Routes Mausrad-Events auf m_countSpin (unabhängig vom Fokus-
     * Status, siehe applyWheelStep()) und auf m_chartView->viewport() (nur
     * wenn die Maus direkt über der Chart-Zeichenfläche steht, nicht über
     * Legende/Selektion) auf dieselbe "Anzahl"-Steuerung durch — ergänzt
     * 12.07.2026 auf Nessies Vorgabe, portiert vom C#-Referenz-Verhalten
     * ("Zeitraum per Mausrad direkt im Chart ändern").
     */
    bool eventFilter(QObject* watched, QEvent* event) override;

    /** Kennung der Cursorlinie in QGraphicsItem::data(0) (ergänzt
     *  17.10.2026) — erlaubt Tests, sie unter den übrigen Linien-Items der
     *  Szene (Achsen, Gitter) eindeutig zu finden. */
    static constexpr const char* kCursorLineDataKey = "chartCursorLine";

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
     * @brief Gemeinsame Wheel-Logik für m_countSpin und m_chartView-Viewport
     * (siehe eventFilter()) — rechnet das Wheel-Delta in "Rasten" um (gleiche
     * Formel wie Qt intern in QAbstractSpinBox::wheelEvent()) und wendet sie
     * per stepBy() auf m_countSpin an. Löst dadurch automatisch dessen
     * bestehende valueChanged()-Verbindung zu m_presenter.onControlsChanged()
     * aus — kein separater Refresh-Aufruf nötig.
     * Rad nach oben (positives angleDelta().y()) = Anzahl erhöhen (Nessies
     * Vorgabe 12.07.2026).
     */
    void applyWheelStep(QWheelEvent* event);

    /** Blendet Cursorlinie und Tooltip aus (ergänzt 17.10.2026) — beim
     *  Verlassen der Zeichenfläche, bei neuen Chart-Daten, beim Leerhinweis
     *  und bei geänderter Zeichenfläche (plotAreaChanged). */
    void hideCrosshair();

private slots:
    /**
     * @brief Cursorlinie und Tooltip an den Datenpunkt setzen, der @p x am
     * nächsten liegt (ergänzt 17.10.2026, Nessies Vorgabe).
     *
     * Ersetzt den früheren onSeriesHovered()-Tooltip je Serie: Der Tooltip
     * erscheint jetzt, sobald die Maus irgendwo in der Zeichenfläche steht,
     * nicht mehr nur knapp an einer Kurve. Die vertikale, magenta gestrichelte
     * Cursorlinie springt dabei von Datenpunkt zu Datenpunkt (Einrasten per
     * ChartPointSearch::nearestIndex() auf der ersten Daten-Serie — alle
     * Serien teilen sich dieselbe Datumsliste, siehe rebuildAxes()).
     *
     * Tooltip-Inhalt: Datum in der ersten Zeile, darunter je sichtbarer
     * Serie "{Serie}: {Wert}" — Kurse mit vier Nachkommastellen und "€",
     * Stückzahlen mit vier Nachkommastellen (Formatierung unverändert vom
     * früheren onSeriesHovered()).
     *
     * Steht die Maus auf einer Kauf-/Verkauf-Markerlinie, bleibt deren
     * eigener Tooltip (onReferenceLineHovered()) stehen; nur die Cursorlinie
     * wandert weiter.
     *
     * Aufgerufen aus eventFilter() bei MouseMove auf dem Viewport. Als
     * `private slots:` deklariert, damit tst_mainwindow.cpp den Ablauf per
     * QMetaObject::invokeMethod() prüfen kann, ohne ein echtes Maus-Hover
     * über die im headless Testlauf nicht verlässlich vermessbare
     * Zeichenfläche zu simulieren — gleiches Muster wie zuvor bei
     * onSeriesHovered().
     *
     * Ohne Daten-Serie (leerer Chart) wird nur ausgeblendet. Ist die
     * Zeichenfläche noch nicht vermessen (plotArea() leer, z. B. vor dem
     * ersten Anzeigen), erscheint nur der Tooltip, keine Linie.
     *
     * @param x  X-Koordinate der Maus in Achsenkoordinaten (msecsSinceEpoch).
     */
    void onCrosshairMoved(double x);

    /**
     * @brief Shows/hides a QToolTip with "{Kauf/Verkauf}\n{Datum}: {Preis}\n
     * {Stückzahl} Stk." when the mouse hovers a Kauf-/Verkauf-Markerlinie —
     * ergänzt 12.07.2026 auf Nessies Vorgabe. Eigener Handler, da
     * Markerlinien kein SeriesKind haben und Datum/Preis/Stückzahl bereits in
     * der ChartReferenceLine stecken (keine Rückrechnung aus den Achsen-
     * Koordinaten nötig). Als `private slots:` deklariert seit 02.08.2026 —
     * siehe Testbarkeits-Hinweis bei onCrosshairMoved() oben.
     * Merkt sich seit 17.10.2026 in m_referenceLineHovered, ob gerade eine
     * Markerlinie gehovert wird, damit onCrosshairMoved() deren Tooltip nicht
     * überschreibt.
     * @param line   Die gehoverte Markerlinie (Wertkopie, siehe setReferenceLines()).
     * @param state  true = entering hover, false = leaving it.
     */
    void onReferenceLineHovered(const ChartReferenceLine& line, bool state);

private:

    // ── MVP wiring ────────────────────────────────────────────────────────
    ModelChart     m_model;
    PresenterChart m_presenter;
    bool           m_compact = false; ///< siehe Klassendoku "Compact-Modus"

    // ── Chart ─────────────────────────────────────────────────────────────
    QStackedWidget* m_stack     = nullptr; ///< index 0 = chart, index 1 = empty-state label
    QChartView*     m_chartView = nullptr;
    QChart*         m_chart     = nullptr;
    QLabel*         m_emptyLabel = nullptr;
    QDateTimeAxis*  m_xAxis      = nullptr;
    QValueAxis*     m_yAxisPrice = nullptr;
    QValueAxis*     m_yAxisVolume = nullptr; ///< "Anteile" ODER "Gehandelte Anteile" — nie beide
                                              ///< gleichzeitig, da die Checkboxen sich gegenseitig
                                              ///< ausschließen (siehe setupSelektionBox()); Titel
                                              ///< wird dynamisch in rebuildAxes() gesetzt.

    /** Vertikale Kauf-/Verkauf-Markerlinien — separat von den Daten-Serien
     *  getrackt, damit setReferenceLines() sie gezielt entfernen kann, ohne
     *  die gerade erst per setChartData() gezeichneten Serien anzufassen. */
    QList<QLineSeries*> m_referenceLineSeries;

    /** Daten-Serie samt Art, in Anzeigereihenfolge (ergänzt 17.10.2026) —
     *  Grundlage für Einrasten und Tooltip in onCrosshairMoved(). Die
     *  Serien gehören m_chart; setChartData() leert die Liste zusammen mit
     *  removeAllSeries(). */
    struct DataSeriesRef {
        SeriesKind   kind;
        QLineSeries* line = nullptr;
    };
    QList<DataSeriesRef> m_dataSeries;

    /** Vertikale Cursorlinie (ergänzt 17.10.2026). Kind von m_chart, damit
     *  ihre Koordinaten den Chart-Koordinaten von plotArea()/mapToPosition()
     *  entsprechen; überlebt removeAllSeries(). Kennung für Tests per
     *  data(0) == kCursorLineDataKey. */
    QGraphicsLineItem* m_cursorLine = nullptr;

    /** true, solange eine Kauf-/Verkauf-Markerlinie gehovert wird — dann
     *  lässt onCrosshairMoved() den Markerlinien-Tooltip stehen. */
    bool m_referenceLineHovered = false;

    // ── Legende box (rebuilt on every refresh) ───────────────────────────────
    QVBoxLayout* m_legendLayout = nullptr;
    QString      m_lastRangeInfo; ///< siehe rangeInfo()

    // ── Selektion box ─────────────────────────────────────────────────────
    QMap<SeriesKind, QCheckBox*> m_seriesCheckBoxes;
    QDateEdit* m_startDateEdit = nullptr;
    QComboBox* m_intervalCombo = nullptr;
    QSpinBox*  m_countSpin     = nullptr;

    /**
     * Vom Benutzer gewählte Anzahl (ergänzt 20.09.2026). Kann größer sein als
     * der angezeigte Spinbox-Wert, wenn setMaxIntervalCount() wegen zu kurzer
     * Kurshistorie kürzen musste — beim Schließen wird dieser Wert
     * gespeichert, nicht der angezeigte (Nessies Vorgabe: eine Kürzung nur
     * wegen weniger Historie soll den gewählten Wert erhalten). Geändert nur
     * durch echte Benutzeränderungen an m_countSpin.
     */
    int m_desiredCount = 1;
};

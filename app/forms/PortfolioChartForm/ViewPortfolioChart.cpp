// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ViewPortfolioChart.h"

#include "../OwnMessageBoxForm/OwnMessageBox.h"
#include "../../utils/ChartPointSearch.h"
#include "../../config/AppSettings.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QPixmap>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineSeries>
#include <QPainter>
#include <QPen>
#include <QSignalBlocker>
#include <QToolTip>
#include <QCursor>
#include <QLocale>
#include <QScrollArea>
#include <QFrame>
#include <QWheelEvent>
#include <QDateTime>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>

#include <algorithm>
#include <limits>

namespace {

/// One stretch of the curve that stays on one side of the zero line.
struct CurveSegment
{
    QList<QPointF> points;
    bool           positive = true;
};

qint64 toMSecs(const QDate& date)
{
    return QDateTime(date, QTime(0, 0)).toMSecsSinceEpoch();
}

/**
 * Splits the curve at every sign change, inserting the interpolated zero
 * crossing into both adjoining segments so the coloured line stays visually
 * closed instead of showing a gap at the axis.
 */
QList<CurveSegment> buildSegments(const QList<QPointF>& points)
{
    // Seit 01.11.2026 auf beliebige Punkte verallgemeinert — der
    // Depotwert-Chart färbt beide Linien nach Vorzeichen.
    QList<CurveSegment> segments;
    if (points.isEmpty())
        return segments;

    const auto isPositive = [](double value) { return value >= 0.0; };

    CurveSegment current;
    current.positive = isPositive(points.constFirst().y());
    current.points.append(points.constFirst());

    for (int i = 1; i < points.size(); ++i) {
        const double x = points.at(i).x();
        const double y = points.at(i).y();

        if (isPositive(y) == current.positive) {
            current.points.append(QPointF(x, y));
            continue;
        }

        const QPointF& previous = current.points.constLast();
        const double   span     = previous.y() - y;

        // span == 0 cannot happen here (the signs differ), the guard only
        // protects against denormal arithmetic.
        const double crossingX = qFuzzyIsNull(span)
            ? x
            : previous.x() + (previous.y() / span) * (x - previous.x());

        current.points.append(QPointF(crossingX, 0.0));
        segments.append(current);

        current = CurveSegment{};
        current.positive = isPositive(y);
        current.points.append(QPointF(crossingX, 0.0));
        current.points.append(QPointF(x, y));
    }

    segments.append(current);
    return segments;
}

/// Häkchen-Symbol: zwei Hälften in den Farben der Linie (oben >= 0, unten
/// < 0); bei einfarbigen Linien beide Hälften gleich (ergänzt 01.11.2026).
QIcon curveIcon(const QColor& upper, const QColor& lower)
{
    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.fillRect(QRect(1, 3, 14, 5), upper);
    painter.fillRect(QRect(1, 8, 14, 5), lower);
    return QIcon(pixmap);
}

} // namespace

// ── Constructor ───────────────────────────────────────────────────────────────

ViewPortfolioChart::ViewPortfolioChart(PortfolioChartMode mode, QWidget* parent)
    : QWidget(parent)
    , m_mode(mode)
    , m_presenter(this, &m_model, mode)
{
    setObjectName(mode == PortfolioChartMode::MarketValue
                      ? QStringLiteral("ViewMarketValueChart")
                      : QStringLiteral("ViewPortfolioChart"));
    setupUi();
    m_presenter.loadAndDisplay();
}

// ── Destructor ──────────────────────────────────────────────────────────────

ViewPortfolioChart::~ViewPortfolioChart()
{
    // Zeitraum-Einstellung beim Schließen speichern (ergänzt 20.09.2026) —
    // gleiche Regeln wie ViewChart::~ViewChart(). Der Depotwert-Chart ist
    // ein Tab im Hauptfenster; "Schließen" heißt hier also Programmende.
    // Seit 17.10.2026 nur bei einer Änderung, siehe Header.
    const QString unitKey = intervalUnitToKey(intervalUnit());
    if (unitKey != m_loadedIntervalKey || m_desiredCount != m_loadedCount)
        AppSettings::instance().setPortfolioChartInterval(unitKey, m_desiredCount);

    // Sichtbare Linien (ergänzt 01.11.2026), je Chart eigene Schlüssel —
    // ebenfalls nur bei einer Änderung.
    const bool showComplete = completeCurveVisible();
    const bool showHeld     = heldCurveVisible();
    if (showComplete != m_loadedShowComplete || showHeld != m_loadedShowHeld) {
        if (m_mode == PortfolioChartMode::MarketValue)
            AppSettings::instance().setMarketChartCurves(showHeld, showComplete);
        else
            AppSettings::instance().setDepotChartCurves(showComplete, showHeld);
    }
}

// ── Kurven ────────────────────────────────────────────────────────────────────

bool ViewPortfolioChart::completeCurveVisible() const
{
    return m_completeCheck->isChecked();
}

bool ViewPortfolioChart::heldCurveVisible() const
{
    return m_heldCheck->isChecked();
}

void ViewPortfolioChart::onCurveToggled(QCheckBox* changed, QCheckBox* other)
{
    // Mindestens eine Linie bleibt sichtbar — ein leerer Chart mit Achsen
    // ohne Linie hätte keinen Nutzen.
    if (!changed->isChecked() && !other->isChecked()) {
        const QSignalBlocker blocker(changed);
        changed->setChecked(true);
        return;
    }
    redraw();
}

// ── eventFilter / applyWheelStep ──────────────────────────────────────────────

bool ViewPortfolioChart::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Wheel &&
        (watched == m_countSpin || watched == m_chartView->viewport())) {
        applyWheelStep(static_cast<QWheelEvent*>(event));
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void ViewPortfolioChart::applyWheelStep(QWheelEvent* event)
{
    // Same angleDelta-to-steps conversion Qt uses internally in
    // QAbstractSpinBox::wheelEvent(); one detent equals 15 degrees / 120 units.
    const int numSteps = (event->angleDelta().y() / 8) / 15;
    if (numSteps != 0)
        m_countSpin->stepBy(numSteps); // triggers valueChanged() -> onControlsChanged()
}

// ── setupUi ───────────────────────────────────────────────────────────────────

void ViewPortfolioChart::setupUi()
{
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // ── Chart area plus warning line ──────────────────────────────────────
    m_chart = new QChart();
    m_chart->legend()->hide(); // Zuordnung über die farbigen Häkchen im Block "Kurven" (01.11.2026)
    m_chart->setMargins(QMargins(4, 4, 4, 4));

    m_chartView = new QChartView(m_chart);
    m_chartView->setObjectName(QStringLiteral("portfolioChartView"));
    m_chartView->setRenderHint(QPainter::Antialiasing);

    const auto makeCenteredLabel = [](const QString& objectName) {
        auto* label = new QLabel();
        label->setObjectName(objectName);
        label->setAlignment(Qt::AlignCenter);
        label->setWordWrap(true);
        QFont font = label->font();
        font.setItalic(true);
        label->setFont(font);
        return label;
    };

    m_emptyLabel       = makeCenteredLabel(QStringLiteral("portfolioChartEmptyLabel"));
    m_calculatingLabel = makeCenteredLabel(QStringLiteral("portfolioChartCalculatingLabel"));

    m_stack = new QStackedWidget();
    m_stack->setObjectName(QStringLiteral("portfolioChartStack"));
    m_stack->addWidget(m_chartView);        // 0
    m_stack->addWidget(m_emptyLabel);       // 1
    m_stack->addWidget(m_calculatingLabel); // 2

    auto* leftLayout = new QVBoxLayout();
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(m_stack, 1);
    mainLayout->addLayout(leftLayout, 1);

    // ── Right panel: Zeitraum only ────────────────────────────────────────
    auto* rightContent = new QWidget();
    rightContent->setObjectName(QStringLiteral("portfolioChartRightPanel"));
    auto* rightLayout = new QVBoxLayout(rightContent);
    rightLayout->setContentsMargins(0, 0, 4, 0);
    rightLayout->addWidget(setupZeitraumBox());
    rightLayout->addWidget(setupKurvenBox());
    rightLayout->addStretch(1);

    auto* rightScroll = new QScrollArea();
    rightScroll->setObjectName(QStringLiteral("portfolioChartRightScroll"));
    rightScroll->setWidget(rightContent);
    rightScroll->setWidgetResizable(true);
    rightScroll->setFrameShape(QFrame::NoFrame);
    rightScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // Schmaler als in ChartForm (380px): dort muss die Legende mit ihren
    // langen Min/Max-Zeilen hineinpassen, hier steht nur der Zeitraum-Block.
    rightScroll->setFixedWidth(240);
    mainLayout->addWidget(rightScroll);

    // Wheel routing — see ViewChart::setupUi() for why the viewport and not
    // the QChartView itself is filtered.
    m_countSpin->installEventFilter(this);
    m_chartView->viewport()->installEventFilter(this);
}

// ── setupZeitraumBox ──────────────────────────────────────────────────────────

QGroupBox* ViewPortfolioChart::setupZeitraumBox()
{
    auto* box = new QGroupBox(tr("Zeitraum:"));
    box->setObjectName(QStringLiteral("portfolioChartZeitraumBox"));
    auto* form = new QFormLayout(box);

    m_startDateEdit = new QDateEdit(QDate::currentDate());
    m_startDateEdit->setObjectName(QStringLiteral("portfolioChartStartDateEdit"));
    m_startDateEdit->setCalendarPopup(true);
    m_startDateEdit->setDisplayFormat(QStringLiteral("dd.MM.yyyy"));
    connect(m_startDateEdit, &QDateEdit::dateChanged,
            this, [this](const QDate&) { m_presenter.onControlsChanged(); });
    form->addRow(tr("Start-Datum:"), m_startDateEdit);

    m_intervalCombo = new QComboBox();
    m_intervalCombo->setObjectName(QStringLiteral("portfolioChartIntervalCombo"));
    m_intervalCombo->addItem(tr("Tag"),   static_cast<int>(IntervalUnit::Day));
    m_intervalCombo->addItem(tr("Woche"), static_cast<int>(IntervalUnit::Week));
    m_intervalCombo->addItem(tr("Monat"), static_cast<int>(IntervalUnit::Month));
    m_intervalCombo->addItem(tr("Jahr"),  static_cast<int>(IntervalUnit::Year));
    // Gespeicherte Einheit aus settings.ini (ergänzt 20.09.2026); ohne
    // Eintrag "Jahr" — Nessies Vorgabe 05.08.2026.
    const IntervalUnit savedUnit = intervalUnitFromKey(
        AppSettings::instance().portfolioChartIntervalUnit(), IntervalUnit::Year);
    m_intervalCombo->setCurrentIndex(
        std::max(0, m_intervalCombo->findData(static_cast<int>(savedUnit))));
    m_loadedIntervalKey = intervalUnitToKey(intervalUnit());
    // "activated" statt "currentIndexChanged": nur echte Nutzerauswahl soll
    // einen Refresh auslösen, kein programmatisches setCurrentIndex().
    connect(m_intervalCombo, QOverload<int>::of(&QComboBox::activated),
            this, [this](int) { m_presenter.onControlsChanged(); });
    form->addRow(tr("Interval:"), m_intervalCombo);

    m_countSpin = new QSpinBox();
    m_countSpin->setObjectName(QStringLiteral("portfolioChartCountSpin"));
    // Platzhalter — der Presenter setzt beim ersten refresh() die tatsächliche
    // Obergrenze über setMaxIntervalCount().
    m_countSpin->setRange(1, 999);
    // Gespeicherte Anzahl (ergänzt 20.09.2026) — siehe ViewChart.
    m_desiredCount = AppSettings::instance().portfolioChartIntervalCount();
    m_loadedCount  = m_desiredCount;
    m_countSpin->setValue(m_desiredCount);
    connect(m_countSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int value) {
                // Nur echte Benutzeränderungen — setMaxIntervalCount() blockt.
                m_desiredCount = value;
                m_presenter.onControlsChanged();
            });
    form->addRow(tr("Anzahl:"), m_countSpin);

    // Diagnose-Export (ergänzt 06.08.2026) — schreibt die Bestandteile jedes
    // Stichtags als CSV, damit sich Auffälligkeiten im Kurvenverlauf einem
    // konkreten Term zuordnen lassen, statt sie über Tooltips zu suchen.
    m_exportButton = new QPushButton(tr("Diagnose speichern…"));
    m_exportButton->setObjectName(QStringLiteral("portfolioChartExportButton"));
    m_exportButton->setToolTip(
        tr("Schreibt je Aktie die geladenen Datensätze und je Stichtag alle "
           "Bestandteile der Berechnung in eine CSV-Datei."));
    connect(m_exportButton, &QPushButton::clicked,
            this, &ViewPortfolioChart::onExportDiagnostics);
    form->addRow(m_exportButton);

    // Nur sichtbar, wenn "Debug/ShowDiagnostics" in settings.ini gesetzt ist
    // (ergänzt 19.09.2026, Standard: aus). Der Knopf wird trotzdem immer
    // angelegt und nur ausgeblendet — Slot und Tests bleiben dadurch
    // unverändert. Gelesen wird einmalig hier; eine Änderung in der INI
    // greift nach einem Neustart.
    m_exportButton->setVisible(AppSettings::instance().showDiagnostics());

    return box;
}

// ── setupKurvenBox ────────────────────────────────────────────────────────────

QGroupBox* ViewPortfolioChart::setupKurvenBox()
{
    // Ergänzt 01.11.2026 (Nessies Entscheidung): je Chart zwei Linien,
    // einzeln ein- und ausblendbar. Die bisherige Linie steht oben.
    auto* box = new QGroupBox(tr("Kurven:"));
    box->setObjectName(QStringLiteral("portfolioChartKurvenBox"));
    auto* layout = new QVBoxLayout(box);

    m_completeCheck = new QCheckBox(tr("Komplett (alle Aktien)"));
    m_completeCheck->setObjectName(QStringLiteral("portfolioChartCompleteCheck"));
    m_heldCheck = new QCheckBox(tr("Aktuell (im Depot)"));
    m_heldCheck->setObjectName(QStringLiteral("portfolioChartHeldCheck"));

    const AppSettings& settings = AppSettings::instance();
    if (m_mode == PortfolioChartMode::MarketValue) {
        m_heldCheck->setIcon(curveIcon(kPortfolioMarketValueColor, kPortfolioMarketValueColor));
        m_heldCheck->setToolTip(tr("Wert der am jeweiligen Tag gehaltenen Stücke — endet auf "
                                   "\"Aktueller Depotstand\" im Tab \"Kompletter Marktwert\"."));
        m_completeCheck->setIcon(curveIcon(kPortfolioMarketCompleteColor, kPortfolioMarketCompleteColor));
        m_completeCheck->setToolTip(tr("Wert der gehaltenen Stücke plus alle Verkäufe brutto bis "
                                       "zum jeweiligen Tag — endet auf \"Kpl. Marktwert\"."));
        m_heldCheck->setChecked(settings.marketChartShowHeld());
        m_completeCheck->setChecked(settings.marketChartShowComplete());
        layout->addWidget(m_heldCheck);
        layout->addWidget(m_completeCheck);
    } else {
        m_completeCheck->setIcon(curveIcon(kPortfolioGainColor, kPortfolioLossColor));
        m_completeCheck->setToolTip(tr("Entwicklung über alle jemals gehandelten Aktien — endet "
                                       "auf \"Komplette Entwicklung\" im Tab \"Kompletter Depotwert\"."));
        m_heldCheck->setIcon(curveIcon(kPortfolioHeldGainColor, kPortfolioHeldLossColor));
        m_heldCheck->setToolTip(tr("Entwicklung der am jeweiligen Tag gehaltenen Stücke, ohne "
                                   "Dividenden — endet auf \"Entwicklung (gesamt)\", Spalte "
                                   "\"Aktuelle Entwicklung\"."));
        m_completeCheck->setChecked(settings.depotChartShowComplete());
        m_heldCheck->setChecked(settings.depotChartShowHeld());
        layout->addWidget(m_completeCheck);
        layout->addWidget(m_heldCheck);
    }
    // Eine von Hand geleerte INI darf nicht beide ausblenden.
    if (!m_completeCheck->isChecked() && !m_heldCheck->isChecked()) {
        m_completeCheck->setChecked(true);
        m_heldCheck->setChecked(true);
    }
    m_loadedShowComplete = m_completeCheck->isChecked();
    m_loadedShowHeld     = m_heldCheck->isChecked();

    connect(m_completeCheck, &QCheckBox::toggled,
            this, [this](bool) { onCurveToggled(m_completeCheck, m_heldCheck); });
    connect(m_heldCheck, &QCheckBox::toggled,
            this, [this](bool) { onCurveToggled(m_heldCheck, m_completeCheck); });

    return box;
}

// ── IViewPortfolioChart: Getter ───────────────────────────────────────────────

QDate ViewPortfolioChart::startDate() const
{
    return m_startDateEdit->date();
}

IntervalUnit ViewPortfolioChart::intervalUnit() const
{
    return static_cast<IntervalUnit>(m_intervalCombo->currentData().toInt());
}

int ViewPortfolioChart::intervalCount() const
{
    return m_countSpin->value();
}

// ── IViewPortfolioChart: Setter ───────────────────────────────────────────────

void ViewPortfolioChart::setDefaultStartDate(const QDate& date)
{
    // Blocked so this presenter-driven initial set does not trigger a
    // redundant onControlsChanged() before loadAndDisplay() refreshes anyway.
    const QSignalBlocker blocker(m_startDateEdit);
    m_startDateEdit->setDate(date);
}

void ViewPortfolioChart::setMaxIntervalCount(int maxCount)
{
    // Blocked so the internal clamp cannot emit valueChanged() and recurse
    // back into onControlsChanged() — same reasoning as ViewChart.
    const QSignalBlocker blocker(m_countSpin);
    const int maximum = std::max(1, maxCount);
    m_countSpin->setMaximum(maximum);

    // Gewünschte Anzahl wiederherstellen, soweit die Historie reicht
    // (ergänzt 20.09.2026) — gleiche Begründung wie
    // ViewChart::setMaxIntervalCount(). PresenterPortfolioChart::refresh()
    // liest die Anzahl seitdem erst nach diesem Aufruf.
    m_countSpin->setValue(std::min(m_desiredCount, maximum));
}

void ViewPortfolioChart::setChartData(const PortfolioChartData& chartData)
{
    m_stack->setCurrentIndex(0);
    m_points = chartData.points;
    m_pointsX.clear();
    m_pointsX.reserve(m_points.size());
    for (const PortfolioChartPoint& point : m_points)
        m_pointsX.append(toMSecs(point.date));

    redraw();
}

// ── redraw ────────────────────────────────────────────────────────────────────

void ViewPortfolioChart::redraw()
{
    m_chart->removeAllSeries();
    rebuildAxes(m_points);

    if (m_points.isEmpty())
        return;

    const auto pointsOf = [this](double PortfolioChartPoint::* member) {
        QList<QPointF> result;
        result.reserve(m_points.size());
        for (const PortfolioChartPoint& point : m_points)
            result.append(QPointF(static_cast<double>(toMSecs(point.date)), point.*member));
        return result;
    };

    // Eine Linie in einer Farbe; Pen/Farbe erst NACH addSeries() setzen, da
    // QChart beim Einfügen sein Theme anwendet (Qt-Charts-Eigenheit, siehe
    // ViewChart).
    const auto addLine = [this](const QList<QPointF>& points, const QColor& color) {
        auto* line = new QLineSeries();
        line->append(points);
        m_chart->addSeries(line);
        line->setColor(color);
        line->attachAxis(m_xAxis);
        line->attachAxis(m_yAxis);
        connect(line, &QLineSeries::hovered, this,
                [this](const QPointF& point, bool state) { onSeriesHovered(point, state); });
    };

    // Linie nach Vorzeichen eingefärbt, an jedem Nulldurchgang geteilt.
    const auto addSignedLine = [&addLine](const QList<QPointF>& points,
                                          const QColor& gain, const QColor& loss) {
        for (const CurveSegment& segment : buildSegments(points))
            addLine(segment.points, segment.positive ? gain : loss);
    };

    // Marktwert-Chart (ergänzt 17.10.2026): einfarbige Linien, keine
    // Null-Linie — ein Marktwert ist nie negativ. Seit 01.11.2026 zwei Linien.
    if (m_mode == PortfolioChartMode::MarketValue) {
        if (completeCurveVisible())
            addLine(pointsOf(&PortfolioChartPoint::marketValueComplete), kPortfolioMarketCompleteColor);
        if (heldCurveVisible())
            addLine(pointsOf(&PortfolioChartPoint::marketValue), kPortfolioMarketValueColor);
        return;
    }

    // Zero line first so the curves are drawn on top of it.
    auto* zeroLine = new QLineSeries();
    zeroLine->append(static_cast<double>(toMSecs(m_points.constFirst().date)), 0.0);
    zeroLine->append(static_cast<double>(toMSecs(m_points.constLast().date)),  0.0);
    m_chart->addSeries(zeroLine);
    QPen zeroPen(kPortfolioZeroLineColor);
    zeroPen.setStyle(Qt::DashLine);
    zeroPen.setWidth(1);
    zeroLine->setPen(zeroPen);
    zeroLine->attachAxis(m_xAxis);
    zeroLine->attachAxis(m_yAxis);

    // "Aktuell (im Depot)" zuerst, damit die bisherige Linie "Komplett" bei
    // Überlagerung oben liegt.
    if (heldCurveVisible())
        addSignedLine(pointsOf(&PortfolioChartPoint::developmentHeld),
                      kPortfolioHeldGainColor, kPortfolioHeldLossColor);
    if (completeCurveVisible())
        addSignedLine(pointsOf(&PortfolioChartPoint::development),
                      kPortfolioGainColor, kPortfolioLossColor);
}

void ViewPortfolioChart::showEmptyChart(const QString& message)
{
    m_emptyLabel->setText(message);
    m_stack->setCurrentIndex(1);
}

void ViewPortfolioChart::showCalculating(const QString& message)
{
    m_calculatingLabel->setText(message);
    m_stack->setCurrentIndex(2);
}

void ViewPortfolioChart::setRangeInfo(const QString& infoText)
{
    m_lastRangeInfo = infoText;
    emit titleInfoChanged(infoText);
}

void ViewPortfolioChart::showError(const QString& message)
{
    OwnMessageBox::critical(this, tr("Fehler"), message);
}

// ── rebuildAxes ───────────────────────────────────────────────────────────────

void ViewPortfolioChart::rebuildAxes(const QList<PortfolioChartPoint>& points)
{
    if (m_xAxis) { m_chart->removeAxis(m_xAxis); delete m_xAxis; m_xAxis = nullptr; }
    if (m_yAxis) { m_chart->removeAxis(m_yAxis); delete m_yAxis; m_yAxis = nullptr; }

    if (points.isEmpty())
        return;

    QDateTime minDt(points.constFirst().date, QTime(0, 0));
    QDateTime maxDt(points.constLast().date,  QTime(0, 0));
    if (minDt == maxDt)
        maxDt = maxDt.addDays(1); // avoid a zero-width X axis for a single point

    m_xAxis = new QDateTimeAxis();
    m_xAxis->setFormat(QStringLiteral("dd.MM.yy"));
    m_xAxis->setRange(minDt, maxDt);
    m_chart->addAxis(m_xAxis, Qt::AlignBottom);

    const bool marketMode = (m_mode == PortfolioChartMode::MarketValue);
    // Wertebereich über alle sichtbaren Linien (seit 01.11.2026 zwei je Chart).
    const bool showComplete = completeCurveVisible();
    const bool showHeld     = heldCurveVisible();

    double lo = std::numeric_limits<double>::max();
    double hi = std::numeric_limits<double>::lowest();
    const auto include = [&lo, &hi](double value) {
        lo = std::min(lo, value);
        hi = std::max(hi, value);
    };
    for (const PortfolioChartPoint& point : points) {
        if (marketMode) {
            if (showHeld)     include(point.marketValue);
            if (showComplete) include(point.marketValueComplete);
        } else {
            if (showComplete) include(point.development);
            if (showHeld)     include(point.developmentHeld);
        }
    }

    // Depotwert-Chart: zero must always be visible — it is the reference the
    // colouring is built around, and a curve that never crosses it would
    // otherwise hide it. Not for the Marktwert-Chart (17.10.2026): forcing
    // zero there would squash the curve into a flat line at the top.
    if (!marketMode) {
        lo = std::min(lo, 0.0);
        hi = std::max(hi, 0.0);
    }

    if (qFuzzyCompare(lo + 1.0, hi + 1.0)) {
        lo -= 1.0;
        hi += 1.0;
    } else {
        const double margin = (hi - lo) * 0.08;
        lo -= margin;
        hi += margin;
    }

    // A market value is never negative — do not let the margin push the
    // axis below zero.
    if (marketMode)
        lo = std::max(lo, 0.0);

    m_yAxis = new QValueAxis();
    m_yAxis->setTitleText(marketMode ? tr("Marktwert (€)") : tr("Entwicklung (€)"));
    m_yAxis->setRange(lo, hi);
    m_chart->addAxis(m_yAxis, Qt::AlignLeft);
}

// ── onSeriesHovered ───────────────────────────────────────────────────────────

void ViewPortfolioChart::onSeriesHovered(const QPointF& point, bool state)
{
    if (!state) {
        QToolTip::hideText();
        return;
    }

    if (m_points.isEmpty())
        return;

    // QLineSeries::hovered() liefert die CURSORPOSITION in Achsenkoordinaten,
    // nicht den Datenpunkt (Bugfix 06.08.2026). point.y() direkt anzuzeigen
    // ergab bei gleichem Datum je nach Zeigerhöhe unterschiedliche Werte, und
    // die Suche nach dem Prozentwert über die exakte X-Koordinate schlug
    // praktisch immer fehl. Deshalb wird auf den nächstgelegenen echten
    // Datenpunkt eingerastet — seit 19.09.2026 über die mit ViewChart
    // geteilte Suche (ChartPointSearch), vorher eine eigene Kopie hier.
    const qsizetype index = ChartPointSearch::nearestIndex(m_pointsX, point.x());
    if (index < 0)
        return;

    const PortfolioChartPoint& nearest = m_points.at(index);

    // Punkt mit aktuellem Kurs statt Schlusskurs kenntlich machen (ergänzt
    // 30.10.2026, Nessies Entscheidung): meist der rechte Rand, wenn der
    // Kursabruf neuer ist als der letzte Tageswert.
    QString dateText = nearest.date.toString(QStringLiteral("dd.MM.yyyy"));
    if (nearest.currentPrice)
        dateText += tr(" (aktueller Kurs)");

    // Je sichtbarer Linie eine Zeile, in der Reihenfolge der Häkchen
    // (ergänzt 01.11.2026). Marktwert-Chart ohne Prozent (17.10.2026).
    const auto euro = [](double value) {
        return QLocale().toString(value, 'f', 2) + QStringLiteral(" €");
    };
    const auto pct = [](double value) {
        return QLocale().toString(value, 'f', 2) + QStringLiteral(" %");
    };

    QStringList lines{ dateText };
    if (m_mode == PortfolioChartMode::MarketValue) {
        if (heldCurveVisible())
            lines << tr("Aktuell: %1").arg(euro(nearest.marketValue));
        if (completeCurveVisible())
            lines << tr("Komplett: %1").arg(euro(nearest.marketValueComplete));
    } else {
        if (completeCurveVisible())
            lines << tr("Komplett: %1 (%2)").arg(euro(nearest.development),
                                                 pct(nearest.developmentPct));
        if (heldCurveVisible())
            lines << tr("Aktuell: %1 (%2)").arg(euro(nearest.developmentHeld),
                                                pct(nearest.developmentHeldPct));
    }

    QToolTip::showText(QCursor::pos(), lines.join(QLatin1Char('\n')), m_chartView);
}

// ── onExportDiagnostics ───────────────────────────────────────────────────────

void ViewPortfolioChart::onExportDiagnostics()
{
    const QString suggested =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        + (m_mode == PortfolioChartMode::MarketValue
               ? QStringLiteral("/spm-marktwert-chart-diagnose-")
               : QStringLiteral("/spm-depotwert-chart-diagnose-"))
        + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"))
        + QStringLiteral(".csv");

    const QString path = QFileDialog::getSaveFileName(
        this, tr("Diagnose speichern"), suggested, tr("CSV-Dateien (*.csv)"));
    if (path.isEmpty())
        return; // vom Benutzer abgebrochen

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        showError(tr("Die Datei konnte nicht geschrieben werden:\n%1").arg(file.errorString()));
        return;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    // BOM, damit Excel die Umlaute in den Spaltenüberschriften korrekt liest.
    stream.setGenerateByteOrderMark(true);
    stream << m_presenter.buildDiagnosticsCsv();
    file.close();
}

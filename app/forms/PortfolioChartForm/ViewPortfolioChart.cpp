// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ViewPortfolioChart.h"

#include "../OwnMessageBoxForm/OwnMessageBox.h"

#include <QHBoxLayout>
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
QList<CurveSegment> buildSegments(const QList<PortfolioChartPoint>& points)
{
    QList<CurveSegment> segments;
    if (points.isEmpty())
        return segments;

    const auto isPositive = [](double value) { return value >= 0.0; };

    CurveSegment current;
    current.positive = isPositive(points.constFirst().development);
    current.points.append(QPointF(static_cast<double>(toMSecs(points.constFirst().date)),
                                  points.constFirst().development));

    for (int i = 1; i < points.size(); ++i) {
        const double x = static_cast<double>(toMSecs(points.at(i).date));
        const double y = points.at(i).development;

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

} // namespace

// ── Constructor ───────────────────────────────────────────────────────────────

ViewPortfolioChart::ViewPortfolioChart(QWidget* parent)
    : QWidget(parent)
    , m_presenter(this, &m_model)
{
    setObjectName(QStringLiteral("ViewPortfolioChart"));
    setupUi();
    m_presenter.loadAndDisplay();
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
    m_chart->legend()->hide(); // single series — a legend would carry no information
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

    m_warningLabel = new QLabel();
    m_warningLabel->setObjectName(QStringLiteral("portfolioChartWarningLabel"));
    m_warningLabel->setWordWrap(true);
    m_warningLabel->hide(); // only shown when setWarning() gets a non-empty text

    auto* leftLayout = new QVBoxLayout();
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(m_stack, 1);
    leftLayout->addWidget(m_warningLabel);
    mainLayout->addLayout(leftLayout, 1);

    // ── Right panel: Zeitraum only ────────────────────────────────────────
    auto* rightContent = new QWidget();
    rightContent->setObjectName(QStringLiteral("portfolioChartRightPanel"));
    auto* rightLayout = new QVBoxLayout(rightContent);
    rightLayout->setContentsMargins(0, 0, 4, 0);
    rightLayout->addWidget(setupZeitraumBox());
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
    m_intervalCombo->setCurrentIndex(3); // "Jahr" — Nessies Vorgabe 05.08.2026
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
    m_countSpin->setValue(1);
    connect(m_countSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { m_presenter.onControlsChanged(); });
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
    m_countSpin->setMaximum(std::max(1, maxCount));
}

void ViewPortfolioChart::setChartData(const PortfolioChartData& data)
{
    m_stack->setCurrentIndex(0);
    m_chart->removeAllSeries();
    m_points.clear();
    m_pointsX.clear();

    rebuildAxes(data.points);

    if (data.points.isEmpty())
        return;

    m_points = data.points;
    m_pointsX.reserve(m_points.size());
    for (const PortfolioChartPoint& point : m_points)
        m_pointsX.append(toMSecs(point.date));

    // Zero line first so the curve is drawn on top of it.
    auto* zeroLine = new QLineSeries();
    zeroLine->append(static_cast<double>(toMSecs(data.points.constFirst().date)), 0.0);
    zeroLine->append(static_cast<double>(toMSecs(data.points.constLast().date)),  0.0);
    m_chart->addSeries(zeroLine);
    QPen zeroPen(kPortfolioZeroLineColor);
    zeroPen.setStyle(Qt::DashLine);
    zeroPen.setWidth(1);
    // Pen must be set AFTER addSeries(): QChart applies its theme on insert
    // and would otherwise overwrite it (Qt Charts quirk, see ViewChart).
    zeroLine->setPen(zeroPen);
    zeroLine->attachAxis(m_xAxis);
    zeroLine->attachAxis(m_yAxis);

    for (const CurveSegment& segment : buildSegments(data.points)) {
        auto* line = new QLineSeries();
        for (const QPointF& point : segment.points)
            line->append(point);

        m_chart->addSeries(line);
        line->setColor(segment.positive ? kPortfolioGainColor : kPortfolioLossColor);
        line->attachAxis(m_xAxis);
        line->attachAxis(m_yAxis);

        connect(line, &QLineSeries::hovered, this,
                [this](const QPointF& point, bool state) { onSeriesHovered(point, state); });
    }
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

void ViewPortfolioChart::setWarning(const QString& message)
{
    m_warningLabel->setText(message);
    m_warningLabel->setVisible(!message.isEmpty());
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

    double lo = std::numeric_limits<double>::max();
    double hi = std::numeric_limits<double>::lowest();
    for (const PortfolioChartPoint& point : points) {
        lo = std::min(lo, point.development);
        hi = std::max(hi, point.development);
    }

    // Zero must always be visible — it is the reference the colouring is
    // built around, and a curve that never crosses it would otherwise hide it.
    lo = std::min(lo, 0.0);
    hi = std::max(hi, 0.0);

    if (qFuzzyCompare(lo + 1.0, hi + 1.0)) {
        lo -= 1.0;
        hi += 1.0;
    } else {
        const double margin = (hi - lo) * 0.08;
        lo -= margin;
        hi += margin;
    }

    m_yAxis = new QValueAxis();
    m_yAxis->setTitleText(tr("Entwicklung (€)"));
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
    // Datenpunkt eingerastet.
    const qint64 x = static_cast<qint64>(point.x());

    auto upper = std::lower_bound(m_pointsX.cbegin(), m_pointsX.cend(), x);
    int index = static_cast<int>(std::distance(m_pointsX.cbegin(), upper));

    if (index >= m_pointsX.size()) {
        index = m_pointsX.size() - 1;
    } else if (index > 0) {
        // Der linke Nachbar kann näher liegen als der gefundene rechte.
        const qint64 distanceRight = m_pointsX.at(index)     - x;
        const qint64 distanceLeft  = x - m_pointsX.at(index - 1);
        if (distanceLeft < distanceRight)
            --index;
    }

    const PortfolioChartPoint& nearest = m_points.at(index);

    const QString text = QStringLiteral("%1\n%2\n%3")
        .arg(nearest.date.toString(QStringLiteral("dd.MM.yyyy")),
             QLocale().toString(nearest.development, 'f', 2) + QStringLiteral(" €"),
             QLocale().toString(nearest.developmentPct, 'f', 2) + QStringLiteral(" %"));

    QToolTip::showText(QCursor::pos(), text, m_chartView);
}

// ── onExportDiagnostics ───────────────────────────────────────────────────────

void ViewPortfolioChart::onExportDiagnostics()
{
    const QString suggested =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        + QStringLiteral("/spm-depotwert-chart-diagnose-")
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

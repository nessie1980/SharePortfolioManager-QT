// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ViewChart.h"

#include "../OwnMessageBoxForm/OwnMessageBox.h"

#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineSeries>
#include <QPainter>
#include <QPixmap>
#include <QSignalBlocker>
#include <QToolTip>
#include <QCursor>
#include <QLocale>
#include <QScrollArea>
#include <QFrame>
#include <QPen>
#include <QWheelEvent>

#include <algorithm>
#include <limits>
#include <cmath>

namespace {

/** Mirrors the checkbox labels in setupSelektionBox() — small, deliberate
 *  duplication (UI-facing display text, not business logic) so the tooltip
 *  doesn't need a back-reference into PresenterChart's internals. */
QString seriesTooltipLabel(SeriesKind kind)
{
    switch (kind) {
    case SeriesKind::ClosingPrice: return QObject::tr("Schluss-Kurs");
    case SeriesKind::OpeningPrice: return QObject::tr("Eröffnungs-Kurs");
    case SeriesKind::High:         return QObject::tr("Höchstwert");
    case SeriesKind::Low:          return QObject::tr("Tiefstwert");
    case SeriesKind::HeldVolume:   return QObject::tr("Anteile");
    case SeriesKind::TradedVolume: return QObject::tr("Gehandelte Anteile");
    }
    return QString();
}

bool isVolumeSeriesKind(SeriesKind kind)
{
    return kind == SeriesKind::HeldVolume || kind == SeriesKind::TradedVolume;
}

} // namespace

// ── Constructor ───────────────────────────────────────────────────────────────

ViewChart::ViewChart(const QString& shareGuid, bool compact, QWidget* parent)
    : QWidget(parent)
    , m_presenter(this, &m_model, shareGuid)
    , m_compact(compact)
{
    setObjectName(QStringLiteral("ViewChart"));
    setupUi();
    m_presenter.loadAndDisplay();
}

// ── eventFilter ──────────────────────────────────────────────────────────────

bool ViewChart::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Wheel &&
        (watched == m_countSpin || watched == m_chartView->viewport())) {
        applyWheelStep(static_cast<QWheelEvent*>(event));
        return true; // konsumiert — verhindert bei countSpin die Fokus-
                      // Abhängigkeit des Default-Verhaltens, bei chartView
                      // ein Durchreichen an QGraphicsView/dahinterliegende
                      // Widgets.
    }
    return QWidget::eventFilter(watched, event);
}

// ── applyWheelStep ───────────────────────────────────────────────────────────

void ViewChart::applyWheelStep(QWheelEvent* event)
{
    // Gleiche Umrechnung von angleDelta() in "Rasten" wie Qt intern in
    // QAbstractSpinBox::wheelEvent() verwendet — ein Standard-Mausrad-Klick
    // entspricht 15° bzw. 120 angleDelta-Einheiten.
    const int numDegrees = event->angleDelta().y() / 8;
    const int numSteps = numDegrees / 15;
    if (numSteps != 0)
        m_countSpin->stepBy(numSteps); // triggert valueChanged() → m_presenter.onControlsChanged()
}

// ── setupUi ───────────────────────────────────────────────────────────────────

void ViewChart::setupUi()
{
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // ── Chart area ────────────────────────────────────────────────────────
    m_chart = new QChart();
    // QtCharts' own legend is hidden — the "Legende" box on the right
    // (setupLegendeBox()) replaces it with richer, presenter-formatted rows
    // (Min/Max, Letzter Kauf/Verkauf), same as the C# reference layout.
    m_chart->legend()->hide();
    m_chart->setMargins(QMargins(4, 4, 4, 4));

    m_chartView = new QChartView(m_chart);
    m_chartView->setObjectName(QStringLiteral("chartView"));
    m_chartView->setRenderHint(QPainter::Antialiasing);

    m_emptyLabel = new QLabel();
    m_emptyLabel->setObjectName(QStringLiteral("chartEmptyLabel"));
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);
    QFont emptyFont = m_emptyLabel->font();
    emptyFont.setItalic(true);
    m_emptyLabel->setFont(emptyFont);

    m_stack = new QStackedWidget();
    m_stack->setObjectName(QStringLiteral("chartStack"));
    m_stack->addWidget(m_chartView);  // index 0 — normal state
    m_stack->addWidget(m_emptyLabel); // index 1 — empty/error state
    mainLayout->addWidget(m_stack, 1);

    // ── Right panel: Legende + Selektion ─────────────────────────────────────
    // In eine QScrollArea gepackt statt direkt ins mainLayout: bei vielen
    // aktiven Selektions-Checkboxen (bis zu 6 Serien + Kauf-/Verkauf-Referenz-
    // zeilen in der Legende) reichte die vertikale Höhe des Dialogs nicht
    // immer aus — Qt hat die Labels dann unter ihre benötigte Höhe gequetscht
    // (mal als überlappender Text, mal als abgeschnittene Zeile, siehe
    // ARCHITECTURE.md "ChartForm-Details", Bugfix-Verlauf 12.07.2026). Mit
    // QScrollArea + setWidgetResizable(true) bekommt der Inhalt immer seine
    // volle benötigte Höhe und scrollt bei Bedarf, statt gestaucht zu werden.
    auto* rightContent = new QWidget();
    rightContent->setObjectName(QStringLiteral("chartRightPanel"));
    auto* rightLayout = new QVBoxLayout(rightContent);
    rightLayout->setContentsMargins(0, 0, 4, 0);
    rightLayout->addWidget(setupLegendeBox());

    // setupSelektionBox() wird immer aufgerufen (legt m_seriesCheckBoxes,
    // m_startDateEdit, m_intervalCombo, m_countSpin an — von den
    // IViewChart-Gettern und der Mausrad-Steuerung auf m_countSpin
    // benötigt, siehe eventFilter()), im Compact-Modus aber nicht ins
    // sichtbare Layout gehängt: dann bleibt nur die Legende-Box sichtbar,
    // wie im C#-Referenz-Popup (ergänzt 31.07.2026, siehe Klassendoku
    // "Compact-Modus" und ARCHITECTURE.md, "ChartPopup").
    QGroupBox* selektionBox = setupSelektionBox();
    if (!m_compact) {
        rightLayout->addWidget(selektionBox);
    } else {
        selektionBox->setParent(this);
        selektionBox->hide();
    }
    rightLayout->addStretch(1);

    auto* rightScroll = new QScrollArea();
    rightScroll->setObjectName(QStringLiteral("chartRightScroll"));
    rightScroll->setWidget(rightContent);
    rightScroll->setWidgetResizable(true);
    rightScroll->setFrameShape(QFrame::NoFrame);
    rightScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // Feste Breite (siehe Kommentar oben zu setFixedWidth() vs.
    // setMaximumWidth()) — nach 520px (zu breit, Nessies Rückmeldung
    // 12.07.2026: "viel zu breit") wieder auf ein moderateres Maß reduziert,
    // das die längste Legende-Zeile noch einzeilig zeigt, ohne unnötig viel
    // Leerraum rechts vom Text zu lassen. Im Compact-Modus genügt eine
    // schmalere Breite, da nur noch die Legende-Box Platz braucht (ergänzt
    // 31.07.2026).
    rightScroll->setFixedWidth(m_compact ? 260 : 380); // 360 Inhaltsbreite + Platz für die Scrollbar
    mainLayout->addWidget(rightScroll);

    // ── Mausrad-Steuerung der "Anzahl" ───────────────────────────────────────
    // Auf m_countSpin: QAbstractSpinBox::wheelEvent() ignoriert Wheel-Events
    // ohne Fokus — der Event-Filter fängt sie stattdessen direkt ab, damit
    // Scrollen ohne vorherigen Klick funktioniert.
    // Auf m_chartView->viewport() (nicht m_chartView selbst!): QGraphicsView
    // leitet Wheel-Events intern an seinen Viewport weiter, ein Filter auf
    // dem QChartView-Objekt würde sie nie sehen. Der Viewport deckt exakt die
    // Zeichenfläche ab (nicht Legende/Selektion daneben), erfüllt also von
    // selbst Nessies Vorgabe "nur über der Zeichenfläche" (12.07.2026).
    m_countSpin->installEventFilter(this);
    m_chartView->viewport()->installEventFilter(this);
}

// ── setupLegendeBox ───────────────────────────────────────────────────────────

QGroupBox* ViewChart::setupLegendeBox()
{
    auto* box = new QGroupBox(tr("Legende"));
    box->setObjectName(QStringLiteral("legendeBox"));
    m_legendLayout = new QVBoxLayout(box);
    m_legendLayout->addStretch(1);
    return box;
}

// ── setupSelektionBox ─────────────────────────────────────────────────────────

QGroupBox* ViewChart::setupSelektionBox()
{
    auto* box = new QGroupBox(tr("Selektion:"));
    box->setObjectName(QStringLiteral("selektionBox"));
    auto* layout = new QVBoxLayout(box);

    // ── Series checkboxes — order matches the C# reference exactly ──────────
    // objectName je Checkbox (ergänzt 12.07.2026, für den Exklusivitäts-Test
    // in tst_mainwindow.cpp per findChild() — siehe test_chartCheckboxes_...).
    auto addCheckBox = [this, layout](SeriesKind kind, const QString& label, bool checked,
                                       const QString& objectName) {
        auto* cb = new QCheckBox(label);
        cb->setObjectName(objectName);
        cb->setChecked(checked);
        connect(cb, &QCheckBox::toggled, this, [this](bool) { m_presenter.onControlsChanged(); });
        layout->addWidget(cb);
        m_seriesCheckBoxes.insert(kind, cb);
    };

    addCheckBox(SeriesKind::ClosingPrice, tr("Schluss-Kurs"),     /*checked=*/true,
                QStringLiteral("seriesCheckBox_ClosingPrice"));
    addCheckBox(SeriesKind::OpeningPrice, tr("Eröffnungs-Kurs"),  /*checked=*/false,
                QStringLiteral("seriesCheckBox_OpeningPrice"));
    addCheckBox(SeriesKind::High,         tr("Höchstwert"),       /*checked=*/false,
                QStringLiteral("seriesCheckBox_High"));
    addCheckBox(SeriesKind::Low,          tr("Tiefstwert"),       /*checked=*/false,
                QStringLiteral("seriesCheckBox_Low"));
    addCheckBox(SeriesKind::HeldVolume,   tr("Anteile"),          /*checked=*/false,
                QStringLiteral("seriesCheckBox_HeldVolume"));
    addCheckBox(SeriesKind::TradedVolume, tr("Gehandelte Anteile"), /*checked=*/false,
                QStringLiteral("seriesCheckBox_TradedVolume"));

    // Gegenseitiger Ausschluss "Anteile"/"Gehandelte Anteile" (ergänzt
    // 12.07.2026, auf Nessies Vorgabe nach visueller Prüfung der vorherigen
    // Drei-Achsen-Optik — siehe ARCHITECTURE.md, "ChartForm-Details").
    // Bewusst reine View-Ebene: PresenterChart bekommt davon nichts mit, er
    // fragt über isSeriesSelected() ohnehin nur die jeweils tatsächlich
    // angehakte Checkbox ab. Ausgegraut statt versteckt, damit die
    // Selektionsbox beim Umschalten nicht in der Höhe springt.
    auto* heldCb   = m_seriesCheckBoxes.value(SeriesKind::HeldVolume);
    auto* tradedCb = m_seriesCheckBoxes.value(SeriesKind::TradedVolume);
    const QString exclusivityTooltip =
        tr("Anteile und Gehandelte Anteile können nicht gleichzeitig angezeigt werden.");
    connect(heldCb, &QCheckBox::toggled, this, [tradedCb, exclusivityTooltip](bool checked) {
        tradedCb->setDisabled(checked);
        tradedCb->setToolTip(checked ? exclusivityTooltip : QString());
    });
    connect(tradedCb, &QCheckBox::toggled, this, [heldCb, exclusivityTooltip](bool checked) {
        heldCb->setDisabled(checked);
        heldCb->setToolTip(checked ? exclusivityTooltip : QString());
    });

    layout->addSpacing(8);

    // ── Start-Datum / Interval / Anzahl ──────────────────────────────────────
    auto* form = new QFormLayout();

    m_startDateEdit = new QDateEdit(QDate::currentDate());
    m_startDateEdit->setObjectName(QStringLiteral("startDateEdit"));
    m_startDateEdit->setCalendarPopup(true);
    m_startDateEdit->setDisplayFormat(QStringLiteral("dd.MM.yyyy"));
    connect(m_startDateEdit, &QDateEdit::dateChanged,
            this, [this](const QDate&) { m_presenter.onControlsChanged(); });
    form->addRow(tr("Start-Datum:"), m_startDateEdit);

    m_intervalCombo = new QComboBox();
    m_intervalCombo->setObjectName(QStringLiteral("intervalCombo"));
    m_intervalCombo->addItem(tr("Tag"),   static_cast<int>(IntervalUnit::Day));
    m_intervalCombo->addItem(tr("Woche"), static_cast<int>(IntervalUnit::Week));
    m_intervalCombo->addItem(tr("Monat"), static_cast<int>(IntervalUnit::Month));
    m_intervalCombo->addItem(tr("Jahr"),  static_cast<int>(IntervalUnit::Year));
    m_intervalCombo->setCurrentIndex(2); // "Monat" — Default wie C#-Referenz-Screenshot
    // "activated" statt "currentIndexChanged" (siehe ARCHITECTURE.md,
    // "Qt6 variadic arg()"-Nachbarabschnitt zu QComboBox-Interaktionssignalen) —
    // nur echte Nutzerauswahl soll einen Refresh auslösen, kein programmatisches setCurrentIndex().
    connect(m_intervalCombo, QOverload<int>::of(&QComboBox::activated),
            this, [this](int) { m_presenter.onControlsChanged(); });
    form->addRow(tr("Interval:"), m_intervalCombo);

    m_countSpin = new QSpinBox();
    m_countSpin->setObjectName(QStringLiteral("countSpin"));
    // Nur ein kurzlebiger Platzhalter-Wert: PresenterChart::loadAndDisplay()
    // (läuft bereits im Konstruktor, siehe unten) ruft sofort refresh() auf,
    // das per setMaxIntervalCount() den tatsächlichen, dynamisch berechneten
    // Maximalwert setzt (siehe PresenterChart::computeMaxIntervalCount() —
    // seit 12.07.2026 keine feste Konstante mehr, sondern die tatsächliche
    // Tagesspanne zur ältesten Kurshistorie plus eine großzügige absolute
    // Notbremse). Der hier gesetzte Wert 999 ist daher reiner Ausgangspunkt,
    // kein mit PresenterChart geteiltes Limit mehr.
    m_countSpin->setRange(1, 999);
    m_countSpin->setValue(1);
    connect(m_countSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int) { m_presenter.onControlsChanged(); });
    form->addRow(tr("Anzahl:"), m_countSpin);

    layout->addLayout(form);
    return box;
}

// ── IViewChart: getters ────────────────────────────────────────────────────────

QDate ViewChart::startDate() const
{
    return m_startDateEdit->date();
}

IntervalUnit ViewChart::intervalUnit() const
{
    return static_cast<IntervalUnit>(m_intervalCombo->currentData().toInt());
}

int ViewChart::intervalCount() const
{
    return m_countSpin->value();
}

bool ViewChart::isSeriesSelected(SeriesKind kind) const
{
    auto it = m_seriesCheckBoxes.constFind(kind);
    return it != m_seriesCheckBoxes.constEnd() && (*it)->isChecked();
}

// ── IViewChart: setters ────────────────────────────────────────────────────────

void ViewChart::setDefaultStartDate(const QDate& date)
{
    // Blocked so this initial, presenter-driven set doesn't trigger a
    // redundant onControlsChanged() before PresenterChart::loadAndDisplay()
    // runs its own explicit refresh() right afterwards.
    const QSignalBlocker blocker(m_startDateEdit);
    m_startDateEdit->setDate(date);
}

void ViewChart::setMaxIntervalCount(int maxCount)
{
    // Geblockt: PresenterChart::refresh() ruft dies bei jedem Refresh auf —
    // der dabei ausgelöste interne Clamp (falls der aktuelle Wert die neue
    // Obergrenze überschreitet) darf kein valueChanged() feuern und damit
    // einen erneuten, rekursiven onControlsChanged()-Aufruf auslösen. Der
    // angezeigte Wert wird trotzdem korrekt geklemmt — QSpinBox hält seinen
    // internen Wert unabhängig vom Signal immer innerhalb [minimum, maximum].
    const QSignalBlocker blocker(m_countSpin);
    m_countSpin->setMaximum(std::max(1, maxCount));
}

void ViewChart::setChartData(const QList<ChartSeriesData>& series)
{
    m_stack->setCurrentIndex(0);
    m_chart->removeAllSeries();
    // removeAllSeries() löscht auch die vorherigen Kauf-/Verkauf-Markerlinien
    // (sie hängen als ganz normale QLineSeries am selben QChart) — die Liste
    // muss deshalb hier ohne erneutes delete geleert werden, sonst zeigt
    // setReferenceLines() beim nächsten Aufruf auf bereits freigegebenen
    // Speicher.
    m_referenceLineSeries.clear();
    rebuildAxes(series);

    for (const auto& s : series) {
        auto* line = new QLineSeries();
        for (int i = 0; i < s.dates.size(); ++i) {
            const QDateTime dt(s.dates.at(i), QTime(0, 0));
            line->append(static_cast<double>(dt.toMSecsSinceEpoch()), s.values.at(i));
        }
        m_chart->addSeries(line);
        // Muss NACH addSeries() gesetzt werden: QChart wendet sein Theme beim
        // Hinzufügen einer Serie an und überschreibt dabei eine vorher per
        // setColor() gesetzte Farbe wieder (Qt-Charts-Eigenheit, gefunden
        // 12.07.2026 — Schluss-Kurs erschien blau statt Schwarz trotz
        // korrekter Legende, die unabhängig vom Chart-Theme läuft).
        line->setColor(s.color);
        line->attachAxis(m_xAxis);

        QAbstractAxis* yAxis = nullptr;
        switch (s.axis) {
        case ChartAxis::Price:  yAxis = m_yAxisPrice;  break;
        case ChartAxis::Volume: yAxis = m_yAxisVolume; break;
        }
        if (yAxis)
            line->attachAxis(yAxis);

        // Hover-Tooltip (Datum + Wert) — ported from the C# reference.
        const SeriesKind kind = s.kind;
        connect(line, &QLineSeries::hovered, this,
                [this, kind](const QPointF& point, bool state) {
                    onSeriesHovered(kind, point, state);
                });
    }
}

void ViewChart::showEmptyChart(const QString& message)
{
    m_emptyLabel->setText(message);
    m_stack->setCurrentIndex(1);
}

void ViewChart::setLegendEntries(const LegendEntries& entries)
{
    clearLegendLayout();

    for (const auto& entry : entries) {
        auto* row = new QWidget();
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 4, 0, 4);
        rowLayout->setSpacing(6);

        auto* swatch = new QLabel();
        swatch->setFixedSize(14, 14);
        QPixmap pix(14, 14);
        pix.fill(entry.color);
        swatch->setPixmap(pix);

        // Ein einzelnes Rich-Text-Label statt drei verschachtelter QLabels in
        // einem eigenen QVBoxLayout: die verschachtelte Variante führte bei
        // den zwei-zeiligen "Letzter Kauf"/"Letzter Verkauf"-Einträgen zu
        // überlappendem Text (Nessies Rückmeldung, 12.07.2026 — auch nach dem
        // deleteLater()-Fix noch reproduzierbar). Ein Label mit <br>-Umbrüchen
        // ist robuster, da es nur eine einzige Widget-Geometrie zu berechnen
        // gibt, und erlaubt zusätzlich Wortumbruch für lange Zeilen.
        QString html = QStringLiteral("<b>%1</b>").arg(entry.title.toHtmlEscaped());
        if (!entry.line1.isEmpty())
            html += QStringLiteral("<br>%1").arg(entry.line1.toHtmlEscaped());
        if (!entry.line2.isEmpty())
            html += QStringLiteral("<br>%1").arg(entry.line2.toHtmlEscaped());

        auto* textLabel = new QLabel();
        textLabel->setTextFormat(Qt::RichText);
        textLabel->setText(html);
        textLabel->setWordWrap(true);

        rowLayout->addWidget(swatch, 0, Qt::AlignTop);
        rowLayout->addWidget(textLabel, 1);

        m_legendLayout->addWidget(row);
    }
    m_legendLayout->addStretch(1);
}

void ViewChart::setReferenceLines(const QList<ChartReferenceLine>& lines)
{
    // Defensiv statt blind auf m_referenceLineSeries zu vertrauen: prüft für
    // jede getrackte Serie, ob sie noch tatsächlich am Chart hängt, bevor sie
    // entfernt/gelöscht wird — setChartData() kann sie über removeAllSeries()
    // bereits gelöscht haben (siehe dortiger Kommentar), dann würde ein
    // erneutes delete hier abstürzen.
    const auto attached = m_chart->series();
    for (auto* s : m_referenceLineSeries) {
        if (attached.contains(s)) {
            m_chart->removeSeries(s);
            delete s;
        }
    }
    m_referenceLineSeries.clear();

    // Ohne Preis-Achse (z. B. nur "Anteile"/"Gehandelte Anteile" selektiert)
    // gibt es nichts, an dem sich eine vertikale Linie sinnvoll orientieren
    // könnte — dann einfach keine Markerlinien zeichnen.
    if (!m_yAxisPrice)
        return;

    const double yMin = m_yAxisPrice->min();
    const double yMax = m_yAxisPrice->max();

    for (const auto& line : lines) {
        auto* series = new QLineSeries();
        const QDateTime dt(line.date, QTime(0, 0));
        const double x = static_cast<double>(dt.toMSecsSinceEpoch());
        series->append(x, yMin);
        series->append(x, yMax);

        m_chart->addSeries(series);
        // Farbe erst NACH addSeries() setzen — siehe Bugfix-Kommentar weiter
        // oben (setChartData()): QChart überschreibt sonst mit der Theme-Farbe.
        QPen pen(line.color);
        pen.setStyle(Qt::DashLine);
        series->setPen(pen);
        series->setPointsVisible(false);

        series->attachAxis(m_xAxis);
        series->attachAxis(m_yAxisPrice);

        // Hover-Tooltip mit Kauf-/Verkauf-Details — ergänzt 12.07.2026.
        connect(series, &QLineSeries::hovered, this,
                [this, line](const QPointF& /*point*/, bool state) {
                    onReferenceLineHovered(line, state);
                });

        m_referenceLineSeries.append(series);
    }
}

void ViewChart::setRangeInfo(const QString& infoText)
{
    m_lastRangeInfo = infoText;
    emit titleInfoChanged(infoText);
}

void ViewChart::showError(const QString& message)
{
    OwnMessageBox::critical(this, tr("Fehler"), message);
}

// ── Helpers ────────────────────────────────────────────────────────────────────

void ViewChart::clearLegendLayout()
{
    // delete statt deleteLater(): deleteLater() löscht erst beim nächsten
    // Event-Loop-Durchlauf. Bis dahin bleibt das alte, gerade aus dem Layout
    // entfernte Widget an seiner eingefrorenen Position sichtbar, während
    // setLegendEntries() direkt im Anschluss neue Zeilen in denselben Layout
    // einfügt — das Ergebnis waren überlappende "Geister-Zeilen", je mehr
    // Selektions-Checkboxen aktiv waren, desto stärker (Nessies Rückmeldung,
    // 12.07.2026). Sofortiges delete ist hier sicher, da wir synchron
    // innerhalb desselben Aufrufs neu befüllen — gleiche Konvention wie
    // ViewShareDetails::populateBox().
    QLayoutItem* item;
    while ((item = m_legendLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
}

void ViewChart::rebuildAxes(const QList<ChartSeriesData>& series)
{
    if (m_xAxis)        { m_chart->removeAxis(m_xAxis);        delete m_xAxis;        m_xAxis = nullptr; }
    if (m_yAxisPrice)   { m_chart->removeAxis(m_yAxisPrice);   delete m_yAxisPrice;   m_yAxisPrice = nullptr; }
    if (m_yAxisVolume)  { m_chart->removeAxis(m_yAxisVolume);  delete m_yAxisVolume;  m_yAxisVolume = nullptr; }

    if (series.isEmpty())
        return;

    // All series share the same date list (see PresenterChart::refresh()).
    const QList<QDate>& dates = series.constFirst().dates;
    QDateTime minDt(dates.constFirst(), QTime(0, 0));
    QDateTime maxDt(dates.constLast(),  QTime(0, 0));
    if (minDt == maxDt)
        maxDt = maxDt.addDays(1); // avoid a degenerate (zero-width) X axis for a single data point

    m_xAxis = new QDateTimeAxis();
    m_xAxis->setFormat(QStringLiteral("dd.MM.yy"));
    m_xAxis->setRange(minDt, maxDt);
    m_chart->addAxis(m_xAxis, Qt::AlignBottom);

    // Zwei unabhängige Skalen — Preis links, Stück rechts. Anteile
    // (Depotbestand) und Gehandelte Anteile (Börsenvolumen) teilen sich seit
    // 12.07.2026 wieder dieselbe Volumen-Achse: die zugehörigen Checkboxen
    // sind in setupSelektionBox() gegenseitig exklusiv, es kann also nie
    // beide Serien gleichzeitig geben — die frühere dritte Achse (getrennt
    // nach Größenordnung) ist damit hinfällig (siehe ARCHITECTURE.md,
    // "ChartForm-Details").
    double priceMin  = std::numeric_limits<double>::max(), priceMax  = std::numeric_limits<double>::lowest();
    double volumeMin = std::numeric_limits<double>::max(), volumeMax = std::numeric_limits<double>::lowest();
    bool hasPrice = false, hasVolume = false;
    QString volumeAxisTitle = tr("Anteile"); // Fallback; wird unten je nach tatsächlicher Serie überschrieben

    for (const auto& s : series) {
        if (s.axis == ChartAxis::Volume)
            volumeAxisTitle = (s.kind == SeriesKind::TradedVolume) ? tr("Gehandelte Anteile") : tr("Anteile");

        for (double v : s.values) {
            switch (s.axis) {
            case ChartAxis::Price:
                priceMin = std::min(priceMin, v); priceMax = std::max(priceMax, v); hasPrice = true;
                break;
            case ChartAxis::Volume:
                volumeMin = std::min(volumeMin, v); volumeMax = std::max(volumeMax, v); hasVolume = true;
                break;
            }
        }
    }

    auto pad = [](double& lo, double& hi) {
        if (qFuzzyCompare(lo + 1.0, hi + 1.0)) { lo -= 1.0; hi += 1.0; return; }
        const double margin = (hi - lo) * 0.08;
        lo -= margin;
        hi += margin;
    };

    if (hasPrice) {
        pad(priceMin, priceMax);
        m_yAxisPrice = new QValueAxis();
        m_yAxisPrice->setTitleText(tr("Preis (€)"));
        m_yAxisPrice->setRange(priceMin, priceMax);
        m_chart->addAxis(m_yAxisPrice, Qt::AlignLeft);
    }
    if (hasVolume) {
        pad(volumeMin, volumeMax);
        m_yAxisVolume = new QValueAxis();
        m_yAxisVolume->setTitleText(volumeAxisTitle);
        m_yAxisVolume->setRange(volumeMin, volumeMax);
        m_chart->addAxis(m_yAxisVolume, Qt::AlignRight);
    }
}

// ── onSeriesHovered ────────────────────────────────────────────────────────────

void ViewChart::onSeriesHovered(SeriesKind kind, const QPointF& point, bool state)
{
    if (!state) {
        QToolTip::hideText();
        return;
    }

    const QDate date = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(point.x())).date();
    const QString dateStr = date.toString(QStringLiteral("dd.MM.yyyy"));

    const QString valueStr = isVolumeSeriesKind(kind)
        ? QLocale().toString(point.y(), 'f', 0)
        : QLocale().toString(point.y(), 'f', 2) + QStringLiteral("€");

    const QString text = QStringLiteral("%1\n%2: %3")
        .arg(seriesTooltipLabel(kind), dateStr, valueStr);

    QToolTip::showText(QCursor::pos(), text, m_chartView);
}

// ── onReferenceLineHovered ─────────────────────────────────────────────────────

void ViewChart::onReferenceLineHovered(const ChartReferenceLine& line, bool state)
{
    if (!state) {
        QToolTip::hideText();
        return;
    }

    const QString label = (line.kind == ChartReferenceLineKind::Buy) ? tr("Kauf") : tr("Verkauf");
    const QString dateStr  = line.date.toString(QStringLiteral("dd.MM.yyyy"));
    const QString priceStr = QLocale().toString(line.price, 'f', 2) + QStringLiteral("€");
    const QString volumeStr = QLocale().toString(line.volume, 'f', 0);

    const QString text = QStringLiteral("%1\n%2: %3\n%4 Stk.")
        .arg(label, dateStr, priceStr, volumeStr);

    QToolTip::showText(QCursor::pos(), text, m_chartView);
}

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "OverviewTabWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFrame>
#include <QFont>
#include <QTimer>

// ── Constructor ───────────────────────────────────────────────────────────────

OverviewTabWidget::OverviewTabWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* barRow = new QHBoxLayout();
    barRow->setContentsMargins(0, 0, 0, 0);
    barRow->setSpacing(0);

    // "Übersicht" bleibt immer sichtbar, egal wie viele Jahres-Tabs folgen —
    // eigene, nie scrollende QTabBar mit genau einem Eintrag (14.07.2026, auf
    // Nessies Vorgabe: siehe Klassenkommentar in OverviewTabWidget.h).
    m_pinnedBar = new QTabBar(this);
    m_pinnedBar->setObjectName(QStringLiteral("pinnedBar"));
    m_pinnedBar->setExpanding(false);

    auto* barSeparator = new QFrame(this);
    barSeparator->setObjectName(QStringLiteral("barSeparator"));
    barSeparator->setFrameShape(QFrame::VLine);
    barSeparator->setFrameShadow(QFrame::Sunken);

    m_yearsBar = new QTabBar(this);
    m_yearsBar->setObjectName(QStringLiteral("yearsBar"));
    m_yearsBar->setExpanding(false);
    m_yearsBar->setUsesScrollButtons(true);

    barRow->addWidget(m_pinnedBar);
    barRow->addWidget(barSeparator);
    barRow->addWidget(m_yearsBar, 1);

    m_stack = new QStackedWidget(this);
    m_stack->setObjectName(QStringLiteral("stack"));

    layout->addLayout(barRow);
    layout->addWidget(m_stack, 1);

    // tabBarClicked statt currentChanged: feuert bei JEDEM Klick, auch wenn
    // sich der interne Index der jeweiligen Bar nicht ändert (m_pinnedBar hat
    // z.B. immer nur genau einen Tab an Index 0). Bugfix 14.07.2026, siehe
    // Klassenkommentar in OverviewTabWidget.h.
    connect(m_pinnedBar, &QTabBar::tabBarClicked,
            this, &OverviewTabWidget::onPinnedBarClicked);
    connect(m_yearsBar, &QTabBar::tabBarClicked,
            this, &OverviewTabWidget::onYearsBarClicked);
}

// ── onPinnedBarClicked / onYearsBarClicked ────────────────────────────────────

void OverviewTabWidget::onPinnedBarClicked(int index)
{
    Q_UNUSED(index);
    if (m_suppressTabSignal)
        return;
    setCurrentIndex(0);
}

void OverviewTabWidget::onYearsBarClicked(int index)
{
    if (m_suppressTabSignal || index < 0)
        return;
    // +1, da Index 0 im Stack dem Übersicht-Tab gehört.
    setCurrentIndex(index + 1);
}

// ── clearAllTableSelections ───────────────────────────────────────────────────

void OverviewTabWidget::clearAllTableSelections()
{
    // Selektion in allen Tabellen leeren — Konsistenz mit dem bisherigen
    // Verhalten (identisch zu ViewBuyEdit/ViewSaleEdit/ViewDividendEdit).
    for (int i = 0; i < m_stack->count(); ++i) {
        auto* container = m_stack->widget(i);
        if (!container)
            continue;
        auto* tbl = qobject_cast<QTableWidget*>(
            container->property("dataTable").value<QObject*>());
        if (tbl)
            tbl->clearSelection();
    }
}

// ── count / widget / tabText / currentIndex / setCurrentIndex ────────────────

int OverviewTabWidget::count() const
{
    return m_stack->count();
}

QWidget* OverviewTabWidget::widget(int index) const
{
    return m_stack->widget(index);
}

QString OverviewTabWidget::tabText(int index) const
{
    if (index == 0)
        return m_pinnedBar->count() > 0 ? m_pinnedBar->tabText(0) : QString();
    return m_yearsBar->tabText(index - 1);
}

int OverviewTabWidget::currentIndex() const
{
    return m_stack->currentIndex();
}

void OverviewTabWidget::setCurrentIndex(int index)
{
    if (index < 0 || index >= m_stack->count())
        return;

    m_stack->setCurrentIndex(index);
    if (index == 0)
        m_pinnedBar->setCurrentIndex(0);
    else
        m_yearsBar->setCurrentIndex(index - 1);

    if (!m_suppressTabSignal) {
        clearAllTableSelections();
        emit currentTabChanged(index);
    }
}

// ── clear ─────────────────────────────────────────────────────────────────────

void OverviewTabWidget::clear()
{
    m_suppressTabSignal = true;

    while (m_yearsBar->count() > 0)
        m_yearsBar->removeTab(0);
    while (m_pinnedBar->count() > 0)
        m_pinnedBar->removeTab(0);
    while (m_stack->count() > 0) {
        auto* w = m_stack->widget(0);
        m_stack->removeWidget(w);
        if (w)
            w->deleteLater();
    }
    m_tabYears.clear();

    m_suppressTabSignal = false;
}

// ── buildFrozenTable ──────────────────────────────────────────────────────────
//
// Gegenüber der bisherigen Implementierung (siehe ViewBuyEdit.cpp /
// ViewSaleEdit.cpp / ViewDividendEdit.cpp) nur um die feste Fettschrift der
// Spaltenköpfe ergänzt (Bugfix 14.07.2026, s.u.) — Tab-Container-Aufbau sonst
// unverändert.

QWidget* OverviewTabWidget::buildFrozenTable(
    int colCount,
    const QStringList& headers,
    const QList<int>& colWidths,
    const std::function<void(QTableWidget*)>& populateData,
    const std::function<void(QTableWidget*)>& populateFooter,
    int docColumn)
{
    // ── Data table ──────────────────────────────────────────────────────────
    auto* data = new QTableWidget(0, colCount);
    data->setHorizontalHeaderLabels(headers);
    data->setEditTriggers(QAbstractItemView::NoEditTriggers);
    data->setSelectionBehavior(QAbstractItemView::SelectRows);
    data->setSelectionMode(QAbstractItemView::SingleSelection);
    data->setAlternatingRowColors(true);
    data->verticalHeader()->setVisible(false);
    data->setFrameShape(QFrame::NoFrame);
    data->horizontalHeader()->setStretchLastSection(false);

    // Spaltenköpfe immer fett, unabhängig von der Zeilen-Selektion — vorher
    // erschienen sie erst fett, sobald Qt's Style (highlightSections) die zur
    // Selektion gehörige Kopfspalte hervorhob (Bugfix 14.07.2026, Nessies
    // Feedback nach dem ersten Build).
    const QFont headerBoldFont = [] { QFont f; f.setBold(true); return f; }();
    data->horizontalHeader()->setFont(headerBoldFont);
    data->horizontalHeader()->setHighlightSections(false);

    populateData(data);

    // Apply fixed initial widths; -1 means stretch that column
    for (int c = 0; c < colCount && c < colWidths.size(); ++c) {
        if (colWidths.at(c) < 0)
            data->horizontalHeader()->setSectionResizeMode(c, QHeaderView::Stretch);
        else
            data->setColumnWidth(c, colWidths.at(c));
    }

    // ── Footer table (1 row, no header, no scrollbars) ────────────────────
    auto* footer = new QTableWidget(1, colCount);
    footer->setEditTriggers(QAbstractItemView::NoEditTriggers);
    footer->setSelectionMode(QAbstractItemView::NoSelection);
    footer->horizontalHeader()->setVisible(false);
    footer->verticalHeader()->setVisible(false);
    footer->setFrameShape(QFrame::NoFrame);
    footer->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    footer->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    footer->horizontalHeader()->setStretchLastSection(false);

    // Match the row height of the data table
    const int rowH = data->rowHeight(0) > 0 ? data->rowHeight(0) : 22;
    footer->setFixedHeight(rowH + 2);

    populateFooter(footer);

    const QFont boldFont = [] { QFont f; f.setBold(true); return f; }();
    for (int c = 0; c < colCount; ++c) {
        if (auto* it = footer->item(0, c)) {
            it->setFont(boldFont);
            it->setBackground(footer->palette().base());
        }
    }

    // Footer bekommt keinen Stretch-Modus — seine Spaltenbreiten werden
    // ausschließlich pixelgenau vom data-Widget übernommen (sectionResized).
    for (int c = 0; c < colCount && c < colWidths.size(); ++c) {
        if (colWidths.at(c) >= 0)
            footer->setColumnWidth(c, colWidths.at(c));
    }

    connect(data->horizontalHeader(), &QHeaderView::sectionResized,
            footer, [footer](int idx, int, int newSize) {
                footer->setColumnWidth(idx, newSize);
            });

    // ── Container ───────────────────────────────────────────────────────────
    auto* container = new QWidget;
    auto* cl = new QVBoxLayout(container);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(2);

    auto* separator = new QFrame;
    separator->setFrameShape(QFrame::HLine);

    cl->addWidget(data, 1);
    cl->addWidget(separator);
    cl->addWidget(footer);

    container->setProperty("dataTable", QVariant::fromValue<QObject*>(data));
    container->setProperty("footerTable", QVariant::fromValue<QObject*>(footer));

    // Initiale Spaltenbreiten-Übertragung für Stretch-Spalten, sobald der Tab
    // zum ersten Mal ein Layout durchlaufen hat (vor dem ersten Layout-Pass
    // liefert data->columnWidth() für Stretch-Spalten noch keinen sinnvollen Wert).
    QTimer::singleShot(0, footer, [data, footer, colCount]() {
        for (int c = 0; c < colCount; ++c)
            footer->setColumnWidth(c, data->columnWidth(c));
    });

    connect(data, &QTableWidget::cellClicked, this, [this, data](int row, int) {
        onJahresRowActivated(data->item(row, 0));
    });

    // Doppelklick auf die Dokument-Spalte → documentActivated(path). Der
    // Aufrufer entscheidet, was damit passiert (z.B. eingebettete
    // Vorschau aktualisieren) — kein Popup, keine PDF-Logik hier.
    if (docColumn >= 0) {
        connect(data, &QTableWidget::cellDoubleClicked, this,
                [this, data, docColumn](int row, int col) {
                    if (col != docColumn)
                        return;
                    const auto* item = data->item(row, docColumn);
                    if (!item)
                        return;
                    const QString path = item->data(Qt::UserRole).toString();
                    if (path.isEmpty())
                        return;
                    emit documentActivated(path);
                });
    }

    return container;
}

// ── onUebersichtRowActivated / onJahresRowActivated ───────────────────────────

void OverviewTabWidget::onUebersichtRowActivated(QTableWidgetItem* item)
{
    if (!item)
        return;
    const int year = item->data(Qt::UserRole).toInt();
    const int idx = m_tabYears.indexOf(year);
    if (idx >= 0)
        setCurrentIndex(idx);
}

void OverviewTabWidget::onJahresRowActivated(QTableWidgetItem* item)
{
    if (!item)
        return;
    emit rowActivated(item->data(Qt::UserRole));
}

// ── populateOverview ───────────────────────────────────────────────────────────

void OverviewTabWidget::populateOverview(
    const QList<int>& years,
    const QString& uebersichtTitle,
    const QStringList& uebersichtHeaders,
    const QList<int>& uebersichtColWidths,
    const std::function<void(QTableWidget* data)>& populateUebersichtData,
    const std::function<void(QTableWidget* footer)>& populateUebersichtFooter,
    const QStringList& jahresHeaders,
    const QList<int>& jahresColWidths,
    const std::function<QString(int year)>& jahresTitleForYear,
    const std::function<void(int year, QTableWidget* data)>& populateJahresData,
    const std::function<void(int year, QTableWidget* footer)>& populateJahresFooter,
    int jahresDocColumn)
{
    m_suppressTabSignal = true;

    while (m_yearsBar->count() > 0)
        m_yearsBar->removeTab(0);
    while (m_pinnedBar->count() > 0)
        m_pinnedBar->removeTab(0);
    while (m_stack->count() > 0) {
        auto* w = m_stack->widget(0);
        m_stack->removeWidget(w);
        if (w)
            w->deleteLater();
    }
    m_tabYears.clear();

    if (years.isEmpty()) {
        m_suppressTabSignal = false;
        return;
    }

    // ── Übersicht-Tab (Index 0, fixiert in m_pinnedBar) ─────────────────────
    {
        auto* container = buildFrozenTable(
            uebersichtHeaders.size(), uebersichtHeaders, uebersichtColWidths,
            populateUebersichtData, populateUebersichtFooter);

        // Klick auf eine Übersicht-Zeile springt zum passenden Jahres-Tab.
        auto* dataTable = qobject_cast<QTableWidget*>(
            container->property("dataTable").value<QObject*>());
        if (dataTable) {
            connect(dataTable, &QTableWidget::cellClicked, this,
                    [this, dataTable](int row, int) {
                        onUebersichtRowActivated(dataTable->item(row, 0));
                    });
        }

        m_stack->addWidget(container);
        m_pinnedBar->addTab(uebersichtTitle);
        m_tabYears.append(-1); // kein Jahr für den Übersicht-Tab
    }

    // ── Jahres-Tabs (Index 1..n, Reihenfolge wie in years, in m_yearsBar) ──
    for (int year : years) {
        auto* container = buildFrozenTable(
            jahresHeaders.size(), jahresHeaders, jahresColWidths,
            [&populateJahresData, year](QTableWidget* data) { populateJahresData(year, data); },
            [&populateJahresFooter, year](QTableWidget* footer) { populateJahresFooter(year, footer); },
            jahresDocColumn);

        m_stack->addWidget(container);
        m_yearsBar->addTab(jahresTitleForYear(year));
        m_tabYears.append(year);
    }

    setCurrentIndex(0);
    m_suppressTabSignal = false;
}

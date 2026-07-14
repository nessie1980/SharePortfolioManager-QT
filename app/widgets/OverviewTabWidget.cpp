// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "OverviewTabWidget.h"

#include <QVBoxLayout>
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

    m_tabs = new QTabWidget(this);
    layout->addWidget(m_tabs);

    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (m_suppressTabSignal)
            return;
        // Selektion im verlassenen/neuen Tab leeren — Konsistenz mit dem
        // bisherigen Verhalten in ViewBuyEdit/ViewSaleEdit/ViewDividendEdit.
        for (int i = 0; i < m_tabs->count(); ++i) {
            auto* container = m_tabs->widget(i);
            if (!container)
                continue;
            auto* tbl = qobject_cast<QTableWidget*>(
                container->property("dataTable").value<QObject*>());
            if (tbl)
                tbl->clearSelection();
        }
        Q_UNUSED(index);
    });
}

// ── clear ─────────────────────────────────────────────────────────────────────

void OverviewTabWidget::clear()
{
    m_suppressTabSignal = true;
    while (m_tabs->count() > 0)
        m_tabs->removeTab(0);
    m_tabYears.clear();
    m_suppressTabSignal = false;
}

// ── buildFrozenTable ──────────────────────────────────────────────────────────
//
// Identisch zum bisherigen buildFrozenTable()-Muster (siehe ViewBuyEdit.cpp /
// ViewSaleEdit.cpp / ViewDividendEdit.cpp) — hier einmalig statt dreifach.

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
    // Fett gedruckte Spaltenköpfe, identisch zu ViewSaleEdit/ViewDividendEdit/
    // ViewBrokerageEdit (13.07.2026 nachgezogen — fehlte bei der Extraktion).
    QFont headerFont = data->horizontalHeader()->font();
    headerFont.setBold(true);
    data->horizontalHeader()->setFont(headerFont);
    data->setEditTriggers(QAbstractItemView::NoEditTriggers);
    data->setSelectionBehavior(QAbstractItemView::SelectRows);
    data->setSelectionMode(QAbstractItemView::SingleSelection);
    data->setAlternatingRowColors(true);
    data->verticalHeader()->setVisible(false);
    data->setFrameShape(QFrame::NoFrame);
    data->horizontalHeader()->setStretchLastSection(false);

    populateData(data);

    for (int c = 0; c < colCount && c < colWidths.size(); ++c) {
        if (colWidths.at(c) < 0)
            data->horizontalHeader()->setSectionResizeMode(c, QHeaderView::Stretch);
        else
            data->setColumnWidth(c, colWidths.at(c));
    }

    // ── Footer table (1 Zeile, kein Header, keine Scrollbars) ─────────────────
    auto* footer = new QTableWidget(1, colCount);
    footer->setEditTriggers(QAbstractItemView::NoEditTriggers);
    footer->setSelectionMode(QAbstractItemView::NoSelection);
    footer->horizontalHeader()->setVisible(false);
    footer->verticalHeader()->setVisible(false);
    footer->setFrameShape(QFrame::NoFrame);
    footer->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    footer->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    footer->horizontalHeader()->setStretchLastSection(false);

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
        m_tabs->setCurrentIndex(idx);
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

    while (m_tabs->count() > 0)
        m_tabs->removeTab(0);
    m_tabYears.clear();

    if (years.isEmpty()) {
        m_suppressTabSignal = false;
        return;
    }

    // ── Übersicht-Tab (Index 0) ───────────────────────────────────────────
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

        m_tabs->addTab(container, uebersichtTitle);
        m_tabYears.append(-1); // kein Jahr für den Übersicht-Tab
    }

    // ── Jahres-Tabs (Index 1..n, Reihenfolge wie in years) ────────────────
    for (int year : years) {
        auto* container = buildFrozenTable(
            jahresHeaders.size(), jahresHeaders, jahresColWidths,
            [&populateJahresData, year](QTableWidget* data) { populateJahresData(year, data); },
            [&populateJahresFooter, year](QTableWidget* footer) { populateJahresFooter(year, footer); },
            jahresDocColumn);

        m_tabs->addTab(container, jahresTitleForYear(year));
        m_tabYears.append(year);
    }

    m_tabs->setCurrentIndex(0);
    m_suppressTabSignal = false;
}

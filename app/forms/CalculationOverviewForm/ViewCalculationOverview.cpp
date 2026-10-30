// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ViewCalculationOverview.h"

#include "../../IconProvider.h"
#include "../../config/AppSettings.h"
#include "../../widgets/GridStyle.h"

#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTableWidget>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

namespace {

/// Spalten der Gegenüberstellung.
enum ComparisonColumn { CNumber, CLabel, CMarket, CDepot, CDiff, CWay, CUsedIn, CCount };
/// Spalten des Abgleichs.
enum CheckColumn { KLabel, KTable, KChart, KDiff, KResult, KCount };

QTableWidget* makeTable(int columns, const QStringList& headers, const QString& objectName)
{
    auto* table = new QTableWidget(0, columns);
    table->setObjectName(objectName);
    table->setHorizontalHeaderLabels(headers);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
    table->setWordWrap(true);
    table->setShowGrid(true);
    GridStyle::applySelectionStyle(table);
    return table;
}

QTableWidgetItem* makeItem(const QString& text, Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter)
{
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(alignment);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

const Qt::Alignment kRight = Qt::AlignRight | Qt::AlignVCenter;

} // namespace

// ── Konstruktor ───────────────────────────────────────────────────────────────

ViewCalculationOverview::ViewCalculationOverview(QWidget* parent)
    : QWidget(parent)
    , m_presenter(this, &m_model)
{
    setObjectName(QStringLiteral("ViewCalculationOverview"));
    setupUi();

    connect(m_entryTree, &QTreeWidget::currentItemChanged,
            &m_presenter, &PresenterCalculationOverview::onSelectionChanged);
    connect(m_saveButton, &QPushButton::clicked,
            this, &ViewCalculationOverview::saveReportRequested);

    m_presenter.loadAndDisplay();
}

// ── setupUi ───────────────────────────────────────────────────────────────────

void ViewCalculationOverview::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    m_summaryLabel = new QLabel();
    m_summaryLabel->setObjectName(QStringLiteral("calculationOverviewSummary"));
    mainLayout->addWidget(m_summaryLabel);

    auto* splitter = new QSplitter(Qt::Horizontal);
    mainLayout->addWidget(splitter, 1);

    // ── Links: Auswahl ────────────────────────────────────────────────────
    m_entryTree = new QTreeWidget();
    m_entryTree->setObjectName(QStringLiteral("calculationOverviewList"));
    m_entryTree->setHeaderHidden(true);
    m_entryTree->setColumnCount(1);
    m_entryTree->setRootIsDecorated(true);
    m_entryTree->setMinimumWidth(200);
    splitter->addWidget(m_entryTree);

    // ── Rechts: Detail ────────────────────────────────────────────────────
    auto* detail       = new QWidget();
    auto* detailLayout = new QVBoxLayout(detail);
    detailLayout->setContentsMargins(4, 0, 0, 0);
    detailLayout->setSpacing(4);

    m_titleLabel = new QLabel();
    m_titleLabel->setObjectName(QStringLiteral("calculationOverviewTitle"));
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.2);
    m_titleLabel->setFont(titleFont);
    detailLayout->addWidget(m_titleLabel);

    m_subtitleLabel = new QLabel();
    m_subtitleLabel->setObjectName(QStringLiteral("calculationOverviewSubtitle"));
    m_subtitleLabel->setWordWrap(true);
    detailLayout->addWidget(m_subtitleLabel);

    // Gegenüberstellung
    m_comparisonBox = new QGroupBox();
    auto* comparisonLayout = new QVBoxLayout(m_comparisonBox);
    m_comparisonTable = makeTable(CCount,
                                  { tr("Nr"), tr("Wert"), tr("Marktwert"), tr("Depotwert"),
                                    tr("Depot − Markt"), tr("Rechenweg"), tr("Fließt ein in") },
                                  QStringLiteral("calculationComparisonTable"));
    QHeaderView* ch = m_comparisonTable->horizontalHeader();
    ch->setSectionResizeMode(QHeaderView::Interactive);
    ch->setSectionResizeMode(CNumber, QHeaderView::ResizeToContents);
    ch->setSectionResizeMode(CMarket, QHeaderView::ResizeToContents);
    ch->setSectionResizeMode(CDepot,  QHeaderView::ResizeToContents);
    ch->setSectionResizeMode(CDiff,   QHeaderView::ResizeToContents);
    ch->setStretchLastSection(true);
    m_comparisonTable->setColumnWidth(CLabel, 210);
    m_comparisonTable->setColumnWidth(CWay,   330);
    // Footer-Werte stehen immer in derselben Spalte wie der Tabellenwert
    // darüber — "→ Footer" meint deshalb die eigene Spalte.
    m_comparisonTable->horizontalHeaderItem(CUsedIn)->setToolTip(
        tr("Wo der Wert in den Tabs \"Kompletter Depotwert\" und \"Kompletter Marktwert\" "
           "steht. \"→ Footer\" meint die Footer-Zeile in derselben Spalte."));
    comparisonLayout->addWidget(m_comparisonTable);

    // Gegenüberstellung oben, Abgleich und Auffälligkeiten unten — über einen
    // Teiler, damit sich der Platz nach Bedarf verschieben lässt.
    auto* verticalSplitter = new QSplitter(Qt::Vertical);
    verticalSplitter->setChildrenCollapsible(false);
    verticalSplitter->addWidget(m_comparisonBox);
    auto* lower       = new QWidget();
    auto* lowerLayout = new QVBoxLayout(lower);
    lowerLayout->setContentsMargins(0, 0, 0, 0);
    lowerLayout->setSpacing(4);
    verticalSplitter->addWidget(lower);
    verticalSplitter->setStretchFactor(0, 3);
    verticalSplitter->setStretchFactor(1, 1);
    detailLayout->addWidget(verticalSplitter, 1);

    // Abgleich gegen die Charts
    m_checkBox = new QGroupBox();
    auto* checkLayout = new QVBoxLayout(m_checkBox);
    m_checkTable = makeTable(KCount,
                             { tr("Wert"), tr("Tabelle"), tr("Chart"), tr("Differenz"), tr("Ergebnis") },
                             QStringLiteral("calculationCheckTable"));
    QHeaderView* kh = m_checkTable->horizontalHeader();
    kh->setSectionResizeMode(QHeaderView::ResizeToContents);
    kh->setSectionResizeMode(KLabel, QHeaderView::Stretch);
    m_checkTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    checkLayout->addWidget(m_checkTable);
    m_checkNotesLabel = new QLabel();
    m_checkNotesLabel->setObjectName(QStringLiteral("calculationCheckNotes"));
    m_checkNotesLabel->setWordWrap(true);
    checkLayout->addWidget(m_checkNotesLabel);
    lowerLayout->addWidget(m_checkBox, 0);

    // Auffälligkeiten
    auto* findingsBox    = new QGroupBox(tr("Auffälligkeiten"));
    auto* findingsLayout = new QVBoxLayout(findingsBox);
    m_findingsList = new QListWidget();
    m_findingsList->setObjectName(QStringLiteral("calculationFindingsList"));
    m_findingsList->setWordWrap(true);
    findingsLayout->addWidget(m_findingsList);
    lowerLayout->addWidget(findingsBox, 1);

    splitter->addWidget(detail);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({ 260, 1000 });

    // ── Unten: Bericht speichern ──────────────────────────────────────────
    auto* buttonRow = new QHBoxLayout();
    buttonRow->addStretch(1);
    m_saveButton = new QPushButton(tr("Bericht speichern…"));
    m_saveButton->setObjectName(QStringLiteral("calculationReportSaveButton"));
    m_saveButton->setToolTip(tr("Schreibt den Berechnungsbericht als Textdatei, mit Realwerten "
                                "und zusätzlich anonymisiert."));
    buttonRow->addWidget(m_saveButton);
    mainLayout->addLayout(buttonRow);
}

// ── IViewCalculationOverview ──────────────────────────────────────────────────

QString ViewCalculationOverview::selectedKey() const
{
    const QTreeWidgetItem* item = m_entryTree->currentItem();
    return item ? item->data(0, Qt::UserRole).toString() : QString();
}

void ViewCalculationOverview::setEntries(const QList<CalculationOverviewEntry>& entries)
{
    const QSignalBlocker blocker(m_entryTree);
    m_entryTree->clear();

    using Group = CalculationOverviewEntry::Group;
    int held = 0, sold = 0;
    for (const CalculationOverviewEntry& e : entries) {
        if (e.group == Group::Held) ++held;
        if (e.group == Group::Sold) ++sold;
    }

    // Gruppenüberschriften sind nicht auswählbar und tragen keinen Schlüssel.
    const auto makeGroup = [this](const QString& title) {
        auto* item = new QTreeWidgetItem(m_entryTree, { title });
        item->setFlags(Qt::ItemIsEnabled);
        QFont font = item->font(0);
        font.setBold(true);
        item->setFont(0, font);
        return item;
    };

    QTreeWidgetItem* heldGroup = nullptr;
    QTreeWidgetItem* soldGroup = nullptr;
    for (const CalculationOverviewEntry& e : entries) {
        QTreeWidgetItem* item = nullptr;
        switch (e.group) {
        case Group::Total:
            item = new QTreeWidgetItem(m_entryTree, { e.title });
            break;
        case Group::Held:
            if (!heldGroup)
                heldGroup = makeGroup(tr("Im Bestand (%1)").arg(held));
            item = new QTreeWidgetItem(heldGroup, { e.title });
            break;
        case Group::Sold:
            if (!soldGroup)
                soldGroup = makeGroup(tr("Vollständig verkauft (%1)").arg(sold));
            item = new QTreeWidgetItem(soldGroup, { e.title });
            break;
        }
        item->setData(0, Qt::UserRole, e.key);
        item->setIcon(0, toneIcon(e.tone));
        item->setToolTip(0, e.toolTip);
        if (e.tone == CalculationOverviewTone::Deviation)
            item->setForeground(0, toneColor(e.tone));
    }
    m_entryTree->expandAll();
}

void ViewCalculationOverview::setSelectedKey(const QString& key)
{
    const QSignalBlocker blocker(m_entryTree);
    for (QTreeWidgetItemIterator it(m_entryTree); *it; ++it) {
        if ((*it)->data(0, Qt::UserRole).toString() == key) {
            m_entryTree->setCurrentItem(*it);
            return;
        }
    }
}

void ViewCalculationOverview::setSummary(const QString& text)
{
    m_summaryLabel->setText(text);
}

void ViewCalculationOverview::showDetail(const CalculationOverviewDetail& detail)
{
    m_titleLabel->setText(detail.title);
    m_subtitleLabel->setText(detail.subtitle);

    // ── Gegenüberstellung ─────────────────────────────────────────────────
    m_comparisonBox->setTitle(detail.comparisonTitle);
    m_comparisonTable->setRowCount(0);
    for (const CalculationOverviewComparisonRow& r : detail.comparison) {
        const int row = m_comparisonTable->rowCount();
        m_comparisonTable->insertRow(row);
        m_comparisonTable->setItem(row, CNumber, makeItem(r.number, kRight));
        m_comparisonTable->setItem(row, CLabel,  makeItem(r.label));
        m_comparisonTable->setItem(row, CMarket, makeItem(r.market, kRight));
        m_comparisonTable->setItem(row, CDepot,  makeItem(r.depot,  kRight));
        m_comparisonTable->setItem(row, CDiff,   makeItem(r.diff,   kRight));
        m_comparisonTable->setItem(row, CWay,    makeItem(r.way));
        m_comparisonTable->setItem(row, CUsedIn, makeItem(r.usedIn));
        // Rechenweg mit Zahlen auf der ganzen Zeile, "Fließt ein in" zusätzlich
        // vollständig auf der eigenen Zelle (falls sie abgeschnitten ist).
        for (int c = 0; c < CCount; ++c) {
            if (c != CUsedIn)
                m_comparisonTable->item(row, c)->setToolTip(r.toolTip);
        }
        m_comparisonTable->item(row, CUsedIn)->setToolTip(r.usedIn);
    }
    m_comparisonTable->resizeRowsToContents();

    // ── Abgleich ──────────────────────────────────────────────────────────
    m_checkBox->setTitle(detail.checkTitle);
    m_checkTable->setRowCount(0);
    for (const CalculationOverviewCheckRow& r : detail.checks) {
        const int row = m_checkTable->rowCount();
        m_checkTable->insertRow(row);
        m_checkTable->setItem(row, KLabel,  makeItem(r.label));
        m_checkTable->setItem(row, KTable,  makeItem(r.tableValue, kRight));
        m_checkTable->setItem(row, KChart,  makeItem(r.chartValue, kRight));
        m_checkTable->setItem(row, KDiff,   makeItem(r.diff, kRight));
        auto* result = makeItem(r.result);
        result->setIcon(toneIcon(r.tone));
        if (r.tone != CalculationOverviewTone::Ok)
            result->setForeground(toneColor(r.tone));
        m_checkTable->setItem(row, KResult, result);
    }
    m_checkTable->setVisible(!detail.checks.isEmpty());
    fitHeightToRows(m_checkTable);
    m_checkNotesLabel->setText(detail.checkNotes.join(QLatin1Char('\n')));
    m_checkNotesLabel->setVisible(!detail.checkNotes.isEmpty());

    // ── Auffälligkeiten ───────────────────────────────────────────────────
    m_findingsList->clear();
    if (detail.findings.isEmpty()) {
        auto* item = new QListWidgetItem(toneIcon(CalculationOverviewTone::Ok),
                                         tr("Keine — Tabellen, Footer und Charts stimmen überein."));
        m_findingsList->addItem(item);
    }
    for (const CalculationOverviewFinding& f : detail.findings) {
        auto* item = new QListWidgetItem(toneIcon(f.tone), f.text);
        item->setForeground(toneColor(f.tone));
        m_findingsList->addItem(item);
    }
}

// ── Hilfsfunktionen ───────────────────────────────────────────────────────────

QColor ViewCalculationOverview::toneColor(CalculationOverviewTone tone)
{
    // Dieselben Farben wie die Status-Meldungen und die Entwicklungsspalten
    // (AppSettings-Logfarben: 2 = Warnung, 3 = Fehler, 5 = Erfolg).
    switch (tone) {
    case CalculationOverviewTone::Deviation: return AppSettings::instance().logColorAt(3);
    case CalculationOverviewTone::Hint:      return AppSettings::instance().logColorAt(2);
    case CalculationOverviewTone::Ok:        return AppSettings::instance().logColorAt(5);
    case CalculationOverviewTone::Normal:    break;
    }
    return QColor();
}

QIcon ViewCalculationOverview::toneIcon(CalculationOverviewTone tone)
{
    switch (tone) {
    case CalculationOverviewTone::Deviation: return IconProvider::icon(IconProvider::SearchFailed);
    case CalculationOverviewTone::Hint:      return IconProvider::icon(IconProvider::SearchInfo);
    case CalculationOverviewTone::Ok:        return IconProvider::icon(IconProvider::SearchOk);
    case CalculationOverviewTone::Normal:    break;
    }
    return QIcon();
}

void ViewCalculationOverview::fitHeightToRows(QTableWidget* table)
{
    // Höchstens fünf Zeilen: genau so hoch wie der Inhalt, damit die
    // Gegenüberstellung darüber den restlichen Platz bekommt.
    table->resizeRowsToContents();
    int height = table->horizontalHeader()->sizeHint().height() + 2 * table->frameWidth();
    for (int r = 0; r < table->rowCount(); ++r)
        height += table->rowHeight(r);
    table->setFixedHeight(height);
}

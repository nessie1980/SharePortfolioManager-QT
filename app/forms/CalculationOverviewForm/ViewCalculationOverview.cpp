// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ViewCalculationOverview.h"

#include "../../IconProvider.h"
#include "../../config/AppSettings.h"
#include "../../widgets/CollapsibleSection.h"
#include "../../widgets/GridStyle.h"

#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTableWidget>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include <utility>

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

    // Gegenüberstellung oben, darunter die Gruppen zum Aufklappen — über
    // einen Teiler, damit sich der Platz nach Bedarf verschieben lässt.
    // Seit v1.31.1 liegen Teiler und Gruppen mit in der Groupbox der
    // Gegenüberstellung (Variante A, Nessies Entscheidung 18.11.2026): ein
    // Rahmen um alles, keine zweite Überschrift, kein Platz für eine zweite
    // Box.
    auto* verticalSplitter = new QSplitter(Qt::Vertical);
    verticalSplitter->setObjectName(QStringLiteral("calculationDetailSplitter"));
    verticalSplitter->setChildrenCollapsible(false);
    verticalSplitter->addWidget(m_comparisonTable);

    // Unten ein eigener Bildlaufbereich (Mischform, Nessies Entscheidung
    // 17.11.2026): Abgleich, Auffälligkeiten und die Gruppen je Aktie
    // untereinander. Die Tabellen darin sind so hoch wie ihr Inhalt und
    // füllen die Breite (seit v1.31.1); gescrollt wird nur hier, nicht in
    // den einzelnen Tabellen.
    auto* lowerScroll = new QScrollArea();
    lowerScroll->setObjectName(QStringLiteral("calculationGroupsScrollArea"));
    lowerScroll->setWidgetResizable(true);
    lowerScroll->setFrameShape(QFrame::NoFrame);
    auto* lower       = new QWidget();
    auto* lowerLayout = new QVBoxLayout(lower);
    lowerLayout->setContentsMargins(0, 0, 4, 0);
    lowerLayout->setSpacing(4);
    lowerScroll->setWidget(lower);
    verticalSplitter->addWidget(lowerScroll);
    verticalSplitter->setStretchFactor(0, 3);
    verticalSplitter->setStretchFactor(1, 2);
    comparisonLayout->addWidget(verticalSplitter);
    detailLayout->addWidget(m_comparisonBox, 1);

    // Abgleich gegen die Charts
    m_checkSection = new CollapsibleSection();
    m_checkSection->setObjectName(QStringLiteral("calculationCheckSection"));
    m_checkTable = makeTable(KCount,
                             { tr("Wert"), tr("Tabelle"), tr("Chart"), tr("Differenz"), tr("Ergebnis") },
                             QStringLiteral("calculationCheckTable"));
    QHeaderView* kh = m_checkTable->horizontalHeader();
    kh->setSectionResizeMode(QHeaderView::ResizeToContents);
    kh->setSectionResizeMode(KLabel, QHeaderView::Stretch);
    m_checkTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_checkSection->bodyLayout()->addWidget(m_checkTable);
    m_checkNotesLabel = new QLabel();
    m_checkNotesLabel->setObjectName(QStringLiteral("calculationCheckNotes"));
    m_checkNotesLabel->setWordWrap(true);
    m_checkSection->bodyLayout()->addWidget(m_checkNotesLabel);
    lowerLayout->addWidget(m_checkSection);

    // Auffälligkeiten
    m_findingsSection = new CollapsibleSection(tr("Auffälligkeiten"));
    m_findingsSection->setObjectName(QStringLiteral("calculationFindingsSection"));
    m_findingsList = new QListWidget();
    m_findingsList->setObjectName(QStringLiteral("calculationFindingsList"));
    m_findingsList->setWordWrap(true);
    m_findingsList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_findingsSection->bodyLayout()->addWidget(m_findingsList);
    lowerLayout->addWidget(m_findingsSection);

    // Gruppen je Aktie (Transaktionen, Bestand / FIFO) — bei jedem
    // showDetail() neu aufgebaut.
    auto* groups = new QWidget();
    groups->setObjectName(QStringLiteral("calculationGroups"));
    m_groupsLayout = new QVBoxLayout(groups);
    m_groupsLayout->setContentsMargins(0, 0, 0, 0);
    m_groupsLayout->setSpacing(4);
    lowerLayout->addWidget(groups);
    lowerLayout->addStretch(1);

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
    m_checkSection->setTitle(detail.checkTitle);
    m_checkSection->setSummary(detail.checkSummary, toneColor(detail.checkTone));
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
    m_checkSection->setExpanded(detail.checkExpanded);

    // ── Auffälligkeiten ───────────────────────────────────────────────────
    m_findingsSection->setSummary(detail.findingsSummary, toneColor(detail.findingsTone));
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
    fitHeightToRows(m_findingsList);
    m_findingsSection->setExpanded(detail.findingsExpanded);

    // ── Gruppen je Aktie ──────────────────────────────────────────────────
    // Neu aufgebaut, damit der vom Presenter vorgegebene Zustand gilt — ein
    // Auf- oder Zuklappen von Hand gilt nur bis zur nächsten Auswahl
    // (Nessie, 17.11.2026, Punkt 1a b).
    for (CollapsibleSection* section : std::as_const(m_groupSections)) {
        m_groupsLayout->removeWidget(section);
        section->hide();
        section->deleteLater();
    }
    m_groupSections.clear();
    for (const CalculationOverviewGroup& g : detail.groups) {
        CollapsibleSection* section = makeGroupSection(g);
        m_groupsLayout->addWidget(section);
        m_groupSections.append(section);
    }
}

// ── Gruppen ───────────────────────────────────────────────────────────────────

CollapsibleSection* ViewCalculationOverview::makeGroupSection(const CalculationOverviewGroup& group)
{
    auto* section = new CollapsibleSection(group.title);
    section->setObjectName(QStringLiteral("calculationGroupSection"));
    section->setSummary(group.summary, toneColor(group.tone));

    if (group.tables.isEmpty()) {
        auto* none = new QLabel(tr("Keine Einträge."));
        section->bodyLayout()->addWidget(none);
    }
    for (const CalculationOverviewTable& t : group.tables) {
        section->bodyLayout()->addWidget(makeGroupTable(t));
        if (!t.note.isEmpty()) {
            auto* note = new QLabel(t.note);
            note->setWordWrap(true);
            section->bodyLayout()->addWidget(note);
        }
    }
    section->setExpanded(group.expanded);
    return section;
}

QTableWidget* ViewCalculationOverview::makeGroupTable(const CalculationOverviewTable& t)
{
    auto* table = makeTable(int(t.headers.size()), t.headers,
                            QStringLiteral("calculationGroupTable"));
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setWordWrap(false);

    const auto alignmentOf = [&t](int column) {
        return t.rightAligned.value(column, false) ? kRight : Qt::AlignLeft | Qt::AlignVCenter;
    };

    for (int r = 0; r < t.rows.size(); ++r) {
        const QStringList& cells = t.rows.at(r);
        const QList<CalculationOverviewTone> tones = t.tones.value(r);
        table->insertRow(r);
        for (int c = 0; c < t.headers.size(); ++c) {
            auto* item = makeItem(cells.value(c), alignmentOf(c));
            const CalculationOverviewTone tone = tones.value(c, CalculationOverviewTone::Normal);
            if (tone != CalculationOverviewTone::Normal) {
                item->setForeground(toneColor(tone));
                if (tone == CalculationOverviewTone::Deviation) {
                    QFont font = item->font();
                    font.setBold(true);
                    item->setFont(font);
                }
            }
            table->setItem(r, c, item);
        }
    }

    // Summenzeile fett, wie die Footer der Übersichtstabellen.
    if (!t.sumRow.isEmpty()) {
        const int r = table->rowCount();
        table->insertRow(r);
        for (int c = 0; c < t.headers.size(); ++c) {
            auto* item = makeItem(t.sumRow.value(c), alignmentOf(c));
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
            table->setItem(r, c, item);
        }
    }

    // Spaltenverteilung (v1.31.1, Variante 2, Nessies Entscheidung
    // 18.11.2026): Die Tabelle füllt die volle Breite wie die
    // Abgleich-Tabelle. Textspalten (linksbündig) bleiben so breit wie ihr
    // Inhalt, die Zahlenspalten (rechtsbündig) teilen sich den Rest zu
    // gleichen Teilen. Ausnahme: Hat die Tabelle nur eine Zahlenspalte —
    // "Bestand (heutige Skala)" —, nimmt wie beim Abgleich die erste
    // Textspalte den Rest auf.
    QHeaderView* header = table->horizontalHeader();
    header->setStretchLastSection(false);
    table->resizeColumnsToContents();

    const int columns = int(t.headers.size());
    int numericCount  = 0;
    for (int c = 0; c < columns; ++c) {
        if (t.rightAligned.value(c, false))
            ++numericCount;
    }
    const bool stretchText = (numericCount == 1 && columns > 1);

    // Mindestbreite: Bei gleich breiten Zahlenspalten braucht jede so viel
    // Platz wie die breiteste von ihnen, sonst würde diese abgeschnitten.
    // Ist der Bereich schmaler, scrollt er als Ganzes waagerecht.
    int textWidth = 0, widestNumeric = 0;
    for (int c = 0; c < columns; ++c) {
        if (t.rightAligned.value(c, false))
            widestNumeric = qMax(widestNumeric, table->columnWidth(c));
        else
            textWidth += table->columnWidth(c);
    }
    const int minimum = stretchText ? header->length()
                                    : textWidth + numericCount * widestNumeric;
    table->setMinimumWidth(minimum + 2 * table->frameWidth());

    bool textStretched = false;
    for (int c = 0; c < columns; ++c) {
        const bool numeric = t.rightAligned.value(c, false);
        QHeaderView::ResizeMode mode = QHeaderView::ResizeToContents;
        if (stretchText) {
            if (!numeric && !textStretched) {
                mode          = QHeaderView::Stretch;
                textStretched = true;
            }
        } else if (numeric) {
            mode = QHeaderView::Stretch;
        }
        header->setSectionResizeMode(c, mode);
    }

    fitHeightToRows(table);
    return table;
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
    // Genau so hoch wie der Inhalt, damit nur der Bildlaufbereich darum
    // scrollt und nicht jede Tabelle für sich.
    table->resizeRowsToContents();
    int height = table->horizontalHeader()->sizeHint().height() + 2 * table->frameWidth();
    for (int r = 0; r < table->rowCount(); ++r)
        height += table->rowHeight(r);
    table->setFixedHeight(height);
}

void ViewCalculationOverview::fitHeightToRows(QListWidget* list)
{
    // Wie bei den Tabellen: genau so hoch wie die Einträge (ergänzt
    // 18.11.2026, seit die Liste in einer Gruppe im Bildlaufbereich steht).
    int height = 2 * list->frameWidth();
    for (int r = 0; r < list->count(); ++r)
        height += list->sizeHintForRow(r);
    list->setFixedHeight(height);
}

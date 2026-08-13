// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ViewShareSplitEdit.h"
#include "PresenterShareSplitEdit.h"
#include "ModelShareSplitEdit.h"
#include "../../IconProvider.h"
#include "../../config/AppSettings.h"
#include "../../core/DocumentRootMigrator.h"
#include "../../widgets/DocumentPreviewPanel.h"
#include "../UiConstants.h"
#include "../OwnMessageBoxForm/OwnMessageBox.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDoubleValidator>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QLocale>
#include <QSizePolicy>

namespace {
/// Spaltenindizes der Übersichtstabelle.
enum Column { kColDate = 0, kColRatio, kColConversion, kColAdjusted, kColComment,
              kColDocument, kColCount };

/// Breite der Dokument-Spalte, projektweit vereinheitlicht (17.07.2026).
constexpr int kDocumentColumnWidth = 36;
}

// ── Constructor ───────────────────────────────────────────────────────────────

ViewShareSplitEdit::ViewShareSplitEdit(const QString& shareGuid, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Hinzufügen / editieren der Splits dieser Aktie"));
    setFixedSize(1100, 680);

    setupUi();

    auto* model = new ModelShareSplitEdit();
    m_presenter = new PresenterShareSplitEdit(this, model, shareGuid, this);

    connect(m_btnAdd,    &QPushButton::clicked, m_presenter, &PresenterShareSplitEdit::onSave);
    connect(m_btnRemove, &QPushButton::clicked, m_presenter, &PresenterShareSplitEdit::onRemove);
    connect(m_btnReset,  &QPushButton::clicked, m_presenter, &PresenterShareSplitEdit::onReset);
    connect(m_btnClose,  &QPushButton::clicked, m_presenter, &PresenterShareSplitEdit::onClose);

    // Verhältnis-Eingaben halten die Umrechnungs-Vorschau aktuell.
    connect(m_ratioNew, &QLineEdit::textChanged,
            m_presenter, &PresenterShareSplitEdit::onValuesChanged);
    connect(m_ratioOld, &QLineEdit::textChanged,
            m_presenter, &PresenterShareSplitEdit::onValuesChanged);

    connect(m_table, &QTableWidget::itemSelectionChanged,
            this, &ViewShareSplitEdit::onTableSelectionChanged);

    connect(m_btnBrowseDoc, &QPushButton::clicked,
            this, &ViewShareSplitEdit::onBrowseDocument);
}

// ── setupUi ───────────────────────────────────────────────────────────────────

void ViewShareSplitEdit::setupUi()
{
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // Linke Spalte: Formular, Buttons, Übersicht.
    auto* leftColumn = new QVBoxLayout;
    leftColumn->setContentsMargins(0, 0, 0, 0);
    leftColumn->setSpacing(6);
    leftColumn->addWidget(createSplitDataGroup());
    leftColumn->addWidget(createDocumentGroup());
    leftColumn->addWidget(createButtonBar());
    leftColumn->addWidget(createOverviewGroup(), 1);

    mainLayout->addLayout(leftColumn, 3);
    mainLayout->addWidget(createPreviewPanel(), 2);
}

// ── createSplitDataGroup ──────────────────────────────────────────────────────

QGroupBox* ViewShareSplitEdit::createSplitDataGroup()
{
    m_splitDataGroup = new QGroupBox(tr("  Split hinzufügen"));
    m_splitDataGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto* grid = new QGridLayout(m_splitDataGroup);
    grid->setColumnStretch(1, 1);
    grid->setVerticalSpacing(4);
    grid->setHorizontalSpacing(8);
    grid->setContentsMargins(8, 8, 8, 10);

    const auto addRow = [&](int row, const QString& labelText, QWidget* field) {
        auto* lbl = new QLabel(labelText);
        lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(lbl,   row, 0);
        grid->addWidget(field, row, 1);
    };

    int row = 0;

    // Ex-Tag. Zukünftige Daten sind erlaubt (Nessies Entscheidung 08.08.2026):
    // ein angekündigter Split darf sofort erfasst werden — ShareSplitAdjuster
    // rechnet ohnehin nur Datensätze VOR dem Splittag um.
    m_date = new QDateEdit(QDate::currentDate());
    m_date->setObjectName(QStringLiteral("splitDate"));
    m_date->setCalendarPopup(true);
    m_date->setDisplayFormat(tr("dd.MM.yyyy"));
    m_date->setMinimumDate(QDate(2000, 1, 1));
    m_date->setMaximumDate(QDate(9999, 12, 31));
    m_date->setFixedHeight(UiConstants::kFieldHeight);
    addRow(row++, tr("Ex-Tag:"), m_date);

    // Verhältnis neu : alt
    auto* ratioWidget = new QWidget;
    auto* ratioLayout = new QHBoxLayout(ratioWidget);
    ratioLayout->setContentsMargins(0, 0, 0, 0);
    ratioLayout->setSpacing(4);

    const auto makeRatioField = [](const QString& objectName) {
        auto* le = new QLineEdit(QStringLiteral("1"));
        le->setObjectName(objectName);
        le->setValidator(new QDoubleValidator(0.0, 1.0e9, 4, le));
        le->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        le->setFixedHeight(UiConstants::kFieldHeight);
        le->setFixedWidth(80);
        return le;
    };
    m_ratioNew = makeRatioField(QStringLiteral("ratioNew"));
    m_ratioOld = makeRatioField(QStringLiteral("ratioOld"));

    ratioLayout->addWidget(m_ratioNew);
    ratioLayout->addWidget(new QLabel(QStringLiteral(":")));
    ratioLayout->addWidget(m_ratioOld);
    ratioLayout->addWidget(new QLabel(tr("(neu : alt)")));
    ratioLayout->addStretch(1);
    addRow(row++, tr("Verhältnis:"), ratioWidget);

    // Umrechnung (abgeleitet)
    //
    // Notationshinweis (11.08.2026, Feldfall Alphabet; als Tooltip statt
    // Dauertext seit 13.08.2026, Nessies Vorgabe): Bankmitteilungen nennen
    // ueblicherweise das Zuteilungsverhaeltnis ("1:19" = 19 ZUSAETZLICHE
    // Stuecke je gehaltenem Stueck), die Maske erwartet aber das
    // Umrechnungsverhaeltnis neu:alt (im selben Beispiel 20:1) — ohne diesen
    // Hinweis wird der Faktor systematisch um genau eins zu klein
    // eingetragen. Erste Fassung war ein dauerhaft sichtbares Label unter
    // dieser Zeile; das wirkte im Dialog aber zu aufdringlich, deshalb jetzt
    // ein Tooltip auf dem Umrechnungs-Feld selbst — gleiches Muster wie beim
    // Tooltip auf "Kurshistorie" direkt darunter.
    m_factorPreview = new QLineEdit;
    m_factorPreview->setObjectName(QStringLiteral("factorPreview"));
    m_factorPreview->setReadOnly(true);
    m_factorPreview->setEnabled(false);
    m_factorPreview->setFixedHeight(UiConstants::kFieldHeight);
    m_factorPreview->setToolTip(
        tr("Bankmitteilungen nennen oft das Zuteilungsverhältnis, z. B. "
           "„1:19“ = 19 zusätzliche Aktien je gehaltenem Stück.\n"
           "Einzutragen ist hier das Umrechnungsverhältnis neu:alt (im "
           "Beispiel 20:1)."));
    addRow(row++, tr("Umrechnung:"), m_factorPreview);

    // prices_adjusted
    m_pricesAdjusted = new QCheckBox(
        tr("Kurshistorie vor dem Ex-Tag liegt bereits split-bereinigt vor"));
    m_pricesAdjusted->setObjectName(QStringLiteral("pricesAdjusted"));
    m_pricesAdjusted->setToolTip(
        tr("Anhaken, wenn die Tageswerte vom Anbieter bereits in heutigen "
           "Stücken geliefert wurden. Dann zeigt die Kurshistorie am Splittag "
           "keinen Sprung mehr und darf nicht zusätzlich umgerechnet werden."));
    addRow(row++, tr("Kurshistorie:"), m_pricesAdjusted);

    // Kommentar
    m_comment = new QLineEdit;
    m_comment->setObjectName(QStringLiteral("comment"));
    m_comment->setPlaceholderText(tr("z. B. Quelle oder Anlass des Splits"));
    m_comment->setFixedHeight(UiConstants::kFieldHeight);
    addRow(row++, tr("Kommentar:"), m_comment);

    return m_splitDataGroup;
}

// ── createDocumentGroup ───────────────────────────────────────────────────────
// Eigene Groupbox statt Zeile in createSplitDataGroup() (13.08.2026, Nessies
// Vorgabe) — Icon und Layout jetzt 1:1 wie in ViewBuyEdit::createDocumentGroup()
// (Ordner-Icon statt "…"-Button, read-only Pfadfeld). Der Root-Zwang wird
// weiterhin in onBrowseDocument() geprüft, die Doppelbelegungs-Prüfung läuft
// jetzt ausschliesslich über die Dateiauswahl (onDocumentSelected() ruft
// PresenterShareSplitEdit::onDocumentPathEdited() bereits selbst auf) — die
// bisherige editingFinished-Verbindung für manuelles Eintippen entfällt, das
// Feld lässt sich nicht mehr direkt beschreiben.
QGroupBox* ViewShareSplitEdit::createDocumentGroup()
{
    auto* gb   = new QGroupBox(tr("  Dokument"));
    gb->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    auto* grid = new QGridLayout(gb);
    grid->setColumnStretch(1, 1);
    grid->setVerticalSpacing(4);
    grid->setHorizontalSpacing(8);
    grid->setContentsMargins(8, 8, 8, 10);

    m_documentPath = new QLineEdit;
    m_documentPath->setObjectName(QStringLiteral("documentPath"));
    m_documentPath->setReadOnly(true);
    m_documentPath->setPlaceholderText(tr("Kein Dokument ausgewählt …"));
    m_documentPath->setFixedHeight(UiConstants::kFieldHeight);

    m_btnBrowseDoc = new QPushButton(IconProvider::icon(IconProvider::MenuFolderOpen16), QString());
    m_btnBrowseDoc->setObjectName(QStringLiteral("btnBrowseDocument"));
    m_btnBrowseDoc->setFixedWidth(36);
    m_btnBrowseDoc->setFixedHeight(UiConstants::kFieldHeight);
    m_btnBrowseDoc->setToolTip(tr("PDF-Dokument auswählen"));

    auto* docRow = new QWidget;
    auto* docLayout = new QHBoxLayout(docRow);
    docLayout->setContentsMargins(0, 0, 0, 0);
    docLayout->setSpacing(4);
    docLayout->addWidget(m_documentPath, 1);
    docLayout->addWidget(m_btnBrowseDoc, 0);

    auto* label = new QLabel(tr("Dokument:"));
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(label,  0, 0);
    grid->addWidget(docRow, 0, 1);

    return gb;
}

// ── createButtonBar ───────────────────────────────────────────────────────────

QWidget* ViewShareSplitEdit::createButtonBar()
{
    auto* bar    = new QWidget;
    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    const auto makeButton = [](IconProvider::IconName icon, const QString& text) {
        auto* btn = new QPushButton(IconProvider::icon(icon), text);
        btn->setFixedHeight(UiConstants::kButtonHeight);
        btn->setMinimumWidth(110);
        return btn;
    };

    m_btnAdd    = makeButton(IconProvider::ButtonAdd,    tr("Hinzufügen"));
    m_btnRemove = makeButton(IconProvider::ButtonDelete, tr("Entfernen"));
    m_btnReset  = makeButton(IconProvider::ButtonReset,  tr("Reset"));
    m_btnClose  = makeButton(IconProvider::ButtonCancel, tr("Schließen"));

    layout->addStretch(1);
    layout->addWidget(m_btnAdd);
    layout->addWidget(m_btnRemove);
    layout->addWidget(m_btnReset);
    layout->addWidget(m_btnClose);

    return bar;
}

// ── createOverviewGroup ───────────────────────────────────────────────────────

QGroupBox* ViewShareSplitEdit::createOverviewGroup()
{
    auto* gb     = new QGroupBox(tr("  Erfasste Splits"));
    auto* layout = new QVBoxLayout(gb);
    layout->setContentsMargins(8, 8, 8, 8);

    m_table = new QTableWidget(0, kColCount);
    m_table->setObjectName(QStringLiteral("splitsTable"));
    // Die Dokument-Spalte bleibt bewusst ohne Überschrift und bei 36 px —
    // projektweit vereinheitlicht am 17.07.2026 (siehe ARCHITECTURE.md).
    m_table->setHorizontalHeaderLabels({ tr("Datum"), tr("Verhältnis"),
                                         tr("Umrechnung"), tr("Kurse bereinigt"),
                                         tr("Kommentar"), QString() });
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);

    m_table->setColumnWidth(kColDate,       90);
    m_table->setColumnWidth(kColRatio,      90);
    m_table->setColumnWidth(kColConversion, 170);
    m_table->setColumnWidth(kColAdjusted,   110);
    m_table->setColumnWidth(kColDocument,   kDocumentColumnWidth);
    // Nicht die letzte Spalte strecken — das wäre die Dokument-Spalte, die
    // ihre feste Breite behalten soll. Stattdessen wächst der Kommentar.
    m_table->horizontalHeader()->setSectionResizeMode(kColComment, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(kColDocument, QHeaderView::Fixed);

    layout->addWidget(m_table);
    return gb;
}

// ── createPreviewPanel ────────────────────────────────────────────────────────

QWidget* ViewShareSplitEdit::createPreviewPanel()
{
    // DocumentPreviewPanel bringt bereits ein eigenes GroupBox
    // ("Dokumenten-Vorschau") mit, kein zusätzlicher Wrapper nötig — analog
    // zu ViewBuyEdit/ViewSaleEdit/ViewDividendEdit/ViewBrokerageEdit.
    m_previewPanel = new DocumentPreviewPanel(this);
    return m_previewPanel;
}

// ── IViewShareSplitEdit: read ─────────────────────────────────────────────────

QDate   ViewShareSplitEdit::splitDate()      const { return m_date->date(); }
double  ViewShareSplitEdit::ratioNew()       const { return parseDouble(m_ratioNew->text()); }
double  ViewShareSplitEdit::ratioOld()       const { return parseDouble(m_ratioOld->text()); }
bool    ViewShareSplitEdit::pricesAdjusted() const { return m_pricesAdjusted->isChecked(); }
QString ViewShareSplitEdit::comment()        const { return m_comment->text(); }
QString ViewShareSplitEdit::documentPath()   const { return m_documentPath->text(); }

// ── IViewShareSplitEdit: write ────────────────────────────────────────────────

void ViewShareSplitEdit::loadSplit(const ShareSplitObject& split)
{
    if (split.date().isValid())
        m_date->setDate(split.date());

    m_ratioNew->setText(formatRatioPart(split.ratioNew()));
    m_ratioOld->setText(formatRatioPart(split.ratioOld()));
    m_pricesAdjusted->setChecked(split.pricesAdjusted());
    m_comment->setText(split.comment());
    m_documentPath->setText(split.document());
}

void ViewShareSplitEdit::clearForm()
{
    m_date->setDate(QDate::currentDate());
    m_ratioNew->setText(QStringLiteral("1"));
    m_ratioOld->setText(QStringLiteral("1"));
    m_pricesAdjusted->setChecked(false);
    m_comment->clear();
    m_documentPath->clear();

    m_splitDataGroup->setTitle(tr("  Split hinzufügen"));
}

void ViewShareSplitEdit::setFactorPreview(const QString& text)
{
    m_factorPreview->setText(text);
}

void ViewShareSplitEdit::setDocumentPath(const QString& path)
{
    m_documentPath->setText(path);
}

void ViewShareSplitEdit::openPdfPreview(const QString& path)
{
    m_previewPanel->showDocument(path);
}

void ViewShareSplitEdit::clearPdfPreview()
{
    m_previewPanel->clearDocument();
}

void ViewShareSplitEdit::populateOverview(const QList<ShareSplitObject>& splits)
{
    // Während des Neuaufbaus darf itemSelectionChanged() nicht auf den
    // Presenter durchschlagen — sonst würde clearContents() die gerade
    // geladene Maske sofort wieder zurücksetzen.
    m_suppressSelectionSignal = true;

    m_table->clearContents();
    m_table->setRowCount(splits.size());

    const QLocale loc;
    int row = 0;
    for (const ShareSplitObject& s : splits) {
        const QString ratio = QStringLiteral("%1:%2")
                                  .arg(formatRatioPart(s.ratioNew()),
                                       formatRatioPart(s.ratioOld()));
        // Singular/Plural als getrennte Sätze statt eingesetztem Verb — sonst
        // wäre der Satzbau für Übersetzer nicht auflösbar.
        const QString from = formatRatioPart(s.ratioOld());
        const QString to   = formatRatioPart(s.ratioNew());
        const QString conversion = (qAbs(s.ratioNew() - 1.0) < 1e-9)
                                       ? tr("aus %1 Stk. wird %2 Stk.").arg(from, to)
                                       : tr("aus %1 Stk. werden %2 Stk.").arg(from, to);

        const QStringList texts = {
            loc.toString(s.date(), QLocale::ShortFormat),
            ratio,
            conversion,
            s.pricesAdjusted() ? tr("ja") : tr("nein"),
            s.comment(),
            QString()            // Dokument-Spalte trägt nur ein Icon
        };

        for (int col = 0; col < texts.size(); ++col) {
            auto* item = new QTableWidgetItem(texts.at(col));
            // GUID an jeder Zelle, damit die Auswahl unabhängig von der
            // angeklickten Spalte aufgelöst werden kann.
            item->setData(Qt::UserRole, s.guid());
            if (col == kColDate || col == kColRatio || col == kColAdjusted)
                item->setTextAlignment(Qt::AlignCenter);

            if (col == kColDocument && !s.document().isEmpty()) {
                item->setIcon(IconProvider::icon(IconProvider::DocPdfImage16));
                item->setToolTip(s.document());
                item->setTextAlignment(Qt::AlignCenter);
            }

            m_table->setItem(row, col, item);
        }
        ++row;
    }

    m_table->clearSelection();
    m_suppressSelectionSignal = false;
}

void ViewShareSplitEdit::setButtonStates(bool canRemove, bool isEdit)
{
    m_btnRemove->setEnabled(canRemove);

    if (isEdit) {
        m_btnAdd->setText(tr("Speichern"));
        m_btnAdd->setIcon(IconProvider::icon(IconProvider::ButtonSave));
    } else {
        m_btnAdd->setText(tr("Hinzufügen"));
        m_btnAdd->setIcon(IconProvider::icon(IconProvider::ButtonAdd));
    }

    m_splitDataGroup->setTitle(isEdit ? tr("  Split editieren")
                                      : tr("  Split hinzufügen"));
}

void ViewShareSplitEdit::showError(const QString& message)
{
    OwnMessageBox::critical(this, tr("Fehler"), message);
}

bool ViewShareSplitEdit::confirm(const QString& title, const QString& message)
{
    return OwnMessageBox::question(this, title, message);
}

void ViewShareSplitEdit::acceptAndClose()
{
    accept();
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void ViewShareSplitEdit::onTableSelectionChanged()
{
    if (m_suppressSelectionSignal || !m_presenter)
        return;

    const QList<QTableWidgetItem*> selected = m_table->selectedItems();
    if (selected.isEmpty()) {
        m_presenter->onRowSelected(QString());
        return;
    }
    m_presenter->onRowSelected(selected.constFirst()->data(Qt::UserRole).toString());
}

// ── onBrowseDocument ──────────────────────────────────────────────────────────

void ViewShareSplitEdit::onBrowseDocument()
{
    const QString root = AppSettings::instance().documentsRootPath();
    const QString startDir = !root.isEmpty() ? root : QDir::homePath();

    // Nur PDF zulässig — dieselbe Einschränkung wie in den anderen fünf
    // Dialogen (siehe ARCHITECTURE.md, "Offene Punkte / TODO": Word-/
    // Excel-Unterstützung bewusst nicht wieder eingebaut).
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("PDF-Dokument auswählen"),
        startDir,
        tr("PDF-Dokumente (*.pdf)"));

    if (path.isEmpty())
        return;

    if (!DocumentRootMigrator::isPathWithinRoot(path, root)) {
        OwnMessageBox::critical(this, tr("Fehler"),
            tr("Die gewählte Datei muss innerhalb des Dokument-Root-Verzeichnisses "
               "liegen:\n%1").arg(root));
        return;
    }

    m_presenter->onDocumentSelected(path);
}

// ── Helpers ───────────────────────────────────────────────────────────────────

double ViewShareSplitEdit::parseDouble(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return 0.0;

    // Erst mit deutschem Locale (Komma), dann als Rückfall mit Punkt —
    // dieselbe Konvention wie in ViewBrokerageEdit::parseDouble().
    bool ok = false;
    double value = QLocale().toDouble(trimmed, &ok);
    if (!ok)
        value = QString(trimmed).replace(QLatin1Char(','), QLatin1Char('.')).toDouble(&ok);
    return ok ? value : 0.0;
}

QString ViewShareSplitEdit::formatRatioPart(double value)
{
    const QLocale loc;
    const double rounded = static_cast<double>(qRound(value));
    if (qAbs(value - rounded) < 1e-9)
        return loc.toString(qRound(value));
    return loc.toString(value, 'f', 2);
}

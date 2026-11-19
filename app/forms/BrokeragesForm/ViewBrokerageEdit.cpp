// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ViewBrokerageEdit.h"
#include "../../widgets/OverviewTabFiller.h"
#include "PresenterBrokerageEdit.h"
#include "ModelBrokerageEdit.h"
#include "../../IconProvider.h"
#include "../../utils/NumberParser.h"
#include "../../utils/NumericFieldValidator.h"
#include "../../utils/ValueFormatter.h"
#include "../../config/AppSettings.h"
#include "../../core/DocumentRootMigrator.h"
#include "../UiConstants.h"

#include <QVBoxLayout>
#include <QPair>
#include <QHBoxLayout>
#include <QFileDialog>
#include "../OwnMessageBoxForm/OwnMessageBox.h"
#include <QLocale>
#include <QSizePolicy>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QDoubleValidator>

// ── Constructor ───────────────────────────────────────────────────────────────

ViewBrokerageEdit::ViewBrokerageEdit(const QString& shareGuid,
                                     QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Hinzufügen / editieren der Kosten dieser Aktie"));
    setFixedSize(1100, 680);

    setupUi();

    auto* model = new ModelBrokerageEdit();
    m_presenter = new PresenterBrokerageEdit(this, model, shareGuid, this);

    connect(m_btnAdd,    &QPushButton::clicked, m_presenter, &PresenterBrokerageEdit::onSave);
    connect(m_btnRemove, &QPushButton::clicked, m_presenter, &PresenterBrokerageEdit::onRemove);
    connect(m_btnReset,  &QPushButton::clicked, m_presenter, &PresenterBrokerageEdit::onReset);
    connect(m_btnClose,  &QPushButton::clicked, m_presenter, &PresenterBrokerageEdit::onClose);

    // Forward fee text changes so derived fields stay current.
    auto fwd = [this](const QString&) { m_presenter->onValuesChanged(); };
    connect(m_provision,  &QLineEdit::textChanged, this, fwd);
    connect(m_brokerFee,  &QLineEdit::textChanged, this, fwd);
    connect(m_traderFee,  &QLineEdit::textChanged, this, fwd);
    connect(m_reduction,  &QLineEdit::textChanged, this, fwd);

    // Live field validation
    connect(m_date, &QDateEdit::editingFinished,
            m_presenter, &PresenterBrokerageEdit::onDateEdited);

    auto connectFee = [this](QLineEdit* le, const QString& key) {
        connect(le, &QLineEdit::editingFinished, m_presenter,
                [this, le, key]() {
                    m_presenter->onFeeEdited(key, parseDouble(le->text()));
                });
    };
    connectFee(m_provision,  QStringLiteral("provision"));
    connectFee(m_brokerFee,  QStringLiteral("brokerFee"));
    connectFee(m_traderFee,  QStringLiteral("traderFee"));
    connectFee(m_reduction,  QStringLiteral("reduction"));
}

// ── setupUi ───────────────────────────────────────────────────────────────────

void ViewBrokerageEdit::setupUi()
{
    // Identical structure to BuysForm/SalesForm/DividendForm:
    // Left panel (form + overview) | Right panel (PDF preview) — top-level QHBoxLayout
    auto* main = new QHBoxLayout(this);
    main->setContentsMargins(6, 6, 6, 6);
    main->setSpacing(8);

    // Dokumenten-Vorschau zuerst erzeugen (aber erst unten ins Layout
    // einfügen) — createOverviewGroup() verbindet OverviewTabWidget::
    // documentActivated mit m_previewPanel und braucht dafür ein bereits
    // existierendes Objekt (derselbe Nullptr-Connect-Bugfix wie bei
    // ViewBuyEdit/ViewSaleEdit/ViewDividendEdit, s. ARCHITECTURE.md).
    auto* previewPanel = createPreviewPanel();

    // ── Left panel ────────────────────────────────────────────────────────
    auto* leftPanel  = new QWidget;
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(6);
    leftLayout->addWidget(createKostendatenGroup(), 0);
    leftLayout->addWidget(createDocumentGroup(),    0);
    leftLayout->addWidget(createButtonBar(),        0);
    leftLayout->addWidget(createOverviewGroup(),    1);

    main->addWidget(leftPanel,   3);
    main->addWidget(previewPanel, 2);
}

// ── createLeftPanel ───────────────────────────────────────────────────────────
// (Left panel is built inline in setupUi — this stub kept for header compatibility)
QWidget* ViewBrokerageEdit::createLeftPanel()
{
    // Not called — left panel is constructed directly in setupUi().
    return new QWidget;
}

// ── createKostendatenGroup ────────────────────────────────────────────────────

QGroupBox* ViewBrokerageEdit::createKostendatenGroup()
{
    m_kostendatenGroup = new QGroupBox(tr("  Kosten hinzufügen"));
    m_kostendatenGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    auto* grid = new QGridLayout(m_kostendatenGroup);
    grid->setColumnStretch(1, 1);
    grid->setVerticalSpacing(4);
    grid->setHorizontalSpacing(8);
    grid->setContentsMargins(8, 8, 8, 10);
    int row = 0;

    // ── Datum + Uhrzeit ───────────────────────────────────────────────────
    m_date = new QDateEdit(QDate::currentDate());
    m_date->setCalendarPopup(true);
    m_date->setDisplayFormat(QStringLiteral("d . M . yyyy"));
    m_date->setFixedHeight(UiConstants::kFieldHeight);

    m_time = new QTimeEdit(QTime(0, 0, 0));
    m_time->setDisplayFormat(QStringLiteral("HH:mm:ss"));
    m_time->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_time->setFixedHeight(UiConstants::kFieldHeight);

    auto* dtWidget = new QWidget;
    auto* dtLayout = new QHBoxLayout(dtWidget);
    dtLayout->setContentsMargins(0, 0, 0, 0);
    dtLayout->setSpacing(4);
    dtLayout->addWidget(m_date, 1);
    dtLayout->addWidget(m_time, 1);
    addRow(grid, row, tr("Datum / Uhrzeit:"), dtWidget);

    // ── Provision ─────────────────────────────────────────────────────────
    m_provision = new QLineEdit(QStringLiteral("0,00"));
    m_provision->setAlignment(Qt::AlignRight);
    m_provision->setValidator(makeNumericValidator(0.0, 9999999.0, 2, m_provision));
    m_provision->setFixedHeight(UiConstants::kFieldHeight);
    addRow(grid, row, tr("Provision:"), m_provision, tr("€"));

    // ── Courtage ──────────────────────────────────────────────────────────
    m_brokerFee = new QLineEdit(QStringLiteral("0,00"));
    m_brokerFee->setAlignment(Qt::AlignRight);
    m_brokerFee->setValidator(makeNumericValidator(0.0, 9999999.0, 2, m_brokerFee));
    m_brokerFee->setFixedHeight(UiConstants::kFieldHeight);
    addRow(grid, row, tr("Courtage:"), m_brokerFee, tr("€"));

    // ── Handelsplatzgebühr ────────────────────────────────────────────────
    m_traderFee = new QLineEdit(QStringLiteral("0,00"));
    m_traderFee->setAlignment(Qt::AlignRight);
    m_traderFee->setValidator(makeNumericValidator(0.0, 9999999.0, 2, m_traderFee));
    m_traderFee->setFixedHeight(UiConstants::kFieldHeight);
    addRow(grid, row, tr("Handelsplatzgebühr:"), m_traderFee, tr("€"));

    // ── Ges. Gebühren (read-only) ─────────────────────────────────────────
    m_gesGebuehren = new QLineEdit(QStringLiteral("0,00"));
    m_gesGebuehren->setReadOnly(true);
    m_gesGebuehren->setAlignment(Qt::AlignRight);
    m_gesGebuehren->setFixedHeight(UiConstants::kFieldHeight);
    m_gesGebuehren->setStyleSheet(
        QStringLiteral("QLineEdit { background: palette(midlight); }"));
    addRow(grid, row, tr("Ges. Gebühren:"), m_gesGebuehren, tr("€"));

    // ── Rabatt ────────────────────────────────────────────────────────────
    m_reduction = new QLineEdit(QStringLiteral("0,00"));
    m_reduction->setAlignment(Qt::AlignRight);
    m_reduction->setValidator(makeNumericValidator(0.0, 9999999.0, 2, m_reduction));
    m_reduction->setFixedHeight(UiConstants::kFieldHeight);
    addRow(grid, row, tr("Rabatt:"), m_reduction, tr("€"));

    // ── Netto-Kosten (read-only, coloured) ────────────────────────────────
    m_nettoKosten = new QLineEdit(QStringLiteral("0,00"));
    m_nettoKosten->setReadOnly(true);
    m_nettoKosten->setAlignment(Qt::AlignRight);
    m_nettoKosten->setFixedHeight(UiConstants::kFieldHeight);
    m_nettoKosten->setStyleSheet(
        QStringLiteral("QLineEdit { background: #d4edda; color: #155724; font-weight: bold; }"));
    addRow(grid, row, tr("Netto-Kosten:"), m_nettoKosten, tr("€"));

    return m_kostendatenGroup;
}

// ── createDocumentGroup ───────────────────────────────────────────────────────

QGroupBox* ViewBrokerageEdit::createDocumentGroup()
{
    auto* gb     = new QGroupBox(tr("  Dokument"));
    auto* layout = new QHBoxLayout(gb);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);

    m_documentPath = new QLineEdit;
    m_documentPath->setReadOnly(true);
    m_documentPath->setPlaceholderText(tr("Kein Dokument ausgewählt"));
    m_documentPath->setFixedHeight(UiConstants::kFieldHeight);

    m_btnBrowse = new QPushButton(
        IconProvider::icon(IconProvider::MenuFolderOpen16), QString());
    m_btnBrowse->setFixedSize(UiConstants::kFieldHeight, UiConstants::kFieldHeight);
    m_btnBrowse->setToolTip(tr("Dokument auswählen"));
    connect(m_btnBrowse, &QPushButton::clicked, this, &ViewBrokerageEdit::onBrowseDocument);

    layout->addWidget(m_documentPath, 1);
    layout->addWidget(m_btnBrowse);
    return gb;
}

// ── createButtonBar ───────────────────────────────────────────────────────────

QWidget* ViewBrokerageEdit::createButtonBar()
{
    auto* bar    = new QWidget;
    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    m_btnAdd    = new QPushButton(IconProvider::icon(IconProvider::ButtonAdd),
                                  tr("Hinzufügen"));
    m_btnRemove = new QPushButton(IconProvider::icon(IconProvider::ButtonDelete),
                                  tr("Entfernen"));
    m_btnReset  = new QPushButton(IconProvider::icon(IconProvider::ButtonReset),
                                  tr("Reset"));
    m_btnClose  = new QPushButton(IconProvider::icon(IconProvider::ButtonBack),
                                  tr("Schließen"));

    for (auto* btn : { m_btnAdd, m_btnRemove, m_btnReset, m_btnClose })
        btn->setFixedHeight(UiConstants::kButtonHeight);

    m_btnRemove->setEnabled(false);

    layout->addStretch(1);
    layout->addWidget(m_btnAdd);
    layout->addWidget(m_btnRemove);
    layout->addWidget(m_btnReset);
    layout->addWidget(m_btnClose);
    return bar;
}

// ── createPreviewPanel ────────────────────────────────────────────────────────

QWidget* ViewBrokerageEdit::createPreviewPanel()
{
    m_previewPanel = new DocumentPreviewPanel(this);
    return m_previewPanel;
}

// ── createOverviewGroup ───────────────────────────────────────────────────────

QGroupBox* ViewBrokerageEdit::createOverviewGroup()
{
    auto* gb     = new QGroupBox(tr("  Kosten-Übersicht"));
    gb->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    auto* layout = new QVBoxLayout(gb);
    layout->setContentsMargins(4, 4, 4, 4);

    m_overviewTabs = new OverviewTabWidget();
    m_overviewTabs->setMinimumHeight(140);
    layout->addWidget(m_overviewTabs);

    // Zeilenklick in einem Jahres-Tab → Kosteneintrag laden. GUID kommt
    // direkt aus OverviewTabWidget::rowActivated(), kein eigener Slot mehr nötig.
    connect(m_overviewTabs, &OverviewTabWidget::rowActivated,
            this, [this](const QVariant& userData) {
                const QString guid = userData.toString();
                if (!guid.isEmpty() && m_presenter)
                    m_presenter->onRowSelected(guid);
            });

    // Tab-Wechsel (Übersicht ↔ Jahr — egal ob per Reiter-Klick oder per
    // Zeilenklick in der Übersicht ausgelöst, beides läuft intern über
    // OverviewTabWidget::setCurrentIndex()): identisches Verhalten zum
    // bisherigen QTabWidget::currentChanged — Übersicht → Formular
    // zurücksetzen, Jahres-Tab → erste Zeile automatisch laden. Selektion
    // in allen Tabellen wird von OverviewTabWidget selbst geleert.
    connect(m_overviewTabs, &OverviewTabWidget::currentTabChanged,
            this, [this](int newIndex) {
                if (m_suppressTabSignal)
                    return;

                if (newIndex == 0) {
                    if (m_presenter) m_presenter->onReset();
                    return;
                }

                auto* container = m_overviewTabs->widget(newIndex);
                if (!container) return;
                auto* tbl = qobject_cast<QTableWidget*>(
                    container->property("dataTable").value<QObject*>());
                if (!tbl || tbl->rowCount() == 0) return;
                tbl->selectRow(0);
                auto* item = tbl->item(0, 0);
                if (!item) return;
                const QString guid = item->data(Qt::UserRole).toString();
                if (!guid.isEmpty() && m_presenter)
                    m_presenter->onRowSelected(guid);
            });

    // Doppelklick auf die Dokument-Spalte einer Jahres-Tab-Zeile →
    // eingebettete Vorschau aktualisieren (neu, analog Buy/Sale/Dividend).
    connect(m_overviewTabs, &OverviewTabWidget::documentActivated,
            m_previewPanel, &DocumentPreviewPanel::showDocument);

    return gb;
}

// ── addRow ────────────────────────────────────────────────────────────────────

QLabel* ViewBrokerageEdit::addRow(QGridLayout* grid, int& row,
                                   const QString& labelText,
                                   QWidget* field,
                                   const QString& unitText)
{
    auto* lbl = new QLabel(labelText);
    lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(lbl, row, 0);

    if (unitText.isEmpty()) {
        grid->addWidget(field, row, 1, 1, 2);
    } else {
        grid->addWidget(field, row, 1);
        auto* unit = new QLabel(unitText);
        unit->setFixedWidth(28);
        grid->addWidget(unit, row, 2);
    }
    ++row;
    return lbl;
}

// ── IViewBrokerageEdit: read accessors ───────────────────────────────────────

QString ViewBrokerageEdit::dateTime() const
{
    const QDate d = m_date->date();
    const QTime t = m_time->time();
    return QDateTime(d, t).toString(Qt::ISODate);
}

double  ViewBrokerageEdit::provision()    const { return parseDouble(m_provision->text()); }
double  ViewBrokerageEdit::brokerFee()    const { return parseDouble(m_brokerFee->text()); }
double  ViewBrokerageEdit::traderFee()    const { return parseDouble(m_traderFee->text()); }
double  ViewBrokerageEdit::reduction()    const { return parseDouble(m_reduction->text()); }
QString ViewBrokerageEdit::documentPath() const { return m_documentPath->text(); }

// ── IViewBrokerageEdit: write methods ────────────────────────────────────────

void ViewBrokerageEdit::loadBrokerage(const BrokerageObject& brokerage)
{
    const QDateTime dt = QDateTime::fromString(brokerage.dateTime(), Qt::ISODate);
    if (dt.isValid()) {
        m_date->blockSignals(true);
        m_date->setDate(dt.date());
        m_date->blockSignals(false);
        m_time->setTime(dt.time());
    }

    // Editierbare Felder ohne Tausendertrennzeichen (08.09.2026): der
    // Validator laesst dort keines mehr zu, der von der Anwendung
    // geschriebene Text muss also derselbe sein, den der Benutzer selbst
    // eintippen koennte. Siehe NumericFieldValidator.h.
    const auto setMoney = [](QLineEdit* le, double v) {
        le->setText(ValueFormatter::formatForInput(v, 2));
    };
    setMoney(m_provision, brokerage.provision());
    setMoney(m_brokerFee, brokerage.brokerFee());
    setMoney(m_traderFee, brokerage.traderFee());
    setMoney(m_reduction, brokerage.reduction());

    m_documentPath->setText(brokerage.document());
}

void ViewBrokerageEdit::clearForm()
{
    m_date->blockSignals(true);
    m_date->setDate(QDate::currentDate());
    m_date->blockSignals(false);
    m_time->setTime(QTime(0, 0, 0));

    const QString zero = QStringLiteral("0,00");
    m_provision->setText(zero);
    m_brokerFee->setText(zero);
    m_traderFee->setText(zero);
    m_reduction->setText(zero);
    m_gesGebuehren->setText(zero);
    m_nettoKosten->setText(zero);
    m_nettoKosten->setStyleSheet(
        QStringLiteral("QLineEdit { background: #d4edda; color: #155724; font-weight: bold; }"));

    m_documentPath->clear();
    m_fieldStates.clear();

    // Restore editable state in case readOnly was active
    for (QLineEdit* le : { m_provision, m_brokerFee, m_traderFee, m_reduction })
        le->setEnabled(true);
    m_date->setEnabled(true);
    m_time->setEnabled(true);
    m_btnBrowse->setEnabled(true);
    m_btnAdd->setEnabled(true);

    m_kostendatenGroup->setTitle(tr("  Kosten hinzufügen"));
}

void ViewBrokerageEdit::setGesamtGebuehren(double value)
{
    m_gesGebuehren->setText(ValueFormatter::formatMoney(value));
}

void ViewBrokerageEdit::setBrokerageReduction(double value)
{
    m_nettoKosten->setText(ValueFormatter::formatMoney(value));
    const QString style = value > 0.0
        ? QStringLiteral("QLineEdit { background: #d4edda; color: #155724; font-weight: bold; }")
        : QStringLiteral("QLineEdit { background: #f8d7da; color: #721c24; font-weight: bold; }");
    m_nettoKosten->setStyleSheet(style);
}

void ViewBrokerageEdit::setDocumentPreview(const QString& text)
{
    m_documentPath->setText(text);
}

// ── PDF preview ───────────────────────────────────────────────────────────────

void ViewBrokerageEdit::openPdfPreview(const QString& pdfPath)
{
    m_previewPanel->showDocument(pdfPath);
}

void ViewBrokerageEdit::clearPdfPreview()
{
    m_previewPanel->clearDocument();
}

// ── populateOverview ──────────────────────────────────────────────────────────

void ViewBrokerageEdit::populateOverview(const QList<BrokerageObject>& brokerages)
{
    // Seit v1.31.2 gemeinsam mit dem Aktiendetails-Dialog (Tab "Kosten") über
    // OverviewTabFiller (Refactor 19.11.2026). Dabei angeglichen: Beträge der
    // Jahres-Tabs mit " €", "-" in der Dokument-Spalte der Fußzeile; leere
    // Liste: Übersicht mit Hinweis "Keine Kosten vorhanden.".
    OverviewTabFiller::fillBrokerages(m_overviewTabs, brokerages);
}

// ── showOverviewTab ───────────────────────────────────────────────────────────

void ViewBrokerageEdit::showOverviewTab()
{
    if (m_overviewTabs && m_overviewTabs->currentIndex() != 0) {
        m_suppressTabSignal = true;
        m_overviewTabs->setCurrentIndex(0);
        m_suppressTabSignal = false;
    }
    clearForm();
}

// ── setButtonStates ───────────────────────────────────────────────────────────

void ViewBrokerageEdit::setButtonStates(bool canRemove, bool isEdit, bool readOnly)
{
    m_btnRemove->setEnabled(canRemove);
    m_btnAdd->setEnabled(!readOnly);

    if (isEdit) {
        m_btnAdd->setText(tr("Speichern"));
        m_btnAdd->setIcon(IconProvider::icon(IconProvider::ButtonSave));
    } else {
        m_btnAdd->setText(tr("Hinzufügen"));
        m_btnAdd->setIcon(IconProvider::icon(IconProvider::ButtonAdd));
    }

    m_kostendatenGroup->setTitle(isEdit ? tr("  Kosten editieren")
                                        : tr("  Kosten hinzufügen"));

    for (QLineEdit* le : { m_provision, m_brokerFee, m_traderFee, m_reduction })
        le->setEnabled(!readOnly);
    m_date->setEnabled(!readOnly);
    m_time->setEnabled(!readOnly);
    m_btnBrowse->setEnabled(!readOnly);
}

// ── showError / acceptAndClose ────────────────────────────────────────────────

void ViewBrokerageEdit::showError(const QString& message)
{
    OwnMessageBox::critical(this, tr("Fehler"), message);
}

void ViewBrokerageEdit::acceptAndClose()
{
    accept();
}

// ── markMissingFieldsAsFailed / hasMissingRequiredFields ─────────────────────

void ViewBrokerageEdit::markMissingFieldsAsFailed()
{
    // Only the date can be missing (fee fields default to 0 which is valid).
}

bool ViewBrokerageEdit::hasMissingRequiredFields(QStringList& missingFields) const
{
    missingFields.clear();
    if (m_date->date() <= QDate(2000, 1, 1))
        missingFields << QStringLiteral("date");
    return !missingFields.isEmpty();
}

// ── Private slots ─────────────────────────────────────────────────────────────

void ViewBrokerageEdit::onBrowseDocument()
{
    const QString root = AppSettings::instance().documentsRootPath();
    const QString startDir = !root.isEmpty() ? root : QDir::homePath();

    // Nur PDF zulässig (Nessies Entscheidung, 20.07.2026 — siehe
    // ARCHITECTURE.md, "Offene Punkte / TODO": Word-/Excel-Unterstützung
    // bewusst nicht wieder eingebaut).
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

// ── Static helpers ────────────────────────────────────────────────────────────

// ── hasUnreadableFields ───────────────────────────────────────────────────────

bool ViewBrokerageEdit::hasUnreadableFields(QStringList& fieldNames) const
{
    fieldNames.clear();

    // ANZEIGENAMEN, nicht Feldschluessel — dieser Dialog hat keine
    // Feldanzeige, die Meldung des Presenters muss die Felder selbst
    // benennen. Die Namen stehen hier, weil sie hier auch als
    // Feldbeschriftung entstehen; eine zweite Tabelle im Presenter waere
    // eine zweite Quelle fuer dieselben Bezeichner (09.09.2026).
    const QList<QPair<QString, const QLineEdit*>> numericFields = {
        { tr("Provision"), m_provision },
        { tr("Courtage"), m_brokerFee },
        { tr("Handelsplatzgebühr"), m_traderFee },
        { tr("Rabatt"), m_reduction },
    };

    for (const auto& [name, edit] : numericFields) {
        if (!edit)
            continue;
        bool ok = false;
        parseDouble(edit->text(), &ok);
        if (!ok)
            fieldNames.append(name);
    }

    return !fieldNames.isEmpty();
}

double ViewBrokerageEdit::parseDouble(const QString& text, bool* ok)
{
    // Delegiert seit 06.09.2026 an NumberParser. Vorher stand hier eine
    // eigene Umwandlung, die das Komma durch einen Punkt ersetzte und ein
    // Tausendertrennzeichen stehen liess — aus "1.003,00" wurde "1.003.00",
    // also keine Zahl mehr, und das Feld fiel lautlos auf 0,0. Siehe
    // ARCHITECTURE.md, "Zahlenfelder verlieren Werte ab 1.000 beim
    // Zuruecklesen".
    return NumberParser::parse(text, ok);
}


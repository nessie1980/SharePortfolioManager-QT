// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ViewBuyEdit.h"
#include "PresenterBuyEdit.h"
#include "ModelBuyEdit.h"
#include "../../IconProvider.h"
#include "../../config/AppSettings.h"
#include "../../core/DocumentRootMigrator.h"
#include "../../config/DocumentsConfig.h"
#include "../UiConstants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFrame>
#include <QFileDialog>
#include "../OwnMessageBoxForm/OwnMessageBox.h"
#include <QLocale>
#include <QSizePolicy>
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QDoubleValidator>
#include <QApplication>
#include <functional>

#ifndef SPM_HAVE_QTPDF
#  include <QPixmap>
#  include <QProcess>
#endif

// ── Constructor ───────────────────────────────────────────────────────────────

ViewBuyEdit::ViewBuyEdit(const QString& shareGuid,
                         DocumentsConfig* config,
                         QWidget* parent)
    : QDialog(parent)
    , m_config(config)
{
    setWindowTitle(tr("Hinzufügen / editieren der Käufe dieser Aktie"));
    setFixedSize(1300, 790);

    // Build UI first — presenter writes into widgets in its constructor.
    setupUi();

    auto* model = new ModelBuyEdit();
    m_presenter = new PresenterBuyEdit(this, model, shareGuid, config, this);

    connect(m_btnAdd,    &QPushButton::clicked, m_presenter, &PresenterBuyEdit::onSave);
    connect(m_btnRemove, &QPushButton::clicked, m_presenter, &PresenterBuyEdit::onRemove);
    connect(m_btnReset,  &QPushButton::clicked, m_presenter, &PresenterBuyEdit::onReset);
    connect(m_btnClose,  &QPushButton::clicked, m_presenter, &PresenterBuyEdit::onClose);

    // Forward numeric text changes so derived fields stay current.
    auto fwd = [this](const QString&) { m_presenter->onValuesChanged(); };
    connect(m_volume,    &QLineEdit::textChanged, this, fwd);
    connect(m_price,     &QLineEdit::textChanged, this, fwd);
    connect(m_provision, &QLineEdit::textChanged, this, fwd);
    connect(m_brokerFee, &QLineEdit::textChanged, this, fwd);
    connect(m_traderFee, &QLineEdit::textChanged, this, fwd);
    connect(m_reduction, &QLineEdit::textChanged, this, fwd);

    // ── Live field validation ─────────────────────────────────────────────
    // Use editingFinished for date — dateChanged fires on programmatic setDate() too.
    connect(m_date, &QDateEdit::editingFinished,
            m_presenter, &PresenterBuyEdit::onDateEdited);
    // activated() fires only on user interaction, not programmatic setCurrentIndex().
    connect(m_depotNumber, QOverload<int>::of(&QComboBox::activated),
            m_presenter, &PresenterBuyEdit::onDepotNumberEdited);
    connect(m_orderNumber, &QLineEdit::editingFinished,
            m_presenter, &PresenterBuyEdit::onOrderNumberEdited);
    connect(m_volume, &QLineEdit::editingFinished,
            m_presenter, &PresenterBuyEdit::onVolumeOrPriceEdited);
    connect(m_price, &QLineEdit::editingFinished,
            m_presenter, &PresenterBuyEdit::onVolumeOrPriceEdited);

    // Optional fee fields — use lambda to pass fieldKey + value
    auto connectFee = [this](QLineEdit* le, const QString& key) {
        connect(le, &QLineEdit::editingFinished, m_presenter,
                [this, le, key]() {
                    m_presenter->onFeeEdited(key, parseDouble(le->text()));
                });
    };
    connectFee(m_provision, QStringLiteral("provision"));
    connectFee(m_brokerFee, QStringLiteral("brokerFee"));
    connectFee(m_traderFee, QStringLiteral("traderFee"));
    connectFee(m_reduction, QStringLiteral("reduction"));

    // Document path validation is triggered explicitly by onBrowseDocument()
    // and by onRowSelected() — NOT via textChanged, which would fire during
    // loadBuy() and onDocumentSelected() with incomplete state.
}

// ── setupUi ───────────────────────────────────────────────────────────────────

void ViewBuyEdit::setupUi()
{
    auto* main = new QHBoxLayout(this);
    main->setContentsMargins(6, 6, 6, 6);
    main->setSpacing(8);

    // Dokumenten-Vorschau zuerst erzeugen (aber erst unten ins Layout
    // einfügen) — createOverviewGroup() verbindet OverviewTabWidget::
    // documentActivated mit m_previewPanel und braucht dafür ein bereits
    // existierendes Objekt (Bugfix 16.07.2026, s. ARCHITECTURE.md).
    auto* previewPanel = createPreviewPanel();

    // ── Left column ───────────────────────────────────────────────────────
    m_leftPanel  = new QWidget;
    auto* leftLayout = new QVBoxLayout(m_leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(6);
    leftLayout->addWidget(createKaufdatenGroup(),  0);
    leftLayout->addWidget(createDocumentGroup(),   0);
    leftLayout->addWidget(createButtonBar(),       0);
    leftLayout->addWidget(createOverviewGroup(),   1);
    m_leftPanel->setMinimumWidth(480);

    main->addWidget(m_leftPanel,  3);
    main->addWidget(previewPanel, 2);
}

// ── createKaufdatenGroup ──────────────────────────────────────────────────────

QGroupBox* ViewBuyEdit::createKaufdatenGroup()
{
    auto* gb   = new QGroupBox(tr("  Kaufdaten"));
    gb->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    auto* grid = new QGridLayout(gb);
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

    m_time = new QTimeEdit(QTime::currentTime());
    m_time->setDisplayFormat(QStringLiteral("HH:mm:ss"));
    m_time->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_time->setFixedHeight(UiConstants::kFieldHeight);

    auto* dtWidget = new QWidget;
    auto* dtLayout = new QHBoxLayout(dtWidget);
    dtLayout->setContentsMargins(0, 0, 0, 0);
    dtLayout->setSpacing(4);
    dtLayout->addWidget(m_date, 1);
    dtLayout->addWidget(m_time, 1);
    m_statusLabels[QStringLiteral("date")] =
        addRow(grid, row, tr("Datum:"), dtWidget, QString(), QStringLiteral("date"));
    m_inputWidgets[QStringLiteral("date")] = m_date;
    m_inputWidgets[QStringLiteral("time")] = m_time;

    // ── Depotnummer ───────────────────────────────────────────────────────
    m_depotNumber = new QComboBox;
    m_depotNumber->setEditable(false);  // Only known depot numbers from Documents.xml
    m_depotNumber->addItem(tr("— bitte wählen —"));
    // Populate from Documents.xml BankIdentifierValues — same as ViewShareAdd
    if (m_config) {
        for (const auto& bank : m_config->entries()) {
            if (!bank.identifier.isEmpty())
                m_depotNumber->addItem(
                    QStringLiteral("%1 (%2)").arg(bank.name, bank.identifier),
                    bank.identifier);
        }
    }
    m_statusLabels[QStringLiteral("depotNumber")] =
        addRow(grid, row, tr("Depotnummer:"), m_depotNumber,
               QString(), QStringLiteral("depotNumber"));
    m_inputWidgets[QStringLiteral("depotNumber")] = m_depotNumber;

    // ── Ordernummer ───────────────────────────────────────────────────────
    m_orderNumber = new QLineEdit;
    m_orderNumber->setMaxLength(100);
    m_statusLabels[QStringLiteral("orderNumber")] =
        addRow(grid, row, tr("Ordernummer:"), m_orderNumber,
               QString(), QStringLiteral("orderNumber"));
    m_inputWidgets[QStringLiteral("orderNumber")] = m_orderNumber;

    // ── Gekaufte Anteile ──────────────────────────────────────────────────
    m_volume = new QLineEdit(QStringLiteral("0,0000"));
    m_volume->setAlignment(Qt::AlignRight);
    m_volume->setValidator(new QDoubleValidator(0.0, 9'999'999.0, 4, m_volume));
    m_statusLabels[QStringLiteral("volume")] =
        addRow(grid, row, tr("Gekaufte Anteile:"), m_volume,
               tr("stk."), QStringLiteral("volume"));
    m_inputWidgets[QStringLiteral("volume")] = m_volume;

    // ── Bereits verkaufte Anteile (read-only) ─────────────────────────────
    m_volumeSold = new QLineEdit(QStringLiteral("0,0000"));
    m_volumeSold->setReadOnly(true);
    m_volumeSold->setStyleSheet(QStringLiteral("background: palette(midlight);"));
    m_volumeSold->setAlignment(Qt::AlignRight);
    addRow(grid, row, tr("Bereits verkaufte Anteile:"), m_volumeSold, tr("stk."));

    // ── Kurs ──────────────────────────────────────────────────────────────
    m_price = new QLineEdit(QStringLiteral("0,0000"));
    m_price->setAlignment(Qt::AlignRight);
    m_price->setValidator(new QDoubleValidator(0.0, 9'999'999.0, 4, m_price));
    m_statusLabels[QStringLiteral("price")] =
        addRow(grid, row, tr("Kurs:"), m_price,
               tr("€"), QStringLiteral("price"));
    m_inputWidgets[QStringLiteral("price")] = m_price;

    // ── Kurswert (read-only) ──────────────────────────────────────────────
    m_kurswert = new QLineEdit(QStringLiteral("0,00"));
    m_kurswert->setReadOnly(true);
    m_kurswert->setStyleSheet(QStringLiteral("background: palette(midlight);"));
    m_kurswert->setAlignment(Qt::AlignRight);
    addRow(grid, row, tr("Kurswert:"), m_kurswert, tr("€"));

    // ── Provision ─────────────────────────────────────────────────────────
    m_provision = new QLineEdit(QStringLiteral("0,00"));
    m_provision->setAlignment(Qt::AlignRight);
    m_provision->setValidator(new QDoubleValidator(0.0, 99'999.0, 2, m_provision));
    m_statusLabels[QStringLiteral("provision")] =
        addRow(grid, row, tr("Provision:"), m_provision,
               tr("€"), QStringLiteral("provision"));
    m_inputWidgets[QStringLiteral("provision")] = m_provision;

    // ── Courtage ──────────────────────────────────────────────────────────
    m_brokerFee = new QLineEdit(QStringLiteral("0,00"));
    m_brokerFee->setAlignment(Qt::AlignRight);
    m_brokerFee->setValidator(new QDoubleValidator(0.0, 99'999.0, 2, m_brokerFee));
    m_statusLabels[QStringLiteral("brokerFee")] =
        addRow(grid, row, tr("Courtage:"), m_brokerFee,
               tr("€"), QStringLiteral("brokerFee"));
    m_inputWidgets[QStringLiteral("brokerFee")] = m_brokerFee;

    // ── Handelsplatzgebühr ────────────────────────────────────────────────
    m_traderFee = new QLineEdit(QStringLiteral("0,00"));
    m_traderFee->setAlignment(Qt::AlignRight);
    m_traderFee->setValidator(new QDoubleValidator(0.0, 99'999.0, 2, m_traderFee));
    m_statusLabels[QStringLiteral("traderFee")] =
        addRow(grid, row, tr("Handelsplatzgebühr:"), m_traderFee,
               tr("€"), QStringLiteral("traderFee"));
    m_inputWidgets[QStringLiteral("traderFee")] = m_traderFee;

    // ── Ges. Gebühren (read-only) ─────────────────────────────────────────
    m_gesGebuehren = new QLineEdit(QStringLiteral("0,00"));
    m_gesGebuehren->setReadOnly(true);
    m_gesGebuehren->setStyleSheet(QStringLiteral("background: palette(midlight);"));
    m_gesGebuehren->setAlignment(Qt::AlignRight);
    addRow(grid, row, tr("Ges. Gebühren:"), m_gesGebuehren, tr("€"));

    // ── Rabatt ────────────────────────────────────────────────────────────
    m_reduction = new QLineEdit(QStringLiteral("0,00"));
    m_reduction->setAlignment(Qt::AlignRight);
    m_reduction->setValidator(new QDoubleValidator(0.0, 99'999.0, 2, m_reduction));
    m_statusLabels[QStringLiteral("reduction")] =
        addRow(grid, row, tr("Rabatt:"), m_reduction,
               tr("€"), QStringLiteral("reduction"));
    m_inputWidgets[QStringLiteral("reduction")] = m_reduction;

    // ── Endbetrag (read-only, green) ──────────────────────────────────────
    m_endbetrag = new QLineEdit(QStringLiteral("0,00"));
    m_endbetrag->setReadOnly(true);
    m_endbetrag->setStyleSheet(
        QStringLiteral("background: #d4edda; color: #155724; font-weight: bold;"));
    m_endbetrag->setAlignment(Qt::AlignRight);
    addRow(grid, row, tr("Endbetrag:"), m_endbetrag, tr("€"));

    return gb;
}

// ── createDocumentGroup ───────────────────────────────────────────────────────
// Identical structure to ViewShareAdd::createDocumentGroup()

QGroupBox* ViewBuyEdit::createDocumentGroup()
{
    auto* gb   = new QGroupBox(tr("  Dokument"));
    gb->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    auto* grid = new QGridLayout(gb);
    grid->setColumnStretch(1, 1);
    grid->setVerticalSpacing(4);
    grid->setHorizontalSpacing(8);
    grid->setContentsMargins(8, 8, 8, 10);
    int row = 0;

    m_documentPath = new QLineEdit;
    m_documentPath->setReadOnly(true);
    m_documentPath->setPlaceholderText(tr("Kein Dokument ausgewählt …"));
    m_btnBrowse = new QPushButton(IconProvider::icon(IconProvider::MenuFolderOpen16), QString());
    m_btnBrowse->setFixedWidth(36);
    m_btnBrowse->setFixedHeight(UiConstants::kFieldHeight);
    m_btnBrowse->setToolTip(tr("PDF-Dokument auswählen"));
    connect(m_btnBrowse, &QPushButton::clicked, this, &ViewBuyEdit::onBrowseDocument);

    auto* docRow = new QWidget;
    auto* docL   = new QHBoxLayout(docRow);
    docL->setContentsMargins(0, 0, 0, 0);
    docL->setSpacing(4);
    docL->addWidget(m_documentPath, 1);
    docL->addWidget(m_btnBrowse);

    auto* label = new QLabel(tr("Dokument:"));
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(label,  row, 0);
    grid->addWidget(docRow, row, 1);

    // Status icon for document (duplicate detection)
    auto* docStatus = new QLabel;
    docStatus->setFixedSize(20, 20);
    docStatus->setAlignment(Qt::AlignCenter);
    grid->addWidget(docStatus, row, 2);
    m_statusLabels[QStringLiteral("document")] = docStatus;

    ++row;

    // ── Parse status bar ──────────────────────────────────────────────────
    // statusRow has a fixed height — space is ALWAYS reserved regardless of
    // child visibility. Child widgets start visually empty (no content shown).
    m_parseProgress = new QProgressBar;
    m_parseProgress->setRange(0, 100);
    m_parseProgress->setValue(0);
    m_parseProgress->setFixedHeight(14);
    m_parseProgress->setFixedWidth(200);
    m_parseProgress->setTextVisible(false);
    m_parseProgress->setStyleSheet(
        QStringLiteral("QProgressBar { background: transparent; border: none; }"));

    m_parseStatusIcon = new QLabel;
    m_parseStatusIcon->setFixedSize(18, 18);
    m_parseStatusIcon->setAlignment(Qt::AlignCenter);

    m_parseStatus = new QLabel;
    m_parseStatus->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* statusRow    = new QWidget;
    statusRow->setFixedHeight(22);
    auto* statusLayout = new QHBoxLayout(statusRow);
    statusLayout->setContentsMargins(0, 2, 0, 0);
    statusLayout->setSpacing(6);
    statusLayout->addWidget(m_parseProgress);
    statusLayout->addWidget(m_parseStatusIcon);
    statusLayout->addWidget(m_parseStatus, 1);

    grid->addWidget(statusRow, row, 0, 1, 4);

    return gb;
}

// ── createPreviewPanel ────────────────────────────────────────────────────────

QWidget* ViewBuyEdit::createPreviewPanel()
{
    m_previewPanel = new DocumentPreviewPanel(this);
    return m_previewPanel;
}

// ── createButtonBar ───────────────────────────────────────────────────────────

QWidget* ViewBuyEdit::createButtonBar()
{
    auto* bar    = new QWidget;
    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(0, 4, 0, 0);
    layout->setSpacing(6);

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

    layout->addStretch(1);        // pushes all buttons to the right
    layout->addWidget(m_btnAdd);
    layout->addWidget(m_btnRemove);
    layout->addWidget(m_btnReset);
    layout->addWidget(m_btnClose);

    return bar;
}

// ── createOverviewGroup ───────────────────────────────────────────────────────

QGroupBox* ViewBuyEdit::createOverviewGroup()
{
    auto* gb     = new QGroupBox(tr("  Kauf-Übersicht"));
    auto* layout = new QVBoxLayout(gb);
    layout->setContentsMargins(6, 6, 6, 6);

    m_overviewTabs = new OverviewTabWidget();
    m_overviewTabs->setMinimumHeight(140);
    layout->addWidget(m_overviewTabs);

    // Zeilenklick in einem Jahres-Tab → Kauf laden. GUID kommt direkt aus
    // OverviewTabWidget::rowActivated(), kein eigener Slot mehr nötig.
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
    // zurücksetzen, Jahres-Tab → erste Zeile automatisch laden.
    // Selektion in allen Tabellen wird von OverviewTabWidget selbst geleert.
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
    // eingebettete Vorschau aktualisieren (neu, analog ShareDetailsForm).
    connect(m_overviewTabs, &OverviewTabWidget::documentActivated,
            m_previewPanel, &DocumentPreviewPanel::showDocument);

    return gb;
}

// ── IViewBuyEdit — read accessors ─────────────────────────────────────────────

QString ViewBuyEdit::dateTime() const
{
    return QDateTime(m_date->date(), m_time->time()).toString(Qt::ISODate);
}

QString ViewBuyEdit::depotNumber() const
{
    // Return the raw BankIdentifierValue stored as item data — same as ViewShareAdd
    const QVariant data = m_depotNumber->currentData();
    return data.isValid() ? data.toString() : QString();
}

QString ViewBuyEdit::orderNumber()  const { return m_orderNumber->text(); }
QString ViewBuyEdit::documentPath() const { return m_documentPath->text(); }

double ViewBuyEdit::volume()    const { return parseDouble(m_volume->text());    }
double ViewBuyEdit::price()     const { return parseDouble(m_price->text());     }
double ViewBuyEdit::provision() const { return parseDouble(m_provision->text()); }
double ViewBuyEdit::brokerFee() const { return parseDouble(m_brokerFee->text()); }
double ViewBuyEdit::traderFee() const { return parseDouble(m_traderFee->text()); }
double ViewBuyEdit::reduction() const { return parseDouble(m_reduction->text()); }

// ── IViewBuyEdit — write methods ──────────────────────────────────────────────

void ViewBuyEdit::loadBuy(const BuyObject& buy, const BrokerageObject& brokerage)
{
    const QDateTime dt = QDateTime::fromString(buy.dateTime(), Qt::ISODate);
    m_date->setDate(dt.isValid() ? dt.date() : QDate::currentDate());
    m_time->setTime(dt.isValid() ? dt.time() : QTime::currentTime());

    // Depot number — match by stored item data (trimmed comparison).
    const QString depotNr = buy.depotNumber().trimmed();
    bool matched = false;
    {
        QSignalBlocker block(m_depotNumber);
        for (int i = 0; i < m_depotNumber->count(); ++i) {
            if (m_depotNumber->itemData(i).toString().trimmed() == depotNr) {
                m_depotNumber->setCurrentIndex(i);
                matched = true;
                break;
            }
        }
        if (!matched && !depotNr.isEmpty()) {
            m_depotNumber->addItem(depotNr, depotNr);
            m_depotNumber->setCurrentIndex(m_depotNumber->count() - 1);
            matched = true;
        }
    }
    // Icons are set by the validation slots called from onRowSelected() after loadBuy.

    m_orderNumber->setText(buy.orderNumber());
    m_volume->setText(formatVolume(buy.volume()));
    m_price->setText(formatVolume(buy.price()));
    m_documentPath->setText(buy.document());

    m_provision->setText(formatMoney(brokerage.isValid() ? brokerage.provision()  : 0.0));
    m_brokerFee->setText(formatMoney(brokerage.isValid() ? brokerage.brokerFee()  : 0.0));
    m_traderFee->setText(formatMoney(brokerage.isValid() ? brokerage.traderFee()  : 0.0));
    m_reduction->setText(formatMoney(brokerage.isValid() ? brokerage.reduction()  : 0.0));
}

void ViewBuyEdit::clearForm()
{
    m_date->setDate(QDate::currentDate());
    m_time->setTime(QTime::currentTime());
    {
        QSignalBlocker block(m_depotNumber);
        m_depotNumber->setCurrentIndex(0);
    }
    m_orderNumber->clear();
    m_volume->setText(QStringLiteral("0,0000"));
    m_price->setText(QStringLiteral("0,0000"));
    m_provision->setText(QStringLiteral("0,00"));
    m_brokerFee->setText(QStringLiteral("0,00"));
    m_traderFee->setText(QStringLiteral("0,00"));
    m_reduction->setText(QStringLiteral("0,00"));
    m_documentPath->clear();
    m_volumeSold->setText(QStringLiteral("0,0000"));

    // ── Restore all input fields to enabled state ─────────────────────────
    m_date->setEnabled(true);        m_date->setStyleSheet(QString());
    m_time->setEnabled(true);        m_time->setStyleSheet(QString());
    m_depotNumber->setEnabled(true);
    m_orderNumber->setEnabled(true); m_orderNumber->setStyleSheet(QString());
    m_volume->setEnabled(true);      m_volume->setStyleSheet(QString());
    m_price->setEnabled(true);       m_price->setStyleSheet(QString());
    m_provision->setEnabled(true);   m_provision->setStyleSheet(QString());
    m_brokerFee->setEnabled(true);   m_brokerFee->setStyleSheet(QString());
    m_traderFee->setEnabled(true);   m_traderFee->setStyleSheet(QString());
    m_reduction->setEnabled(true);   m_reduction->setStyleSheet(QString());

    // ── Reset all field status icons (same principle as ViewShareAdd) ─────
    for (auto it = m_statusLabels.begin(); it != m_statusLabels.end(); ++it) {
        if (it.value()) {
            it.value()->setPixmap(QPixmap());
            it.value()->setToolTip(QString());
        }
    }
    m_fieldStates.clear();   // all fields back to Untouched

    // ── Reset parse status bar to initial invisible state ─────────────────
    m_parseProgress->setValue(0);
    m_parseProgress->setStyleSheet(
        QStringLiteral("QProgressBar { background: transparent; border: none; }"));
    m_parseStatusIcon->setPixmap(QPixmap());
    m_parseStatus->clear();

    // ── Clear the PDF preview ─────────────────────────────────────────────
    clearPdfPreview();
}

void ViewBuyEdit::setVolumeSold(double value)
{
    m_volumeSold->setText(formatVolume(value));
}

void ViewBuyEdit::setKurswert(double value)
{
    m_kurswert->setText(formatMoney(value));
}

void ViewBuyEdit::setGesGebuehren(double value)
{
    m_gesGebuehren->setText(formatMoney(value));
}

void ViewBuyEdit::setEndbetrag(double value)
{
    m_endbetrag->setText(formatMoney(value));
}

// ── Field status (1:1 wie ViewShareAdd) ───────────────────────────────────────

void ViewBuyEdit::setFieldOk(const QString& field, const QString& value)
{
    auto* lbl = m_statusLabels.value(field);
    if (lbl) {
        m_fieldStates[field] = FieldState::Ok;
        lbl->setPixmap(IconProvider::icon(IconProvider::SearchOk).pixmap(16, 16));
        lbl->setToolTip(tr("Eingabe gültig"));
        lbl->setVisible(true);
    }

    auto* widget = m_inputWidgets.value(field);
    if (!widget) return;

    if (auto* le = qobject_cast<QLineEdit*>(widget)) {
        // Only update the widget text when a real value is provided (from parser).
        // Live validation calls setFieldOk with an empty value — don't overwrite.
        if (!value.isEmpty()) {
            QString norm = value; norm.replace(QLatin1Char('.'), QLatin1Char(','));
            le->setText(norm.trimmed());
        }
    } else if (field == QStringLiteral("depotNumber")) {
        // Only update when a real value is provided (from parser).
        if (value.isEmpty()) return;
        QSignalBlocker block(m_depotNumber);
        for (int i = 0; i < m_depotNumber->count(); ++i) {
            if (m_depotNumber->itemData(i).toString() == value.trimmed()) {
                m_depotNumber->setCurrentIndex(i);
                break;
            }
        }
    } else if (auto* cb = qobject_cast<QComboBox*>(widget)) {
        if (value.isEmpty()) return;
        bool matched = false;
        for (int i = 0; i < cb->count(); ++i) {
            if (cb->itemData(i).toString() == value ||
                cb->itemText(i) == value) {
                cb->setCurrentIndex(i);
                matched = true;
                break;
            }
        }
        if (!matched) {
            cb->addItem(value, value);
            cb->setCurrentIndex(cb->count() - 1);
        }
    } else if (auto* de = qobject_cast<QDateEdit*>(widget)) {
        QDate d = QDate::fromString(value, QStringLiteral("d.M.yyyy"));
        if (!d.isValid()) d = QDate::fromString(value, Qt::ISODate);
        if (d.isValid()) de->setDate(d);
    } else if (auto* te = qobject_cast<QTimeEdit*>(widget)) {
        // Try "h:m:s" and "h:m" — same as ViewShareAdd
        QTime t = QTime::fromString(value, QStringLiteral("h:m:s"));
        if (!t.isValid()) t = QTime::fromString(value, QStringLiteral("h:m"));
        if (t.isValid()) te->setTime(t);
    }
}

void ViewBuyEdit::setFieldError(const QString& field)
{
    auto* lbl = m_statusLabels.value(field);
    if (!lbl) return;
    m_fieldStates[field] = FieldState::Error;
    lbl->setPixmap(IconProvider::icon(IconProvider::SearchFailed).pixmap(16, 16));
    lbl->setToolTip(tr("Ungültige oder fehlende Eingabe"));
    lbl->setVisible(true);
}

void ViewBuyEdit::setDocumentPreview(const QString& /*text*/)
{
    // Plain text is only used internally by the parser.
    // Visual preview is handled by openPdfPreview() via onBrowseDocument().
}

// ── Parse status bar (1:1 wie ViewShareAdd) ───────────────────────────────────

void ViewBuyEdit::setParseProgress(int percent, const QString& status)
{
    // Restore normal stylesheet on first use (was transparent before parsing)
    m_parseProgress->setStyleSheet(QString());
    m_parseProgress->setValue(percent);
    m_parseStatus->setText(status);

    if (percent < 100)
        m_parseStatusIcon->setPixmap(
            IconProvider::icon(IconProvider::SearchInfo).pixmap(16, 16));
}

void ViewBuyEdit::setParseStatusIcon(int iconType)
{
    if (!m_parseStatusIcon) return;
    IconProvider::IconName name;
    switch (iconType) {
    case 0:  name = IconProvider::SearchOk;     break;
    case 1:  name = IconProvider::SearchFailed; break;
    default: name = IconProvider::SearchInfo;   break;
    }
    m_parseStatusIcon->setPixmap(IconProvider::icon(name).pixmap(16, 16));
}

void ViewBuyEdit::setUiBusy(bool busy)
{
    if (m_leftPanel)
        m_leftPanel->setDisabled(busy);

    m_btnAdd->setDisabled(busy);
    m_btnClose->setDisabled(busy);

    if (busy) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        m_parseProgress->setStyleSheet(QString());  // restore normal style
        m_parseStatusIcon->setPixmap(
            IconProvider::icon(IconProvider::SearchInfo).pixmap(16, 16));
    } else {
        QApplication::restoreOverrideCursor();
        m_parseProgress->setValue(100);
        if (m_leftPanel) m_leftPanel->update();
        update();
    }
}

void ViewBuyEdit::onParseFinished()
{
    // Required fields that were not touched by the parser get SearchInfo icon.
    static const QStringList requiredFieldKeys = {
        QStringLiteral("date"),
        QStringLiteral("depotNumber"),
        QStringLiteral("orderNumber"),
        QStringLiteral("volume"),
        QStringLiteral("price"),
    };

    for (const QString& field : requiredFieldKeys) {
        if (m_fieldStates.value(field, FieldState::Untouched) == FieldState::Untouched) {
            m_fieldStates[field] = FieldState::Info;
            if (auto* lbl = m_statusLabels.value(field)) {
                lbl->setPixmap(
                    IconProvider::icon(IconProvider::SearchInfo).pixmap(16, 16));
                lbl->setToolTip(tr("Wert fehlt noch — bitte manuell eingeben"));
                lbl->setVisible(true);
            }
        }
    }
}

// ── populateOverview ──────────────────────────────────────────────────────────

void ViewBuyEdit::populateOverview(const QList<BuyObject>&       buys,
                                   const QList<BrokerageObject>& brokerages)
{
    if (buys.isEmpty()) {
        m_overviewTabs->clear();
        return;
    }

    // Collect distinct years, sorted descending (newest year first).
    QList<int> years;
    for (const BuyObject& b : buys) {
        const int y = b.year();
        if (!years.contains(y))
            years.append(y);
    }
    std::sort(years.begin(), years.end(), std::greater<int>());

    // ── Übersicht-Aggregation (Jahr | Anteile | Einzahlung) ────────────────
    QMap<int, double> yearVol;
    QMap<int, double> yearVal;
    double totalVol = 0.0, totalVal = 0.0, totalEinzahlung = 0.0;
    for (int i = 0; i < buys.size(); ++i) {
        const BuyObject&       b  = buys.at(i);
        const BrokerageObject& br = brokerages.at(i);
        const double einzahlung = br.isValid()
                                      ? b.buyValue() + br.brokerageReduction()
                                      : b.buyValue();
        yearVol[b.year()] += b.volume();
        yearVal[b.year()] += einzahlung;
        totalEinzahlung   += einzahlung;
    }
    for (int y : std::as_const(years)) {
        totalVol += yearVol.value(y);
        totalVal += yearVal.value(y);
    }

    auto populateUebersichtData = [this, years, yearVol, yearVal](QTableWidget* tbl) {
        tbl->setRowCount(years.size());
        for (int i = 0; i < years.size(); ++i) {
            const int y = years.at(i);
            auto* iYear = new QTableWidgetItem(QString::number(y));
            auto* iVol  = new QTableWidgetItem(formatVolume(yearVol.value(y)) + QStringLiteral(" stk."));
            auto* iVal  = new QTableWidgetItem(formatMoney(yearVal.value(y)) + QStringLiteral(" €"));
            iYear->setTextAlignment(Qt::AlignCenter);
            iVol->setTextAlignment(Qt::AlignCenter);
            iVal->setTextAlignment(Qt::AlignCenter);
            iYear->setData(Qt::UserRole, y);
            tbl->setItem(i, 0, iYear);
            tbl->setItem(i, 1, iVol);
            tbl->setItem(i, 2, iVal);
        }
    };

    auto populateUebersichtFooter = [totalVol, totalVal](QTableWidget* f) {
        auto* iLabel = new QTableWidgetItem(tr("Gesamt:"));
        auto* iVol   = new QTableWidgetItem(formatVolume(totalVol) + QStringLiteral(" stk."));
        auto* iVal   = new QTableWidgetItem(formatMoney(totalVal) + QStringLiteral(" €"));
        iLabel->setTextAlignment(Qt::AlignCenter);
        iVol->setTextAlignment(Qt::AlignCenter);
        iVal->setTextAlignment(Qt::AlignCenter);
        f->setItem(0, 0, iLabel);
        f->setItem(0, 1, iVol);
        f->setItem(0, 2, iVal);
    };

    const QString uebersichtTitle = tr("Übersicht (%1 €)").arg(formatMoney(totalEinzahlung));

    // ── Jahres-Tabs (Datum | Anteile | Kurswert | Gebühren | Einzahlung | Dok.) ──
    constexpr int kColDate       = 0;
    constexpr int kColVolume     = 1;
    constexpr int kColKurswert   = 2;
    constexpr int kColGebuehren  = 3;
    constexpr int kColEinzahlung = 4;
    constexpr int kColDoc        = 5;

    const QStringList jahresHeaders = {
        tr("Datum"), tr("Anteile"), tr("Kurswert"),
        tr("Gebühren"), tr("Einzahlung"), QString()
    };

    auto populateJahresData = [this, buys, brokerages](int year, QTableWidget* tbl) {
        QList<BuyObject>       yearBuys;
        QList<BrokerageObject> yearBrokerages;
        for (int i = 0; i < buys.size(); ++i) {
            if (buys.at(i).year() == year) {
                yearBuys.append(buys.at(i));
                yearBrokerages.append(brokerages.at(i));
            }
        }

        tbl->setRowCount(yearBuys.size());
        for (int i = 0; i < yearBuys.size(); ++i) {
            const BuyObject&       b  = yearBuys.at(i);
            const BrokerageObject& br = yearBrokerages.at(i);

            const double kurswert   = b.price();
            const double gebuehren  = br.isValid() ? br.brokerageReduction() : 0.0;
            const double einzahlung = b.buyValue() + gebuehren;

            // ⚠️ Datumsformat bitte gegen Original prüfen.
            auto* iDate = new QTableWidgetItem(
                QDateTime::fromString(b.dateTime(), Qt::ISODate).date().toString(QStringLiteral("dd.MM.yyyy")));
            iDate->setTextAlignment(Qt::AlignCenter);
            iDate->setData(Qt::UserRole, b.guid());

            auto* iVol = new QTableWidgetItem(formatVolume(b.volume()) + QStringLiteral(" stk."));
            iVol->setTextAlignment(Qt::AlignCenter);

            auto* iKurswert = new QTableWidgetItem(formatMoney(kurswert) + QStringLiteral(" €"));
            iKurswert->setTextAlignment(Qt::AlignCenter);

            auto* iGebuehr = new QTableWidgetItem(formatMoney(gebuehren) + QStringLiteral(" €"));
            iGebuehr->setTextAlignment(Qt::AlignCenter);

            auto* iEinzahlung = new QTableWidgetItem(formatMoney(einzahlung) + QStringLiteral(" €"));
            iEinzahlung->setTextAlignment(Qt::AlignCenter);

            auto* iDoc = new QTableWidgetItem;
            iDoc->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            if (!b.document().isEmpty()) {
                const QString ext = QFileInfo(b.document()).suffix().toLower();
                IconProvider::IconName iconName;
                if (ext == QStringLiteral("pdf"))
                    iconName = IconProvider::DocPdfImage16;
                else if (ext == QStringLiteral("doc") || ext == QStringLiteral("docx"))
                    iconName = IconProvider::DocWordImage16;
                else if (ext == QStringLiteral("xls") || ext == QStringLiteral("xlsx"))
                    iconName = IconProvider::DocExcelImage16;
                else
                    iconName = IconProvider::SearchFailed2;

                auto* iconLabel = new QLabel;
                iconLabel->setPixmap(IconProvider::icon(iconName).pixmap(16, 16));
                iconLabel->setAlignment(Qt::AlignCenter);
                iconLabel->setToolTip(b.document());
                iDoc->setData(Qt::UserRole, b.document());  // für documentActivated()
                tbl->setItem(i, kColDoc, iDoc);
                tbl->setCellWidget(i, kColDoc, iconLabel);
            } else {
                iDoc->setText(QStringLiteral("-"));
                iDoc->setTextAlignment(Qt::AlignCenter);
                tbl->setItem(i, kColDoc, iDoc);
            }

            tbl->setItem(i, kColDate,       iDate);
            tbl->setItem(i, kColVolume,     iVol);
            tbl->setItem(i, kColKurswert,   iKurswert);
            tbl->setItem(i, kColGebuehren,  iGebuehr);
            tbl->setItem(i, kColEinzahlung, iEinzahlung);
        }
    };

    auto populateJahresFooter = [this, buys, brokerages](int year, QTableWidget* f) {
        double totVol = 0.0, totGebuehr = 0.0, totEinzahlung = 0.0;
        for (int i = 0; i < buys.size(); ++i) {
            if (buys.at(i).year() != year)
                continue;
            const BuyObject&       b  = buys.at(i);
            const BrokerageObject& br = brokerages.at(i);
            totVol        += b.volume();
            totGebuehr    += br.isValid() ? br.brokerageReduction() : 0.0;
            totEinzahlung += b.buyValue() + (br.isValid() ? br.brokerageReduction() : 0.0);
        }

        auto* iLabel      = new QTableWidgetItem(tr("Gesamt:"));
        auto* iVol        = new QTableWidgetItem(formatVolume(totVol) + QStringLiteral(" stk."));
        auto* iKurswert   = new QTableWidgetItem(QStringLiteral("-"));
        auto* iGebuehr    = new QTableWidgetItem(formatMoney(totGebuehr) + QStringLiteral(" €"));
        auto* iEinzahlung = new QTableWidgetItem(formatMoney(totEinzahlung) + QStringLiteral(" €"));
        auto* iDoc        = new QTableWidgetItem(QStringLiteral("-"));
        for (auto* it : { iLabel, iVol, iKurswert, iGebuehr, iEinzahlung, iDoc })
            it->setTextAlignment(Qt::AlignCenter);

        f->setItem(0, kColDate,       iLabel);
        f->setItem(0, kColVolume,     iVol);
        f->setItem(0, kColKurswert,   iKurswert);
        f->setItem(0, kColGebuehren,  iGebuehr);
        f->setItem(0, kColEinzahlung, iEinzahlung);
        f->setItem(0, kColDoc,        iDoc);
    };

    auto jahresTitleForYear = [this, buys, brokerages](int year) {
        double yearTotal = 0.0;
        for (int i = 0; i < buys.size(); ++i) {
            if (buys.at(i).year() != year)
                continue;
            const BrokerageObject& br = brokerages.at(i);
            yearTotal += br.isValid()
                             ? buys.at(i).buyValue() + br.brokerageReduction()
                             : buys.at(i).buyValue();
        }
        return tr("%1 (%2 €)").arg(year).arg(formatMoney(yearTotal));
    };

    // Dokument-Spalte fest auf 36px, reine Icon-Spalte ohne Textinhalt und
    // ohne Spaltenüberschrift (16.07.2026, Nessies Vorgabe zur globalen
    // Vereinheitlichung aller Dokument-Spalten — siehe ARCHITECTURE.md,
    // "Dokument-Spalten: Breite verkleinern + Header"). kColDoc wird weiterhin
    // als jahresDocColumn übergeben, damit Doppelklick documentActivated()
    // auslöst.
    constexpr int kDocColWidth = 36;
    m_overviewTabs->populateOverview(
        years,
        uebersichtTitle,
        { tr("Jahr"), tr("Anteile"), tr("Einzahlung") },
        { 100, -1, -1 },
        populateUebersichtData,
        populateUebersichtFooter,
        jahresHeaders,
        { 100, -1, -1, -1, -1, kDocColWidth },
        jahresTitleForYear,
        populateJahresData,
        populateJahresFooter,
        kColDoc);
}

// ── showOverviewTab ───────────────────────────────────────────────────────────

void ViewBuyEdit::showOverviewTab()
{
    // populateOverview() already sets currentIndex(0) while suppressed.
    // If called without a prior populateOverview (e.g. onReset), ensure Tab 0.
    if (m_overviewTabs && m_overviewTabs->currentIndex() != 0) {
        m_suppressTabSignal = true;
        m_overviewTabs->setCurrentIndex(0);
        m_suppressTabSignal = false;
    }
    clearForm();
}

// ── clearPdfPreview ───────────────────────────────────────────────────────────

void ViewBuyEdit::clearPdfPreview()
{
    m_previewPanel->clearDocument();
}

// ── openPdfPreview ────────────────────────────────────────────────────────────

void ViewBuyEdit::openPdfPreview(const QString& pdfPath)
{
    m_previewPanel->showDocument(pdfPath);
}

// ── setButtonStates ───────────────────────────────────────────────────────────

void ViewBuyEdit::setButtonStates(bool canRemove, bool isLastBuy, bool isEdit)
{
    m_btnRemove->setEnabled(canRemove);

    // Button label: "Speichern" whenever an existing buy is loaded,
    // "Hinzufügen" in new-buy mode (no selection).
    if (isEdit) {
        m_btnAdd->setText(tr("Speichern"));
        m_btnAdd->setIcon(IconProvider::icon(IconProvider::ButtonSave));
    } else {
        m_btnAdd->setText(tr("Hinzufügen"));
        m_btnAdd->setIcon(IconProvider::icon(IconProvider::ButtonAdd));
    }

    // When a non-latest buy is selected, only the document path is editable.
    // Fields are truly disabled (not just read-only) so no interaction is possible.
    const bool readOnlyMode = !isLastBuy && isEdit;

    m_date->setEnabled(!readOnlyMode);
    m_time->setEnabled(!readOnlyMode);
    m_depotNumber->setEnabled(!readOnlyMode);
    m_orderNumber->setEnabled(!readOnlyMode);
    m_volume->setEnabled(!readOnlyMode);
    m_price->setEnabled(!readOnlyMode);
    m_provision->setEnabled(!readOnlyMode);
    m_brokerFee->setEnabled(!readOnlyMode);
    m_traderFee->setEnabled(!readOnlyMode);
    m_reduction->setEnabled(!readOnlyMode);

    // Document path and browse button are always available.
    m_btnBrowse->setEnabled(true);

    // Clear any leftover read-only styles — disabled state provides its own visual.
    const QString rwStyle = QString();
    m_date->setStyleSheet(rwStyle);
    m_time->setStyleSheet(rwStyle);
    m_orderNumber->setStyleSheet(rwStyle);
    m_volume->setStyleSheet(rwStyle);
    m_price->setStyleSheet(rwStyle);
    m_provision->setStyleSheet(rwStyle);
    m_brokerFee->setStyleSheet(rwStyle);
    m_traderFee->setStyleSheet(rwStyle);
    m_reduction->setStyleSheet(rwStyle);
}

// ── showError / acceptAndClose ────────────────────────────────────────────────

void ViewBuyEdit::showError(const QString& message)
{
    OwnMessageBox::critical(this, tr("Fehler"), message);
}

void ViewBuyEdit::acceptAndClose()
{
    accept();
}

// ── Private slots ─────────────────────────────────────────────────────────────

void ViewBuyEdit::onBrowseDocument()
{
    const QString root = AppSettings::instance().documentsRootPath();
    const QString startDir = !root.isEmpty() ? root : m_documentPath->text();

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

    m_documentPath->setText(path);
    m_presenter->onDocumentPathEdited();   // validate duplicate before parsing
    m_presenter->onDocumentSelected(path);
}

// ── Static helpers ────────────────────────────────────────────────────────────

QLabel* ViewBuyEdit::addRow(QGridLayout* grid, int& row,
                          const QString& labelText,
                          QWidget* field,
                          const QString& unitText,
                          const QString& statusKey)
{
    field->setFixedHeight(UiConstants::kFieldHeight);

    auto* lbl = new QLabel(labelText);
    lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(lbl, row, 0);

    if (unitText.isEmpty()) {
        grid->addWidget(field, row, 1, 1, 2);
    } else {
        auto* unit = new QLabel(unitText);
        unit->setFixedWidth(28);
        unit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        grid->addWidget(field, row, 1);
        grid->addWidget(unit,  row, 2);
    }

    QLabel* statusLabel = nullptr;
    if (!statusKey.isEmpty()) {
        statusLabel = new QLabel;
        statusLabel->setFixedSize(20, 20);
        statusLabel->setAlignment(Qt::AlignCenter);
        grid->addWidget(statusLabel, row, 3);
    }

    ++row;
    return statusLabel;
}

QString ViewBuyEdit::formatMoney(double value)
{
    return QLocale().toString(value, 'f', 2);
}

QString ViewBuyEdit::formatVolume(double value)
{
    return QLocale().toString(value, 'f', 4);
}

double ViewBuyEdit::parseDouble(const QString& text)
{
    QString s = text.trimmed();
    s.replace(QLatin1Char(','), QLatin1Char('.'));
    bool ok = false;
    const double v = s.toDouble(&ok);
    return ok ? v : 0.0;
}

// ── markMissingFieldsAsFailed ─────────────────────────────────────────────────

void ViewBuyEdit::markMissingFieldsAsFailed()
{
    struct Check { QString key; bool ok; };
    const QList<Check> checks = {
        { QStringLiteral("depotNumber"), !m_depotNumber->currentData().toString().isEmpty() },
        { QStringLiteral("orderNumber"), !m_orderNumber->text().trimmed().isEmpty() },
        { QStringLiteral("volume"),      parseDouble(m_volume->text()) > 0.0        },
        { QStringLiteral("price"),       parseDouble(m_price->text())  > 0.0        },
    };

    for (const auto& c : checks) {
        auto* lbl = m_statusLabels.value(c.key);
        if (!lbl) continue;

        if (c.ok) {
            if (m_fieldStates.value(c.key, FieldState::Untouched) != FieldState::Ok) {
                m_fieldStates[c.key] = FieldState::Ok;
                lbl->setPixmap(IconProvider::icon(IconProvider::SearchOk).pixmap(16, 16));
                lbl->setToolTip(tr("Feld ausgefüllt"));
            }
        } else {
            m_fieldStates[c.key] = FieldState::Error;
            lbl->setPixmap(IconProvider::icon(IconProvider::SearchFailed).pixmap(16, 16));
            lbl->setToolTip(tr("Pflichtfeld — bitte ausfüllen"));
        }
    }
}

// ── hasMissingRequiredFields ──────────────────────────────────────────────────

bool ViewBuyEdit::hasMissingRequiredFields(QStringList& missingFields) const
{
    missingFields.clear();

    if (m_depotNumber->currentData().toString().isEmpty())
        missingFields.append(tr("Depotnummer"));
    if (m_orderNumber->text().trimmed().isEmpty())
        missingFields.append(tr("Auftragsnummer"));
    if (parseDouble(m_volume->text()) <= 0.0)
        missingFields.append(tr("Gekaufte Anteile"));
    if (parseDouble(m_price->text()) <= 0.0)
        missingFields.append(tr("Kurs"));

    return !missingFields.isEmpty();
}

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ViewSaleEdit.h"
#include "PresenterSaleEdit.h"
#include "../../utils/SaleFifoAllocator.h"
#include "../../utils/ShareSplitAdjuster.h"
#include "../../utils/ShareSplitHint.h"
#include "../../utils/DocumentFieldValue.h"
#include "ModelSaleEdit.h"
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
#include <QDialog>
#include <functional>

#ifndef SPM_HAVE_QTPDF
#  include <QPixmap>
#  include <QProcess>
#endif

// ── Constructor ───────────────────────────────────────────────────────────────

ViewSaleEdit::ViewSaleEdit(const QString& shareGuid,
                           DocumentsConfig* config,
                           QWidget* parent)
    : QDialog(parent)
    , m_config(config)
{
    setWindowTitle(tr("Hinzufügen / editieren der Verkäufe dieser Aktie"));
    setFixedSize(1300, 820);

    setupUi();

    auto* model = new ModelSaleEdit();
    m_presenter = new PresenterSaleEdit(this, model, shareGuid, config, this);

    connect(m_btnAdd,    &QPushButton::clicked, m_presenter, &PresenterSaleEdit::onSave);
    connect(m_btnRemove, &QPushButton::clicked, m_presenter, &PresenterSaleEdit::onRemove);
    connect(m_btnReset,  &QPushButton::clicked, m_presenter, &PresenterSaleEdit::onReset);
    connect(m_btnClose,  &QPushButton::clicked, m_presenter, &PresenterSaleEdit::onClose);

    // Forward numeric text changes so derived fields stay current.
    // Guard against nullptr: these lambdas may fire during clearForm() which is
    // called inside the PresenterSaleEdit constructor — before m_presenter is set.
    auto fwd = [this](const QString&) { if (m_presenter) m_presenter->onValuesChanged(); };
    connect(m_volume,         &QLineEdit::textChanged, this, fwd);
    connect(m_salePrice,      &QLineEdit::textChanged, this, fwd);
    connect(m_taxAtSource,    &QLineEdit::textChanged, this, fwd);
    connect(m_capitalGainsTax,&QLineEdit::textChanged, this, fwd);
    connect(m_solidarityTax,  &QLineEdit::textChanged, this, fwd);
    connect(m_provision,      &QLineEdit::textChanged, this, fwd);
    connect(m_brokerFee,      &QLineEdit::textChanged, this, fwd);
    connect(m_traderFee,      &QLineEdit::textChanged, this, fwd);
    connect(m_reduction,      &QLineEdit::textChanged, this, fwd);

    // Live field validation
    connect(m_date, &QDateEdit::editingFinished,
            m_presenter, &PresenterSaleEdit::onDateEdited);
    connect(m_depotNumber, QOverload<int>::of(&QComboBox::activated),
            m_presenter, &PresenterSaleEdit::onDepotNumberEdited);
    connect(m_orderNumber, &QLineEdit::editingFinished,
            m_presenter, &PresenterSaleEdit::onOrderNumberEdited);
    connect(m_volume, &QLineEdit::editingFinished,
            m_presenter, &PresenterSaleEdit::onVolumeOrPriceEdited);
    connect(m_salePrice, &QLineEdit::editingFinished,
            m_presenter, &PresenterSaleEdit::onVolumeOrPriceEdited);

    auto connectFee = [this](QLineEdit* le, const QString& key) {
        connect(le, &QLineEdit::editingFinished, this,
                [this, le, key]() {
                    if (m_presenter)
                        m_presenter->onFeeEdited(key, parseDouble(le->text()));
                });
    };
    connectFee(m_provision, QStringLiteral("provision"));
    connectFee(m_brokerFee, QStringLiteral("brokerFee"));
    connectFee(m_traderFee, QStringLiteral("traderFee"));
    connectFee(m_reduction, QStringLiteral("reduction"));

    auto connectTax = [this](QLineEdit* le, const QString& key) {
        connect(le, &QLineEdit::editingFinished, this,
                [this, le, key]() {
                    if (m_presenter)
                        m_presenter->onTaxEdited(key, parseDouble(le->text()));
                });
    };
    connectTax(m_taxAtSource,     QStringLiteral("taxAtSource"));
    connectTax(m_capitalGainsTax, QStringLiteral("capitalGainsTax"));
    connectTax(m_solidarityTax,   QStringLiteral("solidarityTax"));
}

// ── setupUi ───────────────────────────────────────────────────────────────────

void ViewSaleEdit::setupUi()
{
    auto* main = new QHBoxLayout(this);
    main->setContentsMargins(6, 6, 6, 6);
    main->setSpacing(8);

    // Dokumenten-Vorschau zuerst erzeugen (aber erst unten ins Layout
    // einfügen) — createOverviewGroup() verbindet OverviewTabWidget::
    // documentActivated mit m_previewPanel und braucht dafür ein bereits
    // existierendes Objekt (Bugfix 16.07.2026, s. ARCHITECTURE.md, gleich
    // aus der BuysForm-Migration übernommen).
    auto* previewPanel = createPreviewPanel();

    m_leftPanel  = new QWidget;
    auto* leftLayout = new QVBoxLayout(m_leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(6);
    leftLayout->addWidget(createVerkaufsdatenGroup(), 0);
    leftLayout->addWidget(createDocumentGroup(),      0);
    leftLayout->addWidget(createButtonBar(),          0);
    leftLayout->addWidget(createOverviewGroup(),      1);
    m_leftPanel->setMinimumWidth(480);

    main->addWidget(m_leftPanel,  3);
    main->addWidget(previewPanel, 2);
}

// ── createVerkaufsdatenGroup ──────────────────────────────────────────────────

QGroupBox* ViewSaleEdit::createVerkaufsdatenGroup()
{
    auto* gb   = new QGroupBox(tr("  Verkaufsdaten"));
    gb->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    auto* grid = new QGridLayout(gb);
    grid->setColumnStretch(1, 1);
    grid->setVerticalSpacing(4);
    grid->setHorizontalSpacing(8);
    // Unterer Rand knapper als in den übrigen Gruppen (4 statt 10):
    // die letzte Zeile ist der Split-Hinweis, ein reines Textlabel ohne
    // Feldrahmen. Mit dem regulären Rand stand er zu weit von der
    // Gruppenunterkante ab (Nessies Vorgabe 11.08.2026).
    grid->setContentsMargins(8, 8, 8, 4);
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
    m_depotNumber->setEditable(false);
    m_depotNumber->addItem(tr("— bitte wählen —"));
    // Ein Eintrag je Depot; die Depotnummer ist der eindeutige Schlüssel und
    // steht deshalb sowohl im Anzeigetext als auch in den item data
    // (siehe DepotEntry).
    if (m_config) {
        for (const auto& depot : m_config->entries()) {
            if (!depot.depotNumber.isEmpty())
                m_depotNumber->addItem(
                    QStringLiteral("%1 (%2)").arg(depot.bankName, depot.depotNumber),
                    depot.depotNumber);
        }
    }
    m_statusLabels[QStringLiteral("depotNumber")] =
        addRow(grid, row, tr("Depot number:"), m_depotNumber,
               QString(), QStringLiteral("depotNumber"));
    m_inputWidgets[QStringLiteral("depotNumber")] = m_depotNumber;

    // ── Ordernummer ───────────────────────────────────────────────────────
    m_orderNumber = new QLineEdit;
    m_orderNumber->setMaxLength(100);
    m_statusLabels[QStringLiteral("orderNumber")] =
        addRow(grid, row, tr("Ordernummer:"), m_orderNumber,
               QString(), QStringLiteral("orderNumber"));
    m_inputWidgets[QStringLiteral("orderNumber")] = m_orderNumber;

    // ── Verkaufte Anteile ─────────────────────────────────────────────────
    m_volume = new QLineEdit(QStringLiteral("0,0000"));
    m_volume->setAlignment(Qt::AlignRight);
    m_volume->setValidator(new QDoubleValidator(0.0, 9'999'999.0, 4, m_volume));
    m_statusLabels[QStringLiteral("volume")] =
        addRow(grid, row, tr("Verkaufte Anteile:"), m_volume,
               tr("stk."), QStringLiteral("volume"));
    m_inputWidgets[QStringLiteral("volume")] = m_volume;

    // ── Verkaufs-Preis einer Aktie ────────────────────────────────────────
    m_salePrice = new QLineEdit(QStringLiteral("0,0000"));
    m_salePrice->setAlignment(Qt::AlignRight);
    m_salePrice->setValidator(new QDoubleValidator(0.0, 9'999'999.0, 4, m_salePrice));
    m_statusLabels[QStringLiteral("salePrice")] =
        addRow(grid, row, tr("Verkaufs- Preis einer Aktie:"), m_salePrice,
               tr("€"), QStringLiteral("salePrice"));
    m_inputWidgets[QStringLiteral("salePrice")] = m_salePrice;

    // ── Verkaufter Kaufwert (read-only: vol × salePrice) ──────────────────
    m_saleValue = new QLineEdit(QStringLiteral("0,00"));
    m_saleValue->setReadOnly(true);
    m_saleValue->setStyleSheet(QStringLiteral("background: palette(midlight);"));
    m_saleValue->setAlignment(Qt::AlignRight);
    addRow(grid, row, tr("Verkaufter Kaufwert:"), m_saleValue, tr("€"));

    // ── Kaufwert (read-only: FIFO buy value) ──────────────────────────────
    m_kaufwert = new QLineEdit(QStringLiteral("0,00"));
    m_kaufwert->setReadOnly(true);
    m_kaufwert->setStyleSheet(QStringLiteral("background: palette(midlight);"));
    m_kaufwert->setAlignment(Qt::AlignRight);
    m_kaufwert->setFixedHeight(UiConstants::kFieldHeight);

    // Details-Button direkt hinter dem Kaufwert-Feld
    m_btnDetails = new QPushButton(tr("Details"));
    m_btnDetails->setFixedHeight(UiConstants::kFieldHeight);
    m_btnDetails->setToolTip(tr("FIFO-Kaufzuteilung für diesen Verkauf anzeigen"));
    // Der Details-Inhalt wird seit dem Bugfix "anteilige Kauf-Nebenkosten
    // gehen bei der FIFO-Zuteilung verloren" im Presenter aufbereitet — nur
    // dort ist loadBrokerageForBuy() erreichbar. Die Verbindung entsteht
    // erst in setupUi()/Konstruktor, wenn m_presenter bereits steht.
    connect(m_btnDetails, &QPushButton::clicked, this, [this]() {
        if (m_presenter) m_presenter->onShowDetails();
    });

    {
        // Kaufwert + €-Einheit als zusammengesetztes Widget in col 1+2
        auto* kaufwertRow = new QWidget;
        kaufwertRow->setFixedHeight(UiConstants::kFieldHeight);
        auto* kl = new QHBoxLayout(kaufwertRow);
        kl->setContentsMargins(0, 0, 0, 0);
        kl->setSpacing(4);
        kl->addWidget(m_kaufwert, 1);
        auto* euroLabel = new QLabel(tr("€"));
        euroLabel->setFixedWidth(28);
        euroLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        kl->addWidget(euroLabel);

        auto* lblKaufwert = new QLabel(tr("Gekaufter Kaufwert:"));
        lblKaufwert->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(lblKaufwert,   row, 0);
        grid->addWidget(kaufwertRow,   row, 1, 1, 2);  // spans field + unit columns
        grid->addWidget(m_btnDetails,  row, 3);         // same column as status icons
        ++row;
    }

    // ── Gewinn / Verlust (read-only) ──────────────────────────────────────
    m_gewinnVerlust = new QLineEdit(QStringLiteral("0,00"));
    m_gewinnVerlust->setReadOnly(true);
    m_gewinnVerlust->setStyleSheet(QStringLiteral("background: palette(midlight);"));
    m_gewinnVerlust->setAlignment(Qt::AlignRight);
    addRow(grid, row, tr("Gewinn / Verlust:"), m_gewinnVerlust, tr("€"));

    // ── Quellsteuer ───────────────────────────────────────────────────────
    m_taxAtSource = new QLineEdit(QStringLiteral("0,00"));
    m_taxAtSource->setAlignment(Qt::AlignRight);
    m_taxAtSource->setValidator(new QDoubleValidator(0.0, 99'999.0, 2, m_taxAtSource));
    m_statusLabels[QStringLiteral("taxAtSource")] =
        addRow(grid, row, tr("Quellsteuer:"), m_taxAtSource,
               tr("€"), QStringLiteral("taxAtSource"));
    m_inputWidgets[QStringLiteral("taxAtSource")] = m_taxAtSource;

    // ── Kapitalertragssteuer ──────────────────────────────────────────────
    m_capitalGainsTax = new QLineEdit(QStringLiteral("0,00"));
    m_capitalGainsTax->setAlignment(Qt::AlignRight);
    m_capitalGainsTax->setValidator(new QDoubleValidator(0.0, 99'999.0, 2, m_capitalGainsTax));
    m_statusLabels[QStringLiteral("capitalGainsTax")] =
        addRow(grid, row, tr("Kapitalertragssteuer:"), m_capitalGainsTax,
               tr("€"), QStringLiteral("capitalGainsTax"));
    m_inputWidgets[QStringLiteral("capitalGainsTax")] = m_capitalGainsTax;

    // ── Solidaritätszuschlag ──────────────────────────────────────────────
    m_solidarityTax = new QLineEdit(QStringLiteral("0,00"));
    m_solidarityTax->setAlignment(Qt::AlignRight);
    m_solidarityTax->setValidator(new QDoubleValidator(0.0, 99'999.0, 2, m_solidarityTax));
    m_statusLabels[QStringLiteral("solidarityTax")] =
        addRow(grid, row, tr("Solidaritätszuschlag:"), m_solidarityTax,
               tr("€"), QStringLiteral("solidarityTax"));
    m_inputWidgets[QStringLiteral("solidarityTax")] = m_solidarityTax;

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

    // ── Rabatt ────────────────────────────────────────────────────────────
    m_reduction = new QLineEdit(QStringLiteral("0,00"));
    m_reduction->setAlignment(Qt::AlignRight);
    m_reduction->setValidator(new QDoubleValidator(0.0, 99'999.0, 2, m_reduction));
    m_statusLabels[QStringLiteral("reduction")] =
        addRow(grid, row, tr("Rabatt:"), m_reduction,
               tr("€"), QStringLiteral("reduction"));
    m_inputWidgets[QStringLiteral("reduction")] = m_reduction;

    // ── Ges. Gebühren (read-only) ─────────────────────────────────────────
    m_gesGebuehren = new QLineEdit(QStringLiteral("0,00"));
    m_gesGebuehren->setReadOnly(true);
    m_gesGebuehren->setStyleSheet(QStringLiteral("background: palette(midlight);"));
    m_gesGebuehren->setAlignment(Qt::AlignRight);
    addRow(grid, row, tr("Ges. Gebühren:"), m_gesGebuehren, tr("€"));

    // ── Auszahlung (read-only, green) ─────────────────────────────────────
    m_auszahlung = new QLineEdit(QStringLiteral("0,00"));
    m_auszahlung->setReadOnly(true);
    m_auszahlung->setStyleSheet(
        QStringLiteral("background: #d4edda; color: #155724; font-weight: bold;"));
    m_auszahlung->setAlignment(Qt::AlignRight);
    addRow(grid, row, tr("Auszahlung:"), m_auszahlung, tr("€"));

    // ── Split-Hinweis (09.08.2026, Phase 3b) ──────────────────────────────
    //
    // Fusszeile der Gruppe statt einer Zeile mitten im Formular: der Text
    // läuft beim Ändern des Datums live mit, und an dieser Stelle bewegt
    // sich dabei nichts oberhalb (Nessies Entscheidung 08.08.2026).
    //
    // Ab Spalte 1 statt Spalte 0 (Nessies Vorgabe 11.08.2026): der Hinweis
    // gehört inhaltlich zu den Eingabewerten, nicht zu den Feldnamen, und
    // beginnt deshalb bündig mit der Feldspalte. Er spannt bis Spalte 3
    // (inkl. Einheiten- und Status-Spalte), damit auch der lange Text mit
    // Umrechnung ohne Umbruch hineinpasst.
    //
    // Ein knapper Innenabstand statt einer Mindesthöhe (Nessies Vorgabe
    // 11.08.2026): der Hinweis soll sich vom letzten Eingabefeld absetzen,
    // aber die Gruppe nicht unnötig in die Höhe ziehen. Die 3 px sind die
    // Stellschraube, falls der Abstand nachjustiert werden muss.
    m_splitHint = new QLabel;
    m_splitHint->setObjectName(QStringLiteral("splitHint"));
    m_splitHint->setWordWrap(true);
    m_splitHint->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_splitHint->setContentsMargins(0, 3, 0, 0);
    grid->addWidget(m_splitHint, row, 1, 1, 3);
    ++row;

    return gb;
}

// ── createDocumentGroup ───────────────────────────────────────────────────────

QGroupBox* ViewSaleEdit::createDocumentGroup()
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
    connect(m_btnBrowse, &QPushButton::clicked, this, &ViewSaleEdit::onBrowseDocument);

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

    auto* docStatus = new QLabel;
    docStatus->setFixedSize(20, 20);
    docStatus->setAlignment(Qt::AlignCenter);
    grid->addWidget(docStatus, row, 2);
    m_statusLabels[QStringLiteral("document")] = docStatus;

    ++row;

    // ── Parse status bar ──────────────────────────────────────────────────
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

QWidget* ViewSaleEdit::createPreviewPanel()
{
    m_previewPanel = new DocumentPreviewPanel(this);
    return m_previewPanel;
}

// ── createButtonBar ───────────────────────────────────────────────────────────

QWidget* ViewSaleEdit::createButtonBar()
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

    layout->addStretch(1);
    layout->addWidget(m_btnAdd);
    layout->addWidget(m_btnRemove);
    layout->addWidget(m_btnReset);
    layout->addWidget(m_btnClose);

    return bar;
}

// ── createOverviewGroup ───────────────────────────────────────────────────────

QGroupBox* ViewSaleEdit::createOverviewGroup()
{
    auto* gb     = new QGroupBox(tr("  Verkaufs-Übersicht"));
    auto* layout = new QVBoxLayout(gb);
    layout->setContentsMargins(6, 6, 6, 6);

    m_overviewTabs = new OverviewTabWidget();
    m_overviewTabs->setMinimumHeight(140);
    layout->addWidget(m_overviewTabs);

    // Zeilenklick in einem Jahres-Tab → Verkauf laden. GUID kommt direkt aus
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
    // eingebettete Vorschau aktualisieren (neu, analog BuysForm).
    connect(m_overviewTabs, &OverviewTabWidget::documentActivated,
            m_previewPanel, &DocumentPreviewPanel::showDocument);

    return gb;
}

// ── IViewSaleEdit — read accessors ────────────────────────────────────────────

QString ViewSaleEdit::dateTime() const
{
    return QDateTime(m_date->date(), m_time->time()).toString(Qt::ISODate);
}

QString ViewSaleEdit::depotNumber() const
{
    const QVariant data = m_depotNumber->currentData();
    return data.isValid() ? data.toString() : QString();
}

QString ViewSaleEdit::orderNumber()  const { return m_orderNumber->text(); }
QString ViewSaleEdit::documentPath() const { return m_documentPath->text(); }

double ViewSaleEdit::volume()          const { return parseDouble(m_volume->text());          }
double ViewSaleEdit::salePrice()       const { return parseDouble(m_salePrice->text());       }
double ViewSaleEdit::taxAtSource()     const { return parseDouble(m_taxAtSource->text());     }
double ViewSaleEdit::capitalGainsTax() const { return parseDouble(m_capitalGainsTax->text()); }
double ViewSaleEdit::solidarityTax()   const { return parseDouble(m_solidarityTax->text());   }
double ViewSaleEdit::provision()       const { return parseDouble(m_provision->text());       }
double ViewSaleEdit::brokerFee()       const { return parseDouble(m_brokerFee->text());       }
double ViewSaleEdit::traderFee()       const { return parseDouble(m_traderFee->text());       }
double ViewSaleEdit::reduction()       const { return parseDouble(m_reduction->text());       }

// ── IViewSaleEdit — write methods ─────────────────────────────────────────────

void ViewSaleEdit::loadSale(const SaleObject& sale)
{
    m_loadedSale = sale;   // cache for Details dialog

    const QDateTime dt = QDateTime::fromString(sale.dateTime(), Qt::ISODate);
    m_date->setDate(dt.isValid() ? dt.date() : QDate::currentDate());
    m_time->setTime(dt.isValid() ? dt.time() : QTime::currentTime());

    const QString depotNr = sale.depotNumber().trimmed();
    {
        QSignalBlocker block(m_depotNumber);
        bool matched = false;
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
        }
    }

    m_orderNumber->setText(sale.orderNumber());
    m_volume->setText(formatVolume(sale.volume()));
    m_salePrice->setText(formatVolume(sale.salePrice()));
    m_taxAtSource->setText(formatMoney(sale.taxAtSource()));
    m_capitalGainsTax->setText(formatMoney(sale.capitalGainsTax()));
    m_solidarityTax->setText(formatMoney(sale.solidarityTax()));
    m_provision->setText(formatMoney(sale.provision()));
    m_brokerFee->setText(formatMoney(sale.brokerFee()));
    m_traderFee->setText(formatMoney(sale.traderFee()));
    m_reduction->setText(formatMoney(sale.reduction()));
    m_documentPath->setText(sale.document());
}

void ViewSaleEdit::clearForm()
{
    m_date->setDate(QDate::currentDate());
    m_time->setTime(QTime::currentTime());
    {
        QSignalBlocker block(m_depotNumber);
        m_depotNumber->setCurrentIndex(0);
    }
    m_orderNumber->clear();
    m_volume->setText(QStringLiteral("0,0000"));
    m_salePrice->setText(QStringLiteral("0,0000"));
    m_taxAtSource->setText(QStringLiteral("0,00"));
    m_capitalGainsTax->setText(QStringLiteral("0,00"));
    m_solidarityTax->setText(QStringLiteral("0,00"));
    m_provision->setText(QStringLiteral("0,00"));
    m_brokerFee->setText(QStringLiteral("0,00"));
    m_traderFee->setText(QStringLiteral("0,00"));
    m_reduction->setText(QStringLiteral("0,00"));
    m_documentPath->clear();

    m_loadedSale = SaleObject{};   // reset cached sale — back to new-sale mode

    // Restore all editable fields
    m_date->setEnabled(true);            m_date->setStyleSheet(QString());
    m_time->setEnabled(true);            m_time->setStyleSheet(QString());
    m_depotNumber->setEnabled(true);
    m_orderNumber->setEnabled(true);     m_orderNumber->setStyleSheet(QString());
    m_volume->setEnabled(true);          m_volume->setStyleSheet(QString());
    m_salePrice->setEnabled(true);       m_salePrice->setStyleSheet(QString());
    m_taxAtSource->setEnabled(true);     m_taxAtSource->setStyleSheet(QString());
    m_capitalGainsTax->setEnabled(true); m_capitalGainsTax->setStyleSheet(QString());
    m_solidarityTax->setEnabled(true);   m_solidarityTax->setStyleSheet(QString());
    m_provision->setEnabled(true);       m_provision->setStyleSheet(QString());
    m_brokerFee->setEnabled(true);       m_brokerFee->setStyleSheet(QString());
    m_traderFee->setEnabled(true);       m_traderFee->setStyleSheet(QString());
    m_reduction->setEnabled(true);       m_reduction->setStyleSheet(QString());

    for (auto it = m_statusLabels.begin(); it != m_statusLabels.end(); ++it) {
        if (it.value()) {
            it.value()->setPixmap(QPixmap());
            it.value()->setToolTip(QString());
        }
    }
    m_fieldStates.clear();

    m_parseProgress->setValue(0);
    m_parseProgress->setStyleSheet(
        QStringLiteral("QProgressBar { background: transparent; border: none; }"));
    m_parseStatusIcon->setPixmap(QPixmap());
    m_parseStatus->clear();

    clearPdfPreview();
}

void ViewSaleEdit::populateAvailableBuys(const QList<BuyObject>& buys)
{
    m_availableBuys = buys;
}

void ViewSaleEdit::setAllBuys(const QList<BuyObject>& buys)
{
    m_allBuys = buys;
}

void ViewSaleEdit::setSplits(const QList<ShareSplitObject>& splits)
{
    m_splits = splits;
}

void ViewSaleEdit::setSaleValue(double value)
{
    m_saleValue->setText(formatMoney(value));
    // Color: positive = green, negative = red
    if (value >= 0.0)
        m_saleValue->setStyleSheet(QStringLiteral("background: palette(midlight);"));
    else
        m_saleValue->setStyleSheet(QStringLiteral("background: palette(midlight);"));
}

void ViewSaleEdit::setKaufwert(double value)
{
    m_kaufwert->setText(formatMoney(value));
}

void ViewSaleEdit::setGewinnVerlust(double value)
{
    m_gewinnVerlust->setText(formatMoney(value));
    // Positive = green tint, negative = red tint
    if (value >= 0.0)
        m_gewinnVerlust->setStyleSheet(
            QStringLiteral("background: #d4edda; color: #155724; font-weight: bold;"));
    else
        m_gewinnVerlust->setStyleSheet(
            QStringLiteral("background: #f8d7da; color: #721c24; font-weight: bold;"));
}

void ViewSaleEdit::setGesGebuehren(double value)
{
    m_gesGebuehren->setText(formatMoney(value));
}

void ViewSaleEdit::setTaxSum(double value)
{
    // taxSum is displayed as the sum in the Auszahlung derivation; shown nowhere separately
    Q_UNUSED(value)
}

void ViewSaleEdit::setAuszahlung(double value)
{
    m_auszahlung->setText(formatMoney(value));
    if (value >= 0.0)
        m_auszahlung->setStyleSheet(
            QStringLiteral("background: #d4edda; color: #155724; font-weight: bold;"));
    else
        m_auszahlung->setStyleSheet(
            QStringLiteral("background: #f8d7da; color: #721c24; font-weight: bold;"));
}

// ── Field status ──────────────────────────────────────────────────────────────

void ViewSaleEdit::setSplitHint(const QString& text, const QString& tooltip, bool hasSplit)
{
    m_splitHint->setText(text);
    m_splitHint->setToolTip(tooltip);

    // Mit Split hervorgehoben, ohne Split gedämpft — die Zeile bleibt in
    // beiden Fällen stehen, damit das Formular nicht springt.
    //
    // Orange statt Blau (Nessies Vorgabe 11.08.2026): der Hinweis meldet,
    // dass die Beleg-Stückzahl nicht mehr dem heutigen Stand entspricht.
    // Der gedämpfte Zustand bleibt bewusst zurückhaltend — er steht bei
    // jeder Aktie ohne Split dauerhaft im Formular und würde in Warnfarbe
    // abstumpfen. `palette(mid)` war im dunklen Theme aber kaum lesbar,
    // deshalb `palette(placeholderText)`: gedämpft, aber kontrastreich.
    m_splitHint->setStyleSheet(hasSplit
        ? QStringLiteral("color: #C77400; font-weight: bold;")
        : QStringLiteral("color: palette(placeholderText);"));
}

void ViewSaleEdit::setFieldOk(const QString& field, const QString& value)
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
        if (!value.isEmpty()) {
            // Zahlenfeld oder Textfeld? Der QDoubleValidator entscheidet —
            // nur dort darf der Punkt umgedeutet werden. Siehe ViewBuyEdit
            // und ARCHITECTURE.md, "Rohwerte aus Belegen".
            const bool numeric =
                qobject_cast<const QDoubleValidator*>(le->validator()) != nullptr;
            le->setText(numeric ? DocumentFieldValue::forNumericField(value)
                                : DocumentFieldValue::forTextField(value));
        }
    } else if (field == QStringLiteral("depotNumber")) {
        if (value.isEmpty()) return;
        QSignalBlocker block(m_depotNumber);
        for (int i = 0; i < m_depotNumber->count(); ++i) {
            if (m_depotNumber->itemData(i).toString() == value.trimmed()) {
                m_depotNumber->setCurrentIndex(i);
                return;
            }
        }
        // Not found in known entries (e.g. parsed from PDF) — add dynamically.
        m_depotNumber->addItem(value.trimmed(), value.trimmed());
        m_depotNumber->setCurrentIndex(m_depotNumber->count() - 1);
    } else if (auto* de = qobject_cast<QDateEdit*>(widget)) {
        // Der DKB-Verkaufsbeleg liefert Datum und Uhrzeit in EINEM Fang
        // ("Schlusstag/-Zeit  27.02.2020 19:16:37") — toDate() holt sich
        // seinen Teil heraus. Schlägt das fehl, muss es sichtbar werden:
        // sonst bliebe das heutige Datum stehen und ginge als Verkaufstag
        // in die FIFO-Zuordnung ein (Nessies Bugreport 22.08.2026).
        const QDate d = DocumentFieldValue::toDate(value);
        if (d.isValid())
            de->setDate(d);
        else if (!value.isEmpty())
            setFieldError(field);
    } else if (auto* te = qobject_cast<QTimeEdit*>(widget)) {
        const QTime t = DocumentFieldValue::toTime(value);
        if (t.isValid())
            te->setTime(t);
        else if (!value.isEmpty())
            setFieldError(field);
    }
}

void ViewSaleEdit::setFieldError(const QString& field)
{
    auto* lbl = m_statusLabels.value(field);
    if (!lbl) return;
    m_fieldStates[field] = FieldState::Error;
    lbl->setPixmap(IconProvider::icon(IconProvider::SearchFailed).pixmap(16, 16));
    lbl->setToolTip(tr("Ungültige oder fehlende Eingabe"));
    lbl->setVisible(true);
}

void ViewSaleEdit::setDocumentPath(const QString& path)
{
    m_documentPath->setText(path);
}

void ViewSaleEdit::setDocumentPreview(const QString& /*text*/) {}

// ── Parse status bar ──────────────────────────────────────────────────────────

void ViewSaleEdit::setParseProgress(int percent, const QString& status)
{
    m_parseProgress->setStyleSheet(QString());
    m_parseProgress->setValue(percent);
    m_parseStatus->setText(status);
    if (percent < 100)
        m_parseStatusIcon->setPixmap(
            IconProvider::icon(IconProvider::SearchInfo).pixmap(16, 16));
}

void ViewSaleEdit::setParseStatusIcon(int iconType)
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

void ViewSaleEdit::setUiBusy(bool busy)
{
    if (m_leftPanel)
        m_leftPanel->setDisabled(busy);
    m_btnAdd->setDisabled(busy);
    m_btnClose->setDisabled(busy);

    if (busy) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        m_parseProgress->setStyleSheet(QString());
        m_parseStatusIcon->setPixmap(
            IconProvider::icon(IconProvider::SearchInfo).pixmap(16, 16));
    } else {
        QApplication::restoreOverrideCursor();
        m_parseProgress->setValue(100);
        if (m_leftPanel) m_leftPanel->update();
        update();
    }
}

void ViewSaleEdit::onParseFinished()
{
    static const QStringList requiredFieldKeys = {
        QStringLiteral("date"),
        QStringLiteral("depotNumber"),
        QStringLiteral("orderNumber"),
        QStringLiteral("volume"),
        QStringLiteral("salePrice"),
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

void ViewSaleEdit::populateOverview(const QList<SaleObject>&       sales,
                                    const QList<ShareSplitObject>& splits)
{
    if (sales.isEmpty()) {
        m_overviewTabs->clear();
        return;
    }

    QList<int> years;
    for (const SaleObject& s : sales) {
        const int y = s.year();
        if (!years.contains(y)) years.append(y);
    }
    std::sort(years.begin(), years.end(), std::greater<int>());

    // ── Übersicht-Tab (Jahr | Anteile | Auszahlung | G/V) ─────────────────
    //
    // Aggregate rechnen je Beleg über ShareSplitAdjuster::adjustedVolume()
    // auf heutige Skala und summieren erst danach — niemals die Summe
    // skalieren. Liegt vor dem ältesten Beleg einer Summe ein Split, war
    // mindestens ein summierter Beleg umzurechnen; genau dann tragen Zelle
    // und Fusszeile den Marker (Phase 3c, 11.08.2026).
    //
    // @note Diese Regel gilt für STÜCKZAHLEN, nicht für Geldbeträge.
    // Auszahlung und Gewinn/Verlust werden unverändert summiert — ein Split
    // schafft weder Gewinn noch Verlust. Siehe ARCHITECTURE.md, "Anteilige
    // Kauf-Nebenkosten der FIFO-Zuteilung".
    QMap<int, double> yearVolToday, yearPayout, yearGV;
    QMap<int, QDate>  yearEarliest;
    QDate             earliestOverall;

    for (const SaleObject& s : sales) {
        const int y = s.year();
        yearVolToday[y] += ShareSplitAdjuster::adjustedVolume(s.volume(), splits, s.date());
        yearPayout[y]   += s.payoutBrokerageReduction();
        yearGV[y]       += s.profitLossBrokerageReduction();

        const QDate d = s.date();
        if (d.isValid()) {
            if (!yearEarliest.contains(y) || d < yearEarliest.value(y))
                yearEarliest[y] = d;
            if (!earliestOverall.isValid() || d < earliestOverall)
                earliestOverall = d;
        }
    }

    double totVolToday = 0, totPayout = 0, totGV = 0;
    for (int y : std::as_const(years)) {
        totVolToday += yearVolToday.value(y);
        totPayout   += yearPayout.value(y);
        totGV       += yearGV.value(y);
    }

    // Marker- und Tooltip-Texte einmal vorab bauen, damit die Lambdas unten
    // nur noch nachschlagen und nicht jede für sich dieselbe Prüfung machen.
    QMap<int, bool>    yearVolAffected;
    QMap<int, QString> yearVolTooltip;
    for (int y : std::as_const(years)) {
        const QDate earliest = yearEarliest.value(y);
        yearVolAffected[y] = ShareSplitHint::hasSplitAfter(splits, earliest);
        yearVolTooltip[y]  = ShareSplitHint::overviewAggregateTooltip(splits, earliest);
    }
    const bool    totalVolAffected = ShareSplitHint::hasSplitAfter(splits, earliestOverall);
    const QString totalVolTooltip  = ShareSplitHint::overviewAggregateTooltip(splits, earliestOverall);

    auto populateUebersichtData = [years, yearVolToday, yearPayout, yearGV,
                                   yearVolAffected, yearVolTooltip](QTableWidget* tbl) {
        tbl->setRowCount(years.size());
        for (int i = 0; i < years.size(); ++i) {
            const int y = years.at(i);
            auto* iY = new QTableWidgetItem(QString::number(y));
            auto* iV = new QTableWidgetItem(
                ShareSplitHint::withMarker(
                    formatVolume(yearVolToday.value(y)) + QStringLiteral(" stk."),
                    yearVolAffected.value(y)));
            auto* iP = new QTableWidgetItem(
                formatMoney(yearPayout.value(y)) + QStringLiteral(" €"));
            auto* iG = new QTableWidgetItem(
                formatMoney(yearGV.value(y)) + QStringLiteral(" €"));
            for (auto* it : { iY, iV, iP, iG })
                it->setTextAlignment(Qt::AlignCenter);
            iY->setData(Qt::UserRole, y);
            if (!yearVolTooltip.value(y).isEmpty())
                iV->setToolTip(yearVolTooltip.value(y));
            tbl->setItem(i, 0, iY);
            tbl->setItem(i, 1, iV);
            tbl->setItem(i, 2, iP);
            tbl->setItem(i, 3, iG);
        }
    };

    auto populateUebersichtFooter = [this, totVolToday, totPayout, totGV,
                                     totalVolAffected, totalVolTooltip](QTableWidget* f) {
        auto* iL = new QTableWidgetItem(tr("Gesamt:"));
        auto* iV = new QTableWidgetItem(
            ShareSplitHint::withMarker(
                formatVolume(totVolToday) + QStringLiteral(" stk."), totalVolAffected));
        auto* iP = new QTableWidgetItem(formatMoney(totPayout) + QStringLiteral(" €"));
        auto* iG = new QTableWidgetItem(formatMoney(totGV) + QStringLiteral(" €"));
        for (auto* it : { iL, iV, iP, iG })
            it->setTextAlignment(Qt::AlignCenter);
        if (!totalVolTooltip.isEmpty())
            iV->setToolTip(totalVolTooltip);
        f->setItem(0, 0, iL); f->setItem(0, 1, iV);
        f->setItem(0, 2, iP); f->setItem(0, 3, iG);
    };

    const QString uebersichtTitle = tr("Übersicht (%1 €)").arg(formatMoney(totPayout));

    // ── Jahres-Tabs ───────────────────────────────────────────────────────
    constexpr int kColDate       = 0;
    constexpr int kColVolume     = 1;
    constexpr int kColAuszahlung = 2;
    constexpr int kColGV         = 3;
    constexpr int kColDoc        = 4;

    const QStringList jahresHeaders = {
        tr("Datum"), tr("Anteile"), tr("Auszahlung"), tr("Gewinn / Verlust"), QString()
    };

    auto populateJahresData = [this, sales, splits](int year, QTableWidget* tbl) {
        QList<SaleObject> yearSales;
        for (const SaleObject& s : sales)
            if (s.year() == year) yearSales.append(s);

        tbl->setRowCount(yearSales.size());
        for (int i = 0; i < yearSales.size(); ++i) {
            const SaleObject& s = yearSales.at(i);

            auto* iDate = new QTableWidgetItem(s.dateAsStr());
            iDate->setData(Qt::UserRole, s.guid());
            iDate->setTextAlignment(Qt::AlignCenter);

            // Belegzeile: bleibt in BELEG-Skala. Die Zeile ist eine
            // Abschrift des Dokuments, das nach einem Zeilenklick rechts in
            // der Vorschau steht — die Zahlen müssen sich decken. Der Marker
            // und sein Tooltip nennen die heutige Entsprechung.
            const bool    volAffected = ShareSplitHint::hasSplitAfter(splits, s.date());
            const QString volTooltip  = ShareSplitHint::overviewRowTooltip(
                splits, s.date(), s.volume(), s.salePrice());

            auto* iVol = new QTableWidgetItem(
                ShareSplitHint::withMarker(
                    formatVolume(s.volume()) + QStringLiteral(" stk."), volAffected));
            iVol->setTextAlignment(Qt::AlignCenter);
            if (!volTooltip.isEmpty())
                iVol->setToolTip(volTooltip);

            auto* iAusz = new QTableWidgetItem(
                formatMoney(s.payoutBrokerageReduction()) + QStringLiteral(" €"));
            iAusz->setTextAlignment(Qt::AlignCenter);

            auto* iGV = new QTableWidgetItem(
                formatMoney(s.profitLossBrokerageReduction()) + QStringLiteral(" €"));
            iGV->setTextAlignment(Qt::AlignCenter);

            auto* iDoc = new QTableWidgetItem;
            iDoc->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            iDoc->setTextAlignment(Qt::AlignCenter);
            if (!s.document().isEmpty()) {
                iDoc->setData(Qt::UserRole, s.document());
                const QString ext = QFileInfo(s.document()).suffix().toLower();
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
                iconLabel->setToolTip(s.document());
                tbl->setItem(i, kColDoc, iDoc);
                tbl->setCellWidget(i, kColDoc, iconLabel);
            } else {
                iDoc->setText(QStringLiteral("-"));
                tbl->setItem(i, kColDoc, iDoc);
            }

            tbl->setItem(i, kColDate,       iDate);
            tbl->setItem(i, kColVolume,     iVol);
            tbl->setItem(i, kColAuszahlung, iAusz);
            tbl->setItem(i, kColGV,         iGV);
        }
    };

    auto populateJahresFooter = [this, sales, splits,
                                 yearVolAffected, yearVolTooltip](int year, QTableWidget* f) {
        // Summe über die Belege eines Jahres: je Beleg umrechnen, dann
        // summieren. Fällt ein Split mitten ins Jahr, mischte die frühere
        // rohe Summe zwei Stückelungen und war damit bedeutungslos.
        double volToday = 0, payout = 0, gv = 0;
        for (const SaleObject& s : sales) {
            if (s.year() != year) continue;
            volToday += ShareSplitAdjuster::adjustedVolume(s.volume(), splits, s.date());
            payout   += s.payoutBrokerageReduction();
            gv       += s.profitLossBrokerageReduction();
        }
        auto* iL = new QTableWidgetItem(tr("Gesamt:"));
        auto* iV = new QTableWidgetItem(
            ShareSplitHint::withMarker(
                formatVolume(volToday) + QStringLiteral(" stk."),
                yearVolAffected.value(year)));
        auto* iP = new QTableWidgetItem(formatMoney(payout)  + QStringLiteral(" €"));
        auto* iG = new QTableWidgetItem(formatMoney(gv)      + QStringLiteral(" €"));
        auto* iD = new QTableWidgetItem(QStringLiteral("-"));
        for (auto* it : { iL, iV, iP, iG, iD })
            it->setTextAlignment(Qt::AlignCenter);
        if (!yearVolTooltip.value(year).isEmpty())
            iV->setToolTip(yearVolTooltip.value(year));
        f->setItem(0, 0, iL); f->setItem(0, 1, iV);
        f->setItem(0, 2, iP); f->setItem(0, 3, iG);
        f->setItem(0, 4, iD);
    };

    auto jahresTitleForYear = [this, sales](int year) {
        double payout = 0;
        for (const SaleObject& s : sales)
            if (s.year() == year) payout += s.payoutBrokerageReduction();
        return tr("%1 (%2 €)").arg(year).arg(formatMoney(payout));
    };

    // Dokument-Spalte fest auf 36px, reine Icon-Spalte ohne Textinhalt und
    // ohne Spaltenüberschrift (16.07.2026, Nessies Vorgabe zur globalen
    // Vereinheitlichung aller Dokument-Spalten — siehe ARCHITECTURE.md,
    // "Dokument-Spalten: Breite verkleinern + Header"; löst den vorherigen
    // 120px-Zwischenstand ab). kColDoc wird weiterhin als jahresDocColumn
    // übergeben, damit der Doppelklick documentActivated() auslöst.
    constexpr int kDocColWidth = 36;
    m_overviewTabs->populateOverview(
        years,
        uebersichtTitle,
        { tr("Jahr"), tr("Anteile"), tr("Auszahlung"), tr("Gewinn / Verlust") },
        { 100, -1, -1, -1 },
        populateUebersichtData,
        populateUebersichtFooter,
        jahresHeaders,
        { 100, -1, -1, -1, kDocColWidth },
        jahresTitleForYear,
        populateJahresData,
        populateJahresFooter,
        kColDoc);
}

// ── showOverviewTab ───────────────────────────────────────────────────────────

void ViewSaleEdit::showOverviewTab()
{
    if (m_overviewTabs && m_overviewTabs->currentIndex() != 0) {
        m_suppressTabSignal = true;
        m_overviewTabs->setCurrentIndex(0);
        m_suppressTabSignal = false;
    }
    clearForm();
}

// ── clearPdfPreview / openPdfPreview ──────────────────────────────────────────

void ViewSaleEdit::clearPdfPreview()
{
    m_previewPanel->clearDocument();
}

void ViewSaleEdit::openPdfPreview(const QString& pdfPath)
{
    m_previewPanel->showDocument(pdfPath);
}

// ── setButtonStates ───────────────────────────────────────────────────────────

void ViewSaleEdit::setButtonStates(bool canRemove, bool isLastSale, bool isEdit)
{
    // Für onShowDetails(): steuert dort, ob die FIFO-Zuteilung neu berechnet
    // (jüngster Verkauf, editierbar) oder die gespeicherte Zuteilung gezeigt
    // wird (älterer, nicht editierbarer Verkauf). Siehe ARCHITECTURE.md,
    // "Offene Punkte", "Aktiensplits werden nicht behandelt", Phase 2c.
    m_isLastSale = isLastSale;

    m_btnRemove->setEnabled(canRemove);

    if (isEdit) {
        m_btnAdd->setText(tr("Speichern"));
        m_btnAdd->setIcon(IconProvider::icon(IconProvider::ButtonSave));
    } else {
        m_btnAdd->setText(tr("Hinzufügen"));
        m_btnAdd->setIcon(IconProvider::icon(IconProvider::ButtonAdd));
    }

    // Non-latest sale: only document path editable
    const bool readOnlyMode = !isLastSale && isEdit;

    m_date->setEnabled(!readOnlyMode);
    m_time->setEnabled(!readOnlyMode);
    m_depotNumber->setEnabled(!readOnlyMode);
    m_orderNumber->setEnabled(!readOnlyMode);
    m_volume->setEnabled(!readOnlyMode);
    m_salePrice->setEnabled(!readOnlyMode);
    m_taxAtSource->setEnabled(!readOnlyMode);
    m_capitalGainsTax->setEnabled(!readOnlyMode);
    m_solidarityTax->setEnabled(!readOnlyMode);
    m_provision->setEnabled(!readOnlyMode);
    m_brokerFee->setEnabled(!readOnlyMode);
    m_traderFee->setEnabled(!readOnlyMode);
    m_reduction->setEnabled(!readOnlyMode);

    m_btnBrowse->setEnabled(true);
    m_btnDetails->setEnabled(true);   // always clickable: edit→stored details, new→FIFO preview

    const QString rwStyle = QString();
    m_date->setStyleSheet(rwStyle);
    m_time->setStyleSheet(rwStyle);
    m_orderNumber->setStyleSheet(rwStyle);
    m_volume->setStyleSheet(rwStyle);
    m_salePrice->setStyleSheet(rwStyle);
    m_taxAtSource->setStyleSheet(rwStyle);
    m_capitalGainsTax->setStyleSheet(rwStyle);
    m_solidarityTax->setStyleSheet(rwStyle);
    m_provision->setStyleSheet(rwStyle);
    m_brokerFee->setStyleSheet(rwStyle);
    m_traderFee->setStyleSheet(rwStyle);
    m_reduction->setStyleSheet(rwStyle);
}

// ── showError / acceptAndClose ────────────────────────────────────────────────

void ViewSaleEdit::showError(const QString& message)
{
    OwnMessageBox::critical(this, tr("Fehler"), message);
}

void ViewSaleEdit::acceptAndClose()
{
    accept();
}

// ── Private slots ─────────────────────────────────────────────────────────────

void ViewSaleEdit::onBrowseDocument()
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

    // onDocumentSelected() now writes the path into the view and validates
    // it itself (same as PresenterShareSplitEdit::onDocumentSelected()), so
    // both this manual browse path and a document dropped onto "Direkte
    // Dokumentenerfassung" go through the identical single code path.
    m_presenter->onDocumentSelected(path);
}

void ViewSaleEdit::showBuyDetails(const SaleBuyDetailSummary& summary)
{
    // ── Daten ─────────────────────────────────────────────────────────────
    // Die Aufbereitung liegt seit dem Bugfix "anteilige Kauf-Nebenkosten
    // gehen bei der FIFO-Zuteilung verloren" vollständig im Presenter
    // (PresenterSaleEdit::buildBuyDetailSummary(), siehe ARCHITECTURE.md).
    // Hier wird nur noch gerendert. Grund für die Verlagerung: die
    // anteilige Kauf-Brokerage kommt über IModelSaleEdit::
    // loadBrokerageForBuy(), und die View hat per MVP keinen Modellzugriff
    // — der Live-FIFO-Zweig setzte die Kosten deshalb hart auf 0,00 €.
    //
    // Alle Werte liegen bereits auf heutiger (split-bereinigter) Skala vor;
    // Geldbeträge (Kosten, Rabatt) sind unskaliert.
    using DetailRow = SaleBuyDetailRow;

    const QList<SaleBuyDetailRow>& rows = summary.rows;

    const bool   isEditMode = summary.editMode;
    const double totVol     = summary.totalVolume;
    const double totFees    = summary.totalFees;
    const double totRed     = summary.totalReduction;
    const double totBuyVal  = summary.totalBuyValue;
    const double totSaleVal = summary.totalSaleValue;
    const double saleFees   = summary.saleFees;

    const double totBuyValWithFees = totBuyVal + totFees - totRed;
    const double totPL             = summary.totalProfitLoss;

    // ── Dialog ────────────────────────────────────────────────────────────
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("Details der verwendeten Käufe"));
    dlg->setMinimumWidth(800);
    dlg->resize(860, 420);
    auto* mainLayout = new QVBoxLayout(dlg);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    // Mode-Hinweis
    auto* modeLabel = new QLabel(
        isEditMode
            ? tr("Tatsächliche FIFO-Zuteilung des gespeicherten Verkaufs:")
            : tr("Vorschau der FIFO-Zuteilung (wird beim Speichern festgelegt):"));
    modeLabel->setStyleSheet(
        isEditMode
            ? QStringLiteral("font-weight: bold; color: #155724;")
            : QStringLiteral("font-weight: bold; color: #856404;"));
    mainLayout->addWidget(modeLabel);

    // ── GroupBox 1: Verwendete Käufe ──────────────────────────────────────
    // Spalten: Datum | Anteile | × | Kaufkurs | = | Kaufsumme | + | Kosten | - | Rabatt | = | Gesamt | Dok.
    // Indizes:   0       1      2      3        4       5        6     7      8     9      10    11      12
    constexpr int kCols     = 13;
    constexpr int kColDate  = 0;
    constexpr int kColVol   = 1;
    constexpr int kColMul   = 2;
    constexpr int kColPrice = 3;
    constexpr int kColEq1   = 4;
    constexpr int kColSumme = 5;
    constexpr int kColPlus  = 6;
    constexpr int kColFees  = 7;
    constexpr int kColMinus = 8;
    constexpr int kColRed   = 9;
    constexpr int kColEq2   = 10;
    constexpr int kColTotal = 11;
    constexpr int kColDoc   = 12;

    const QStringList headers = {
        tr("Datum"), tr("Anteile"), QString(),
        tr("Kaufkurs"), QString(),
        tr("Kaufsumme"), QString(),
        tr("Kosten"), QString(),
        tr("Rabatt"), QString(),
        tr("Gesamt"), QString()
    };

    // Hilfsfunktion: zentriertes Item
    auto makeCenter = [](const QString& text) {
        auto* it = new QTableWidgetItem(text);
        it->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
        return it;
    };
    // Hilfsfunktion: rechtsbündiges Item
    auto makeOp = [](const QString& sym) {
        auto* it = new QTableWidgetItem(sym);
        it->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
        it->setForeground(QColor(QStringLiteral("#888888")));
        it->setFlags(Qt::ItemIsEnabled);
        return it;
    };

    QFont boldFont;
    boldFont.setBold(true);

    // ── dataTable ─────────────────────────────────────────────────────────
    auto* dataTable = new QTableWidget(rows.size(), kCols);
    dataTable->setHorizontalHeaderLabels(headers);
    dataTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    dataTable->setSelectionMode(QAbstractItemView::NoSelection);
    dataTable->setAlternatingRowColors(true);
    dataTable->verticalHeader()->setVisible(false);
    dataTable->setFrameShape(QFrame::NoFrame);

    // Datum: 100px fix (identisch zur Verkaufsübersicht im Eltern-Dialog)
    // Operatoren: 24px fix
    // Anteile / Kaufkurs / Gesamt: breit gestreckt
    // Kosten / Rabatt: schmal gestreckt (keine großen Summen erwartet)
    dataTable->horizontalHeader()->setSectionResizeMode(kColDate,  QHeaderView::Fixed);
    dataTable->setColumnWidth(kColDate,  100);
    dataTable->horizontalHeader()->setSectionResizeMode(kColVol,   QHeaderView::Stretch);
    dataTable->horizontalHeader()->setSectionResizeMode(kColMul,   QHeaderView::Fixed);
    dataTable->setColumnWidth(kColMul, 24);
    dataTable->horizontalHeader()->setSectionResizeMode(kColPrice, QHeaderView::Stretch);
    dataTable->horizontalHeader()->setSectionResizeMode(kColEq1,   QHeaderView::Fixed);
    dataTable->setColumnWidth(kColEq1, 24);
    dataTable->horizontalHeader()->setSectionResizeMode(kColSumme, QHeaderView::Stretch);
    dataTable->horizontalHeader()->setSectionResizeMode(kColPlus,  QHeaderView::Fixed);
    dataTable->setColumnWidth(kColPlus, 24);
    dataTable->horizontalHeader()->setSectionResizeMode(kColFees,  QHeaderView::Stretch);
    dataTable->horizontalHeader()->setSectionResizeMode(kColMinus, QHeaderView::Fixed);
    dataTable->setColumnWidth(kColMinus, 24);
    dataTable->horizontalHeader()->setSectionResizeMode(kColRed,   QHeaderView::Stretch);
    dataTable->horizontalHeader()->setSectionResizeMode(kColEq2,   QHeaderView::Fixed);
    dataTable->setColumnWidth(kColEq2, 24);
    dataTable->horizontalHeader()->setSectionResizeMode(kColTotal, QHeaderView::Stretch);
    dataTable->horizontalHeader()->setSectionResizeMode(kColDoc,   QHeaderView::Fixed);
    dataTable->setColumnWidth(kColDoc, 36);
    // Initialbreiten für den ersten Paint (Stretch-Spalten)
    dataTable->setColumnWidth(kColDate,  100);
    dataTable->setColumnWidth(kColVol,   140);
    dataTable->setColumnWidth(kColPrice, 120);
    dataTable->setColumnWidth(kColSumme, 120);
    dataTable->setColumnWidth(kColFees,   70);
    dataTable->setColumnWidth(kColRed,    70);
    dataTable->setColumnWidth(kColTotal, 120);

    for (int i = 0; i < rows.size(); ++i) {
        const DetailRow& r = rows.at(i);
        dataTable->setItem(i, kColDate,  makeCenter(r.date));
        dataTable->setItem(i, kColVol,   makeCenter(formatVolume(r.volume)   + QStringLiteral(" Stk.")));
        dataTable->setItem(i, kColMul,   makeOp(QStringLiteral("×")));
        dataTable->setItem(i, kColPrice, makeCenter(formatMoney(r.buyPrice)  + QStringLiteral(" €")));
        dataTable->setItem(i, kColEq1,   makeOp(QStringLiteral("=")));
        dataTable->setItem(i, kColSumme, makeCenter(formatMoney(r.buyValue)  + QStringLiteral(" €")));
        dataTable->setItem(i, kColPlus,  makeOp(QStringLiteral("+")));
        dataTable->setItem(i, kColFees,  makeCenter(formatMoney(r.fees)      + QStringLiteral(" €")));
        dataTable->setItem(i, kColMinus, makeOp(QStringLiteral("-")));
        dataTable->setItem(i, kColRed,   makeCenter(formatMoney(r.reduction) + QStringLiteral(" €")));
        dataTable->setItem(i, kColEq2,   makeOp(QStringLiteral("=")));
        dataTable->setItem(i, kColTotal, makeCenter(formatMoney(r.buyValue + r.fees - r.reduction) + QStringLiteral(" €")));

        // Dokument-Icon
        auto* iDoc = new QTableWidgetItem;
        iDoc->setFlags(Qt::ItemIsEnabled);
        iDoc->setData(Qt::UserRole, r.document);   // Pfad für Doppelklick-Handler
        if (!r.document.isEmpty()) {
            const QString ext = QFileInfo(r.document).suffix().toLower();
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
            iconLabel->setToolTip(tr("Doppelklick: Dokument anzeigen\n%1").arg(r.document));
            dataTable->setItem(i, kColDoc, iDoc);
            dataTable->setCellWidget(i, kColDoc, iconLabel);
        } else {
            iDoc->setText(QStringLiteral("-"));
            iDoc->setTextAlignment(Qt::AlignCenter);
            dataTable->setItem(i, kColDoc, iDoc);
        }
    }

    // Doppelklick auf Dokument-Spalte → PDF-Vorschau-Dialog
    QObject::connect(
        dataTable, &QTableWidget::cellDoubleClicked,
        dlg, [dataTable, kColDoc, dlg](int row, int col) {
            if (col != kColDoc) return;
            const auto* item = dataTable->item(row, kColDoc);
            if (!item) return;
            const QString path = item->data(Qt::UserRole).toString();
            if (path.isEmpty()) return;

            // ── Dokumenten-Vorschau-Dialog ──────────────────────────────
            auto* previewDlg = new QDialog(dlg);
            previewDlg->setWindowTitle(QFileInfo(path).fileName());
            previewDlg->resize(700, 900);
            auto* ly = new QVBoxLayout(previewDlg);
            ly->setContentsMargins(6, 6, 6, 6);
            ly->setSpacing(4);

#ifdef SPM_HAVE_QTPDF
            auto* pdfDoc  = new QPdfDocument(previewDlg);
            auto* pdfView = new QPdfView(previewDlg);
            pdfView->setDocument(pdfDoc);
            pdfView->setPageMode(QPdfView::PageMode::MultiPage);
            pdfView->setZoomMode(QPdfView::ZoomMode::FitInView);
            pdfView->setStyleSheet(
                QStringLiteral("QPdfView { background-color: #ffffff; }"
                                "QPdfView > QWidget { background-color: #ffffff; }"));
            pdfDoc->load(path);
            QTimer::singleShot(100, pdfView, [pdfView]() {
                pdfView->setZoomMode(QPdfView::ZoomMode::FitInView);
            });
            ly->addWidget(pdfView, 1);
#else
            auto* scroll = new QScrollArea(previewDlg);
            scroll->setWidgetResizable(true);
            auto* imgLabel = new QLabel;
            imgLabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
            scroll->setWidget(imgLabel);
            ly->addWidget(scroll, 1);

            // pdftoppm-Fallback: nur erste Seite
            const QString outBase = QDir::tempPath()
                                    + QStringLiteral("/spm_details_preview");
            auto* proc = new QProcess(previewDlg);
            QObject::connect(proc,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                previewDlg, [proc, imgLabel, scroll, outBase](int exitCode, QProcess::ExitStatus) {
                    proc->deleteLater();
                    if (exitCode != 0) {
                        imgLabel->setText(QObject::tr("PDF-Vorschau konnte nicht gerendert werden."));
                        return;
                    }
                    const QString imgPath = outBase + QStringLiteral("-1.png");
                    QPixmap px(imgPath);
                    if (px.isNull()) {
                        imgLabel->setText(QObject::tr("Vorschaubild nicht gefunden."));
                        return;
                    }
                    const int availW = scroll->viewport()->width() - 4;
                    if (availW > 0 && px.width() > availW)
                        px = px.scaledToWidth(availW, Qt::SmoothTransformation);
                    imgLabel->setPixmap(px);
                    imgLabel->resize(px.size());
                });
            proc->start(QStringLiteral("pdftoppm"),
                        { QStringLiteral("-r"),  QStringLiteral("150"),
                          QStringLiteral("-png"),
                          QStringLiteral("-f"),  QStringLiteral("1"),
                          QStringLiteral("-l"),  QStringLiteral("1"),
                          path, outBase });
#endif
            auto* btnClose = new QPushButton(QObject::tr("Schließen"));
            QObject::connect(btnClose, &QPushButton::clicked,
                             previewDlg, &QDialog::accept);
            auto* btnBar2 = new QHBoxLayout;
            btnBar2->addStretch();
            btnBar2->addWidget(btnClose);
            ly->addLayout(btnBar2);

            previewDlg->exec();
            previewDlg->deleteLater();
        });


    // ── footerTable ───────────────────────────────────────────────────────
    auto* footerTable = new QTableWidget(1, kCols);
    footerTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    footerTable->setSelectionMode(QAbstractItemView::NoSelection);
    footerTable->horizontalHeader()->setVisible(false);
    footerTable->verticalHeader()->setVisible(false);
    footerTable->setShowGrid(false);
    footerTable->setFrameShape(QFrame::NoFrame);
    footerTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    footerTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    const int rowH = dataTable->rowHeight(0) > 0 ? dataTable->rowHeight(0) : 24;
    footerTable->setFixedHeight(rowH + 2);

    // Alle Footer-Spalten auf Fixed — Breiten kommen vom dataTable
    for (int c = 0; c < kCols; ++c)
        footerTable->horizontalHeader()->setSectionResizeMode(c, QHeaderView::Fixed);

    auto makeSumItem = [&](const QString& text, Qt::Alignment align = Qt::AlignCenter) {
        auto* it = new QTableWidgetItem(text);
        it->setTextAlignment(align | Qt::AlignVCenter);
        it->setFont(boldFont);
        return it;
    };

    footerTable->setItem(0, kColDate,  makeSumItem(QString()));
    footerTable->setItem(0, kColVol,   makeSumItem(formatVolume(totVol)   + QStringLiteral(" Stk.")));
    footerTable->setItem(0, kColMul,   makeSumItem(QString()));
    footerTable->setItem(0, kColPrice, makeSumItem(QString()));
    footerTable->setItem(0, kColEq1,   makeSumItem(QString()));
    footerTable->setItem(0, kColSumme, makeSumItem(formatMoney(totBuyVal) + QStringLiteral(" €")));
    footerTable->setItem(0, kColPlus,  makeSumItem(QString()));
    footerTable->setItem(0, kColFees,  makeSumItem(formatMoney(totFees)   + QStringLiteral(" €")));
    footerTable->setItem(0, kColMinus, makeSumItem(QString()));
    footerTable->setItem(0, kColRed,   makeSumItem(formatMoney(totRed)    + QStringLiteral(" €")));
    footerTable->setItem(0, kColEq2,   makeSumItem(QString()));
    footerTable->setItem(0, kColTotal, makeSumItem(formatMoney(totBuyValWithFees) + QStringLiteral(" €")));
    footerTable->setItem(0, kColDoc,   makeSumItem(QString()));

    // Spaltenbreiten initial vom dataTable übernehmen (nach erstem Layout-Durchlauf),
    // dann live synchron halten.
    QObject::connect(
        dataTable->horizontalHeader(), &QHeaderView::sectionResized,
        footerTable, [footerTable](int col, int, int newSize) {
            footerTable->setColumnWidth(col, newSize);
        });
    // Einmalige initiale Übertragung nach dem ersten Paint (Stretch braucht sichtbares Widget)
    QTimer::singleShot(0, dataTable, [dataTable, footerTable, kCols=kCols]() {
        for (int c = 0; c < kCols; ++c)
            footerTable->setColumnWidth(c, dataTable->columnWidth(c));
    });

    // Separator + GroupBox zusammenbauen
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);

    auto* gbBuys   = new QGroupBox(tr("  Verwendete Käufe"));
    auto* gbBuysLy = new QVBoxLayout(gbBuys);
    gbBuysLy->setContentsMargins(6, 6, 6, 6);
    gbBuysLy->setSpacing(0);
    gbBuysLy->addWidget(dataTable, 1);
    gbBuysLy->addWidget(sep);
    gbBuysLy->addWidget(footerTable);
    mainLayout->addWidget(gbBuys, 1);

    // ── GroupBox 2: Gewinn/Verlust-Rechnung ───────────────────────────────
    const QString plFg = totPL >= 0.0
                         ? QStringLiteral("#155724") : QStringLiteral("#721c24");
    const QString plBg = totPL >= 0.0
                         ? QStringLiteral("#d4edda") : QStringLiteral("#f8d7da");

    auto* gbGV   = new QGroupBox(tr("  Übersicht Gewinn/Verlust-Rechnung"));
    auto* gbGVLy = new QHBoxLayout(gbGV);
    gbGVLy->setContentsMargins(10, 8, 10, 8);
    gbGVLy->setSpacing(0);

    // addGvCell: Label-Text normal, Zahlenwert fett — als QWidget mit zwei QLabels
    auto addGvCell = [&](const QString& labelText, const QString& valueText,
                         const QString& fg = QString(),
                         const QString& bg = QString()) {
        auto* cell   = new QWidget;
        auto* cellLy = new QVBoxLayout(cell);
        cellLy->setContentsMargins(6, 4, 6, 4);
        cellLy->setSpacing(1);

        auto* lblTitle = new QLabel(labelText);
        lblTitle->setAlignment(Qt::AlignCenter);
        QString titleStyle = QStringLiteral("font-size: 11px;");
        if (!fg.isEmpty()) titleStyle += QStringLiteral(" color: %1;").arg(fg);
        lblTitle->setStyleSheet(titleStyle);

        auto* lblValue = new QLabel(valueText);
        lblValue->setAlignment(Qt::AlignCenter);
        QString valStyle = QStringLiteral("font-weight: bold; font-size: 13px;");
        if (!fg.isEmpty()) valStyle += QStringLiteral(" color: %1;").arg(fg);
        lblValue->setStyleSheet(valStyle);

        if (!bg.isEmpty()) {
            cell->setAutoFillBackground(true);
            cell->setStyleSheet(QStringLiteral(
                "background: %1; border-radius: 3px;").arg(bg));
        }

        cellLy->addWidget(lblTitle);
        cellLy->addWidget(lblValue);
        gbGVLy->addWidget(cell, 2);
    };

    auto addGvSep = [&](const QString& sym) {
        auto* lbl = new QLabel(sym);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px; "
                                           "color: palette(mid-dark);"));
        lbl->setFixedWidth(24);
        gbGVLy->addWidget(lbl);
    };

    addGvCell(tr("Ges. Anteile"),
              formatVolume(totVol) + QStringLiteral(" Stk."));
    addGvSep(QStringLiteral("·"));
    addGvCell(tr("Ges. Verkauf"),
              formatMoney(totSaleVal) + QStringLiteral(" €"));
    addGvSep(QStringLiteral("−"));
    addGvCell(tr("Ges. Kauf (inkl. Kosten)"),
              formatMoney(totBuyValWithFees) + QStringLiteral(" €"));
    addGvSep(QStringLiteral("−"));
    addGvCell(tr("Verkaufsgebühren / Steuern"),
              formatMoney(saleFees) + QStringLiteral(" €"));
    addGvSep(QStringLiteral("="));
    addGvCell(tr("Gewinn / Verlust"),
              formatMoney(totPL) + QStringLiteral(" €"),
              plFg, plBg);

    mainLayout->addWidget(gbGV);

    // ── Hinweis + OK ──────────────────────────────────────────────────────
    auto* note = new QLabel(
        isEditMode
            ? tr("Kaufkosten anteilig aus den zugeordneten Käufen. Gewinn/Verlust inkl. anteiliger Kaufkosten, Verkaufsgebühren und Steuern.")
            : tr("Vorschau nach FIFO: älteste Käufe des gewählten Depots zuerst. "
                 "Kaufkosten anteilig aus den zugeordneten Käufen. Gewinn/Verlust inkl. anteiliger Kaufkosten, Verkaufsgebühren und Steuern."));
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color: palette(mid-dark); font-style: italic;"));
    mainLayout->addWidget(note);

    auto* btnOk = new QPushButton(tr("OK"));
    btnOk->setFixedWidth(80);
    connect(btnOk, &QPushButton::clicked, dlg, &QDialog::accept);
    auto* btnBar = new QHBoxLayout;
    btnBar->addStretch();
    btnBar->addWidget(btnOk);
    mainLayout->addLayout(btnBar);

    dlg->exec();
    dlg->deleteLater();
}

// ── Static helpers ────────────────────────────────────────────────────────────

QLabel* ViewSaleEdit::addRow(QGridLayout* grid, int& row,
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

QString ViewSaleEdit::formatMoney(double value)
{
    return QLocale().toString(value, 'f', 2);
}

QString ViewSaleEdit::formatVolume(double value)
{
    return QLocale().toString(value, 'f', 4);
}

double ViewSaleEdit::parseDouble(const QString& text)
{
    QString s = text.trimmed();
    s.replace(QLatin1Char(','), QLatin1Char('.'));
    bool ok = false;
    const double v = s.toDouble(&ok);
    return ok ? v : 0.0;
}

// ── markMissingFieldsAsFailed ─────────────────────────────────────────────────

void ViewSaleEdit::markMissingFieldsAsFailed()
{
    struct Check { QString key; bool ok; };
    const QList<Check> checks = {
        { QStringLiteral("depotNumber"), !m_depotNumber->currentData().toString().isEmpty() },
        { QStringLiteral("orderNumber"), !m_orderNumber->text().trimmed().isEmpty()         },
        { QStringLiteral("volume"),      parseDouble(m_volume->text())    > 0.0             },
        { QStringLiteral("salePrice"),   parseDouble(m_salePrice->text()) > 0.0             },
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

bool ViewSaleEdit::hasMissingRequiredFields(QStringList& missingFields) const
{
    missingFields.clear();
    if (m_depotNumber->currentData().toString().isEmpty())
        missingFields.append(tr("Depotnummer"));
    if (m_orderNumber->text().trimmed().isEmpty())
        missingFields.append(tr("Auftragsnummer"));
    if (parseDouble(m_volume->text()) <= 0.0)
        missingFields.append(tr("Verkaufte Anteile"));
    if (parseDouble(m_salePrice->text()) <= 0.0)
        missingFields.append(tr("Verkaufspreis"));
    return !missingFields.isEmpty();
}

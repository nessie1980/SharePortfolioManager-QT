// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "MainWindow.h"
#include "../../config/AppSettings.h"
#include "../../config/WebSitesConfig.h"
#include "../../config/DocumentsConfig.h"
#include "../../core/Database.h"
#include "../LoggerSettingsForm/LoggerSettingsForm.h"
#include "../SoundSettingsForm/SoundSettingsForm.h"
#include "../BackupSettingsForm/BackupSettingsForm.h"
#include "../DocumentsSettingsForm/DocumentsSettingsForm.h"
#include "../ShareDetailsForm/ViewShareDetails.h"
#include "../ChartForm/ChartPopup.h"
#include "../AboutForm/AboutForm.h"
#include "../ApiSettingsForm/ApiSettingsForm.h"
#include "../../repositories/ShareRepository.h"
#include "../../repositories/DailyValuesRepository.h"
#include "../../models/ShareObject.h"
#include "../../utils/ShareCalculator.h"
#include "../../utils/DocumentClassifier.h"
#include "../../IconProvider.h"
#include "../../widgets/GridStyle.h"

// ── Direkte Dokumentenerfassung (Drag+Drop, Feature 27.07.2026) ───────────────
// Full presenter headers (not just the forward-declared View headers) are
// needed here because openCaptureDialog() calls a method directly on the
// presenter returned by each dialog's presenter() accessor.
#include "../BuysForm/ViewBuyEdit.h"
#include "../BuysForm/PresenterBuyEdit.h"
#include "../SalesForm/ViewSaleEdit.h"
#include "../SalesForm/PresenterSaleEdit.h"
#include "../DividendForm/ViewDividendEdit.h"
#include "../DividendForm/PresenterDividendEdit.h"
#include "../ShareAddForm/PresenterShareAdd.h"

#include <QApplication>
#include <QCoreApplication>
#include <QMenuBar>
#include <QStatusBar>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSizePolicy>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QSoundEffect>
#include <QUrl>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <algorithm>
#include "../OwnMessageBoxForm/OwnMessageBox.h"
#include "../BackupProgressForm/BackupProgressDialog.h"
#include <QTime>
#include <QLocale>
#include <QTextCursor>
#include <QDebug>
#include "../../../libs/parser/src/Parser.h"
#include "../../../libs/parser/src/DataTypes.h"

// ── Constructor ───────────────────────────────────────────────────────────────

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    initialize();
}

// Test-only constructor (07.07.2026): injects a QNetworkAccessManager into
// both internal Parser instances so tests can exercise the real refresh flow
// against a ParserTestUtils::FakeNetworkAccessManager instead of a real
// network — see tests/parser/FakeNetworkAccessManager.h. The member
// initializer list is the only difference from the constructor above; the
// rest of construction is identical and shared via initialize().
MainWindow::MainWindow(QNetworkAccessManager* networkManagerForTesting, QWidget* parent)
    : QMainWindow(parent)
    , m_parserMarketValues(networkManagerForTesting)
    , m_parserDailyValues(networkManagerForTesting)
{
    Q_ASSERT_X(networkManagerForTesting, "MainWindow",
              "Test-only constructor requires a non-null QNetworkAccessManager "
              "(e.g. a ParserTestUtils::FakeNetworkAccessManager). Use the "
              "MainWindow(QWidget*) constructor in production code.");
    initialize();
}

// ── initialize ────────────────────────────────────────────────────────────────
//
// Shared construction body for both constructors above. Extracted 07.07.2026
// so the test-only constructor doesn't have to duplicate it — the only thing
// that differs between the two constructors is which Parser constructor
// m_parserMarketValues/m_parserDailyValues are built with, which is decided
// in each constructor's member initializer list, before this method runs.
// No behavioral change versus the previous single-constructor version.
void MainWindow::initialize()
{
    setWindowTitle(baseWindowTitle());
    setMinimumSize(900, 600);

    // Activate the configured icon set (default for now)
    IconProvider::setIconSet(QStringLiteral("default"));

    setupActions();
    setupMenuBar();
    setupToolBar();
    setupCentralWidget();
    setupStatusBar();
    restoreWindowGeometry();

    // Show startup info in status message area
    addStatusMessage(tr("Anwendung gestartet."), MessageType::Info);

    // Load and validate all configuration files
    // If any fail, all controls are disabled except Quit
    if (!checkAndLoadConfigurations())
        return;

    const QString portfolioPath = AppSettings::instance().portfolioPath();
    if (portfolioPath.isEmpty()) {
        // No portfolio configured yet — normal first start
        addStatusMessage(tr("Kein Portfolio konfiguriert. Bitte legen Sie ein neues Portfolio an oder öffnen Sie ein vorhandenes."),
                         MessageType::Warning);
        updateStatusBarPortfolio(QString());
    } else if (!QFileInfo::exists(portfolioPath)) {
        // Configured portfolio file no longer exists — do NOT create a new one
        addStatusMessage(tr("Portfolio nicht gefunden: %1").arg(portfolioPath),
                         MessageType::FatalError);
        addStatusMessage(tr("Bitte legen Sie ein neues Portfolio an oder öffnen Sie ein vorhandenes."),
                         MessageType::Warning);
        // Clear the invalid path from settings
        AppSettings::instance().setPortfolioPath(QString());
        updateStatusBarPortfolio(QString());
    } else {
        // Portfolio loaded successfully by AppStartup — create backup first
        createBackup(portfolioPath);
        addStatusMessage(tr("Portfolio geladen: %1").arg(portfolioPath),
                         MessageType::Success);
        updateStatusBarPortfolio(portfolioPath);
        populatePortfolioTables();
    }

    // Kein Root-Verzeichnis für Dokumente konfiguriert: Dialog wird angeboten
    // (nicht zwingend), egal welcher der drei Zweige oben gelaufen ist.
    // Muss nach dem Öffnen der Portfolio-DB laufen, damit DocumentRootMigrator
    // vorhandene Dokumentpfade auswerten kann (siehe ARCHITECTURE.md).
    ensureDocumentsRootConfigured();
}

// ── setupActions ──────────────────────────────────────────────────────────────

void MainWindow::setupActions()
{
    // ── File ──────────────────────────────────────────────────────────────
    m_actionNew = new QAction(IconProvider::icon(IconProvider::MenuFileAdd),
                              tr("&Neu"), this);
    m_actionNew->setShortcut(QKeySequence::New);
    m_actionNew->setStatusTip(tr("Neues Portfolio erstellen"));
    connect(m_actionNew, &QAction::triggered, this, &MainWindow::onNewPortfolio);

    m_actionOpen = new QAction(IconProvider::icon(IconProvider::MenuFolderOpen),
                               tr("&Öffnen..."), this);
    m_actionOpen->setShortcut(QKeySequence::Open);
    m_actionOpen->setStatusTip(tr("Vorhandenes Portfolio öffnen"));
    connect(m_actionOpen, &QAction::triggered, this, &MainWindow::onOpenPortfolio);

    m_actionSaveAs = new QAction(IconProvider::icon(IconProvider::ButtonSaveAs),
                                 tr("Speichern &unter..."), this);
    m_actionSaveAs->setShortcut(QKeySequence::SaveAs);
    m_actionSaveAs->setStatusTip(tr("Portfolio unter neuem Namen speichern"));
    connect(m_actionSaveAs, &QAction::triggered, this, &MainWindow::onSaveAsPortfolio);

    m_actionQuit = new QAction(IconProvider::icon(IconProvider::ButtonExit),
                               tr("&Beenden"), this);
    m_actionQuit->setShortcut(QKeySequence::Quit);
    m_actionQuit->setStatusTip(tr("Anwendung beenden"));
    connect(m_actionQuit, &QAction::triggered, qApp, &QApplication::quit);

    // ── Share ─────────────────────────────────────────────────────────────
    m_actionAdd = new QAction(IconProvider::icon(IconProvider::ButtonAdd),
                              tr("&Hinzufügen"), this);
    m_actionAdd->setStatusTip(tr("Neue Aktie hinzufügen"));
    m_actionAdd->setEnabled(false);
    connect(m_actionAdd, &QAction::triggered, this, &MainWindow::onAddShare);

    m_actionEdit = new QAction(IconProvider::icon(IconProvider::ButtonEdit),
                               tr("&Editieren"), this);
    m_actionEdit->setStatusTip(tr("Ausgewählte Aktie bearbeiten"));
    m_actionEdit->setEnabled(false);
    connect(m_actionEdit, &QAction::triggered, this, &MainWindow::onEditShare);

    m_actionDelete = new QAction(IconProvider::icon(IconProvider::ButtonDelete),
                                 tr("En&tfernen"), this);
    m_actionDelete->setStatusTip(tr("Ausgewählte Aktie entfernen"));
    m_actionDelete->setEnabled(false);
    connect(m_actionDelete, &QAction::triggered, this, &MainWindow::onDeleteShare);

    m_actionRefresh = new QAction(IconProvider::icon(IconProvider::ButtonUpdate),
                                  tr("&Aktualisieren"), this);
    m_actionRefresh->setStatusTip(tr("Kurs der ausgewählten Aktie aktualisieren"));
    m_actionRefresh->setEnabled(false);
    connect(m_actionRefresh, &QAction::triggered, this, &MainWindow::onRefreshShare);

    m_actionRefreshAll = new QAction(IconProvider::icon(IconProvider::ButtonUpdateAll),
                                     tr("Alle &aktualisieren"), this);
    m_actionRefreshAll->setStatusTip(tr("Kurse aller Aktien aktualisieren"));
    m_actionRefreshAll->setEnabled(false);
    connect(m_actionRefreshAll, &QAction::triggered, this, &MainWindow::onRefreshAll);

    // ── Parser setup ──────────────────────────────────────────────────────
    connect(&m_parserMarketValues, &ParserLib::Parser::parserUpdated,
            this, &MainWindow::onMarketValuesUpdated);
    connect(&m_parserDailyValues, &ParserLib::Parser::parserUpdated,
            this, &MainWindow::onDailyValuesUpdated);

    // ── Settings ──────────────────────────────────────────────────────────
    m_actionLanguage = new QAction(IconProvider::icon(IconProvider::MenuFlagGerman),
                                   tr("&Sprache..."), this);
    m_actionLogger   = new QAction(IconProvider::icon(IconProvider::MenuEventLog),
                                   tr("&Logger..."), this);
    connect(m_actionLogger, &QAction::triggered, this, [this]() {
        LoggerSettingsForm dialog(this);
        dialog.exec();
    });
    m_actionSound    = new QAction(IconProvider::icon(IconProvider::MenuSound),
                                   tr("S&ound..."), this);
    connect(m_actionSound, &QAction::triggered, this, [this]() {
        SoundSettingsForm dialog(this);
        dialog.exec();
    });
    m_actionBackup   = new QAction(IconProvider::icon(IconProvider::MenuSettings),
                                   tr("&Backup..."), this);
    connect(m_actionBackup, &QAction::triggered, this, [this]() {
        BackupSettingsForm dialog(this);
        dialog.exec();
    });
    m_actionDocuments = new QAction(IconProvider::icon(IconProvider::MenuFolderOpen),
                                    tr("&Dokumente..."), this);
    connect(m_actionDocuments, &QAction::triggered, this, [this]() {
        DocumentsSettingsForm dialog(this);
        dialog.exec();
    });

    // ── API Settings ──────────────────────────────────────────────────────
    m_actionApiKeyYahoo = new QAction(IconProvider::icon(IconProvider::MenuKey),
                                      tr("&Yahoo Finance..."), this);
    connect(m_actionApiKeyYahoo, &QAction::triggered, this, [this]() {
        ApiSettingsForm dialog(
            QStringLiteral("ApiYahoo"),
            AppSettings::instance().apiKeyYahoo(),
            this);
        if (dialog.exec() == QDialog::Accepted)
            AppSettings::instance().setApiKeyYahoo(dialog.apiKey());
    });

    // ── Help ──────────────────────────────────────────────────────────────
    m_actionAbout = new QAction(IconProvider::icon(IconProvider::MenuAbout),
                                tr("&Über..."), this);
    connect(m_actionAbout, &QAction::triggered, this, [this]() {
        AboutForm dialog(this);
        dialog.exec();
    });
}

// ── setupMenuBar ──────────────────────────────────────────────────────────────

void MainWindow::setupMenuBar()
{
    QMenu* fileMenu = menuBar()->addMenu(tr("&Datei"));
    fileMenu->addAction(m_actionNew);
    fileMenu->addAction(m_actionOpen);
    fileMenu->addAction(m_actionSaveAs);
    fileMenu->addSeparator();
    fileMenu->addAction(m_actionQuit);

    QMenu* settingsMenu = menuBar()->addMenu(tr("&Einstellungen"));
    settingsMenu->addAction(m_actionLanguage);
    settingsMenu->addSeparator();
    settingsMenu->addAction(m_actionLogger);
    settingsMenu->addAction(m_actionSound);
    settingsMenu->addAction(m_actionBackup);
    settingsMenu->addAction(m_actionDocuments);

    QMenu* apiMenu = menuBar()->addMenu(tr("&API-Einstellung"));
    apiMenu->addAction(m_actionApiKeyYahoo);

    QMenu* helpMenu = menuBar()->addMenu(tr("&Hilfe"));
    helpMenu->addAction(m_actionAbout);
}

// ── setupToolBar ──────────────────────────────────────────────────────────────

void MainWindow::setupToolBar()
{
    m_toolBar = addToolBar(tr("Hauptleiste"));
    m_toolBar->setObjectName(QStringLiteral("MainToolBar"));
    m_toolBar->setMovable(false);
    m_toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_toolBar->setIconSize(QSize(24, 24));

    m_toolBar->addAction(m_actionNew);
    m_toolBar->addAction(m_actionOpen);
    m_toolBar->addSeparator();
    m_toolBar->addAction(m_actionRefreshAll);
    m_toolBar->addAction(m_actionRefresh);
    m_toolBar->addSeparator();
    m_toolBar->addAction(m_actionAdd);
    m_toolBar->addAction(m_actionEdit);
    m_toolBar->addAction(m_actionDelete);

    // Separators: same color and thickness as toolbar top/bottom border lines.
    // QPalette::Light matches the highlight border Qt draws around toolbars.
    const QString borderColor = palette().color(QPalette::Light).name();
    m_toolBar->setStyleSheet(QStringLiteral(
        "QToolBar::separator {"
        "  background: transparent;"
        "  border-left: 2px solid %1;"
        "  width: 2px;"
        "  margin: 2px 6px;"
        "}").arg(borderColor));
}

// ── setupCentralWidget ────────────────────────────────────────────────────────

void MainWindow::setupCentralWidget()
{
    // ── Portfolio label ───────────────────────────────────────────────────
    m_portfolioLabel = new QLabel(
        tr("Portfolio-Übersicht ( Einträge: 0 ) / Letzte Aktualisierung: -"));

    // ── Tab widget ────────────────────────────────────────────────────────
    m_portfolioTabs = new QTabWidget();

    auto setupTable = [](QTableWidget* tbl) {
        tbl->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        tbl->verticalHeader()->setVisible(false);
        tbl->setSelectionBehavior(QAbstractItemView::SelectRows);
        tbl->setSelectionMode(QAbstractItemView::SingleSelection);
        tbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
        tbl->setAlternatingRowColors(true);
        tbl->setSortingEnabled(false); // disabled while inserting; enabled after load
        tbl->setShowGrid(true);
        tbl->setIconSize(QSize(24, 24)); // development arrows ship as 24px PNGs
        // Einheitliche App-weite Selektionsfarbe (Blau/Gelb, wie C#-Referenz);
        // derselbe Helper wird von OverviewTabWidget für alle Edit-Dialoge und
        // ShareDetailsForm verwendet, siehe GridStyle.h.
        GridStyle::applySelectionStyle(tbl);
    };

    auto setupFooter = [](QTableWidget* tbl) {
        tbl->horizontalHeader()->setVisible(false);
        tbl->verticalHeader()->setVisible(false);
        tbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
        tbl->setSelectionMode(QAbstractItemView::NoSelection);
        tbl->setFocusPolicy(Qt::NoFocus);
        tbl->setShowGrid(true);
        tbl->setAlternatingRowColors(true);
        tbl->setIconSize(QSize(24, 24));
        // Fixed 3-row footer: never scroll.
        tbl->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        tbl->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        // Rows are 34px each (set in updatePortfolioFooters); fit them exactly
        // plus the frame so no vertical scrollbar is needed.
        tbl->setFixedHeight(34 * 3 + 2 * tbl->frameWidth());
    };

    // ── Depotwert-Tab ─────────────────────────────────────────────────────
    m_finalValueTable = new QTableWidget(0, static_cast<int>(FinalValueColumn::Count));
    m_finalValueTable->setHorizontalHeaderLabels({
        QStringLiteral(""),          // Icon
        tr("WKN"),
        tr("Name"),
        tr("Anteile"),
        tr("Kosten /\nDividenden"),
        tr("Preis"),
        QStringLiteral(""),          // PrevDay chart icon
        tr("Vortag"),
        tr("Aktuelle\nEntwicklung"),
        tr("Einzahlung\nMarktwert"),
        QStringLiteral(""),          // Complete chart icon
        tr("Komplette\nEntwicklung"),
        tr("Kpl. Einzahlung\nKpl. Marktwert")
    });
    setupTable(m_finalValueTable);
    m_finalValueTable->horizontalHeader()->setStretchLastSection(false);
    m_finalValueTable->horizontalHeader()->setSectionResizeMode(
        static_cast<int>(FinalValueColumn::Name), QHeaderView::Stretch);

    // Icon columns — fixed width
    m_finalValueTable->setColumnWidth(static_cast<int>(FinalValueColumn::Icon),         28);
    m_finalValueTable->setColumnWidth(static_cast<int>(FinalValueColumn::PrevDayChart), 32);
    m_finalValueTable->setColumnWidth(static_cast<int>(FinalValueColumn::CompleteChart),32);

    m_finalValueFooter = new QTableWidget(3, static_cast<int>(FinalValueColumn::Count));
    setupFooter(m_finalValueFooter);
    // Mirror column widths from main table — done after population

    auto* finalContainer = new QWidget();
    auto* finalLayout    = new QVBoxLayout(finalContainer);
    finalLayout->setContentsMargins(0, 0, 0, 0);
    finalLayout->setSpacing(0);
    finalLayout->addWidget(m_finalValueTable, 1);
    finalLayout->addWidget(m_finalValueFooter, 0);

    // ── Marktwert-Tab ─────────────────────────────────────────────────────
    m_marketValueTable = new QTableWidget(0, static_cast<int>(MarketValueColumn::Count));
    m_marketValueTable->setHorizontalHeaderLabels({
        QStringLiteral(""),          // Icon
        tr("WKN"),
        tr("Name"),
        tr("Anteile"),
        tr("Preis"),
        QStringLiteral(""),          // PrevDay chart icon
        tr("Vortag"),
        tr("Aktuelle\nEntwicklung"),
        tr("Einzahlung\nMarktwert"),
        QStringLiteral(""),          // Complete chart icon
        tr("Komplette\nEntwicklung"),
        tr("Kpl. Einzahlung\nKpl. Marktwert")
    });
    setupTable(m_marketValueTable);
    m_marketValueTable->horizontalHeader()->setStretchLastSection(false);
    m_marketValueTable->horizontalHeader()->setSectionResizeMode(
        static_cast<int>(MarketValueColumn::Name), QHeaderView::Stretch);

    m_marketValueTable->setColumnWidth(static_cast<int>(MarketValueColumn::Icon),         28);
    m_marketValueTable->setColumnWidth(static_cast<int>(MarketValueColumn::PrevDayChart), 32);
    m_marketValueTable->setColumnWidth(static_cast<int>(MarketValueColumn::CompleteChart),32);

    m_marketValueFooter = new QTableWidget(3, static_cast<int>(MarketValueColumn::Count));
    setupFooter(m_marketValueFooter);

    auto* marketContainer = new QWidget();
    auto* marketLayout    = new QVBoxLayout(marketContainer);
    marketLayout->setContentsMargins(0, 0, 0, 0);
    marketLayout->setSpacing(0);
    marketLayout->addWidget(m_marketValueTable, 1);
    marketLayout->addWidget(m_marketValueFooter, 0);

    m_portfolioTabs->addTab(finalContainer,  tr("Kompletter Depotwert"));
    m_portfolioTabs->addTab(marketContainer, tr("Kompletter Marktwert"));

    // Install two-line delegates
    setupTableDelegates();

    // Keep each footer's column widths in sync with its grid continuously.
    // The grid's "Name" column is stretched and only reaches its real width
    // during layout (after the first show / on every resize). A one-off mirror
    // at data-update time would capture a stale (default) width and shift all
    // footer value columns left, so they would no longer sit under the grid
    // columns. sectionResized fires whenever a column (incl. the stretched one)
    // changes width, keeping the footer aligned.
    connect(m_finalValueTable->horizontalHeader(), &QHeaderView::sectionResized,
            this, [this](int idx, int /*oldSize*/, int newSize) {
                m_finalValueFooter->setColumnWidth(idx, newSize);
            });
    connect(m_marketValueTable->horizontalHeader(), &QHeaderView::sectionResized,
            this, [this](int idx, int /*oldSize*/, int newSize) {
                m_marketValueFooter->setColumnWidth(idx, newSize);
            });

    // Enable Edit / Delete / Refresh when a row is selected
    auto enableShareActions = [this]() {
        // While a refresh is running (single share or "Alle aktualisieren"),
        // Edit/Delete/Refresh must stay disabled — selectShareRow() moves the
        // table selection programmatically to follow the share currently
        // being updated, which would otherwise re-enable these actions via
        // this same selectionChanged handler.
        //
        // Bugfix 07.07.2026: m_refreshInProgress is checked IN ADDITION to
        // Parser::isBusy(), not instead of it — startRefreshForShare() calls
        // selectShareRow() before either Parser's startParsing(), so on a
        // table's first-ever selection, isBusy() can still be false at the
        // exact moment selectShareRow() fires selectionChanged(). Relying on
        // isBusy() alone let that one selectionChanged() through and
        // re-enabled the actions mid-refresh.
        if (m_refreshInProgress ||
            m_parserMarketValues.isBusy() || m_parserDailyValues.isBusy())
            return;

        const bool hasSelection =
            m_finalValueTable->selectionModel()->hasSelection() ||
            m_marketValueTable->selectionModel()->hasSelection();
        m_actionEdit->setEnabled(hasSelection);
        m_actionDelete->setEnabled(hasSelection);
        m_actionRefresh->setEnabled(hasSelection);
    };
    connect(m_finalValueTable->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this, enableShareActions);
    connect(m_marketValueTable->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this, enableShareActions);

    // Double-click on a portfolio row opens the read-only share-details dialog
    // (ShareDetailsForm) — independent of which of the two tabs is active.
    connect(m_finalValueTable, &QTableWidget::itemDoubleClicked,
            this, &MainWindow::onPortfolioRowDoubleClicked);
    connect(m_marketValueTable, &QTableWidget::itemDoubleClicked,
            this, &MainWindow::onPortfolioRowDoubleClicked);

    // Rechtsklick auf eine Portfolio-Zeile öffnet stattdessen das rahmenlose
    // ChartPopup (nur Graph + Legende, siehe ARCHITECTURE.md, "ChartPopup —
    // Rechtsklick-Popup-Chart", Feature 31.07.2026). Qt::CustomContextMenu
    // wird bewusst zweckentfremdet, um Rechtsklicks abzufangen, ohne ein
    // natives Kontextmenü zu zeigen.
    m_finalValueTable->setContextMenuPolicy(Qt::CustomContextMenu);
    m_marketValueTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_finalValueTable, &QTableWidget::customContextMenuRequested,
            this, &MainWindow::onPortfolioRowRightClicked);
    connect(m_marketValueTable, &QTableWidget::customContextMenuRequested,
            this, &MainWindow::onPortfolioRowRightClicked);

    // ── Main layout: portfolio (expands) + bottom panel (fixed) ──────────
    auto* centralWidget = new QWidget(this);
    auto* mainLayout    = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // Top area — label + tabs — takes all available space
    auto* topWidget = new QWidget();
    auto* topLayout = new QVBoxLayout(topWidget);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(2);
    // Indent the portfolio label slightly from the left edge
    m_portfolioLabel->setContentsMargins(6, 2, 0, 2);
    topLayout->addWidget(m_portfolioLabel);
    topLayout->addWidget(m_portfolioTabs);

    mainLayout->addWidget(topWidget, 1);   // stretch = 1 → takes all remaining space

    // ── Bottom panel — left: Status-Meldungen ─────────────────────────────
    m_statusMessageGroup = new QGroupBox(tr("  Status-Meldungen"));
    m_statusMessageText  = new QTextEdit();
    m_statusMessageText->setReadOnly(true);
    m_statusMessageText->setAcceptRichText(true);
    // Transparent background so theme colors don't override text colors
    m_statusMessageText->setStyleSheet(
        QStringLiteral("QTextEdit { background-color: transparent; border: none; }"));
    auto* statusLayout = new QVBoxLayout(m_statusMessageGroup);
    statusLayout->setContentsMargins(8, 8, 4, 4);
    statusLayout->addWidget(m_statusMessageText);

    // ── Bottom panel — right: Document capture (compact) ──────────────────
    m_documentCaptureGroup = new QGroupBox(tr("  Direkte Dokumentenerfassung"));
    m_documentCaptureEdit  = new QLineEdit();
    m_documentCaptureEdit->setReadOnly(true);
    m_documentCaptureEdit->setPlaceholderText(
        tr("PDF-Dokument hier ablegen (Kauf/Verkauf/Dividende)"));
    // Explizit deaktiviert: ein QLineEdit kann eigene Drop-Handhabung mitbringen,
    // die das Event abfangen würde, bevor es (per Qt-Default-Bubbling an den
    // nächsten Vorfahren mit acceptDrops == true) bei m_documentCaptureGroup
    // ankommt — siehe eventFilter() weiter unten.
    m_documentCaptureEdit->setAcceptDrops(false);
    auto* captureLayout = new QVBoxLayout(m_documentCaptureGroup);
    captureLayout->setContentsMargins(8, 8, 4, 4);
    captureLayout->addWidget(m_documentCaptureEdit);
    m_documentCaptureGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    // Direkte Dokumentenerfassung per Drag+Drop (Feature 27.07.2026): Drops
    // werden bewusst nur auf dieser einen Gruppe akzeptiert (nicht auf dem
    // gesamten Fenster) — siehe eventFilter() und ARCHITECTURE.md.
    m_documentCaptureGroup->setAcceptDrops(true);
    m_documentCaptureGroup->installEventFilter(this);
    connect(&m_documentCaptureExtractor, &PdfTextExtractor::finished,
            this, &MainWindow::onDocumentCaptureTextExtracted);

    // ── Bottom panel — right: Aktualisierungs-Status ──────────────────────
    m_updateStateGroup      = new QGroupBox(tr("  Aktualisierungs-Status"));
    m_marketValueStateLabel = new QLabel(tr("Marktwert:"));
    m_marketValueProgress   = new QProgressBar();
    m_marketValueProgress->setRange(0, 100);
    m_marketValueProgress->setValue(0);
    m_marketValueProgress->setMinimumHeight(24);
    m_dailyValuesStateLabel = new QLabel(tr("Tageswerte:"));
    m_dailyValuesProgress   = new QProgressBar();
    m_dailyValuesProgress->setRange(0, 100);
    m_dailyValuesProgress->setValue(0);
    m_dailyValuesProgress->setMinimumHeight(24);

    auto* updateLayout = new QVBoxLayout(m_updateStateGroup);
    updateLayout->setContentsMargins(8, 8, 4, 4);
    updateLayout->setSpacing(6);
    updateLayout->addWidget(m_marketValueStateLabel);
    updateLayout->addWidget(m_marketValueProgress);
    updateLayout->addWidget(m_dailyValuesStateLabel);
    updateLayout->addWidget(m_dailyValuesProgress);

    // Right column: document capture (fixed height) on top,
    // update state expands to fill remaining space — flush with left groupbox bottom
    auto* rightWidget = new QWidget();
    auto* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(4);
    rightLayout->addWidget(m_documentCaptureGroup, 0); // fixed height
    rightLayout->addWidget(m_updateStateGroup,     1); // expands to fill remaining

    // Bottom row: status messages left (~75%) + right column (~25%)
    auto* bottomWidget = new QWidget();
    bottomWidget->setFixedHeight(200);
    auto* bottomLayout = new QHBoxLayout(bottomWidget);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(4);
    bottomLayout->addWidget(m_statusMessageGroup, 3);
    bottomLayout->addWidget(rightWidget, 1);

    mainLayout->addWidget(bottomWidget, 0);  // stretch = 0 → fixed height

    setCentralWidget(centralWidget);
}

// ── setupStatusBar ────────────────────────────────────────────────────────────

void MainWindow::setupStatusBar()
{
    m_statusLabel = new QLabel(tr("Bereit"), this);
    statusBar()->addWidget(m_statusLabel, 1);

    m_portfolioPathLabel = new QLabel(this);
    m_portfolioPathLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_portfolioPathLabel->setContentsMargins(0, 0, 6, 0);
    statusBar()->addPermanentWidget(m_portfolioPathLabel);
    updateStatusBarPortfolio(QString());
}

// ── restoreWindowGeometry ─────────────────────────────────────────────────────

void MainWindow::restoreWindowGeometry()
{
    const auto& settings = AppSettings::instance();
    if (settings.windowSize().isValid())
        resize(settings.windowSize());
    if (!settings.windowPos().isNull())
        move(settings.windowPos());
    if (settings.windowState() == QStringLiteral("Maximized"))
        showMaximized();
}

// ── Configuration loading ─────────────────────────────────────────────────────

bool MainWindow::checkAndLoadConfigurations()
{
    bool allOk = true;
    const QString appDir = QCoreApplication::applicationDirPath();

    // ── settings.ini ──────────────────────────────────────────────────────
    // Already loaded by AppStartup — just verify it is accessible.
    //
    // Bugfix (24.07.2026): Fehlende settings.ini war bis hierhin ein
    // FatalError, der über allOk=false disableAllControls() auslöste — die
    // komplette App war dann bis auf "Beenden" gesperrt. Das traf jede
    // frische Installation (z. B. Linux-AppImage), weil settings.ini nur
    // durch einen tatsächlichen AppSettings::save()-Aufruf entsteht und der
    // Installer bewusst keine Vorlage mitliefert (siehe ARCHITECTURE.md,
    // "Erstlauf ohne settings.ini"). AppSettings::load() kommt mit fehlender
    // Datei aber längst klar — es verwendet dann einfach die in AppSettings.h
    // einprogrammierten Member-Defaults, mit denen die App voll funktionsfähig
    // ist (leeres Portfolio/Dokumente-Root führt zu den üblichen, nicht
    // fatalen Erstlauf-Hinweisen weiter unten). Fehlende settings.ini ist
    // daher nur eine Warnung, kein Grund, allOk auf false zu setzen.
    const QString settingsPath = AppSettings::instance().settingsPath();
    if (!QFileInfo::exists(settingsPath)) {
        addStatusMessage(tr("Einstellungsdatei nicht gefunden — Standardwerte werden verwendet."),
                         MessageType::Warning);
    } else {
        addStatusMessage(tr("Einstellungen geladen."),
                         MessageType::Success);
    }

    // ── WebSites.xml ──────────────────────────────────────────────────────
    const QString webSitesPath = appDir + QStringLiteral("/WebSites.xml");
    const auto webResult = m_webSitesConfig.load(webSitesPath);

    if (webResult == WebSitesConfig::LoadResult::Success) {
        addStatusMessage(tr("WebSites-Konfiguration geladen: %1 Einträge.")
                             .arg(m_webSitesConfig.count()),
                         MessageType::Success);
    } else {
        addStatusMessage(tr("WebSites-Konfiguration konnte nicht geladen werden: %1")
                             .arg(m_webSitesConfig.lastError()),
                         webResult == WebSitesConfig::LoadResult::FileNotFound
                             ? MessageType::FatalError
                             : MessageType::Error);
        allOk = false;
    }

    // ── Documents.xml ─────────────────────────────────────────────────────
    const QString documentsPath = appDir + QStringLiteral("/Documents.xml");
    const auto docResult = m_documentsConfig.load(documentsPath);

    if (docResult == DocumentsConfig::LoadResult::Success) {
        addStatusMessage(tr("Dokument-Konfiguration geladen: %1 Bank(en).")
                             .arg(m_documentsConfig.count()),
                         MessageType::Success);
    } else {
        addStatusMessage(tr("Dokument-Konfiguration konnte nicht geladen werden: %1")
                             .arg(m_documentsConfig.lastError()),
                         docResult == DocumentsConfig::LoadResult::FileNotFound
                             ? MessageType::FatalError
                             : MessageType::Error);
        allOk = false;
    }

    if (!allOk) {
        addStatusMessage(tr("Kritische Konfigurationsfehler — bitte Konfigurationsdateien prüfen."),
                         MessageType::FatalError);
        disableAllControls();
    }

    // ── Sound files (non-critical — warn but don't disable controls) ──────
    const QString soundsDir = appDir + QStringLiteral("/sounds");
    const QString updateSoundFile = soundsDir + QStringLiteral("/")
                                  + AppSettings::instance().soundUpdateFile();
    const QString errorSoundFile  = soundsDir + QStringLiteral("/")
                                  + AppSettings::instance().soundErrorFile();

    if (!QFileInfo::exists(updateSoundFile)) {
        addStatusMessage(tr("Update-Sound nicht gefunden: %1 — Sound deaktiviert.")
                             .arg(AppSettings::instance().soundUpdateFile()),
                         MessageType::Warning);
        AppSettings::instance().setSoundUpdateEnabled(false);
    }

    if (!QFileInfo::exists(errorSoundFile)) {
        addStatusMessage(tr("Fehler-Sound nicht gefunden: %1 — Sound deaktiviert.")
                             .arg(AppSettings::instance().soundErrorFile()),
                         MessageType::Warning);
        AppSettings::instance().setSoundErrorEnabled(false);
    }

    return allOk;
}

// ── ensureDocumentsRootConfigured ───────────────────────────────────────────────

void MainWindow::ensureDocumentsRootConfigured()
{
    if (!AppSettings::instance().documentsRootPath().isEmpty())
        return;

    // Nicht mehr zwingend — bricht der Benutzer ab, bleibt der Root-Pfad
    // leer und der Dialog wird beim nächsten Start erneut angeboten.
    DocumentsSettingsForm dialog(this);
    dialog.exec();
}

void MainWindow::disableAllControls()
{
    // Disable all toolbar actions
    m_actionNew->setEnabled(false);
    m_actionOpen->setEnabled(false);
    m_actionSaveAs->setEnabled(false);
    m_actionAdd->setEnabled(false);
    m_actionEdit->setEnabled(false);
    m_actionDelete->setEnabled(false);
    m_actionRefresh->setEnabled(false);
    m_actionRefreshAll->setEnabled(false);

    // Disable settings and API menus
    m_actionLanguage->setEnabled(false);
    m_actionLogger->setEnabled(false);
    m_actionSound->setEnabled(false);
    m_actionBackup->setEnabled(false);
    m_actionDocuments->setEnabled(false);
    m_actionApiKeyYahoo->setEnabled(false);

    // Disable portfolio tabs
    m_portfolioTabs->setEnabled(false);

    // Only Quit remains enabled — already connected and always active
    addStatusMessage(tr("Nur Beenden ist verfügbar."), MessageType::Warning);
}

void MainWindow::addStatusMessage(const QString& message, MessageType type)
{
    // Map MessageType to AppSettings color index:
    // 0=Start, 1=Info, 2=Warning, 3=Error, 4=FatalError, 5=Success
    int colorIndex = 1;
    switch (type) {
    case MessageType::Info:       colorIndex = 1; break;
    case MessageType::Success:    colorIndex = 5; break;
    case MessageType::Warning:    colorIndex = 2; break;
    case MessageType::Error:      colorIndex = 3; break;
    case MessageType::FatalError: colorIndex = 4; break;
    default:                      colorIndex = 1; break;
    }

    const QColor color = AppSettings::instance().logColorAt(colorIndex);
    const QString colorName = color.name();
    const QString timestamp = QTime::currentTime().toString(QStringLiteral("hh:mm:ss"));

    // Move cursor to end and insert colored HTML.
    // insertHtml() is used instead of append() because append() wraps each
    // call in its own paragraph block which resets inline color styles under
    // some Qt themes. insertHtml() with moveCursor(End) inserts the HTML
    // directly at the current position and reliably preserves inline styles.
    m_statusMessageText->moveCursor(QTextCursor::End);
    if (!m_statusMessageText->document()->isEmpty())
        m_statusMessageText->insertHtml(QStringLiteral("<br>"));
    m_statusMessageText->insertHtml(
        QStringLiteral("<span style=\"color:%1;\">%2 %3</span>")
            .arg(colorName,
                 timestamp.toHtmlEscaped(),
                 message.toHtmlEscaped()));
    m_statusMessageText->moveCursor(QTextCursor::End);
}


QString MainWindow::formatLastPortfolioUpdate() const
{
    ShareRepository shareRepo;
    const QString iso = shareRepo.maxLastInternetUpdate();
    if (iso.isEmpty())
        return tr("-");

    const QDateTime dt = QDateTime::fromString(iso, Qt::ISODate);
    if (!dt.isValid())
        return iso; // defensiver Fallback, analog zu formatDateTime() in PresenterShareDetails

    const QLocale locale;
    return locale.toString(dt, QLocale::ShortFormat);
}

void MainWindow::updatePortfolioLabel(int entryCount, const QString& lastUpdate)
{
    m_portfolioLabel->setText(
        tr("Portfolio-Übersicht ( Einträge: %1 ) / Letzte Aktualisierung: %2")
            .arg(entryCount)
            .arg(lastUpdate));
}

void MainWindow::clearPortfolioTables()
{
    m_finalValueTable->setRowCount(0);
    m_marketValueTable->setRowCount(0);
    // Clear footer contents but keep row structure
    for (int r = 0; r < m_finalValueFooter->rowCount(); ++r)
        for (int c = 0; c < m_finalValueFooter->columnCount(); ++c)
            if (auto* it = m_finalValueFooter->item(r, c)) it->setText(QString());
    for (int r = 0; r < m_marketValueFooter->rowCount(); ++r)
        for (int c = 0; c < m_marketValueFooter->columnCount(); ++c)
            if (auto* it = m_marketValueFooter->item(r, c)) it->setText(QString());
}

QString MainWindow::baseWindowTitle() const
{
    return tr("Share Portfolio Manager (Version %1)")
        .arg(QCoreApplication::applicationVersion());
}

// ── updateStatusBarPortfolio ──────────────────────────────────────────────────

void MainWindow::updateStatusBarPortfolio(const QString& portfolioPath)
{
    if (portfolioPath.isEmpty())
        m_portfolioPathLabel->setText(tr("Kein Portfolio geladen"));
    else
        m_portfolioPathLabel->setText(tr("Portfolio: %1").arg(portfolioPath));
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void MainWindow::onNewPortfolio()
{
    // Let the user choose where to save the new portfolio
    const QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Neues Portfolio anlegen"),
        AppSettings::instance().portfolioPath(),
        tr("Portfolio-Datenbank (*.db);;Alle Dateien (*)"));

    if (filePath.isEmpty())
        return; // User cancelled

    // Close existing database if open
    if (Database::instance().isOpen()) {
        Database::instance().close();
        qInfo() << "[MainWindow] Closed current portfolio.";
    }

    // Open (create) the new database
    if (!Database::instance().open(filePath)) {
        OwnMessageBox::critical(this,
            tr("Fehler"),
            tr("Das neue Portfolio konnte nicht angelegt werden:\n\n%1").arg(filePath));
        qCritical() << "[MainWindow] Failed to create new portfolio:" << filePath;
        return;
    }

    // Persist the new path
    AppSettings::instance().setPortfolioPath(filePath);

    // Update UI
    clearPortfolioTables();
    updatePortfolioLabel(0);
    updateStatusBarPortfolio(filePath);

    // New empty portfolio: only Add is enabled — nothing to update yet
    m_actionAdd->setEnabled(true);
    m_actionRefreshAll->setEnabled(false);
    m_actionRefresh->setEnabled(false);
    m_actionEdit->setEnabled(false);
    m_actionDelete->setEnabled(false);

    addStatusMessage(tr("Neues Portfolio angelegt: %1").arg(filePath),
                     MessageType::Success);

    qInfo() << "[MainWindow] New portfolio created:" << filePath;
}

void MainWindow::onOpenPortfolio()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Portfolio öffnen"),
        AppSettings::instance().portfolioPath(),
        tr("Portfolio-Datenbank (*.db);;Alle Dateien (*)"));

    if (filePath.isEmpty())
        return; // User cancelled

    // Close existing database if open
    if (Database::instance().isOpen()) {
        Database::instance().close();
        qInfo() << "[MainWindow] Closed current portfolio.";
    }

    // Open the selected database
    if (!Database::instance().open(filePath)) {
        OwnMessageBox::critical(this,
            tr("Fehler"),
            tr("Das Portfolio konnte nicht geöffnet werden:\n\n%1").arg(filePath));
        qCritical() << "[MainWindow] Failed to open portfolio:" << filePath;
        return;
    }

    // Create a timestamped backup of the opened portfolio
    createBackup(filePath);

    // Persist the new path
    AppSettings::instance().setPortfolioPath(filePath);

    // Update UI
    updateStatusBarPortfolio(filePath);
    addStatusMessage(tr("Portfolio geöffnet: %1").arg(filePath),
                     MessageType::Success);
    populatePortfolioTables();

    qInfo() << "[MainWindow] Portfolio opened:" << filePath;
}

void MainWindow::populatePortfolioTables()
{
    clearPortfolioTables();

    ShareRepository shareRepo;
    const QList<ShareObject> shares = shareRepo.findAll();
    const int shareCount = shares.size();

    const QLocale locale;

    // Helper: create a colored two-line item
    auto makeTwoLine = [&](const QString& top,    const QColor& topColor,
                           const QString& bottom, const QColor& bottomColor) {
        auto* item = new QTableWidgetItem();
        item->setData(TwoLineRole::Top,         top);
        item->setData(TwoLineRole::Bottom,       bottom);
        item->setData(TwoLineRole::TopColor,     topColor);
        item->setData(TwoLineRole::BottomColor,  bottomColor);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        return item;
    };

    // Helper: performance color — same green/red as the status message box
    // (AppSettings log colors: 5 = success green, 3 = error red).
    auto perfColor = [this](double value) -> QColor {
        if (value >  0.0) return AppSettings::instance().logColorAt(5);
        if (value <  0.0) return AppSettings::instance().logColorAt(3);
        return palette().color(QPalette::Text);          // neutral (theme-aware)
    };

    // Helper: pick development icon
    auto devIcon = [](double pct) -> QIcon {
        if (pct >  2.0)  return IconProvider::icon(IconProvider::PositivStrong);
        if (pct >  0.0)  return IconProvider::icon(IconProvider::PositivNormal);
        if (pct <  -2.0) return IconProvider::icon(IconProvider::NegativStrong);
        if (pct <  0.0)  return IconProvider::icon(IconProvider::NegativNormal);
        return IconProvider::icon(IconProvider::Neutral);
    };

    // Helper: update state icon from ShareObject
    auto stateIcon = [](const ShareObject& s) -> QIcon {
        using T = ShareUpdateType;
        switch (s.updateType()) {
        case T::Both:        return IconProvider::icon(IconProvider::StateUpdateBoth);
        case T::MarketPrice: return IconProvider::icon(IconProvider::StateUpdateMarket);
        case T::DailyValues: return IconProvider::icon(IconProvider::StateUpdateDaily);
        default:             return IconProvider::icon(IconProvider::StateNoUpdate);
        }
    };

    QList<ShareValues> allValues;
    allValues.reserve(shareCount);

    for (const ShareObject& share : shares) {
        const ShareValues v = ShareCalculator::compute(
            share.guid(), share.curPrice(), share.prevDayPrice());
        allValues.append(v);

        const QColor neutral = palette().color(QPalette::Text);
        QColor muted = neutral;
        muted.setAlpha(140); // dimmed toward background, works in light + dark

        // ── Formatted strings ─────────────────────────────────────────────
        const QString curPriceStr   = locale.toString(v.curPrice,     'f', 4)
                                    + QStringLiteral(" €");
        const QString prevPriceStr  = locale.toString(v.prevDayPrice, 'f', 4)
                                    + QStringLiteral(" €");
        const QString prevDiffStr   = (v.prevDayDiff >= 0 ? QStringLiteral("+") : QString())
                                    + locale.toString(v.prevDayDiff, 'f', 2)
                                    + QStringLiteral(" €");
        const QString prevPctStr    = (v.prevDayPct >= 0 ? QStringLiteral("+") : QString())
                                    + locale.toString(v.prevDayPct, 'f', 2)
                                    + QStringLiteral(" %");
        const QString purchaseStr   = locale.toString(v.purchaseValue, 'f', 2)
                                    + QStringLiteral(" €");
        const QString curValueStr   = locale.toString(v.curValue, 'f', 2)
                                    + QStringLiteral(" €");
        const QString volumeStr     = locale.toString(v.volume, 'f', 2);
        const QString brokerDivStr  = locale.toString(v.totalBrokerage, 'f', 2)
                                    + QStringLiteral(" €");
        const QString dividendStr   = locale.toString(v.totalDividend, 'f', 2)
                                    + QStringLiteral(" €");
        const QString cProfitStr    = locale.toString(v.completeProfitLoss, 'f', 2)
                                    + QStringLiteral(" €");
        const QString cProfitPctStr = locale.toString(v.completeProfitPct, 'f', 2)
                                    + QStringLiteral(" %");
        const QString cPurchaseStr  = locale.toString(v.completePurchase, 'f', 2)
                                    + QStringLiteral(" €");
        const QString cCurValueStr  = locale.toString(v.completeCurValue, 'f', 2)
                                    + QStringLiteral(" €");

        // ── Depotwert-Tab ──────────────────────────────────────────────────
        const int fr = m_finalValueTable->rowCount();
        m_finalValueTable->insertRow(fr);
        m_finalValueTable->setRowHeight(fr, 38);

        // Icon
        auto* iconItemF = new QTableWidgetItem();
        iconItemF->setIcon(stateIcon(share));
        iconItemF->setData(Qt::UserRole, share.guid());
        iconItemF->setFlags(iconItemF->flags() & ~Qt::ItemIsEditable);

        // WKN
        auto* wknItemF = new QTableWidgetItem(share.wkn());
        wknItemF->setData(Qt::UserRole, share.guid());
        wknItemF->setTextAlignment(Qt::AlignCenter);

        // Name
        auto* nameItemF = new QTableWidgetItem(share.name());

        // Volume
        auto* volItemF = new QTableWidgetItem(volumeStr);
        volItemF->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        // Kosten / Dividenden (2-line)
        auto* bdItemF = makeTwoLine(brokerDivStr, neutral,
                                    dividendStr,  neutral);

        // Preis (2-line)
        auto* priceItemF = makeTwoLine(curPriceStr,  neutral,
                                       prevPriceStr, neutral);

        // PrevDay chart icon
        auto* prevChartItemF = new QTableWidgetItem();
        prevChartItemF->setIcon(devIcon(v.prevDayPct));
        prevChartItemF->setFlags(prevChartItemF->flags() & ~Qt::ItemIsEditable);

        // Vortag (2-line, colored)
        auto* prevDayItemF = makeTwoLine(prevDiffStr, perfColor(v.prevDayDiff),
                                         prevPctStr,  perfColor(v.prevDayPct));

        // Depotwert-specific strings (WITH brokerage)
        const QString profitFinalStr    = locale.toString(v.profitLossFinal, 'f', 2)
                                        + QStringLiteral(" €");
        const QString profitFinalPctStr = locale.toString(v.profitLossPctFinal, 'f', 2)
                                        + QStringLiteral(" %");
        const QString purchaseFinalStr  = locale.toString(v.purchaseValueFinal, 'f', 2)
                                        + QStringLiteral(" €");

        // Aktuelle Entwicklung (2-line, colored)
        auto* perfItemF = makeTwoLine(profitFinalStr,    perfColor(v.profitLossFinal),
                                      profitFinalPctStr, perfColor(v.profitLossPctFinal));

        // Einzahlung / Marktwert (2-line) — Einzahlung WITH brokerage, Marktwert = curValue
        auto* pvItemF = makeTwoLine(purchaseFinalStr, neutral,
                                    curValueStr, neutral);

        // Complete chart icon
        auto* cChartItemF = new QTableWidgetItem();
        cChartItemF->setIcon(devIcon(v.completeProfitPct));
        cChartItemF->setFlags(cChartItemF->flags() & ~Qt::ItemIsEditable);

        // Komplette Entwicklung (2-line, colored)
        auto* cPerfItemF = makeTwoLine(cProfitStr,    perfColor(v.completeProfitLoss),
                                       cProfitPctStr, perfColor(v.completeProfitPct));

        // Kpl. Einzahlung / Kpl. Marktwert (2-line)
        auto* cPvItemF = makeTwoLine(cPurchaseStr, neutral,
                                     cCurValueStr, neutral);

        using FC = FinalValueColumn;
        m_finalValueTable->setItem(fr, static_cast<int>(FC::Icon),                     iconItemF);
        m_finalValueTable->setItem(fr, static_cast<int>(FC::Wkn),                      wknItemF);
        m_finalValueTable->setItem(fr, static_cast<int>(FC::Name),                     nameItemF);
        m_finalValueTable->setItem(fr, static_cast<int>(FC::Volume),                   volItemF);
        m_finalValueTable->setItem(fr, static_cast<int>(FC::BrokerageDividend),        bdItemF);
        m_finalValueTable->setItem(fr, static_cast<int>(FC::Price),                    priceItemF);
        m_finalValueTable->setItem(fr, static_cast<int>(FC::PrevDayChart),             prevChartItemF);
        m_finalValueTable->setItem(fr, static_cast<int>(FC::PrevDay),                  prevDayItemF);
        m_finalValueTable->setItem(fr, static_cast<int>(FC::Performance),              perfItemF);
        m_finalValueTable->setItem(fr, static_cast<int>(FC::PurchaseFinalValue),       pvItemF);
        m_finalValueTable->setItem(fr, static_cast<int>(FC::CompleteChart),            cChartItemF);
        m_finalValueTable->setItem(fr, static_cast<int>(FC::CompletePerformance),      cPerfItemF);
        m_finalValueTable->setItem(fr, static_cast<int>(FC::CompletePurchaseFinalValue), cPvItemF);

        // ── Marktwert-Tab ──────────────────────────────────────────────────
        const int mr = m_marketValueTable->rowCount();
        m_marketValueTable->insertRow(mr);
        m_marketValueTable->setRowHeight(mr, 38);

        auto* iconItemM = new QTableWidgetItem();
        iconItemM->setIcon(stateIcon(share));
        iconItemM->setData(Qt::UserRole, share.guid());
        iconItemM->setFlags(iconItemM->flags() & ~Qt::ItemIsEditable);

        auto* wknItemM = new QTableWidgetItem(share.wkn());
        wknItemM->setData(Qt::UserRole, share.guid());
        wknItemM->setTextAlignment(Qt::AlignCenter);

        auto* nameItemM  = new QTableWidgetItem(share.name());
        auto* volItemM   = new QTableWidgetItem(volumeStr);
        volItemM->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        // Marktwert-specific strings
        const QString profitMStr     = locale.toString(v.profitLoss, 'f', 2)
                                     + QStringLiteral(" €");
        const QString profitMPctStr  = locale.toString(v.profitLossPct, 'f', 2)
                                     + QStringLiteral(" %");

        auto* priceItemM = makeTwoLine(curPriceStr, neutral, prevPriceStr, neutral);
        auto* prevChartItemM = new QTableWidgetItem();
        prevChartItemM->setIcon(devIcon(v.prevDayPct));
        prevChartItemM->setFlags(prevChartItemM->flags() & ~Qt::ItemIsEditable);
        auto* prevDayItemM   = makeTwoLine(prevDiffStr, perfColor(v.prevDayDiff),
                                           prevPctStr,  perfColor(v.prevDayPct));
        // Aktuelle Entwicklung: profitLoss per buy (no brokerage)
        auto* perfItemM      = makeTwoLine(profitMStr,    perfColor(v.profitLoss),
                                           profitMPctStr, perfColor(v.profitLossPct));
        // Einzahlung (upper) = purchaseValue, Marktwert (lower) = reiner Marktwert (curValue)
        auto* pvItemM        = makeTwoLine(purchaseStr, neutral, curValueStr, neutral);

        // Marktwert complete columns (brokerage-free)
        const QString cProfitMStr    = locale.toString(v.completeProfitLossMarket, 'f', 2)
                                     + QStringLiteral(" €");
        const QString cProfitMPctStr = locale.toString(v.completeProfitPctMarket, 'f', 2)
                                     + QStringLiteral(" %");
        const QString cPurchaseMStr  = locale.toString(v.completePurchaseMarket, 'f', 2)
                                     + QStringLiteral(" €");
        const QString cCurValueMStr  = locale.toString(v.completeCurValueMarket, 'f', 2)
                                     + QStringLiteral(" €");

        auto* cChartItemM = new QTableWidgetItem();
        cChartItemM->setIcon(devIcon(v.completeProfitPctMarket));
        cChartItemM->setFlags(cChartItemM->flags() & ~Qt::ItemIsEditable);
        auto* cPerfItemM  = makeTwoLine(cProfitMStr,    perfColor(v.completeProfitLossMarket),
                                        cProfitMPctStr, perfColor(v.completeProfitPctMarket));
        auto* cPvItemM    = makeTwoLine(cPurchaseMStr, neutral, cCurValueMStr, neutral);

        using MC = MarketValueColumn;
        m_marketValueTable->setItem(mr, static_cast<int>(MC::Icon),                iconItemM);
        m_marketValueTable->setItem(mr, static_cast<int>(MC::Wkn),                 wknItemM);
        m_marketValueTable->setItem(mr, static_cast<int>(MC::Name),                nameItemM);
        m_marketValueTable->setItem(mr, static_cast<int>(MC::Volume),              volItemM);
        m_marketValueTable->setItem(mr, static_cast<int>(MC::Price),               priceItemM);
        m_marketValueTable->setItem(mr, static_cast<int>(MC::PrevDayChart),        prevChartItemM);
        m_marketValueTable->setItem(mr, static_cast<int>(MC::PrevDay),             prevDayItemM);
        m_marketValueTable->setItem(mr, static_cast<int>(MC::Performance),         perfItemM);
        m_marketValueTable->setItem(mr, static_cast<int>(MC::PurchaseMarketValue), pvItemM);
        m_marketValueTable->setItem(mr, static_cast<int>(MC::CompleteChart),               cChartItemM);
        m_marketValueTable->setItem(mr, static_cast<int>(MC::CompletePerformance),         cPerfItemM);
        m_marketValueTable->setItem(mr, static_cast<int>(MC::CompletePurchaseMarketValue), cPvItemM);
    }

    updatePortfolioFooters(allValues);
    updatePortfolioLabel(shareCount, formatLastPortfolioUpdate());

    m_actionAdd->setEnabled(true);
    m_actionRefreshAll->setEnabled(shareCount > 0);
    m_actionEdit->setEnabled(false);
    m_actionDelete->setEnabled(false);
    m_actionRefresh->setEnabled(false);
}

void MainWindow::onSaveAsPortfolio()
{
    const QString currentPath = AppSettings::instance().portfolioPath();

    const QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Portfolio speichern unter"),
        currentPath,
        tr("Portfolio-Datenbank (*.db);;Alle Dateien (*)"));

    if (filePath.isEmpty())
        return; // User cancelled

    if (filePath == currentPath) {
        OwnMessageBox::information(this,
            tr("Hinweis"),
            tr("Das Portfolio ist bereits unter diesem Namen gespeichert."));
        return;
    }

    // Copy current database file to new location
    if (QFile::exists(filePath))
        QFile::remove(filePath);

    if (!QFile::copy(currentPath, filePath)) {
        OwnMessageBox::critical(this,
            tr("Fehler"),
            tr("Das Portfolio konnte nicht gespeichert werden:\n\n%1").arg(filePath));
        return;
    }

    // Switch to the new file
    Database::instance().close();
    AppSettings::instance().setPortfolioPath(filePath);
    Database::instance().open(filePath);
    updateStatusBarPortfolio(filePath);

    addStatusMessage(tr("Portfolio gespeichert unter: %1").arg(filePath),
                     MessageType::Success);

    qInfo() << "[MainWindow] Portfolio saved as:" << filePath;
}

// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onAddShare()
{
    ViewShareAdd dlg(&m_documentsConfig, this);
    if (dlg.exec() == QDialog::Accepted) {
        addStatusMessage(
            tr("Aktie \"%1\" (%2) wurde erfolgreich hinzugefügt.")
                .arg(dlg.name().trimmed(), dlg.wkn().trimmed()),
            MessageType::Success);
        populatePortfolioTables();
    } else {
        addStatusMessage(
            tr("Aktie hinzufügen wurde abgebrochen."),
            MessageType::Info);
    }
}

void MainWindow::onEditShare()
{
    // Find the selected row in whichever table is currently active
    QTableWidget* table = nullptr;
    int row = -1;

    if (m_finalValueTable->selectionModel()->hasSelection()) {
        table = m_finalValueTable;
        row   = m_finalValueTable->currentRow();
    } else if (m_marketValueTable->selectionModel()->hasSelection()) {
        table = m_marketValueTable;
        row   = m_marketValueTable->currentRow();
    }

    if (!table || row < 0)
        return;

    // The GUID is stored in the WKN cell (column 0) via Qt::UserRole
    QTableWidgetItem* wknItem = table->item(row, 0);
    if (!wknItem)
        return;

    const QString shareGuid = wknItem->data(Qt::UserRole).toString();
    if (shareGuid.isEmpty())
        return;

    ViewShareEdit dlg(shareGuid, &m_documentsConfig, this);
    if (dlg.exec() == QDialog::Accepted) {
        addStatusMessage(tr("Aktie wurde gespeichert."), MessageType::Success);
        populatePortfolioTables();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onDeleteShare()
{
    // Find the selected row in whichever table is currently active
    QTableWidget* table = nullptr;
    int row = -1;

    if (m_finalValueTable->selectionModel()->hasSelection()) {
        table = m_finalValueTable;
        row   = m_finalValueTable->currentRow();
    } else if (m_marketValueTable->selectionModel()->hasSelection()) {
        table = m_marketValueTable;
        row   = m_marketValueTable->currentRow();
    }

    if (!table || row < 0)
        return;

    // GUID and display name from the selected row
    QTableWidgetItem* wknItem  = table->item(row, 0);
    QTableWidgetItem* nameItem = table->item(row, 1);
    if (!wknItem || !nameItem)
        return;

    const QString shareGuid = wknItem->data(Qt::UserRole).toString();
    const QString shareName = nameItem->text();
    if (shareGuid.isEmpty())
        return;

    // Confirmation dialog
    const bool confirmed = OwnMessageBox::question(
        this,
        tr("Aktie entfernen"),
        tr("Möchten Sie den Aktien-Eintrag \"%1\" wirklich aus dem Portfolio entfernen?\n\n"
           "Alle zugehörigen Käufe, Verkäufe, Dividenden und Kosten werden ebenfalls gelöscht.")
            .arg(shareName));

    if (!confirmed)
        return;

    // Delete from database
    ShareRepository shareRepo;
    if (!shareRepo.remove(shareGuid)) {
        OwnMessageBox::critical(
            this,
            tr("Fehler"),
            tr("Die Aktie \"%1\" konnte nicht entfernt werden:\n\n%2")
                .arg(shareName, shareRepo.lastError().text()));
        return;
    }

    m_actionDelete->setEnabled(false);
    m_actionEdit->setEnabled(false);
    populatePortfolioTables();

    addStatusMessage(
        tr("Aktie \"%1\" wurde aus dem Portfolio entfernt.").arg(shareName),
        MessageType::Success);

    qInfo() << "[MainWindow] Share removed:" << shareName << shareGuid;
}

// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onPortfolioRowDoubleClicked(QTableWidgetItem* item)
{
    if (!item)
        return;

    QTableWidget* table = item->tableWidget();
    if (!table)
        return;

    // The GUID is stored in the WKN cell (column 0) via Qt::UserRole,
    // regardless of which cell in the row was actually double-clicked.
    QTableWidgetItem* wknItem = table->item(item->row(), 0);
    if (!wknItem)
        return;

    const QString shareGuid = wknItem->data(Qt::UserRole).toString();
    if (shareGuid.isEmpty())
        return;

    // m_marketValueTable triggers the brokerage-free "Komplette Marktbewertung"
    // mode; any other table (currently only m_finalValueTable) triggers the
    // default Depotwert mode.
    const bool marketValueMode = (table == m_marketValueTable);

    ViewShareDetails dlg(shareGuid, marketValueMode, this);
    if (!dlg.hasValidShare())
        return; // Error already reported via showError() inside the presenter

    dlg.exec();
}

// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::onPortfolioRowRightClicked(const QPoint& pos)
{
    // Anders als bei onPortfolioRowDoubleClicked() liefert customContextMenuRequested()
    // nur die Klick-Position, kein QTableWidgetItem — sender() liefert die auslösende
    // Tabelle, dieselbe Konvention, die beide Tabellen mit einem gemeinsamen Slot
    // verbindet (siehe Verbindung in setupCentralWidget()).
    auto* table = qobject_cast<QTableWidget*>(sender());
    if (!table)
        return;

    QTableWidgetItem* item = table->itemAt(pos);
    if (!item)
        return;

    // Gleiche GUID-Ermittlung wie onPortfolioRowDoubleClicked() — siehe dort.
    QTableWidgetItem* wknItem = table->item(item->row(), 0);
    if (!wknItem)
        return;

    const QString shareGuid = wknItem->data(Qt::UserRole).toString();
    if (shareGuid.isEmpty())
        return;

    // Name-Spalte für die Popup-Überschrift (siehe ChartPopup) — Index 2 bei
    // FinalValueColumn UND MarketValueColumn (praktischer Zufall, siehe
    // beide Enums in dieser Datei), daher hier ohne Fallunterscheidung
    // zwischen den beiden Tabellen lesbar. Einfacher QTableWidgetItem::text()
    // statt TwoLineRole, da die Name-Zelle (anders als die meisten übrigen
    // Spalten) nur eine Zeile trägt (siehe populatePortfolioTables()).
    QTableWidgetItem* nameItem = table->item(item->row(), 2);
    const QString shareName = nameItem ? nameItem->text() : QString();

    // Kein Owner: ChartPopup zerstört sich per Qt::WA_DeleteOnClose selbst,
    // sobald die Maus das Popup verlässt (siehe ChartPopup::leaveEvent()).
    auto* popup = new ChartPopup(shareGuid, shareName);

    // Breite: Hauptfensterbreite − 50px (Nessies Vereinfachung, 31.07.2026:
    // "Hauptfensterbreite − 50px" statt der vorherigen 2×5px+50px-Rechnung —
    // mathematisch dasselbe Ergebnis bei zentrierter Ausrichtung, siehe
    // unten). Horizontal zentriert zum Hauptfenster (Nessies Vorgabe:
    // "horizontal zentriert zum Hauptfenster ausgerichtet") statt links
    // ausgerichtet — ergibt bei dieser Breite automatisch 25px Rand auf
    // jeder Seite. Nur die vertikale Position folgt weiterhin dem
    // Rechtsklick. Höhe bleibt bei ChartPopup's kompaktem Standardmaß.
    constexpr int kNarrower = 50;
    const int popupWidth = qMax(200, this->width() - kNarrower);
    popup->resize(popupWidth, popup->height());

    const QPoint clickGlobalPos = table->viewport()->mapToGlobal(pos);
    const int mainWindowGlobalCenterX = this->mapToGlobal(QPoint(this->width() / 2, 0)).x();
    const QPoint topLeft(mainWindowGlobalCenterX - popupWidth / 2, clickGlobalPos.y());
    popup->showAt(topLeft);
}

// ── Direkte Dokumentenerfassung (Drag+Drop, Feature 27.07.2026) ───────────────

// ── eventFilter ───────────────────────────────────────────────────────────────
// Scopes drag&drop handling to m_documentCaptureGroup only (installed via
// setAcceptDrops(true) + installEventFilter(this) in setupCentralWidget()).

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_documentCaptureGroup) {
        if (event->type() == QEvent::DragEnter) {
            auto* dragEvent = static_cast<QDragEnterEvent*>(event);
            const QList<QUrl> urls = dragEvent->mimeData()->urls();
            const bool singlePdf = urls.size() == 1
                && urls.first().isLocalFile()
                && urls.first().toLocalFile().endsWith(
                       QStringLiteral(".pdf"), Qt::CaseInsensitive);
            if (singlePdf)
                dragEvent->acceptProposedAction();
            else
                dragEvent->ignore();
            return true;
        }

        if (event->type() == QEvent::Drop) {
            auto* dropEvent = static_cast<QDropEvent*>(event);
            const QList<QUrl> urls = dropEvent->mimeData()->urls();

            // Einzeldatei-Drop only (Nessies Vorgabe, 27.07.2026) — bei
            // Mehrfachauswahl: Ablehnung mit Statusmeldung, keine Verarbeitung.
            if (urls.size() != 1) {
                addStatusMessage(
                    tr("Bitte nur ein Dokument gleichzeitig ablegen."),
                    MessageType::Warning);
                dropEvent->ignore();
                return true;
            }

            const QUrl& url = urls.first();
            if (!url.isLocalFile()
                || !url.toLocalFile().endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive)) {
                dropEvent->ignore();
                return true;
            }

            dropEvent->acceptProposedAction();
            handleDroppedDocument(url.toLocalFile());
            return true;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

// ── handleDroppedDocument ──────────────────────────────────────────────────────

void MainWindow::handleDroppedDocument(const QString& pdfPath)
{
    m_pendingCaptureDocumentPath = pdfPath;
    m_documentCaptureEdit->setText(QFileInfo(pdfPath).fileName());

    addStatusMessage(
        tr("Analysiere Dokument: %1 …").arg(QFileInfo(pdfPath).fileName()),
        MessageType::Info);

    m_documentCaptureExtractor.extract(pdfPath);
}

// ── onDocumentCaptureTextExtracted ─────────────────────────────────────────────

void MainWindow::onDocumentCaptureTextExtracted(bool success, const QString& text)
{
    const QString fileName = QFileInfo(m_pendingCaptureDocumentPath).fileName();

    if (!success) {
        addStatusMessage(
            tr("PDF-Konvertierung fehlgeschlagen oder kein Text extrahierbar: %1")
                .arg(fileName),
            MessageType::Error);
        m_documentCaptureEdit->clear();
        return;
    }

    const DocumentClassifier::Result result =
        DocumentClassifier::classify(text, m_documentsConfig);
    if (!result.matched) {
        addStatusMessage(
            tr("Dokument konnte keiner Bank/keinem Dokumenttyp zugeordnet werden: %1")
                .arg(fileName),
            MessageType::Error);
        m_documentCaptureEdit->clear();
        return;
    }

    openCaptureDialog(text, m_pendingCaptureDocumentPath, result.type, result.docEntry);
    // Feld erst nach Rückkehr aus openCaptureDialog() leeren — dessen
    // dlg.exec()-Aufrufe blockieren, bis der Benutzer den geöffneten Dialog
    // schließt, das Feld soll den Dateinamen also so lange sichtbar halten.
    m_documentCaptureEdit->clear();
}

// ── resolveShareGuidForDocument ────────────────────────────────────────────────

QString MainWindow::resolveShareGuidForDocument(const QString& pdfText,
                                                const DocumentEntry& docEntry)
{
    const QString wkn  = DocumentClassifier::extractWkn(pdfText, docEntry).toUpper();
    const QString isin = DocumentClassifier::extractIsin(pdfText, docEntry).toUpper();

    ShareRepository shareRepo;

    if (!wkn.isEmpty()) {
        const ShareObject share = shareRepo.findByWkn(wkn);
        if (share.isValid())
            return share.guid();
    }
    if (!isin.isEmpty()) {
        const ShareObject share = shareRepo.findByIsin(isin);
        if (share.isValid())
            return share.guid();
    }
    return QString();
}

// ── openCaptureDialog ──────────────────────────────────────────────────────────

void MainWindow::openCaptureDialog(const QString& pdfText,
                                   const QString& pdfPath,
                                   DocumentType docType,
                                   const DocumentEntry& docEntry)
{
    const QString fileName = QFileInfo(pdfPath).fileName();

    switch (docType) {
    case DocumentType::Buy: {
        const QString shareGuid = resolveShareGuidForDocument(pdfText, docEntry);
        if (shareGuid.isEmpty()) {
            addStatusMessage(
                tr("Keine passende Aktie im Portfolio gefunden — öffne "
                   "\"Aktie hinzufügen\" für %1").arg(fileName),
                MessageType::Info);

            ViewShareAdd dlg(&m_documentsConfig, this);
            dlg.presenter()->onDocumentSelected(pdfPath);
            if (dlg.exec() == QDialog::Accepted) {
                addStatusMessage(
                    tr("Aktie \"%1\" (%2) wurde erfolgreich hinzugefügt.")
                        .arg(dlg.name().trimmed(), dlg.wkn().trimmed()),
                    MessageType::Success);
                populatePortfolioTables();
            } else {
                addStatusMessage(tr("Aktie hinzufügen wurde abgebrochen."),
                                 MessageType::Info);
            }
        } else {
            addStatusMessage(
                tr("Kauf-Dokument erkannt — öffne \"Käufe\" für %1").arg(fileName),
                MessageType::Info);

            ViewBuyEdit dlg(shareGuid, &m_documentsConfig, this);
            dlg.presenter()->onDocumentSelected(pdfPath);
            dlg.exec();
            populatePortfolioTables();
        }
        break;
    }
    case DocumentType::Sale: {
        const QString shareGuid = resolveShareGuidForDocument(pdfText, docEntry);
        if (shareGuid.isEmpty()) {
            addStatusMessage(
                tr("Keine passende Aktie im Portfolio gefunden für "
                   "Verkaufs-Dokument: %1").arg(fileName),
                MessageType::Error);
            break;
        }

        addStatusMessage(
            tr("Verkaufs-Dokument erkannt — öffne \"Verkäufe\" für %1").arg(fileName),
            MessageType::Info);

        ViewSaleEdit dlg(shareGuid, &m_documentsConfig, this);
        dlg.presenter()->onDocumentSelected(pdfPath);
        dlg.exec();
        populatePortfolioTables();
        break;
    }
    case DocumentType::Dividend: {
        const QString shareGuid = resolveShareGuidForDocument(pdfText, docEntry);
        if (shareGuid.isEmpty()) {
            addStatusMessage(
                tr("Keine passende Aktie im Portfolio gefunden für "
                   "Dividenden-Dokument: %1").arg(fileName),
                MessageType::Error);
            break;
        }

        addStatusMessage(
            tr("Dividenden-Dokument erkannt — öffne \"Dividenden\" für %1").arg(fileName),
            MessageType::Info);

        ViewDividendEdit dlg(shareGuid, &m_documentsConfig, this);
        dlg.presenter()->onDocumentSelected(pdfPath);
        dlg.exec();
        populatePortfolioTables();
        break;
    }
    case DocumentType::Brokerage:
        // Bewusst außen vor (Nessies Vorgabe, 27.07.2026) — siehe ARCHITECTURE.md.
        addStatusMessage(
            tr("Dokumenttyp \"Kosten\" wird über die Direkte Dokumentenerfassung "
               "aktuell nicht unterstützt: %1").arg(fileName),
            MessageType::Info);
        break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::createBackup(const QString& portfolioPath)
{
    const auto& settings = AppSettings::instance();

    // BackupSettingsForm — "Backup aktivieren" (Standard: an). Ist Backup
    // deaktiviert, wird die Methode ohne Statusmeldung sofort verlassen —
    // analog zum bisherigen Verhalten, wenn die Portfolio-Datei fehlt.
    if (!settings.backupEnabled()) {
        qInfo() << "[MainWindow] Backup deaktiviert (Einstellungen) — kein Backup erstellt.";
        return;
    }

    const QFileInfo fi(portfolioPath);
    if (!fi.exists())
        return;

    // Zielverzeichnis: konfiguriertes Backup-Verzeichnis, sonst (Standard,
    // leer) wie bisher der Ordner der Portfolio-Datei selbst.
    QString dir = settings.backupDirectory();
    if (dir.isEmpty())
        dir = fi.absolutePath();

    if (!QDir(dir).exists() && !QDir().mkpath(dir)) {
        qWarning() << "[MainWindow] Backup-Verzeichnis konnte nicht angelegt werden:" << dir;
        addStatusMessage(tr("Backup-Verzeichnis konnte nicht angelegt werden: %1").arg(dir),
                         MessageType::Warning);
        return;
    }

    const QString baseName = fi.baseName();                 // e.g. "ShareList"
    const QString suffix   = fi.suffix();                   // e.g. "db"

    // Defensive Fallback auf die Standardwerte — BackupSettingsForm::saveSettings()
    // verhindert leere Werte zwar bereits vor dem Speichern, aber AppSettings
    // liest auch eine von Hand editierte INI ein, daher hier zusätzlich abgesichert.
    const QString prefixRaw = settings.backupNamePrefix().trimmed();
    const QString prefix    = prefixRaw.isEmpty() ? QStringLiteral("Backup") : prefixRaw;
    const QString dateFormatRaw = settings.backupDateFormat().trimmed();
    const QString dateFormat    = dateFormatRaw.isEmpty()
        ? QStringLiteral("yyyy_MM_dd_HH_mm_ss") : dateFormatRaw;
    const QString timestamp = QDateTime::currentDateTime().toString(dateFormat);

    // <Präfix>_ShareList_2026_06_16_21_59_30.db — mit Standardeinstellungen
    // identisch zum bisherigen fest codierten Schema.
    const QString backupName = QStringLiteral("%1_%2_%3.%4")
                                   .arg(prefix, baseName, timestamp, suffix);
    const QString backupPath = dir + QDir::separator() + backupName;

    // Show progress dialog — copy runs in background thread
    BackupProgressDialog dlg(portfolioPath, backupPath, this);
    dlg.exec();

    if (!dlg.wasSuccessful()) {
        qWarning() << "[MainWindow] Backup failed or cancelled:" << backupPath;
        addStatusMessage(tr("Backup fehlgeschlagen oder abgebrochen: %1").arg(backupName),
                         MessageType::Warning);
        return;
    }

    qInfo() << "[MainWindow] Backup created:" << backupPath;
    addStatusMessage(tr("Backup erstellt: %1").arg(backupName), MessageType::Info);

    // Keep only the most recent backupMaxCount() — delete oldest if exceeded.
    //
    // Namensfilter bewusst OHNE Präfix ("*_<Portfolioname>_*.<Endung>" statt
    // "<Präfix>_<Portfolioname>_*.<Endung>"): "Max. Anzahl Backups" soll eine
    // Obergrenze über ALLE Backups dieses Portfolios sein, unabhängig davon,
    // mit welchem Präfix sie jeweils erzeugt wurden. Würde der Filter am
    // aktuell konfigurierten Präfix festhalten, würde eine Präfix-Änderung in
    // BackupSettingsForm alle bisherigen Backups aus der Zählung herausfallen
    // lassen — sie würden nie mehr rotiert und blieben unbegrenzt liegen, weil
    // sie den neuen Filter nicht mehr treffen. Der Portfolioname (baseName)
    // plus Endung reicht als Anker aus, um Backups dieses Portfolios von
    // fremden Dateien im selben Verzeichnis zu unterscheiden.
    //
    // Sortierung nach tatsächlichem Änderungsdatum (QFileInfo::lastModified()),
    // NICHT nach Dateiname: eine rein alphabetische Sortierung wäre nur zufällig
    // korrekt, solange backupDateFormat() nullgepolstert und groß-nach-klein ist
    // (wie der Standard "yyyy_MM_dd_HH_mm_ss"). Ändert der Benutzer das Format
    // in BackupSettingsForm (z. B. auf "dd_MM_yyyy_..."), würde eine
    // Namens-Sortierung lautlos die falschen Dateien als "älteste" ansehen —
    // insbesondere wenn ältere Backups noch mit dem alten Format benannt sind
    // und im selben Namensfilter-Match landen. lastModified() ist von der
    // gewählten Textdarstellung völlig unabhängig und bleibt daher auch nach
    // einer Formatänderung korrekt.
    const int maxBackups = qMax(1, settings.backupMaxCount());
    QDir backupDir(dir);
    backupDir.setNameFilters({ QStringLiteral("*_%1_*.%2").arg(baseName, suffix) });
    backupDir.setFilter(QDir::Files);

    QFileInfoList backupInfos = backupDir.entryInfoList();
    std::sort(backupInfos.begin(), backupInfos.end(),
              [](const QFileInfo& a, const QFileInfo& b) {
                  return a.lastModified() < b.lastModified();
              });

    if (backupInfos.size() > maxBackups) {
        const int toDelete = backupInfos.size() - maxBackups;
        for (int i = 0; i < toDelete; ++i) {
            const QString oldBackupName = backupInfos.at(i).fileName();
            const QString oldBackup     = backupInfos.at(i).absoluteFilePath();
            if (QFile::remove(oldBackup)) {
                qInfo() << "[MainWindow] Old backup removed:" << oldBackup;
                addStatusMessage(tr("Altes Backup entfernt: %1").arg(oldBackupName),
                                 MessageType::Info);
            } else {
                qWarning() << "[MainWindow] Could not remove old backup:" << oldBackup;
                addStatusMessage(tr("Altes Backup konnte nicht entfernt werden: %1").arg(oldBackupName),
                                 MessageType::Warning);
            }
        }
    }
}

// ── buildDailyValuesUrl ───────────────────────────────────────────────────────

QString MainWindow::buildDailyValuesUrl(const QString& urlTemplate,
                                        const QDate&   latestExistingDate,
                                        ShareParsingType parsingType)
{
    // Mirrors Helper.BuildDailyValuesUrl() from the C# reference implementation.
    //
    // The URL templates stored in the DB may use C#-style placeholders {0}, {1}
    // instead of Qt's %1, %2 — normalise them first.
    // Also replace XML-escaped &amp; with & (carried over from C# XML storage).
    auto normalise = [](const QString& tmpl) -> QString {
        return QString(tmpl)
            .replace(QStringLiteral("{0}"), QStringLiteral("%1"))
            .replace(QStringLiteral("{1}"), QStringLiteral("%2"))
            .replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    };

    const QString tpl   = normalise(urlTemplate);
    const QDate   today = QDate::currentDate();

    auto monthDiff = [&](const QDate& from) -> int {
        int months = (today.year() - from.year()) * 12
                   + (today.month() - from.month());
        if (today.day() < from.day())
            --months;
        return qMax(0, months);
    };

    if (!latestExistingDate.isValid()) {
        // No data yet — fetch 5 years of history
        const QDate start = today.addYears(-5);
        switch (parsingType) {
        case ShareParsingType::ApiOnVista:
            return tpl
                .arg(start.toString(QStringLiteral("yyyy-MM-dd")),
                     QStringLiteral("Y5"));
        case ShareParsingType::ApiYahoo:
            return tpl.arg(QStringLiteral("20y"));
        default:
            return {};
        }
    }

    const int diff = monthDiff(latestExistingDate);

    // Select the minimal window that covers the gap
    struct PeriodEntry { int maxMonths; const char* onVistaCode; const char* yahooCode; };
    static const PeriodEntry kPeriods[] = {
        {  1, "M1",  "1mo" },
        {  3, "M3",  "3mo" },
        {  6, "M6",  "6mo" },
        { 12, "Y1",  "1y"  },
        { 36, "Y3",  "3y"  },
        { 60, "Y5",  "5y"  },
    };

    for (const auto& p : kPeriods) {
        if (diff < p.maxMonths) {
            switch (parsingType) {
            case ShareParsingType::ApiOnVista: {
                const QDate from = today.addMonths(-p.maxMonths);
                return tpl
                    .arg(from.toString(QStringLiteral("yyyy-MM-dd")),
                         QLatin1String(p.onVistaCode));
            }
            case ShareParsingType::ApiYahoo:
                return tpl.arg(QLatin1String(p.yahooCode));
            default:
                return {};
            }
        }
    }

    // Fallback: 5 years
    switch (parsingType) {
    case ShareParsingType::ApiOnVista: {
        const QDate from = today.addMonths(-60);
        return tpl
            .arg(from.toString(QStringLiteral("yyyy-MM-dd")),
                 QStringLiteral("Y5"));
    }
    case ShareParsingType::ApiYahoo:
        return tpl.arg(QStringLiteral("5y"));
    default:
        return {};
    }
}

// ── selectFirstShareRow ────────────────────────────────────────────────────────

void MainWindow::selectFirstShareRow()
{
    for (QTableWidget* table : { m_finalValueTable, m_marketValueTable }) {
        if (table->rowCount() > 0) {
            table->setCurrentCell(0, 0);
            table->scrollToTop();
        }
    }
}

// ── selectShareRow ─────────────────────────────────────────────────────────────

void MainWindow::selectShareRow(const QString& guid)
{
    if (guid.isEmpty())
        return;

    for (QTableWidget* table : { m_finalValueTable, m_marketValueTable }) {
        for (int row = 0; row < table->rowCount(); ++row) {
            QTableWidgetItem* item = table->item(row, 0);
            if (item && item->data(Qt::UserRole).toString() == guid) {
                table->setCurrentCell(row, 0);
                table->scrollToItem(item, QAbstractItemView::PositionAtCenter);
                break;
            }
        }
    }
}

// ── startRefreshForShare ──────────────────────────────────────────────────────

void MainWindow::startRefreshForShare(const ShareObject& share)
{
    m_refreshShare  = share;
    m_marketDone    = false;
    m_dailyDone     = false;
    m_errorOccurred = false;

    // Bugfix 07.07.2026: set BEFORE selectShareRow() — see m_refreshInProgress
    // doc comment in MainWindow.h for why isBusy() alone isn't sufficient here.
    m_refreshInProgress = true;

    // Keep the grid selection in sync with whichever share is currently
    // being updated — both for a single refresh and for every step of the
    // "Alle aktualisieren" queue.
    selectShareRow(share.guid());

    const bool doMarket = (share.updateType() == ShareUpdateType::MarketPrice ||
                           share.updateType() == ShareUpdateType::Both);
    const bool doDaily  = (share.updateType() == ShareUpdateType::DailyValues ||
                           share.updateType() == ShareUpdateType::Both);

    if (!doMarket) m_marketDone = true;
    if (!doDaily)  m_dailyDone  = true;

    addStatusMessage(tr("Aktualisierung gestartet: %1").arg(share.name()),
                     MessageType::Info);

    // ── MarketPrice parser ────────────────────────────────────────────────
    if (doMarket) {
        const QString apiKey = (share.marketPriceParsingType() == ShareParsingType::ApiYahoo)
            ? AppSettings::instance().apiKeyYahoo()
            : AppSettings::instance().apiKeyOnVista();

        ParserLib::ParsingType pt = (share.marketPriceParsingType() == ShareParsingType::ApiYahoo)
            ? ParserLib::ParsingType::YahooRealTime
            : ParserLib::ParsingType::OnVistaRealTime;

        const QString marketUrl = QString(share.marketPriceUrl())
            .replace(QStringLiteral("&amp;"), QStringLiteral("&"));

        m_parserMarketValues.setParsingValues(
            ParserLib::ParsingValues(
                QUrl(marketUrl),
                apiKey,
                share.marketPriceEncoding(),
                pt));
        m_marketValueProgress->setValue(0);
        m_marketValueStateLabel->setText(tr("Kurswert: gestartet..."));
        m_parserMarketValues.startParsing();
    }

    // ── DailyValues parser ────────────────────────────────────────────────
    if (doDaily) {
        DailyValuesRepository dvRepo;
        const QDate latestDate = dvRepo.latestDate(share.guid());
        const QString url = buildDailyValuesUrl(share.dailyValuesUrl(),
                                                latestDate,
                                                share.dailyValuesParsingType());
        if (url.isEmpty()) {
            addStatusMessage(
                tr("Fehler: Ungültige Tageswerte-URL für \"%1\"").arg(share.name()),
                MessageType::Error);
            m_dailyDone = true;
            if (m_marketDone)
                onRefreshShareFinished();
            return;
        }

        const QString apiKey = (share.dailyValuesParsingType() == ShareParsingType::ApiYahoo)
            ? AppSettings::instance().apiKeyYahoo()
            : AppSettings::instance().apiKeyOnVista();

        ParserLib::ParsingType pt = (share.dailyValuesParsingType() == ShareParsingType::ApiYahoo)
            ? ParserLib::ParsingType::YahooHistoryData
            : ParserLib::ParsingType::OnVistaHistoryData;

        m_parserDailyValues.setParsingValues(
            ParserLib::ParsingValues(
                QUrl(url),
                apiKey,
                share.dailyValuesEncoding(),
                pt));
        m_dailyValuesProgress->setValue(0);
        m_dailyValuesStateLabel->setText(tr("Tageswerte: gestartet..."));
        m_parserDailyValues.startParsing();
    }
}

// ── onRefreshShare ────────────────────────────────────────────────────────────

void MainWindow::onRefreshShare()
{
    if (m_parserMarketValues.isBusy() || m_parserDailyValues.isBusy())
        return;

    // Determine selected row in the active tab
    QTableWidget* activeTable = (m_portfolioTabs->currentIndex() == 0)
        ? m_finalValueTable
        : m_marketValueTable;

    const QList<QTableWidgetItem*> selected = activeTable->selectedItems();
    if (selected.isEmpty())
        return;

    const QString guid = activeTable->item(selected.first()->row(), 0)
                             ->data(Qt::UserRole).toString();
    if (guid.isEmpty())
        return;

    ShareRepository repo;
    const ShareObject share = repo.findByGuid(guid);
    if (!share.isValid() || share.updateType() == ShareUpdateType::None)
        return;

    m_updateAllFlag = false;

    // Disable actions while updating
    m_actionRefresh->setEnabled(false);
    m_actionRefreshAll->setEnabled(false);
    m_actionAdd->setEnabled(false);
    m_actionEdit->setEnabled(false);
    m_actionDelete->setEnabled(false);

    startRefreshForShare(share);
}

// ── onRefreshAll ──────────────────────────────────────────────────────────────

void MainWindow::onRefreshAll()
{
    if (m_parserMarketValues.isBusy() || m_parserDailyValues.isBusy())
        return;

    ShareRepository repo;
    const QList<ShareObject> all = repo.findAll();

    m_refreshQueue.clear();
    for (const ShareObject& s : all) {
        if (s.updateType() != ShareUpdateType::None)
            m_refreshQueue.enqueue(s);
    }

    if (m_refreshQueue.isEmpty()) {
        addStatusMessage(tr("Keine Aktien für Aktualisierung konfiguriert."),
                         MessageType::Warning);
        return;
    }

    m_updateAllFlag = true;

    m_actionRefresh->setEnabled(false);
    m_actionRefreshAll->setEnabled(false);
    m_actionAdd->setEnabled(false);
    m_actionEdit->setEnabled(false);
    m_actionDelete->setEnabled(false);

    startRefreshForShare(m_refreshQueue.dequeue());
}

// ── onRefreshShareFinished ────────────────────────────────────────────────────

void MainWindow::onRefreshShareFinished()
{
    if (m_errorOccurred) {
        // At least one parser failed — stop the queue entirely
        m_errorOccurred = false;
        m_refreshQueue.clear();
        m_updateAllFlag = false;
        finaliseRefresh();
        return;
    }

    // The share's market price and/or daily values have now been fully
    // fetched and persisted (m_marketDone && m_dailyDone, no error) — refresh
    // both footer tables immediately so the totals stay in sync, not just
    // once "Alle aktualisieren" has finished.
    refreshPortfolioFooters();

    // Feature 21.07.2026: Portfolio-Label ("Letzte Aktualisierung") live
    // mitziehen — nicht erst beim nächsten populatePortfolioTables()
    // (Neustart/Neuladen). Entry-Count bleibt unverändert (kein Hinzufügen/
    // Entfernen während eines Refreshs), daher genügt m_finalValueTable->
    // rowCount() statt einer erneuten ShareRepository::findAll()-Abfrage.
    updatePortfolioLabel(m_finalValueTable->rowCount(), formatLastPortfolioUpdate());

    if (m_updateAllFlag && !m_refreshQueue.isEmpty()) {
        // Start next share in queue
        startRefreshForShare(m_refreshQueue.dequeue());
    } else {
        // Capture before finaliseRefresh() resets it — only reset the
        // selection back to the first share when the whole "Alle
        // aktualisieren" run has just finished (queue now empty). A
        // completed single-share refresh leaves the selection untouched.
        const bool wasUpdateAll = m_updateAllFlag;
        finaliseRefresh();
        if (wasUpdateAll)
            selectFirstShareRow();

        // Dieser Zweig wird genau einmal pro abgeschlossenem Refresh-Lauf
        // erreicht — entweder ein fertiger Einzel-Refresh, oder das Ende
        // eines erfolgreichen "Alle aktualisieren"-Laufs (Queue jetzt leer) —
        // niemals pro einzelner Aktie innerhalb der Queue. Die Prüfung auf
        // m_errorOccurred oben (early return) stellt sicher, dass dies nur
        // bei Erfolg läuft.
        playUpdateFinishedSound();
    }
}

// ── finaliseRefresh ───────────────────────────────────────────────────────────

void MainWindow::finaliseRefresh()
{
    m_updateAllFlag     = false;
    m_refreshInProgress = false;
    m_refreshQueue.clear();

    m_marketValueProgress->setValue(0);
    m_dailyValuesProgress->setValue(0);
    m_marketValueStateLabel->setText(tr("Kurswert:"));
    m_dailyValuesStateLabel->setText(tr("Tageswerte:"));

    m_actionAdd->setEnabled(true);
    m_actionRefreshAll->setEnabled(true);

    // Re-enable Edit/Delete/Refresh only if a row is still selected
    QTableWidget* active = (m_portfolioTabs->currentIndex() == 0)
        ? m_finalValueTable : m_marketValueTable;
    const bool hasSelection = !active->selectedItems().isEmpty();
    m_actionEdit->setEnabled(hasSelection);
    m_actionDelete->setEnabled(hasSelection);
    m_actionRefresh->setEnabled(hasSelection);
}

// ── playUpdateFinishedSound ────────────────────────────────────────────────────

void MainWindow::playUpdateFinishedSound()
{
    const auto& settings = AppSettings::instance();
    if (!settings.soundUpdateEnabled())
        return;

    const QString soundsDir = QCoreApplication::applicationDirPath()
                             + QStringLiteral("/sounds");
    const QString filePath  = soundsDir + QStringLiteral("/") + settings.soundUpdateFile();

    // Defensive erneute Prüfung — checkAndLoadConfigurations() deaktiviert
    // den Sound bereits beim Start, falls die Datei fehlt; der Nutzer könnte
    // sie aber danach ohne Neustart gelöscht haben.
    if (!QFileInfo::exists(filePath))
        return;

    m_updateSoundEffect.setSource(QUrl::fromLocalFile(filePath));
    m_updateSoundEffect.setVolume(1.0);
    m_updateSoundEffect.play();
}

// ── onMarketValuesUpdated ─────────────────────────────────────────────────────

void MainWindow::onMarketValuesUpdated(const ParserLib::ParserInfoState& state)
{
    using EC = ParserLib::ParserErrorCode;

    // Update progress bar
    m_marketValueProgress->setValue(state.percentage);

    // Progress state labels
    if (state.lastErrorCode == EC::Starting) {
        m_marketValueStateLabel->setText(tr("Kurswert:"));
        m_marketValueProgress->setValue(0);
        return;
    }
    if (state.lastErrorCode == EC::Started) {
        m_marketValueStateLabel->setText(
            tr("Kurswert: lädt... (%1 %)").arg(state.percentage));
        return;
    }
    if (state.lastErrorCode == EC::ContentLoadStarted ||
        state.lastErrorCode == EC::ContentLoadFinished ||
        state.lastErrorCode == EC::SearchStarted ||
        state.lastErrorCode == EC::SearchRunning ||
        state.lastErrorCode == EC::SearchFinished) {
        m_marketValueStateLabel->setText(
            tr("Kurswert: %1 %").arg(state.percentage));
        return;
    }

    // ── Error handling ────────────────────────────────────────────────────
    if (state.lastErrorCode < EC::NoError) {
        if (m_marketDone) return; // guard against double-firing
        QString errMsg;
        switch (state.lastErrorCode) {
        case EC::InvalidWebSiteGiven:
            errMsg = tr("Kurswert: Ungültige URL für \"%1\"")
                         .arg(m_refreshShare.name()); break;
        case EC::NoWebContentLoaded:
            errMsg = tr("Kurswert: Kein Inhalt geladen für \"%1\"")
                         .arg(m_refreshShare.name()); break;
        case EC::ParsingFailed:
            errMsg = tr("Kurswert: Parsing fehlgeschlagen für \"%1\"")
                         .arg(m_refreshShare.name()); break;
        case EC::NetworkError:
            errMsg = tr("Kurswert: Netzwerkfehler für \"%1\" — %2")
                         .arg(m_refreshShare.name(), state.exceptionMessage); break;
        case EC::JsonError:
            errMsg = tr("Kurswert: JSON-Fehler für \"%1\"")
                         .arg(m_refreshShare.name()); break;
        default:
            errMsg = tr("Kurswert: Fehler beim Abruf von \"%1\" (%2)")
                         .arg(m_refreshShare.name(),
                              QString::number(static_cast<int>(state.lastErrorCode))); break;
        }
        addStatusMessage(errMsg, MessageType::Error);

        m_marketDone    = true;
        m_errorOccurred = true; // signals onRefreshShareFinished() to stop the queue

        // Let the daily parser finish on its own — do not cancel it
        if (m_dailyDone)
            onRefreshShareFinished();
        return;
    }

    // ── Finished ──────────────────────────────────────────────────────────
    if (state.lastErrorCode == EC::Finished) {
        if (m_marketDone) return; // guard against double-firing
        const auto& result = state.searchResult;

        // Extract price
        double newPrice = 0.0;
        const auto& priceList = result[QStringLiteral("Price")];
        if (!priceList.isEmpty())
            newPrice = QLocale::c().toDouble(priceList.first());

        // prevDayPrice: most recent entry in daily_values before today
        double prevDay = 0.0;
        {
            DailyValuesRepository dvRepo;
            const QDate today = QDate::currentDate();
            const QList<DailyValuesObject> entries =
                dvRepo.findByShareAndDateRange(m_refreshShare.guid(),
                                              QDate(1900, 1, 1),
                                              today.addDays(-1));
            if (!entries.isEmpty())
                prevDay = entries.last().closingPrice();
        }

        // Persist to DB
        const QString now = QDateTime::currentDateTime()
                                .toString(Qt::ISODate);
        ShareRepository shareRepo;
        shareRepo.updatePrice(m_refreshShare.guid(), newPrice, prevDay, now);
        shareRepo.updateLastInternetUpdate(m_refreshShare.guid(), now);

        // Update the in-memory copy
        m_refreshShare.setCurPrice(newPrice);
        m_refreshShare.setPrevDayPrice(prevDay);

        // Recompute all values for this share and update both grid rows
        const ShareValues v = ShareCalculator::compute(
            m_refreshShare.guid(), newPrice, prevDay);
        const QLocale locale;

        auto perfColor = [this](double val) -> QColor {
            if (val > 0.0) return AppSettings::instance().logColorAt(5);
            if (val < 0.0) return AppSettings::instance().logColorAt(3);
            return palette().color(QPalette::Text);
        };

        // Pick development icon — mirrors the devIcon lambda in
        // populatePortfolioTables()/updatePortfolioFooters(). Needed here too,
        // since a single-share refresh must also refresh the PrevDayChart/
        // CompleteChart icon cells, not just their text (Bugfix: Icon blieb
        // nach Einzel-Refresh auf dem alten, ggf. fallenden Stand stehen).
        auto devIcon = [](double pct) -> QIcon {
            if (pct >  2.0)  return IconProvider::icon(IconProvider::PositivStrong);
            if (pct >  0.0)  return IconProvider::icon(IconProvider::PositivNormal);
            if (pct <  -2.0) return IconProvider::icon(IconProvider::NegativStrong);
            if (pct <  0.0)  return IconProvider::icon(IconProvider::NegativNormal);
            return IconProvider::icon(IconProvider::Neutral);
        };

        auto setTwoLine = [&](QTableWidget* tbl, int row, int col,
                               const QString& top,    const QColor& tc,
                               const QString& bottom, const QColor& bc) {
            if (auto* it = tbl->item(row, col)) {
                it->setData(TwoLineRole::Top,         top);
                it->setData(TwoLineRole::Bottom,       bottom);
                it->setData(TwoLineRole::TopColor,     tc);
                it->setData(TwoLineRole::BottomColor,  bc);
            }
        };

        const QColor neutral = palette().color(QPalette::Text);
        QColor muted = neutral;
        muted.setAlpha(140);

        auto findRow = [&](QTableWidget* tbl) -> int {
            for (int r = 0; r < tbl->rowCount(); ++r) {
                auto* it = tbl->item(r, 0);
                if (it && it->data(Qt::UserRole).toString() == m_refreshShare.guid())
                    return r;
            }
            return -1;
        };

        const QString curPriceStr  = locale.toString(v.curPrice, 'f', 4) + QStringLiteral(" €");
        const QString prevPriceStr = locale.toString(v.prevDayPrice, 'f', 4) + QStringLiteral(" €");
        const QString prevDiffStr  = (v.prevDayDiff >= 0 ? QStringLiteral("+") : QString())
                                   + locale.toString(v.prevDayDiff, 'f', 2) + QStringLiteral(" €");
        const QString prevPctStr   = (v.prevDayPct >= 0 ? QStringLiteral("+") : QString())
                                   + locale.toString(v.prevDayPct, 'f', 2) + QStringLiteral(" %");
        const QString profitStr    = locale.toString(v.profitLoss, 'f', 2) + QStringLiteral(" €");
        const QString profitPctStr = locale.toString(v.profitLossPct, 'f', 2) + QStringLiteral(" %");
        const QString purchaseStr  = locale.toString(v.purchaseValue, 'f', 2) + QStringLiteral(" €");
        const QString curValStr    = locale.toString(v.curValue, 'f', 2) + QStringLiteral(" €");

        // Update Depotwert row
        if (const int fr = findRow(m_finalValueTable); fr >= 0) {
            using FC = FinalValueColumn;
            // Depotwert-specific strings (WITH brokerage)
            const QString profitFinalStr    = locale.toString(v.profitLossFinal, 'f', 2) + QStringLiteral(" €");
            const QString profitFinalPctStr = locale.toString(v.profitLossPctFinal, 'f', 2) + QStringLiteral(" %");
            const QString purchaseFinalStr  = locale.toString(v.purchaseValueFinal, 'f', 2) + QStringLiteral(" €");
            setTwoLine(m_finalValueTable, fr, static_cast<int>(FC::Price),
                       curPriceStr, neutral, prevPriceStr, neutral);
            setTwoLine(m_finalValueTable, fr, static_cast<int>(FC::PrevDay),
                       prevDiffStr, perfColor(v.prevDayDiff), prevPctStr, perfColor(v.prevDayPct));
            if (auto* it = m_finalValueTable->item(fr, static_cast<int>(FC::PrevDayChart)))
                it->setIcon(devIcon(v.prevDayPct));
            setTwoLine(m_finalValueTable, fr, static_cast<int>(FC::Performance),
                       profitFinalStr, perfColor(v.profitLossFinal),
                       profitFinalPctStr, perfColor(v.profitLossPctFinal));
            setTwoLine(m_finalValueTable, fr, static_cast<int>(FC::PurchaseFinalValue),
                       purchaseFinalStr, neutral, curValStr, neutral);

            const QString cProfitStr    = locale.toString(v.completeProfitLoss, 'f', 2) + QStringLiteral(" €");
            const QString cProfitPctStr = locale.toString(v.completeProfitPct,  'f', 2) + QStringLiteral(" %");
            const QString cPurchaseStr  = locale.toString(v.completePurchase,   'f', 2) + QStringLiteral(" €");
            const QString cCurValStr    = locale.toString(v.completeCurValue,   'f', 2) + QStringLiteral(" €");
            setTwoLine(m_finalValueTable, fr, static_cast<int>(FC::CompletePerformance),
                       cProfitStr, perfColor(v.completeProfitLoss), cProfitPctStr, perfColor(v.completeProfitPct));
            setTwoLine(m_finalValueTable, fr, static_cast<int>(FC::CompletePurchaseFinalValue),
                       cPurchaseStr, neutral, cCurValStr, neutral);
            if (auto* it = m_finalValueTable->item(fr, static_cast<int>(FC::CompleteChart)))
                it->setIcon(devIcon(v.completeProfitPct));
        }

        // Update Marktwert row
        if (const int mr = findRow(m_marketValueTable); mr >= 0) {
            using MC = MarketValueColumn;
            setTwoLine(m_marketValueTable, mr, static_cast<int>(MC::Price),
                       curPriceStr, neutral, prevPriceStr, neutral);
            setTwoLine(m_marketValueTable, mr, static_cast<int>(MC::PrevDay),
                       prevDiffStr, perfColor(v.prevDayDiff), prevPctStr, perfColor(v.prevDayPct));
            if (auto* it = m_marketValueTable->item(mr, static_cast<int>(MC::PrevDayChart)))
                it->setIcon(devIcon(v.prevDayPct));
            setTwoLine(m_marketValueTable, mr, static_cast<int>(MC::Performance),
                       profitStr, perfColor(v.profitLoss), profitPctStr, perfColor(v.profitLossPct));
            setTwoLine(m_marketValueTable, mr, static_cast<int>(MC::PurchaseMarketValue),
                       purchaseStr, neutral, curValStr, neutral);

            const QString cProfitMStr    = locale.toString(v.completeProfitLossMarket, 'f', 2) + QStringLiteral(" €");
            const QString cProfitMPctStr = locale.toString(v.completeProfitPctMarket,  'f', 2) + QStringLiteral(" %");
            const QString cPurchaseMStr  = locale.toString(v.completePurchaseMarket,   'f', 2) + QStringLiteral(" €");
            const QString cCurValMStr    = locale.toString(v.completeCurValueMarket,   'f', 2) + QStringLiteral(" €");
            setTwoLine(m_marketValueTable, mr, static_cast<int>(MC::CompletePerformance),
                       cProfitMStr, perfColor(v.completeProfitLossMarket), cProfitMPctStr, perfColor(v.completeProfitPctMarket));
            setTwoLine(m_marketValueTable, mr, static_cast<int>(MC::CompletePurchaseMarketValue),
                       cPurchaseMStr, neutral, cCurValMStr, neutral);
            if (auto* it = m_marketValueTable->item(mr, static_cast<int>(MC::CompleteChart)))
                it->setIcon(devIcon(v.completeProfitPctMarket));
        }

        const QString priceStr = locale.toString(newPrice, 'f', 2);
        addStatusMessage(
            tr("Kurswert aktualisiert: %1 — %2")
                .arg(m_refreshShare.name(), priceStr),
            MessageType::Success);

        m_marketValueStateLabel->setText(tr("Kurswert: %1").arg(priceStr));

        m_marketDone = true;
        if (m_dailyDone)
            onRefreshShareFinished();
    }
}

// ── onDailyValuesUpdated ──────────────────────────────────────────────────────

void MainWindow::onDailyValuesUpdated(const ParserLib::ParserInfoState& state)
{
    using EC = ParserLib::ParserErrorCode;

    // Update progress bar
    m_dailyValuesProgress->setValue(state.percentage);

    // Progress state labels
    if (state.lastErrorCode == EC::Starting) {
        m_dailyValuesStateLabel->setText(tr("Tageswerte:"));
        m_dailyValuesProgress->setValue(0);
        return;
    }
    if (state.lastErrorCode == EC::Started) {
        m_dailyValuesStateLabel->setText(
            tr("Tageswerte: lädt... (%1 %)").arg(state.percentage));
        return;
    }
    if (state.lastErrorCode == EC::ContentLoadStarted ||
        state.lastErrorCode == EC::ContentLoadFinished ||
        state.lastErrorCode == EC::SearchStarted ||
        state.lastErrorCode == EC::SearchRunning ||
        state.lastErrorCode == EC::SearchFinished) {
        m_dailyValuesStateLabel->setText(
            tr("Tageswerte: %1 %").arg(state.percentage));
        return;
    }

    // ── Error handling ────────────────────────────────────────────────────
    if (state.lastErrorCode < EC::NoError) {
        if (m_dailyDone) return; // guard against double-firing
        QString errMsg;
        switch (state.lastErrorCode) {
        case EC::InvalidWebSiteGiven:
            errMsg = tr("Tageswerte: Ungültige URL für \"%1\"")
                         .arg(m_refreshShare.name()); break;
        case EC::NoWebContentLoaded:
            errMsg = tr("Tageswerte: Kein Inhalt geladen für \"%1\"")
                         .arg(m_refreshShare.name()); break;
        case EC::ParsingFailed:
            errMsg = tr("Tageswerte: Parsing fehlgeschlagen für \"%1\"")
                         .arg(m_refreshShare.name()); break;
        case EC::NetworkError:
            errMsg = tr("Tageswerte: Netzwerkfehler für \"%1\" — %2")
                         .arg(m_refreshShare.name(), state.exceptionMessage); break;
        case EC::JsonError:
            errMsg = tr("Tageswerte: JSON-Fehler für \"%1\"")
                         .arg(m_refreshShare.name()); break;
        default:
            errMsg = tr("Tageswerte: Fehler beim Abruf von \"%1\" (%2)")
                         .arg(m_refreshShare.name(),
                              QString::number(static_cast<int>(state.lastErrorCode))); break;
        }
        addStatusMessage(errMsg, MessageType::Error);

        m_dailyDone     = true;
        m_errorOccurred = true; // signals onRefreshShareFinished() to stop the queue

        // Let the market parser finish on its own — do not cancel it
        if (m_marketDone)
            onRefreshShareFinished();
        return;
    }

    // ── Finished ──────────────────────────────────────────────────────────
    if (state.lastErrorCode == EC::Finished) {
        if (m_dailyDone) return; // guard against double-firing

        // Feature 21.07.2026: last_internet_update wurde bisher nur von
        // onMarketValuesUpdated() gesetzt — ein reiner DailyValues-Refresh
        // (ShareUpdateType::DailyValues) blieb dadurch unberücksichtigt,
        // obwohl der Internet-Zugriff erfolgreich war. Analog zu
        // onMarketValuesUpdated() unbedingt bei Finished setzen (auch wenn
        // dvList leer ist oder der Upsert unten fehlschlägt — der
        // Internet-Abruf selbst war erfolgreich), damit
        // ShareRepository::maxLastInternetUpdate() auch reine
        // Tageswerte-Refreshs im Portfolio-Label widerspiegelt.
        const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
        ShareRepository().updateLastInternetUpdate(m_refreshShare.guid(), now);

        const QList<ParserLib::DailyValues>& dvList = state.dailyValuesList;

        if (!dvList.isEmpty()) {
            // Convert and upsert
            QList<DailyValuesObject> objects;
            objects.reserve(dvList.size());
            for (const auto& dv : dvList) {
                objects.append(DailyValuesObject(
                    m_refreshShare.guid(),
                    dv.date,
                    dv.openingPrice,
                    dv.closingPrice,
                    dv.top,
                    dv.bottom,
                    dv.volume));
            }

            DailyValuesRepository dvRepo;
            DailyValuesRepository::UpsertStats stats;
            if (!dvRepo.upsertList(objects, &stats)) {
                addStatusMessage(
                    tr("Tageswerte: Speichern fehlgeschlagen für \"%1\": %2")
                        .arg(m_refreshShare.name(), dvRepo.lastError().text()),
                    MessageType::Error);
            } else {
                addStatusMessage(
                    tr("Tageswerte aktualisiert: %1 — %2 Einträge geholt "
                       "(Eingefügt: %3 / Aktualisiert: %4 / Unverändert: %5)")
                        .arg(m_refreshShare.name())
                        .arg(stats.fetched)
                        .arg(stats.inserted)
                        .arg(stats.updated)
                        .arg(stats.unchanged),
                    MessageType::Success);
            }

            m_dailyValuesStateLabel->setText(
                tr("Tageswerte: %1 Einträge").arg(dvList.size()));
        } else {
            addStatusMessage(
                tr("Tageswerte: Keine neuen Einträge für \"%1\"")
                    .arg(m_refreshShare.name()),
                MessageType::Info);
        }

        m_dailyDone = true;
        if (m_marketDone)
            onRefreshShareFinished();
    }
}

// ── setupTableDelegates ───────────────────────────────────────────────────────

void MainWindow::setupTableDelegates()
{
    // One delegate instance per table — parented so Qt manages lifetime.
    // setItemDelegateForColumn() does NOT take ownership; the parent does.

    // Depotwert tab — two-line columns
    using FC = FinalValueColumn;
    auto* delF = new TwoLineDelegate(m_finalValueTable);
    for (int col : {
            static_cast<int>(FC::BrokerageDividend),
            static_cast<int>(FC::Price),
            static_cast<int>(FC::PrevDay),
            static_cast<int>(FC::Performance),
            static_cast<int>(FC::PurchaseFinalValue),
            static_cast<int>(FC::CompletePerformance),
            static_cast<int>(FC::CompletePurchaseFinalValue) }) {
        m_finalValueTable->setItemDelegateForColumn(col, delF);
        m_finalValueFooter->setItemDelegateForColumn(col, delF);
    }

    // Marktwert tab — two-line columns
    using MC = MarketValueColumn;
    auto* delM = new TwoLineDelegate(m_marketValueTable);
    for (int col : {
            static_cast<int>(MC::Price),
            static_cast<int>(MC::PrevDay),
            static_cast<int>(MC::Performance),
            static_cast<int>(MC::PurchaseMarketValue),
            static_cast<int>(MC::CompletePerformance),
            static_cast<int>(MC::CompletePurchaseMarketValue) }) {
        m_marketValueTable->setItemDelegateForColumn(col, delM);
        m_marketValueFooter->setItemDelegateForColumn(col, delM);
    }

    // Center the decoration (icon) in all icon columns of both tabs and footers.
    auto* iconDelF = new CenterIconDelegate(m_finalValueTable);
    for (int col : { static_cast<int>(FC::Icon),
                     static_cast<int>(FC::PrevDayChart),
                     static_cast<int>(FC::CompleteChart) }) {
        m_finalValueTable->setItemDelegateForColumn(col, iconDelF);
        m_finalValueFooter->setItemDelegateForColumn(col, iconDelF);
    }
    auto* iconDelM = new CenterIconDelegate(m_marketValueTable);
    for (int col : { static_cast<int>(MC::Icon),
                     static_cast<int>(MC::PrevDayChart),
                     static_cast<int>(MC::CompleteChart) }) {
        m_marketValueTable->setItemDelegateForColumn(col, iconDelM);
        m_marketValueFooter->setItemDelegateForColumn(col, iconDelM);
    }

    // The Depotwert footer merges Preis+(Chart-)Icon+Vortag into a single
    // right-aligned row label whose span anchor is the Price column. Give that
    // column a plain-text delegate in the FOOTER only, so the label renders —
    // the two-line price delegate would swallow plain DisplayRole text. The
    // main table keeps its two-line price delegate untouched.
    m_finalValueFooter->setItemDelegateForColumn(static_cast<int>(FC::Price), iconDelF);
    // The "Kosten / Dividenden (ges.)" footer label is two-line; its span anchor
    // is the Icon column, which therefore uses the two-line delegate (footer only).
    m_finalValueFooter->setItemDelegateForColumn(static_cast<int>(FC::Icon), delF);
}

// ── updatePortfolioFooters ────────────────────────────────────────────────────

void MainWindow::updatePortfolioFooters(const QList<ShareValues>& shareValues)
{
    const QLocale locale;
    const QColor  neutral = palette().color(QPalette::Text);
    QColor        muted   = neutral;
    muted.setAlpha(140);

    auto perfColor = [this](double value) -> QColor {
        if (value > 0.0) return AppSettings::instance().logColorAt(5);
        if (value < 0.0) return AppSettings::instance().logColorAt(3);
        return palette().color(QPalette::Text);
    };

    auto devIcon = [](double pct) -> QIcon {
        if (pct >  2.0)  return IconProvider::icon(IconProvider::PositivStrong);
        if (pct >  0.0)  return IconProvider::icon(IconProvider::PositivNormal);
        if (pct <  -2.0) return IconProvider::icon(IconProvider::NegativStrong);
        if (pct <  0.0)  return IconProvider::icon(IconProvider::NegativNormal);
        return IconProvider::icon(IconProvider::Neutral);
    };

    auto makeFooterItem = [&](const QString& top,    const QColor& topColor,
                               const QString& bottom, const QColor& bottomColor) {
        auto* item = new QTableWidgetItem();
        item->setData(TwoLineRole::Top,         top);
        item->setData(TwoLineRole::Bottom,       bottom);
        item->setData(TwoLineRole::TopColor,     topColor);
        item->setData(TwoLineRole::BottomColor,  bottomColor);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        return item;
    };

    auto makeTextItem = [&](const QString& text, const QColor& color = QColor(Qt::black),
                             Qt::Alignment align = Qt::AlignRight | Qt::AlignVCenter) {
        auto* item = new QTableWidgetItem(text);
        item->setForeground(color);
        item->setTextAlignment(align);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        return item;
    };

    // ── Depotwert footer ──────────────────────────────────────────────────
    double bd = 0, tPurchase = 0, tCurValue = 0, tProfit = 0, tProfitPct = 0;
    double cPurchase = 0, cCurValue = 0, cProfit = 0, cProfitPct = 0;
    ShareCalculator::portfolioTotalsFinal(shareValues,
        bd, tPurchase, tCurValue, tProfit, tProfitPct,
        cPurchase, cCurValue, cProfit, cProfitPct);

    // Initialise footer rows (3 rows: Einzahlung / Entwicklung / Depotstand)
    for (int r = 0; r < 3; ++r) {
        if (m_finalValueFooter->item(r, 0) == nullptr) {
            for (int c = 0; c < m_finalValueFooter->columnCount(); ++c)
                m_finalValueFooter->setItem(r, c, new QTableWidgetItem());
        }
    }
    m_finalValueFooter->setRowHeight(0, 34);
    m_finalValueFooter->setRowHeight(1, 34);
    m_finalValueFooter->setRowHeight(2, 34);

    // Separate totals for the Kosten / Dividenden cell (2-line, middle row).
    double tBrokerage = 0.0, tDividend = 0.0;
    for (const ShareValues& v : shareValues) {
        tBrokerage += v.totalBrokerage;
        tDividend  += v.totalDividend;
    }

    using FC = FinalValueColumn;
    // Merge Preis + (Chart-)Icon + Vortag so the row label is right-aligned up
    // to the Vortag column, mirroring the Marktwert footer. The Kosten /
    // Dividenden column (left of Preis) stays separate for its own 2-line total.
    m_finalValueFooter->setSpan(0, static_cast<int>(FC::Price), 1, 3);
    m_finalValueFooter->setSpan(1, static_cast<int>(FC::Price), 1, 3);
    m_finalValueFooter->setSpan(2, static_cast<int>(FC::Price), 1, 3);

    // Row 0 — Einzahlung (gesamt): single-line (current Depotstand is row 2).
    m_finalValueFooter->setItem(0, static_cast<int>(FC::Price),
        makeTextItem(tr("Einzahlung (gesamt)"), neutral, Qt::AlignRight | Qt::AlignVCenter));
    m_finalValueFooter->setItem(0, static_cast<int>(FC::PurchaseFinalValue),
        makeFooterItem(locale.toString(tPurchase, 'f', 2) + QStringLiteral(" €"), neutral,
                       QString(), muted));
    m_finalValueFooter->setItem(0, static_cast<int>(FC::CompletePurchaseFinalValue),
        makeFooterItem(locale.toString(cPurchase, 'f', 2) + QStringLiteral(" €"), neutral,
                       QString(), muted));

    // Row 1 — Entwicklung (gesamt); Kosten / Dividenden total (2-line) in the
    // Kosten / Dividenden column, plus the complete-development icon.
    m_finalValueFooter->setItem(1, static_cast<int>(FC::Price),
        makeTextItem(tr("Entwicklung (gesamt)"), neutral, Qt::AlignRight | Qt::AlignVCenter));
    // "Kosten / Dividenden (ges.)" label, two-line to match its value:
    // Kosten (top) / Dividenden (bottom), both full neutral (no dimming).
    m_finalValueFooter->setSpan(1, static_cast<int>(FC::Icon), 1, 4);
    m_finalValueFooter->setItem(1, static_cast<int>(FC::Icon),
        makeFooterItem(tr("Kosten (ges.)"),     neutral,
                       tr("Dividenden (ges.)"), neutral));
    m_finalValueFooter->setItem(1, static_cast<int>(FC::BrokerageDividend),
        makeFooterItem(locale.toString(tBrokerage, 'f', 2) + QStringLiteral(" €"), neutral,
                       locale.toString(tDividend, 'f', 2) + QStringLiteral(" €"), neutral));
    m_finalValueFooter->setItem(1, static_cast<int>(FC::Performance),
        makeFooterItem(locale.toString(tProfit, 'f', 2) + QStringLiteral(" €"),
                       perfColor(tProfit),
                       locale.toString(tProfitPct, 'f', 2) + QStringLiteral(" %"),
                       perfColor(tProfitPct)));
    m_finalValueFooter->setItem(1, static_cast<int>(FC::CompletePerformance),
        makeFooterItem(locale.toString(cProfit, 'f', 2) + QStringLiteral(" €"),
                       perfColor(cProfit),
                       locale.toString(cProfitPct, 'f', 2) + QStringLiteral(" %"),
                       perfColor(cProfitPct)));
    {
        auto* chartItem = new QTableWidgetItem();
        chartItem->setIcon(devIcon(cProfitPct));
        chartItem->setFlags(chartItem->flags() & ~Qt::ItemIsEditable);
        m_finalValueFooter->setItem(1, static_cast<int>(FC::CompleteChart), chartItem);
    }

    // Row 2 — Aktueller Depotstand: single-line.
    m_finalValueFooter->setItem(2, static_cast<int>(FC::Price),
        makeTextItem(tr("Aktueller Depotstand"), neutral, Qt::AlignRight | Qt::AlignVCenter));
    m_finalValueFooter->setItem(2, static_cast<int>(FC::PurchaseFinalValue),
        makeFooterItem(locale.toString(tCurValue, 'f', 2) + QStringLiteral(" €"), neutral,
                       QString(), muted));
    m_finalValueFooter->setItem(2, static_cast<int>(FC::CompletePurchaseFinalValue),
        makeFooterItem(locale.toString(cCurValue, 'f', 2) + QStringLiteral(" €"), neutral,
                       QString(), muted));

    // Mirror column widths from main table
    for (int c = 0; c < m_finalValueTable->columnCount(); ++c)
        m_finalValueFooter->setColumnWidth(c, m_finalValueTable->columnWidth(c));

    // ── Marktwert footer ──────────────────────────────────────────────────
    double mPurchase = 0, mCurValue = 0, mMarketValue = 0, mProfit = 0, mProfitPct = 0, mMarketPct = 0;
    ShareCalculator::portfolioTotalsMarket(shareValues,
        mPurchase, mCurValue, mMarketValue, mProfit, mProfitPct, mMarketPct);

    double mcPurchase = 0, mcCurValue = 0, mcProfit = 0, mcProfitPct = 0;
    ShareCalculator::portfolioCompleteTotalsMarket(shareValues,
        mcPurchase, mcCurValue, mcProfit, mcProfitPct);

    for (int r = 0; r < 3; ++r) {
        if (m_marketValueFooter->item(r, 0) == nullptr) {
            for (int c = 0; c < m_marketValueFooter->columnCount(); ++c)
                m_marketValueFooter->setItem(r, c, new QTableWidgetItem());
        }
    }
    m_marketValueFooter->setRowHeight(0, 34);
    m_marketValueFooter->setRowHeight(1, 34);
    m_marketValueFooter->setRowHeight(2, 34);

    // Merge only the label area (Icon..Vortag) so the row label is right-aligned
    // up to the Vortag column. Columns after Vortag are NOT merged (matches C#).
    m_marketValueFooter->setSpan(0, 0, 1, 7);
    m_marketValueFooter->setSpan(1, 0, 1, 7);
    m_marketValueFooter->setSpan(2, 0, 1, 7);

    using MC = MarketValueColumn;
    // Row 0 — Einzahlung (gesamt): only the Einzahlung total (the current
    // Marktwert total is shown in row 2 "Aktueller Depotstand").
    m_marketValueFooter->setItem(0, static_cast<int>(MC::Icon),
        makeTextItem(tr("Einzahlung (gesamt)"), neutral, Qt::AlignRight | Qt::AlignVCenter));
    m_marketValueFooter->setItem(0, static_cast<int>(MC::PurchaseMarketValue),
        makeFooterItem(locale.toString(mPurchase, 'f', 2) + QStringLiteral(" €"), neutral,
                       QString(), muted));
    m_marketValueFooter->setItem(0, static_cast<int>(MC::CompletePurchaseMarketValue),
        makeFooterItem(locale.toString(mcPurchase, 'f', 2) + QStringLiteral(" €"), neutral,
                       QString(), muted));

    // Row 1 — Entwicklung (gesamt)
    m_marketValueFooter->setItem(1, static_cast<int>(MC::Icon),
        makeTextItem(tr("Entwicklung (gesamt)"), neutral, Qt::AlignRight | Qt::AlignVCenter));
    m_marketValueFooter->setItem(1, static_cast<int>(MC::Performance),
        makeFooterItem(locale.toString(mProfit, 'f', 2) + QStringLiteral(" €"),
                       perfColor(mProfit),
                       locale.toString(mProfitPct, 'f', 2) + QStringLiteral(" %"),
                       perfColor(mProfitPct)));
    m_marketValueFooter->setItem(1, static_cast<int>(MC::CompletePerformance),
        makeFooterItem(locale.toString(mcProfit, 'f', 2) + QStringLiteral(" €"),
                       perfColor(mcProfit),
                       locale.toString(mcProfitPct, 'f', 2) + QStringLiteral(" %"),
                       perfColor(mcProfitPct)));
    {
        auto* chartItem = new QTableWidgetItem();
        chartItem->setIcon(devIcon(mcProfitPct));
        chartItem->setFlags(chartItem->flags() & ~Qt::ItemIsEditable);
        m_marketValueFooter->setItem(1, static_cast<int>(MC::CompleteChart), chartItem);
    }

    // Row 2 — Aktueller Depotstand (= marketValue gesamt)
    m_marketValueFooter->setItem(2, static_cast<int>(MC::Icon),
        makeTextItem(tr("Aktueller Depotstand"), neutral, Qt::AlignRight | Qt::AlignVCenter));
    m_marketValueFooter->setItem(2, static_cast<int>(MC::PurchaseMarketValue),
        makeFooterItem(locale.toString(mMarketValue, 'f', 2) + QStringLiteral(" €"), neutral,
                       QString(), muted));
    m_marketValueFooter->setItem(2, static_cast<int>(MC::CompletePurchaseMarketValue),
        makeFooterItem(locale.toString(mcCurValue, 'f', 2) + QStringLiteral(" €"), neutral,
                       QString(), muted));

    for (int c = 0; c < m_marketValueTable->columnCount(); ++c)
        m_marketValueFooter->setColumnWidth(c, m_marketValueTable->columnWidth(c));
}

// ── refreshPortfolioFooters ───────────────────────────────────────────────────

void MainWindow::refreshPortfolioFooters()
{
    // Recompute ShareValues for ALL shares (not just the one just refreshed) —
    // the footer totals are portfolio-wide aggregates, so a single-share price
    // update still needs a full recompute to stay correct.
    ShareRepository shareRepo;
    const QList<ShareObject> shares = shareRepo.findAll();

    QList<ShareValues> allValues;
    allValues.reserve(shares.size());
    for (const ShareObject& share : shares) {
        allValues.append(ShareCalculator::compute(
            share.guid(), share.curPrice(), share.prevDayPrice()));
    }

    updatePortfolioFooters(allValues);
}

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QMainWindow>
#include <QAction>
#include <QMenu>
#include <QToolBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QLabel>
#include <QProgressBar>
#include <QGroupBox>
#include <QTextEdit>
#include <QLineEdit>
#include <QSoundEffect>

#include "../../config/WebSitesConfig.h"
#include "../../config/DocumentsConfig.h"
#include "../ShareAddForm/ViewShareAdd.h"
#include "../ShareEditForm/ViewShareEdit.h"
#include "../../models/ShareObject.h"
#include "../../../libs/parser/src/Parser.h"
#include "../../../libs/parser/src/DataTypes.h"
#include "TwoLineDelegate.h"
#include "CenterIconDelegate.h"
#include "../../utils/ShareCalculator.h"

#include <QList>
#include <QQueue>

class QNetworkAccessManager;

/**
 * @brief Main application window.
 *
 * Layout (top to bottom):
 * - Menu bar  (Datei | Einstellungen | API-Einstellung | Hilfe)
 * - Toolbar   (Neu | Öffnen | --- | Alle aktualisieren | Aktualisieren | --- | Hinzufügen | Editieren | Entfernen)
 * - Portfolio label ("Portfolio-Übersicht ( Einträge: X ) / Letzte Aktualisierung: -")
 * - QTabWidget:
 *     - "Kompletter Depotwert"  (final value)
 *     - "Kompletter Marktwert"  (market value)
 * - Bottom area (horizontal, via QSplitter):
 *     - Left  (~75%): Status-Meldungen (scrollable log)
 *     - Right (~25%): Direkte Dokumentenerfassung (compact) + Aktualisierungs-Status (2 progress bars)
 * - Status bar
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    /**
     * @brief Test-only constructor: injects a QNetworkAccessManager into both
     * internal Parser instances (m_parserMarketValues, m_parserDailyValues).
     *
     * Lets unit tests exercise the real refresh flow (startRefreshForShare(),
     * onMarketValuesUpdated(), onDailyValuesUpdated(), onRefreshShareFinished(),
     * "Alle aktualisieren") against a ParserTestUtils::FakeNetworkAccessManager
     * (see tests/parser/FakeNetworkAccessManager.h) instead of a real network,
     * while running through the exact same production code path otherwise.
     *
     * @param networkManagerForTesting  Non-null QNetworkAccessManager (typically
     *        a FakeNetworkAccessManager) to inject into both parsers. Ownership
     *        stays with the caller. Must not be null — use the
     *        MainWindow(QWidget*) constructor in production code.
     * @param parent  Optional parent widget.
     */
    explicit MainWindow(QNetworkAccessManager* networkManagerForTesting, QWidget* parent = nullptr);

    ~MainWindow() override = default;

    // ── Column indices — Final value tab ──────────────────────────────────
    // ── Column indices — Depotwert tab ────────────────────────────────────
    /// @note Columns marked (2-line) use TwoLineDelegate; data stored via TwoLineRole.
    enum class FinalValueColumn {
        Icon                = 0,  ///< Update state icon (StateUpdate*/StateNoUpdate)
        Wkn                 = 1,  ///< WKN string
        Name                = 2,  ///< Share name (stretch)
        Volume              = 3,  ///< Net volume (buys – sold)
        BrokerageDividend   = 4,  ///< Σ brokerage + Σ dividend (2-line)
        Price               = 5,  ///< curPrice / prevDayPrice (2-line)
        PrevDayChart        = 6,  ///< Development icon (Negativ/Neutral/Positiv)
        PrevDay             = 7,  ///< Δ€ / Δ% vs. previous day (2-line, colored)
        Performance         = 8,  ///< profitLoss€ / profitLoss% (2-line, colored)
        PurchaseFinalValue  = 9,  ///< purchaseValue€ / curValue€ (2-line)
        CompleteChart       = 10, ///< Complete development icon
        CompletePerformance = 11, ///< completeProfitLoss€ / completeProfitPct% (2-line, colored)
        CompletePurchaseFinalValue = 12, ///< completePurchase€ / completeCurValue€ (2-line)
        Count               = 13
    };

    // ── Column indices — Marktwert tab ────────────────────────────────────
    enum class MarketValueColumn {
        Icon                = 0,  ///< Update state icon
        Wkn                 = 1,  ///< WKN string
        Name                = 2,  ///< Share name (stretch)
        Volume              = 3,  ///< Net volume
        Price               = 4,  ///< curPrice / prevDayPrice (2-line)
        PrevDayChart        = 5,  ///< Development icon
        PrevDay             = 6,  ///< Δ€ / Δ% vs. previous day (2-line, colored)
        Performance         = 7,  ///< profitLoss€ / profitLoss% (2-line, colored)
        PurchaseMarketValue = 8,  ///< purchaseValue€ / curValue€ (2-line)
        CompleteChart               = 9,  ///< Complete development icon
        CompletePerformance         = 10, ///< completeProfitLossMarket€ / completeProfitPctMarket% (2-line, colored)
        CompletePurchaseMarketValue = 11, ///< completePurchaseMarket€ / completeCurValueMarket€ (2-line)
        Count               = 12
    };

    /**
     * @brief Build the DailyValues API URL for a share based on existing data.
     *
     * Mirrors Helper.BuildDailyValuesUrl() from the C# reference implementation.
     * If no daily values exist yet, requests 5 years of history.
     * If data exists, selects the minimal window covering the gap to today:
     * 1 month / 3 months / 6 months / 1 year / 3 years / 5 years.
     *
     * The urlTemplate must contain Qt::QString arg placeholders:
     * - OnVista: %1 = ISO date (yyyy-MM-dd), %2 = period string (M1/M3/Y1/…)
     * - Yahoo:   %1 = period string (1mo/3mo/1y/…)
     *
     * Declared `public static` (07.07.2026) rather than as a `private slot`
     * (the pattern used for selectShareRow()/selectFirstShareRow()): unlike
     * those, this method touches no instance state at all — it's a pure
     * function of its three parameters — so `static` is both more correct
     * and lets tests call it directly (`MainWindow::buildDailyValuesUrl(...)`)
     * without any QMetaObject::invokeMethod involvement. That matters here
     * specifically because `ShareParsingType` is a plain `enum class` with no
     * `Q_DECLARE_METATYPE`/`Q_ENUM` registration, which `Q_ARG()` would need
     * for invokeMethod-by-name; a plain static call sidesteps that
     * entirely. Mirrors the existing `XmlPortfolioParser::normalizeWebSiteUrl()`
     * pattern (public static pure-utility method, tested directly).
     *
     * @param urlTemplate          Raw URL template from ShareObject::dailyValuesUrl().
     * @param latestExistingDate   Most recent date in daily_values, invalid if none.
     * @param parsingType          OnVista or Yahoo parsing strategy.
     * @return                     Fully resolved URL string, empty on error.
     */
    static QString buildDailyValuesUrl(const QString& urlTemplate,
                                       const QDate&   latestExistingDate,
                                       ShareParsingType parsingType);

private slots:
    /**
     * @brief Create a new empty portfolio database.
     *
     * Prompts the user for a file path, closes the current database if open,
     * creates a new empty database and updates AppSettings.
     * Only activates the "Hinzufügen" button — all other share-related
     * actions remain disabled until at least one share has been added.
     */
    void onNewPortfolio();

    /**
     * @brief Open an existing portfolio database.
     */
    void onOpenPortfolio();

    /**
     * @brief Save the current portfolio under a new name.
     */
    void onSaveAsPortfolio();

    /**
     * @brief Open the "Aktie hinzufügen" dialog.
     *
     * Shows ViewShareAdd as a modal dialog. On success, reloads
     * the share table to reflect the newly added entry.
     */
    void onAddShare();

    /**
     * @brief Open the "Aktie editieren" dialog for the currently selected share.
     *
     * Connected to m_actionEdit. Reads the GUID from the selected row's
     * WKN cell (Qt::UserRole) and opens ViewShareEdit as a modal dialog.
     */
    void onEditShare();

    /**
     * @brief Remove the currently selected share from the portfolio.
     *
     * Connected to m_actionDelete. Shows a confirmation dialog before
     * deleting the share and all associated data (buys, sales, dividends,
     * brokerages, daily values) via cascade delete.
     */
    void onDeleteShare();

    /**
     * @brief Start a price/history update for the currently selected share.
     *
     * Reads the share GUID from the selected row, loads the ShareObject,
     * and starts ParserLib for MarketPrice and/or DailyValues depending
     * on the share's updateType(). If a parse is already running, the
     * call is ignored.
     */
    void onRefreshShare();

    /**
     * @brief Start a sequential price/history update for all shares.
     *
     * Builds a queue of all shares with updateType() != None and triggers
     * the first update. After each share completes (both parsers finished),
     * the next share in the queue is started automatically.
     * On any parser error the queue is cleared and the update stops.
     */
    void onRefreshAll();

    /**
     * @brief Slot called by m_parserMarketValues on every state change.
     * @param state  Current parser state snapshot.
     */
    void onMarketValuesUpdated(const ParserLib::ParserInfoState& state);

    /**
     * @brief Slot called by m_parserDailyValues on every state change.
     * @param state  Current parser state snapshot.
     */
    void onDailyValuesUpdated(const ParserLib::ParserInfoState& state);

    /**
     * @brief Select the first row and scroll to the top in both portfolio tables.
     *
     * Called once the "Alle aktualisieren" run has fully completed without
     * any error, so the grid resets back to showing the first share instead
     * of leaving the last-processed share selected. Not called when an
     * error occurred — in that case the selection stays on the share that
     * failed so the problem is immediately visible. No-op if a table is
     * empty.
     *
     * Declared as a slot (rather than a plain private method) solely so
     * unit tests can invoke it directly via QMetaObject::invokeMethod
     * without needing a real ParserLib::Parser run — it has no Parser or
     * network dependency of its own. No behavioral difference otherwise.
     */
    void selectFirstShareRow();

    /**
     * @brief Select the row belonging to the given share in both portfolio tables.
     *
     * Called at the start of each refresh (single share or as part of the
     * "Alle aktualisieren" queue) so the grid always highlights whichever
     * share is currently being updated — regardless of which tab is
     * currently visible. No-op if the GUID is empty or not found in either
     * table.
     *
     * Declared as a slot (rather than a plain private method) solely so
     * unit tests can invoke it directly via QMetaObject::invokeMethod
     * without needing a real ParserLib::Parser run — it has no Parser or
     * network dependency of its own. No behavioral difference otherwise.
     *
     * @param guid  GUID of the share to select.
     */
    void selectShareRow(const QString& guid);

    /**
     * @brief Opens ShareDetailsForm (read-only) for a double-clicked portfolio row.
     *
     * Connected to itemDoubleClicked() of both m_finalValueTable and
     * m_marketValueTable (setupCentralWidget()). Reads the GUID from column 0
     * of the clicked row (Qt::UserRole) — same pattern as onEditShare()/
     * onDeleteShare() — and shows ViewShareDetails modally. If the GUID does
     * not resolve to a valid share, ViewShareDetails::hasValidShare() is
     * false; in that case the error has already been reported by the
     * presenter (showError()) and the dialog is not exec()'d.
     *
     * @param item  The QTableWidgetItem that was double-clicked (any column).
     */
    void onPortfolioRowDoubleClicked(QTableWidgetItem* item);

private:
    /**
     * @brief Create a timestamped backup of the given portfolio file.
     *
     * The backup is placed in the same directory as the original file.
     * Filename format: Backup_<basename>_YYYY_MM_DD_HH_MM_SS.db
     * At most 5 backups are kept — the oldest is deleted when the limit
     * is exceeded.
     *
     * @param portfolioPath  Full path to the portfolio file to back up.
     */
    void createBackup(const QString& portfolioPath);
private:
    /**
     * @brief Shared construction body for both constructors.
     *
     * Extracted 07.07.2026 so the test-only MainWindow(QNetworkAccessManager*, ...)
     * constructor doesn't have to duplicate it — the only difference between
     * the two constructors is which Parser constructor
     * m_parserMarketValues/m_parserDailyValues are built with (decided in
     * each constructor's member initializer list, before this method runs).
     * No behavioral change versus the previous single-constructor version.
     */
    void initialize();

    void setupActions();
    void setupMenuBar();
    void setupToolBar();
    void setupCentralWidget();
    void setupStatusBar();
    void restoreWindowGeometry();

    /**
     * @brief Start the parser(s) for a single share.
     *
     * Called both from onRefreshShare() (single) and from the "all" queue.
     * Stores the share in m_refreshShare so callbacks can access it.
     *
     * @param share  The ShareObject to update.
     */
    void startRefreshForShare(const ShareObject& share);

    /**
     * @brief Called when both parsers for the current share have finished (or errored).
     *
     * In "all" mode: pops the next share from m_refreshQueue and calls
     * startRefreshForShare(), or finalises the run if the queue is empty.
     * In single mode: re-enables all actions.
     */
    void onRefreshShareFinished();

    /**
     * @brief Re-enable all actions after a refresh run has completed or failed.
     */
    void finaliseRefresh();

    /**
     * @brief Play the configured "Aktualisierung erfolgreich"-Sound (Feature 21.07.2026).
     *
     * Called exactly once per abgeschlossenem Refresh-Lauf — sowohl bei
     * einem einzelnen Refresh als auch am Ende eines kompletten "Alle
     * aktualisieren"-Laufs (nicht nach jeder einzelnen Aktie in der Queue) —
     * und nur, wenn der Lauf ohne Fehler zu Ende ging
     * (`m_errorOccurred == false`). No-op, wenn
     * `AppSettings::soundUpdateEnabled()` false ist oder die konfigurierte
     * Datei (`sounds/<AppSettings::soundUpdateFile()>` neben der
     * Executable) nicht existiert.
     *
     * Deklariert als `private virtual`, damit eine Testklasse sie per
     * `override` abfangen kann, um Aufrufzeitpunkt/-anzahl zu prüfen — ohne
     * von echter QSoundEffect-Wiedergabe (benötigt ein Audio-Gerät, das in
     * CI/Testumgebungen ggf. fehlt) abhängig zu sein.
     */
    virtual void playUpdateFinishedSound();

    /**
     * @brief Update the portfolio label with entry count and last update time.
     * @param entryCount      Number of shares in the portfolio.
     * @param lastUpdate      Last update timestamp string (e.g. "15.06.2024 10:30").
     */
    void updatePortfolioLabel(int entryCount = 0,
                              const QString& lastUpdate = QStringLiteral("-"));

    /**
     * @brief Clear all rows from both portfolio tables.
     */
    void clearPortfolioTables();

    /**
     * @brief Update the window title to show the current portfolio file name.
     * @param portfolioPath  Full path to the current portfolio database.
     */
    void updateWindowTitle(const QString& portfolioPath);

    /**
     * @brief Update the right-hand status-bar label with the current portfolio path.
     * @param portfolioPath  Full path to the open portfolio, or empty string to
     *                       show "Kein Portfolio geladen".
     */
    void updateStatusBarPortfolio(const QString& portfolioPath);

    /**
     * @brief Load all shares from the database and populate both portfolio tables.
     */
    void populatePortfolioTables();

    /**
     * @brief Refresh the summary footer rows for both portfolio tabs.
     *
     * Called after populatePortfolioTables() and after any single-share
     * refresh so totals stay in sync.
     *
     * @param shareValues  Per-share computed values (same order as table rows).
     */
    void updatePortfolioFooters(const QList<ShareValues>& shareValues);

    /**
     * @brief Recompute ShareValues for all shares and refresh both footer tables.
     *
     * Called from onRefreshShareFinished() once a single share's parsers have
     * both completed successfully (market price and/or daily values, per its
     * ShareUpdateType) — independent of whether this is a single-share refresh
     * or part of "Alle aktualisieren". Loads all shares fresh from the DB so
     * the totals reflect the just-persisted price/daily-values update.
     */
    void refreshPortfolioFooters();

    /**
     * @brief Install TwoLineDelegate on all two-line columns of both tables.
     *
     * Called once from setupCentralWidget() after the tables are created.
     */
    void setupTableDelegates();

    /**
     * @brief Message type for status message coloring.
     *
     * Matches the color scheme from the original C# application:
     * - Info:       black   — normal informational messages
     * - Success:    green   — operation completed successfully
     * - Warning:    orange  — non-critical issue
     * - Error:      red     — recoverable error
     * - FatalError: dark red — unrecoverable error
     */
    enum class MessageType {
        Info,
        Success,
        Warning,
        Error,
        FatalError
    };

    /**
     * @brief Load and validate all required configuration files at startup.
     *
     * Loads WebSitesConfig and DocumentsConfig, writes status messages
     * for each result, and disables all UI controls if any configuration
     * fails to load. Only Quit remains accessible in the error case.
     *
     * @return true if all configurations loaded successfully, false otherwise.
     */
    bool checkAndLoadConfigurations();

    /**
     * @brief Offers the documents root directory dialog if none is configured.
     *
     * If AppSettings::documentsRootPath() is empty, opens
     * DocumentsSettingsForm once. Not mandatory — if the user cancels,
     * nothing changes and the dialog is simply offered again on the next
     * start. No-op if a root is already configured. Must be called after
     * the portfolio database (if any) has been opened, so
     * DocumentRootMigrator::detectCommonRoot() can inspect existing
     * document paths for its suggestion.
     */
    void ensureDocumentsRootConfigured();

    /**
     * @brief Disable all UI controls except Quit.
     *
     * Called when a critical configuration error occurs at startup.
     * The user can only quit the application in this state.
     */
    void disableAllControls();

    /**
     * @brief Append a timestamped colored message to the status message log.
     * @param message  The message text to display.
     * @param type     Message type that determines the text color.
     */
    void addStatusMessage(const QString& message,
                          MessageType type = MessageType::Info);

    // ── Toolbar ───────────────────────────────────────────────────────────
    QToolBar*     m_toolBar               = nullptr;

    // ── Portfolio area ────────────────────────────────────────────────────
    QLabel*       m_portfolioLabel        = nullptr; ///< Entry count + last update
    QTabWidget*   m_portfolioTabs         = nullptr;
    QTableWidget* m_finalValueTable       = nullptr; ///< "Kompletter Depotwert"
    QTableWidget* m_marketValueTable      = nullptr; ///< "Kompletter Marktwert"
    QTableWidget* m_finalValueFooter      = nullptr; ///< Summary footer for Depotwert tab
    QTableWidget* m_marketValueFooter     = nullptr; ///< Summary footer for Marktwert tab

    // ── Bottom panel — left ───────────────────────────────────────────────
    QGroupBox*    m_statusMessageGroup    = nullptr;
    QTextEdit*    m_statusMessageText     = nullptr;

    // ── Bottom panel — right ──────────────────────────────────────────────
    QGroupBox*    m_documentCaptureGroup  = nullptr;
    QLineEdit*    m_documentCaptureEdit   = nullptr;

    QGroupBox*    m_updateStateGroup      = nullptr;
    QLabel*       m_marketValueStateLabel = nullptr;
    QProgressBar* m_marketValueProgress   = nullptr;
    QLabel*       m_dailyValuesStateLabel = nullptr;
    QProgressBar* m_dailyValuesProgress   = nullptr;

    // ── Status bar ────────────────────────────────────────────────────────
    QLabel*       m_statusLabel           = nullptr;
    QLabel*       m_portfolioPathLabel    = nullptr; ///< Status-Bar: aktuell geladenes Portfolio

    // ── Configuration ─────────────────────────────────────────────────────
    WebSitesConfig  m_webSitesConfig;  ///< Website parsing configuration
    DocumentsConfig m_documentsConfig; ///< Document parsing configuration

    // ── Parser ────────────────────────────────────────────────────────────
    ParserLib::Parser m_parserMarketValues; ///< Parser for current price (MarketPrice)
    ParserLib::Parser m_parserDailyValues;  ///< Parser for OHLCV history (DailyValues)

    // ── Refresh state ─────────────────────────────────────────────────────
    ShareObject        m_refreshShare;       ///< Share currently being updated
    QQueue<ShareObject> m_refreshQueue;      ///< Queue for "Alle aktualisieren"
    bool               m_updateAllFlag = false; ///< true while "Alle aktualisieren" is running
    bool               m_marketDone    = false; ///< MarketValues parser finished/idle for current share
    bool               m_dailyDone     = false; ///< DailyValues parser finished/idle for current share
    bool               m_errorOccurred = false; ///< At least one parser failed for current share
    QSoundEffect       m_updateSoundEffect;  ///< Wiederverwendete Instanz für playUpdateFinishedSound()
    /**
     * @brief True from the start of startRefreshForShare() until finaliseRefresh().
     *
     * Bugfix 07.07.2026: startRefreshForShare() calls selectShareRow() BEFORE
     * either Parser's startParsing() — so on the very first refresh (when a
     * table has never been selected before), selectShareRow() can trigger a
     * genuine selectionChanged() while m_parserMarketValues.isBusy() and
     * m_parserDailyValues.isBusy() are still both false, letting the
     * enableShareActions busy-guard (setupCentralWidget()) slip through and
     * re-enable Edit/Delete/Refresh mid-refresh. m_refreshInProgress closes
     * that gap by being set before selectShareRow() runs, independent of
     * whether either Parser has actually started yet.
     */
    bool               m_refreshInProgress = false;
    QAction* m_actionNew                  = nullptr;
    QAction* m_actionOpen                 = nullptr;
    QAction* m_actionSaveAs               = nullptr;
    QAction* m_actionQuit                 = nullptr;

    // ── Actions — Share ───────────────────────────────────────────────────
    QAction* m_actionAdd                  = nullptr;
    QAction* m_actionEdit                 = nullptr;
    QAction* m_actionDelete               = nullptr;
    QAction* m_actionRefresh              = nullptr;
    QAction* m_actionRefreshAll           = nullptr;

    // ── Actions — Settings ────────────────────────────────────────────────
    QAction* m_actionLanguage             = nullptr;
    QAction* m_actionLogger               = nullptr;
    QAction* m_actionSound                = nullptr;
    QAction* m_actionBackup               = nullptr;
    QAction* m_actionDocuments            = nullptr;

    // ── Actions — API Settings ────────────────────────────────────────────
    QAction* m_actionApiKeyYahoo          = nullptr;

    // ── Actions — Help ────────────────────────────────────────────────────
    QAction* m_actionAbout                = nullptr;
};

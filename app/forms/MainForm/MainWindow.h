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
#include <QPoint>
#include <QSystemTrayIcon>

#include "../../config/WebSitesConfig.h"
#include "../../config/DocumentsConfig.h"
#include "../ShareAddForm/ViewShareAdd.h"
#include "../ShareEditForm/ViewShareEdit.h"
#include "../PortfolioChartForm/ViewPortfolioChart.h"
#include "../../models/ShareObject.h"
#include "../../../libs/parser/src/Parser.h"
#include "../../../libs/parser/src/DataTypes.h"
#include "TwoLineDelegate.h"
#include "CenterIconDelegate.h"
#include "../../utils/ShareCalculator.h"
#include "../../utils/ShareUpdateRules.h"
#include "../../utils/PdfTextExtractor.h"
#include "../../utils/SplitAdjustmentAudit.h"

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

    /**
     * @brief Resolve which existing share (if any) a parsed document belongs to.
     *
     * Extracts WKN/ISIN from @p pdfText via DocumentClassifier::extractWkn()/
     * extractIsin() and looks the share up via ShareRepository::findByWkn()/
     * findByIsin(). WKN takes precedence if both are present (WKN is the
     * primary identifier elsewhere in the app, e.g. ViewShareAdd's duplicate
     * check). Returns an empty string — not an error — if the DocumentEntry
     * has no Wkn/Isin rule at all.
     *
     * Declared `public static` (27.07.2026, corrected from an earlier private
     * non-static version that was wrongly documented as "directly callable
     * from tests" — a private method has no such access) for the same reason
     * as buildDailyValuesUrl(): it touches no instance state (only
     * DocumentClassifier + a locally constructed ShareRepository), so static
     * is both more correct and lets tests call it directly
     * (`MainWindow::resolveShareGuidForDocument(...)`) against a real test
     * database without any MainWindow instance or QMetaObject::invokeMethod
     * involved.
     *
     * @param pdfText   Plain text extracted from the PDF.
     * @param docEntry  Matched DocumentEntry (regex rules) from DocumentsConfig.
     * @return Matching share's GUID, or an empty string if none found.
     */
    static QString resolveShareGuidForDocument(const QString& pdfText,
                                                const DocumentEntry& docEntry);

    /**
     * @brief Decide whether minimizing should hide the window into the tray.
     *
     * Feature 03.08.2026 ("Minimieren wahlweise in Taskleiste oder Tray"):
     * pure decision function used by changeEvent() — declared `public static`
     * (same rationale as buildDailyValuesUrl()/resolveShareGuidForDocument()
     * above) so it is directly testable without constructing a real
     * QSystemTrayIcon or MainWindow, and without depending on whether a tray
     * is actually available in the test/CI environment.
     *
     * @param settingEnabled     AppSettings::trayOnMinimizeEnabled().
     * @param trayIconAvailable  true if a QSystemTrayIcon was successfully
     *                           created for this MainWindow (setupTrayIcon()
     *                           only creates one when
     *                           QSystemTrayIcon::isSystemTrayAvailable()).
     * @return true if minimizing should hide the window and show the tray
     *         icon instead of the normal taskbar minimize.
     */
    static bool shouldMinimizeToTray(bool settingEnabled, bool trayIconAvailable);

    /**
     * @brief Beschriftung eines Update-Typs für Benutzertexte.
     *
     * Feature 06.08.2026. Wortgleich zu den Radiobuttons in ViewShareEdit,
     * damit der Nutzer die in einer Meldung genannte Einstellung im Dialog
     * direkt wiederfindet.
     *
     * @param type  Der Update-Typ.
     * @return Die deutsche Beschriftung, bei unbekanntem Wert "Unbekannt".
     */
    static QString updateTypeLabel(ShareUpdateType type);

    /**
     * @brief Baut den Text der Start-Meldung über fehlende Tageswerte.
     *
     * Feature 06.08.2026. Als `public static` herausgezogen (gleiche
     * Begründung wie bei buildDailyValuesUrl()/shouldMinimizeToTray() weiter
     * oben) — der eigentliche Inhalt der Meldung ist damit gegen feste
     * Eingaben prüfbar, ohne ein MainWindow zu bauen und ohne den modalen
     * Dialog zu öffnen.
     *
     * Der Anlass war die Fehlerform: liefe der Textaufbau falsch, bliebe die
     * Meldung leer oder nennte falsche Beschriftungen — und das Ausbleiben
     * eines Dialogs sieht für den Nutzer genauso aus wie "alles in Ordnung".
     * Ein solcher Fehler würde also nie auffallen.
     *
     * @param shares  Aktien mit Bestand, die keine Tageswerte abrufen.
     * @return Fertiger Meldungstext, oder ein leerer String bei leerer Liste.
     */
    static QString buildDailyValuesWarningMessage(
        const QList<ShareUpdateRules::ShareState>& shares);

    /**
     * @brief Ein Split, dessen gespeicherter `prices_adjusted`-Zustand der
     * aus der Kurshistorie erkannten Bereinigung widerspricht, angereichert
     * um Aktienname/WKN für die Meldung.
     *
     * Phase 4 der Aktiensplit-Behandlung (siehe ARCHITECTURE.md, "Offene
     * Punkte"). `SplitAdjustmentAudit::Discrepancy` selbst kennt nur den
     * Split (siehe dort) — Name und WKN sind hier ergänzt, damit
     * buildSplitAdjustmentWarningMessage() sie für den Text verwenden kann,
     * ohne dafür je Zeile erneut die Datenbank abzufragen.
     */
    struct SplitAdjustmentWarning
    {
        QString shareName;
        QString wkn;
        SplitAdjustmentAudit::Discrepancy discrepancy;
    };

    /**
     * @brief Baut den Text der Start-Meldung über Splits mit abweichendem
     * Bereinigungs-Zustand.
     *
     * Phase 4 der Aktiensplit-Behandlung (siehe ARCHITECTURE.md, "Offene
     * Punkte", "Aktiensplits werden nicht behandelt"). Als `public static`
     * herausgezogen, gleiche Begründung wie bei
     * buildDailyValuesWarningMessage() oben: der Meldungstext bleibt damit
     * ohne MainWindow und ohne modalen Dialog prüfbar.
     *
     * @param warnings  Splits, deren gespeicherter Zustand widerspricht.
     * @return Fertiger Meldungstext, oder ein leerer String bei leerer Liste.
     */
    static QString buildSplitAdjustmentWarningMessage(
        const QList<SplitAdjustmentWarning>& warnings);

    /**
     * @brief Restore the main window to the foreground.
     *
     * Hides the tray icon again (no-op if it wasn't shown) and brings the
     * window back via `showNormal()` + `raise()` + `activateWindow()`.
     * Used for two independent purposes, both connected in `setupTrayIcon()`
     * and `main.cpp` respectively:
     * - the tray icon's own single-click activation / its "Anzeigen"
     *   context-menu action (m_actionTrayShow) — reversing a
     *   minimize-to-tray (Feature 03.08.2026, "Minimieren wahlweise in
     *   Taskleiste oder Tray");
     * - `SingleInstanceGuard::activationRequested()` — bringing this,
     *   the already-running instance, to the foreground when a second
     *   launch attempt is detected (Feature 03.08.2026, "Die Anwendung
     *   darf nur einmal gestartet werden"), regardless of whether the
     *   window is currently hidden in the tray, minimized, or simply
     *   behind other windows.
     *
     * Public specifically for the second use case above — main.cpp
     * connects to it directly without any MainWindow-internal trigger.
     */
    void restoreFromTray();

protected:
    /**
     * @brief Intercepts the minimize state change to optionally route it to the tray.
     *
     * Feature 03.08.2026: when the window becomes minimized and
     * shouldMinimizeToTray() returns true (AppSettings::trayOnMinimizeEnabled()
     * is set and a tray icon is available), the window is hidden and the
     * tray icon (m_trayIcon) is shown instead of the normal taskbar
     * minimize — restoreFromTray() reverses this. The actual hide() is
     * deferred via `QTimer::singleShot(0, ...)` rather than called directly
     * from within changeEvent() — same idiom already used elsewhere in this
     * class for post-event-loop deferred operations — to avoid interfering
     * with the platform's own in-progress window-state transition.
     */
    void changeEvent(QEvent* event) override;

    /**
     * @brief Scopes drag&drop handling to m_documentCaptureGroup only.
     *
     * "Direkte Dokumentenerfassung" (Feature 27.07.2026): rather than
     * overriding dragEnterEvent()/dropEvent() on the whole MainWindow (which
     * would trigger on a PDF dropped anywhere, e.g. onto the portfolio
     * table), setupCentralWidget() calls
     * `m_documentCaptureGroup->setAcceptDrops(true)` +
     * `m_documentCaptureGroup->installEventFilter(this)` — this filter then
     * only reacts to QEvent::DragEnter/QEvent::Drop on that one widget.
     * Accepts exactly one dropped `*.pdf`; anything else (multiple files,
     * non-PDF) is rejected with a status message (multi-file) or silently
     * ignored (wrong type), per Nessies Vorgabe vom 27.07.2026 — siehe
     * ARCHITECTURE.md, "Direkte Dokumentenerfassung per Drag+Drop".
     */
    bool eventFilter(QObject* watched, QEvent* event) override;

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

    /**
     * @brief Öffnet ChartPopup (rahmenloses Popup mit nur Graph + Legende)
     * für eine rechtsgeklickte Portfolio-Zeile — Pendant zu
     * onPortfolioRowDoubleClicked(), aber für Rechtsklick statt Doppelklick
     * und ChartPopup statt ViewShareDetails (Feature 31.07.2026, siehe
     * ARCHITECTURE.md, "ChartPopup — Rechtsklick-Popup-Chart").
     *
     * Verbunden mit customContextMenuRequested() beider Portfolio-Tabellen
     * statt einem echten Kontextmenü — Qt::CustomContextMenu wird hier
     * bewusst zweckentfremdet, um Rechtsklicks direkt abzufangen, ohne ein
     * natives Kontextmenü zu zeigen (Nessies Vorgabe: einfacher Rechtsklick
     * öffnet das Popup unmittelbar).
     *
     * Gleiche GUID-Ermittlung wie onPortfolioRowDoubleClicked() (WKN-Zelle,
     * Spalte 0, Qt::UserRole) — anders als dort aber keine
     * hasValidShare()-Prüfung nötig: eine leere/unbekannte GUID führt in
     * ChartPopup/PresenterChart lediglich zum "keine Kursdaten"-Leerzustand,
     * kein modaler Fehlerdialog.
     *
     * @param pos  Position (in Tabellen-Koordinaten) des Rechtsklicks — von
     *             customContextMenuRequested() geliefert.
     */
    void onPortfolioRowRightClicked(const QPoint& pos);

    /**
     * @brief Called when PdfTextExtractor finishes converting a document
     * dropped onto "Direkte Dokumentenerfassung" (Feature 27.07.2026).
     *
     * On success: classifies the text via DocumentClassifier and routes to
     * openCaptureDialog(). On failure: status message only, no dialog.
     *
     * @param success  true if pdftotext produced text.
     * @param text     Extracted plain text, empty on failure.
     */
    void onDocumentCaptureTextExtracted(bool success, const QString& text);

    /**
     * @brief Entry point for a single PDF dropped onto "Direkte Dokumentenerfassung".
     *
     * Kicks off m_documentCaptureExtractor; classification and dialog
     * routing happen in onDocumentCaptureTextExtracted() once the text is
     * ready.
     *
     * Declared as a slot (27.07.2026, corrected from an earlier plain
     * private method that was wrongly documented as "directly callable from
     * tests" — a private non-slot method has no such access) solely so unit
     * tests can invoke it via QMetaObject::invokeMethod without a real
     * Qt drag&drop — same reasoning and pattern as selectShareRow()/
     * selectFirstShareRow() above. No behavioral difference otherwise;
     * called directly (not via invokeMethod) from eventFilter().
     *
     * @param pdfPath  Full path to the dropped PDF file.
     */
    void handleDroppedDocument(const QString& pdfPath);

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

    // ── Direkte Dokumentenerfassung (Drag+Drop, Feature 27.07.2026) ────────
    // handleDroppedDocument() ist jetzt privater Slot (siehe oben,
    // "private slots:"), resolveShareGuidForDocument() public static (siehe
    // "public:" ganz oben) — beide aus demselben Grund aus diesem rein
    // privaten Block herausgezogen: Testbarkeit.

    /**
     * @brief Open the correct edit dialog for a classified, dropped document.
     *
     * - Buy + unknown share  → ViewShareAdd (legt Aktie + Kauf gemeinsam an).
     * - Buy + known share    → ViewBuyEdit(shareGuid, ...).
     * - Sale/Dividend + known share   → ViewSaleEdit/ViewDividendEdit(shareGuid, ...).
     * - Sale/Dividend + unknown share → Statusmeldung, kein Dialog (fachlich
     *   nicht möglich: Verkauf/Dividende ohne vorhandene Aktie).
     * - Brokerage → Statusmeldung, kein Dialog (bewusst außen vor, siehe
     *   ARCHITECTURE.md).
     *
     * Each opened dialog receives the already-extracted document via its
     * presenter's onDocumentSelected(pdfPath) — same effect as the user
     * clicking "…" manually, so the document re-parses inside that dialog's
     * own, unmodified pipeline (a second pdftotext run; see ARCHITECTURE.md
     * for why this deliberate small redundancy was chosen over threading
     * pre-extracted text through all four presenters). Not directly
     * unit-tested — opens real, modal QDialogs (dlg.exec()), same as the
     * rest of the codebase never unit-tests real dialog-exec() flows.
     *
     * @param pdfText   Plain text extracted from the dropped PDF.
     * @param pdfPath   Full path to the dropped PDF file.
     * @param docType   Document type determined by DocumentClassifier.
     * @param docEntry  Matching DocumentEntry for the WKN/ISIN lookup.
     */
    void openCaptureDialog(const QString& pdfText,
                           const QString& pdfPath,
                           DocumentType docType,
                           const DocumentEntry& docEntry);

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
     *
     * @note Seit 06.08.2026 gibt es einen zweiten Unterschied: der
     * Test-Konstruktor setzt m_showStartupWarnings auf false, damit beim
     * Aufbau keine modalen Hinweis-Dialoge erscheinen.
     */
    void initialize();

    void setupActions();
    void setupMenuBar();
    void setupToolBar();
    void setupCentralWidget();
    void setupStatusBar();
    void restoreWindowGeometry();

    /**
     * @brief Create the tray icon and its context menu, if a tray is available.
     *
     * Feature 03.08.2026: no-op (m_trayIcon stays nullptr) if
     * `QSystemTrayIcon::isSystemTrayAvailable()` is false — e.g. some Linux
     * desktop environments without a notification area, or headless/offscreen
     * CI. The icon itself is only made visible while the window is actually
     * hidden into the tray (see changeEvent()/restoreFromTray()), not shown
     * permanently once created.
     */
    void setupTrayIcon();

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
     * @brief Compute the display string for the portfolio-wide "Letzte
     * Aktualisierung" im Portfolio-Label (Feature 21.07.2026).
     *
     * Liest MAX(last_internet_update) über alle Aktien via
     * ShareRepository::maxLastInternetUpdate() und formatiert das Ergebnis
     * mit der App-Locale (QLocale::ShortFormat) — gleiche Konvention wie
     * PresenterShareDetails::formatDateTime(). Fällt auf "-" zurück, wenn
     * noch keine Aktie jemals aktualisiert wurde (oder das Portfolio leer ist).
     */
    QString formatLastPortfolioUpdate() const;

    /**
     * @brief Formats a money value with an explicit sign, e.g. "+123,45 €"
     * or "-42,00 €" (locale-aware via QLocale). Exakt 0 bekommt bewusst KEIN
     * "+" (Nessies Vorgabe 02.08.2026) — "0,00 €", nicht "+0,00 €", da ein
     * neutraler Wert kein Vorzeichen tragen soll.
     *
     * Feature 02.08.2026 ("Vortag"-Tooltip): dieselbe Vorzeichen-Formatierung,
     * die bereits lokal für prevDiffStr/prevPctStr dupliziert war
     * (populatePortfolioTables()/onMarketValuesUpdated()), hier einmalig als
     * Hilfsmethode, da sie jetzt zusätzlich für den Tooltip-Text der
     * Vortag-Spalte (Grid + Footer-Gesamtsumme) gebraucht wird.
     */
    QString formatSignedMoney(double value) const;

    /**
     * @brief Wraps a preformatted tooltip text fragment in a colored HTML
     * `<span>` (rot/grün je nach Entwicklung, wie im Grid selbst).
     *
     * Feature 02.08.2026 (Vortag-Tooltip, Nessies Vorgabe): `QToolTip`
     * unterstützt einen Rich-Text-Teilsatz (Qt erkennt "sieht nach HTML aus"
     * automatisch via `Qt::mightBeRichText()`) — ein `<span
     * style="color:...">`-Tag genügt, kein `<html>`-Wrapper nötig. `color`
     * kommt aus derselben `perfColor()`-Lambda, die auch die Grid-Zellen
     * selbst einfärbt, daher garantiert farblich konsistent.
     */
    QString colorizeToolTip(const QString& text, const QColor& color) const;

    /**
     * @brief Formats a signed money value for a tooltip, colored only when
     * non-zero (used both for the per-unit price movement and the total
     * result in the "Vortag"-Tooltip — each colored by its own sign).
     *
     * Bugfix 02.08.2026: Bei exakt 0 wird bewusst KEIN `<span
     * style="color:...">` angewendet — weder Farbe noch führendes "+". Ein
     * expliziter Farb-Span mit `palette().color(QPalette::Text)` (der Farbe,
     * die `perfColor()` für den Neutral-Fall liefert) rendert im `QToolTip`
     * sichtbar gräulich statt sattem Schwarz, da `QToolTip` intern eine
     * eigene Palette (`ToolTipText`) statt der `MainWindow`-Palette
     * verwendet. Reiner Text ohne Span übernimmt automatisch dieselbe
     * (korrekte) Standard-Tooltip-Textfarbe wie der übrige, ungefärbte
     * Tooltip-Text.
     */
    QString formatSignedMoneyMaybeColored(double value, const QColor& color) const;

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
     * @brief Baut den Basis-Fenstertitel "Share Portfolio Manager (Version X.Y.Z)".
     *
     * Feature (01.08.2026): Versionsnummer im Fenstertitel, dynamisch aus
     * `QCoreApplication::applicationVersion()` — dieselbe App-weite Quelle,
     * die `AboutForm` bereits verwendet (siehe main.cpp,
     * `app.setApplicationVersion(SPM_VERSION_STRING)`, aus der von CMake
     * generierten Version.h). Kein zusätzlicher Header-Include (Version.h)
     * hier nötig, kein zweiter Versionsbump-Ort — siehe ARCHITECTURE.md,
     * Abschnitt "Versionierung".
     *
     * Im selben Zug (Bugfix 01.08.2026) verwendet als alleinige Quelle für
     * den Fenstertitel — der zuvor zusätzlich per updateWindowTitle()
     * angehängte Portfolio-Dateiname wurde entfernt, siehe dort.
     */
    QString baseWindowTitle() const;

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
     * @brief Weist beim Start auf Aktien hin, die trotz Bestand keine
     * Tageswerte abrufen.
     *
     * Feature 06.08.2026. Aktien mit Update-Typ "Markt-Preis" oder "Keine"
     * bauen keine Tageswert-Historie auf und werden deshalb vollständig aus
     * dem Depotwert-Chart ausgeschlossen — der dort gezeigte Verlauf lässt
     * diese Positionen stillschweigend weg. Seit demselben Datum lässt sich
     * die Einstellung in ViewShareEdit gar nicht mehr so wählen; für Aktien,
     * die vor der Umstellung angelegt wurden, braucht es aber einen aktiven
     * Hinweis, sonst bleibt der Fehlstand unbemerkt.
     *
     * Läuft einmal je Programmstart, nicht bei jedem Neuaufbau der Tabellen.
     * Die Liste selbst entsteht ohne zusätzliche Datenbankzugriffe in
     * populatePortfolioTables(), das den Bestand je Aktie ohnehin berechnet.
     *
     * @see m_sharesMissingDailyValues
     */
    void warnAboutSharesWithoutDailyValues();

    /**
     * @brief Füllt m_splitAdjustmentWarnings neu, über alle Aktien mit
     * mindestens einem Split.
     *
     * Phase 4 der Aktiensplit-Behandlung (siehe ARCHITECTURE.md, "Offene
     * Punkte"). Läuft einmal je Programmstart (analog
     * warnAboutSharesWithoutDailyValues()) — anders als
     * m_sharesMissingDailyValues braucht das Ergebnis hier je Aktie mit
     * Splits einen eigenen Datenbankzugriff (Splits + komplette
     * Kurshistorie), daher bewusst NICHT Teil von populatePortfolioTables()
     * (das bei jeder Tabellen-Neuaufbau läuft, auch nach einzelnen
     * Beleg-Änderungen ohne jeden Bezug zu Splits).
     *
     * @see refreshSplitAdjustmentWarningsForShare() für die gezielte
     * Aktualisierung einer einzelnen Aktie nach ihrem Tageswert-Abruf.
     */
    void populateSplitAdjustmentWarnings();

    /**
     * @brief Prüft eine einzelne Aktie erneut und aktualisiert ihren Anteil
     * an m_splitAdjustmentWarnings.
     *
     * Phase 4 der Aktiensplit-Behandlung — "automatische Nachprüfung des
     * prices_adjusted-Zustands nach jedem Tageswert-Abruf" (siehe
     * ARCHITECTURE.md, "Offene Punkte"). Aufgerufen aus
     * onDailyValuesUpdated() nach jedem erfolgreichen Abruf, unabhängig
     * davon, ob dabei neue Tageswerte hinzukamen — ein Split kann auch ohne
     * neuen Tageswert-Abruf zwischenzeitlich angelegt oder geändert worden
     * sein. Schreibt nichts in die Datenbank, siehe SplitAdjustmentAudit.h.
     *
     * @param shareGuid  GUID der geprüften Aktie.
     * @param shareName  Name der Aktie, für die Statusmeldung.
     * @param wkn        WKN der Aktie, für eine spätere Startmeldung.
     * @return Anzahl der für diese Aktie gefundenen Widersprüche.
     */
    int refreshSplitAdjustmentWarningsForShare(const QString& shareGuid,
                                               const QString& shareName,
                                               const QString& wkn);

    /**
     * @brief Weist beim Start auf Splits mit abweichendem
     * Bereinigungs-Zustand hin.
     *
     * Phase 4 der Aktiensplit-Behandlung (siehe ARCHITECTURE.md, "Offene
     * Punkte"), analog warnAboutSharesWithoutDailyValues() oben.
     *
     * @see m_splitAdjustmentWarnings
     */
    void warnAboutSplitAdjustmentDiscrepancies();

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
     * @brief Liefert die Tabelle des aktuell sichtbaren Portfolio-Tabs.
     *
     * Seit dem Depotwert-Chart (05.08.2026) liegen die beiden Tabellen nicht
     * mehr auf den Indizes 0 und 1: der Chart-Tab sitzt dazwischen. Statt an
     * mehreren Stellen auf konkrete Indizes zu prüfen, kapselt dieser Helfer
     * die Zuordnung an einer Stelle.
     *
     * @return Zeiger auf die Tabelle, oder nullptr auf dem Chart-Tab.
     */
    QTableWidget* activePortfolioTable() const;

    /**
     * @brief Erzeugt den Depotwert-Chart beim ersten Betreten seines Tabs
     * bzw. lässt ihn neu rechnen, wenn seine Daten veraltet sind.
     *
     * Bewusst verzögert: die Aggregation über alle Aktien und deren gesamte
     * Tageswert-Historie ist der teure Teil und soll nicht bei jedem
     * Portfolio-Ladevorgang mitlaufen, sondern nur, wenn der Tab auch
     * tatsächlich angesehen wird (Nessies Vorgabe 05.08.2026).
     */
    void ensurePortfolioChartUpToDate();

    /**
     * @brief Markiert die Chart-Daten als veraltet.
     *
     * Aufgerufen nach einer Kursaktualisierung und bei jedem Portfoliowechsel.
     * Neu gerechnet wird erst beim nächsten Betreten des Tabs.
     */
    void invalidatePortfolioChart();

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

    /** Tab-Indizes des Portfolio-Tabwidgets. Der Chart sitzt direkt hinter
     *  dem Depotwert-Grid (Nessies Vorgabe 05.08.2026). */
    static constexpr int kTabFinalValue     = 0;
    static constexpr int kTabPortfolioChart = 1;
    static constexpr int kTabMarketValue    = 2;

    /** Aktien mit Bestand > 0, die keine Tageswerte abrufen — bei jedem
     *  populatePortfolioTables() neu gefüllt, ausgewertet von
     *  warnAboutSharesWithoutDailyValues(). */
    QList<ShareUpdateRules::ShareState> m_sharesMissingDailyValues;

    /** Splits mit abweichendem Bereinigungs-Zustand — von
     *  populateSplitAdjustmentWarnings() (Programmstart) bzw.
     *  refreshSplitAdjustmentWarningsForShare() (je Tageswert-Abruf)
     *  gefüllt, ausgewertet von warnAboutSplitAdjustmentDiscrepancies().
     *  Phase 4 der Aktiensplit-Behandlung, siehe ARCHITECTURE.md. */
    QList<SplitAdjustmentWarning> m_splitAdjustmentWarnings;

    /**
     * @brief Dürfen beim Start modale Hinweis-Dialoge erscheinen?
     *
     * Nur der Produktivkonstruktor lässt sie zu; der Test-Konstruktor
     * MainWindow(QNetworkAccessManager*, QWidget*) setzt das Flag auf false
     * (06.08.2026).
     *
     * Grund: warnAboutSharesWithoutDailyValues() öffnet einen modalen
     * OwnMessageBox per exec(). Zahlreiche Tests in tst_mainwindow.cpp legen
     * Aktien mit Update-Typ MarketPrice samt Käufen an — also genau den Fall,
     * den die Meldung anprangert — und rufen anschliessend
     * QApplication::processEvents(). Der verzögerte Aufruf würde dort feuern
     * und den Test in exec() dauerhaft blockieren, im CI-Runner ebenso.
     *
     * @note Damit bleibt die Meldung selbst untestbar. Das ist konsistent mit
     * der bestehenden Projektkonvention, exec()-getriebene Dialogpfade nicht
     * zu automatisieren (siehe openCaptureDialog()). Die Regel dahinter ist
     * über tst_shareupdaterules abgedeckt, ihre Anwendung im Presenter über
     * die PresenterShareEdit-Tests weiter unten in dieser Datei.
     */
    bool m_showStartupWarnings = true;

    QWidget*            m_portfolioChartContainer = nullptr; ///< Platzhalter, siehe ensurePortfolioChartUpToDate()
    ViewPortfolioChart* m_portfolioChart          = nullptr; ///< erst beim ersten Betreten erzeugt
    bool                m_portfolioChartDirty     = true;    ///< siehe invalidatePortfolioChart()

    // ── Bottom panel — left ───────────────────────────────────────────────
    QGroupBox*    m_statusMessageGroup    = nullptr;
    QTextEdit*    m_statusMessageText     = nullptr;

    // ── Bottom panel — right ──────────────────────────────────────────────
    QGroupBox*    m_documentCaptureGroup  = nullptr;
    QLineEdit*    m_documentCaptureEdit   = nullptr;

    /// PDF→Text-Konvertierung für Direkte Dokumentenerfassung (Feature 27.07.2026).
    PdfTextExtractor m_documentCaptureExtractor;
    /// Pfad des zuletzt abgelegten Dokuments — zwischen handleDroppedDocument()
    /// und onDocumentCaptureTextExtracted() gültig (analog m_pendingPdfPath in
    /// den vier Presentern).
    QString          m_pendingCaptureDocumentPath;

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
    QAction* m_actionTraySettings         = nullptr;

    // ── Actions — API Settings ────────────────────────────────────────────
    QAction* m_actionApiKeyYahoo          = nullptr;

    // ── Actions — Help ────────────────────────────────────────────────────
    QAction* m_actionAbout                = nullptr;

    // ── Tray (Feature 03.08.2026) ─────────────────────────────────────────
    QSystemTrayIcon* m_trayIcon           = nullptr; ///< nullptr wenn kein Tray verfügbar (isSystemTrayAvailable())
    QAction*         m_actionTrayShow     = nullptr; ///< "Anzeigen" im Tray-Kontextmenü
};

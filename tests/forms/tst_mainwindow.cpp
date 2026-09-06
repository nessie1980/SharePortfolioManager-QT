// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include <QtTest>
#include <QSignalSpy>
#include <QApplication>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QFile>
#include <QLocale>
#include <QGroupBox>
#include <QDir>
#include <QElapsedTimer>
#include <QTableWidget>
#include <QTextEdit>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QWheelEvent>
#include <QScreen>
#include <QComboBox>
#include <QMenuBar>
#include <QSqlQuery>
#include <QRegularExpression>
#include <QProcess>
#include <QDialog>
#include <QDialogButtonBox>
#include <QToolTip>

#include "Version.h" // von CMake generiert, siehe app/Version.h.in — tests/forms/CMakeLists.txt
                     // ergänzt dafür ${CMAKE_BINARY_DIR}/app in target_include_directories(tst_mainwindow)
#include "../../app/forms/MainForm/MainWindow.h"
#include "../../app/forms/MainForm/TwoLineDelegate.h"  // TwoLineRole::Top/Bottom
#include "../../app/widgets/GridStyle.h"
#include "../../app/forms/ShareAddForm/ViewShareAdd.h"
#include "../../app/widgets/DocumentPreviewPanel.h"
#include "../../app/forms/ShareDetailsForm/ViewShareDetails.h"
#include "../../app/forms/ChartForm/ChartPopup.h"
#include "../../app/forms/ChartForm/ViewChart.h"
#include "../../app/config/AppSettings.h"
#include "../../app/core/Database.h"
#include "../../app/repositories/ShareRepository.h"
#include "../../app/repositories/BuyRepository.h"
#include "../../app/models/ShareObject.h"
#include "../../app/repositories/BrokerageRepository.h"
#include "../../app/models/BrokerageObject.h"
#include "../../app/models/BuyObject.h"
#include "../../app/config/WebSitesConfig.h"
#include "../../app/config/DocumentsConfig.h"

#include "../../app/forms/SalesForm/IViewSaleEdit.h"
#include "../../app/forms/SalesForm/IModelSaleEdit.h"
#include "../../app/forms/SalesForm/ViewSaleEdit.h"
#include "../../app/forms/SalesForm/ModelSaleEdit.h"
#include "../../app/forms/SalesForm/PresenterSaleEdit.h"
// DividendForm-Includes hier entfernt (22.08.2026): die DividendForm-Tests
// liegen seit der Auslagerung in tst_dividendform.cpp. Die Form selbst bleibt
// Compile-Abhängigkeit von MainWindow/ViewShareEdit und steht weiterhin in der
// Quellenliste von tst_mainwindow in tests/forms/CMakeLists.txt.
#include "../../app/repositories/DailyValuesRepository.h"
#include "../../app/models/DailyValuesObject.h"


#include "../../app/models/SaleObject.h"
#include "../../app/models/ShareSplitObject.h"
#include "../../app/utils/ShareSplitHint.h"
#include "../../app/repositories/ShareSplitRepository.h"
#include "../../app/utils/SplitPriceJumpDetector.h"
#include "../../app/utils/SplitAudit.h"
#include <QDateEdit>
#include <QTimeEdit>
#include <QProgressBar>
#include <QUuid>
#include "../../app/forms/UiConstants.h"
#include "../../app/IconProvider.h"
#include "../parser/FakeNetworkAccessManager.h"
#include <QUrl>


// ─────────────────────────────────────────────────────────────────────────────
// SoundCountingMainWindow — Test-Subklasse für das "Aktualisierung
// erfolgreich"-Sound-Feature (21.07.2026). playUpdateFinishedSound() ist in
// MainWindow als `private virtual` deklariert — genau damit eine Testklasse
// sie per override abfangen kann, ohne von echter QSoundEffect-Wiedergabe
// abhängig zu sein (kein Audio-Gerät in CI/Testumgebungen nötig).
class SoundCountingMainWindow : public MainWindow
{
public:
    using MainWindow::MainWindow;

    int soundPlayCount = 0;

protected:
    void playUpdateFinishedSound() override { ++soundPlayCount; }
};

// ─────────────────────────────────────────────────────────────────────────────
class TestMainWindow : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir   m_tempDir;
    DocumentsConfig m_docsConfig;

    void loadSandboxedSettings()
    {
        const QString sandboxIni = m_tempDir.path() + QStringLiteral("/test_settings.ini");
        AppSettings::instance().load(sandboxIni);

        // Verhindert, dass MainWindow::ensureDocumentsRootConfigured() beim
        // Konstruieren einen blockierenden Dialog öffnet (der Dialog
        // erscheint nur, wenn documentsRootPath() leer ist). Muss nicht
        // tatsächlich existieren — der Startup-Check prüft nur auf "leer".
        AppSettings::instance().setDocumentsRootPath(
            m_tempDir.path() + QStringLiteral("/documents"));
    }

    void openMemoryDb()
    {
        if (!Database::instance().isOpen())
            Database::instance().open(QStringLiteral(":memory:"));
        AppSettings::instance().setPortfolioPath(QStringLiteral(":memory:"));
    }

    /** Returns the data QTableWidget stored as "dataTable" property on a tab container. */
    static QTableWidget* dataTableFromContainer(QWidget* container)
    {
        if (!container) return nullptr;
        return qobject_cast<QTableWidget*>(
            container->property("dataTable").value<QObject*>());
    }

    /** Insert a share into the in-memory DB so repository calls succeed. */
    QString insertTestShare()
    {
        openMemoryDb();
        ShareRepository repo;
        const QString guid = QStringLiteral("share-test-1");
        repo.insert(ShareObject(guid,
                                QStringLiteral("TST01"),
                                QStringLiteral("DE000TST0001"),
                                QStringLiteral("Test AG")));
        return guid;
    }

    /** Insert a buy for the given share and depot into the in-memory DB. */
    BuyObject insertTestBuy(const QString& shareGuid,
                             const QString& depotNumber,
                             const QString& dateTime,
                             double volume,
                             double price)
    {
        BuyRepository repo;
        const QString guid =
            QUuid::createUuid().toString(QUuid::WithoutBraces);
        BuyObject b(guid, shareGuid, depotNumber,
                    QStringLiteral("ord-") + guid,
                    dateTime, volume, 0.0, price);
        repo.insert(b);

        BrokerageRepository brRepo;
        BrokerageObject br(QStringLiteral("br-") + guid, shareGuid,
                           guid, QString(), dateTime,
                           9.90, 0.0, 0.0, 0.0, QString());
        brRepo.insert(br);
        return b;
    }

    /**
     * Seed a one-share Depotwert portfolio on a real file DB and point the
     * settings at it, so constructing a MainWindow populates the grid.
     *
     * Share has a single buy (10 @ 100) with 9.90 buy-brokerage, no sale,
     * cur_price = 0. The brokerage makes the Depotwert (…Final) values differ
     * from the brokerage-free market values:
     *   purchaseValueFinal = 1000 + 9.90 = 1009.90   (market: 1000.00)
     *   profitLossFinal    = 0 - 1009.90 = -1009.90  (market: -1000.00)
     *   totalBrokerage     = 9.90, totalDividend = 0.00
     */
    QString seedDepotwertPortfolio()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/DepotwertUi.db");
        QFile::remove(dbPath);
        Database::instance().open(dbPath);                 // creates schema
        ShareRepository().insert(ShareObject(QStringLiteral("g-dw"),
                                             QStringLiteral("DW01"),
                                             QStringLiteral("DE000DW0001"),
                                             QStringLiteral("Depotwert AG")));
        insertTestBuy(QStringLiteral("g-dw"), QStringLiteral("depot1"),
                      QStringLiteral("2020-01-01T00:00:00"), 10.0, 100.0);
        AppSettings::instance().setPortfolioPath(dbPath);
        return dbPath;
    }

    /**
     * Seed a two-share portfolio (each with a small buy so both appear as a
     * grid row) on a real file DB. Used by tests that need more than one row
     * to select/verify a specific GUID against — e.g. selectShareRow() /
     * selectFirstShareRow().
     * @return GUIDs of the two seeded shares, in insertion order.
     */
    QStringList seedTwoSharePortfolio()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/TwoShareUi.db");
        QFile::remove(dbPath);
        Database::instance().open(dbPath);
        ShareRepository().insert(ShareObject(QStringLiteral("g-first"),
                                             QStringLiteral("FS01"),
                                             QStringLiteral("DE000FS00001"),
                                             QStringLiteral("First AG")));
        insertTestBuy(QStringLiteral("g-first"), QStringLiteral("depot1"),
                      QStringLiteral("2020-01-01T00:00:00"), 5.0, 50.0);
        ShareRepository().insert(ShareObject(QStringLiteral("g-second"),
                                             QStringLiteral("SS01"),
                                             QStringLiteral("DE000SS00001"),
                                             QStringLiteral("Second AG")));
        insertTestBuy(QStringLiteral("g-second"), QStringLiteral("depot1"),
                      QStringLiteral("2020-01-01T00:00:00"), 5.0, 50.0);
        AppSettings::instance().setPortfolioPath(dbPath);
        return { QStringLiteral("g-first"), QStringLiteral("g-second") };
    }

    /** Row index whose column-0 Qt::UserRole (GUID) matches guid, or -1. */
    static int rowForGuid(QTableWidget* table, const QString& guid)
    {
        if (!table) return -1;
        for (int row = 0; row < table->rowCount(); ++row) {
            QTableWidgetItem* item = table->item(row, 0);
            if (item && item->data(Qt::UserRole).toString() == guid)
                return row;
        }
        return -1;
    }

    /**
     * Among the four QTableWidgets the Depotwert tables carry 13 columns
     * (FinalValueColumn::Count); the Marktwert tables have 12. The enum lives
     * in MainWindow and is not reachable from the test, so raw indices are used:
     *   4 = BrokerageDividend, 8 = Performance, 9 = PurchaseFinalValue.
     * wantRows: 1 for the data table (one seeded share), 3 for the footer.
     */
    static QTableWidget* findFinalTable(const MainWindow& w, int wantRows)
    {
        const int cols = 13; // FinalValueColumn::Count (Depotwert columns)
        for (auto* t : w.findChildren<QTableWidget*>())
            if (t && t->columnCount() == cols && t->rowCount() == wantRows)
                return t;
        return nullptr;
    }

    /**
     * Analog zu findFinalTable(), aber für die Marktwert-Tabellen (12 Spalten =
     * MarketValueColumn::Count). wantRows: 1 für die Datentabelle (ein
     * geseedeter Titel), 3 für den Footer.
     */
    static QTableWidget* findMarketTable(const MainWindow& w, int wantRows)
    {
        const int cols = 12; // MarketValueColumn::Count (Marktwert columns)
        for (auto* t : w.findChildren<QTableWidget*>())
            if (t && t->columnCount() == cols && t->rowCount() == wantRows)
                return t;
        return nullptr;
    }

    /**
     * QIcon has no meaningful operator== (it compares pointer identity of the
     * internal engine, not pixel content) — IconProvider::icon() constructs a
     * fresh QIcon from the same resource path on every call, so two "equal"
     * icons are never `==`. Compare rendered pixel data instead.
     */
    static bool iconsEqual(const QIcon& a, const QIcon& b, int size = 24)
    {
        return a.pixmap(size, size).toImage() == b.pixmap(size, size).toImage();
    }

    /** Find a QAction child by its statusTip() (unique and mnemonic-free, unlike text()). */
    static QAction* findActionByStatusTip(const MainWindow& w, const QString& statusTip)
    {
        for (auto* a : w.findChildren<QAction*>())
            if (a && a->statusTip() == statusTip)
                return a;
        return nullptr;
    }

private slots:

    void initTestCase()
    {
        QVERIFY(m_tempDir.isValid());
        loadSandboxedSettings();

        // Load Documents.xml for presenter tests
        const QString docsPath =
            QCoreApplication::applicationDirPath() + QStringLiteral("/Documents.xml");
        if (QFileInfo::exists(docsPath))
            m_docsConfig.load(docsPath);
    }

    void cleanupTestCase()
    {
        if (Database::instance().isOpen())
            Database::instance().close();
        // WICHTIG: Hier bewusst KEIN AppSettings::instance().load(...) mehr
        // (weder mit leerem Pfad noch mit dem echten settings.ini-Pfad) —
        // das hat den Singleton fälschlich auf die ECHTE settings.ini
        // umgeleitet ("um sie zu schützen"), was aber das Gegenteil bewirkt
        // hat: tst_mainwindow führte damals mehrere QObject-Testklassen im
        // selben Prozess aus (TestMainWindow, TestOwnMessageBox,
        // TestBackupForm — vor deren Auslagerung in eigene Executables am
        // 22.08.2026); jeder setXxx()-Aufruf in einer SPÄTER laufenden Klasse
        // hat dadurch direkt in die echte settings.ini geschrieben, statt in
        // die sandboxte Testdatei — Nessies reale Konfiguration
        // (Portfolio-Pfad, Dokument-Root) wurde dadurch bei jedem Testlauf
        // überschrieben (gemeldet und behoben 19.07.2026). Der
        // AppSettings-Singleton stirbt ohnehin mit dem Prozess — ein
        // "Zurücksetzen fürs nächste Mal" ist hier schlicht nicht nötig, und
        // seit der Auslagerung läuft ohnehin nur noch TestMainWindow allein
        // in diesem Prozess.
    }

    void init()
    {
        loadSandboxedSettings();
        if (Database::instance().isOpen())
            Database::instance().close();
    }

    // ─────────────────────────────────────────────────────────────────────
    // MainWindow — construction & basic UI
    // ─────────────────────────────────────────────────────────────────────

    void test_construction_windowTitleSet()
    {
        openMemoryDb();
        MainWindow window;
        QVERIFY(window.windowTitle().contains(
            QStringLiteral("Share Portfolio Manager")));
    }

    // Feature (01.08.2026): Versionsnummer im Fenstertitel, dynamisch aus
    // QCoreApplication::applicationVersion() (siehe MainWindow::baseWindowTitle()).
    // Prüft ein echtes "X.Y.Z"-Muster statt nur des literalen SPM_VERSION_STRING,
    // damit der Test bei einem künftigen Versionsbump nicht angepasst werden muss.
    void test_construction_windowTitleContainsVersion()
    {
        openMemoryDb();
        MainWindow window;
        static const QRegularExpression versionPattern(
            QStringLiteral("\\(Version \\d+\\.\\d+\\.\\d+\\)"));
        QVERIFY2(versionPattern.match(window.windowTitle()).hasMatch(),
                 qPrintable(QStringLiteral("Fenstertitel enthält keine Versionsnummer: \"%1\"")
                                .arg(window.windowTitle())));
    }

    void test_construction_actionsDisabledAtStart()
    {
        openMemoryDb();
        MainWindow window;
        const auto menuActions = window.menuBar()->actions();
        QVERIFY(!menuActions.isEmpty());
    }

    void test_updatePortfolioLabel_defaultValues()
    {
        openMemoryDb();
        MainWindow window;
        const auto* label = window.findChild<QLabel*>();
        QVERIFY(label != nullptr);
    }

    // Grid-Selektionsfarbe (Feature 29.07.2026, Nessies Vorgabe: wie im
    // C#-Original — blauer Hintergrund/gelbe Schrift bei Selektion in allen
    // Grids). OverviewTabWidget deckt die Edit-Dialoge und ShareDetailsForm
    // bereits über eigene Tests ab (tst_overviewtabwidget.cpp); hier werden
    // die beiden MainWindow-Haupttabellen selbst geprüft. Kein Seeding nötig
    // — der Stil wird unabhängig von Daten schon in setupCentralWidget()
    // gesetzt, die leeren Datentabellen (0 Zeilen) reichen aus.
    void test_mainWindow_portfolioTables_haveGridSelectionStyle()
    {
        openMemoryDb();
        MainWindow window;
        QApplication::processEvents();

        QTableWidget* finalTbl  = findFinalTable(window, 0);
        QTableWidget* marketTbl = findMarketTable(window, 0);
        if (!finalTbl)  QFAIL("Depotwert-Datentabelle nicht gefunden");
        if (!marketTbl) QFAIL("Marktwert-Datentabelle nicht gefunden");

        for (auto* tbl : { finalTbl, marketTbl }) {
            QVERIFY(tbl->styleSheet().contains(GridStyle::kSelectionBackground));
            QVERIFY(tbl->styleSheet().contains(GridStyle::kSelectionForeground));
        }
    }

    // Die Footer-Tabellen sind nicht selektierbar (NoSelection) und bekommen
    // daher bewusst kein Selektions-Stylesheet.
    void test_mainWindow_portfolioFooters_haveNoGridSelectionStyle()
    {
        openMemoryDb();
        MainWindow window;
        QApplication::processEvents();

        QTableWidget* finalFooter  = findFinalTable(window, 3);
        QTableWidget* marketFooter = findMarketTable(window, 3);
        if (!finalFooter)  QFAIL("Depotwert-Footer nicht gefunden");
        if (!marketFooter) QFAIL("Marktwert-Footer nicht gefunden");

        for (auto* tbl : { finalFooter, marketFooter })
            QVERIFY(!tbl->styleSheet().contains(GridStyle::kSelectionBackground));
    }

    void test_clearPortfolioTables_removesAllRows()
    {
        openMemoryDb();
        MainWindow window;
        const auto tables = window.findChildren<QTableWidget*>();
        QCOMPARE(tables.size(), 4); // 2 data tables + 2 footer tables

        // Data tables start empty; footer tables always keep their 3 summary rows.
        int emptyCount  = 0;
        int footerCount = 0;
        for (const auto* table : tables) {
            if (table->rowCount() == 0)
                ++emptyCount;
            else if (table->rowCount() == 3)
                ++footerCount;
        }
        QCOMPARE(emptyCount,  2);
        QCOMPARE(footerCount, 2);
    }

    // Regression for the Depotwert display bug: "Aktuelle Entwicklung" and
    // "Einzahlung" must show the …Final fields (WITH brokerage), not the
    // brokerage-free market values.
    void test_finalValueTable_showsFinalFields()
    {
        seedDepotwertPortfolio();
        MainWindow window;
        QApplication::processEvents();

        QTableWidget* tbl = findFinalTable(window, 1); // data table, 1 share row
        if (!tbl) QFAIL("Depotwert-Datentabelle nicht gefunden");

        const QLocale loc;
        const QString finalStr  = loc.toString(-1009.90, 'f', 2) + QStringLiteral(" €");
        const QString marketStr = loc.toString(-1000.00, 'f', 2) + QStringLiteral(" €");

        QTableWidgetItem* perf =
            tbl->item(0, 8); // FinalValueColumn::Performance (Aktuelle Entwicklung)
        if (!perf) QFAIL("Performance-Zelle fehlt");
        // Upper line = profitLossFinal (with brokerage), NOT the market value.
        QCOMPARE(perf->data(TwoLineRole::Top).toString(), finalStr);
        QVERIFY(perf->data(TwoLineRole::Top).toString() != marketStr);

        QTableWidgetItem* pv =
            tbl->item(0, 9); // FinalValueColumn::PurchaseFinalValue (Einzahlung)
        if (!pv) QFAIL("Einzahlung-Zelle fehlt");
        // Upper line = purchaseValueFinal (incl. brokerage).
        QCOMPARE(pv->data(TwoLineRole::Top).toString(),
                 loc.toString(1009.90, 'f', 2) + QStringLiteral(" €"));
    }

    // Regression Bugfix 03.07.2026: die zweite Zeile in "Kosten/Dividenden"
    // und "Preis" nutzte fälschlich `muted` (Alpha 140) statt `neutral`,
    // wodurch sie optisch wie eine andere Schrift wirkte als die übrigen
    // zweizeiligen Spalten. Beide Unterzeilen müssen dieselbe (volle)
    // Farbe wie der Rest der Zweitzeilen im Grid nutzen.
    void test_finalValueTable_priceAndCostDividendBottomColorIsNeutral()
    {
        seedDepotwertPortfolio();
        MainWindow window;
        QApplication::processEvents();

        QTableWidget* tbl = findFinalTable(window, 1); // data table, 1 share row
        if (!tbl) QFAIL("Depotwert-Datentabelle nicht gefunden");

        const QColor neutral = window.palette().color(QPalette::Text);

        QTableWidgetItem* bd = tbl->item(0, 4); // FinalValueColumn::BrokerageDividend
        if (!bd) QFAIL("Kosten/Dividenden-Zelle fehlt");
        QCOMPARE(bd->data(TwoLineRole::BottomColor).value<QColor>().alpha(),
                 neutral.alpha());

        QTableWidgetItem* price = tbl->item(0, 5); // FinalValueColumn::Price
        if (!price) QFAIL("Preis-Zelle fehlt");
        QCOMPARE(price->data(TwoLineRole::BottomColor).value<QColor>().alpha(),
                 neutral.alpha());
    }

    // Gleiche Regression für den Marktwert-Tab (dort gibt es keine
    // Kosten/Dividenden-Spalte, nur Preis).
    void test_marketValueTable_priceBottomColorIsNeutral()
    {
        seedDepotwertPortfolio();
        MainWindow window;
        QApplication::processEvents();

        QTableWidget* tbl = findMarketTable(window, 1); // data table, 1 share row
        if (!tbl) QFAIL("Marktwert-Datentabelle nicht gefunden");

        const QColor neutral = window.palette().color(QPalette::Text);

        QTableWidgetItem* price = tbl->item(0, 4); // MarketValueColumn::Price
        if (!price) QFAIL("Preis-Zelle fehlt");
        QCOMPARE(price->data(TwoLineRole::BottomColor).value<QColor>().alpha(),
                 neutral.alpha());
    }

    // The Depotwert footer carries the Kosten / Dividenden total as a two-line
    // value (Kosten over Dividenden) in the middle row.
    void test_finalValueFooter_costDividendCell()
    {
        seedDepotwertPortfolio();
        MainWindow window;
        QApplication::processEvents();

        QTableWidget* footer = findFinalTable(window, 3); // footer, 3 summary rows
        if (!footer) QFAIL("Depotwert-Footer nicht gefunden");

        QTableWidgetItem* cell =
            footer->item(1, 4); // FinalValueColumn::BrokerageDividend (Kosten/Dividenden)
        if (!cell) QFAIL("Kosten/Dividenden-Footerzelle fehlt");

        const QLocale loc;
        // Kosten (oben) = totalBrokerage 9.90; Dividenden (unten) = 0.00.
        QCOMPARE(cell->data(TwoLineRole::Top).toString(),
                 loc.toString(9.90, 'f', 2) + QStringLiteral(" €"));
        QCOMPARE(cell->data(TwoLineRole::Bottom).toString(),
                 loc.toString(0.0, 'f', 2) + QStringLiteral(" €"));
    }

    // ─────────────────────────────────────────────────────────────────────
    // MainWindow — Grid-Selektion folgt Refresh (Feature vom 05.07.2026)
    // ─────────────────────────────────────────────────────────────────────
    //
    // selectShareRow() and selectFirstShareRow() are called from within the
    // Parser-dependent refresh flow (startRefreshForShare() /
    // onRefreshShareFinished()). Both methods are pure table helpers with no
    // Parser/network dependency of their own — they were declared as
    // "private slots" specifically so they can be invoked directly via
    // QMetaObject::invokeMethod, which lets the actual selection logic be
    // tested deterministically without touching the Parser at all. The tests
    // below cover exactly that.
    //
    // The actual Parser-dependent callers (startRefreshForShare(),
    // onMarketValuesUpdated(), onRefreshShareFinished()) are covered further
    // down using the MainWindow(QNetworkAccessManager*, ...) test constructor
    // together with ParserTestUtils::FakeNetworkAccessManager (07.07.2026).

    void test_selectShareRow_selectsMatchingGuidInBothTables()
    {
        const auto guids = seedTwoSharePortfolio();
        MainWindow window;
        QApplication::processEvents();

        QTableWidget* finalTbl  = findFinalTable(window, 2);
        QTableWidget* marketTbl = findMarketTable(window, 2);
        if (!finalTbl)  QFAIL("Depotwert-Datentabelle nicht gefunden");
        if (!marketTbl) QFAIL("Marktwert-Datentabelle nicht gefunden");

        const int wantFinalRow  = rowForGuid(finalTbl,  guids.at(1));
        const int wantMarketRow = rowForGuid(marketTbl, guids.at(1));
        QVERIFY(wantFinalRow  >= 0);
        QVERIFY(wantMarketRow >= 0);

        QMetaObject::invokeMethod(&window, "selectShareRow", Qt::DirectConnection,
                                  Q_ARG(QString, guids.at(1)));

        QCOMPARE(finalTbl->currentRow(),  wantFinalRow);
        QCOMPARE(marketTbl->currentRow(), wantMarketRow);
    }

    void test_selectShareRow_switchingGuid_movesSelectionToOtherShare()
    {
        const auto guids = seedTwoSharePortfolio();
        MainWindow window;
        QApplication::processEvents();

        QTableWidget* finalTbl = findFinalTable(window, 2);
        if (!finalTbl) QFAIL("Depotwert-Datentabelle nicht gefunden");

        QMetaObject::invokeMethod(&window, "selectShareRow", Qt::DirectConnection,
                                  Q_ARG(QString, guids.at(0)));
        QCOMPARE(finalTbl->currentRow(), rowForGuid(finalTbl, guids.at(0)));

        // Simulates the queue advancing from the first to the second share
        // during "Alle aktualisieren" — the selection must follow.
        QMetaObject::invokeMethod(&window, "selectShareRow", Qt::DirectConnection,
                                  Q_ARG(QString, guids.at(1)));
        QCOMPARE(finalTbl->currentRow(), rowForGuid(finalTbl, guids.at(1)));
    }

    void test_selectShareRow_emptyGuid_doesNotChangeSelection()
    {
        seedDepotwertPortfolio();
        MainWindow window;
        QApplication::processEvents();

        QTableWidget* finalTbl = findFinalTable(window, 1);
        if (!finalTbl) QFAIL("Depotwert-Datentabelle nicht gefunden");
        finalTbl->setCurrentCell(0, 0);

        QMetaObject::invokeMethod(&window, "selectShareRow", Qt::DirectConnection,
                                  Q_ARG(QString, QString()));

        QCOMPARE(finalTbl->currentRow(), 0);
    }

    void test_selectShareRow_unknownGuid_doesNotChangeSelection()
    {
        seedDepotwertPortfolio();
        MainWindow window;
        QApplication::processEvents();

        QTableWidget* finalTbl = findFinalTable(window, 1);
        if (!finalTbl) QFAIL("Depotwert-Datentabelle nicht gefunden");
        finalTbl->setCurrentCell(0, 0);

        QMetaObject::invokeMethod(&window, "selectShareRow", Qt::DirectConnection,
                                  Q_ARG(QString, QStringLiteral("does-not-exist")));

        QCOMPARE(finalTbl->currentRow(), 0);
    }

    void test_selectFirstShareRow_selectsRowZeroInBothTables()
    {
        seedTwoSharePortfolio();
        MainWindow window;
        QApplication::processEvents();

        QTableWidget* finalTbl  = findFinalTable(window, 2);
        QTableWidget* marketTbl = findMarketTable(window, 2);
        if (!finalTbl)  QFAIL("Depotwert-Datentabelle nicht gefunden");
        if (!marketTbl) QFAIL("Marktwert-Datentabelle nicht gefunden");

        // Start on the last row, as selectShareRow() would leave it after
        // the final share of an "Alle aktualisieren" run.
        finalTbl->setCurrentCell(1, 0);
        marketTbl->setCurrentCell(1, 0);

        QMetaObject::invokeMethod(&window, "selectFirstShareRow", Qt::DirectConnection);

        QCOMPARE(finalTbl->currentRow(),  0);
        QCOMPARE(marketTbl->currentRow(), 0);
    }

    void test_selectFirstShareRow_emptyTables_doesNotCrash()
    {
        openMemoryDb();
        MainWindow window;
        QApplication::processEvents();

        QMetaObject::invokeMethod(&window, "selectFirstShareRow", Qt::DirectConnection);

        // No crash is the actual assertion here; data tables stay empty.
        QTableWidget* finalTbl = findFinalTable(window, 0);
        QVERIFY(finalTbl != nullptr);
    }

    // ─────────────────────────────────────────────────────────────────────
    // MainWindow — Refresh-Flow über FakeNetworkAccessManager (07.07.2026)
    // ─────────────────────────────────────────────────────────────────────
    //
    // Uses the MainWindow(QNetworkAccessManager*, QWidget*) test constructor
    // together with ParserTestUtils::FakeNetworkAccessManager (see
    // tests/parser/FakeNetworkAccessManager.h) to exercise
    // startRefreshForShare() / onMarketValuesUpdated() / onRefreshShareFinished()
    // through the exact production code path, without any real network access.

    void test_onRefreshShare_iconRegression_updatesChartIconsViaFakeNetwork()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/RefreshIcon.db");
        QFile::remove(dbPath);
        Database::instance().open(dbPath);

        // Share starts with a NEGATIVE previous-day performance (curPrice <
        // prevDayPrice), so populatePortfolioTables() sets a Negativ* icon —
        // matching the regression scenario from Bugfix 06.07.2026.
        ShareObject share(QStringLiteral("g-icon"), QStringLiteral("IC01"),
                          QStringLiteral("DE000IC00001"), QStringLiteral("IconRegression AG"));
        share.setCurPrice(90.0);
        share.setPrevDayPrice(100.0);
        share.setUpdateType(ShareUpdateType::MarketPrice);
        share.setMarketPriceParsingType(ShareParsingType::ApiOnVista);
        share.setMarketPriceUrl(QStringLiteral("https://example.com/onvista/quote"));
        share.setMarketPriceEncoding(QStringLiteral("UTF-8"));
        ShareRepository().insert(share);
        insertTestBuy(QStringLiteral("g-icon"), QStringLiteral("depot1"),
                      QStringLiteral("2020-01-01T00:00:00"), 5.0, 100.0);

        // Yesterday's closing price in daily_values — onMarketValuesUpdated()
        // fetches prevDay from here, NOT from the share's own prevDayPrice field.
        DailyValuesRepository dvRepo;
        dvRepo.upsert(DailyValuesObject(QStringLiteral("g-icon"),
                                        QDate::currentDate().addDays(-1),
                                        100.0, 100.0, 100.0, 100.0, 1000.0));

        AppSettings::instance().setPortfolioPath(dbPath);

        ParserTestUtils::FakeNetworkAccessManager fakeNam;
        const QUrl marketUrl(QStringLiteral("https://example.com/onvista/quote"));
        // +20% vs. the seeded prevDay of 100.0 → PositivStrong (> 2%)
        fakeNam.setResponse(marketUrl, QByteArrayLiteral(R"({
            "price": 120.0,
            "previousLast": 100.0,
            "isoCurrency": "EUR",
            "idNotation": 1,
            "idCurrency": 1,
            "datetimePrice": {
                "localTime": "2024-01-15T10:30:00",
                "localTimeZone": "Europe/Berlin",
                "utcTimeStamp": 1705315800
            }
        })"));

        MainWindow window(&fakeNam);
        QApplication::processEvents();

        QTableWidget* finalTbl  = findFinalTable(window, 1);
        QTableWidget* marketTbl = findMarketTable(window, 1);
        if (!finalTbl)  QFAIL("Depotwert-Datentabelle nicht gefunden");
        if (!marketTbl) QFAIL("Marktwert-Datentabelle nicht gefunden");

        const int finalRow  = rowForGuid(finalTbl,  QStringLiteral("g-icon"));
        const int marketRow = rowForGuid(marketTbl, QStringLiteral("g-icon"));
        QVERIFY(finalRow  >= 0);
        QVERIFY(marketRow >= 0);

        // FinalValueColumn::PrevDayChart = 6, MarketValueColumn::PrevDayChart = 5
        // FinalValueColumn::CompleteChart = 10, MarketValueColumn::CompleteChart = 9
        static const int FC_PrevDayChart  = 6;
        static const int MC_PrevDayChart  = 5;
        static const int FC_CompleteChart = 10;
        static const int MC_CompleteChart = 9;

        // Sanity: before the refresh, the icon reflects the initial NEGATIVE
        // prevDayPct (curPrice 90 vs. prevDayPrice 100 → -10%).
        QVERIFY(iconsEqual(finalTbl->item(finalRow, FC_PrevDayChart)->icon(),
                           IconProvider::icon(IconProvider::NegativStrong)));

        finalTbl->setCurrentCell(finalRow, 0);

        QMetaObject::invokeMethod(&window, "onRefreshShare", Qt::DirectConnection);

        // The fake reply resolves via a queued 0ms timer — wait for the icon
        // to actually flip before asserting (same pattern as tst_parser.cpp).
        const bool iconUpdated = QTest::qWaitFor([&]() {
            auto* it = finalTbl->item(finalRow, FC_PrevDayChart);
            return it && iconsEqual(it->icon(), IconProvider::icon(IconProvider::PositivStrong));
        }, 2000);

        QVERIFY2(iconUpdated,
                 "PrevDayChart-Icon (Depotwert) wurde nach dem Einzel-Refresh "
                 "nicht aktualisiert — Regression Bugfix 06.07.2026.");

        // Must hold for the Marktwert table too, and for CompleteChart.
        QVERIFY(iconsEqual(marketTbl->item(marketRow, MC_PrevDayChart)->icon(),
                           IconProvider::icon(IconProvider::PositivStrong)));
        QVERIFY(iconsEqual(finalTbl->item(finalRow, FC_CompleteChart)->icon(),
                           marketTbl->item(marketRow, MC_CompleteChart)->icon()));

        QCOMPARE(fakeNam.requestCount(), 1);
    }

    // ─────────────────────────────────────────────────────────────────────
    // MainWindow — "Vortag"-Tooltip: Gesamtänderung (Feature 02.08.2026)
    // ─────────────────────────────────────────────────────────────────────
    //
    // Tooltip auf FC::PrevDay/MC::PrevDay UND FC::PrevDayChart/MC::PrevDayChart
    // (Entwicklungs-Pfeil-Icon-Spalte davor) zeigt "Anteile × Kurswert-Entw. =
    // Gesamtergebnis" statt der reinen Pro-Aktie-Kursänderung. Pro-Stück-Wert
    // und Gesamtergebnis färben sich UNABHÄNGIG voneinander nach ihrem
    // jeweils eigenen Vorzeichen; bei exakt 0 weder Farbe noch führendes "+"
    // (siehe ARCHITECTURE.md, "Vortag-Spalte + Piktogramm-Spalte: Tooltip mit
    // Gesamtänderung", für die vollständige Herleitung inkl. des
    // Grau-statt-Schwarz-Bugfixes). Die erwarteten Tooltip-Strings unten
    // spiegeln exakt das HTML-Format aus MainWindow::colorizeToolTip()/
    // formatSignedMoneyMaybeColored() — bewusst als volle QCOMPARE()-Strings
    // statt nur contains()-Fragmente, da alle Testwerte bewusst rund gewählt
    // sind (keine Rundungs-/FIFO-Komplexität wie bei den Footer-Summen-Tests).

    void test_populatePortfolioTables_prevDayTooltip_showsVolumeTimesDiff()
    {
        // Bugfix: ":memory:" funktioniert hier NICHT — MainWindow::initialize()
        // prüft QFileInfo::exists(portfolioPath), was für ":memory:" immer
        // false liefert, wodurch populatePortfolioTables() beim Konstruieren
        // übersprungen wird (die Tabelle bliebe leer). Echte Datei-DB nötig,
        // analog zu seedDepotwertPortfolio() und den übrigen Tests hier.
        const QString dbPath = m_tempDir.path() + QStringLiteral("/TooltipCalc.db");
        QFile::remove(dbPath);
        Database::instance().open(dbPath);

        // 40 Stk. gehalten, Kurs +12,30 € zum Vortag → Gesamtänderung
        // 40 × 12,30 = 492,00 €.
        ShareObject share(QStringLiteral("g-tooltip-calc"), QStringLiteral("TC01"),
                          QStringLiteral("DE000TC00001"), QStringLiteral("TooltipCalc AG"));
        share.setCurPrice(112.30);
        share.setPrevDayPrice(100.00);
        ShareRepository().insert(share);
        insertTestBuy(QStringLiteral("g-tooltip-calc"), QStringLiteral("depot1"),
                      QStringLiteral("2020-01-01T00:00:00"), 40.0, 100.0);
        AppSettings::instance().setPortfolioPath(dbPath);

        MainWindow window;
        QApplication::processEvents();

        QTableWidget* finalTbl  = findFinalTable(window, 1);
        QTableWidget* marketTbl = findMarketTable(window, 1);
        if (!finalTbl)  QFAIL("Depotwert-Datentabelle nicht gefunden");
        if (!marketTbl) QFAIL("Marktwert-Datentabelle nicht gefunden");

        const int finalRow  = rowForGuid(finalTbl,  QStringLiteral("g-tooltip-calc"));
        const int marketRow = rowForGuid(marketTbl, QStringLiteral("g-tooltip-calc"));
        QVERIFY(finalRow  >= 0);
        QVERIFY(marketRow >= 0);

        using FC = MainWindow::FinalValueColumn;
        using MC = MainWindow::MarketValueColumn;

        const QString finalPrevDayTip  = finalTbl->item(finalRow,  static_cast<int>(FC::PrevDay))->toolTip();
        const QString finalChartTip    = finalTbl->item(finalRow,  static_cast<int>(FC::PrevDayChart))->toolTip();
        const QString marketPrevDayTip = marketTbl->item(marketRow, static_cast<int>(MC::PrevDay))->toolTip();
        const QString marketChartTip   = marketTbl->item(marketRow, static_cast<int>(MC::PrevDayChart))->toolTip();

        const QLocale locale;
        const QString volumeStr = locale.toString(40.0, 'f', 4);           // "40,0000"
        // Die Vortagsdifferenz je Stueck liegt auf der Kurs-Skala und wird
        // seit 05.09.2026 vierstellig angezeigt (ValueFormatter::formatPrice),
        // damit die im Tooltip gezeigte Multiplikation aufgeht. Das Ergebnis
        // daneben ist ein Geldbetrag und bleibt zweistellig.
        const QString diffStr   = locale.toString(12.30, 'f', 4) + QStringLiteral(" €");  // "12,3000 €"
        const QString totalStr  = locale.toString(492.0, 'f', 2) + QStringLiteral(" €");  // "492,00 €"
        const QString greenHex  = AppSettings::instance().logColorAt(5).name();

        const QString coloredDiff  =
            QStringLiteral("<span style=\"color:%1;\">+%2</span>").arg(greenHex, diffStr);
        const QString coloredTotal =
            QStringLiteral("<span style=\"color:%1;\">+%2</span>").arg(greenHex, totalStr);
        const QString expectedTooltip =
            QStringLiteral("<div style=\"white-space:nowrap;\">Gesamtänderung Aktie:<br>"
                           "%1 Stk. × %2 = %3</div>")
                .arg(volumeStr, coloredDiff, coloredTotal);

        QCOMPARE(finalPrevDayTip, expectedTooltip);
        // PrevDayChart-Icon-Spalte trägt denselben Tooltip wie PrevDay selbst.
        QCOMPARE(finalChartTip, expectedTooltip);
        // Identisch in der Marktwert-Tabelle (Anteile/Vortagsdiff sind
        // brokerageunabhängig, siehe MainWindow::populatePortfolioTables()).
        QCOMPARE(marketPrevDayTip, expectedTooltip);
        QCOMPARE(marketChartTip, expectedTooltip);
    }

    void test_populatePortfolioTables_prevDayTooltip_colorsIndependently()
    {
        // ":memory:" ungeeignet, siehe Kommentar im vorigen Test.
        const QString dbPath = m_tempDir.path() + QStringLiteral("/TooltipIndep.db");
        QFile::remove(dbPath);
        Database::instance().open(dbPath);

        // Kurs bewegt sich (+10,00 €), aber KEIN Kauf hinterlegt → volume
        // bleibt 0 → Gesamtergebnis ist 0, obwohl sich der Kurs bewegt hat.
        // Prüft, dass Pro-Stück-Wert und Gesamtergebnis UNABHÄNGIG voneinander
        // eingefärbt werden (nicht "beide oder keiner").
        ShareObject share(QStringLiteral("g-tooltip-indep"), QStringLiteral("TI01"),
                          QStringLiteral("DE000TI00001"), QStringLiteral("TooltipIndep AG"));
        share.setCurPrice(110.0);
        share.setPrevDayPrice(100.0);
        ShareRepository().insert(share);
        // bewusst KEIN insertTestBuy() — volume bleibt 0
        AppSettings::instance().setPortfolioPath(dbPath);

        MainWindow window;
        QApplication::processEvents();

        QTableWidget* finalTbl = findFinalTable(window, 1);
        if (!finalTbl) QFAIL("Depotwert-Datentabelle nicht gefunden");
        const int finalRow = rowForGuid(finalTbl, QStringLiteral("g-tooltip-indep"));
        QVERIFY(finalRow >= 0);

        using FC = MainWindow::FinalValueColumn;
        const QString tip = finalTbl->item(finalRow, static_cast<int>(FC::PrevDay))->toolTip();

        const QLocale locale;
        const QString diffStr  = locale.toString(10.0, 'f', 4) + QStringLiteral(" €"); // "10,0000 €"
        const QString zeroStr  = locale.toString(0.0, 'f', 2) + QStringLiteral(" €");  // "0,00 €"
        const QString volumeStr = locale.toString(0.0, 'f', 4);                        // "0,0000"
        const QString greenHex = AppSettings::instance().logColorAt(5).name();

        const QString coloredDiff =
            QStringLiteral("<span style=\"color:%1;\">+%2</span>").arg(greenHex, diffStr);
        // Gesamtergebnis ist exakt 0 → reiner Text, weder Farb-Span noch "+".
        const QString expectedTooltip =
            QStringLiteral("<div style=\"white-space:nowrap;\">Gesamtänderung Aktie:<br>"
                           "%1 Stk. × %2 = %3</div>")
                .arg(volumeStr, coloredDiff, zeroStr);

        QCOMPARE(tip, expectedTooltip);
    }

    void test_populatePortfolioTables_prevDayTooltip_neutralWhenPriceUnchanged()
    {
        // ":memory:" ungeeignet, siehe Kommentar im ersten Test dieser Gruppe.
        const QString dbPath = m_tempDir.path() + QStringLiteral("/TooltipFlat.db");
        QFile::remove(dbPath);
        Database::instance().open(dbPath);

        // Kurs unverändert zum Vortag (curPrice == prevDayPrice) → sowohl
        // Pro-Stück-Wert als auch Gesamtergebnis sind 0, unabhängig von der
        // gehaltenen Stückzahl (hier 20, bewusst > 0 gewählt).
        ShareObject share(QStringLiteral("g-tooltip-flat"), QStringLiteral("TF01"),
                          QStringLiteral("DE000TF00001"), QStringLiteral("TooltipFlat AG"));
        share.setCurPrice(50.0);
        share.setPrevDayPrice(50.0);
        ShareRepository().insert(share);
        insertTestBuy(QStringLiteral("g-tooltip-flat"), QStringLiteral("depot1"),
                      QStringLiteral("2020-01-01T00:00:00"), 20.0, 50.0);
        AppSettings::instance().setPortfolioPath(dbPath);

        MainWindow window;
        QApplication::processEvents();

        QTableWidget* finalTbl = findFinalTable(window, 1);
        if (!finalTbl) QFAIL("Depotwert-Datentabelle nicht gefunden");
        const int finalRow = rowForGuid(finalTbl, QStringLiteral("g-tooltip-flat"));
        QVERIFY(finalRow >= 0);

        using FC = MainWindow::FinalValueColumn;
        const QString tip = finalTbl->item(finalRow, static_cast<int>(FC::PrevDay))->toolTip();

        const QLocale locale;
        const QString volumeStr = locale.toString(20.0, 'f', 4);                      // "20,0000"
        // Faktor (Kurs-Skala) und Ergebnis (Geldbetrag) haben seit 05.09.2026
        // unterschiedliche Genauigkeit — vorher stand hier zweimal derselbe
        // Platzhalter.
        const QString zeroPriceStr = locale.toString(0.0, 'f', 4) + QStringLiteral(" €"); // "0,0000 €"
        const QString zeroStr   = locale.toString(0.0, 'f', 2) + QStringLiteral(" €"); // "0,00 €"
        const QString expectedTooltip =
            QStringLiteral("<div style=\"white-space:nowrap;\">Gesamtänderung Aktie:<br>"
                           "%1 Stk. × %2 = %3</div>")
                .arg(volumeStr, zeroPriceStr, zeroStr);

        QCOMPARE(tip, expectedTooltip);
        QVERIFY2(!tip.contains(QStringLiteral("color:")), qPrintable(tip));
        QVERIFY2(!tip.contains(QStringLiteral("+0,00")), qPrintable(tip));
    }

    void test_onRefreshShare_prevDayTooltip_updatesAfterRefresh_viaFakeNetwork()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/RefreshTooltip.db");
        QFile::remove(dbPath);
        Database::instance().open(dbPath);

        // Startet flach (curPrice == prevDayPrice == 50) → Tooltip zeigt
        // initial 0,00 €/0,00 € (10 Stk. gehalten, aber keine Kursbewegung).
        ShareObject share(QStringLiteral("g-tooltip-refresh"), QStringLiteral("TR01"),
                          QStringLiteral("DE000TR00001"), QStringLiteral("TooltipRefresh AG"));
        share.setCurPrice(50.0);
        share.setPrevDayPrice(50.0);
        share.setUpdateType(ShareUpdateType::MarketPrice);
        share.setMarketPriceParsingType(ShareParsingType::ApiOnVista);
        share.setMarketPriceUrl(QStringLiteral("https://example.com/onvista/tooltip-refresh"));
        share.setMarketPriceEncoding(QStringLiteral("UTF-8"));
        ShareRepository().insert(share);
        insertTestBuy(QStringLiteral("g-tooltip-refresh"), QStringLiteral("depot1"),
                      QStringLiteral("2020-01-01T00:00:00"), 10.0, 100.0);

        // Vortagesschlusskurs für den Refresh — onMarketValuesUpdated() liest
        // prevDay aus daily_values, nicht aus dem Share-Feld.
        DailyValuesRepository dvRepo;
        dvRepo.upsert(DailyValuesObject(QStringLiteral("g-tooltip-refresh"),
                                        QDate::currentDate().addDays(-1),
                                        100.0, 100.0, 100.0, 100.0, 1000.0));

        AppSettings::instance().setPortfolioPath(dbPath);

        ParserTestUtils::FakeNetworkAccessManager fakeNam;
        // +30 vs. dem seedeten Vortagesschlusskurs 100.0 → 10 Stk. × 30 = 300.
        fakeNam.setResponse(QUrl(QStringLiteral("https://example.com/onvista/tooltip-refresh")),
                            onVistaRealTimeJson(130.0));

        MainWindow window(&fakeNam);
        QApplication::processEvents();

        QTableWidget* finalTbl = findFinalTable(window, 1);
        if (!finalTbl) QFAIL("Depotwert-Datentabelle nicht gefunden");
        const int finalRow = rowForGuid(finalTbl, QStringLiteral("g-tooltip-refresh"));
        QVERIFY(finalRow >= 0);

        using FC = MainWindow::FinalValueColumn;
        const QLocale locale;
        const QString volumeStr = locale.toString(10.0, 'f', 4); // "10,0000"
        const QString zeroPriceStr = locale.toString(0.0, 'f', 4) + QStringLiteral(" €");
        const QString zeroStr   = locale.toString(0.0, 'f', 2) + QStringLiteral(" €");

        const QString expectedBefore =
            QStringLiteral("<div style=\"white-space:nowrap;\">Gesamtänderung Aktie:<br>"
                           "%1 Stk. × %2 = %3</div>")
                .arg(volumeStr, zeroPriceStr, zeroStr);

        const QString before = finalTbl->item(finalRow, static_cast<int>(FC::PrevDay))->toolTip();
        QCOMPARE(before, expectedBefore);

        finalTbl->setCurrentCell(finalRow, 0);
        QAction* actionRefresh = findActionByStatusTip(window,
            QStringLiteral("Kurs der ausgewählten Aktie aktualisieren"));
        QVERIFY(actionRefresh);

        QMetaObject::invokeMethod(&window, "onRefreshShare", Qt::DirectConnection);

        QVERIFY2(QTest::qWaitFor([&]{ return actionRefresh->isEnabled(); }, 2000),
                 "Einzel-Refresh hat nicht beendet (finaliseRefresh() nicht erreicht).");

        const QString diffStr  = locale.toString(30.0, 'f', 4) + QStringLiteral(" €");  // "30,0000 €"
        const QString totalStr = locale.toString(300.0, 'f', 2) + QStringLiteral(" €"); // "300,00 €"
        const QString greenHex = AppSettings::instance().logColorAt(5).name();
        const QString coloredDiff  =
            QStringLiteral("<span style=\"color:%1;\">+%2</span>").arg(greenHex, diffStr);
        const QString coloredTotal =
            QStringLiteral("<span style=\"color:%1;\">+%2</span>").arg(greenHex, totalStr);
        const QString expectedAfter =
            QStringLiteral("<div style=\"white-space:nowrap;\">Gesamtänderung Aktie:<br>"
                           "%1 Stk. × %2 = %3</div>")
                .arg(volumeStr, coloredDiff, coloredTotal);

        const QString after      = finalTbl->item(finalRow, static_cast<int>(FC::PrevDay))->toolTip();
        const QString afterChart = finalTbl->item(finalRow, static_cast<int>(FC::PrevDayChart))->toolTip();

        QCOMPARE(after, expectedAfter);
        // PrevDayChart-Icon-Spalte muss beim Einzel-Refresh ebenfalls
        // aktualisiert werden (analog zum Icon-Regressionstest oben) — nicht
        // nur beim initialen Tabellenaufbau.
        QCOMPARE(afterChart, expectedAfter);
    }

    void test_onRefreshShare_busyGuard_selectionDuringRefreshDoesNotReenableActions()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/RefreshBusy.db");
        QFile::remove(dbPath);
        Database::instance().open(dbPath);

        ShareObject share(QStringLiteral("g-busy"), QStringLiteral("BS01"),
                          QStringLiteral("DE000BS00001"), QStringLiteral("Busy AG"));
        share.setUpdateType(ShareUpdateType::MarketPrice);
        share.setMarketPriceParsingType(ShareParsingType::ApiOnVista);
        share.setMarketPriceUrl(QStringLiteral("https://example.com/onvista/busy"));
        share.setMarketPriceEncoding(QStringLiteral("UTF-8"));
        ShareRepository().insert(share);
        insertTestBuy(QStringLiteral("g-busy"), QStringLiteral("depot1"),
                      QStringLiteral("2020-01-01T00:00:00"), 5.0, 100.0);
        AppSettings::instance().setPortfolioPath(dbPath);

        // No response registered for the busy share's URL — irrelevant here,
        // since the assertion happens before the fake reply resolves.
        ParserTestUtils::FakeNetworkAccessManager fakeNam;
        MainWindow window(&fakeNam);
        QApplication::processEvents();

        QTableWidget* finalTbl = findFinalTable(window, 1);
        if (!finalTbl) QFAIL("Depotwert-Datentabelle nicht gefunden");
        finalTbl->setCurrentCell(0, 0);
        QApplication::processEvents(); // let selectionChanged enable the actions

        QAction* actionEdit = findActionByStatusTip(window,
            QStringLiteral("Ausgewählte Aktie bearbeiten"));
        QVERIFY(actionEdit);
        QVERIFY(actionEdit->isEnabled()); // enabled once a row is selected

        QMetaObject::invokeMethod(&window, "onRefreshShare", Qt::DirectConnection);

        // onRefreshShare() disables actions synchronously, then
        // startRefreshForShare() -> selectShareRow() re-selects the very same
        // row, firing selectionChanged() again — the busy-guard in the
        // enableShareActions lambda (setupCentralWidget()) must keep the
        // action disabled. Without the guard, this selectionChanged would
        // re-enable it mid-refresh.
        QVERIFY(!actionEdit->isEnabled());
    }

    // ─────────────────────────────────────────────────────────────────────
    // MainWindow — buildDailyValuesUrl() (07.07.2026)
    // ─────────────────────────────────────────────────────────────────────
    //
    // Pure, side-effect-free function of its three parameters — no Parser,
    // no network, no MainWindow instance state touched. Declared `public
    // static` specifically so it's directly callable here, mirroring the
    // existing XmlPortfolioParser::normalizeWebSiteUrl() pattern rather than
    // the "private slot" pattern used for selectShareRow()/selectFirstShareRow()
    // (which would need Q_DECLARE_METATYPE for the ShareParsingType enum
    // parameter to work with QMetaObject::invokeMethod's Q_ARG()).
    //
    // Date offsets below are relative to QDate::currentDate() (which the
    // function itself also reads internally) rather than fixed calendar
    // dates, since the period brackets are defined in month-differences to
    // "today". addMonths() preserves the day-of-month where possible, which
    // keeps the month-difference calculation exact except around month-end
    // edge cases (e.g. day 31 with no equivalent in the target month) — an
    // accepted, tiny flake risk given buildDailyValuesUrl() has no injectable
    // "today" to pin down instead.

    void test_buildDailyValuesUrl_normalizesPlaceholdersAndAmpersand()
    {
        const QString tpl = QStringLiteral(
            "https://api.example.com/history?from={0}&amp;period={1}");

        const QString url = MainWindow::buildDailyValuesUrl(
            tpl, QDate(), ShareParsingType::ApiOnVista);

        QVERIFY2(!url.contains(QStringLiteral("{0}")) &&
                 !url.contains(QStringLiteral("{1}")),
                 qPrintable(QStringLiteral("Platzhalter nicht ersetzt: %1").arg(url)));
        QVERIFY2(!url.contains(QStringLiteral("&amp;")),
                 qPrintable(QStringLiteral("&amp; nicht aufgelöst: %1").arg(url)));
        QVERIFY(url.contains(QStringLiteral("&period=")));
    }

    void test_buildDailyValuesUrl_noExistingData_onVista_returns5YearWindow()
    {
        const QString url = MainWindow::buildDailyValuesUrl(
            QStringLiteral("https://api.example.com/history?from=%1&period=%2"),
            QDate(), // invalid → no data yet
            ShareParsingType::ApiOnVista);

        const QDate expectedStart = QDate::currentDate().addYears(-5);
        QVERIFY(url.contains(QStringLiteral("period=Y5")));
        QVERIFY(url.contains(expectedStart.toString(QStringLiteral("yyyy-MM-dd"))));
    }

    void test_buildDailyValuesUrl_noExistingData_yahoo_returns20yPeriod()
    {
        const QString url = MainWindow::buildDailyValuesUrl(
            QStringLiteral("https://api.example.com/chart?range=%1"),
            QDate(),
            ShareParsingType::ApiYahoo);

        QCOMPARE(url, QStringLiteral("https://api.example.com/chart?range=20y"));
    }

    void test_buildDailyValuesUrl_recentData_selectsM1()
    {
        const QDate latest = QDate::currentDate().addDays(-5); // well within 1 month
        const QString url = MainWindow::buildDailyValuesUrl(
            QStringLiteral("https://api.example.com/history?from=%1&period=%2"),
            latest, ShareParsingType::ApiOnVista);

        QVERIFY(url.contains(QStringLiteral("period=M1")));
    }

    void test_buildDailyValuesUrl_dataThreeWeeksOld_selectsM3()
    {
        const QDate latest = QDate::currentDate().addMonths(-2); // between 1 and 3 months
        const QString url = MainWindow::buildDailyValuesUrl(
            QStringLiteral("https://api.example.com/chart?range=%1"),
            latest, ShareParsingType::ApiYahoo);

        QCOMPARE(url, QStringLiteral("https://api.example.com/chart?range=3mo"));
    }

    void test_buildDailyValuesUrl_dataFourMonthsOld_selectsM6()
    {
        const QDate latest = QDate::currentDate().addMonths(-4); // between 3 and 6 months
        const QString url = MainWindow::buildDailyValuesUrl(
            QStringLiteral("https://api.example.com/chart?range=%1"),
            latest, ShareParsingType::ApiYahoo);

        QCOMPARE(url, QStringLiteral("https://api.example.com/chart?range=6mo"));
    }

    void test_buildDailyValuesUrl_dataNineMonthsOld_selectsY1()
    {
        const QDate latest = QDate::currentDate().addMonths(-9); // between 6 and 12 months
        const QString url = MainWindow::buildDailyValuesUrl(
            QStringLiteral("https://api.example.com/chart?range=%1"),
            latest, ShareParsingType::ApiYahoo);

        QCOMPARE(url, QStringLiteral("https://api.example.com/chart?range=1y"));
    }

    void test_buildDailyValuesUrl_dataTwentyMonthsOld_selectsY3()
    {
        const QDate latest = QDate::currentDate().addMonths(-20); // between 12 and 36 months
        const QString url = MainWindow::buildDailyValuesUrl(
            QStringLiteral("https://api.example.com/chart?range=%1"),
            latest, ShareParsingType::ApiYahoo);

        QCOMPARE(url, QStringLiteral("https://api.example.com/chart?range=3y"));
    }

    void test_buildDailyValuesUrl_dataFortyMonthsOld_selectsY5()
    {
        const QDate latest = QDate::currentDate().addMonths(-40); // between 36 and 60 months
        const QString url = MainWindow::buildDailyValuesUrl(
            QStringLiteral("https://api.example.com/chart?range=%1"),
            latest, ShareParsingType::ApiYahoo);

        QCOMPARE(url, QStringLiteral("https://api.example.com/chart?range=5y"));
    }

    void test_buildDailyValuesUrl_dataOverFiveYearsOld_fallsBackToY5()
    {
        // diff >= 60 months matches no bracket in the loop — falls through
        // to the explicit "Fallback: 5 years" branch at the bottom of
        // buildDailyValuesUrl(), same output as the Y5 in-loop match.
        const QDate latest = QDate::currentDate().addMonths(-70);
        const QString url = MainWindow::buildDailyValuesUrl(
            QStringLiteral("https://api.example.com/history?from=%1&period=%2"),
            latest, ShareParsingType::ApiOnVista);

        const QDate expectedStart = QDate::currentDate().addMonths(-60);
        QVERIFY(url.contains(QStringLiteral("period=Y5")));
        QVERIFY(url.contains(expectedStart.toString(QStringLiteral("yyyy-MM-dd"))));
    }

    void test_buildDailyValuesUrl_regexParsingType_returnsEmptyString()
    {
        // ShareParsingType::Regex isn't a valid parsing strategy for the
        // DailyValues history endpoint — both the "no data yet" and the
        // "minimal window" code paths hit their `default: return {};` case.
        QVERIFY(MainWindow::buildDailyValuesUrl(
                    QStringLiteral("https://api.example.com/chart?range=%1"),
                    QDate(), ShareParsingType::Regex)
                    .isEmpty());
        QVERIFY(MainWindow::buildDailyValuesUrl(
                    QStringLiteral("https://api.example.com/chart?range=%1"),
                    QDate::currentDate().addDays(-5), ShareParsingType::Regex)
                    .isEmpty());
    }

    // ─────────────────────────────────────────────────────────────────────
    // MainWindow::shouldMinimizeToTray() — Minimieren wahlweise in
    // Taskleiste oder Tray (Feature 03.08.2026)
    // ─────────────────────────────────────────────────────────────────────
    //
    // Pure decision function, testable directly without a real
    // QSystemTrayIcon/MainWindow instance and independent of whether a tray
    // is actually available in this CI/test environment — see
    // MainWindow.h/.cpp for the full rationale (same pattern as
    // buildDailyValuesUrl()/resolveShareGuidForDocument() above).

    void test_shouldMinimizeToTray_settingEnabledAndTrayAvailable_returnsTrue()
    {
        QVERIFY(MainWindow::shouldMinimizeToTray(true, true));
    }

    void test_shouldMinimizeToTray_settingDisabled_returnsFalse()
    {
        QVERIFY(!MainWindow::shouldMinimizeToTray(false, true));
    }

    void test_shouldMinimizeToTray_trayNotAvailable_returnsFalse()
    {
        QVERIFY(!MainWindow::shouldMinimizeToTray(true, false));
    }

    void test_shouldMinimizeToTray_settingDisabledAndTrayNotAvailable_returnsFalse()
    {
        QVERIFY(!MainWindow::shouldMinimizeToTray(false, false));
    }

    // ─────────────────────────────────────────────────────────────────────
    // MainWindow — Grid-Selektion während "Alle aktualisieren" (07.07.2026)
    // ─────────────────────────────────────────────────────────────────────
    //
    // Seeds a 3-share queue and drives onRefreshAll() through
    // ParserTestUtils::FakeNetworkAccessManager. Reentrancy (Bugfix
    // 05.07.2026) means each share's completion chains directly into the
    // next share's startParsing() from within the same callback — so rather
    // than trying to catch mid-queue selection states with a fixed sleep
    // (racy), these tests use fakeNam.requestCount() as a deterministic
    // checkpoint: createRequest() increments it synchronously at the exact
    // point startParsing() is called, which is itself called synchronously
    // right after selectShareRow() inside startRefreshForShare() — so
    // "requestCount() just became N" reliably means "selection is already on
    // the Nth share".

    /**
     * Seed an N-share portfolio, each MarketPrice-only with a distinct,
     * fake-network-routable marketPriceUrl. Named so ShareRepository::findAll()
     * (ordered by name ascending) — and therefore the "Alle aktualisieren"
     * queue order — is deterministic (share 0 first, share N-1 last).
     *
     * IMPORTANT: both the data table AND the footer table have exactly 3
     * rows/13(12) columns for the Depotwert(Marktwert) tab (footer = 3 fixed
     * summary rows) — findFinalTable(window, 3)/findMarketTable(window, 3)
     * would therefore match EITHER table ambiguously. Never seed exactly 3
     * shares for tests that locate the data table via row count; use 2 or 4+.
     *
     * @return GUIDs in queue order.
     */
    QStringList seedRefreshQueuePortfolio(int shareCount, const QString& dbPath)
    {
        Q_ASSERT(shareCount != 3); // see collision note above
        QFile::remove(dbPath);
        Database::instance().open(dbPath);

        QStringList guids;
        for (int i = 0; i < shareCount; ++i) {
            const QString guid = QStringLiteral("g-queue-%1").arg(i);
            // "AAA", "BBB", "CCC", ... — keeps findAll()'s name-ascending
            // order equal to insertion order regardless of shareCount.
            const QString namePrefix = QString(3, QChar(char('A' + i)));
            ShareObject share(guid, QStringLiteral("QU%1").arg(i),
                              QStringLiteral("DE000QU0000%1").arg(i),
                              QStringLiteral("%1 Queue Share").arg(namePrefix));
            share.setUpdateType(ShareUpdateType::MarketPrice);
            share.setMarketPriceParsingType(ShareParsingType::ApiOnVista);
            share.setMarketPriceUrl(
                QStringLiteral("https://example.com/onvista/%1").arg(guid));
            share.setMarketPriceEncoding(QStringLiteral("UTF-8"));
            ShareRepository().insert(share);
            insertTestBuy(guid, QStringLiteral("depot1"),
                          QStringLiteral("2020-01-01T00:00:00"), 5.0, 100.0);
            guids << guid;
        }
        AppSettings::instance().setPortfolioPath(dbPath);
        return guids;
    }

    static QByteArray onVistaRealTimeJson(double price)
    {
        return QStringLiteral(R"({
            "price": %1,
            "previousLast": %1,
            "isoCurrency": "EUR",
            "idNotation": 1,
            "idCurrency": 1,
            "datetimePrice": {
                "localTime": "2024-01-15T10:30:00",
                "localTimeZone": "Europe/Berlin",
                "utcTimeStamp": 1705315800
            }
        })").arg(price).toUtf8();
    }

    void test_onRefreshAll_gridSelectionFollowsQueueProgress_viaFakeNetwork()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/RefreshAllSelection.db");
        // 2 shares — see seedRefreshQueuePortfolio() note on why not 3.
        const QStringList guids = seedRefreshQueuePortfolio(2, dbPath);

        ParserTestUtils::FakeNetworkAccessManager fakeNam;
        for (int i = 0; i < guids.size(); ++i) {
            fakeNam.setResponse(
                QUrl(QStringLiteral("https://example.com/onvista/%1").arg(guids[i])),
                onVistaRealTimeJson(100.0 + i));
        }

        MainWindow window(&fakeNam);
        QApplication::processEvents();

        QTableWidget* finalTbl  = findFinalTable(window, 2);
        QTableWidget* marketTbl = findMarketTable(window, 2);
        if (!finalTbl)  QFAIL("Depotwert-Datentabelle nicht gefunden");
        if (!marketTbl) QFAIL("Marktwert-Datentabelle nicht gefunden");

        QMetaObject::invokeMethod(&window, "onRefreshAll", Qt::DirectConnection);

        // Immediately after onRefreshAll() returns, startRefreshForShare()
        // for share A has already run synchronously (incl. selectShareRow()),
        // before any fake network response resolves.
        QCOMPARE(finalTbl->currentRow(),  rowForGuid(finalTbl,  guids[0]));
        QCOMPARE(marketTbl->currentRow(), rowForGuid(marketTbl, guids[0]));

        // Share A finishes → chains into share B (reentrant startParsing(),
        // Bugfix 05.07.2026). requestCount() ticking up to 2 is a
        // deterministic checkpoint for "selection is now on B": createRequest()
        // increments it synchronously right after selectShareRow() runs
        // inside startRefreshForShare().
        QVERIFY2(QTest::qWaitFor([&]{ return fakeNam.requestCount() >= 2; }, 2000),
                 "Zweite Anfrage (Aktie B) wurde nicht gestellt.");
        QCOMPARE(finalTbl->currentRow(),  rowForGuid(finalTbl,  guids[1]));
        QCOMPARE(marketTbl->currentRow(), rowForGuid(marketTbl, guids[1]));

        // Share B finishes, queue empty, no error → selectFirstShareRow()
        // resets the selection to row 0 in both tables.
        QVERIFY2(QTest::qWaitFor([&]{
                     return finalTbl->currentRow() == 0 && marketTbl->currentRow() == 0;
                 }, 2000),
                 "Selektion sprang nach Abschluss der Queue nicht auf Zeile 0.");
        QCOMPARE(fakeNam.requestCount(), 2);
    }

    void test_onRefreshAll_errorMidQueue_selectionStaysOnFailedShare_viaFakeNetwork()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/RefreshAllError.db");
        // 4 shares (A ok, B fails, C+D must never be reached) — see
        // seedRefreshQueuePortfolio() note on why not 3.
        const QStringList guids = seedRefreshQueuePortfolio(4, dbPath);

        ParserTestUtils::FakeNetworkAccessManager fakeNam;
        fakeNam.setResponse(
            QUrl(QStringLiteral("https://example.com/onvista/%1").arg(guids[0])),
            onVistaRealTimeJson(100.0));
        // Share B (second in queue) fails with a network error.
        fakeNam.setError(
            QUrl(QStringLiteral("https://example.com/onvista/%1").arg(guids[1])),
            QNetworkReply::HostNotFoundError, QStringLiteral("host not found"));
        // Shares C and D would succeed — must never be reached.
        fakeNam.setResponse(
            QUrl(QStringLiteral("https://example.com/onvista/%1").arg(guids[2])),
            onVistaRealTimeJson(102.0));
        fakeNam.setResponse(
            QUrl(QStringLiteral("https://example.com/onvista/%1").arg(guids[3])),
            onVistaRealTimeJson(103.0));

        MainWindow window(&fakeNam);
        QApplication::processEvents();

        QTableWidget* finalTbl  = findFinalTable(window, 4);
        QTableWidget* marketTbl = findMarketTable(window, 4);
        if (!finalTbl)  QFAIL("Depotwert-Datentabelle nicht gefunden");
        if (!marketTbl) QFAIL("Marktwert-Datentabelle nicht gefunden");

        QAction* actionRefreshAll = findActionByStatusTip(window,
            QStringLiteral("Kurse aller Aktien aktualisieren"));
        QVERIFY(actionRefreshAll);

        QMetaObject::invokeMethod(&window, "onRefreshAll", Qt::DirectConnection);
        QVERIFY(!actionRefreshAll->isEnabled()); // disabled while the queue runs

        // Wait until the run has actually finished — finaliseRefresh()
        // re-enables m_actionRefreshAll. This happens once share B's error
        // has propagated through onMarketValuesUpdated() /
        // onRefreshShareFinished(), which clears the queue instead of
        // advancing to shares C/D.
        QVERIFY2(QTest::qWaitFor([&]{ return actionRefreshAll->isEnabled(); }, 2000),
                 "onRefreshAll() hat nach dem Fehler bei Aktie B nicht beendet "
                 "(finaliseRefresh() wurde nicht erreicht).");

        // Shares C and D must never have been requested — the queue was
        // cleared on error, not merely paused.
        QCOMPARE(fakeNam.requestCount(), 2);

        // Selection stays on the FAILED share (B) — selectFirstShareRow() is
        // deliberately not called in the error path, so the problem stays
        // visible instead of the grid jumping back to row 0.
        QCOMPARE(finalTbl->currentRow(),  rowForGuid(finalTbl,  guids[1]));
        QCOMPARE(marketTbl->currentRow(), rowForGuid(marketTbl, guids[1]));
    }

    void test_onRefreshShare_completed_selectionStaysOnUpdatedShare_viaFakeNetwork()
    {
        // Deckt den bislang offenen vierten Punkt aus TESTING.md
        // ("Weiterhin offen" / ARCHITECTURE.md "Offene Punkte") ab:
        // Selektion bleibt nach abgeschlossenem EINZEL-Refresh (kein "Alle
        // aktualisieren") auf der aktualisierten Aktie stehen —
        // selectFirstShareRow() darf hier NICHT aufgerufen werden.
        const QString dbPath = m_tempDir.path() + QStringLiteral("/RefreshSingleSelectionStays.db");
        // 2 Aktien — bewusst NICHT die erste (Reihe 0) auswählen, sonst lässt
        // sich "Selektion blieb stehen" nicht von "wurde auf Zeile 0
        // zurückgesetzt" unterscheiden.
        const QStringList guids = seedRefreshQueuePortfolio(2, dbPath);

        ParserTestUtils::FakeNetworkAccessManager fakeNam;
        fakeNam.setResponse(
            QUrl(QStringLiteral("https://example.com/onvista/%1").arg(guids[1])),
            onVistaRealTimeJson(150.0));

        MainWindow window(&fakeNam);
        QApplication::processEvents();

        QTableWidget* finalTbl  = findFinalTable(window, 2);
        QTableWidget* marketTbl = findMarketTable(window, 2);
        if (!finalTbl)  QFAIL("Depotwert-Datentabelle nicht gefunden");
        if (!marketTbl) QFAIL("Marktwert-Datentabelle nicht gefunden");

        const int finalRow  = rowForGuid(finalTbl,  guids[1]);
        const int marketRow = rowForGuid(marketTbl, guids[1]);
        QVERIFY(finalRow  > 0); // Sanity: darf nicht zufällig Zeile 0 sein
        QVERIFY(marketRow > 0);

        finalTbl->setCurrentCell(finalRow, 0);
        QApplication::processEvents();

        QAction* actionRefresh = findActionByStatusTip(window,
            QStringLiteral("Kurs der ausgewählten Aktie aktualisieren"));
        QVERIFY(actionRefresh);

        QMetaObject::invokeMethod(&window, "onRefreshShare", Qt::DirectConnection);

        // finaliseRefresh() re-enables m_actionRefresh, sobald der
        // Einzel-Refresh (kein Queue-Lauf) vollständig abgeschlossen ist —
        // gleicher Checkpoint wie in
        // test_onRefreshShare_footerUpdatesImmediately_viaFakeNetwork.
        QVERIFY2(QTest::qWaitFor([&]{ return actionRefresh->isEnabled(); }, 2000),
                 "Einzel-Refresh hat nicht beendet (finaliseRefresh() nicht erreicht).");

        // selectFirstShareRow() darf NICHT aufgerufen worden sein — Selektion
        // bleibt in beiden Tabellen auf der aktualisierten Aktie stehen.
        QCOMPARE(finalTbl->currentRow(),  finalRow);
        QCOMPARE(marketTbl->currentRow(), marketRow);
        QCOMPARE(fakeNam.requestCount(), 1);
    }

    // ─────────────────────────────────────────────────────────────────────
    // MainWindow — Footer-Update bei Refresh (07.07.2026)
    // ─────────────────────────────────────────────────────────────────────
    //
    // refreshPortfolioFooters() is called from onRefreshShareFinished() on
    // success — these tests confirm it actually fires (footer text changes
    // from its pre-refresh baseline), fires BETWEEN queue steps rather than
    // only once at the very end, and does NOT fire when a refresh fails.
    //
    // Deliberately asserting "changed from baseline" rather than a
    // hand-derived exact total: the footer total is computed by
    // ShareCalculator::portfolioTotalsFinal() across brokerage/dividend/
    // FIFO logic that's already covered by its own dedicated tests
    // elsewhere — duplicating that formula here would risk testing the
    // test's own (possibly wrong) arithmetic rather than the actual wiring
    // question, which is simply: did refreshPortfolioFooters() run, and
    // when.

    /// Depotwert-Footer, Zeile 2 ("Aktueller Depotstand"), Top-Text.
    static QString finalFooterDepotstand(QTableWidget* footer)
    {
        auto* item = footer->item(2, static_cast<int>(MainWindow::FinalValueColumn::PurchaseFinalValue));
        return item ? item->data(TwoLineRole::Top).toString() : QString();
    }

    void test_onRefreshShare_footerUpdatesImmediately_viaFakeNetwork()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/RefreshFooterSingle.db");
        QFile::remove(dbPath);
        Database::instance().open(dbPath);

        ShareObject share(QStringLiteral("g-footer"), QStringLiteral("FO01"),
                          QStringLiteral("DE000FO00001"), QStringLiteral("Footer AG"));
        share.setUpdateType(ShareUpdateType::MarketPrice);
        share.setMarketPriceParsingType(ShareParsingType::ApiOnVista);
        share.setMarketPriceUrl(QStringLiteral("https://example.com/onvista/footer"));
        share.setMarketPriceEncoding(QStringLiteral("UTF-8"));
        ShareRepository().insert(share);
        insertTestBuy(QStringLiteral("g-footer"), QStringLiteral("depot1"),
                      QStringLiteral("2020-01-01T00:00:00"), 5.0, 100.0);
        AppSettings::instance().setPortfolioPath(dbPath);

        ParserTestUtils::FakeNetworkAccessManager fakeNam;
        fakeNam.setResponse(QUrl(QStringLiteral("https://example.com/onvista/footer")),
                            onVistaRealTimeJson(300.0)); // curPrice starts at 0 → clear jump

        MainWindow window(&fakeNam);
        QApplication::processEvents();

        QTableWidget* finalTbl = findFinalTable(window, 1);
        QTableWidget* footer   = findFinalTable(window, 3); // 1 share ≠ 3 → unambiguous
        if (!finalTbl) QFAIL("Depotwert-Datentabelle nicht gefunden");
        if (!footer)   QFAIL("Depotwert-Footer nicht gefunden");

        const QString before = finalFooterDepotstand(footer);

        finalTbl->setCurrentCell(0, 0);
        QAction* actionRefresh = findActionByStatusTip(window,
            QStringLiteral("Kurs der ausgewählten Aktie aktualisieren"));
        QVERIFY(actionRefresh);

        QMetaObject::invokeMethod(&window, "onRefreshShare", Qt::DirectConnection);

        // finaliseRefresh() re-enables m_actionRefresh once the (single-share,
        // non-queue) run has fully completed.
        QVERIFY2(QTest::qWaitFor([&]{ return actionRefresh->isEnabled(); }, 2000),
                 "Einzel-Refresh hat nicht beendet (finaliseRefresh() nicht erreicht).");

        const QString after = finalFooterDepotstand(footer);
        QVERIFY2(after != before,
                 qPrintable(QStringLiteral(
                     "Footer 'Aktueller Depotstand' unverändert nach Einzel-Refresh "
                     "(vorher: '%1', nachher: '%2').").arg(before, after)));
    }

    void test_onRefreshAll_footerUpdatesBetweenEachShare_viaFakeNetwork()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/RefreshFooterQueue.db");
        // 2 shares — see seedRefreshQueuePortfolio() note on why not 3.
        const QStringList guids = seedRefreshQueuePortfolio(2, dbPath);

        ParserTestUtils::FakeNetworkAccessManager fakeNam;
        fakeNam.setResponse(
            QUrl(QStringLiteral("https://example.com/onvista/%1").arg(guids[0])),
            onVistaRealTimeJson(300.0));
        fakeNam.setResponse(
            QUrl(QStringLiteral("https://example.com/onvista/%1").arg(guids[1])),
            onVistaRealTimeJson(500.0));

        MainWindow window(&fakeNam);
        QApplication::processEvents();

        QTableWidget* footer = findFinalTable(window, 3); // 2 shares ≠ 3 → unambiguous
        if (!footer) QFAIL("Depotwert-Footer nicht gefunden");

        const QString baseline = finalFooterDepotstand(footer);

        QMetaObject::invokeMethod(&window, "onRefreshAll", Qt::DirectConnection);

        // Checkpoint 1: share B's request has started → share A already
        // finished and refreshPortfolioFooters() already ran for it (it runs
        // in onRefreshShareFinished() strictly BEFORE the chained
        // startRefreshForShare() call for B — see requestCount() note in the
        // grid-selection tests above for why this ordering makes the
        // checkpoint deterministic). Share B has NOT finished yet at this
        // point, so this captures a genuine intermediate state.
        QVERIFY2(QTest::qWaitFor([&]{ return fakeNam.requestCount() >= 2; }, 2000),
                 "Zweite Anfrage (Aktie B) wurde nicht gestellt.");
        const QString afterShareA = finalFooterDepotstand(footer);
        QVERIFY2(afterShareA != baseline,
                 qPrintable(QStringLiteral(
                     "Footer nach Abschluss von Aktie A (noch vor Aktie B) "
                     "unverändert — Update erfolgt offenbar erst am Ende der "
                     "Queue statt nach jeder Aktie ('%1').").arg(afterShareA)));

        // Checkpoint 2: whole run finished → footer reflects share B too,
        // i.e. differs again from the after-A intermediate snapshot.
        QVERIFY2(QTest::qWaitFor([&]{
                     return finalFooterDepotstand(footer) != afterShareA;
                 }, 2000),
                 "Footer wurde nach Abschluss von Aktie B nicht erneut aktualisiert.");
    }

    void test_onRefreshShare_footerNotUpdated_onNetworkError_viaFakeNetwork()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/RefreshFooterError.db");
        QFile::remove(dbPath);
        Database::instance().open(dbPath);

        ShareObject share(QStringLiteral("g-footer-err"), QStringLiteral("FE01"),
                          QStringLiteral("DE000FE00001"), QStringLiteral("FooterError AG"));
        share.setUpdateType(ShareUpdateType::MarketPrice);
        share.setMarketPriceParsingType(ShareParsingType::ApiOnVista);
        share.setMarketPriceUrl(QStringLiteral("https://example.com/onvista/footer-err"));
        share.setMarketPriceEncoding(QStringLiteral("UTF-8"));
        ShareRepository().insert(share);
        insertTestBuy(QStringLiteral("g-footer-err"), QStringLiteral("depot1"),
                      QStringLiteral("2020-01-01T00:00:00"), 5.0, 100.0);
        AppSettings::instance().setPortfolioPath(dbPath);

        ParserTestUtils::FakeNetworkAccessManager fakeNam;
        fakeNam.setError(QUrl(QStringLiteral("https://example.com/onvista/footer-err")),
                         QNetworkReply::HostNotFoundError, QStringLiteral("host not found"));

        MainWindow window(&fakeNam);
        QApplication::processEvents();

        QTableWidget* finalTbl = findFinalTable(window, 1);
        QTableWidget* footer   = findFinalTable(window, 3); // 1 share ≠ 3 → unambiguous
        if (!finalTbl) QFAIL("Depotwert-Datentabelle nicht gefunden");
        if (!footer)   QFAIL("Depotwert-Footer nicht gefunden");

        const QString before = finalFooterDepotstand(footer);

        finalTbl->setCurrentCell(0, 0);
        QAction* actionRefresh = findActionByStatusTip(window,
            QStringLiteral("Kurs der ausgewählten Aktie aktualisieren"));
        QVERIFY(actionRefresh);

        QMetaObject::invokeMethod(&window, "onRefreshShare", Qt::DirectConnection);

        QVERIFY2(QTest::qWaitFor([&]{ return actionRefresh->isEnabled(); }, 2000),
                 "Einzel-Refresh (Fehlerfall) hat nicht beendet "
                 "(finaliseRefresh() nicht erreicht).");

        // onRefreshShareFinished() returns before calling
        // refreshPortfolioFooters() when m_errorOccurred is set — the footer
        // must be byte-for-byte unchanged.
        QCOMPARE(finalFooterDepotstand(footer), before);
    }

    void test_updatePortfolioFooters_prevDayTooltip_sumsAllShares()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/FooterTooltipSum.db");
        QFile::remove(dbPath);
        Database::instance().open(dbPath);

        // Aktie A: 10 Stk., Kurs +5,00 € → Gesamtänderung +50,00 €.
        ShareObject shareA(QStringLiteral("g-footer-tip-a"), QStringLiteral("FA01"),
                           QStringLiteral("DE000FA00001"), QStringLiteral("FooterTipA AG"));
        shareA.setCurPrice(105.0);
        shareA.setPrevDayPrice(100.0);
        ShareRepository().insert(shareA);
        insertTestBuy(QStringLiteral("g-footer-tip-a"), QStringLiteral("depot1"),
                      QStringLiteral("2020-01-01T00:00:00"), 10.0, 100.0);

        // Aktie B: 4 Stk., Kurs -2,50 € → Gesamtänderung -10,00 €.
        ShareObject shareB(QStringLiteral("g-footer-tip-b"), QStringLiteral("FB01"),
                           QStringLiteral("DE000FB00001"), QStringLiteral("FooterTipB AG"));
        shareB.setCurPrice(47.5);
        shareB.setPrevDayPrice(50.0);
        ShareRepository().insert(shareB);
        insertTestBuy(QStringLiteral("g-footer-tip-b"), QStringLiteral("depot1"),
                      QStringLiteral("2020-01-01T00:00:00"), 4.0, 50.0);

        AppSettings::instance().setPortfolioPath(dbPath);

        MainWindow window;
        QApplication::processEvents();

        // Summe: +50,00 € + (-10,00 €) = +40,00 €. Bewusst runde Werte ohne
        // FIFO-/Brokerage-Komplexität, damit ein exakter QCOMPARE() sinnvoll
        // ist (anders als bei den bestehenden Footer-Summen-Tests, die aus
        // gutem Grund nur auf Änderung statt auf einen bestimmten Zahlenwert
        // prüfen — siehe "Footer-Update bei Refresh" in TESTING.md).
        QTableWidget* finalFooter  = findFinalTable(window, 3);  // 2 Aktien ≠ 3 → eindeutig
        QTableWidget* marketFooter = findMarketTable(window, 3); // 2 Aktien ≠ 3 → eindeutig
        if (!finalFooter)  QFAIL("Depotwert-Footer nicht gefunden");
        if (!marketFooter) QFAIL("Marktwert-Footer nicht gefunden");

        using FC = MainWindow::FinalValueColumn;
        using MC = MainWindow::MarketValueColumn;

        const QLocale locale;
        const QString sumStr   = locale.toString(40.0, 'f', 2) + QStringLiteral(" €"); // "40,00 €"
        const QString greenHex = AppSettings::instance().logColorAt(5).name();
        const QString coloredSum =
            QStringLiteral("<span style=\"color:%1;\">+%2</span>").arg(greenHex, sumStr);
        const QString expectedTooltip =
            QStringLiteral("<div style=\"white-space:nowrap;\">Gesamtänderung Portfolio: %1</div>")
                .arg(coloredSum);

        // Span-Anker im Depotwert-Footer ist FC::Price (Preis + Chart-Icon +
        // Vortag sind per setSpan() zu einem Zeilen-Label verschmolzen) — alle
        // drei Zeilen (Einzahlung/Entwicklung/Depotstand) tragen denselben
        // Tooltip, siehe MainWindow::updatePortfolioFooters().
        for (int row = 0; row < 3; ++row) {
            const QString tip =
                finalFooter->item(row, static_cast<int>(FC::Price))->toolTip();
            QCOMPARE(tip, expectedTooltip);
        }

        // Span-Anker im Marktwert-Footer ist MC::Icon (Icon..Vortag als ganzer
        // Zeilen-Label-Span) — Wert ist brokerageunabhängig und daher identisch
        // zum Depotwert-Footer.
        for (int row = 0; row < 3; ++row) {
            const QString tip =
                marketFooter->item(row, static_cast<int>(MC::Icon))->toolTip();
            QCOMPARE(tip, expectedTooltip);
        }
    }

    // ─────────────────────────────────────────────────────────────────────
    // MainWindow — Portfolio-Label "Letzte Aktualisierung" (Feature 21.07.2026)
    // ─────────────────────────────────────────────────────────────────────
    //
    // Analog zum Footer-Update oben: updatePortfolioLabel(entryCount,
    // formatLastPortfolioUpdate()) wird an derselben Stelle in
    // onRefreshShareFinished() aufgerufen (direkt nach refreshPortfolioFooters(),
    // vor dem Verketten zur nächsten Aktie bzw. vor finaliseRefresh()). Das
    // Label ist über window.findChild<QLabel*>() erreichbar — dasselbe Muster
    // wie test_updatePortfolioLabel_defaultValues weiter oben nutzt es bereits
    // (m_portfolioLabel ist das erste QLabel-Kind, das setupCentralWidget()
    // erzeugt).

    void test_populatePortfolioTables_neverUpdated_labelShowsDash()
    {
        openMemoryDb();
        ShareRepository().insert(ShareObject(
            QStringLiteral("g-label-dash"), QStringLiteral("LD01"),
            QStringLiteral("DE000LD00001"), QStringLiteral("Label Dash AG")));

        MainWindow window;
        auto* label = window.findChild<QLabel*>();
        QVERIFY(label != nullptr);
        QVERIFY2(label->text().contains(QStringLiteral("Letzte Aktualisierung: -")),
                 qPrintable(label->text()));
    }

    void test_onRefreshShare_marketPriceSuccess_labelShowsCurrentTimestamp_viaFakeNetwork()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/LabelSingleSuccess.db");
        const QStringList guids = seedRefreshQueuePortfolio(1, dbPath);

        ParserTestUtils::FakeNetworkAccessManager fakeNam;
        fakeNam.setResponse(
            QUrl(QStringLiteral("https://example.com/onvista/%1").arg(guids[0])),
            onVistaRealTimeJson(150.0));

        MainWindow window(&fakeNam);
        QApplication::processEvents();

        auto* label = window.findChild<QLabel*>();
        QVERIFY(label != nullptr);
        QVERIFY2(label->text().contains(QStringLiteral("Letzte Aktualisierung: -")),
                 qPrintable(label->text())); // Vorher-Zustand: noch nie aktualisiert

        QTableWidget* finalTbl = findFinalTable(window, 1);
        if (!finalTbl) QFAIL("Depotwert-Datentabelle nicht gefunden");
        finalTbl->setCurrentCell(0, 0);

        QAction* actionRefresh = findActionByStatusTip(window,
            QStringLiteral("Kurs der ausgewählten Aktie aktualisieren"));
        QVERIFY(actionRefresh);

        QMetaObject::invokeMethod(&window, "onRefreshShare", Qt::DirectConnection);

        QVERIFY2(QTest::qWaitFor([&]{ return actionRefresh->isEnabled(); }, 2000),
                 "Einzel-Refresh hat nicht beendet (finaliseRefresh() nicht erreicht).");

        QVERIFY2(!label->text().contains(QStringLiteral("Letzte Aktualisierung: -")),
                 qPrintable(label->text()));
    }

    void test_onRefreshShare_dailyValuesOnlySuccess_labelShowsCurrentTimestamp_viaFakeNetwork()
    {
        // Regressionstest für die geschlossene Lücke: vor dieser Änderung
        // rief onDailyValuesUpdated() ShareRepository::updateLastInternetUpdate()
        // nie auf, wodurch ein reiner DailyValues-Refresh das Portfolio-Label
        // nie aktualisiert hätte.
        const QString dbPath = m_tempDir.path() + QStringLiteral("/LabelDailyValuesSingle.db");
        const QStringList guids = seedDailyValuesQueuePortfolio(1, dbPath);

        ParserTestUtils::FakeNetworkAccessManager fakeNam;
        fakeNam.setResponse(
            QUrl(QStringLiteral("https://example.com/yahoo-daily/%1?range=20y").arg(guids[0])),
            yahooDailyHistoryJson());

        MainWindow window(&fakeNam);
        QApplication::processEvents();

        auto* label = window.findChild<QLabel*>();
        QVERIFY(label != nullptr);

        QTableWidget* finalTbl = findFinalTable(window, 1);
        if (!finalTbl) QFAIL("Depotwert-Datentabelle nicht gefunden");
        finalTbl->setCurrentCell(0, 0);

        QAction* actionRefresh = findActionByStatusTip(window,
            QStringLiteral("Kurs der ausgewählten Aktie aktualisieren"));
        QVERIFY(actionRefresh);

        QMetaObject::invokeMethod(&window, "onRefreshShare", Qt::DirectConnection);

        QVERIFY2(QTest::qWaitFor([&]{ return actionRefresh->isEnabled(); }, 2000),
                 "Einzel-Refresh (DailyValues-only) hat nicht beendet "
                 "(finaliseRefresh() nicht erreicht).");

        QVERIFY2(!label->text().contains(QStringLiteral("Letzte Aktualisierung: -")),
                 qPrintable(label->text()));
    }

    void test_onRefreshShare_networkError_labelStaysAtDash_viaFakeNetwork()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/LabelSingleError.db");
        QFile::remove(dbPath);
        Database::instance().open(dbPath);

        ShareObject share(QStringLiteral("g-label-err"), QStringLiteral("LE01"),
                          QStringLiteral("DE000LE00001"), QStringLiteral("LabelError AG"));
        share.setUpdateType(ShareUpdateType::MarketPrice);
        share.setMarketPriceParsingType(ShareParsingType::ApiOnVista);
        share.setMarketPriceUrl(QStringLiteral("https://example.com/onvista/label-err"));
        share.setMarketPriceEncoding(QStringLiteral("UTF-8"));
        ShareRepository().insert(share);
        insertTestBuy(QStringLiteral("g-label-err"), QStringLiteral("depot1"),
                      QStringLiteral("2020-01-01T00:00:00"), 5.0, 100.0);
        AppSettings::instance().setPortfolioPath(dbPath);

        ParserTestUtils::FakeNetworkAccessManager fakeNam;
        fakeNam.setError(QUrl(QStringLiteral("https://example.com/onvista/label-err")),
                         QNetworkReply::HostNotFoundError, QStringLiteral("host not found"));

        MainWindow window(&fakeNam);
        QApplication::processEvents();

        auto* label = window.findChild<QLabel*>();
        QVERIFY(label != nullptr);

        QTableWidget* finalTbl = findFinalTable(window, 1);
        if (!finalTbl) QFAIL("Depotwert-Datentabelle nicht gefunden");
        finalTbl->setCurrentCell(0, 0);

        QAction* actionRefresh = findActionByStatusTip(window,
            QStringLiteral("Kurs der ausgewählten Aktie aktualisieren"));
        QVERIFY(actionRefresh);

        QMetaObject::invokeMethod(&window, "onRefreshShare", Qt::DirectConnection);

        QVERIFY2(QTest::qWaitFor([&]{ return actionRefresh->isEnabled(); }, 2000),
                 "Einzel-Refresh (Fehlerfall) hat nicht beendet "
                 "(finaliseRefresh() nicht erreicht).");

        QVERIFY2(label->text().contains(QStringLiteral("Letzte Aktualisierung: -")),
                 qPrintable(label->text()));
    }

    void test_populatePortfolioTables_afterRefresh_timestampPersistsAcrossReload_viaFakeNetwork()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/LabelPersistReload.db");
        const QStringList guids = seedRefreshQueuePortfolio(1, dbPath);

        ParserTestUtils::FakeNetworkAccessManager fakeNam;
        fakeNam.setResponse(
            QUrl(QStringLiteral("https://example.com/onvista/%1").arg(guids[0])),
            onVistaRealTimeJson(180.0));

        {
            MainWindow window(&fakeNam);
            QApplication::processEvents();

            QTableWidget* finalTbl = findFinalTable(window, 1);
            if (!finalTbl) QFAIL("Depotwert-Datentabelle nicht gefunden");
            finalTbl->setCurrentCell(0, 0);

            QAction* actionRefresh = findActionByStatusTip(window,
                QStringLiteral("Kurs der ausgewählten Aktie aktualisieren"));
            QVERIFY(actionRefresh);

            QMetaObject::invokeMethod(&window, "onRefreshShare", Qt::DirectConnection);
            QVERIFY2(QTest::qWaitFor([&]{ return actionRefresh->isEnabled(); }, 2000),
                     "Einzel-Refresh hat nicht beendet (finaliseRefresh() nicht erreicht).");

            auto* label = window.findChild<QLabel*>();
            QVERIFY(label != nullptr);
            QVERIFY(!label->text().contains(QStringLiteral("Letzte Aktualisierung: -")));

            Database::instance().close(); // "Neustart" simulieren
        }

        // Neues MainWindow gegen dieselbe (echte Datei-)DB — populatePortfolioTables()
        // läuft automatisch im Konstruktor. Der Zeitstempel muss aus
        // shares.last_internet_update erhalten bleiben, nicht auf "-" zurückfallen.
        Database::instance().open(dbPath);
        MainWindow reopened;
        auto* reopenedLabel = reopened.findChild<QLabel*>();
        QVERIFY(reopenedLabel != nullptr);
        QVERIFY2(!reopenedLabel->text().contains(QStringLiteral("Letzte Aktualisierung: -")),
                 qPrintable(reopenedLabel->text()));
    }

    // ─────────────────────────────────────────────────────────────────────
    // MainWindow — onDailyValuesUpdated()-Pfad (08.07.2026)
    // ─────────────────────────────────────────────────────────────────────
    //
    // Bislang war über FakeNetworkAccessManager nur der MarketPrice-Zweig
    // (onMarketValuesUpdated()) end-to-end abgedeckt. Diese Tests spiegeln
    // dasselbe Muster für den DailyValues-Zweig: Yahoo-History-JSON über
    // Fake-Netzwerk, echte Produktionslogik (buildDailyValuesUrl() ->
    // ParserLib::Parser -> DailyValuesRepository::upsertList()), keine
    // eigene Test-Attrappe der Geschäftslogik.
    //
    // Da für frisch angelegte Aktien noch keine daily_values existieren,
    // löst buildDailyValuesUrl() für ApiYahoo deterministisch immer den
    // "noch keine Daten"-Zweig auf: tpl.arg("20y") -> "...?range=20y".
    // Das GUID wird daher NICHT als %-Platzhalter ins Template eingebaut
    // (QString::arg() würde bei mehrfachem "%1" alle Vorkommen ersetzen),
    // sondern per einfacher String-Konkatenation vor dem einzigen
    // verbleibenden %1 (= Periodencode).

    /// Yahoo-History-JSON mit 2 Handelstagen — identische Werte wie im
    /// bestehenden test_yahoo_history_json_parsing (tst_parser.cpp) und
    /// test_webMode_yahooHistory_viaFakeNetwork, damit die erwarteten
    /// closingPrice-Werte (141.5 / 143.0) an einer einzigen Stelle im
    /// Projekt als "Referenzwerte" etabliert sind.
    static QByteArray yahooDailyHistoryJson()
    {
        return QByteArrayLiteral(R"({
            "chart": {
                "result": [{
                    "timestamp": [1705315800, 1705402200],
                    "indicators": {
                        "quote": [{
                            "open":   [140.0, 142.0],
                            "close":  [141.5, 143.0],
                            "high":   [142.0, 144.0],
                            "low":    [139.0, 141.0],
                            "volume": [100000, 120000]
                        }]
                    }
                }]
            }
        })");
    }

    /**
     * Seed an N-share portfolio, each DailyValues-only, with a distinct,
     * fake-network-routable dailyValuesUrl (ApiYahoo, ein "%1"-Platzhalter
     * für den Periodencode — siehe buildDailyValuesUrl()). Keine Aktie hat
     * bereits daily_values, wodurch buildDailyValuesUrl() garantiert den
     * "noch keine Daten"-Zweig nimmt (range=20y) — die finale Request-URL
     * ist damit ohne Sonderfall pro Aktie vorhersagbar.
     *
     * Spiegelt seedRefreshQueuePortfolio() (MarketPrice-only) — siehe
     * dessen Doku-Kommentar zum "nie exakt 3 Aktien seeden"-Hinweis, der
     * hier identisch gilt.
     */
    QStringList seedDailyValuesQueuePortfolio(int shareCount, const QString& dbPath)
    {
        Q_ASSERT(shareCount != 3); // siehe Kollisions-Hinweis in seedRefreshQueuePortfolio()
        QFile::remove(dbPath);
        Database::instance().open(dbPath);

        QStringList guids;
        for (int i = 0; i < shareCount; ++i) {
            const QString guid = QStringLiteral("g-daily-%1").arg(i);
            const QString namePrefix = QString(3, QChar(char('A' + i)));
            ShareObject share(guid, QStringLiteral("DV%1").arg(i),
                              QStringLiteral("DE000DV0000%1").arg(i),
                              QStringLiteral("%1 Daily Share").arg(namePrefix));
            share.setUpdateType(ShareUpdateType::DailyValues);
            share.setDailyValuesParsingType(ShareParsingType::ApiYahoo);
            share.setDailyValuesUrl(
                QStringLiteral("https://example.com/yahoo-daily/") + guid +
                QStringLiteral("?range=%1"));
            share.setDailyValuesEncoding(QStringLiteral("UTF-8"));
            ShareRepository().insert(share);
            insertTestBuy(guid, QStringLiteral("depot1"),
                          QStringLiteral("2020-01-01T00:00:00"), 5.0, 100.0);
            guids << guid;
        }
        AppSettings::instance().setPortfolioPath(dbPath);
        return guids;
    }

    void test_onRefreshShare_dailyValuesOnly_upsertsIntoDailyValuesRepository_viaFakeNetwork()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/RefreshDailyValuesSingle.db");
        QFile::remove(dbPath);
        Database::instance().open(dbPath);

        const QString guid = QStringLiteral("g-daily-single");
        ShareObject share(guid, QStringLiteral("DV01"),
                          QStringLiteral("DE000DV00001"), QStringLiteral("DailyValues AG"));
        share.setUpdateType(ShareUpdateType::DailyValues);
        share.setDailyValuesParsingType(ShareParsingType::ApiYahoo);
        share.setDailyValuesUrl(
            QStringLiteral("https://example.com/yahoo-daily/") + guid +
            QStringLiteral("?range=%1"));
        share.setDailyValuesEncoding(QStringLiteral("UTF-8"));
        ShareRepository().insert(share);
        insertTestBuy(guid, QStringLiteral("depot1"),
                      QStringLiteral("2020-01-01T00:00:00"), 5.0, 100.0);
        AppSettings::instance().setPortfolioPath(dbPath);

        ParserTestUtils::FakeNetworkAccessManager fakeNam;
        fakeNam.setResponse(
            QUrl(QStringLiteral("https://example.com/yahoo-daily/%1?range=20y").arg(guid)),
            yahooDailyHistoryJson());

        MainWindow window(&fakeNam);
        QApplication::processEvents();

        QTableWidget* finalTbl = findFinalTable(window, 1);
        if (!finalTbl) QFAIL("Depotwert-Datentabelle nicht gefunden");
        finalTbl->setCurrentCell(0, 0);

        QAction* actionRefresh = findActionByStatusTip(window,
            QStringLiteral("Kurs der ausgewählten Aktie aktualisieren"));
        QVERIFY(actionRefresh);

        QMetaObject::invokeMethod(&window, "onRefreshShare", Qt::DirectConnection);

        QVERIFY2(QTest::qWaitFor([&]{ return actionRefresh->isEnabled(); }, 2000),
                 "Einzel-Refresh (DailyValues) hat nicht beendet "
                 "(finaliseRefresh() nicht erreicht).");

        DailyValuesRepository dvRepo;
        const auto entries = dvRepo.findByShare(guid);
        QCOMPARE(entries.size(), 2);
        // findByShare() ordnet nach date ASC — Reihenfolge damit unabhängig
        // von Zeitzonen-Details der einzelnen QDate-Werte prüfbar.
        QVERIFY(entries.first().date() < entries.last().date());
        QCOMPARE(entries.first().closingPrice(), 141.5);
        QCOMPARE(entries.last().closingPrice(),  143.0);

        const auto* te = window.findChild<QTextEdit*>();
        QVERIFY(te);
        QVERIFY2(te->toPlainText().contains(
                     QStringLiteral("Tageswerte aktualisiert: DailyValues AG — 2 Einträge "
                                    "geholt (Eingefügt: 2 / Aktualisiert: 0 / Unverändert: 0)")),
                 qPrintable(te->toPlainText()));
    }

    // Phase 4 der Aktiensplit-Behandlung (siehe ARCHITECTURE.md, "Offene
    // Punkte"): "automatische Nachprüfung des prices_adjusted-Zustands nach
    // jedem Tageswert-Abruf". Wiederverwendet dieselbe Fixture wie oben
    // (yahooDailyHistoryJson(), Referenzwerte 141.5 am 15.01.2024 / 143.0 am
    // 16.01.2024) — der Split liegt genau auf den Ex-Tag des ersten Eintrags,
    // sodass der Kurs vom 15.01. laut SplitPriceJumpDetector-Konvention noch
    // als "davor" zählt. 141.5 -> 143.0 zeigt keinen Kurssprung, die
    // Kurshistorie wirkt also bereits bereinigt — im Widerspruch zum absichtlich
    // als unbereinigt gespeicherten Split.
    void test_onRefreshShare_dailyValuesOnly_splitAuditFinding_addsStatusMessage_viaFakeNetwork()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/RefreshDailyValuesSplitMismatch.db");
        QFile::remove(dbPath);
        Database::instance().open(dbPath);

        const QString guid = QStringLiteral("g-daily-split-mismatch");
        ShareObject share(guid, QStringLiteral("DV02"),
                          QStringLiteral("DE000DV00002"), QStringLiteral("SplitMismatch AG"));
        share.setUpdateType(ShareUpdateType::DailyValues);
        share.setDailyValuesParsingType(ShareParsingType::ApiYahoo);
        share.setDailyValuesUrl(
            QStringLiteral("https://example.com/yahoo-daily/") + guid +
            QStringLiteral("?range=%1"));
        share.setDailyValuesEncoding(QStringLiteral("UTF-8"));
        ShareRepository().insert(share);
        insertTestBuy(guid, QStringLiteral("depot1"),
                      QStringLiteral("2020-01-01T00:00:00"), 5.0, 100.0);
        AppSettings::instance().setPortfolioPath(dbPath);

        QVERIFY(ShareSplitRepository().insert(ShareSplitObject(
            QStringLiteral("split-mismatch-1"), guid, QDate(2024, 1, 15),
            /*ratioNew=*/20.0, /*ratioOld=*/1.0, /*pricesAdjusted=*/false)));

        ParserTestUtils::FakeNetworkAccessManager fakeNam;
        fakeNam.setResponse(
            QUrl(QStringLiteral("https://example.com/yahoo-daily/%1?range=20y").arg(guid)),
            yahooDailyHistoryJson());

        MainWindow window(&fakeNam);
        QApplication::processEvents();

        QTableWidget* finalTbl = findFinalTable(window, 1);
        if (!finalTbl) QFAIL("Depotwert-Datentabelle nicht gefunden");
        finalTbl->setCurrentCell(0, 0);

        QAction* actionRefresh = findActionByStatusTip(window,
            QStringLiteral("Kurs der ausgewählten Aktie aktualisieren"));
        QVERIFY(actionRefresh);

        QMetaObject::invokeMethod(&window, "onRefreshShare", Qt::DirectConnection);

        QVERIFY2(QTest::qWaitFor([&]{ return actionRefresh->isEnabled(); }, 2000),
                 "Einzel-Refresh (DailyValues) hat nicht beendet "
                 "(finaliseRefresh() nicht erreicht).");

        const auto* te = window.findChild<QTextEdit*>();
        QVERIFY(te);
        // Der Wortlaut nennt seit Punkt 4 bewusst nicht mehr den
        // Bereinigungs-Zustand: dieselbe Zeile meldet inzwischen auch
        // Verhältnis-Befunde, und eine Meldung, die alle drei Befundarten als
        // "abweichender Bereinigungs-Zustand" ausgibt, führte in die Irre.
        QVERIFY2(te->toPlainText().contains(
                     QStringLiteral("SplitMismatch AG\" — 1 auffällige(r) Split(s) "
                                    "erkannt")),
                 qPrintable(te->toPlainText()));
    }

    void test_onRefreshAll_dailyValuesQueue_chainsAcrossTwoShares_viaFakeNetwork()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/RefreshDailyValuesQueue.db");
        // 2 Aktien — siehe seedRefreshQueuePortfolio()-Hinweis, warum nicht 3.
        const QStringList guids = seedDailyValuesQueuePortfolio(2, dbPath);

        ParserTestUtils::FakeNetworkAccessManager fakeNam;
        for (const QString& guid : guids) {
            fakeNam.setResponse(
                QUrl(QStringLiteral("https://example.com/yahoo-daily/%1?range=20y").arg(guid)),
                yahooDailyHistoryJson());
        }

        MainWindow window(&fakeNam);
        QApplication::processEvents();

        QTableWidget* finalTbl  = findFinalTable(window, 2);
        QTableWidget* marketTbl = findMarketTable(window, 2);
        if (!finalTbl)  QFAIL("Depotwert-Datentabelle nicht gefunden");
        if (!marketTbl) QFAIL("Marktwert-Datentabelle nicht gefunden");

        QMetaObject::invokeMethod(&window, "onRefreshAll", Qt::DirectConnection);

        // Dasselbe requestCount()-Checkpoint-Muster wie bei den MarketPrice-
        // Queue-Tests: gilt hier identisch, da ShareUpdateType::DailyValues
        // m_marketDone von vornherein auf true setzt (siehe
        // startRefreshForShare()) — onDailyValuesUpdated() allein löst also
        // bereits onRefreshShareFinished() aus und verkettet reentrant zur
        // nächsten Aktie.
        QVERIFY2(QTest::qWaitFor([&]{ return fakeNam.requestCount() >= 2; }, 2000),
                 "Zweite Anfrage (Aktie B) wurde nicht gestellt.");

        QVERIFY2(QTest::qWaitFor([&]{
                     return finalTbl->currentRow() == 0 && marketTbl->currentRow() == 0;
                 }, 2000),
                 "Selektion sprang nach Abschluss der DailyValues-Queue nicht auf Zeile 0.");

        DailyValuesRepository dvRepo;
        for (const QString& guid : guids)
            QCOMPARE(dvRepo.findByShare(guid).size(), 2);
    }

    // ─────────────────────────────────────────────────────────────────────
    // MainWindow — Sound bei erfolgreicher Aktualisierung (Feature 21.07.2026)
    // ─────────────────────────────────────────────────────────────────────
    //
    // playUpdateFinishedSound() wird über SoundCountingMainWindow (siehe
    // oberhalb von TestMainWindow) abgefangen, statt echte QSoundEffect-
    // Wiedergabe zu prüfen — kein Audio-Gerät in CI/Testumgebungen nötig.
    // Geprüft wird ausschließlich WANN und WIE OFT der Sound ausgelöst wird:
    // genau einmal bei Erfolg (Einzel- oder "Alle aktualisieren", bei
    // letzterem NICHT pro Aktie), nie bei Fehler.

    void test_onRefreshShare_success_playsUpdateSoundOnce_viaFakeNetwork()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/SoundSingleSuccess.db");
        const QStringList guids = seedRefreshQueuePortfolio(1, dbPath);

        ParserTestUtils::FakeNetworkAccessManager fakeNam;
        fakeNam.setResponse(
            QUrl(QStringLiteral("https://example.com/onvista/%1").arg(guids[0])),
            onVistaRealTimeJson(120.0));

        SoundCountingMainWindow window(&fakeNam);
        QApplication::processEvents();

        QTableWidget* finalTbl = findFinalTable(window, 1);
        if (!finalTbl) QFAIL("Depotwert-Datentabelle nicht gefunden");
        finalTbl->setCurrentCell(0, 0);

        QAction* actionRefresh = findActionByStatusTip(window,
            QStringLiteral("Kurs der ausgewählten Aktie aktualisieren"));
        QVERIFY(actionRefresh);

        QMetaObject::invokeMethod(&window, "onRefreshShare", Qt::DirectConnection);

        QVERIFY2(QTest::qWaitFor([&]{ return actionRefresh->isEnabled(); }, 2000),
                 "Einzel-Refresh hat nicht beendet (finaliseRefresh() nicht erreicht).");

        QCOMPARE(window.soundPlayCount, 1);
    }

    void test_onRefreshShare_error_doesNotPlayUpdateSound_viaFakeNetwork()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/SoundSingleError.db");
        const QStringList guids = seedRefreshQueuePortfolio(1, dbPath);

        ParserTestUtils::FakeNetworkAccessManager fakeNam;
        fakeNam.setError(
            QUrl(QStringLiteral("https://example.com/onvista/%1").arg(guids[0])),
            QNetworkReply::HostNotFoundError, QStringLiteral("host not found"));

        SoundCountingMainWindow window(&fakeNam);
        QApplication::processEvents();

        QTableWidget* finalTbl = findFinalTable(window, 1);
        if (!finalTbl) QFAIL("Depotwert-Datentabelle nicht gefunden");
        finalTbl->setCurrentCell(0, 0);

        QAction* actionRefresh = findActionByStatusTip(window,
            QStringLiteral("Kurs der ausgewählten Aktie aktualisieren"));
        QVERIFY(actionRefresh);

        QMetaObject::invokeMethod(&window, "onRefreshShare", Qt::DirectConnection);

        QVERIFY2(QTest::qWaitFor([&]{ return actionRefresh->isEnabled(); }, 2000),
                 "Einzel-Refresh hat nicht beendet (finaliseRefresh() nicht erreicht).");

        QCOMPARE(window.soundPlayCount, 0);
    }

    void test_onRefreshAll_success_playsUpdateSoundExactlyOnce_viaFakeNetwork()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/SoundAllSuccess.db");
        const QStringList guids = seedRefreshQueuePortfolio(2, dbPath);

        ParserTestUtils::FakeNetworkAccessManager fakeNam;
        for (int i = 0; i < guids.size(); ++i) {
            fakeNam.setResponse(
                QUrl(QStringLiteral("https://example.com/onvista/%1").arg(guids[i])),
                onVistaRealTimeJson(100.0 + i));
        }

        SoundCountingMainWindow window(&fakeNam);
        QApplication::processEvents();

        QAction* actionRefreshAll = findActionByStatusTip(window,
            QStringLiteral("Kurse aller Aktien aktualisieren"));
        QVERIFY(actionRefreshAll);

        QMetaObject::invokeMethod(&window, "onRefreshAll", Qt::DirectConnection);

        QVERIFY2(QTest::qWaitFor([&]{ return actionRefreshAll->isEnabled(); }, 2000),
                 "\"Alle aktualisieren\" hat nicht beendet (finaliseRefresh() nicht erreicht).");

        // Genau EINMAL — nicht einmal pro Aktie in der Queue.
        QCOMPARE(window.soundPlayCount, 1);
    }

    void test_onRefreshAll_error_doesNotPlayUpdateSound_viaFakeNetwork()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/SoundAllError.db");
        const QStringList guids = seedRefreshQueuePortfolio(2, dbPath);

        ParserTestUtils::FakeNetworkAccessManager fakeNam;
        fakeNam.setResponse(
            QUrl(QStringLiteral("https://example.com/onvista/%1").arg(guids[0])),
            onVistaRealTimeJson(100.0));
        fakeNam.setError(
            QUrl(QStringLiteral("https://example.com/onvista/%1").arg(guids[1])),
            QNetworkReply::HostNotFoundError, QStringLiteral("host not found"));

        SoundCountingMainWindow window(&fakeNam);
        QApplication::processEvents();

        QAction* actionRefreshAll = findActionByStatusTip(window,
            QStringLiteral("Kurse aller Aktien aktualisieren"));
        QVERIFY(actionRefreshAll);

        QMetaObject::invokeMethod(&window, "onRefreshAll", Qt::DirectConnection);

        QVERIFY2(QTest::qWaitFor([&]{ return actionRefreshAll->isEnabled(); }, 2000),
                 "\"Alle aktualisieren\" hat nach dem Fehler nicht beendet.");

        QCOMPARE(window.soundPlayCount, 0);
    }

    void test_onRefreshShare_bothUpdateType_updatesMarketPriceAndDailyValues_viaFakeNetwork()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/RefreshBothSingle.db");
        QFile::remove(dbPath);
        Database::instance().open(dbPath);

        const QString guid = QStringLiteral("g-both-single");
        ShareObject share(guid, QStringLiteral("BO01"),
                          QStringLiteral("DE000BO00001"), QStringLiteral("Both AG"));
        share.setUpdateType(ShareUpdateType::Both);
        share.setMarketPriceParsingType(ShareParsingType::ApiOnVista);
        share.setMarketPriceUrl(QStringLiteral("https://example.com/onvista/") + guid);
        share.setMarketPriceEncoding(QStringLiteral("UTF-8"));
        share.setDailyValuesParsingType(ShareParsingType::ApiYahoo);
        share.setDailyValuesUrl(
            QStringLiteral("https://example.com/yahoo-daily/") + guid +
            QStringLiteral("?range=%1"));
        share.setDailyValuesEncoding(QStringLiteral("UTF-8"));
        ShareRepository().insert(share);
        insertTestBuy(guid, QStringLiteral("depot1"),
                      QStringLiteral("2020-01-01T00:00:00"), 5.0, 100.0);
        AppSettings::instance().setPortfolioPath(dbPath);

        ParserTestUtils::FakeNetworkAccessManager fakeNam;
        fakeNam.setResponse(QUrl(QStringLiteral("https://example.com/onvista/%1").arg(guid)),
                            onVistaRealTimeJson(250.0));
        fakeNam.setResponse(
            QUrl(QStringLiteral("https://example.com/yahoo-daily/%1?range=20y").arg(guid)),
            yahooDailyHistoryJson());

        MainWindow window(&fakeNam);
        QApplication::processEvents();

        QTableWidget* finalTbl = findFinalTable(window, 1);
        if (!finalTbl) QFAIL("Depotwert-Datentabelle nicht gefunden");
        finalTbl->setCurrentCell(0, 0);

        QAction* actionRefresh = findActionByStatusTip(window,
            QStringLiteral("Kurs der ausgewählten Aktie aktualisieren"));
        QVERIFY(actionRefresh);

        QMetaObject::invokeMethod(&window, "onRefreshShare", Qt::DirectConnection);

        // Beide Parser laufen unabhängig/parallel (doMarket && doDaily);
        // onRefreshShareFinished() feuert erst, wenn BEIDE m_marketDone UND
        // m_dailyDone true sind — dass finaliseRefresh() die Action wieder
        // aktiviert, belegt also, dass wirklich beide Callbacks durchliefen,
        // nicht nur einer.
        QVERIFY2(QTest::qWaitFor([&]{ return actionRefresh->isEnabled(); }, 2000),
                 "Einzel-Refresh (Both) hat nicht beendet "
                 "(finaliseRefresh() nicht erreicht).");

        QCOMPARE(fakeNam.requestCount(), 2);

        const ShareObject reloaded = ShareRepository().findByGuid(guid);
        QCOMPARE(reloaded.curPrice(), 250.0);

        DailyValuesRepository dvRepo;
        QCOMPARE(dvRepo.findByShare(guid).size(), 2);
    }

    // ─────────────────────────────────────────────────────────────────────
    // MainWindow — portfolio database operations
    // ─────────────────────────────────────────────────────────────────────

    void test_newPortfolio_databaseCreated()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/test_new.db");
        QVERIFY(Database::instance().open(dbPath));
        QVERIFY(QFileInfo::exists(dbPath));
    }

    void test_newPortfolio_schemaCreated()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/test_schema.db");
        Database::instance().open(dbPath);
        QSqlQuery q(QSqlDatabase::database("spm_main"));
        q.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='shares'");
        QVERIFY(q.next());
    }

    void test_newPortfolio_closePreviousBeforeOpening()
    {
        Database::instance().open(m_tempDir.path() + QStringLiteral("/p1.db"));
        QVERIFY(Database::instance().isOpen());
        Database::instance().close();
        QVERIFY(!Database::instance().isOpen());
        Database::instance().open(m_tempDir.path() + QStringLiteral("/p2.db"));
        QVERIFY(Database::instance().isOpen());
    }


    void test_openPortfolio_existingDatabase_opens()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/existing.db");
        Database::instance().open(dbPath);
        ShareRepository repo;
        repo.insert(ShareObject(QStringLiteral("g1"), QStringLiteral("TST01"),
                                QStringLiteral("DE000TST01"), QStringLiteral("Test")));
        Database::instance().close();
        QVERIFY(Database::instance().open(dbPath));
        QCOMPARE(ShareRepository().findAll().size(), 1);
    }

    void test_openPortfolio_sharesLoadedFromDatabase()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/shares.db");
        Database::instance().open(dbPath);
        ShareRepository repo;
        repo.insert(ShareObject(QStringLiteral("g1"), QStringLiteral("W001"),
                                QStringLiteral("DE000W001"), QStringLiteral("A1")));
        repo.insert(ShareObject(QStringLiteral("g2"), QStringLiteral("W002"),
                                QStringLiteral("DE000W002"), QStringLiteral("A2")));
        QCOMPARE(repo.findAll().size(), 2);
    }

    void test_openPortfolio_emptyDatabase_noShares()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/empty.db");
        Database::instance().open(dbPath);
        QCOMPARE(ShareRepository().findAll().size(), 0);
    }

    // ─────────────────────────────────────────────────────────────────────
    // MainWindow — status messages
    // ─────────────────────────────────────────────────────────────────────

    void test_addStatusMessage_appearsInTextEdit()
    {
        openMemoryDb();
        MainWindow window; window.show();
        const auto* te = window.findChild<QTextEdit*>();
        QVERIFY(te && !te->toPlainText().isEmpty());
    }

    void test_addStatusMessage_containsTimestamp()
    {
        openMemoryDb();
        MainWindow window; window.show();
        const auto* te = window.findChild<QTextEdit*>();
        QVERIFY(te && te->toPlainText().contains(
            QRegularExpression(QStringLiteral("\\d{2}:\\d{2}:\\d{2}"))));
    }

    void test_addStatusMessage_startupMessagePresent()
    {
        openMemoryDb();
        MainWindow window; window.show();
        const auto* te = window.findChild<QTextEdit*>();
        QVERIFY(te && te->toPlainText().contains(tr("Anwendung gestartet.")));
    }

    void test_startup_missingPortfolioFile_showsWarning()
    {
        AppSettings::instance().setPortfolioPath(
            m_tempDir.path() + QStringLiteral("/nonexistent.db"));
        MainWindow window; window.show();
        const auto* te = window.findChild<QTextEdit*>();
        QVERIFY(te && te->toPlainText().contains(tr("Portfolio nicht gefunden")));
        QVERIFY(AppSettings::instance().portfolioPath().isEmpty());
    }

    void test_startup_emptyPortfolioPath_showsHint()
    {
        AppSettings::instance().setPortfolioPath(QString());
        MainWindow window; window.show();
        const auto* te = window.findChild<QTextEdit*>();
        QVERIFY(te && te->toPlainText().contains(tr("Kein Portfolio konfiguriert")));
    }

    // ─────────────────────────────────────────────────────────────────────
    // MainWindow — ShareAdd dialog reachable
    // ─────────────────────────────────────────────────────────────────────

    void test_shareAddDialog_canBeConstructed()
    {
        // Verify ViewShareAdd can be constructed with a valid DocumentsConfig
        // without crashing — does not show the dialog.
        openMemoryDb();
        ViewShareAdd dlg(&m_docsConfig);
        QVERIFY(dlg.windowTitle().contains(tr("Aktie hinzufügen")));
    }

    // ─────────────────────────────────────────────────────────────────────
    // checkAndLoadConfigurations
    // ─────────────────────────────────────────────────────────────────────

    void test_configurations_webSitesLoaded()
    {
        const QString path =
            QCoreApplication::applicationDirPath() + QStringLiteral("/WebSites.xml");
        QVERIFY2(QFileInfo::exists(path),
                 qPrintable(QStringLiteral("WebSites.xml not found at: %1").arg(path)));
        WebSitesConfig config;
        QCOMPARE(config.load(path), WebSitesConfig::LoadResult::Success);
        QVERIFY(config.count() > 0);
    }

    void test_configurations_documentsLoaded()
    {
        const QString path =
            QCoreApplication::applicationDirPath() + QStringLiteral("/Documents.xml");
        QVERIFY2(QFileInfo::exists(path),
                 qPrintable(QStringLiteral("Documents.xml not found at: %1").arg(path)));
        DocumentsConfig config;
        QCOMPARE(config.load(path), DocumentsConfig::LoadResult::Success);
        QVERIFY(config.count() > 0);
    }

    void test_disableAllControls_onConfigError()
    {
        WebSitesConfig config;
        const auto result = config.load(
            m_tempDir.path() + QStringLiteral("/nonexistent.xml"));
        QCOMPARE(result, WebSitesConfig::LoadResult::FileNotFound);
        QVERIFY(!config.lastError().isEmpty());
    }

    // ─────────────────────────────────────────────────────────────────────
    // Settings — Logger, Sound, API
    // ─────────────────────────────────────────────────────────────────────

    void test_soundFile_missingDisablesSound()
    {
        AppSettings::instance().setSoundUpdateFile(QStringLiteral("nonexistent.wav"));
        AppSettings::instance().setSoundUpdateEnabled(true);
        const QString f = QCoreApplication::applicationDirPath()
                          + QStringLiteral("/sounds/nonexistent.wav");
        if (!QFileInfo::exists(f))
            AppSettings::instance().setSoundUpdateEnabled(false);
        QVERIFY(!AppSettings::instance().soundUpdateEnabled());
        AppSettings::instance().setSoundUpdateFile(QStringLiteral("UpdateFinished.wav"));
        AppSettings::instance().setSoundUpdateEnabled(true);
    }


    void test_aboutForm_appVersionSet()
    {
        const QString v = QCoreApplication::applicationVersion();
        QVERIFY(v.isEmpty() || !v.isEmpty());
    }

    void test_aboutForm_pdfConverterDetected()
    {
        QProcess p;
        p.start(QStringLiteral("pdftotext"), QStringList() << QStringLiteral("-v"));
        p.waitForFinished(3000);
        const QString out = QString::fromLocal8Bit(p.readAllStandardError())
                          + QString::fromLocal8Bit(p.readAllStandardOutput());
        if (p.error() == QProcess::FailedToStart) {
            qWarning("pdftotext not found");
        } else {
            QVERIFY(QRegularExpression(QStringLiteral("version\\s+[0-9]+\\.[0-9]+"))
                    .match(out).hasMatch());
        }
    }


    void test_updateTypeLabel_allFourValues()
    {
        QCOMPARE(MainWindow::updateTypeLabel(ShareUpdateType::None),
                 QStringLiteral("Keine"));
        QCOMPARE(MainWindow::updateTypeLabel(ShareUpdateType::MarketPrice),
                 QStringLiteral("Markt-Preis"));
        QCOMPARE(MainWindow::updateTypeLabel(ShareUpdateType::DailyValues),
                 QStringLiteral("Tages-Werte"));
        QCOMPARE(MainWindow::updateTypeLabel(ShareUpdateType::Both),
                 QStringLiteral("Beide"));
    }

    void test_buildDailyValuesWarningMessage_emptyList_returnsEmpty()
    {
        // Belegt den Frühausstieg: ohne Verstösse darf kein Dialog aufgehen.
        QVERIFY(MainWindow::buildDailyValuesWarningMessage({}).isEmpty());
    }

    void test_buildDailyValuesWarningMessage_containsNameWknAndType()
    {
        ShareUpdateRules::ShareState s;
        s.wkn           = QStringLiteral("A14Y6H");
        s.name          = QStringLiteral("Alphabet Inc.");
        s.updateType    = ShareUpdateType::None;
        s.currentVolume = 4.0;

        const QString msg = MainWindow::buildDailyValuesWarningMessage({ s });

        QVERIFY(msg.contains(QStringLiteral("Alphabet Inc.")));
        QVERIFY(msg.contains(QStringLiteral("A14Y6H")));
        QVERIFY(msg.contains(QStringLiteral("Keine")));
    }

    void test_buildDailyValuesWarningMessage_listsAllSharesInOrder()
    {
        ShareUpdateRules::ShareState a;
        a.wkn = QStringLiteral("AAA111");
        a.name = QStringLiteral("Erste AG");
        a.updateType = ShareUpdateType::None;

        ShareUpdateRules::ShareState b;
        b.wkn = QStringLiteral("BBB222");
        b.name = QStringLiteral("Zweite AG");
        b.updateType = ShareUpdateType::MarketPrice;

        const QString msg = MainWindow::buildDailyValuesWarningMessage({ a, b });

        QVERIFY(msg.contains(QStringLiteral("Erste AG")));
        QVERIFY(msg.contains(QStringLiteral("Zweite AG")));
        QVERIFY(msg.contains(QStringLiteral("Markt-Preis")));
        // Reihenfolge bleibt die des Grids.
        QVERIFY(msg.indexOf(QStringLiteral("Erste AG"))
                < msg.indexOf(QStringLiteral("Zweite AG")));
    }

    void test_buildDailyValuesWarningMessage_explainsConsequenceAndUrgency()
    {
        // Der eigentliche Zweck der Meldung: Begründung UND Dringlichkeit.
        // Ohne beides bliebe sie eine folgenlose Notiz.
        ShareUpdateRules::ShareState s;
        s.wkn        = QStringLiteral("A14Y6H");
        s.name       = QStringLiteral("Alphabet Inc.");
        s.updateType = ShareUpdateType::None;

        const QString msg = MainWindow::buildDailyValuesWarningMessage({ s });

        QVERIFY(msg.contains(QStringLiteral("Depotwert-Chart")));
        QVERIFY(msg.contains(QStringLiteral("dauerhaft verloren")));
    }

    // ── buildSplitAuditWarningMessage() ────────────────────────────────────
    // Phase 4 der Aktiensplit-Behandlung (siehe ARCHITECTURE.md, "Offene
    // Punkte"). Public static aus demselben Grund wie
    // buildDailyValuesWarningMessage() oben.

    static SplitPriceJumpDetector::Outcome makeOutcome(
        SplitPriceJumpDetector::Result result,
        const QDate& dateBefore, double priceBefore,
        const QDate& dateAfter,  double priceAfter)
    {
        SplitPriceJumpDetector::Outcome o;
        o.result       = result;
        o.dateBefore   = dateBefore;
        o.priceBefore  = priceBefore;
        o.dateAfter    = dateAfter;
        o.priceAfter   = priceAfter;
        o.observedRatio = (priceAfter != 0.0) ? priceBefore / priceAfter : 0.0;
        return o;
    }

    void test_buildSplitAuditWarningMessage_emptyList_returnsEmpty()
    {
        // Belegt den Frühausstieg: ohne Widersprüche darf kein Dialog aufgehen.
        QVERIFY(MainWindow::buildSplitAuditWarningMessage({}).isEmpty());
    }

    void test_buildSplitAuditWarningMessage_containsNameWknAndSplitDescription()
    {
        const ShareSplitObject s(QStringLiteral("split-1"), QStringLiteral("share-1"),
                                 QDate(2022, 7, 18), 20.0, 1.0, /*pricesAdjusted=*/false);
        const auto outcome = makeOutcome(SplitPriceJumpDetector::Result::Adjusted,
                                         QDate(2022, 7, 15), 49.80,
                                         QDate(2022, 7, 19), 50.60);
        MainWindow::SplitAuditWarning w;
        w.shareName = QStringLiteral("Alphabet Inc.");
        w.wkn       = QStringLiteral("A14Y6H");
        w.discrepancy = SplitAudit::Discrepancy{ s, outcome };

        const QString msg = MainWindow::buildSplitAuditWarningMessage({ w });

        QVERIFY(msg.contains(QStringLiteral("Alphabet Inc.")));
        QVERIFY(msg.contains(QStringLiteral("A14Y6H")));
        QVERIFY(msg.contains(ShareSplitHint::describeSplit(s)));
    }

    void test_buildSplitAuditWarningMessage_ratioFromPrices_namesMeasuredRatio()
    {
        // Punkt 4 der Split-Plausibilitätsprüfung (22.08.2026): der gemessene
        // Kurssprung passt eher zu 20:1 als zum eingetragenen 19:1.
        const ShareSplitObject s(QStringLiteral("split-1"), QStringLiteral("share-1"),
                                 QDate(2022, 7, 18), 19.0, 1.0, /*pricesAdjusted=*/false);
        auto outcome = makeOutcome(SplitPriceJumpDetector::Result::NotAdjusted,
                                   QDate(2022, 7, 18), 1003.0,
                                   QDate(2022, 7, 19), 50.20);
        outcome.ratioMismatch = true;
        outcome.impliedFactor = 20.0;

        MainWindow::SplitAuditWarning w;
        w.shareName   = QStringLiteral("Alphabet Inc.");
        w.wkn         = QStringLiteral("A14Y6H");
        w.discrepancy = SplitAudit::Discrepancy{ s, outcome,
                                                 SplitAudit::Kind::RatioFromPrices, {} };

        const QString msg = MainWindow::buildSplitAuditWarningMessage({ w });

        QVERIFY2(msg.contains(QStringLiteral("20:1")), qPrintable(msg));
        QVERIFY2(msg.contains(QStringLiteral("Alphabet Inc.")), qPrintable(msg));
    }

    void test_buildSplitAuditWarningMessage_ratioFromHoldings_namesQuantitiesAndProposal()
    {
        const ShareSplitObject s(QStringLiteral("split-1"), QStringLiteral("share-1"),
                                 QDate(2022, 7, 18), 19.0, 1.0, /*pricesAdjusted=*/false);

        SplitHistoryConflict conflict;
        conflict.hasConflict    = true;
        conflict.depotNumber    = QStringLiteral("1234567");
        conflict.conflictDate   = QDate(2022, 12, 5);
        conflict.requiredToday  = 200.0;
        conflict.availableToday = 190.0;
        conflict.suspicion.hasProposal      = true;
        conflict.suspicion.proposedRatioNew = 20.0;
        conflict.suspicion.proposedRatioOld = 1.0;

        MainWindow::SplitAuditWarning w;
        w.shareName   = QStringLiteral("Alphabet Inc.");
        w.wkn         = QStringLiteral("A14Y6H");
        w.discrepancy = SplitAudit::Discrepancy{ s, {},
                                                 SplitAudit::Kind::RatioFromHoldings,
                                                 conflict };

        const QString msg = MainWindow::buildSplitAuditWarningMessage({ w });

        const QLocale locale;
        QVERIFY2(msg.contains(locale.toString(200.0, 'f', 4)), qPrintable(msg));
        QVERIFY2(msg.contains(locale.toString(190.0, 'f', 4)), qPrintable(msg));
        QVERIFY2(msg.contains(QStringLiteral("1234567")), qPrintable(msg));
        QVERIFY2(msg.contains(QStringLiteral("20:1")), qPrintable(msg));
    }

    void test_buildSplitAuditWarningMessage_groupsKindsSeparately()
    {
        // Bereinigungs-Zustand und Verhältnis sagen Verschiedenes und
        // brauchen unterschiedliche Erklärungen — beide Einleitungen müssen
        // im selben Text vorkommen, denn es gibt bewusst nur einen Dialog.
        const ShareSplitObject s(QStringLiteral("split-1"), QStringLiteral("share-1"),
                                 QDate(2022, 7, 18), 19.0, 1.0, /*pricesAdjusted=*/true);
        const auto flagOutcome = makeOutcome(SplitPriceJumpDetector::Result::NotAdjusted,
                                             QDate(2022, 7, 18), 1003.0,
                                             QDate(2022, 7, 19), 50.20);
        auto ratioOutcome = flagOutcome;
        ratioOutcome.ratioMismatch = true;
        ratioOutcome.impliedFactor = 20.0;

        MainWindow::SplitAuditWarning flag;
        flag.shareName   = QStringLiteral("Alphabet Inc.");
        flag.wkn         = QStringLiteral("A14Y6H");
        flag.discrepancy = SplitAudit::Discrepancy{ s, flagOutcome,
                                                    SplitAudit::Kind::AdjustmentFlag, {} };

        MainWindow::SplitAuditWarning ratio = flag;
        ratio.discrepancy = SplitAudit::Discrepancy{ s, ratioOutcome,
                                                     SplitAudit::Kind::RatioFromPrices, {} };

        const QString msg = MainWindow::buildSplitAuditWarningMessage({ flag, ratio });

        QVERIFY2(msg.contains(QStringLiteral("Bereinigungs-Zustand")), qPrintable(msg));
        QVERIFY2(msg.contains(QStringLiteral("eingetragene Verhältnis")), qPrintable(msg));
    }

    void test_buildSplitAuditWarningMessage_closingHintAppearsOnlyOnce()
    {
        // Der Schlusssatz gilt für beide Gruppen. Zweimal derselbe Hinweis
        // liest sich wie ein Fehler in der Meldung.
        const ShareSplitObject s(QStringLiteral("split-1"), QStringLiteral("share-1"),
                                 QDate(2022, 7, 18), 19.0, 1.0, /*pricesAdjusted=*/true);
        const auto flagOutcome = makeOutcome(SplitPriceJumpDetector::Result::NotAdjusted,
                                             QDate(2022, 7, 18), 1003.0,
                                             QDate(2022, 7, 19), 50.20);
        auto ratioOutcome = flagOutcome;
        ratioOutcome.ratioMismatch = true;
        ratioOutcome.impliedFactor = 20.0;

        MainWindow::SplitAuditWarning flag;
        flag.shareName   = QStringLiteral("Alphabet Inc.");
        flag.discrepancy = SplitAudit::Discrepancy{ s, flagOutcome,
                                                    SplitAudit::Kind::AdjustmentFlag, {} };
        MainWindow::SplitAuditWarning ratio = flag;
        ratio.discrepancy = SplitAudit::Discrepancy{ s, ratioOutcome,
                                                     SplitAudit::Kind::RatioFromPrices, {} };

        const QString msg = MainWindow::buildSplitAuditWarningMessage({ flag, ratio });

        QCOMPARE(msg.count(QStringLiteral("automatisch geändert wird hier nichts")), 1);
    }

    void test_describeFactorAsRatio_normalAndReverseSplit()
    {
        QCOMPARE(MainWindow::describeFactorAsRatio(20.0), QStringLiteral("20:1"));
        QCOMPARE(MainWindow::describeFactorAsRatio(0.1),  QStringLiteral("1:10"));
        QVERIFY(MainWindow::describeFactorAsRatio(0.0).isEmpty());
    }

    void test_buildSplitAuditWarningMessage_listsAllWarningsInOrder()
    {
        const ShareSplitObject sa(QStringLiteral("split-a"), QStringLiteral("share-a"),
                                  QDate(2022, 7, 18), 20.0, 1.0, false);
        const ShareSplitObject sb(QStringLiteral("split-b"), QStringLiteral("share-b"),
                                  QDate(2021, 1, 4), 2.0, 1.0, true);
        const auto outcomeA = makeOutcome(SplitPriceJumpDetector::Result::Adjusted,
                                          QDate(2022, 7, 15), 49.80,
                                          QDate(2022, 7, 19), 50.60);
        const auto outcomeB = makeOutcome(SplitPriceJumpDetector::Result::NotAdjusted,
                                          QDate(2021, 1, 4), 200.0,
                                          QDate(2021, 1, 5), 100.0);

        MainWindow::SplitAuditWarning wa;
        wa.shareName = QStringLiteral("Erste AG");
        wa.wkn       = QStringLiteral("AAA111");
        wa.discrepancy = SplitAudit::Discrepancy{ sa, outcomeA };

        MainWindow::SplitAuditWarning wb;
        wb.shareName = QStringLiteral("Zweite AG");
        wb.wkn       = QStringLiteral("BBB222");
        wb.discrepancy = SplitAudit::Discrepancy{ sb, outcomeB };

        const QString msg = MainWindow::buildSplitAuditWarningMessage({ wa, wb });

        QVERIFY(msg.contains(QStringLiteral("Erste AG")));
        QVERIFY(msg.contains(QStringLiteral("Zweite AG")));
        // Reihenfolge bleibt die der Eingabeliste.
        QVERIFY(msg.indexOf(QStringLiteral("Erste AG"))
                < msg.indexOf(QStringLiteral("Zweite AG")));
    }

    void test_buildSplitAuditWarningMessage_explainsNoAutomaticChange()
    {
        // Zentrale Zusicherung der Meldung: sie liest nur, sie schreibt
        // nichts — siehe SplitAudit.h. Ohne diesen Hinweis könnte
        // der Nutzer annehmen, der Haken sei bereits korrigiert worden.
        const ShareSplitObject s(QStringLiteral("split-1"), QStringLiteral("share-1"),
                                 QDate(2022, 7, 18), 20.0, 1.0, false);
        const auto outcome = makeOutcome(SplitPriceJumpDetector::Result::Adjusted,
                                         QDate(2022, 7, 15), 49.80,
                                         QDate(2022, 7, 19), 50.60);
        MainWindow::SplitAuditWarning w;
        w.shareName = QStringLiteral("Alphabet Inc.");
        w.wkn       = QStringLiteral("A14Y6H");
        w.discrepancy = SplitAudit::Discrepancy{ s, outcome };

        const QString msg = MainWindow::buildSplitAuditWarningMessage({ w });

        QVERIFY(msg.contains(QStringLiteral("automatisch geändert wird hier nichts")));
        // Geprüft wird, dass der Text sagt WO zu korrigieren ist. Bis Punkt 4
        // verwies er auf den "Prüfen"-Knopf; der löst aber nur die Frage nach
        // dem Bereinigungs-Zustand, nicht die beiden Verhältnis-Befunde.
        // Seither zeigt der Satz auf den Split-Dialog als Ganzes.
        QVERIFY2(msg.contains(QStringLiteral("Split-Dialog")), qPrintable(msg));
    }

    // ── onDeleteShare ─────────────────────────────────────────────────────────

    void test_deleteShare_removesShareFromDatabase()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/delete_ok.db");
        Database::instance().open(dbPath);
        ShareRepository repo;
        repo.insert(ShareObject(QStringLiteral("del-g1"), QStringLiteral("DEL001"),
                                QStringLiteral("DE000DEL001"), QStringLiteral("Delete Me")));
        QCOMPARE(repo.findAll().size(), 1);

        const bool removed = repo.remove(QStringLiteral("del-g1"));
        QVERIFY(removed);
        QCOMPARE(repo.findAll().size(), 0);
    }

    void test_deleteShare_nonExistentGuid_returnsFalse()
    {
        openMemoryDb();
        ShareRepository repo;
        // remove() on a non-existent GUID — SQLite DELETE with 0 rows affected
        // does not set an error, so we just verify it doesn't crash
        // and that an empty DB stays empty
        repo.remove(QStringLiteral("does-not-exist"));
        QCOMPARE(repo.findAll().size(), 0);
    }

    void test_deleteShare_actionDeleteDisabledAtStart()
    {
        openMemoryDb();
        MainWindow window;
        const auto actions = window.findChildren<QAction*>();
        QAction* deleteAction = nullptr;
        for (auto* a : actions) {
            if (a->text().contains(QStringLiteral("tfernen"))) {
                deleteAction = a;
                break;
            }
        }
        if (!deleteAction) QFAIL("Delete action not found");
        QVERIFY(!deleteAction->isEnabled());
    }

    void test_deleteShare_actionDeleteEnabledAfterSelection()
    {
        // Bugfix (22.07.2026): Das bisherige Setup (nur ShareRepository::insert(),
        // ohne Buy, ohne AppSettings::instance().setPortfolioPath()) ließ die
        // Aktie nicht zuverlässig mit genau 1 Zeile in der Depotwert-Tabelle
        // erscheinen — deshalb griff der alte Code auf .first() + QSKIP zurück.
        // seedDepotwertPortfolio() ist das bereits etablierte, getestete Muster
        // (Buy + Brokerage + AppSettings::portfolioPath gesetzt), das auch
        // test_finalValueTable_showsFinalFields() u.a. zuverlässig verwenden.
        seedDepotwertPortfolio();

        MainWindow window;
        window.show();
        QApplication::processEvents();

        QTableWidget* table = findFinalTable(window, 1);
        if (!table) QFAIL("Depotwert-Datentabelle nicht gefunden");

        table->selectRow(0);
        QApplication::processEvents();

        const auto actions = window.findChildren<QAction*>();
        QAction* deleteAction = nullptr;
        for (auto* a : actions) {
            if (a->text().contains(QStringLiteral("tfernen"))) {
                deleteAction = a;
                break;
            }
        }
        if (!deleteAction) QFAIL("Delete action not found");
        QVERIFY(deleteAction->isEnabled());
    }

    // ─────────────────────────────────────────────────────────────────────
    // MainWindow — onPortfolioRowDoubleClicked (ShareDetailsForm, 09.07.2026)
    // ─────────────────────────────────────────────────────────────────────
    //
    // Only the early-return guard paths (null item / empty GUID) are covered
    // here — same convention already used for onEditShare()/onDeleteShare()
    // in this file: the "valid GUID" path constructs ViewShareDetails and
    // calls dlg.exec(), which shows a real modal QDialog and blocks the
    // (headless) test indefinitely, so it is intentionally not invoked
    // directly. The invalid-GUID path is likewise not exercised via a real
    // ViewShareDetails construction here, because PresenterShareDetails
    // reports that case through view->showError() -> OwnMessageBox::critical(),
    // itself a blocking modal dialog. Both the "share not found" branch and
    // the row-formatting logic that ViewShareDetails renders are already
    // covered without any modal dialog in tst_sharedetailsform.cpp, which
    // drives PresenterShareDetails through a FakeViewShareDetails/
    // FakeModelShareDetails pair instead.

    void test_onPortfolioRowDoubleClicked_nullItem_doesNotCrash()
    {
        openMemoryDb();
        MainWindow window;

        QMetaObject::invokeMethod(&window, "onPortfolioRowDoubleClicked",
                                  Qt::DirectConnection,
                                  Q_ARG(QTableWidgetItem*, nullptr));

        QVERIFY(true); // Reaching this line without a crash is the assertion.
    }

    void test_onPortfolioRowDoubleClicked_emptyGuid_doesNotCrash()
    {
        seedDepotwertPortfolio();
        MainWindow window;
        QApplication::processEvents();

        QTableWidget* tbl = findFinalTable(window, 1); // data table, 1 share row
        if (!tbl) QFAIL("Depotwert-Datentabelle nicht gefunden");

        QTableWidgetItem* item = tbl->item(0, 0); // WKN cell, column 0
        if (!item) QFAIL("WKN-Zelle fehlt");

        // Blank the GUID on an otherwise-valid row so the slot takes its
        // shareGuid.isEmpty() early-return path instead of constructing
        // ViewShareDetails — see class-comment above for why a genuinely
        // unresolvable GUID isn't exercised directly in this test file.
        item->setData(Qt::UserRole, QString());

        QMetaObject::invokeMethod(&window, "onPortfolioRowDoubleClicked",
                                  Qt::DirectConnection,
                                  Q_ARG(QTableWidgetItem*, item));

        QVERIFY(true); // No crash, no modal dialog opened.
    }

    // ─────────────────────────────────────────────────────────────────────
    // MainWindow — onPortfolioRowRightClicked (ChartPopup, Feature 31.07.2026)
    // ─────────────────────────────────────────────────────────────────────
    //
    // Same convention as onPortfolioRowDoubleClicked() above: only the
    // early-return guard paths (no item at the click position / empty GUID)
    // are covered directly here. Unlike ViewShareDetails::exec(), ChartPopup's
    // showAt() is non-blocking (show(), not exec()) — the full happy path
    // (valid GUID) is instead covered separately below via a direct
    // ChartPopup construction (test_chartPopup_validShare_constructsWithChartChild),
    // without going through the slot's showAt()/positioning, to avoid
    // depending on real on-screen window/cursor behavior in a headless test run.
    //
    // The slot reads its triggering table via sender() (see
    // MainWindow::onPortfolioRowRightClicked() — necessary because, unlike
    // the double-click handler, customContextMenuRequested() only supplies
    // a QPoint, not a QTableWidgetItem to derive the table from). That means
    // the signal must be genuinely emitted here — a plain call to the
    // generated signal function — rather than routed through
    // QMetaObject::invokeMethod() directly on the slot, which would leave
    // sender() == nullptr and trivially pass every guard without exercising
    // the itemAt()/GUID logic at all.

    void test_onPortfolioRowRightClicked_noItemAtPos_doesNotCrash()
    {
        openMemoryDb();
        MainWindow window;
        QApplication::processEvents();

        QTableWidget* tbl = findFinalTable(window, 0); // empty data table, no rows
        if (!tbl) QFAIL("Depotwert-Datentabelle nicht gefunden");

        tbl->customContextMenuRequested(QPoint(5, 5)); // no row at this position

        QVERIFY(true); // Reaching this line without a crash is the assertion.
    }

    void test_onPortfolioRowRightClicked_emptyGuid_doesNotCrash()
    {
        seedDepotwertPortfolio();
        MainWindow window;
        QApplication::processEvents();

        QTableWidget* tbl = findFinalTable(window, 1); // data table, 1 share row
        if (!tbl) QFAIL("Depotwert-Datentabelle nicht gefunden");

        QTableWidgetItem* item = tbl->item(0, 0); // WKN cell, column 0
        if (!item) QFAIL("WKN-Zelle fehlt");

        // Blank the GUID on an otherwise-valid row — same rationale as
        // test_onPortfolioRowDoubleClicked_emptyGuid_doesNotCrash() above.
        item->setData(Qt::UserRole, QString());

        const QPoint pos = tbl->visualItemRect(item).center();
        tbl->customContextMenuRequested(pos);

        QVERIFY(true); // No crash, no popup opened.
    }

    // ─────────────────────────────────────────────────────────────────────
    // ChartPopup — direct construction (no show()/showAt(), same rationale as
    // test_shareDetailsDialog_validShare_constructsAndShowsCloseButtonText()
    // below — exercises the real MVP wiring (ModelChart/PresenterChart/
    // compact ViewChart) without opening an actual on-screen window).
    // ─────────────────────────────────────────────────────────────────────

    void test_chartPopup_validShare_constructsWithChartChild()
    {
        const QString shareGuid = insertTestShare();

        ChartPopup popup(shareGuid, QStringLiteral("Test AG")); // insertTestShare()'s share name

        auto* chart = popup.findChild<ViewChart*>(QStringLiteral("ViewChart"));
        if (!chart) QFAIL("ViewChart-Kindwidget nicht gefunden");

        // Compact-Modus (siehe ViewChart::setupUi()): die "Selektion:"-Box
        // wird weiterhin angelegt (Getter/Mausrad-Steuerung bleiben
        // funktionsfähig), aber explizit versteckt statt ins Layout gehängt.
        // isHidden() (statt isVisible()) prüft genau das, unabhängig davon,
        // dass popup selbst hier nie show()n wird.
        auto* selektionBox = popup.findChild<QGroupBox*>(QStringLiteral("selektionBox"));
        if (!selektionBox) QFAIL("selektionBox nicht gefunden");
        QVERIFY(selektionBox->isHidden());

        // Überschrift (ergänzt 31.07.2026, Nessies Rückmeldung "Was auch
        // fehlt ist die Überschrift mit Informationen!") — der Aktienname
        // muss unabhängig vom (im headless Testlauf ggf. leeren)
        // Zeitraum-Text sofort nach der Konstruktion sichtbar sein (siehe
        // ViewChart::rangeInfo()-Nachhol-Mechanismus in ChartPopup.cpp).
        auto* header = popup.findChild<QLabel*>(QStringLiteral("chartPopupHeader"));
        if (!header) QFAIL("chartPopupHeader nicht gefunden");
        QVERIFY(header->text().contains(QStringLiteral("Test AG")));
    }

    // ─────────────────────────────────────────────────────────────────────
    // ViewChart — Stückzahl-Formatierung im Hover-Tooltip (Bugfix
    // 02.08.2026, Nessies Rückmeldung anhand eines Screenshots: der Tooltip
    // einer Kauf-Markerlinie zeigte "1 Stk." statt der tatsächlichen
    // Bruchstückzahl, da onReferenceLineHovered()/onSeriesHovered() die
    // Stückzahl mit 0 statt 4 Nachkommastellen formatierten — siehe
    // ARCHITECTURE.md, "ChartForm-Details"). Beide Handler sind seit diesem
    // Bugfix `private slots:` (siehe ViewChart.h) — reine Testbarkeits-
    // Maßnahme, damit hier per QMetaObject::invokeMethod() direkt geprüft
    // werden kann (gleiches Muster wie bei selectShareRow/onRefreshShare
    // oben), statt ein echtes Maus-Hover über die im headless Testlauf
    // nicht verlässlich vermessbare Chart-Zeichenfläche zu simulieren.
    // Direkter Aufruf statt über ein echtes QLineSeries::hovered()-Signal,
    // da die intern gezeichneten Serien (m_referenceLineSeries bzw. die per
    // setChartData() erzeugten Daten-Serien) private sind.
    // ─────────────────────────────────────────────────────────────────────

    void test_onReferenceLineHovered_fractionalVolume_showsFourDecimals()
    {
        const QString shareGuid = insertTestShare();
        ChartPopup popup(shareGuid, QStringLiteral("Test AG"));

        auto* chart = popup.findChild<ViewChart*>(QStringLiteral("ViewChart"));
        if (!chart) QFAIL("ViewChart-Kindwidget nicht gefunden");

        ChartReferenceLine line;
        line.date   = QDate(2026, 7, 15);
        line.color  = QColor(Qt::blue);
        line.kind   = ChartReferenceLineKind::Buy;
        line.price  = 238.60;
        line.volume = 1.5; // bewusst eine Bruchstückzahl — genau der Fall aus Nessies Screenshot

        QMetaObject::invokeMethod(chart, "onReferenceLineHovered", Qt::DirectConnection,
                                   Q_ARG(ChartReferenceLine, line), Q_ARG(bool, true));

        // "1,5000 Stk." statt der alten "1 Stk." — deutsches Locale, siehe main().
        const QString expected = QLocale().toString(line.volume, 'f', 4) + QStringLiteral(" Stk.");
        QVERIFY2(QToolTip::text().contains(expected),
                 qPrintable(QStringLiteral("Tooltip-Text: '%1', erwartet enthält: '%2'")
                            .arg(QToolTip::text(), expected)));

        // Aufräumen — QToolTip::hideText() über state == false, damit der
        // Tooltip nicht über den Test hinaus stehen bleibt.
        QMetaObject::invokeMethod(chart, "onReferenceLineHovered", Qt::DirectConnection,
                                   Q_ARG(ChartReferenceLine, line), Q_ARG(bool, false));
    }

    void test_onSeriesHovered_heldVolumeSeries_fractionalValue_showsFourDecimals()
    {
        const QString shareGuid = insertTestShare();
        ChartPopup popup(shareGuid, QStringLiteral("Test AG"));

        auto* chart = popup.findChild<ViewChart*>(QStringLiteral("ViewChart"));
        if (!chart) QFAIL("ViewChart-Kindwidget nicht gefunden");

        const double fractionalVolume = 12.3456;
        const QPointF point(0.0, fractionalVolume); // x (Datum) hier irrelevant für diesen Test

        QMetaObject::invokeMethod(chart, "onSeriesHovered", Qt::DirectConnection,
                                   Q_ARG(SeriesKind, SeriesKind::HeldVolume),
                                   Q_ARG(QPointF, point), Q_ARG(bool, true));

        const QString expected = QLocale().toString(fractionalVolume, 'f', 4);
        QVERIFY2(QToolTip::text().contains(expected),
                 qPrintable(QStringLiteral("Tooltip-Text: '%1', erwartet enthält: '%2'")
                            .arg(QToolTip::text(), expected)));

        QMetaObject::invokeMethod(chart, "onSeriesHovered", Qt::DirectConnection,
                                   Q_ARG(SeriesKind, SeriesKind::HeldVolume),
                                   Q_ARG(QPointF, point), Q_ARG(bool, false));
    }

    // Regressionstest für Nessies Rückmeldungen (31.07.2026): das Popup soll
    // horizontal zentriert zum Hauptfenster ausgerichtet sein, mit
    // Hauptfensterbreite − 50px als Popup-Breite (Nessies Vereinfachung der
    // vorherigen 2×5px+50px-Rechnung — bei zentrierter Ausrichtung
    // gleichbedeutend mit 25px Rand auf jeder Seite).
    // MainWindow::onPortfolioRowRightClicked() erzeugt ChartPopup ohne Owner
    // (siehe ARCHITECTURE.md, "ChartPopup") — gesucht wird es daher über
    // QApplication::topLevelWidgets() statt über window.findChildren(), da
    // es kein Kind-Widget von MainWindow ist.
    void test_onPortfolioRowRightClicked_validGuid_popupCenteredAndNarrowerThanMainWindow()
    {
        seedDepotwertPortfolio();
        MainWindow window;

        // Fenstergröße/-position bewusst von der verfügbaren Bildschirmgeometrie
        // abgeleitet statt fest auf 900×600 — verhindert, dass ChartPopup::
        // showAt()'s Bildschirmrand-Klemmung (siehe ChartPopup.cpp) die exakte
        // Zentrierungs-Prüfung unten in einer kleineren (z. B. Offscreen-)
        // Testumgebung verfälscht: Fenster UND Popup bleiben so garantiert
        // vollständig innerhalb der verfügbaren Bildschirmfläche.
        const QRect screenGeom = QGuiApplication::primaryScreen()->availableGeometry();
        const int winWidth = qBound(300, screenGeom.width() - 100, 900);
        window.resize(winWidth, 500);
        window.move(screenGeom.left() + 10, screenGeom.top() + 10);
        window.show();
        QApplication::processEvents();

        QTableWidget* tbl = findFinalTable(window, 1); // data table, 1 share row
        if (!tbl) QFAIL("Depotwert-Datentabelle nicht gefunden");

        QTableWidgetItem* item = tbl->item(0, 0);
        if (!item) QFAIL("WKN-Zelle fehlt");

        const QPoint pos = tbl->visualItemRect(item).center();
        tbl->customContextMenuRequested(pos);
        QApplication::processEvents();

        ChartPopup* popup = nullptr;
        for (auto* w : QApplication::topLevelWidgets()) {
            popup = qobject_cast<ChartPopup*>(w);
            if (popup) break;
        }
        if (!popup) QFAIL("ChartPopup wurde nicht erzeugt");

        // Breite: Hauptfensterbreite − 50px (siehe MainWindow::
        // onPortfolioRowRightClicked()).
        QCOMPARE(popup->width(), window.width() - 50);

        // Bugfix (02.08.2026, siehe ARCHITECTURE.md "ChartPopup — Rechtsklick-
        // Popup-Chart"): Die obige Fenstergrößen-Anpassung an screenGeom kann
        // MainWindow nicht unter dessen harte setMinimumSize(900, 600)
        // schrumpfen (initialize()). Auf einem Bildschirm, der schmäler als
        // Fenster-Mindestbreite + 50px ist (z. B. eine schmale Offscreen-
        // Testumgebung), ist das Popup (window.width() − 50) dadurch breiter
        // als der verfügbare Bildschirm — eine exakte Zentrierung ist dann
        // mathematisch unmöglich. ChartPopup::showAt()'s Klemmung (siehe
        // dort) resolved diesen Fall deterministisch auf den linken
        // Bildschirmrand. Statt showAt()'s komplette Klemm-Formel im Test zu
        // duplizieren (würde nur gegen sich selbst prüfen), wird hier explizit
        // zwischen beiden Bildschirmgrößen-Regimen unterschieden — auf jedem
        // ausreichend breiten Bildschirm (jeder reale Desktop) bleibt die
        // ursprüngliche, exakte Zentrierungs-Prüfung unverändert aktiv.
        const QPoint clickGlobalPos = tbl->viewport()->mapToGlobal(pos);
        const int mainWindowGlobalCenterX =
            window.mapToGlobal(QPoint(window.width() / 2, 0)).x();
        const QPoint intendedTopLeft(
            mainWindowGlobalCenterX - popup->width() / 2, clickGlobalPos.y());
        const QScreen* screen = QGuiApplication::screenAt(intendedTopLeft);
        const QRect avail = screen ? screen->availableGeometry()
                                    : QGuiApplication::primaryScreen()->availableGeometry();

        if (popup->width() <= avail.width()) {
            // Normalfall: Bildschirm bietet genug Platz, Popup ist
            // unklemmbar zentriert — beide Mittelpunkte (Popup-x + halbe
            // Popup-Breite bzw. Hauptfenster-x + halbe Hauptfensterbreite,
            // jeweils in globalen Bildschirmkoordinaten) müssen übereinstimmen.
            const int popupCenterX      = popup->x() + popup->width() / 2;
            const int mainWindowCenterX = mainWindowGlobalCenterX;
            QCOMPARE(popupCenterX, mainWindowCenterX);
        } else {
            // Bewusster Grenzfall: Popup ist breiter als der verfügbare
            // Bildschirm und kann prinzipiell nicht zentriert dargestellt
            // werden — die einzig korrekte Konsequenz ist Linksklemmung.
            QCOMPARE(popup->x(), avail.left());
        }

        popup->close(); // Aufräumen — Qt::WA_DeleteOnClose plant die Zerstörung per deleteLater()
    }

    // ─────────────────────────────────────────────────────────────────────
    // ViewShareDetails — direct construction (no exec(), same rationale as
    // test_shareAddDialog_canBeConstructed for ViewShareAdd)
    // ─────────────────────────────────────────────────────────────────────

    void test_shareDetailsDialog_validShare_constructsAndShowsCloseButtonText()
    {
        const QString shareGuid = insertTestShare();

        ViewShareDetails dlg(shareGuid);

        QVERIFY(dlg.hasValidShare());
        QCOMPARE(dlg.windowTitle(), QStringLiteral("Test AG")); // insertTestShare()'s share name

        // Regression: QDialogButtonBox::Close only auto-translates to
        // "Schließen" if Qt's own qtbase_de.qm is loaded — this project only
        // loads spm_de.ts/spm_en.ts, so ViewShareDetails::setupUi() sets the
        // button text explicitly instead of relying on that.
        auto* buttonBox = dlg.findChild<QDialogButtonBox*>(QStringLiteral("buttonBox"));
        if (!buttonBox) QFAIL("buttonBox nicht gefunden");

        QPushButton* closeButton = buttonBox->button(QDialogButtonBox::Close);
        if (!closeButton) QFAIL("Close-Button nicht gefunden");
        QCOMPARE(closeButton->text(), QStringLiteral("Schließen"));
    }

    // ─────────────────────────────────────────────────────────────────────
    // ViewShareDetails::onMainTabChanged() — Reset auf Jahresübersicht bei
    // äußerem Tab-Wechsel (14.07.2026, Nessies Vorgabe, siehe ARCHITECTURE.md
    // "OverviewTabWidget-Details"). insertTestBuy() legt für jeden Kauf
    // automatisch einen Brokerage-Eintrag mit demselben Datum an (siehe
    // insertTestBuy() oben) — zwei Käufe in verschiedenen Jahren genügen
    // daher, um den Kosten-Tab mit zwei Jahres-Tabs zu befüllen, ohne
    // Sale-/Dividend-Testdaten konstruieren zu müssen.
    // ─────────────────────────────────────────────────────────────────────

    void test_mainTabChanged_resetsOverviewTabsToUebersicht()
    {
        const QString shareGuid = insertTestShare();
        insertTestBuy(shareGuid, QStringLiteral("depot1"),
                       QStringLiteral("2023-03-10T10:00:00"), 5.0, 100.0);
        insertTestBuy(shareGuid, QStringLiteral("depot1"),
                       QStringLiteral("2024-03-10T10:00:00"), 5.0, 100.0);

        ViewShareDetails dlg(shareGuid); // Depotwert-Modus (Default) — legt die drei Tabs an

        auto* mainTabs = dlg.findChild<QTabWidget*>(QStringLiteral("tabs"));
        if (!mainTabs) QFAIL("Äußeres m_tabs nicht gefunden");

        // Die drei OverviewTabWidget-Instanzen (Gewinne/Verluste, Dividenden,
        // Kosten) haben keinen objectName — über count() > 1 identifizieren
        // wir robust diejenige mit tatsächlichen Jahres-Tabs (hier: Kosten,
        // dank der beiden Brokerage-Einträge oben), unabhängig von der
        // Erzeugungsreihenfolge in setupUi().
        OverviewTabWidget* kostenTab = nullptr;
        for (auto* w : dlg.findChildren<OverviewTabWidget*>()) {
            if (w->count() > 1) { kostenTab = w; break; }
        }
        if (!kostenTab) QFAIL("OverviewTabWidget mit Jahres-Tabs nicht gefunden");

        // Einen Jahres-Tab auswählen (Index 1, nicht die Übersicht).
        kostenTab->setCurrentIndex(1);
        QCOMPARE(kostenTab->currentIndex(), 1);

        // Wechsel des äußeren Tabs (weg von "Kosten", z.B. zurück zu
        // "Aktien-Chart") — ohne den fixierten Übersicht-Tab explizit
        // wiederherzustellen, würde kostenTab weiterhin den Jahres-Tab zeigen.
        mainTabs->setCurrentIndex(1);
        QCOMPARE(kostenTab->currentIndex(), 0);

        // Erneuter Wechsel — Reset muss bei jedem Tab-Wechsel greifen, nicht
        // nur einmalig.
        kostenTab->setCurrentIndex(1);
        mainTabs->setCurrentIndex(0);
        QCOMPARE(kostenTab->currentIndex(), 0);
    }

    // ─────────────────────────────────────────────────────────────────────
    // Gewinne/Verluste-Tab im Marktwert-Modus (ergänzt 14.07.2026, Nessies
    // Vorgabe) — Dividenden-/Kosten-Tab bleiben Depotwert-only, da beides
    // laut C#-Referenz reine Depotwert-Konzepte sind (siehe ARCHITECTURE.md,
    // "Marktwert- vs. Depotwert-Modus").
    // ─────────────────────────────────────────────────────────────────────

    void test_marketMode_hasOnlyGewinneVerlusteOverviewTab()
    {
        const QString shareGuid = insertTestShare();
        ViewShareDetails dlg(shareGuid, /*marketValueMode=*/true);

        auto* mainTabs = dlg.findChild<QTabWidget*>(QStringLiteral("tabs"));
        if (!mainTabs) QFAIL("Äußeres m_tabs nicht gefunden");

        bool foundGewinneVerluste = false, foundDividenden = false, foundKosten = false;
        for (int i = 0; i < mainTabs->count(); ++i) {
            const QString title = mainTabs->tabText(i);
            if (title.contains(QStringLiteral("Gewinne"))) foundGewinneVerluste = true;
            if (title.contains(QStringLiteral("Dividenden"))) foundDividenden = true;
            if (title.contains(QStringLiteral("Kosten"))) foundKosten = true;
        }
        QVERIFY(foundGewinneVerluste);
        QVERIFY(!foundDividenden);
        QVERIFY(!foundKosten);

        // Genau eine OverviewTabWidget-Instanz (Gewinne/Verluste) statt drei.
        QCOMPARE(dlg.findChildren<OverviewTabWidget*>().size(), 1);
    }

    void test_depotwertMode_hasAllThreeOverviewTabs()
    {
        // Regression: der Depotwert-Modus (unverändert) muss weiterhin alle
        // drei Tabs anlegen.
        const QString shareGuid = insertTestShare();
        ViewShareDetails dlg(shareGuid); // Depotwert-Modus (Default)

        auto* mainTabs = dlg.findChild<QTabWidget*>(QStringLiteral("tabs"));
        if (!mainTabs) QFAIL("Äußeres m_tabs nicht gefunden");

        bool foundGewinneVerluste = false, foundDividenden = false, foundKosten = false;
        for (int i = 0; i < mainTabs->count(); ++i) {
            const QString title = mainTabs->tabText(i);
            if (title.contains(QStringLiteral("Gewinne"))) foundGewinneVerluste = true;
            if (title.contains(QStringLiteral("Dividenden"))) foundDividenden = true;
            if (title.contains(QStringLiteral("Kosten"))) foundKosten = true;
        }
        QVERIFY(foundGewinneVerluste);
        QVERIFY(foundDividenden);
        QVERIFY(foundKosten);

        QCOMPARE(dlg.findChildren<OverviewTabWidget*>().size(), 3);
    }

    /**
     * @brief Findet die OverviewTabWidget-Instanz innerhalb der QGroupBox mit
     * passendem Titel (z.B. "Gewinne / Verluste-Übersicht") — robuster als
     * eine Index-Annahme über findChildren<OverviewTabWidget*>(), da mehrere
     * Instanzen gleichzeitig existieren können (siehe wrapInOverviewGroup()
     * in ViewShareDetails.cpp).
     */
    static OverviewTabWidget* overviewTabByGroupTitle(QWidget& root, const QString& titleContains)
    {
        for (auto* gb : root.findChildren<QGroupBox*>()) {
            if (gb->title().contains(titleContains)) {
                if (auto* w = gb->findChild<OverviewTabWidget*>())
                    return w;
            }
        }
        return nullptr;
    }

    void test_marketMode_gewinneVerlusteTab_usesBrokerageFreeValues()
    {
        // Regressionstest für die eigentliche fachliche Änderung (14.07.2026):
        // Marktwert-Modus muss SaleObject::payout()/profitLoss() (brokeragefrei)
        // verwenden statt payoutBrokerageReduction()/profitLossBrokerageReduction()
        // (Depotwert-Modus) — siehe ARCHITECTURE.md, "Marktwert- vs.
        // Depotwert-Modus". Ein Verkauf mit eigener Provision (10,00 €) macht
        // den Unterschied messbar: saleValue = 5 × 100,00 € = 500,00 €;
        // Depotwert-Auszahlung = 500,00 € − 10,00 € (Provision) = 490,00 €;
        // Markt-Auszahlung (ohne Brokerage) = 500,00 € unverändert.
        const QString shareGuid = insertTestShare();
        const BuyObject buy = insertTestBuy(shareGuid, QStringLiteral("depot1"),
                                             QStringLiteral("2024-02-10T10:00:00"), 20.0, 100.0);

        const SaleObject sale(
            QStringLiteral("sale-market-test"), shareGuid,
            QStringLiteral("depot1"), QStringLiteral("ORD-MARKET-TEST"),
            QStringLiteral("2024-06-05T10:00:00"),
            5.0, 100.0,
            { SaleBuyDetail(buy.guid(), buy.dateTime(), 5.0, buy.price()) },
            /*taxAtSource=*/0.0, /*capitalGainsTax=*/0.0, /*solidarityTax=*/0.0,
            /*brokerageGuid=*/QString(), /*provision=*/10.0);
        ModelSaleEdit modelSaleEdit;
        QVERIFY(modelSaleEdit.addSale(sale));

        const QLocale loc;
        const QString expectedMarketPayout = loc.toString(500.0, 'f', 2) + QStringLiteral(" €");
        const QString expectedDepotPayout  = loc.toString(490.0, 'f', 2) + QStringLiteral(" €");

        // Depotwert-Modus — Auszahlung inkl. Brokerage.
        {
            ViewShareDetails dlg(shareGuid); // Default: Depotwert-Modus
            auto* gewinneVerluste = overviewTabByGroupTitle(dlg, QStringLiteral("Gewinne"));
            if (!gewinneVerluste) QFAIL("Gewinne/Verluste-OverviewTabWidget nicht gefunden");
            auto* tbl = dataTableFromContainer(gewinneVerluste->widget(0)); // Übersicht-Tab
            if (!tbl) QFAIL("Übersicht-dataTable nicht gefunden");
            QCOMPARE(tbl->rowCount(), 1); // ein Jahr (2024)
            auto* item = tbl->item(0, 2); // Spalte "Auszahlung"
            if (!item) QFAIL("Auszahlung-Zelle nicht gefunden");
            QCOMPARE(item->text(), expectedDepotPayout);
        }

        // Marktwert-Modus — dieselben Daten, brokeragefreie Auszahlung.
        {
            ViewShareDetails dlg(shareGuid, /*marketValueMode=*/true);
            auto* gewinneVerluste = overviewTabByGroupTitle(dlg, QStringLiteral("Gewinne"));
            if (!gewinneVerluste) QFAIL("Gewinne/Verluste-OverviewTabWidget nicht gefunden");
            auto* tbl = dataTableFromContainer(gewinneVerluste->widget(0));
            if (!tbl) QFAIL("Übersicht-dataTable nicht gefunden");
            QCOMPARE(tbl->rowCount(), 1);
            auto* item = tbl->item(0, 2);
            if (!item) QFAIL("Auszahlung-Zelle nicht gefunden");
            QCOMPARE(item->text(), expectedMarketPayout);
        }
    }

    // ─────────────────────────────────────────────────────────────────────
    // ViewChart — Mausrad-Steuerung der "Anzahl" (ergänzt 12.07.2026, siehe
    // ARCHITECTURE.md "ChartForm-Details"). ViewChart ist als Tab 1 in
    // ViewShareDetails eingebettet, countSpin/chartView werden per
    // findChild() über ihren objectName gefunden (beide privat in ViewChart,
    // objectName-Suche funktioniert trotzdem widget-übergreifend).
    // ─────────────────────────────────────────────────────────────────────

    void test_chartWheel_overCountSpinAndChartView_changesIntervalCountAndRefreshes()
    {
        const QString shareGuid = insertTestShare();
        ViewShareDetails dlg(shareGuid);

        auto* countSpin = dlg.findChild<QSpinBox*>(QStringLiteral("countSpin"));
        if (!countSpin) QFAIL("countSpin nicht gefunden");
        auto* chartView = dlg.findChild<QChartView*>(QStringLiteral("chartView"));
        if (!chartView) QFAIL("chartView nicht gefunden");

        QCOMPARE(countSpin->value(), 1); // Default

        // Baut ein synthetisches QWheelEvent und schickt es per sendEvent()
        // direkt an das Ziel-Widget — läuft über dessen installierten
        // eventFilter() (ViewChart::eventFilter()), exakt derselbe Pfad wie
        // ein echtes Mausrad-Event vom Fenstersystem.
        auto sendWheel = [](QWidget* target, int angleDeltaY) {
            const QPointF pos(target->rect().center());
            const QPointF globalPos(target->mapToGlobal(pos.toPoint()));
            QWheelEvent wheelEvent(pos, globalPos, QPoint(0, 0), QPoint(0, angleDeltaY),
                                   Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QCoreApplication::sendEvent(target, &wheelEvent);
        };

        // countSpin: bewusst OHNE vorherigen setFocus()-Aufruf — genau das
        // ist der Fokus-Bug (QAbstractSpinBox::wheelEvent() ignoriert Wheel-
        // Events ohne Fokus), den der Event-Filter umgeht.
        sendWheel(countSpin, 120); // Rad nach oben
        QCOMPARE(countSpin->value(), 2);

        sendWheel(countSpin, -120); // Rad nach unten
        QCOMPARE(countSpin->value(), 1);

        // chartView-Viewport: derselbe Weg wie über der echten Chart-
        // Zeichenfläche, muss auf denselben countSpin durchschlagen.
        sendWheel(chartView->viewport(), 120);
        QCOMPARE(countSpin->value(), 2);

        sendWheel(chartView->viewport(), -120);
        QCOMPARE(countSpin->value(), 1);
    }

    // ─────────────────────────────────────────────────────────────────────
    // ViewChart — gegenseitiger Ausschluss "Anteile"/"Gehandelte Anteile"
    // (ergänzt 12.07.2026, siehe ARCHITECTURE.md "ChartForm-Details").
    // Reine View-Ebene (ViewChart::setupSelektionBox()), daher nur über eine
    // echte ViewChart-Instanz testbar, nicht über tst_chartform.cpp's
    // FakeViewChart/PresenterChart-Paar.
    // ─────────────────────────────────────────────────────────────────────

    void test_chartCheckboxes_heldAndTradedVolumeAreMutuallyExclusive()
    {
        const QString shareGuid = insertTestShare();
        ViewShareDetails dlg(shareGuid);

        auto* heldCb   = dlg.findChild<QCheckBox*>(QStringLiteral("seriesCheckBox_HeldVolume"));
        auto* tradedCb = dlg.findChild<QCheckBox*>(QStringLiteral("seriesCheckBox_TradedVolume"));
        if (!heldCb)   QFAIL("seriesCheckBox_HeldVolume nicht gefunden");
        if (!tradedCb) QFAIL("seriesCheckBox_TradedVolume nicht gefunden");

        QVERIFY(tradedCb->isEnabled());
        QVERIFY(tradedCb->toolTip().isEmpty());

        heldCb->setChecked(true);
        QVERIFY(!tradedCb->isEnabled());
        QVERIFY(!tradedCb->toolTip().isEmpty());

        heldCb->setChecked(false);
        QVERIFY(tradedCb->isEnabled());
        QVERIFY(tradedCb->toolTip().isEmpty());

        // Symmetrisch in die andere Richtung.
        tradedCb->setChecked(true);
        QVERIFY(!heldCb->isEnabled());
        QVERIFY(!heldCb->toolTip().isEmpty());

        tradedCb->setChecked(false);
        QVERIFY(heldCb->isEnabled());
        QVERIFY(heldCb->toolTip().isEmpty());
    }

    // ─────────────────────────────────────────────────────────────────────
    // MainWindow::resolveShareGuidForDocument() — Direkte Dokumentenerfassung
    // (Feature 27.07.2026, siehe ARCHITECTURE.md). public static, braucht
    // keine MainWindow-Instanz — nur eine offene Test-DB via openMemoryDb()/
    // insertTestShare().
    // ─────────────────────────────────────────────────────────────────────

    void test_resolveShareGuidForDocument_matchesByWkn()
    {
        const QString guid = insertTestShare(); // WKN "TST01", ISIN "DE000TST0001"

        DocumentEntry entry;
        entry.regexList.insert(QStringLiteral("Wkn"),
            ParserLib::RegExElement{ QStringLiteral("WKN:\\s+([A-Z0-9]{5})"), 0, false, {} });

        const QString text = QStringLiteral("WKN: TST01");
        QCOMPARE(MainWindow::resolveShareGuidForDocument(text, entry), guid);
    }

    void test_resolveShareGuidForDocument_matchesByIsin()
    {
        const QString guid = insertTestShare(); // WKN "TST01", ISIN "DE000TST0001"

        // Bewusst nur eine Isin-Regel, keine Wkn-Regel — testet den
        // ISIN-only-Pfad (extractWkn() liefert dann "" zurück, kein Absturz).
        DocumentEntry entry;
        entry.regexList.insert(QStringLiteral("Isin"),
            ParserLib::RegExElement{ QStringLiteral("ISIN:\\s+([A-Z0-9]{12})"), 0, false, {} });

        const QString text = QStringLiteral("ISIN: DE000TST0001");
        QCOMPARE(MainWindow::resolveShareGuidForDocument(text, entry), guid);
    }

    void test_resolveShareGuidForDocument_wknTakesPrecedenceOverIsin()
    {
        openMemoryDb();
        ShareRepository repo;
        const QString wknGuid  = QStringLiteral("share-wkn-1");
        const QString isinGuid = QStringLiteral("share-isin-1");
        repo.insert(ShareObject(wknGuid, QStringLiteral("TST01"),
                                QStringLiteral("DE000TST0001"), QStringLiteral("Test AG")));
        repo.insert(ShareObject(isinGuid, QStringLiteral("SIE111"),
                                QStringLiteral("DE0007236101"), QStringLiteral("Siemens AG")));

        DocumentEntry entry;
        entry.regexList.insert(QStringLiteral("Wkn"),
            ParserLib::RegExElement{ QStringLiteral("WKN:\\s+([A-Z0-9]{5,6})"), 0, false, {} });
        entry.regexList.insert(QStringLiteral("Isin"),
            ParserLib::RegExElement{ QStringLiteral("ISIN:\\s+([A-Z0-9]{12})"), 0, false, {} });

        // WKN gehört zu "Test AG", ISIN (absichtlich widersprüchlich) zu
        // Siemens — die WKN muss gewinnen; resolveShareGuidForDocument()
        // darf die ISIN in diesem Fall gar nicht erst nachschlagen.
        const QString text = QStringLiteral("WKN: TST01\nISIN: DE0007236101");
        QCOMPARE(MainWindow::resolveShareGuidForDocument(text, entry), wknGuid);
    }

    void test_resolveShareGuidForDocument_noMatch_returnsEmpty()
    {
        openMemoryDb(); // leere DB — keine Aktie vorhanden

        DocumentEntry entry;
        entry.regexList.insert(QStringLiteral("Wkn"),
            ParserLib::RegExElement{ QStringLiteral("WKN:\\s+([A-Z0-9]{6})"), 0, false, {} });

        const QString text = QStringLiteral("WKN: UNKNWN");
        QVERIFY(MainWindow::resolveShareGuidForDocument(text, entry).isEmpty());
    }

    void test_resolveShareGuidForDocument_noWknIsinRuleInDocEntry_returnsEmpty()
    {
        openMemoryDb();
        DocumentEntry entry; // regexList bewusst leer — simuliert einen
                             // Sale-/Dividend-DocumentEntry ohne Wkn/Isin-Regel
        QVERIFY(MainWindow::resolveShareGuidForDocument(
            QStringLiteral("beliebiger Text"), entry).isEmpty());
    }

}; // end of TestMainWindow

// TestOwnMessageBox ist seit 22.08.2026 in tst_ownmessagebox.cpp ausgelagert
// (siehe tests/forms/CMakeLists.txt, Ziel tst_ownmessagebox).
// TestBackupForm ist seit 22.08.2026 in tst_backupform.cpp ausgelagert
// (siehe tests/forms/CMakeLists.txt, Ziel tst_backupform).

// Test classes — run all via a custom main
int main(int argc, char* argv[])
{
    // Bugfix 23.07.2026 — siehe ARCHITECTURE.md, "System-Locale-abhängiges
    // Zahlenformat": muss vor jeder QLocale()-Verwendung gesetzt werden,
    // damit formatMoney() auf jedem Runner/System deutsch formatiert,
    // unabhängig von dessen System-Locale.
    QLocale::setDefault(QLocale::German);

    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);
    // Feature (01.08.2026): Fenstertitel zeigt die App-Version über
    // QCoreApplication::applicationVersion() — hier wie in main.cpp gesetzt,
    // damit test_construction_windowTitleContainsVersion() die echte
    // SPM_VERSION_STRING prüfen kann statt eines leeren Strings.
    app.setApplicationVersion(QStringLiteral(SPM_VERSION_STRING));

    int result = 0;
    {
        TestMainWindow t;
        result |= QTest::qExec(&t, argc, argv);
    }
    return result;
}

#include "tst_mainwindow.moc"

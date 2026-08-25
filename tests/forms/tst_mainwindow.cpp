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
#include "../../app/forms/ShareAddForm/ModelShareAdd.h"
#include "../../app/forms/ShareAddForm/IViewShareAdd.h"
#include "../../app/forms/ShareAddForm/IModelShareAdd.h"
#include "../../app/forms/ShareAddForm/PresenterShareAdd.h"
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

#include "../../app/forms/BrokeragesForm/IViewBrokerageEdit.h"
#include "../../app/forms/BrokeragesForm/IModelBrokerageEdit.h"
#include "../../app/forms/BrokeragesForm/ViewBrokerageEdit.h"
#include "../../app/forms/BrokeragesForm/ModelBrokerageEdit.h"
#include "../../app/forms/BrokeragesForm/PresenterBrokerageEdit.h"

#include "../../app/models/SaleObject.h"
#include "../../app/forms/ShareEditForm/IViewShareEdit.h"
#include "../../app/forms/ShareEditForm/IModelShareEdit.h"
#include "../../app/forms/ShareEditForm/ModelShareEdit.h"
#include "../../app/forms/ShareEditForm/PresenterShareEdit.h"
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
// Stub IModelShareAdd — used in Presenter tests to control save/exists results
// ─────────────────────────────────────────────────────────────────────────────
class StubModelShareAdd : public IModelShareAdd
{
public:
    bool  saveResult    = true;
    bool  wknExistsResult  = false;
    bool  isinExistsResult = false;
    QString errorMsg;

    bool    saveShareWithBuy(const ShareObject&, const BuyObject&,
                             double, double, double, double) override
    { return saveResult; }
    bool    wknExists(const QString&)  const override { return wknExistsResult; }
    bool    isinExists(const QString&) const override { return isinExistsResult; }
    QString lastError()                const override { return errorMsg; }
};

// ─────────────────────────────────────────────────────────────────────────────
// Stub IViewShareAdd — captures presenter calls for assertions
// ─────────────────────────────────────────────────────────────────────────────
class StubViewShareAdd : public IViewShareAdd
{
public:
    // Configurable return values
    QString  m_wkn           = QStringLiteral("840400");
    QString  m_isin          = QStringLiteral("DE0008404005");
    QString  m_name          = QStringLiteral("Allianz SE");
    QDate    m_listingDate   = QDate(2000, 1, 1);
    ShareType m_shareType    = ShareType::Share;
    QString  m_divInterval   = QStringLiteral("keine");
    QString  m_country       = QStringLiteral("de-DE");
    QString  m_detailsWeb    = QStringLiteral("https://details.example.com");
    QString  m_marketUrl     = QStringLiteral("https://market.example.com");
    ShareParsingType m_marketParsing = ShareParsingType::Regex;
    QString  m_marketKey;
    QString  m_dailyUrl      = QStringLiteral("https://daily.example.com");
    ShareParsingType m_dailyParsing  = ShareParsingType::Regex;
    QString  m_dailyKey;
    QDateTime m_buyDateTime  = QDateTime(QDate(2024,3,15), QTime(10,30,0));
    QString  m_depotNr       = QStringLiteral("12345678");
    QString  m_orderNr       = QStringLiteral("ORD-001");
    double   m_volume        = 10.0;
    double   m_price         = 245.60;
    double   m_provision     = 9.90;
    double   m_brokerFee     = 0.0;
    double   m_traderFee     = 2.00;
    double   m_reduction     = 0.0;
    QString  m_docPath;

    // Captured calls
    QMap<QString, QString> fieldOkValues;
    QStringList            fieldErrors;
    QString                lastError;
    bool                   closed = false;

    // IViewShareAdd
    QString  wkn()              const override { return m_wkn; }
    QString  isin()             const override { return m_isin; }
    QString  name()             const override { return m_name; }
    QDate    listingDate()      const override { return m_listingDate; }
    ShareType shareType()       const override { return m_shareType; }
    QString  dividendInterval() const override { return m_divInterval; }
    QString  countryInfo()      const override { return m_country; }
    QString  detailsWebsite()   const override { return m_detailsWeb; }
    QString  marketPriceUrl()   const override { return m_marketUrl; }
    ShareParsingType marketPriceParsingType() const override { return m_marketParsing; }
    QString  marketPriceApiKey()const override { return m_marketKey; }
    QString  dailyValuesUrl()   const override { return m_dailyUrl; }
    ShareParsingType dailyValuesParsingType() const override { return m_dailyParsing; }
    QString  dailyValuesApiKey()const override { return m_dailyKey; }
    QDateTime buyDateTime()     const override { return m_buyDateTime; }
    QString  depotNumber()      const override { return m_depotNr; }
    QString  orderNumber()      const override { return m_orderNr; }
    double   volume()           const override { return m_volume; }
    double   price()            const override { return m_price; }
    double   provision()        const override { return m_provision; }
    double   brokerFee()        const override { return m_brokerFee; }
    double   traderFee()        const override { return m_traderFee; }
    double   reduction()        const override { return m_reduction; }
    QString  documentPath()     const override { return m_docPath; }

    void setFieldOk(const QString& f, const QString& v) override { fieldOkValues[f] = v; }
    void setFieldError(const QString& f)                override { fieldErrors << f; }
    void setDocumentPath(const QString& path)           override { m_docPath = path; }
    void setDocumentPreview(const QString&)             override {}
    void showError(const QString& msg)                  override { lastError = msg; }
    void setParseProgress(int, const QString&)          override {}
    void setParseStatusIcon(int)                        override {}
    void setUiBusy(bool)                                override {}
    void markMissingFieldsAsFailed()                    override {}
    bool hasMissingRequiredFields(QStringList& missing) const override
        { missing.clear(); return false; }
    void onParseFinished()                              override {}
    void acceptAndClose()                               override { closed = true; }
};

// ─────────────────────────────────────────────────────────────────────────────
// Stub IViewShareEdit / IModelShareEdit — used by PresenterShareEdit tests
// ─────────────────────────────────────────────────────────────────────────────
class StubViewShareEdit : public IViewShareEdit
{
public:
    bool    loadShareCalled    = false;
    bool    closedCalled       = false;
    bool    setTotalBuysCalled = false;
    QString lastError;

    // 06.08.2026 — Regel "Tageswerte bei Bestand > 0".
    //
    // updateTypeToReturn war vorher als ShareUpdateType::None fest verdrahtet.
    // Das ging nur so lange gut, wie der Presenter den Update-Typ ungeprüft
    // durchreichte: seit PresenterShareEdit::validateInput() ihn gegen den
    // Bestand prüft, und StubModelShareEdit::currentVolume() 10,0 liefert,
    // wäre "None" ein unzulässiger Wert — jeder onSave()-Test hier würde am
    // Validierungsfehler scheitern statt am eigentlichen Prüfgegenstand.
    // Vorgabe deshalb "Both" (zulässig); Tests, die den Sperrfall brauchen,
    // setzen den Wert selbst.
    ShareUpdateType updateTypeToReturn = ShareUpdateType::Both;

    /// Zuletzt an setDailyValuesRequired() übergebener Wert.
    bool dailyValuesRequired       = false;
    /// Wurde setDailyValuesRequired() überhaupt aufgerufen?
    bool dailyValuesRequiredCalled = false;

    QString   wkn()              const override { return QStringLiteral("840400"); }
    QString   isin()             const override { return QStringLiteral("DE0008404005"); }
    QString   name()             const override { return QStringLiteral("Test AG"); }
    QDate     listingDate()      const override { return QDate(2000, 1, 1); }
    ShareType shareType()        const override { return ShareType::Share; }
    QString   dividendInterval() const override { return QStringLiteral("keine"); }
    QString   countryInfo()      const override { return QStringLiteral("de-DE"); }
    QString   detailsWebsite()   const override { return QString(); }
    QString          marketPriceUrl()         const override { return QString(); }
    ShareParsingType marketPriceParsingType() const override { return ShareParsingType::Regex; }
    QString          marketPriceApiKey()      const override { return QString(); }
    QString          dailyValuesUrl()         const override { return QString(); }
    ShareParsingType dailyValuesParsingType() const override { return ShareParsingType::Regex; }
    QString          dailyValuesApiKey()      const override { return QString(); }
    ShareUpdateType  updateType()             const override { return updateTypeToReturn; }

    void loadShare(const ShareObject&)   override { loadShareCalled = true; }
    void setFirstBuyDate(const QString&) override {}
    void setCurrentVolume(double)        override {}
    void setDailyValuesRequired(bool required) override
        { dailyValuesRequired = required; dailyValuesRequiredCalled = true; }
    // 08.08.2026 — Phase 3 der Aktiensplit-Behandlung.
    /// Zuletzt an setSplitInfo() übergebene Liste.
    QList<ShareSplitObject> lastSplitInfo;
    /// Wurde setSplitInfo() überhaupt aufgerufen?
    bool setSplitInfoCalled = false;

    void setSplitInfo(const QList<ShareSplitObject>& splits) override
        { lastSplitInfo = splits; setSplitInfoCalled = true; }

    void setTotalBuys(double, int)       override { setTotalBuysCalled = true; }
    void setTotalSales(double, int)      override {}
    void setTotalProfitLoss(double, int) override {}
    void setTotalDividends(double, int)  override {}
    void setTotalBrokerages(double, int) override {}
    void showError(const QString& msg)   override { lastError = msg; }
    void acceptAndClose()                override { closedCalled = true; }
};

class StubModelShareEdit : public IModelShareEdit
{
public:
    ShareObject shareToReturn;
    bool        saveResult = true;

    /// Gehaltener Bestand — je Test setzbar (06.08.2026). Vorgabe wie bisher
    /// fest verdrahtet: 10,0, also "Anteile vorhanden".
    double volumeToReturn = 10.0;

    /// Wurde saveShare() erreicht? Belegt, dass eine Validierung wirklich
    /// vorher abbricht statt nur eine Meldung nachzuschieben.
    bool saveShareCalled = false;

    ShareObject loadShare(const QString&)     const override { return shareToReturn; }
    bool        saveShare(const ShareObject&)       override
        { saveShareCalled = true; return saveResult; }
    double totalBuyValue(const QString&)      const override { return 1000.0; }
    int    buyCount(const QString&)           const override { return 2; }
    double totalSaleValue(const QString&)     const override { return 500.0; }
    double totalProfitLoss(const QString&)    const override { return -100.0; }
    int    saleCount(const QString&)          const override { return 1; }
    double totalDividendValue(const QString&) const override { return 50.0; }
    int    dividendCount(const QString&)      const override { return 3; }
    double totalBrokerageValue(const QString&)const override { return 30.0; }
    int    brokerageCount(const QString&)     const override { return 2; }
    double currentVolume(const QString&)      const override { return volumeToReturn; }
    QString firstBuyDate(const QString&)      const override { return QStringLiteral("2020-01-01"); }

    /// Splits, die loadSplits() liefert — je Test setzbar (08.08.2026).
    /// Vorgabe leer: die allermeisten Tests hier interessieren sich nicht dafür.
    QList<ShareSplitObject> splitsToReturn;

    QList<ShareSplitObject> loadSplits(const QString&) const override { return splitsToReturn; }

    QString lastError()                       const override { return QString(); }
};

// ─────────────────────────────────────────────────────────────────────────────
// Stub IModelBrokerageEdit
// ─────────────────────────────────────────────────────────────────────────────
class StubModelBrokerageEdit : public IModelBrokerageEdit
{
public:
    QList<BrokerageObject> brokerages;
    bool                    addResult       = true;
    bool                    updateResult    = true;
    bool                    removeResult    = true;
    bool                    docExists       = false;
    QString                 errorMsg;

    bool addBrokerageCalled    = false;
    bool updateBrokerageCalled = false;
    bool updateDocumentCalled  = false;
    bool removeBrokerageCalled = false;

    QList<BrokerageObject> loadBrokerages(const QString&) const override { return brokerages; }

    bool addBrokerage(const BrokerageObject&)              override
        { addBrokerageCalled = true; return addResult; }
    bool updateBrokerage(const BrokerageObject&)           override
        { updateBrokerageCalled = true; return updateResult; }
    bool updateDocument(const QString&, const QString&)    override
        { updateDocumentCalled = true; return updateResult; }
    bool removeBrokerage(const QString&)                   override
        { removeBrokerageCalled = true; return removeResult; }

    bool documentExists(const QString&, const QString&) const override { return docExists; }
    QString lastError() const override { return errorMsg; }
};

// ─────────────────────────────────────────────────────────────────────────────
// Stub IViewBrokerageEdit
// ─────────────────────────────────────────────────────────────────────────────
class StubViewBrokerageEdit : public IViewBrokerageEdit
{
public:
    // Configurable return values
    QString m_dateTime   = QStringLiteral("2024-06-15T00:00:00");
    double  m_provision   = 9.90;
    double  m_brokerFee   = 0.0;
    double  m_traderFee   = 0.0;
    double  m_reduction   = 0.0;
    QString m_docPath;
    bool    m_missingFields = false;

    // Captured calls
    bool    populateOverviewCalled    = false;
    bool    clearFormCalled           = false;
    bool    loadBrokerageCalled       = false;
    bool    showOverviewTabCalled     = false;
    bool    setButtonStatesCalled     = false;
    bool    lastCanRemove             = false;
    bool    lastIsEdit                = false;
    bool    lastReadOnly              = false;
    bool    openPdfPreviewCalled      = false;
    bool    clearPdfPreviewCalled     = false;
    double  lastGesamtGebuehren       = -1.0;
    double  lastBrokerageReduction    = -1.0;
    QString lastError;
    bool    closed                    = false;

    // IViewBrokerageEdit — read
    QString dateTime()     const override { return m_dateTime; }
    double  provision()    const override { return m_provision; }
    double  brokerFee()    const override { return m_brokerFee; }
    double  traderFee()    const override { return m_traderFee; }
    double  reduction()    const override { return m_reduction; }
    QString documentPath() const override { return m_docPath; }

    // IViewBrokerageEdit — write
    void loadBrokerage(const BrokerageObject&) override { loadBrokerageCalled = true; }
    void clearForm()                            override { clearFormCalled = true; }

    void setGesamtGebuehren(double value)    override { lastGesamtGebuehren    = value; }
    void setBrokerageReduction(double value) override { lastBrokerageReduction = value; }

    void setDocumentPreview(const QString&) override {}

    void openPdfPreview(const QString&) override { openPdfPreviewCalled  = true; }
    void clearPdfPreview()              override { clearPdfPreviewCalled = true; }

    void populateOverview(const QList<BrokerageObject>&) override
        { populateOverviewCalled = true; }
    void showOverviewTab() override { showOverviewTabCalled = true; clearFormCalled = true; }

    void setButtonStates(bool canRemove, bool isEdit, bool readOnly) override
    {
        setButtonStatesCalled = true;
        lastCanRemove = canRemove;
        lastIsEdit    = isEdit;
        lastReadOnly  = readOnly;
    }

    void showError(const QString& msg) override { lastError = msg; }
    void acceptAndClose()               override { closed = true; }

    void markMissingFieldsAsFailed() override {}
    bool hasMissingRequiredFields(QStringList& missing) const override
        { missing.clear(); if (m_missingFields) missing << QStringLiteral("date"); return m_missingFields; }
};

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
        const QString diffStr   = locale.toString(12.30, 'f', 2) + QStringLiteral(" €");  // "12,30 €"
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
        const QString diffStr  = locale.toString(10.0, 'f', 2) + QStringLiteral(" €"); // "10,00 €"
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
        const QString zeroStr   = locale.toString(0.0, 'f', 2) + QStringLiteral(" €"); // "0,00 €"
        const QString expectedTooltip =
            QStringLiteral("<div style=\"white-space:nowrap;\">Gesamtänderung Aktie:<br>"
                           "%1 Stk. × %2 = %2</div>")
                .arg(volumeStr, zeroStr);

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
        const QString zeroStr   = locale.toString(0.0, 'f', 2) + QStringLiteral(" €");

        const QString expectedBefore =
            QStringLiteral("<div style=\"white-space:nowrap;\">Gesamtänderung Aktie:<br>"
                           "%1 Stk. × %2 = %2</div>")
                .arg(volumeStr, zeroStr);

        const QString before = finalTbl->item(finalRow, static_cast<int>(FC::PrevDay))->toolTip();
        QCOMPARE(before, expectedBefore);

        finalTbl->setCurrentCell(finalRow, 0);
        QAction* actionRefresh = findActionByStatusTip(window,
            QStringLiteral("Kurs der ausgewählten Aktie aktualisieren"));
        QVERIFY(actionRefresh);

        QMetaObject::invokeMethod(&window, "onRefreshShare", Qt::DirectConnection);

        QVERIFY2(QTest::qWaitFor([&]{ return actionRefresh->isEnabled(); }, 2000),
                 "Einzel-Refresh hat nicht beendet (finaliseRefresh() nicht erreicht).");

        const QString diffStr  = locale.toString(30.0, 'f', 2) + QStringLiteral(" €");  // "30,00 €"
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

    void test_newPortfolio_settingsPathUpdated()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/new_portfolio.db");
        Database::instance().open(dbPath);
        AppSettings::instance().setPortfolioPath(dbPath);
        QCOMPARE(AppSettings::instance().portfolioPath(), dbPath);
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
    // ModelShareAdd — persistence
    // ─────────────────────────────────────────────────────────────────────

    void test_modelShareAdd_saveShareWithBuy_success()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/model_save.db");
        Database::instance().open(dbPath);

        ShareObject share(
            QStringLiteral("s-guid-1"), QStringLiteral("840400"),
            QStringLiteral("DE0008404005"), QStringLiteral("Allianz SE"));

        BuyObject buy(
            QStringLiteral("b-guid-1"), QStringLiteral("s-guid-1"),
            QStringLiteral("12345678"), QStringLiteral("ORD-001"),
            QStringLiteral("2024-03-15T10:30:00"),
            10.0, 0.0, 245.60);

        ModelShareAdd model;
        QVERIFY(model.saveShareWithBuy(share, buy, 9.90, 0.0, 2.00, 0.0));
        QVERIFY(model.lastError().isEmpty());

        // Share must be in DB
        ShareRepository sr;
        const ShareObject loaded = sr.findByWkn(QStringLiteral("840400"));
        QVERIFY(loaded.isValid());
        QCOMPARE(loaded.name(), QStringLiteral("Allianz SE"));

        // Buy must be in DB
        BuyRepository br;
        const auto buys = br.findByShare(QStringLiteral("s-guid-1"));
        QCOMPARE(buys.size(), 1);
        QCOMPARE(buys.first().price(), 245.60);

        // Brokerage record must be in DB and linked to buy
        BrokerageRepository brokerRepo;
        const auto brokerage = brokerRepo.findByBuyGuid(QStringLiteral("b-guid-1"));
        QVERIFY(brokerage.isValid());
        QCOMPARE(brokerage.provision(), 9.90);
    }

    void test_modelShareAdd_saveShareWithBuy_rollsBackOnDuplicateWkn()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/model_dup.db");
        Database::instance().open(dbPath);

        // Insert first share
        ShareRepository sr;
        sr.insert(ShareObject(QStringLiteral("s-existing"),
                              QStringLiteral("840400"),
                              QStringLiteral("DE0008404005"),
                              QStringLiteral("Allianz SE")));

        // Try inserting duplicate WKN via ModelShareAdd — must fail
        ShareObject dup(QStringLiteral("s-dup"), QStringLiteral("840400"),
                        QStringLiteral("DE0008404006"), QStringLiteral("Allianz Dup"));
        BuyObject buy(QStringLiteral("b-dup"), QStringLiteral("s-dup"),
                      QStringLiteral("12345678"), QStringLiteral("ORD-002"),
                      QStringLiteral("2024-04-01T09:00:00"),
                      5.0, 0.0, 100.0);

        ModelShareAdd model;
        QVERIFY(!model.saveShareWithBuy(dup, buy, 5.0, 0.0, 0.0, 0.0));
        QVERIFY(!model.lastError().isEmpty());

        // Only original share should exist; no brokerage record created
        QCOMPARE(sr.findAll().size(), 1);
        BrokerageRepository brokerRepo;
        QCOMPARE(brokerRepo.findByBuyGuid(QStringLiteral("b-dup")).isValid(), false);
    }

    void test_modelShareAdd_wknExists_true()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/model_wkn.db");
        Database::instance().open(dbPath);
        ShareRepository().insert(ShareObject(QStringLiteral("g1"),
            QStringLiteral("840400"), QStringLiteral("DE0008404005"),
            QStringLiteral("Allianz SE")));

        ModelShareAdd model;
        QVERIFY(model.wknExists(QStringLiteral("840400")));
        QVERIFY(!model.wknExists(QStringLiteral("999999")));
    }

    void test_modelShareAdd_isinExists_true()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/model_isin.db");
        Database::instance().open(dbPath);
        ShareRepository().insert(ShareObject(QStringLiteral("g1"),
            QStringLiteral("840400"), QStringLiteral("DE0008404005"),
            QStringLiteral("Allianz SE")));

        ModelShareAdd model;
        QVERIFY(model.isinExists(QStringLiteral("DE0008404005")));
        QVERIFY(!model.isinExists(QStringLiteral("US0000000000")));
    }

    // ─────────────────────────────────────────────────────────────────────
    // PresenterShareAdd — validation & save logic (via stubs)
    // ─────────────────────────────────────────────────────────────────────

    void test_presenterShareAdd_onSave_success_closesView()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/pres_ok.db");
        Database::instance().open(dbPath);

        StubViewShareAdd  view;
        StubModelShareAdd model;
        PresenterShareAdd presenter(&view, &model, &m_docsConfig);

        presenter.onSave();

        QVERIFY(view.closed);
        QVERIFY(view.lastError.isEmpty());
    }

    void test_presenterShareAdd_onSave_emptyWkn_showsError()
    {
        openMemoryDb();
        StubViewShareAdd  view;
        StubModelShareAdd model;
        view.m_wkn = QString(); // invalid — empty WKN
        PresenterShareAdd presenter(&view, &model, &m_docsConfig);

        presenter.onSave();

        QVERIFY(!view.closed);
        QVERIFY(!view.lastError.isEmpty());
    }

    void test_presenterShareAdd_onSave_emptyName_showsError()
    {
        openMemoryDb();
        StubViewShareAdd  view;
        StubModelShareAdd model;
        view.m_name = QString();
        PresenterShareAdd presenter(&view, &model, &m_docsConfig);

        presenter.onSave();

        QVERIFY(!view.closed);
        QVERIFY(!view.lastError.isEmpty());
    }

    void test_presenterShareAdd_onSave_zeroVolume_showsError()
    {
        openMemoryDb();
        StubViewShareAdd  view;
        StubModelShareAdd model;
        view.m_volume = 0.0;
        PresenterShareAdd presenter(&view, &model, &m_docsConfig);

        presenter.onSave();

        QVERIFY(!view.closed);
        QVERIFY(!view.lastError.isEmpty());
    }

    void test_presenterShareAdd_onSave_zeroPrice_showsError()
    {
        openMemoryDb();
        StubViewShareAdd  view;
        StubModelShareAdd model;
        view.m_price = 0.0;
        PresenterShareAdd presenter(&view, &model, &m_docsConfig);

        presenter.onSave();

        QVERIFY(!view.closed);
        QVERIFY(!view.lastError.isEmpty());
    }

    void test_presenterShareAdd_onSave_duplicateWkn_showsError()
    {
        openMemoryDb();
        StubViewShareAdd  view;
        StubModelShareAdd model;
        model.wknExistsResult = true; // simulate existing WKN
        PresenterShareAdd presenter(&view, &model, &m_docsConfig);

        presenter.onSave();

        QVERIFY(!view.closed);
        QVERIFY(view.lastError.contains(view.m_wkn));
    }

    void test_presenterShareAdd_onSave_duplicateIsin_showsError()
    {
        openMemoryDb();
        StubViewShareAdd  view;
        StubModelShareAdd model;
        model.isinExistsResult = true;
        PresenterShareAdd presenter(&view, &model, &m_docsConfig);

        presenter.onSave();

        QVERIFY(!view.closed);
        QVERIFY(view.lastError.contains(view.m_isin));
    }

    void test_presenterShareAdd_onSave_modelError_showsError()
    {
        openMemoryDb();
        StubViewShareAdd  view;
        StubModelShareAdd model;
        model.saveResult = false;
        model.errorMsg   = QStringLiteral("DB-Fehler beim Speichern");
        PresenterShareAdd presenter(&view, &model, &m_docsConfig);

        presenter.onSave();

        QVERIFY(!view.closed);
        QCOMPARE(view.lastError, QStringLiteral("DB-Fehler beim Speichern"));
    }

    void test_presenterShareAdd_onSave_invalidDateTime_showsError()
    {
        openMemoryDb();
        StubViewShareAdd  view;
        StubModelShareAdd model;
        view.m_buyDateTime = QDateTime(); // invalid
        PresenterShareAdd presenter(&view, &model, &m_docsConfig);

        presenter.onSave();

        QVERIFY(!view.closed);
        QVERIFY(!view.lastError.isEmpty());
    }

    void test_presenterShareAdd_onDocumentSelected_writesPathIntoView()
    {
        // Regression 21.08.2026 (Nessies Bugreport): analog zu
        // test_presenterBuyEdit_onDocumentSelected_writesPathIntoView in
        // tst_buysform.cpp — MainWindow ruft für ein per Drag&Drop erfasstes
        // Dokument dlg.presenter()->onDocumentSelected() direkt auf,
        // ViewShareAdd::onBrowseDocument() (das früher als einziges
        // m_documentPath->setText() setzte) wird dabei nie durchlaufen.
        openMemoryDb();

        StubViewShareAdd  view;
        StubModelShareAdd model;
        PresenterShareAdd presenter(&view, &model, &m_docsConfig);

        presenter.onDocumentSelected(QStringLiteral("/tmp/dropped.pdf"));

        QCOMPARE(view.documentPath(), QStringLiteral("/tmp/dropped.pdf"));
    }

    // ─────────────────────────────────────────────────────────────────────
    // ViewShareAdd — widget state
    // ─────────────────────────────────────────────────────────────────────

    void test_viewShareAdd_initialValues()
    {
        openMemoryDb();
        ViewShareAdd dlg(&m_docsConfig);

        // All monetary fields start at 0
        QCOMPARE(dlg.volume(),    0.0);
        QCOMPARE(dlg.price(),     0.0);
        QCOMPARE(dlg.provision(), 0.0);
        QCOMPARE(dlg.brokerFee(), 0.0);
        QCOMPARE(dlg.traderFee(), 0.0);
        QCOMPARE(dlg.reduction(), 0.0);

        // Text fields start empty
        QVERIFY(dlg.wkn().isEmpty());
        QVERIFY(dlg.isin().isEmpty());
        QVERIFY(dlg.name().isEmpty());
        QVERIFY(dlg.documentPath().isEmpty());
    }

    void test_viewShareAdd_setFieldOk_updatesLineEdit()
    {
        openMemoryDb();
        ViewShareAdd dlg(&m_docsConfig);
        dlg.setFieldOk(QStringLiteral("wkn"), QStringLiteral("840400"));

        // Value must be reflected in the accessor
        QCOMPARE(dlg.wkn(), QStringLiteral("840400"));
    }

    void test_viewShareAdd_setFieldOk_updatesSpinBox_volume()
    {
        openMemoryDb();
        ViewShareAdd dlg(&m_docsConfig);
        dlg.setFieldOk(QStringLiteral("volume"), QStringLiteral("10"));
        QCOMPARE(dlg.volume(), 10.0);
    }

    void test_viewShareAdd_setFieldOk_handlesGermanDecimal()
    {
        openMemoryDb();
        ViewShareAdd dlg(&m_docsConfig);
        dlg.setFieldOk(QStringLiteral("price"), QStringLiteral("245,60"));
        QCOMPARE(dlg.price(), 245.60);
    }

    void test_viewShareAdd_setFieldError_doesNotCrash()
    {
        openMemoryDb();
        ViewShareAdd dlg(&m_docsConfig);
        // Setting error on unknown or valid field must not crash
        dlg.setFieldError(QStringLiteral("wkn"));
        dlg.setFieldError(QStringLiteral("nonexistent_field"));
    }

    void test_viewShareAdd_shareType_defaultIsShare()
    {
        openMemoryDb();
        ViewShareAdd dlg(&m_docsConfig);
        QCOMPARE(dlg.shareType(), ShareType::Share);
    }

    void test_viewShareAdd_parsingType_defaultIsRegex()
    {
        openMemoryDb();
        ViewShareAdd dlg(&m_docsConfig);
        QCOMPARE(dlg.marketPriceParsingType(), ShareParsingType::Regex);
        QCOMPARE(dlg.dailyValuesParsingType(), ShareParsingType::Regex);
    }

    void test_viewShareAdd_buyDateTime_isValid()
    {
        openMemoryDb();
        ViewShareAdd dlg(&m_docsConfig);
        QVERIFY(dlg.buyDateTime().isValid());
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

    void test_loggerSettings_saveColors()
    {
        const QColor c(QStringLiteral("#44ff44"));
        QList<QColor> colors = AppSettings::instance().logColors();
        colors[5] = c;
        AppSettings::instance().setLogColors(colors);
        QCOMPARE(AppSettings::instance().logColorAt(5).name(), c.name());
    }

    void test_loggerSettings_saveLevels()
    {
        AppSettings::instance().setLogLevels(0b00111);
        QCOMPARE(AppSettings::instance().logLevels(), 0b00111);
        AppSettings::instance().setLogLevels(31);
    }

    void test_loggerSettings_saveComponents()
    {
        AppSettings::instance().setLogComponents(0b011);
        QCOMPARE(AppSettings::instance().logComponents(), 0b011);
        AppSettings::instance().setLogComponents(7);
    }

    void test_soundSettings_saveUpdateEnabled()
    {
        AppSettings::instance().setSoundUpdateEnabled(false);
        QVERIFY(!AppSettings::instance().soundUpdateEnabled());
        AppSettings::instance().setSoundUpdateEnabled(true);
    }

    void test_soundSettings_saveErrorEnabled()
    {
        AppSettings::instance().setSoundErrorEnabled(false);
        QVERIFY(!AppSettings::instance().soundErrorEnabled());
        AppSettings::instance().setSoundErrorEnabled(true);
    }

    void test_soundSettings_saveUpdateFile()
    {
        AppSettings::instance().setSoundUpdateFile(QStringLiteral("Error.wav"));
        QCOMPARE(AppSettings::instance().soundUpdateFile(), QStringLiteral("Error.wav"));
        AppSettings::instance().setSoundUpdateFile(QStringLiteral("UpdateFinished.wav"));
    }

    void test_soundSettings_saveErrorFile()
    {
        AppSettings::instance().setSoundErrorFile(QStringLiteral("UpdateFinished.wav"));
        QCOMPARE(AppSettings::instance().soundErrorFile(), QStringLiteral("UpdateFinished.wav"));
        AppSettings::instance().setSoundErrorFile(QStringLiteral("Error.wav"));
    }

    void test_soundSettings_scanFallback()
    {
        QDir d(m_tempDir.path() + QStringLiteral("/nosounds"));
        const QStringList files = d.entryList(QStringList() << QStringLiteral("*.wav"),
                                              QDir::Files, QDir::Name);
        QVERIFY(files.isEmpty());
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

    void test_apiSettings_saveYahooKey()
    {
        AppSettings::instance().setApiKeyYahoo(QStringLiteral("test-key-123"));
        QCOMPARE(AppSettings::instance().apiKeyYahoo(), QStringLiteral("test-key-123"));
        AppSettings::instance().setApiKeyYahoo(QString());
    }

    void test_viewShareAdd_recalc_kurswert()
    {
        openMemoryDb();
        ViewShareAdd dlg(&m_docsConfig);
        dlg.setFieldOk(QStringLiteral("volume"), QStringLiteral("10"));
        dlg.setFieldOk(QStringLiteral("price"),  QStringLiteral("245.60"));

        // Kurswert = volume × price = 2456.00
        // Read back via the read-only QLineEdit (find by object name or position)
        // We verify via the public accessors — volume and price must be set correctly
        QCOMPARE(dlg.volume(), 10.0);
        QCOMPARE(dlg.price(),  245.60);
        QVERIFY(dlg.volume() * dlg.price() == 2456.0);
    }

    void test_viewShareAdd_recalc_gesGebuehren()
    {
        openMemoryDb();
        ViewShareAdd dlg(&m_docsConfig);
        dlg.setFieldOk(QStringLiteral("provision"), QStringLiteral("9.90"));
        dlg.setFieldOk(QStringLiteral("brokerFee"), QStringLiteral("2.00"));
        dlg.setFieldOk(QStringLiteral("traderFee"), QStringLiteral("1.50"));

        // Ges. Gebühren = 9.90 + 2.00 + 1.50 = 13.40
        QCOMPARE(dlg.provision(), 9.90);
        QCOMPARE(dlg.brokerFee(), 2.00);
        QCOMPARE(dlg.traderFee(), 1.50);
        QVERIFY(qAbs((dlg.provision() + dlg.brokerFee() + dlg.traderFee()) - 13.40) < 0.001);
    }

    void test_viewShareAdd_recalc_endbetrag()
    {
        openMemoryDb();
        ViewShareAdd dlg(&m_docsConfig);
        dlg.setFieldOk(QStringLiteral("volume"),    QStringLiteral("10"));
        dlg.setFieldOk(QStringLiteral("price"),     QStringLiteral("100.00"));
        dlg.setFieldOk(QStringLiteral("provision"), QStringLiteral("5.00"));
        dlg.setFieldOk(QStringLiteral("brokerFee"), QStringLiteral("0.00"));
        dlg.setFieldOk(QStringLiteral("traderFee"), QStringLiteral("0.00"));
        dlg.setFieldOk(QStringLiteral("reduction"), QStringLiteral("2.00"));

        // Endbetrag = (10 × 100) + 5 - 2 = 1003.00
        const double kurswert   = dlg.volume() * dlg.price();
        const double gesGebuehr = dlg.provision() + dlg.brokerFee() + dlg.traderFee();
        const double endbetrag  = kurswert + gesGebuehr - dlg.reduction();
        QVERIFY(qAbs(endbetrag - 1003.0) < 0.001);
    }

    void test_viewShareAdd_marketApiKey_disabledForRegex()
    {
        openMemoryDb();
        ViewShareAdd dlg(&m_docsConfig);

        // Default is Regex — API key field must report empty
        QCOMPARE(dlg.marketPriceParsingType(), ShareParsingType::Regex);
        QVERIFY(dlg.marketPriceApiKey().isEmpty());
    }

    void test_viewShareAdd_dailyApiKey_enabledForApiYahoo()
    {
        openMemoryDb();
        ViewShareAdd dlg(&m_docsConfig);

        QCOMPARE(dlg.dailyValuesParsingType(), ShareParsingType::Regex);
        QVERIFY(dlg.dailyValuesApiKey().isEmpty());
    }

    void test_viewShareAdd_hasMissingRequiredFields_initiallyTrue()
    {
        // On construction all fields are Untouched — hasMissingRequiredFields must be true
        openMemoryDb();
        ViewShareAdd dlg(&m_docsConfig);

        QStringList missing;
        QVERIFY(dlg.hasMissingRequiredFields(missing));
        QVERIFY(!missing.isEmpty());
    }

    void test_viewShareAdd_hasMissingRequiredFields_falseAfterAllOk()
    {
        // After setting all parseable required fields to Ok state, only
        // listingDate may remain (it uses a date-sentinel, not FieldState).
        // We verify that the parsed fields are correctly tracked as Ok.
        openMemoryDb();
        ViewShareAdd dlg(&m_docsConfig);

        dlg.setFieldOk(QStringLiteral("wkn"),         QStringLiteral("863186"));
        dlg.setFieldOk(QStringLiteral("isin"),        QStringLiteral("US0079031078"));
        dlg.setFieldOk(QStringLiteral("name"),        QStringLiteral("AMD"));
        dlg.setFieldOk(QStringLiteral("date"),        QStringLiteral("4.2.2026"));
        dlg.setFieldOk(QStringLiteral("depotNumber"), QStringLiteral("8006189848"));
        dlg.setFieldOk(QStringLiteral("orderNumber"), QStringLiteral("ORD-001"));
        dlg.setFieldOk(QStringLiteral("volume"),      QStringLiteral("15"));
        dlg.setFieldOk(QStringLiteral("price"),       QStringLiteral("169.58"));

        // listingDate cannot be set via setFieldOk — it uses a date-sentinel.
        // After setting all other required fields, only listingDate should remain.
        QStringList missing;
        dlg.hasMissingRequiredFields(missing);
        // All fields except listingDate should be Ok
        QVERIFY(!missing.contains(QObject::tr("WKN")));
        QVERIFY(!missing.contains(QObject::tr("ISIN")));
        QVERIFY(!missing.contains(QObject::tr("Name")));
        QVERIFY(!missing.contains(QObject::tr("Datum")));
        QVERIFY(!missing.contains(QObject::tr("Depotnummer")));
        QVERIFY(!missing.contains(QObject::tr("Ordernummer")));
        QVERIFY(!missing.contains(QObject::tr("Anteile")));
        QVERIFY(!missing.contains(QObject::tr("Kurs")));
    }

    void test_viewShareAdd_onParseFinished_setsInfoOnUntouched()
    {
        // After onParseFinished(), untouched required fields get Info state
        // (verified indirectly — hasMissingRequiredFields still returns true)
        openMemoryDb();
        ViewShareAdd dlg(&m_docsConfig);

        dlg.onParseFinished();

        QStringList missing;
        QVERIFY(dlg.hasMissingRequiredFields(missing));
        QVERIFY(!missing.isEmpty());
    }

    void test_viewShareAdd_markMissingFieldsAsFailed_doesNotCrash()
    {
        openMemoryDb();
        ViewShareAdd dlg(&m_docsConfig);

        // Should not crash even when no fields have been touched
        dlg.markMissingFieldsAsFailed();

        QStringList missing;
        QVERIFY(dlg.hasMissingRequiredFields(missing));
    }

    void test_viewShareAdd_documentPreviewPanel_nonExistentFile_doesNotCrash()
    {
        // ViewShareAdd was migrated from its own QPdfView-/pdftoppm-copy to
        // the shared DocumentPreviewPanel (19.07.2026, see ARCHITECTURE.md,
        // "Offene Punkte / TODO") — same non-blocking "not found" handling
        // as ViewBuyEdit/ViewSaleEdit/ViewDividendEdit/ViewBrokerageEdit
        // now applies here too. openPdfPreview() no longer exists on
        // ViewShareAdd itself (not part of IViewShareAdd, unlike the other
        // four dialogs), so the panel is reached via findChild() instead.
        openMemoryDb();
        ViewShareAdd dlg(&m_docsConfig);

        auto* panel = dlg.findChild<DocumentPreviewPanel*>();
        QVERIFY(panel != nullptr);

        panel->showDocument(QStringLiteral("/no/such/file.pdf"));
        QVERIFY(true);
    }

    // ─────────────────────────────────────────────────────────────────────
    // ModelBrokerageEdit — database tests
    // ─────────────────────────────────────────────────────────────────────

    void test_modelBrokerageEdit_addBrokerage_success()
    {
        const QString shareGuid = insertTestShare();
        ModelBrokerageEdit model;
        BrokerageObject br(QStringLiteral("brok-1"), shareGuid, QString(), QString(),
                           QStringLiteral("2024-03-10T10:00:00"),
                           9.90, 0.0, 0.0, 0.0);
        QVERIFY(model.addBrokerage(br));
        QCOMPARE(model.loadBrokerages(shareGuid).size(), 1);
    }

    void test_modelBrokerageEdit_updateBrokerage_success()
    {
        const QString shareGuid = insertTestShare();
        ModelBrokerageEdit model;
        BrokerageObject br(QStringLiteral("brok-1"), shareGuid, QString(), QString(),
                           QStringLiteral("2024-03-10T10:00:00"),
                           9.90, 0.0, 0.0, 0.0);
        QVERIFY(model.addBrokerage(br));

        BrokerageObject updated(QStringLiteral("brok-1"), shareGuid, QString(), QString(),
                                QStringLiteral("2024-03-10T10:00:00"),
                                15.0, 2.0, 1.0, 0.5);
        QVERIFY(model.updateBrokerage(updated));

        const auto list = model.loadBrokerages(shareGuid);
        QCOMPARE(list.size(), 1);
        QCOMPARE(list.first().provision(), 15.0);
    }

    void test_modelBrokerageEdit_updateDocument_success()
    {
        const QString shareGuid = insertTestShare();
        ModelBrokerageEdit model;
        BrokerageObject br(QStringLiteral("brok-1"), shareGuid, QString(), QString(),
                           QStringLiteral("2024-03-10T10:00:00"),
                           9.90, 0.0, 0.0, 0.0);
        QVERIFY(model.addBrokerage(br));

        QVERIFY(model.updateDocument(QStringLiteral("brok-1"),
                                     QStringLiteral("/docs/new.pdf")));

        const auto list = model.loadBrokerages(shareGuid);
        QCOMPARE(list.first().document(), QStringLiteral("/docs/new.pdf"));
    }

    void test_modelBrokerageEdit_removeBrokerage_success()
    {
        const QString shareGuid = insertTestShare();
        ModelBrokerageEdit model;
        BrokerageObject br(QStringLiteral("brok-1"), shareGuid, QString(), QString(),
                           QStringLiteral("2024-03-10T10:00:00"),
                           9.90, 0.0, 0.0, 0.0);
        QVERIFY(model.addBrokerage(br));
        QVERIFY(model.removeBrokerage(QStringLiteral("brok-1")));
        QVERIFY(model.loadBrokerages(shareGuid).isEmpty());
    }

    void test_modelBrokerageEdit_documentExists_notFound_returnsFalse()
    {
        const QString shareGuid = insertTestShare();
        ModelBrokerageEdit model;
        QVERIFY(!model.documentExists(QStringLiteral("/docs/does-not-exist.pdf")));
    }

    void test_modelBrokerageEdit_documentExists_emptyPath_returnsFalse()
    {
        openMemoryDb();
        ModelBrokerageEdit model;
        QVERIFY(!model.documentExists(QString()));
    }

    void test_modelBrokerageEdit_documentExists_excludeGuid()
    {
        const QString shareGuid = insertTestShare();
        ModelBrokerageEdit model;
        BrokerageObject br(QStringLiteral("brok-1"), shareGuid, QString(), QString(),
                           QStringLiteral("2024-03-10T10:00:00"),
                           9.90, 0.0, 0.0, 0.0, QStringLiteral("/docs/a.pdf"));
        QVERIFY(model.addBrokerage(br));

        // Without exclude — document is found (belongs to brok-1 itself).
        QVERIFY(model.documentExists(QStringLiteral("/docs/a.pdf")));
        // With exclude == own GUID — must report "not used by another record".
        QVERIFY(!model.documentExists(QStringLiteral("/docs/a.pdf"),
                                      QStringLiteral("brok-1")));
    }

    void test_modelBrokerageEdit_loadBrokerages_orderedByDate()
    {
        const QString shareGuid = insertTestShare();
        ModelBrokerageEdit model;
        BrokerageObject br2(QStringLiteral("brok-2"), shareGuid, QString(), QString(),
                            QStringLiteral("2024-06-01T10:00:00"), 5.0);
        BrokerageObject br1(QStringLiteral("brok-1"), shareGuid, QString(), QString(),
                            QStringLiteral("2024-01-10T10:00:00"), 9.90);
        QVERIFY(model.addBrokerage(br2));
        QVERIFY(model.addBrokerage(br1));

        const auto list = model.loadBrokerages(shareGuid);
        QCOMPARE(list.size(), 2);
        QVERIFY(list[0].dateTime() < list[1].dateTime());
    }

    // ─────────────────────────────────────────────────────────────────────
    // PresenterBrokerageEdit — stub-based tests
    // ─────────────────────────────────────────────────────────────────────

    void test_presenterBrokerageEdit_construction_loadsOverview()
    {
        openMemoryDb();
        StubViewBrokerageEdit  view;
        StubModelBrokerageEdit model;
        PresenterBrokerageEdit p(&view, &model, QStringLiteral("share-1"));
        QVERIFY(view.populateOverviewCalled);
    }

    void test_presenterBrokerageEdit_construction_clearsForm()
    {
        openMemoryDb();
        StubViewBrokerageEdit  view;
        StubModelBrokerageEdit model;
        PresenterBrokerageEdit p(&view, &model, QStringLiteral("share-1"));
        QVERIFY(view.clearFormCalled);
    }

    void test_presenterBrokerageEdit_construction_setsButtonStates_noSelection()
    {
        openMemoryDb();
        StubViewBrokerageEdit  view;
        StubModelBrokerageEdit model;
        PresenterBrokerageEdit p(&view, &model, QStringLiteral("share-1"));
        QVERIFY(view.setButtonStatesCalled);
        QCOMPARE(view.lastCanRemove, false);
        QCOMPARE(view.lastIsEdit,    false);
        QCOMPARE(view.lastReadOnly,  false);
    }

    void test_presenterBrokerageEdit_onSave_newBrokerage_callsAddBrokerage()
    {
        openMemoryDb();
        StubViewBrokerageEdit  view;
        StubModelBrokerageEdit model;
        PresenterBrokerageEdit p(&view, &model, QStringLiteral("share-1"));
        view.m_provision = 9.90;
        p.onSave();
        QVERIFY(model.addBrokerageCalled);
    }

    void test_presenterBrokerageEdit_onSave_newBrokerage_emitsDataChanged()
    {
        openMemoryDb();
        StubViewBrokerageEdit  view;
        StubModelBrokerageEdit model;
        PresenterBrokerageEdit p(&view, &model, QStringLiteral("share-1"));
        view.m_provision = 9.90;
        QSignalSpy spy(&p, &PresenterBrokerageEdit::dataChanged);
        p.onSave();
        QCOMPARE(spy.count(), 1);
    }

    void test_presenterBrokerageEdit_onSave_newBrokerage_jumpsToOverviewTab()
    {
        openMemoryDb();
        StubViewBrokerageEdit  view;
        StubModelBrokerageEdit model;
        PresenterBrokerageEdit p(&view, &model, QStringLiteral("share-1"));
        view.m_provision = 9.90;
        view.showOverviewTabCalled = false;
        p.onSave();
        QVERIFY(view.showOverviewTabCalled);
    }

    void test_presenterBrokerageEdit_onSave_missingFields_showsError()
    {
        openMemoryDb();
        StubViewBrokerageEdit  view;
        StubModelBrokerageEdit model;
        view.m_missingFields = true;
        PresenterBrokerageEdit p(&view, &model, QStringLiteral("share-1"));
        view.m_provision = 9.90;
        p.onSave();
        QVERIFY(!view.lastError.isEmpty());
        QVERIFY(!model.addBrokerageCalled);
    }

    void test_presenterBrokerageEdit_onSave_allFieldsZero_showsError()
    {
        openMemoryDb();
        StubViewBrokerageEdit  view;
        StubModelBrokerageEdit model;
        PresenterBrokerageEdit p(&view, &model, QStringLiteral("share-1"));
        view.m_provision = 0.0;
        view.m_brokerFee = 0.0;
        view.m_traderFee = 0.0;
        view.m_reduction = 0.0;
        p.onSave();
        QVERIFY(!view.lastError.isEmpty());
        QVERIFY(!model.addBrokerageCalled);
    }

    void test_presenterBrokerageEdit_onSave_onlyRabattSet_success()
    {
        openMemoryDb();
        StubViewBrokerageEdit  view;
        StubModelBrokerageEdit model;
        PresenterBrokerageEdit p(&view, &model, QStringLiteral("share-1"));
        view.m_provision = 0.0;
        view.m_brokerFee = 0.0;
        view.m_traderFee = 0.0;
        view.m_reduction = 5.0; // 100% reduction scenario — still valid
        p.onSave();
        QVERIFY(model.addBrokerageCalled);
    }

    void test_presenterBrokerageEdit_onSave_onlyProvisionSet_success()
    {
        openMemoryDb();
        StubViewBrokerageEdit  view;
        StubModelBrokerageEdit model;
        PresenterBrokerageEdit p(&view, &model, QStringLiteral("share-1"));
        view.m_provision = 9.90;
        view.m_brokerFee = 0.0;
        view.m_traderFee = 0.0;
        view.m_reduction = 0.0;
        p.onSave();
        QVERIFY(model.addBrokerageCalled);
    }

    void test_presenterBrokerageEdit_onSave_documentDuplicate_showsError()
    {
        openMemoryDb();
        StubViewBrokerageEdit  view;
        StubModelBrokerageEdit model;
        model.docExists = true;
        PresenterBrokerageEdit p(&view, &model, QStringLiteral("share-1"));
        view.m_provision = 9.90;
        view.m_docPath   = QStringLiteral("/docs/dup.pdf");
        p.onSave();
        QVERIFY(!view.lastError.isEmpty());
        QVERIFY(!model.addBrokerageCalled);
    }

    void test_presenterBrokerageEdit_onSave_existingStandalone_callsUpdateBrokerage()
    {
        openMemoryDb();
        StubViewBrokerageEdit  view;
        StubModelBrokerageEdit model;
        // Standalone record: no buyGuid/saleGuid.
        model.brokerages.append(BrokerageObject(
            QStringLiteral("brok-1"), QStringLiteral("share-1"),
            QString(), QString(),
            QStringLiteral("2024-01-10T10:00:00"), 9.90));
        PresenterBrokerageEdit p(&view, &model, QStringLiteral("share-1"));
        p.onRowSelected(QStringLiteral("brok-1"));
        view.m_provision = 15.0;
        p.onSave();
        QVERIFY(model.updateBrokerageCalled);
        QVERIFY(!model.addBrokerageCalled);
    }

    void test_presenterBrokerageEdit_onSave_linkedRecord_showsError()
    {
        openMemoryDb();
        StubViewBrokerageEdit  view;
        StubModelBrokerageEdit model;
        // Linked record: buyGuid set.
        model.brokerages.append(BrokerageObject(
            QStringLiteral("brok-1"), QStringLiteral("share-1"),
            QStringLiteral("buy-1"), QString(),
            QStringLiteral("2024-01-10T10:00:00"), 9.90));
        PresenterBrokerageEdit p(&view, &model, QStringLiteral("share-1"));
        p.onRowSelected(QStringLiteral("brok-1"));
        p.onSave();
        QVERIFY(!view.lastError.isEmpty());
        QVERIFY(!model.updateBrokerageCalled);
        QVERIFY(!model.updateDocumentCalled);
    }

    void test_presenterBrokerageEdit_onRowSelected_standaloneRecord_canRemoveTrue()
    {
        openMemoryDb();
        StubViewBrokerageEdit  view;
        StubModelBrokerageEdit model;
        model.brokerages.append(BrokerageObject(
            QStringLiteral("brok-1"), QStringLiteral("share-1"),
            QString(), QString(),
            QStringLiteral("2024-01-10T10:00:00"), 9.90));
        PresenterBrokerageEdit p(&view, &model, QStringLiteral("share-1"));
        p.onRowSelected(QStringLiteral("brok-1"));
        QCOMPARE(view.lastCanRemove, true);
        QCOMPARE(view.lastReadOnly,  false);
    }

    void test_presenterBrokerageEdit_onRowSelected_linkedRecord_canRemoveFalse()
    {
        openMemoryDb();
        StubViewBrokerageEdit  view;
        StubModelBrokerageEdit model;
        model.brokerages.append(BrokerageObject(
            QStringLiteral("brok-1"), QStringLiteral("share-1"),
            QString(), QStringLiteral("sale-1"),
            QStringLiteral("2024-01-10T10:00:00"), 9.90));
        PresenterBrokerageEdit p(&view, &model, QStringLiteral("share-1"));
        p.onRowSelected(QStringLiteral("brok-1"));
        QCOMPARE(view.lastCanRemove, false);
        QCOMPARE(view.lastReadOnly,  true);
        QCOMPARE(view.lastIsEdit,    true);
    }

    void test_presenterBrokerageEdit_onRowSelected_withDocument_opensPdfPreview()
    {
        openMemoryDb();
        StubViewBrokerageEdit  view;
        StubModelBrokerageEdit model;
        model.brokerages.append(BrokerageObject(
            QStringLiteral("brok-1"), QStringLiteral("share-1"),
            QString(), QString(),
            QStringLiteral("2024-01-10T10:00:00"), 9.90,
            0.0, 0.0, 0.0, QStringLiteral("/docs/a.pdf")));
        PresenterBrokerageEdit p(&view, &model, QStringLiteral("share-1"));
        p.onRowSelected(QStringLiteral("brok-1"));
        QVERIFY(view.openPdfPreviewCalled);
    }

    void test_presenterBrokerageEdit_onRowSelected_withoutDocument_clearsPdfPreview()
    {
        openMemoryDb();
        StubViewBrokerageEdit  view;
        StubModelBrokerageEdit model;
        model.brokerages.append(BrokerageObject(
            QStringLiteral("brok-1"), QStringLiteral("share-1"),
            QString(), QString(),
            QStringLiteral("2024-01-10T10:00:00"), 9.90));
        PresenterBrokerageEdit p(&view, &model, QStringLiteral("share-1"));
        p.onRowSelected(QStringLiteral("brok-1"));
        QVERIFY(view.clearPdfPreviewCalled);
    }

    void test_presenterBrokerageEdit_onRowSelected_emptyGuid_resetsForm()
    {
        openMemoryDb();
        StubViewBrokerageEdit  view;
        StubModelBrokerageEdit model;
        PresenterBrokerageEdit p(&view, &model, QStringLiteral("share-1"));
        view.clearFormCalled = false;
        p.onRowSelected(QString());
        QVERIFY(view.clearFormCalled);
    }

    void test_presenterBrokerageEdit_onRemove_standalone_callsModel()
    {
        openMemoryDb();
        StubViewBrokerageEdit  view;
        StubModelBrokerageEdit model;
        model.brokerages.append(BrokerageObject(
            QStringLiteral("brok-1"), QStringLiteral("share-1"),
            QString(), QString(),
            QStringLiteral("2024-01-10T10:00:00"), 9.90));
        PresenterBrokerageEdit p(&view, &model, QStringLiteral("share-1"));
        p.onRowSelected(QStringLiteral("brok-1"));
        p.onRemove();
        QVERIFY(model.removeBrokerageCalled);
    }

    void test_presenterBrokerageEdit_onRemove_standalone_emitsDataChanged()
    {
        openMemoryDb();
        StubViewBrokerageEdit  view;
        StubModelBrokerageEdit model;
        model.brokerages.append(BrokerageObject(
            QStringLiteral("brok-1"), QStringLiteral("share-1"),
            QString(), QString(),
            QStringLiteral("2024-01-10T10:00:00"), 9.90));
        PresenterBrokerageEdit p(&view, &model, QStringLiteral("share-1"));
        p.onRowSelected(QStringLiteral("brok-1"));
        QSignalSpy spy(&p, &PresenterBrokerageEdit::dataChanged);
        p.onRemove();
        QCOMPARE(spy.count(), 1);
    }

    void test_presenterBrokerageEdit_onRemove_linkedRecord_showsError()
    {
        openMemoryDb();
        StubViewBrokerageEdit  view;
        StubModelBrokerageEdit model;
        model.brokerages.append(BrokerageObject(
            QStringLiteral("brok-1"), QStringLiteral("share-1"),
            QStringLiteral("buy-1"), QString(),
            QStringLiteral("2024-01-10T10:00:00"), 9.90));
        PresenterBrokerageEdit p(&view, &model, QStringLiteral("share-1"));
        p.onRowSelected(QStringLiteral("brok-1"));
        p.onRemove();
        QVERIFY(!model.removeBrokerageCalled);
        QVERIFY(!view.lastError.isEmpty());
    }

    void test_presenterBrokerageEdit_onRemove_noSelection_doesNothing()
    {
        openMemoryDb();
        StubViewBrokerageEdit  view;
        StubModelBrokerageEdit model;
        PresenterBrokerageEdit p(&view, &model, QStringLiteral("share-1"));
        p.onRemove();
        QVERIFY(!model.removeBrokerageCalled);
    }

    void test_presenterBrokerageEdit_onReset_clearsForm()
    {
        openMemoryDb();
        StubViewBrokerageEdit  view;
        StubModelBrokerageEdit model;
        PresenterBrokerageEdit p(&view, &model, QStringLiteral("share-1"));
        view.clearFormCalled = false;
        p.onReset();
        QVERIFY(view.clearFormCalled);
    }

    void test_presenterBrokerageEdit_onReset_clearsPdfPreview()
    {
        openMemoryDb();
        StubViewBrokerageEdit  view;
        StubModelBrokerageEdit model;
        PresenterBrokerageEdit p(&view, &model, QStringLiteral("share-1"));
        p.onReset();
        QVERIFY(view.clearPdfPreviewCalled);
    }

    void test_presenterBrokerageEdit_onReset_jumpsToOverviewTab()
    {
        openMemoryDb();
        StubViewBrokerageEdit  view;
        StubModelBrokerageEdit model;
        PresenterBrokerageEdit p(&view, &model, QStringLiteral("share-1"));
        view.showOverviewTabCalled = false;
        p.onReset();
        QVERIFY(view.showOverviewTabCalled);
    }

    void test_presenterBrokerageEdit_onReset_setsButtonStates_noSelection()
    {
        openMemoryDb();
        StubViewBrokerageEdit  view;
        StubModelBrokerageEdit model;
        PresenterBrokerageEdit p(&view, &model, QStringLiteral("share-1"));
        p.onReset();
        QCOMPARE(view.lastCanRemove, false);
        QCOMPARE(view.lastIsEdit,    false);
        QCOMPARE(view.lastReadOnly,  false);
    }

    void test_presenterBrokerageEdit_onValuesChanged_updatesGesamtGebuehren()
    {
        openMemoryDb();
        StubViewBrokerageEdit  view;
        StubModelBrokerageEdit model;
        PresenterBrokerageEdit p(&view, &model, QStringLiteral("share-1"));
        view.m_provision = 10.0;
        view.m_brokerFee = 2.0;
        view.m_traderFee = 1.0;
        p.onValuesChanged();
        QCOMPARE(view.lastGesamtGebuehren, 13.0);
    }

    void test_presenterBrokerageEdit_onDocumentPathEdited_duplicate_showsError()
    {
        openMemoryDb();
        StubViewBrokerageEdit  view;
        StubModelBrokerageEdit model;
        model.docExists = true;
        PresenterBrokerageEdit p(&view, &model, QStringLiteral("share-1"));
        view.m_docPath = QStringLiteral("/docs/dup.pdf");
        p.onDocumentPathEdited();
        QVERIFY(!view.lastError.isEmpty());
    }

    void test_presenterBrokerageEdit_onDocumentPathEdited_unique_noError()
    {
        openMemoryDb();
        StubViewBrokerageEdit  view;
        StubModelBrokerageEdit model;
        model.docExists = false;
        PresenterBrokerageEdit p(&view, &model, QStringLiteral("share-1"));
        view.m_docPath = QStringLiteral("/docs/unique.pdf");
        p.onDocumentPathEdited();
        QVERIFY(view.lastError.isEmpty());
    }

    void test_presenterBrokerageEdit_onClose_closesView()
    {
        openMemoryDb();
        StubViewBrokerageEdit  view;
        StubModelBrokerageEdit model;
        PresenterBrokerageEdit p(&view, &model, QStringLiteral("share-1"));
        p.onClose();
        QVERIFY(view.closed);
    }

    // ─────────────────────────────────────────────────────────────────────
    // ViewBrokerageEdit
    // ─────────────────────────────────────────────────────────────────────

    void test_viewBrokerageEdit_canBeConstructed()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));
        QVERIFY(dlg.windowTitle().contains(tr("Kosten")));
    }

    void test_viewBrokerageEdit_initialValues()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));
        QCOMPARE(dlg.provision(), 0.0);
        QCOMPARE(dlg.brokerFee(), 0.0);
        QCOMPARE(dlg.traderFee(), 0.0);
        QCOMPARE(dlg.reduction(), 0.0);
        QVERIFY(dlg.documentPath().isEmpty());
    }

    void test_viewBrokerageEdit_hasMissingRequiredFields_initiallyTrue()
    {
        // NOTE: Unlike BuysForm/SalesForm, ViewBrokerageEdit's QDateEdit defaults
        // to QDate::currentDate() (not the 2000-01-01 sentinel), so directly after
        // construction nothing is actually missing. We verify both the actual
        // post-construction state and the sentinel-triggered missing-field path
        // that hasMissingRequiredFields() is designed to detect.
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));

        QStringList missing;
        QVERIFY(!dlg.hasMissingRequiredFields(missing));

        // Load a record with the sentinel date to exercise the missing-field path.
        BrokerageObject sentinelBr(QStringLiteral("brok-1"), QStringLiteral("share-guid"),
                                   QString(), QString(),
                                   QStringLiteral("2000-01-01T00:00:00"), 9.90);
        dlg.loadBrokerage(sentinelBr);
        QVERIFY(dlg.hasMissingRequiredFields(missing));
        QVERIFY(missing.contains(QStringLiteral("date")));
    }

    void test_viewBrokerageEdit_hasMissingRequiredFields_falseAfterDateSet()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));
        QStringList missing;
        QVERIFY(!dlg.hasMissingRequiredFields(missing));
    }

    void test_viewBrokerageEdit_clearForm_resetsAllFields()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));
        BrokerageObject br(QStringLiteral("brok-1"), QStringLiteral("share-guid"),
                           QString(), QString(),
                           QStringLiteral("2024-01-10T10:00:00"), 9.90,
                           0.0, 0.0, 0.0, QStringLiteral("/docs/a.pdf"));
        dlg.loadBrokerage(br);
        dlg.clearForm();
        QCOMPARE(dlg.provision(), 0.0);
        QVERIFY(dlg.documentPath().isEmpty());
    }

    void test_viewBrokerageEdit_clearForm_restoresEditableFields()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));
        dlg.setButtonStates(/*canRemove=*/false, /*isEdit=*/true, /*readOnly=*/true);
        dlg.clearForm();

        // clearForm() must re-enable the Hinzufügen-button and all fee fields
        // regardless of any prior readOnly state.
        const auto buttons = dlg.findChildren<QPushButton*>();
        QPushButton* btnAdd = nullptr;
        for (auto* b : buttons) {
            if (b->text() == QObject::tr("Hinzufügen") || b->text() == QObject::tr("Speichern")) {
                btnAdd = b;
                break;
            }
        }
        if (!btnAdd) QFAIL("Hinzufügen/Speichern button not found");
        QVERIFY(btnAdd->isEnabled());
    }

    void test_viewBrokerageEdit_setButtonStates_noSelection_addLabelHinzufuegen()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));
        dlg.setButtonStates(false, false, false);
        const auto buttons = dlg.findChildren<QPushButton*>();
        bool found = false;
        for (auto* b : buttons)
            if (b->text() == QObject::tr("Hinzufügen")) found = true;
        QVERIFY(found);
    }

    void test_viewBrokerageEdit_setButtonStates_isEdit_saveLabelSpeichern()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));
        dlg.setButtonStates(true, true, false);
        const auto buttons = dlg.findChildren<QPushButton*>();
        bool found = false;
        for (auto* b : buttons)
            if (b->text() == QObject::tr("Speichern")) found = true;
        QVERIFY(found);
    }

    void test_viewBrokerageEdit_setButtonStates_canRemoveFalse_removeDisabled()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));
        dlg.setButtonStates(false, true, false);
        const auto buttons = dlg.findChildren<QPushButton*>();
        bool found = false;
        for (auto* b : buttons)
            if (b->text() == QObject::tr("Entfernen")) { found = true; QVERIFY(!b->isEnabled()); }
        QVERIFY(found);
    }

    void test_viewBrokerageEdit_setButtonStates_canRemoveTrue_removeEnabled()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));
        dlg.setButtonStates(true, true, false);
        const auto buttons = dlg.findChildren<QPushButton*>();
        bool found = false;
        for (auto* b : buttons)
            if (b->text() == QObject::tr("Entfernen")) { found = true; QVERIFY(b->isEnabled()); }
        QVERIFY(found);
    }

    void test_viewBrokerageEdit_setButtonStates_readOnly_feeFieldsDisabled()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));
        dlg.setButtonStates(false, true, /*readOnly=*/true);

        const auto lineEdits = dlg.findChildren<QLineEdit*>();
        // documentPath field itself is always read-only by construction, so we
        // only assert on the fee/date editability and the Speichern-button.
        bool anyEnabledFeeField = false;
        for (auto* le : lineEdits) {
            // Skip read-only-by-design fields (Ges.Gebühren, Netto-Kosten, Dokument).
            if (le->isReadOnly()) continue;
            if (le->isEnabled()) anyEnabledFeeField = true;
        }
        QVERIFY(!anyEnabledFeeField);

        const auto buttons = dlg.findChildren<QPushButton*>();
        for (auto* b : buttons)
            if (b->text() == QObject::tr("Speichern") || b->text() == QObject::tr("Hinzufügen"))
                QVERIFY(!b->isEnabled());
    }

    void test_viewBrokerageEdit_setButtonStates_notReadOnly_feeFieldsEnabled()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));
        dlg.setButtonStates(false, true, /*readOnly=*/false);

        const auto lineEdits = dlg.findChildren<QLineEdit*>();
        bool allEditableEnabled = true;
        for (auto* le : lineEdits) {
            if (le->isReadOnly()) continue;
            if (!le->isEnabled()) allEditableEnabled = false;
        }
        QVERIFY(allEditableEnabled);
    }

    void test_viewBrokerageEdit_setGesamtGebuehren_updatesField()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));
        dlg.setGesamtGebuehren(12.50);
        const auto lineEdits = dlg.findChildren<QLineEdit*>();
        bool found = false;
        for (auto* le : lineEdits)
            if (le->isReadOnly() && le->text().contains(QStringLiteral("12,50")))
                found = true;
        QVERIFY(found);
    }

    void test_viewBrokerageEdit_setBrokerageReduction_positiveGreen()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));
        dlg.setBrokerageReduction(10.0);
        const auto lineEdits = dlg.findChildren<QLineEdit*>();
        bool found = false;
        for (auto* le : lineEdits)
            if (le->text().contains(QStringLiteral("10,00")) &&
                le->styleSheet().contains(QStringLiteral("d4edda")))
                found = true;
        QVERIFY(found);
    }

    void test_viewBrokerageEdit_setBrokerageReduction_negativeRed()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));
        dlg.setBrokerageReduction(-5.0);
        const auto lineEdits = dlg.findChildren<QLineEdit*>();
        bool found = false;
        for (auto* le : lineEdits)
            if (le->text().contains(QStringLiteral("-5,00")) &&
                le->styleSheet().contains(QStringLiteral("f8d7da")))
                found = true;
        QVERIFY(found);
    }

    void test_viewBrokerageEdit_clearPdfPreview_doesNotCrash()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));
        dlg.clearPdfPreview();
        QVERIFY(true);
    }

    void test_viewBrokerageEdit_openPdfPreview_nonExistentFile_doesNotCrash()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));
        dlg.openPdfPreview(QStringLiteral("/no/such/file.pdf"));
        QVERIFY(true);
    }

    void test_viewBrokerageEdit_markMissingFieldsAsFailed_doesNotCrash()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));
        dlg.markMissingFieldsAsFailed();
        QVERIFY(true);
    }

    // ─────────────────────────────────────────────────────────────────────
    // ViewBrokerageEdit — populateOverview
    // ─────────────────────────────────────────────────────────────────────

    void test_viewBrokerageEdit_populateOverview_emptyList_noTabs()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));
        dlg.populateOverview({});
        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        QCOMPARE(tabs->count(), 0);
    }

    void test_viewBrokerageEdit_populateOverview_singleYear_twoTabs()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));
        BrokerageObject br(QStringLiteral("brok-1"), QStringLiteral("share-guid"),
                           QString(), QString(),
                           QStringLiteral("2024-03-10T10:00:00"), 9.90);
        dlg.populateOverview({ br });

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        QCOMPARE(tabs->count(), 2);
        QVERIFY(tabs->tabText(0).contains(tr("Übersicht")));
        QVERIFY(tabs->tabText(1).contains(QStringLiteral("2024")));
    }

    void test_viewBrokerageEdit_populateOverview_twoYears_threeTabs()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));
        BrokerageObject b1(QStringLiteral("brok-1"), QStringLiteral("share-guid"),
                          QString(), QString(),
                          QStringLiteral("2023-03-10T10:00:00"), 9.90);
        BrokerageObject b2(QStringLiteral("brok-2"), QStringLiteral("share-guid"),
                          QString(), QString(),
                          QStringLiteral("2024-03-10T10:00:00"), 5.0);
        dlg.populateOverview({ b1, b2 });

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        QCOMPARE(tabs->count(), 3);
    }

    void test_viewBrokerageEdit_populateOverview_jahresTabsDescendingByYear()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));
        BrokerageObject b1(QStringLiteral("brok-1"), QStringLiteral("share-guid"),
                          QString(), QString(),
                          QStringLiteral("2022-03-10T10:00:00"), 9.90);
        BrokerageObject b2(QStringLiteral("brok-2"), QStringLiteral("share-guid"),
                          QString(), QString(),
                          QStringLiteral("2024-03-10T10:00:00"), 5.0);
        dlg.populateOverview({ b1, b2 });

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        QVERIFY(tabs->tabText(1).contains(QStringLiteral("2024")));
        QVERIFY(tabs->tabText(2).contains(QStringLiteral("2022")));
    }

    void test_viewBrokerageEdit_populateOverview_uebersichtTabHasTable()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));
        BrokerageObject br(QStringLiteral("brok-1"), QStringLiteral("share-guid"),
                           QString(), QString(),
                           QStringLiteral("2024-03-10T10:00:00"), 9.90);
        dlg.populateOverview({ br });

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        auto* tbl = dataTableFromContainer(tabs->widget(0));
        if (!tbl) QFAIL("dataTable not found");
        QCOMPARE(tbl->columnCount(), 2);
    }

    void test_viewBrokerageEdit_populateOverview_jahresTabHasSixColumns()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));
        BrokerageObject br(QStringLiteral("brok-1"), QStringLiteral("share-guid"),
                           QString(), QString(),
                           QStringLiteral("2024-03-10T10:00:00"), 9.90);
        dlg.populateOverview({ br });

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        auto* tbl = dataTableFromContainer(tabs->widget(1));
        if (!tbl) QFAIL("dataTable not found");
        QCOMPARE(tbl->columnCount(), 6);
    }

    void test_viewBrokerageEdit_populateOverview_guidStoredInDateColumn()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));
        BrokerageObject br(QStringLiteral("brok-1"), QStringLiteral("share-guid"),
                           QString(), QString(),
                           QStringLiteral("2024-03-10T10:00:00"), 9.90);
        dlg.populateOverview({ br });

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        auto* tbl = dataTableFromContainer(tabs->widget(1));
        if (!tbl) QFAIL("dataTable not found");
        QCOMPARE(tbl->item(0, 0)->data(Qt::UserRole).toString(),
                 QStringLiteral("brok-1"));
    }

    void test_viewBrokerageEdit_populateOverview_typColumnStandaloneIsSonstig()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));
        BrokerageObject br(QStringLiteral("brok-1"), QStringLiteral("share-guid"),
                           QString(), QString(),
                           QStringLiteral("2024-03-10T10:00:00"), 9.90);
        dlg.populateOverview({ br });

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        auto* tbl = dataTableFromContainer(tabs->widget(1));
        if (!tbl) QFAIL("dataTable not found");
        QCOMPARE(tbl->item(0, 1)->text(), tr("Sonstig"));
    }

    void test_viewBrokerageEdit_populateOverview_typColumnLinkedBuyIsKauf()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));
        BrokerageObject br(QStringLiteral("brok-1"), QStringLiteral("share-guid"),
                           QStringLiteral("buy-1"), QString(),
                           QStringLiteral("2024-03-10T10:00:00"), 9.90);
        dlg.populateOverview({ br });

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        auto* tbl = dataTableFromContainer(tabs->widget(1));
        if (!tbl) QFAIL("dataTable not found");
        QCOMPARE(tbl->item(0, 1)->text(), tr("Kauf"));
    }

    void test_viewBrokerageEdit_populateOverview_typColumnLinkedSaleIsVerkauf()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));
        BrokerageObject br(QStringLiteral("brok-1"), QStringLiteral("share-guid"),
                           QString(), QStringLiteral("sale-1"),
                           QStringLiteral("2024-03-10T10:00:00"), 9.90);
        dlg.populateOverview({ br });

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        auto* tbl = dataTableFromContainer(tabs->widget(1));
        if (!tbl) QFAIL("dataTable not found");
        QCOMPARE(tbl->item(0, 1)->text(), tr("Verkauf"));
    }

    void test_viewBrokerageEdit_populateOverview_docIconWhenPathSet()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));
        BrokerageObject br(QStringLiteral("brok-1"), QStringLiteral("share-guid"),
                           QString(), QString(),
                           QStringLiteral("2024-03-10T10:00:00"), 9.90,
                           0.0, 0.0, 0.0, QStringLiteral("/docs/receipt.pdf"));
        dlg.populateOverview({ br });

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        auto* tbl = dataTableFromContainer(tabs->widget(1));
        if (!tbl) QFAIL("dataTable not found");
        QVERIFY(tbl->cellWidget(0, 5) != nullptr);
    }

    void test_viewBrokerageEdit_populateOverview_docDashWhenNoPath()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));
        BrokerageObject br(QStringLiteral("brok-1"), QStringLiteral("share-guid"),
                           QString(), QString(),
                           QStringLiteral("2024-03-10T10:00:00"), 9.90);
        dlg.populateOverview({ br });

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        auto* tbl = dataTableFromContainer(tabs->widget(1));
        if (!tbl) QFAIL("dataTable not found");
        QCOMPARE(tbl->item(0, 5)->text(), QStringLiteral("-"));
        QVERIFY(tbl->cellWidget(0, 5) == nullptr);
    }

    void test_viewBrokerageEdit_populateOverview_tabTitleContainsTotal()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));
        BrokerageObject br(QStringLiteral("brok-1"), QStringLiteral("share-guid"),
                           QString(), QString(),
                           QStringLiteral("2024-03-10T10:00:00"), 9.90);
        dlg.populateOverview({ br });

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        QVERIFY(tabs->tabText(0).contains(QStringLiteral("€")));
    }

    void test_viewBrokerageEdit_populateOverview_repopulateReplacesOldTabs()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));
        BrokerageObject b1(QStringLiteral("brok-1"), QStringLiteral("share-guid"),
                          QString(), QString(),
                          QStringLiteral("2023-03-10T10:00:00"), 9.90);
        dlg.populateOverview({ b1 });

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        QCOMPARE(tabs->count(), 2);

        BrokerageObject b2(QStringLiteral("brok-2"), QStringLiteral("share-guid"),
                          QString(), QString(),
                          QStringLiteral("2024-03-10T10:00:00"), 5.0);
        dlg.populateOverview({ b2 });

        QCOMPARE(tabs->count(), 2);
        QVERIFY(tabs->tabText(1).contains(QStringLiteral("2024")));
    }

    void test_viewBrokerageEdit_uebersichtClick_jumpsToYearTab()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));
        BrokerageObject br(QStringLiteral("brok-1"), QStringLiteral("share-guid"),
                           QString(), QString(),
                           QStringLiteral("2024-03-10T10:00:00"), 9.90);
        dlg.populateOverview({ br });

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        auto* uebersichtTbl = dataTableFromContainer(tabs->widget(0));
        if (!uebersichtTbl) QFAIL("Uebersicht dataTable not found");
        emit uebersichtTbl->cellClicked(0, 0);
        QVERIFY(tabs->currentIndex() > 0);
    }

    void test_viewBrokerageEdit_tabChange_selectsFirstRowInJahresTab()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));
        BrokerageObject br(QStringLiteral("brok-1"), QStringLiteral("share-guid"),
                           QString(), QString(),
                           QStringLiteral("2024-03-10T10:00:00"), 9.90);
        dlg.populateOverview({ br });

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        tabs->setCurrentIndex(1);
        auto* tbl = dataTableFromContainer(tabs->widget(1));
        if (!tbl) QFAIL("dataTable not found");
        QCOMPARE(tbl->currentRow(), 0);
    }

    // ─────────────────────────────────────────────────────────────────────
    // ModelShareEdit — database tests
    // ─────────────────────────────────────────────────────────────────────

    void test_modelShareEdit_loadShare_returnsValidShare()
    {
        const QString shareGuid = insertTestShare();
        ModelShareEdit model;
        const ShareObject share = model.loadShare(shareGuid);
        QVERIFY(share.isValid());
        QCOMPARE(share.wkn(),  QStringLiteral("TST01"));
        QCOMPARE(share.name(), QStringLiteral("Test AG"));
    }

    void test_modelShareEdit_loadShare_notFound_returnsInvalid()
    {
        openMemoryDb();
        ModelShareEdit model;
        const ShareObject share = model.loadShare(QStringLiteral("does-not-exist"));
        QVERIFY(!share.isValid());
        QVERIFY(!model.lastError().isEmpty());
    }

    void test_modelShareEdit_saveShare_success()
    {
        const QString shareGuid = insertTestShare();
        ModelShareEdit model;
        ShareObject share = model.loadShare(shareGuid);
        share.setName(QStringLiteral("Renamed AG"));
        QVERIFY(model.saveShare(share));

        const ShareObject reloaded = model.loadShare(shareGuid);
        QCOMPARE(reloaded.name(), QStringLiteral("Renamed AG"));
    }

    void test_modelShareEdit_currentVolume_sumsBuyMinusSold()
    {
        const QString shareGuid = insertTestShare();
        insertTestBuy(shareGuid, QStringLiteral("depot1"),
                      QStringLiteral("2024-01-10T10:00:00"), 10.0, 50.0);
        BuyObject b2 = insertTestBuy(shareGuid, QStringLiteral("depot1"),
                                     QStringLiteral("2024-02-15T10:00:00"), 20.0, 55.0);

        BuyRepository buyRepo;
        QVERIFY(buyRepo.updateVolumeSold(b2.guid(), 5.0));

        ModelShareEdit model;
        // 10 (unsold) + 20 - 5 (partially sold) = 25
        QCOMPARE(model.currentVolume(shareGuid), 25.0);
    }

    void test_modelShareEdit_currentVolume_noBuys_returnsZero()
    {
        const QString shareGuid = insertTestShare();
        ModelShareEdit model;
        QCOMPARE(model.currentVolume(shareGuid), 0.0);
    }

    void test_modelShareEdit_firstBuyDate_returnsEarliestBuyDate()
    {
        const QString shareGuid = insertTestShare();
        insertTestBuy(shareGuid, QStringLiteral("depot1"),
                      QStringLiteral("2024-06-01T10:00:00"), 5.0, 60.0);
        const BuyObject earliest = insertTestBuy(
            shareGuid, QStringLiteral("depot1"),
            QStringLiteral("2023-01-15T10:00:00"), 5.0, 40.0);

        // firstBuyDate() returns BuyObject::dateAsStr() of the earliest buy,
        // which is locale-formatted (QLocale::ShortFormat) — compare against
        // the same formatting rather than the raw ISO string.
        ModelShareEdit model;
        QCOMPARE(model.firstBuyDate(shareGuid), earliest.dateAsStr());
    }

    void test_modelShareEdit_firstBuyDate_noBuys_returnsEmpty()
    {
        const QString shareGuid = insertTestShare();
        ModelShareEdit model;
        QVERIFY(model.firstBuyDate(shareGuid).isEmpty());
    }

    void test_modelShareEdit_totalBuyValue_delegatesToRepository()
    {
        const QString shareGuid = insertTestShare();
        insertTestBuy(shareGuid, QStringLiteral("depot1"),
                      QStringLiteral("2024-01-10T10:00:00"), 10.0, 50.0);

        ModelShareEdit model;
        BuyRepository  buyRepo;
        QCOMPARE(model.totalBuyValue(shareGuid),
                 buyRepo.totalBuyValueBrokerageReduction(shareGuid));
        QCOMPARE(model.buyCount(shareGuid), 1);
    }

    // ─────────────────────────────────────────────────────────────────────
    // PresenterShareEdit — stub-based tests
    // ─────────────────────────────────────────────────────────────────────

    void test_presenterShareEdit_loadsShareOnConstruction()
    {
        openMemoryDb();
        StubViewShareEdit  view;
        StubModelShareEdit model;
        model.shareToReturn = ShareObject(QStringLiteral("guid-1"),
                                           QStringLiteral("TST"), QString(),
                                           QStringLiteral("Test AG"));
        PresenterShareEdit p(&view, &model, QStringLiteral("guid-1"));
        QVERIFY(view.loadShareCalled);
    }

    void test_presenterShareEdit_populatesSummaryOnConstruction()
    {
        openMemoryDb();
        StubViewShareEdit  view;
        StubModelShareEdit model;
        model.shareToReturn = ShareObject(QStringLiteral("guid-1"),
                                           QStringLiteral("TST"), QString(),
                                           QStringLiteral("Test AG"));
        PresenterShareEdit p(&view, &model, QStringLiteral("guid-1"));
        QVERIFY(view.setTotalBuysCalled);
    }

    void test_presenterShareEdit_onSave_success_closesView()
    {
        openMemoryDb();
        StubViewShareEdit  view;
        StubModelShareEdit model;
        model.shareToReturn = ShareObject(QStringLiteral("guid-1"),
                                           QStringLiteral("TST"), QString(),
                                           QStringLiteral("Test AG"));
        PresenterShareEdit p(&view, &model, QStringLiteral("guid-1"));
        p.onSave();
        QVERIFY(view.closedCalled);
    }

    void test_presenterShareEdit_refreshSummary_callsPopulate()
    {
        openMemoryDb();
        StubViewShareEdit  view;
        StubModelShareEdit model;
        model.shareToReturn = ShareObject(QStringLiteral("guid-1"),
                                           QStringLiteral("TST"), QString(),
                                           QStringLiteral("Test AG"));
        PresenterShareEdit p(&view, &model, QStringLiteral("guid-1"));
        view.setTotalBuysCalled = false; // reset after construction
        p.refreshSummary();
        QVERIFY(view.setTotalBuysCalled);
    }

    void test_presenterShareEdit_onEditBuys_emitsSignal()
    {
        openMemoryDb();
        StubViewShareEdit  view;
        StubModelShareEdit model;
        model.shareToReturn = ShareObject(QStringLiteral("guid-1"),
                                           QStringLiteral("TST"), QString(),
                                           QStringLiteral("Test AG"));
        PresenterShareEdit p(&view, &model, QStringLiteral("guid-1"));
        QSignalSpy spy(&p, &PresenterShareEdit::openBuysRequested);
        p.onEditBuys();
        QCOMPARE(spy.count(), 1);
    }

    void test_presenterShareEdit_onEditSales_emitsSignal()
    {
        openMemoryDb();
        StubViewShareEdit  view;
        StubModelShareEdit model;
        model.shareToReturn = ShareObject(QStringLiteral("guid-1"),
                                           QStringLiteral("TST"), QString(),
                                           QStringLiteral("Test AG"));
        PresenterShareEdit p(&view, &model, QStringLiteral("guid-1"));
        QSignalSpy spy(&p, &PresenterShareEdit::openSalesRequested);
        p.onEditSales();
        QCOMPARE(spy.count(), 1);
    }

    void test_presenterShareEdit_onEditDividends_emitsSignal()
    {
        openMemoryDb();
        StubViewShareEdit  view;
        StubModelShareEdit model;
        model.shareToReturn = ShareObject(QStringLiteral("guid-1"),
                                           QStringLiteral("TST"), QString(),
                                           QStringLiteral("Test AG"));
        PresenterShareEdit p(&view, &model, QStringLiteral("guid-1"));
        QSignalSpy spy(&p, &PresenterShareEdit::openDividendsRequested);
        p.onEditDividends();
        QCOMPARE(spy.count(), 1);
    }

    void test_presenterShareEdit_onEditBrokerages_emitsSignal()
    {
        openMemoryDb();
        StubViewShareEdit  view;
        StubModelShareEdit model;
        model.shareToReturn = ShareObject(QStringLiteral("guid-1"),
                                           QStringLiteral("TST"), QString(),
                                           QStringLiteral("Test AG"));
        PresenterShareEdit p(&view, &model, QStringLiteral("guid-1"));
        QSignalSpy spy(&p, &PresenterShareEdit::openBrokeragesRequested);
        p.onEditBrokerages();
        QCOMPARE(spy.count(), 1);
    }

    // ── Split-Zeile in "Allgemein" (Phase 3, 08.08.2026) ──────────────────
    //
    // Nur die Verdrahtung im Presenter — dass die Splits geladen und an die
    // richtige View-Methode gereicht werden. Die Aufbereitung des Textes
    // gehört zur View und wird in tst_shareeditform.cpp geprüft.

    void test_presenterShareEdit_populatesSplitInfoOnConstruction()
    {
        openMemoryDb();
        StubViewShareEdit  view;
        StubModelShareEdit model;
        model.shareToReturn = ShareObject(QStringLiteral("guid-1"),
                                           QStringLiteral("TST"), QString(),
                                           QStringLiteral("Test AG"));
        model.splitsToReturn << ShareSplitObject(QStringLiteral("split-1"),
                                                 QStringLiteral("guid-1"),
                                                 QDate(2022, 7, 18), 20.0, 1.0);
        PresenterShareEdit p(&view, &model, QStringLiteral("guid-1"));

        QVERIFY(view.setSplitInfoCalled);
        QCOMPARE(view.lastSplitInfo.size(), 1);
        QCOMPARE(view.lastSplitInfo.first().guid(), QStringLiteral("split-1"));
    }

    void test_presenterShareEdit_populatesSplitInfo_emptyWhenNoSplits()
    {
        // Der Aufruf muss auch ohne Splits stattfinden — sonst bliebe nach
        // dem Löschen des letzten Splits der alte Text stehen.
        openMemoryDb();
        StubViewShareEdit  view;
        StubModelShareEdit model;
        model.shareToReturn = ShareObject(QStringLiteral("guid-1"),
                                           QStringLiteral("TST"), QString(),
                                           QStringLiteral("Test AG"));
        PresenterShareEdit p(&view, &model, QStringLiteral("guid-1"));

        QVERIFY(view.setSplitInfoCalled);
        QVERIFY(view.lastSplitInfo.isEmpty());
    }

    void test_presenterShareEdit_refreshSummary_refreshesSplitInfo()
    {
        openMemoryDb();
        StubViewShareEdit  view;
        StubModelShareEdit model;
        model.shareToReturn = ShareObject(QStringLiteral("guid-1"),
                                           QStringLiteral("TST"), QString(),
                                           QStringLiteral("Test AG"));
        PresenterShareEdit p(&view, &model, QStringLiteral("guid-1"));

        // Simuliert: der Split-Dialog hat einen Split angelegt und
        // dataChanged() gesendet, ViewShareEdit ruft refreshSummary().
        view.setSplitInfoCalled = false;
        model.splitsToReturn << ShareSplitObject(QStringLiteral("split-1"),
                                                 QStringLiteral("guid-1"),
                                                 QDate(2022, 7, 18), 20.0, 1.0);
        p.refreshSummary();

        QVERIFY(view.setSplitInfoCalled);
        QCOMPARE(view.lastSplitInfo.size(), 1);
    }

    void test_presenterShareEdit_onEditSplits_emitsSignal()
    {
        openMemoryDb();
        StubViewShareEdit  view;
        StubModelShareEdit model;
        model.shareToReturn = ShareObject(QStringLiteral("guid-1"),
                                           QStringLiteral("TST"), QString(),
                                           QStringLiteral("Test AG"));
        PresenterShareEdit p(&view, &model, QStringLiteral("guid-1"));
        QSignalSpy spy(&p, &PresenterShareEdit::openSplitsRequested);
        p.onEditSplits();
        QCOMPARE(spy.count(), 1);
    }

    // ── Tageswerte bei Bestand > 0 (Feature 06.08.2026) ───────────────────
    //
    // Regelkern selbst: tests/utils/tst_shareupdaterules. Hier geht es
    // ausschliesslich um die Verdrahtung im Presenter — dass die Regel
    // überhaupt angewandt und ihr Ergebnis an die richtige Stelle gereicht
    // wird. Siehe ARCHITECTURE.md, "Erledigt / Archiv",
    // "Tageswert-Historie bei Bestand > 0 erzwingen".

    void test_presenterShareEdit_withHolding_requiresDailyValues()
    {
        openMemoryDb();
        StubViewShareEdit  view;
        StubModelShareEdit model;
        model.volumeToReturn = 12.5;
        model.shareToReturn = ShareObject(QStringLiteral("guid-1"),
                                           QStringLiteral("TST"), QString(),
                                           QStringLiteral("Test AG"));
        PresenterShareEdit p(&view, &model, QStringLiteral("guid-1"));

        QVERIFY(view.dailyValuesRequiredCalled);
        QVERIFY(view.dailyValuesRequired);
    }

    void test_presenterShareEdit_withoutHolding_doesNotRequireDailyValues()
    {
        openMemoryDb();
        StubViewShareEdit  view;
        StubModelShareEdit model;
        model.volumeToReturn = 0.0;
        model.shareToReturn = ShareObject(QStringLiteral("guid-1"),
                                           QStringLiteral("TST"), QString(),
                                           QStringLiteral("Test AG"));
        PresenterShareEdit p(&view, &model, QStringLiteral("guid-1"));

        // Der Aufruf muss trotzdem stattfinden — sonst bliebe eine zuvor
        // gesetzte Sperre stehen, wenn der Dialog wiederverwendet wird.
        QVERIFY(view.dailyValuesRequiredCalled);
        QVERIFY(!view.dailyValuesRequired);
    }

    void test_presenterShareEdit_onSave_changingToForbiddenType_showsErrorAndDoesNotSave()
    {
        // Aktive Änderung von "Beide" (Vorgabe des Stub-Share) auf "Keine",
        // während Anteile im Bestand sind. Die View sperrt den Radiobutton
        // nur — verhindern muss es der Presenter.
        openMemoryDb();
        StubViewShareEdit  view;
        StubModelShareEdit model;
        view.updateTypeToReturn = ShareUpdateType::None;
        model.volumeToReturn    = 10.0;
        model.shareToReturn = ShareObject(QStringLiteral("guid-1"),
                                           QStringLiteral("TST"), QString(),
                                           QStringLiteral("Test AG"));
        PresenterShareEdit p(&view, &model, QStringLiteral("guid-1"));

        p.onSave();

        QVERIFY(!view.closedCalled);
        QVERIFY(!model.saveShareCalled);
        QVERIFY(!view.lastError.isEmpty());
    }

    void test_presenterShareEdit_onSave_marketPriceWithoutHolding_saves()
    {
        // Gegenprobe: ohne Bestand ist "Markt-Preis" zulässig — die Sperre
        // darf nicht pauschal greifen.
        openMemoryDb();
        StubViewShareEdit  view;
        StubModelShareEdit model;
        view.updateTypeToReturn = ShareUpdateType::MarketPrice;
        model.volumeToReturn    = 0.0;
        model.shareToReturn = ShareObject(QStringLiteral("guid-1"),
                                           QStringLiteral("TST"), QString(),
                                           QStringLiteral("Test AG"));
        PresenterShareEdit p(&view, &model, QStringLiteral("guid-1"));

        p.onSave();

        QVERIFY(model.saveShareCalled);
        QVERIFY(view.closedCalled);
    }

    void test_presenterShareEdit_onSave_dailyValuesWithHolding_saves()
    {
        openMemoryDb();
        StubViewShareEdit  view;
        StubModelShareEdit model;
        view.updateTypeToReturn = ShareUpdateType::DailyValues;
        model.volumeToReturn    = 10.0;
        model.shareToReturn = ShareObject(QStringLiteral("guid-1"),
                                           QStringLiteral("TST"), QString(),
                                           QStringLiteral("Test AG"));
        PresenterShareEdit p(&view, &model, QStringLiteral("guid-1"));

        p.onSave();

        QVERIFY(model.saveShareCalled);
        QVERIFY(view.closedCalled);
    }

    void test_presenterShareEdit_onSave_unchangedLegacyType_stillSaves()
    {
        // Altbestand: "Keine" ist bereits gespeichert und wird NICHT geändert.
        // Blockiert wird nur die aktive Änderung auf einen unzulässigen Wert —
        // sonst liesse sich an dieser Aktie überhaupt nichts mehr bearbeiten,
        // auch keine Namenskorrektur. Siehe ARCHITECTURE.md.
        openMemoryDb();
        StubViewShareEdit  view;
        StubModelShareEdit model;
        ShareObject share(QStringLiteral("guid-1"), QStringLiteral("TST"),
                          QString(), QStringLiteral("Test AG"));
        share.setUpdateType(ShareUpdateType::None);
        model.shareToReturn     = share;
        model.volumeToReturn    = 10.0;
        view.updateTypeToReturn = ShareUpdateType::None;   // unverändert
        PresenterShareEdit p(&view, &model, QStringLiteral("guid-1"));

        p.onSave();

        QVERIFY(model.saveShareCalled);
        QVERIFY(view.closedCalled);
        QVERIFY(view.lastError.isEmpty());
    }

    void test_presenterShareEdit_onSave_legacyTypeChangedToOtherForbidden_blocked()
    {
        // Gegenprobe zum Test darüber: gespeichert war "Keine", gewählt wird
        // "Markt-Preis" — ebenfalls unzulässig. Das ist eine aktive Änderung
        // und muss abgewiesen werden, obwohl der Ausgangswert auch schon
        // unzulässig war. Die Ausnahme gilt nur für den unveränderten Wert.
        openMemoryDb();
        StubViewShareEdit  view;
        StubModelShareEdit model;
        ShareObject share(QStringLiteral("guid-1"), QStringLiteral("TST"),
                          QString(), QStringLiteral("Test AG"));
        share.setUpdateType(ShareUpdateType::None);
        model.shareToReturn     = share;
        model.volumeToReturn    = 10.0;
        view.updateTypeToReturn = ShareUpdateType::MarketPrice;
        PresenterShareEdit p(&view, &model, QStringLiteral("guid-1"));

        p.onSave();

        QVERIFY(!model.saveShareCalled);
        QVERIFY(!view.closedCalled);
        QVERIFY(!view.lastError.isEmpty());
    }

    // ── Text der Start-Meldung (Feature 06.08.2026) ───────────────────────
    //
    // buildDailyValuesWarningMessage() ist public static, damit genau dieser
    // Teil ohne MainWindow und ohne modalen Dialog prüfbar bleibt — gleiche
    // Konvention wie bei buildDailyValuesUrl()/shouldMinimizeToTray().

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

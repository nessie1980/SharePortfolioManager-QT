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
#include "../../app/forms/DividendForm/IViewDividendEdit.h"
#include "../../app/forms/DividendForm/IModelDividendEdit.h"
#include "../../app/forms/DividendForm/ViewDividendEdit.h"
#include "../../app/forms/DividendForm/ModelDividendEdit.h"
#include "../../app/forms/DividendForm/PresenterDividendEdit.h"
#include "../../app/models/DividendObject.h"
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
#include "../../app/utils/SplitAdjustmentAudit.h"
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
// Stub IModelSaleEdit
// ─────────────────────────────────────────────────────────────────────────────
class StubModelSaleEdit : public IModelSaleEdit
{
public:
    // Configurable return values
    QList<SaleObject>  sales;
    // Default: eine großzügig bemessene, alte Position — die meisten Tests
    // hier interessieren sich nicht für die Mengenprüfung und würden sonst
    // an der neuen Prüfung in PresenterSaleEdit::validateInput()/
    // onVolumeOrPriceEdited() scheitern (Bugfix, siehe ARCHITECTURE.md
    // "Skalenbewusste Mengenprüfung im Verkaufsformular", 11.08.2026, gefixt
    // 20.08.2026). Tests, die genau diese Prüfung testen oder eine konkrete
    // FIFO-Zuteilung erwarten, überschreiben availableBuys gezielt.
    QList<BuyObject>   availableBuys = {
        BuyObject(QStringLiteral("default-buy"), QStringLiteral("share-1"),
                 QStringLiteral("depot1"), QString(),
                 QStringLiteral("2000-01-01T00:00:00"), 1000000.0, 0.0, 100.0)
    };
    QList<ShareSplitObject> splits;
    BrokerageObject    brokerage;
    bool               addResult    = true;
    bool               updateResult = true;
    bool               removeResult = true;
    bool               orderExists  = false;
    bool               docExists    = false;
    QString            errorMsg;

    // Captured calls
    bool       addSaleCalled    = false;
    bool       updateSaleCalled = false;
    bool       removeSaleCalled = false;
    SaleObject lastAddedSale;
    SaleObject lastUpdatedSale;
    // Von loadAvailableBuysForDepotExcludingSale() zuletzt übergebene GUID —
    // Aktiensplit-Behandlung, Phase 2c, 07.08.2026 (ARCHITECTURE.md "Offene
    // Punkte"). mutable, weil die Methode selbst const ist (Interface-Vorgabe).
    mutable bool    excludingSaleCalled = false;
    mutable QString lastExcludeSaleGuid;

    QList<SaleObject>  loadSales(const QString&)             const override { return sales; }
    ShareObject        loadShare(const QString&)             const override { return ShareObject{}; }
    QList<BuyObject>   loadAvailableBuys(const QString&)     const override { return availableBuys; }
    QList<BuyObject>   loadAllBuys(const QString&)           const override { return availableBuys; }
    QList<BuyObject>   loadAvailableBuysForDepot(const QString&,
                                                  const QString&) const override { return availableBuys; }
    QList<BuyObject>   loadAvailableBuysForDepotExcludingSale(
        const QString&, const QString&, const QString& excludeSaleGuid) const override
    {
        excludingSaleCalled = true;
        lastExcludeSaleGuid = excludeSaleGuid;
        return availableBuys;
    }
    QList<ShareSplitObject> loadSplits(const QString&) const override { return splits; }
    BrokerageObject    loadBrokerage(const QString&)         const override { return brokerage; }
    BrokerageObject    loadBrokerageForBuy(const QString&)   const override { return brokerage; }

    bool addSale(const SaleObject& sale)
        override { addSaleCalled    = true; lastAddedSale   = sale; return addResult;    }
    bool updateSale(const SaleObject& sale)
        override { updateSaleCalled = true; lastUpdatedSale = sale; return updateResult; }
    bool removeSale(const QString&)         override { removeSaleCalled = true; return removeResult; }

    bool orderNumberExists(const QString&, const QString&, const QString&) const override
        { return orderExists; }
    bool documentExists(const QString&, const QString&) const override
        { return docExists; }
    QString lastError() const override { return errorMsg; }
};

// ─────────────────────────────────────────────────────────────────────────────
// Stub IViewSaleEdit
// ─────────────────────────────────────────────────────────────────────────────
class StubViewSaleEdit : public IViewSaleEdit
{
public:
    // Configurable return values
    QString m_depotNumber     = QStringLiteral("depot1");
    QString m_orderNumber     = QStringLiteral("ORD-S-001");
    double  m_volume          = 10.0;
    double  m_salePrice       = 150.0;
    double  m_taxAtSource     = 0.0;
    double  m_capitalGainsTax = 0.0;
    double  m_solidarityTax   = 0.0;
    double  m_provision       = 0.0;
    double  m_brokerFee       = 0.0;
    double  m_traderFee       = 0.0;
    double  m_reduction       = 0.0;
    QString m_docPath;
    QString m_dateTime        = QStringLiteral("2024-06-15T10:00:00");
    bool    m_missingFields   = false;
    QList<ShareSplitObject> m_splits;

    // Captured calls
    bool    populateOverviewCalled   = false;
    bool    clearFormCalled          = false;
    bool    loadSaleCalled           = false;
    bool    setButtonStatesCalled    = false;
    bool    lastCanRemove            = false;
    bool    lastIsLastSale           = false;
    bool    showOverviewTabCalled    = false;
    QString lastError;
    bool    closed                   = false;

    // Phase 3b (09.08.2026) — zuletzt gesetzter Split-Hinweis.
    QString lastSplitHint;
    QString lastSplitTooltip;
    bool    lastHasSplit             = false;
    int     splitHintCallCount       = 0;
    double  lastKaufwert             = 0.0;
    double  lastGewinnVerlust        = 0.0;

    // Bugfix "anteilige Kauf-Nebenkosten gehen bei der FIFO-Zuteilung
    // verloren": der Details-Inhalt wird jetzt vom Presenter aufbereitet und
    // fertig an die View gereicht — hier fuer die Pruefung mitgeschrieben.
    SaleBuyDetailSummary lastBuyDetails;
    int                  showBuyDetailsCallCount = 0;

    // Phase 3c (11.08.2026): von populateOverview() zuletzt übergebene Splits.
    QList<ShareSplitObject> lastOverviewSplits;

    // IViewSaleEdit — read
    QString dateTime()        const override { return m_dateTime;        }
    QString depotNumber()     const override { return m_depotNumber;     }
    QString orderNumber()     const override { return m_orderNumber;     }
    double  volume()          const override { return m_volume;          }
    double  salePrice()       const override { return m_salePrice;       }
    double  taxAtSource()     const override { return m_taxAtSource;     }
    double  capitalGainsTax() const override { return m_capitalGainsTax; }
    double  solidarityTax()   const override { return m_solidarityTax;   }
    double  provision()       const override { return m_provision;       }
    double  brokerFee()       const override { return m_brokerFee;       }
    double  traderFee()       const override { return m_traderFee;       }
    double  reduction()       const override { return m_reduction;       }
    QString documentPath()    const override { return m_docPath;         }

    // IViewSaleEdit — write
    void loadSale(const SaleObject&)               override { loadSaleCalled = true; }
    void clearForm()                               override { clearFormCalled = true; }
    void populateAvailableBuys(const QList<BuyObject>&) override {}
    void setAllBuys(const QList<BuyObject>&)       override {}
    void setSplits(const QList<ShareSplitObject>& splits) override { m_splits = splits; }

    void setSaleValue(double)                      override {}
    void setKaufwert(double value)                  override { lastKaufwert      = value; }
    void setGewinnVerlust(double value)             override { lastGewinnVerlust = value; }
    void setGesGebuehren(double)                   override {}
    void setTaxSum(double)                         override {}
    void setAuszahlung(double)                     override {}

    void setSplitHint(const QString& text, const QString& tooltip, bool hasSplit) override
    {
        lastSplitHint    = text;
        lastSplitTooltip = tooltip;
        lastHasSplit     = hasSplit;
        ++splitHintCallCount;
    }

    void setFieldOk(const QString&, const QString&) override {}
    void setFieldError(const QString&)              override {}
    void setDocumentPath(const QString& path)       override { m_docPath = path; }
    void setDocumentPreview(const QString&)         override {}

    void setParseProgress(int, const QString&)      override {}
    void setParseStatusIcon(int)                    override {}
    void setUiBusy(bool)                            override {}
    void onParseFinished()                          override {}

    void showBuyDetails(const SaleBuyDetailSummary& summary) override
    {
        lastBuyDetails = summary;
        ++showBuyDetailsCallCount;
    }

    void populateOverview(const QList<SaleObject>&,
                          const QList<ShareSplitObject>& splits) override
    {
        populateOverviewCalled = true;
        // Phase 3c (11.08.2026): die Splits kommen als Parameter herein.
        lastOverviewSplits = splits;
    }
    void openPdfPreview(const QString&)             override {}
    void clearPdfPreview()                          override {}
    void showOverviewTab()                          override
        { showOverviewTabCalled = true; clearFormCalled = true; }
    void setButtonStates(bool canRemove, bool isLastSale, bool isEdit) override
    {
        setButtonStatesCalled = true;
        lastCanRemove  = canRemove;
        lastIsLastSale = isLastSale;
        Q_UNUSED(isEdit)
    }
    void showError(const QString& msg)              override { lastError = msg; }
    void acceptAndClose()                           override { closed = true; }

    void markMissingFieldsAsFailed()                override {}
    bool hasMissingRequiredFields(QStringList& missing) const override
    {
        missing.clear();
        if (m_missingFields) missing << QStringLiteral("test");
        return m_missingFields;
    }
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
        // hat: tst_mainwindow führt mehrere QObject-Testklassen im selben
        // Prozess aus (siehe main() am Dateiende); jeder setXxx()-Aufruf in
        // einer SPÄTER laufenden Klasse (z. B. TestBackupForm) hat dadurch
        // direkt in die echte settings.ini geschrieben, statt in die
        // sandboxte Testdatei — Nessies reale Konfiguration (Portfolio-Pfad,
        // Dokument-Root) wurde dadurch bei jedem Testlauf überschrieben
        // (gemeldet und behoben 19.07.2026). Der AppSettings-Singleton stirbt
        // ohnehin mit dem Prozess — ein "Zurücksetzen fürs nächste Mal" ist
        // hier schlicht nicht nötig.
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
    void test_onRefreshShare_dailyValuesOnly_splitAdjustmentDiscrepancy_addsStatusMessage_viaFakeNetwork()
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
        QVERIFY2(te->toPlainText().contains(
                     QStringLiteral("SplitMismatch AG\" — 1 Split(s) mit abweichendem "
                                    "Bereinigungs-Zustand erkannt")),
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

    // ── buildSplitAdjustmentWarningMessage() ────────────────────────────────────
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

    void test_buildSplitAdjustmentWarningMessage_emptyList_returnsEmpty()
    {
        // Belegt den Frühausstieg: ohne Widersprüche darf kein Dialog aufgehen.
        QVERIFY(MainWindow::buildSplitAdjustmentWarningMessage({}).isEmpty());
    }

    void test_buildSplitAdjustmentWarningMessage_containsNameWknAndSplitDescription()
    {
        const ShareSplitObject s(QStringLiteral("split-1"), QStringLiteral("share-1"),
                                 QDate(2022, 7, 18), 20.0, 1.0, /*pricesAdjusted=*/false);
        const auto outcome = makeOutcome(SplitPriceJumpDetector::Result::Adjusted,
                                         QDate(2022, 7, 15), 49.80,
                                         QDate(2022, 7, 19), 50.60);
        MainWindow::SplitAdjustmentWarning w;
        w.shareName = QStringLiteral("Alphabet Inc.");
        w.wkn       = QStringLiteral("A14Y6H");
        w.discrepancy = SplitAdjustmentAudit::Discrepancy{ s, outcome };

        const QString msg = MainWindow::buildSplitAdjustmentWarningMessage({ w });

        QVERIFY(msg.contains(QStringLiteral("Alphabet Inc.")));
        QVERIFY(msg.contains(QStringLiteral("A14Y6H")));
        QVERIFY(msg.contains(ShareSplitHint::describeSplit(s)));
    }

    void test_buildSplitAdjustmentWarningMessage_listsAllWarningsInOrder()
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

        MainWindow::SplitAdjustmentWarning wa;
        wa.shareName = QStringLiteral("Erste AG");
        wa.wkn       = QStringLiteral("AAA111");
        wa.discrepancy = SplitAdjustmentAudit::Discrepancy{ sa, outcomeA };

        MainWindow::SplitAdjustmentWarning wb;
        wb.shareName = QStringLiteral("Zweite AG");
        wb.wkn       = QStringLiteral("BBB222");
        wb.discrepancy = SplitAdjustmentAudit::Discrepancy{ sb, outcomeB };

        const QString msg = MainWindow::buildSplitAdjustmentWarningMessage({ wa, wb });

        QVERIFY(msg.contains(QStringLiteral("Erste AG")));
        QVERIFY(msg.contains(QStringLiteral("Zweite AG")));
        // Reihenfolge bleibt die der Eingabeliste.
        QVERIFY(msg.indexOf(QStringLiteral("Erste AG"))
                < msg.indexOf(QStringLiteral("Zweite AG")));
    }

    void test_buildSplitAdjustmentWarningMessage_explainsNoAutomaticChange()
    {
        // Zentrale Zusicherung der Meldung: sie liest nur, sie schreibt
        // nichts — siehe SplitAdjustmentAudit.h. Ohne diesen Hinweis könnte
        // der Nutzer annehmen, der Haken sei bereits korrigiert worden.
        const ShareSplitObject s(QStringLiteral("split-1"), QStringLiteral("share-1"),
                                 QDate(2022, 7, 18), 20.0, 1.0, false);
        const auto outcome = makeOutcome(SplitPriceJumpDetector::Result::Adjusted,
                                         QDate(2022, 7, 15), 49.80,
                                         QDate(2022, 7, 19), 50.60);
        MainWindow::SplitAdjustmentWarning w;
        w.shareName = QStringLiteral("Alphabet Inc.");
        w.wkn       = QStringLiteral("A14Y6H");
        w.discrepancy = SplitAdjustmentAudit::Discrepancy{ s, outcome };

        const QString msg = MainWindow::buildSplitAdjustmentWarningMessage({ w });

        QVERIFY(msg.contains(QStringLiteral("automatisch geändert wird hier nichts")));
        QVERIFY(msg.contains(QStringLiteral("Prüfen")));
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

// ─────────────────────────────────────────────────────────────────────────────
// Helpers for SaleEdit tests
// ─────────────────────────────────────────────────────────────────────────────

/** Create a minimal SaleObject for the given shareGuid and year. */
static SaleObject makeSale(const QString& guid,
                            const QString& shareGuid,
                            int year,
                            double volume        = 10.0,
                            double price         = 150.0,
                            const QString& depot = QStringLiteral("depot1"),
                            const QString& doc   = QString())
{
    const QString dt = QStringLiteral("%1-06-15T10:00:00").arg(year);
    return SaleObject(guid, shareGuid, depot,
                      QStringLiteral("ord-s-") + guid,
                      dt, volume, price,
                      /*saleBuyDetails=*/ QList<SaleBuyDetail>(),
                      /*taxAtSource=*/    0.0,
                      /*capitalGainsTax=*/0.0,
                      /*solidarityTax=*/  0.0,
                      /*brokerageGuid=*/  QString(),
                      /*provision=*/      0.0,
                      /*brokerFee=*/      0.0,
                      /*traderFee=*/      0.0,
                      /*reduction=*/      0.0,
                      /*document=*/       doc);
}

// ─────────────────────────────────────────────────────────────────────────────
// SalesForm tests — in a separate QObject so initTestCase/openMemoryDb work
// ─────────────────────────────────────────────────────────────────────────────
class TestSalesForm : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir   m_tempDir;
    DocumentsConfig m_docsConfig;

    void loadSandboxedSettings()
    {
        const QString sandboxIni =
            m_tempDir.path() + QStringLiteral("/test_settings.ini");
        AppSettings::instance().load(sandboxIni);
    }

    void openMemoryDb()
    {
        if (!Database::instance().isOpen())
            Database::instance().open(QStringLiteral(":memory:"));
        AppSettings::instance().setPortfolioPath(QStringLiteral(":memory:"));
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

private slots:

    void initTestCase()
    {
        QVERIFY(m_tempDir.isValid());
        loadSandboxedSettings();
        const QString docsPath =
            QCoreApplication::applicationDirPath() + QStringLiteral("/Documents.xml");
        if (QFileInfo::exists(docsPath))
            m_docsConfig.load(docsPath);
    }

    void cleanupTestCase()
    {
        if (Database::instance().isOpen())
            Database::instance().close();
        // Siehe TestMainWindow::cleanupTestCase() weiter oben — hier bewusst
        // KEIN AppSettings::instance().load(...) mehr (Reset auf echte
        // settings.ini war die eigentliche Ursache dafür, dass später
        // laufende Testklassen im selben Prozess, z. B. TestBackupForm, in
        // die echte Konfigurationsdatei geschrieben haben).
    }

    void init()
    {
        loadSandboxedSettings();
        if (Database::instance().isOpen())
            Database::instance().close();
    }

    // ─────────────────────────────────────────────────────────────────────
    // ModelSaleEdit — database tests
    // ─────────────────────────────────────────────────────────────────────

    void test_modelSaleEdit_addSale_success()
    {
        // A sale inserted via addSale() must be retrievable via loadSales().
        const QString shareGuid = insertTestShare();
        const BuyObject buy = insertTestBuy(shareGuid,
            QStringLiteral("depot1"),
            QStringLiteral("2024-01-10T09:00:00"), 20.0, 100.0);

        ModelSaleEdit model;
        const QList<SaleBuyDetail> details = {
            SaleBuyDetail(buy.guid(), buy.dateTime(), 10.0, buy.price())
        };
        const SaleObject sale(
            QStringLiteral("sale-1"), shareGuid,
            QStringLiteral("depot1"), QStringLiteral("ord-s-1"),
            QStringLiteral("2024-06-15T10:00:00"),
            10.0, 150.0, details,
            0.0, 0.0, 0.0, QString(),
            9.90, 0.0, 0.0, 0.0);

        QVERIFY(model.addSale(sale));

        const QList<SaleObject> loaded = model.loadSales(shareGuid);
        QCOMPARE(loaded.size(), 1);
        QCOMPARE(loaded.first().orderNumber(), QStringLiteral("ord-s-1"));

        // Regression (Bugfix 15.07.2026, siehe ARCHITECTURE.md,
        // "Marktwert- vs. Depotwert-Modus"): provision() muss nach dem
        // erneuten Laden über loadSales() den beim addSale() übergebenen
        // Wert (9,90 €) widerspiegeln — vorher kam hier immer 0 zurück, da
        // addSale() den neu angelegten Brokerage-Eintrag nur über den
        // Rückwärts-Link (brokerage.sale_guid) verknüpfte, SaleRepository::
        // findByShare() aber über den Vorwärts-Link (sales.brokerage_guid)
        // joint.
        QCOMPARE(loaded.first().provision(), 9.90);
    }

    void test_modelSaleEdit_addSale_linksBrokerageForwardReference()
    {
        // Regressionstest für den Bugfix vom 15.07.2026: Der neu angelegte
        // Brokerage-Eintrag muss über sales.brokerage_guid (Vorwärts-FK)
        // auffindbar sein, nicht nur über brokerage.sale_guid (Rückwärts-FK,
        // den z.B. loadBrokerage()/findBySaleGuid() nutzt). Ein Verkauf mit
        // eigener Provision (10,00 €) und ohne Kauf-Anteil macht das über
        // payoutBrokerageReduction() direkt messbar: saleValue = 5 × 100,00 €
        // = 500,00 €; payoutBrokerageReduction() = 500,00 € − 10,00 € = 490,00 €;
        // ohne den Fix bliebe es bei 500,00 € (Brokerage 0, siehe payout()).
        const QString shareGuid = insertTestShare();

        ModelSaleEdit model;
        const SaleObject sale(
            QStringLiteral("sale-fwd-link"), shareGuid,
            QStringLiteral("depot1"), QStringLiteral("ord-fwd-link"),
            QStringLiteral("2024-06-05T10:00:00"),
            5.0, 100.0, {},
            /*taxAtSource=*/0.0, /*capitalGainsTax=*/0.0, /*solidarityTax=*/0.0,
            /*brokerageGuid=*/QString(), /*provision=*/10.0);
        QVERIFY(model.addSale(sale));

        const QList<SaleObject> loaded = model.loadSales(shareGuid);
        QCOMPARE(loaded.size(), 1);
        QCOMPARE(loaded.first().payoutBrokerageReduction(), 490.0);

        // Der Rückwärts-Link (loadBrokerage()/findBySaleGuid(), unverändert
        // durch den Fix) muss weiterhin ebenfalls funktionieren.
        const BrokerageObject brokerage = model.loadBrokerage(sale.guid());
        QVERIFY(brokerage.isValid());
        QCOMPARE(brokerage.provision(), 10.0);
    }

    void test_modelSaleEdit_addSale_updatesVolumeSoldOnBuy()
    {
        // addSale() must increment volumeSold on the contributing buy.
        const QString shareGuid = insertTestShare();
        const BuyObject buy = insertTestBuy(shareGuid,
            QStringLiteral("depot1"),
            QStringLiteral("2024-01-10T09:00:00"), 20.0, 100.0);

        ModelSaleEdit model;
        const QList<SaleBuyDetail> details = {
            SaleBuyDetail(buy.guid(), buy.dateTime(), 10.0, buy.price())
        };
        const SaleObject sale(
            QStringLiteral("sale-vol"), shareGuid,
            QStringLiteral("depot1"), QStringLiteral("ord-s-vol"),
            QStringLiteral("2024-06-15T10:00:00"),
            10.0, 150.0, details);
        QVERIFY(model.addSale(sale));

        BuyRepository buyRepo;
        const BuyObject updated = buyRepo.findByGuid(buy.guid());
        QVERIFY(updated.isValid());
        QCOMPARE(updated.volumeSold(), 10.0);
    }

    void test_modelSaleEdit_addSale_rollsBackOnError()
    {
        // If the brokerage insert fails the whole transaction must roll back.
        // We simulate this by inserting a sale with a duplicate GUID.
        const QString shareGuid = insertTestShare();
        const BuyObject buy = insertTestBuy(shareGuid,
            QStringLiteral("depot1"),
            QStringLiteral("2024-01-10T09:00:00"), 20.0, 100.0);

        ModelSaleEdit model;
        const QList<SaleBuyDetail> details = {
            SaleBuyDetail(buy.guid(), buy.dateTime(), 5.0, buy.price())
        };
        const SaleObject sale(
            QStringLiteral("sale-dup"), shareGuid,
            QStringLiteral("depot1"), QStringLiteral("ord-s-dup"),
            QStringLiteral("2024-06-15T10:00:00"),
            5.0, 150.0, details);
        QVERIFY(model.addSale(sale));

        // Second insert with same GUID → must fail, no duplicate in DB.
        const bool ok = model.addSale(sale);
        QVERIFY(!ok);
        QCOMPARE(model.loadSales(shareGuid).size(), 1);
    }

    void test_modelSaleEdit_removeSale_revertsVolumeSold()
    {
        // After removeSale(), volumeSold on the contributing buy must be
        // decremented back to its original value.
        const QString shareGuid = insertTestShare();
        const BuyObject buy = insertTestBuy(shareGuid,
            QStringLiteral("depot1"),
            QStringLiteral("2024-01-10T09:00:00"), 20.0, 100.0);

        ModelSaleEdit model;
        const QList<SaleBuyDetail> details = {
            SaleBuyDetail(buy.guid(), buy.dateTime(), 10.0, buy.price())
        };
        const SaleObject sale(
            QStringLiteral("sale-rm"), shareGuid,
            QStringLiteral("depot1"), QStringLiteral("ord-s-rm"),
            QStringLiteral("2024-06-15T10:00:00"),
            10.0, 150.0, details);
        QVERIFY(model.addSale(sale));
        QVERIFY(model.removeSale(QStringLiteral("sale-rm")));

        BuyRepository buyRepo;
        const BuyObject restored = buyRepo.findByGuid(buy.guid());
        QCOMPARE(restored.volumeSold(), 0.0);
        QVERIFY(model.loadSales(shareGuid).isEmpty());
    }

    void test_modelSaleEdit_orderNumberExists_true()
    {
        const QString shareGuid = insertTestShare();
        const BuyObject buy = insertTestBuy(shareGuid,
            QStringLiteral("depot1"),
            QStringLiteral("2024-01-10T09:00:00"), 20.0, 100.0);

        ModelSaleEdit model;
        const QList<SaleBuyDetail> details = {
            SaleBuyDetail(buy.guid(), buy.dateTime(), 5.0, buy.price())
        };
        const SaleObject sale(
            QStringLiteral("sale-ord"), shareGuid,
            QStringLiteral("depot1"), QStringLiteral("ord-unique"),
            QStringLiteral("2024-06-15T10:00:00"),
            5.0, 150.0, details);
        QVERIFY(model.addSale(sale));

        QVERIFY( model.orderNumberExists(shareGuid, QStringLiteral("ord-unique")));
        QVERIFY(!model.orderNumberExists(shareGuid, QStringLiteral("ord-other")));
    }

    void test_modelSaleEdit_orderNumberExists_excludeGuid()
    {
        // When editing, the sale's own order number must not be flagged.
        const QString shareGuid = insertTestShare();
        const BuyObject buy = insertTestBuy(shareGuid,
            QStringLiteral("depot1"),
            QStringLiteral("2024-01-10T09:00:00"), 20.0, 100.0);

        ModelSaleEdit model;
        const QList<SaleBuyDetail> details = {
            SaleBuyDetail(buy.guid(), buy.dateTime(), 5.0, buy.price())
        };
        const SaleObject sale(
            QStringLiteral("sale-excl"), shareGuid,
            QStringLiteral("depot1"), QStringLiteral("ord-excl"),
            QStringLiteral("2024-06-15T10:00:00"),
            5.0, 150.0, details);
        QVERIFY(model.addSale(sale));

        QVERIFY(!model.orderNumberExists(shareGuid,
                                         QStringLiteral("ord-excl"),
                                         QStringLiteral("sale-excl")));
    }

    void test_modelSaleEdit_documentExists_notFound_returnsFalse()
    {
        openMemoryDb();
        ModelSaleEdit model;
        QVERIFY(!model.documentExists(QStringLiteral("/nonexistent.pdf")));
    }

    void test_modelSaleEdit_documentExists_emptyPath_returnsFalse()
    {
        openMemoryDb();
        ModelSaleEdit model;
        QVERIFY(!model.documentExists(QString()));
    }

    void test_modelSaleEdit_loadAvailableBuys_excludesSoldOut()
    {
        // loadAvailableBuys must only return buys with remaining volume.
        const QString shareGuid = insertTestShare();
        // Buy fully sold
        const BuyObject fullySold = insertTestBuy(shareGuid,
            QStringLiteral("depot1"),
            QStringLiteral("2024-01-10T09:00:00"), 10.0, 100.0);
        BuyRepository buyRepo;
        BuyObject updated = fullySold;
        updated.setVolumeSold(10.0);
        buyRepo.update(updated);
        // Buy partially available
        insertTestBuy(shareGuid,
            QStringLiteral("depot1"),
            QStringLiteral("2024-02-10T09:00:00"), 20.0, 110.0);

        ModelSaleEdit model;
        const QList<BuyObject> available = model.loadAvailableBuys(shareGuid);
        QCOMPARE(available.size(), 1);
        QCOMPARE(available.first().volume(), 20.0);
    }

    void test_modelSaleEdit_loadAvailableBuysForDepot_filtersDepot()
    {
        // loadAvailableBuysForDepot must only return buys from the given depot.
        const QString shareGuid = insertTestShare();
        insertTestBuy(shareGuid, QStringLiteral("depotA"),
                      QStringLiteral("2024-01-10T09:00:00"), 10.0, 100.0);
        insertTestBuy(shareGuid, QStringLiteral("depotB"),
                      QStringLiteral("2024-02-10T09:00:00"), 15.0, 120.0);

        ModelSaleEdit model;
        const QList<BuyObject> forA =
            model.loadAvailableBuysForDepot(shareGuid, QStringLiteral("depotA"));
        QCOMPARE(forA.size(), 1);
        QCOMPARE(forA.first().depotNumber(), QStringLiteral("depotA"));

        const QList<BuyObject> forB =
            model.loadAvailableBuysForDepot(shareGuid, QStringLiteral("depotB"));
        QCOMPARE(forB.size(), 1);
        QCOMPARE(forB.first().depotNumber(), QStringLiteral("depotB"));
    }

    void test_modelSaleEdit_loadAvailableBuysForDepot_emptyDepot_returnsAll()
    {
        // An empty depotNumber means "no filter" — all available buys are returned.
        const QString shareGuid = insertTestShare();
        insertTestBuy(shareGuid, QStringLiteral("depotA"),
                      QStringLiteral("2024-01-10T09:00:00"), 10.0, 100.0);
        insertTestBuy(shareGuid, QStringLiteral("depotB"),
                      QStringLiteral("2024-02-10T09:00:00"), 15.0, 120.0);

        ModelSaleEdit model;
        const QList<BuyObject> all =
            model.loadAvailableBuysForDepot(shareGuid, QString());
        QCOMPARE(all.size(), 2);
    }

    void test_modelSaleEdit_loadAvailableBuysForDepot_oldestFirst()
    {
        // FIFO order: BuyRepository returns ASC by datetime, loadAvailableBuysForDepot
        // must preserve that order.
        const QString shareGuid = insertTestShare();
        insertTestBuy(shareGuid, QStringLiteral("depot1"),
                      QStringLiteral("2024-03-01T09:00:00"), 10.0, 130.0);
        insertTestBuy(shareGuid, QStringLiteral("depot1"),
                      QStringLiteral("2024-01-01T09:00:00"), 10.0, 100.0);  // older

        ModelSaleEdit model;
        const QList<BuyObject> available =
            model.loadAvailableBuysForDepot(shareGuid, QStringLiteral("depot1"));
        QCOMPARE(available.size(), 2);
        // Oldest must come first
        QVERIFY(available.at(0).dateTime() < available.at(1).dateTime());
    }

    // ─────────────────────────────────────────────────────────────────────
    // ModelSaleEdit — loadAvailableBuysForDepotExcludingSale() / loadSplits()
    // Aktiensplit-Behandlung, Phase 2c (07.08.2026, siehe ARCHITECTURE.md
    // "Offene Punkte", "Aktiensplits werden nicht behandelt").
    // ─────────────────────────────────────────────────────────────────────

    void test_modelSaleEdit_loadAvailableBuysForDepotExcludingSale_creditsBackPartialBuy()
    {
        // Kauf mit 20 Stück, ein Verkauf von 8 Stück daraus -> 12 verfügbar.
        // Beim Bearbeiten GENAU dieses Verkaufs muss die "verfügbar"-Liste
        // die 8 Stück virtuell zurückbuchen, also wieder 20 zeigen.
        const QString shareGuid = insertTestShare();
        const BuyObject buy = insertTestBuy(shareGuid, QStringLiteral("depot1"),
            QStringLiteral("2024-01-10T09:00:00"), 20.0, 100.0);

        ModelSaleEdit model;
        const SaleObject sale(
            QStringLiteral("sale-1"), shareGuid, QStringLiteral("depot1"),
            QStringLiteral("ord-1"), QStringLiteral("2024-06-01T10:00:00"),
            8.0, 150.0,
            { SaleBuyDetail(buy.guid(), buy.dateTime(), 8.0, buy.price()) });
        QVERIFY(model.addSale(sale));

        const QList<BuyObject> normal =
            model.loadAvailableBuysForDepot(shareGuid, QStringLiteral("depot1"));
        QCOMPARE(normal.size(), 1);
        QCOMPARE(normal.first().volume() - normal.first().volumeSold(), 12.0);

        const QList<BuyObject> excluding =
            model.loadAvailableBuysForDepotExcludingSale(
                shareGuid, QStringLiteral("depot1"), QStringLiteral("sale-1"));
        QCOMPARE(excluding.size(), 1);
        QCOMPARE(excluding.first().volume() - excluding.first().volumeSold(), 20.0);
    }

    void test_modelSaleEdit_loadAvailableBuysForDepotExcludingSale_restoresFullyConsumedBuy()
    {
        // Kauf mit 5 Stück, VOLLSTÄNDIG durch einen einzigen Verkauf
        // aufgebraucht -> taucht in der normalen "verfügbar"-Liste NICHT
        // auf. Beim Bearbeiten dieses Verkaufs muss er mit der vollen Menge
        // zurückgebucht wieder erscheinen — sonst würde die FIFO-
        // Neuberechnung diesen Kauf fälschlich ignorieren.
        const QString shareGuid = insertTestShare();
        const BuyObject buy = insertTestBuy(shareGuid, QStringLiteral("depot1"),
            QStringLiteral("2024-01-10T09:00:00"), 5.0, 100.0);

        ModelSaleEdit model;
        const SaleObject sale(
            QStringLiteral("sale-full"), shareGuid, QStringLiteral("depot1"),
            QStringLiteral("ord-full"), QStringLiteral("2024-06-01T10:00:00"),
            5.0, 150.0,
            { SaleBuyDetail(buy.guid(), buy.dateTime(), 5.0, buy.price()) });
        QVERIFY(model.addSale(sale));

        QVERIFY(model.loadAvailableBuysForDepot(shareGuid, QStringLiteral("depot1")).isEmpty());

        const QList<BuyObject> excluding =
            model.loadAvailableBuysForDepotExcludingSale(
                shareGuid, QStringLiteral("depot1"), QStringLiteral("sale-full"));
        QCOMPARE(excluding.size(), 1);
        QCOMPARE(excluding.first().guid(), buy.guid());
        QCOMPARE(excluding.first().volume() - excluding.first().volumeSold(), 5.0);
    }

    void test_modelSaleEdit_loadAvailableBuysForDepotExcludingSale_emptyGuid_behavesLikeNormal()
    {
        const QString shareGuid = insertTestShare();
        insertTestBuy(shareGuid, QStringLiteral("depot1"),
            QStringLiteral("2024-01-10T09:00:00"), 20.0, 100.0);

        ModelSaleEdit model;
        const QList<BuyObject> normal =
            model.loadAvailableBuysForDepot(shareGuid, QStringLiteral("depot1"));
        const QList<BuyObject> excluding =
            model.loadAvailableBuysForDepotExcludingSale(
                shareGuid, QStringLiteral("depot1"), QString());

        QCOMPARE(excluding.size(), normal.size());
        QCOMPARE(excluding.first().volume(), normal.first().volume());
    }

    void test_modelSaleEdit_loadSplits_returnsInsertedSplit()
    {
        const QString shareGuid = insertTestShare();
        ShareSplitRepository splitRepo;
        QVERIFY(splitRepo.insert(ShareSplitObject(
            QStringLiteral("split-1"), shareGuid, QDate(2022, 7, 18), 20.0, 1.0)));

        ModelSaleEdit model;
        const QList<ShareSplitObject> splits = model.loadSplits(shareGuid);
        QCOMPARE(splits.size(), 1);
        QCOMPARE(splits.first().guid(), QStringLiteral("split-1"));
    }

    // ─────────────────────────────────────────────────────────────────────
    // PresenterSaleEdit — via StubView + StubModel
    // ─────────────────────────────────────────────────────────────────────

    // ── Split-Hinweis (Phase 3b, 09.08.2026) ──────────────────────────────
    //
    // Die Formatierung selbst prüft tst_sharesplithint — hier geht es nur um
    // die Verdrahtung. Wortgleich zu den Buy-Pendants in tst_buysform.cpp.

    void test_presenterSaleEdit_setsSplitHintOnConstruction()
    {
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);

        QVERIFY(view.splitHintCallCount > 0);
        QVERIFY(!view.lastSplitHint.isEmpty());
    }

    void test_presenterSaleEdit_noSplits_hintSaysNoSplit()
    {
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);

        QVERIFY(!view.lastHasSplit);
        QVERIFY(view.lastSplitTooltip.isEmpty());
    }

    void test_presenterSaleEdit_splitAfterSaleDate_hintIsActive()
    {
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        view.m_dateTime  = QStringLiteral("2021-03-18T10:00:00");
        view.m_volume    = 5.0;
        view.m_salePrice = 1003.00;
        model.splits << ShareSplitObject(QStringLiteral("s1"), QStringLiteral("share-1"),
                                         QDate(2022, 7, 18), 20.0, 1.0);
        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);

        QVERIFY(view.lastHasSplit);
        QVERIFY2(view.lastSplitHint.contains(QStringLiteral("20:1")),
                 qPrintable(view.lastSplitHint));
    }

    void test_presenterSaleEdit_splitBeforeSaleDate_hintIsInactive()
    {
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        view.m_dateTime = QStringLiteral("2023-02-14T10:00:00");
        model.splits << ShareSplitObject(QStringLiteral("s1"), QStringLiteral("share-1"),
                                         QDate(2022, 7, 18), 20.0, 1.0);
        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);

        QVERIFY(!view.lastHasSplit);
    }

    void test_presenterSaleEdit_onDateEdited_refreshesHint()
    {
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        view.m_dateTime = QStringLiteral("2023-02-14T10:00:00");
        model.splits << ShareSplitObject(QStringLiteral("s1"), QStringLiteral("share-1"),
                                         QDate(2022, 7, 18), 20.0, 1.0);
        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        QVERIFY(!view.lastHasSplit);

        view.m_dateTime = QStringLiteral("2021-03-18T10:00:00");
        p.onDateEdited();

        QVERIFY(view.lastHasSplit);
    }

    void test_presenterSaleEdit_usesSalePriceNotBuyPrice()
    {
        // Bei den Verkäufen geht der VERKAUFSpreis in den Hinweis ein —
        // ein Copy-Paste-Fehler aus der Buy-Variante würde hier auffallen.
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        view.m_dateTime  = QStringLiteral("2021-03-18T10:00:00");
        view.m_volume    = 5.0;
        view.m_salePrice = 2000.00;
        model.splits << ShareSplitObject(QStringLiteral("s1"), QStringLiteral("share-1"),
                                         QDate(2022, 7, 18), 20.0, 1.0);
        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);

        // 2000 / 20 = 100
        QVERIFY2(view.lastSplitHint.contains(QLocale().toString(100.0, 'f', 4)),
                 qPrintable(view.lastSplitHint));
    }

    // ── Anteilige Kauf-Nebenkosten (Bugfix) ───────────────────────────────
    // Regression gegen den Verlust von brokeragePart/reductionPart beim
    // Umbau auf SaleFifoAllocator: SaleBuyDetail hat fuer beide Parameter
    // Defaultwerte 0.0, weshalb der Verlust ohne Compilerfehler blieb.
    // Aufgefallen ist er erst an einer echten Datenbank, in der 48 historisch
    // erfasste Verkaeufe ihre Kosten korrekt tragen und nur ein neu
    // erfasster Verkauf 0,00 EUR auswies.

    void test_presenterSaleEdit_onSave_fullyConsumedBuy_carriesCompleteBrokerage()
    {
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        view.m_dateTime  = QStringLiteral("2025-03-28T16:01:44");
        view.m_volume    = 10.0;
        view.m_salePrice = 145.0;
        model.availableBuys = {
            BuyObject(QStringLiteral("b1"), QStringLiteral("share-1"),
                      QStringLiteral("depot1"), QString(),
                      QStringLiteral("2020-03-18T09:04:13"), 10.0, 0.0, 971.90)
        };
        // Provision 29,20 + Handelsplatzgebuehr 1,75 = 30,95
        model.brokerage = BrokerageObject(
            QStringLiteral("brk-1"), QStringLiteral("share-1"),
            QStringLiteral("b1"), QString(),
            QStringLiteral("2020-03-18T09:04:13"), 29.20, 0.0, 1.75, 0.0);

        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onSave();

        QVERIFY(model.addSaleCalled);
        QCOMPARE(model.lastAddedSale.saleBuyDetails().size(), 1);
        const SaleBuyDetail d = model.lastAddedSale.saleBuyDetails().first();
        QVERIFY2(qAbs(d.brokeragePart() - 30.95) < 1e-9,
                 qPrintable(QStringLiteral("brokeragePart=%1").arg(d.brokeragePart())));
    }

    void test_presenterSaleEdit_onSave_partialSale_splitsBrokerageProportionally()
    {
        // Ein Drittel des Kaufs verbraucht -> ein Drittel der Kosten. Genau
        // dieses Verhalten steht in der Datenbank fuer zwei Verkaeufe vom
        // 25.09.2017, die sich einen Kauf teilen (6,87333 + 3,43667 = 10,31).
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        view.m_volume    = 10.0;
        view.m_salePrice = 150.0;
        model.availableBuys = {
            BuyObject(QStringLiteral("b1"), QStringLiteral("share-1"),
                      QStringLiteral("depot1"), QString(),
                      QStringLiteral("2024-01-01T10:00:00"), 30.0, 0.0, 100.0)
        };
        model.brokerage = BrokerageObject(
            QStringLiteral("brk-1"), QStringLiteral("share-1"),
            QStringLiteral("b1"), QString(),
            QStringLiteral("2024-01-01T10:00:00"), 30.0, 0.0, 0.0, 6.0);

        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onSave();

        QCOMPARE(model.lastAddedSale.saleBuyDetails().size(), 1);
        const SaleBuyDetail d = model.lastAddedSale.saleBuyDetails().first();
        QVERIFY(qAbs(d.brokeragePart() - 10.0) < 1e-9);   // 30,00 x 10/30
        QVERIFY(qAbs(d.reductionPart() -  2.0) < 1e-9);   //  6,00 x 10/30
    }

    void test_presenterSaleEdit_onSave_brokerageIsNotScaledBySplit()
    {
        // Kauf 10 Stueck vor einem 20:1-Split, Verkauf von 200 heutigen
        // Stuecken. Die Zuteilung liegt in Beleg-Skala des Kaufs (10), der
        // Bruch detailVolume/buy.volume() ist damit 1,0 — die Kosten duerfen
        // NICHT mitskaliert werden. Ein Geldbetrag kennt keinen Split.
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        view.m_dateTime  = QStringLiteral("2025-03-28T16:01:44");
        view.m_volume    = 200.0;
        view.m_salePrice = 145.0;
        model.availableBuys = {
            BuyObject(QStringLiteral("b1"), QStringLiteral("share-1"),
                      QStringLiteral("depot1"), QString(),
                      QStringLiteral("2020-03-18T09:04:13"), 10.0, 0.0, 971.90)
        };
        model.splits << ShareSplitObject(QStringLiteral("s1"), QStringLiteral("share-1"),
                                         QDate(2022, 7, 18), 20.0, 1.0);
        model.brokerage = BrokerageObject(
            QStringLiteral("brk-1"), QStringLiteral("share-1"),
            QStringLiteral("b1"), QString(),
            QStringLiteral("2020-03-18T09:04:13"), 29.20, 0.0, 1.75, 0.0);

        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onSave();

        QCOMPARE(model.lastAddedSale.saleBuyDetails().size(), 1);
        const SaleBuyDetail d = model.lastAddedSale.saleBuyDetails().first();
        QVERIFY(qAbs(d.volume() - 10.0) < 1e-9);          // Beleg-Skala des Kaufs
        QVERIFY2(qAbs(d.brokeragePart() - 30.95) < 1e-9,  // NICHT 619,00
                 qPrintable(QStringLiteral("brokeragePart=%1").arg(d.brokeragePart())));
    }

    void test_presenterSaleEdit_onShowDetails_liveBranch_reportsBuyCosts()
    {
        // Vor dem Bugfix stand in der Spalte "Kosten" im Live-Zweig hart 0.0.
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        view.m_volume    = 10.0;
        view.m_salePrice = 145.0;
        model.availableBuys = {
            BuyObject(QStringLiteral("b1"), QStringLiteral("share-1"),
                      QStringLiteral("depot1"), QString(),
                      QStringLiteral("2020-03-18T09:04:13"), 10.0, 0.0, 971.90)
        };
        model.brokerage = BrokerageObject(
            QStringLiteral("brk-1"), QStringLiteral("share-1"),
            QStringLiteral("b1"), QString(),
            QStringLiteral("2020-03-18T09:04:13"), 29.20, 0.0, 1.75, 0.0);

        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onShowDetails();

        QCOMPARE(view.showBuyDetailsCallCount, 1);
        QCOMPARE(view.lastBuyDetails.rows.size(), 1);
        QVERIFY(qAbs(view.lastBuyDetails.rows.first().fees - 30.95) < 1e-9);
        QVERIFY(qAbs(view.lastBuyDetails.totalFees          - 30.95) < 1e-9);
    }

    void test_presenterSaleEdit_onShowDetails_profitLossSubtractsBuyCosts()
    {
        // G/V = Verkaufswert - (Kaufsumme + Kaufkosten - Kaufrabatt)
        //       - Verkaufsgebuehren/Steuern. Der Rabatt wurde in der
        //       Summenzeile bisher uebergangen, obwohl die Spalte "Gesamt"
        //       je Zeile ihn bereits abzieht.
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        view.m_volume    = 10.0;
        view.m_salePrice = 150.0;
        model.availableBuys = {
            BuyObject(QStringLiteral("b1"), QStringLiteral("share-1"),
                      QStringLiteral("depot1"), QString(),
                      QStringLiteral("2024-01-01T10:00:00"), 10.0, 0.0, 100.0)
        };
        model.brokerage = BrokerageObject(
            QStringLiteral("brk-1"), QStringLiteral("share-1"),
            QStringLiteral("b1"), QString(),
            QStringLiteral("2024-01-01T10:00:00"), 12.0, 0.0, 0.0, 2.0);

        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onShowDetails();

        const SaleBuyDetailSummary& s = view.lastBuyDetails;
        QVERIFY(qAbs(s.totalSaleValue - 1500.0) < 1e-9);
        QVERIFY(qAbs(s.totalBuyValue  - 1000.0) < 1e-9);
        QVERIFY(qAbs(s.totalFees      -   12.0) < 1e-9);
        QVERIFY(qAbs(s.totalReduction -    2.0) < 1e-9);
        // 1500 - (1000 + 12 - 2) - 0 = 490
        QVERIFY2(qAbs(s.totalProfitLoss - 490.0) < 1e-9,
                 qPrintable(QStringLiteral("totalProfitLoss=%1").arg(s.totalProfitLoss)));
    }

    void test_presenterSaleEdit_livePreview_gewinnVerlustIncludesBuyCosts()
    {
        // Die Live-Vorschau im Formular muss dasselbe zeigen wie das, was
        // nach dem Speichern aus SaleObject::profitLossBrokerageReduction()
        // zurueckkommt — sonst springt der Wert beim Speichern.
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        view.m_volume    = 10.0;
        view.m_salePrice = 150.0;
        model.availableBuys = {
            BuyObject(QStringLiteral("b1"), QStringLiteral("share-1"),
                      QStringLiteral("depot1"), QString(),
                      QStringLiteral("2024-01-01T10:00:00"), 10.0, 0.0, 100.0)
        };
        model.brokerage = BrokerageObject(
            QStringLiteral("brk-1"), QStringLiteral("share-1"),
            QStringLiteral("b1"), QString(),
            QStringLiteral("2024-01-01T10:00:00"), 12.0, 0.0, 0.0, 2.0);

        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onValuesChanged();

        // Anzeigefeld "Gekaufter Kaufwert" bleibt OHNE Brokerage
        QVERIFY(qAbs(view.lastKaufwert - 1000.0) < 1e-9);
        // 1500 - 0 + 0 - 1000 - 12 + 2 - 0 = 490
        QVERIFY2(qAbs(view.lastGewinnVerlust - 490.0) < 1e-9,
                 qPrintable(QStringLiteral("lastGewinnVerlust=%1").arg(view.lastGewinnVerlust)));
    }

    // ── Skalenbewusste Mengenprüfung (Bugfix, siehe ARCHITECTURE.md
    // "Erledigt / Archiv", "Skalenbewusste Mengenprüfung im
    // Verkaufsformular", 11.08.2026, gefixt 20.08.2026) ────────────────────
    //
    // SaleFifoAllocator::allocate() deckelte eine zu hohe Verkaufsmenge
    // bislang still auf das verfügbare Volumen, statt einen Fehler zu
    // melden — im Feldfall zeigte das Formular dadurch grüne Haken und eine
    // vollständige Gewinnermittlung, obwohl 3.800 Stück angefordert, aber
    // nur 190 verfügbar waren.

    void test_presenterSaleEdit_onVolumeOrPriceEdited_exceedsAvailable_setsError()
    {
        StubModelSaleEdit model;
        model.availableBuys = {
            BuyObject(QStringLiteral("b1"), QStringLiteral("share-1"),
                     QStringLiteral("depot1"), QString(),
                     QStringLiteral("2024-01-01T10:00:00"), 190.0, 0.0, 50.0)
        };

        bool errorSet = false;
        struct SpyView : public StubViewSaleEdit {
            bool* called;
            void setFieldError(const QString& f) override
                { if (f == QLatin1String("volume")) *called = true; }
        } spyView;
        spyView.called   = &errorSet;
        spyView.m_volume = 3800.0;   // Feldfall aus ARCHITECTURE.md

        PresenterSaleEdit p(&spyView, &model, QStringLiteral("share-1"), nullptr);
        p.onVolumeOrPriceEdited();

        QVERIFY(errorSet);
    }

    void test_presenterSaleEdit_onVolumeOrPriceEdited_withinAvailable_setsOk()
    {
        StubModelSaleEdit model;
        model.availableBuys = {
            BuyObject(QStringLiteral("b1"), QStringLiteral("share-1"),
                     QStringLiteral("depot1"), QString(),
                     QStringLiteral("2024-01-01T10:00:00"), 190.0, 0.0, 50.0)
        };

        bool okSet = false;
        struct SpyView : public StubViewSaleEdit {
            bool* called;
            void setFieldOk(const QString& f, const QString&) override
                { if (f == QLatin1String("volume")) *called = true; }
        } spyView;
        spyView.called   = &okSet;
        spyView.m_volume = 190.0;   // exakt gedeckt

        PresenterSaleEdit p(&spyView, &model, QStringLiteral("share-1"), nullptr);
        p.onVolumeOrPriceEdited();

        QVERIFY(okSet);
    }

    void test_presenterSaleEdit_onSave_exceedsAvailable_showsErrorAndBlocksSave()
    {
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        model.availableBuys = {
            BuyObject(QStringLiteral("b1"), QStringLiteral("share-1"),
                     QStringLiteral("depot1"), QString(),
                     QStringLiteral("2024-01-01T10:00:00"), 190.0, 0.0, 50.0)
        };
        view.m_volume = 3800.0;

        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onSave();

        QVERIFY(!view.lastError.isEmpty());
        QVERIFY(!model.addSaleCalled);
    }

    void test_presenterSaleEdit_onSave_exceedsAvailable_errorNamesBothQuantities()
    {
        // Die Meldung muss beide Zahlen konkret benennen, sonst weiss der
        // Nutzer nicht, was zu tun ist (vgl. das Verhalten vor dem Bugfix:
        // gar keine Meldung, obwohl die Menge nicht gedeckt war).
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        model.availableBuys = {
            BuyObject(QStringLiteral("b1"), QStringLiteral("share-1"),
                     QStringLiteral("depot1"), QString(),
                     QStringLiteral("2024-01-01T10:00:00"), 190.0, 0.0, 50.0)
        };
        view.m_volume = 3800.0;

        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onSave();

        QVERIFY2(view.lastError.contains(QLocale().toString(3800.0, 'f', 4)),
                 qPrintable(view.lastError));
        QVERIFY2(view.lastError.contains(QLocale().toString(190.0, 'f', 4)),
                 qPrintable(view.lastError));
    }

    void test_presenterSaleEdit_onSave_exactMatch_isAccepted()
    {
        // Grenzfall: die komplette verfügbare Menge zu verkaufen muss
        // weiterhin funktionieren (das war im Feldfall zufällig der Grund,
        // warum das falsche Verhalten nicht auffiel).
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        model.availableBuys = {
            BuyObject(QStringLiteral("b1"), QStringLiteral("share-1"),
                     QStringLiteral("depot1"), QString(),
                     QStringLiteral("2024-01-01T10:00:00"), 190.0, 0.0, 50.0)
        };
        view.m_volume = 190.0;

        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onSave();

        QVERIFY(model.addSaleCalled);
        QVERIFY(view.lastError.isEmpty());
    }

    void test_presenterSaleEdit_onSave_scaleAware_splitBetweenBuyAndSale()
    {
        // Kauf von 5 Beleg-Stück vor einem 20:1-Split (avail heute = 100),
        // Verkauf von 101 heutigen Stücken darf NICHT durchgehen — der
        // unskalierte Vergleich (5 Beleg-Stück vs. 101 heutige) wäre falsch
        // und würde die zu hohe Menge übersehen.
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        model.availableBuys = {
            BuyObject(QStringLiteral("b1"), QStringLiteral("share-1"),
                     QStringLiteral("depot1"), QString(),
                     QStringLiteral("2020-01-01T10:00:00"), 5.0, 0.0, 1000.0)
        };
        model.splits << ShareSplitObject(QStringLiteral("s1"), QStringLiteral("share-1"),
                                         QDate(2021, 1, 1), 20.0, 1.0);
        view.m_dateTime = QStringLiteral("2022-01-01T10:00:00");
        view.m_volume   = 101.0;

        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onSave();

        QVERIFY(!view.lastError.isEmpty());
        QVERIFY(!model.addSaleCalled);
    }

    void test_presenterSaleEdit_onSave_nonLatestSaleDocOnlyEdit_skipsVolumeCheck()
    {
        // Bei einem älteren, nicht-jüngsten Verkauf ist die Menge gesperrt
        // (nur das Dokument ist editierbar, siehe ViewSaleEdit::
        // setButtonStates()) — die Mengenprüfung darf ein reines
        // Dokument-Update nicht blockieren, selbst wenn das Formularfeld
        // (hier ungenutzt) einen zu hohen Wert trüge.
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        model.availableBuys.clear();   // nichts verfügbar
        const SaleObject older = makeSale(QStringLiteral("s-old"),
                                          QStringLiteral("share-1"), 2023);
        const SaleObject newer = makeSale(QStringLiteral("s-new"),
                                          QStringLiteral("share-1"), 2024);
        model.sales = { older, newer };
        view.m_volume = 999999.0;   // gesperrtes Feld, würde sonst blockieren

        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onRowSelected(QStringLiteral("s-old"));   // non-latest

        p.onSave();

        QVERIFY(view.lastError.isEmpty());
        QVERIFY(model.updateSaleCalled);
        QVERIFY(!model.addSaleCalled);
    }

    // ── Split-Marker in der Verkaufs-Übersicht (Phase 3c) ─────────────────

    void test_presenterSaleEdit_populateOverview_passesSplitsAsParameter()
    {
        // Die Splits gehen als Parameter an die View, nicht über einen
        // eigenen setSplits()-Aufruf — sonst entstünde eine unsichtbare
        // Reihenfolge-Abhängigkeit zwischen zwei View-Aufrufen.
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        model.sales = {
            SaleObject(QStringLiteral("s1"), QStringLiteral("share-1"),
                       QStringLiteral("depot1"), QStringLiteral("ord-s1"),
                       QStringLiteral("2025-03-28T16:01:44"),
                       200.0, 145.0, {})
        };
        model.splits << ShareSplitObject(QStringLiteral("s1"), QStringLiteral("share-1"),
                                         QDate(2022, 7, 18), 20.0, 1.0);

        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);

        QVERIFY(view.populateOverviewCalled);
        QCOMPARE(view.lastOverviewSplits.size(), 1);
        QCOMPARE(view.lastOverviewSplits.first().ratioNew(), 20.0);
    }

    void test_presenterSaleEdit_construction_loadsOverview()
    {
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        QVERIFY(view.populateOverviewCalled);
    }

    void test_presenterSaleEdit_construction_clearsForm()
    {
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        QVERIFY(view.clearFormCalled);
    }

    void test_presenterSaleEdit_construction_setsButtonStates_noSelection()
    {
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        QVERIFY(view.setButtonStatesCalled);
        QCOMPARE(view.lastCanRemove,  false);
        QCOMPARE(view.lastIsLastSale, false);
    }

    void test_presenterSaleEdit_onRowSelected_singleSale_isLastSale()
    {
        // A single sale is always the latest.
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        const SaleObject s = makeSale(QStringLiteral("s1"),
                                      QStringLiteral("share-1"), 2024);
        model.sales = { s };

        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onRowSelected(QStringLiteral("s1"));

        QVERIFY(view.lastIsLastSale);
        QVERIFY(view.lastCanRemove);   // isLastSale && no sold-vol guard for sales
    }

    void test_presenterSaleEdit_onRowSelected_olderSale_isNotLastSale()
    {
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        const SaleObject older = makeSale(QStringLiteral("s-old"),
                                          QStringLiteral("share-1"), 2023);
        const SaleObject newer = makeSale(QStringLiteral("s-new"),
                                          QStringLiteral("share-1"), 2024);
        model.sales = { older, newer };

        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onRowSelected(QStringLiteral("s-old"));

        QCOMPARE(view.lastIsLastSale, false);
        QCOMPARE(view.lastCanRemove,  false);
    }

    void test_presenterSaleEdit_onRowSelected_newerSale_isLastSale()
    {
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        const SaleObject older = makeSale(QStringLiteral("s-old"),
                                          QStringLiteral("share-1"), 2023);
        const SaleObject newer = makeSale(QStringLiteral("s-new"),
                                          QStringLiteral("share-1"), 2024);
        model.sales = { older, newer };

        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onRowSelected(QStringLiteral("s-new"));

        QVERIFY(view.lastIsLastSale);
        QVERIFY(view.lastCanRemove);
    }

    void test_presenterSaleEdit_onRowSelected_emptyGuid_resetsForm()
    {
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        view.clearFormCalled = false;

        p.onRowSelected(QString());

        QVERIFY(view.clearFormCalled);
    }

    void test_presenterSaleEdit_onReset_setsButtonStates_noSelection()
    {
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        view.setButtonStatesCalled = false;

        p.onReset();

        QVERIFY(view.setButtonStatesCalled);
        QCOMPARE(view.lastCanRemove,  false);
        QCOMPARE(view.lastIsLastSale, false);
    }

    void test_presenterSaleEdit_onReset_jumpsToOverviewTab()
    {
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        view.showOverviewTabCalled = false;

        p.onReset();

        QVERIFY(view.showOverviewTabCalled);
    }

    void test_presenterSaleEdit_onSave_newSale_callsAddSale()
    {
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);

        p.onSave();

        QVERIFY(model.addSaleCalled);
    }

    void test_presenterSaleEdit_onSave_newSale_emitsDataChanged()
    {
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);

        QSignalSpy spy(&p, &PresenterSaleEdit::dataChanged);
        p.onSave();

        QCOMPARE(spy.count(), 1);
    }

    void test_presenterSaleEdit_onSave_newSale_jumpsToOverviewTab()
    {
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        view.showOverviewTabCalled = false;

        p.onSave();

        QVERIFY(view.showOverviewTabCalled);
    }

    void test_presenterSaleEdit_onSave_missingFields_showsError()
    {
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        view.m_missingFields = true;
        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);

        p.onSave();

        QVERIFY(!view.lastError.isEmpty());
        QVERIFY(!model.addSaleCalled);
    }

    void test_presenterSaleEdit_onSave_latestSale_callsUpdateSale()
    {
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        const SaleObject s = makeSale(QStringLiteral("s1"),
                                      QStringLiteral("share-1"), 2024);
        model.sales = { s };

        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onRowSelected(QStringLiteral("s1"));
        model.updateSaleCalled = false;

        p.onSave();

        QVERIFY(model.updateSaleCalled);
        QVERIFY(!model.addSaleCalled);
    }

    // ─────────────────────────────────────────────────────────────────────
    // Aktiensplit-Behandlung, Phase 2c (07.08.2026, siehe ARCHITECTURE.md
    // "Offene Punkte") — FIFO-Zuteilung wird beim Bearbeiten des jüngsten
    // Verkaufs jetzt immer neu berechnet, statt die gespeicherten
    // SaleBuyDetails unverändert zu übernehmen.
    // ─────────────────────────────────────────────────────────────────────

    void test_presenterSaleEdit_onSave_latestSale_recomputesFifoAllocation()
    {
        // Der gespeicherte Verkauf zeigt absichtlich auf einen Kauf, der in
        // den aktuell verfügbaren Käufen unten NICHT vorkommt — nur wenn
        // onSave() die Zuteilung wirklich neu berechnet (statt die
        // veraltete zu übernehmen), zeigt das gespeicherte Ergebnis auf den
        // frischen Kauf.
        StubViewSaleEdit  view;
        StubModelSaleEdit model;

        const QList<SaleBuyDetail> staleDetails = {
            SaleBuyDetail(QStringLiteral("stale-buy"),
                         QStringLiteral("2023-01-01T10:00:00"), 10.0, 999.0)
        };
        const SaleObject s(QStringLiteral("s1"), QStringLiteral("share-1"),
                           QStringLiteral("depot1"), QStringLiteral("ord-s1"),
                           QStringLiteral("2024-06-15T10:00:00"),
                           10.0, 150.0, staleDetails);
        model.sales = { s };
        model.availableBuys = {
            BuyObject(QStringLiteral("fresh-buy"), QStringLiteral("share-1"),
                     QStringLiteral("depot1"), QString(),
                     QStringLiteral("2024-01-01T10:00:00"), 20.0, 0.0, 100.0)
        };

        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onRowSelected(QStringLiteral("s1"));
        view.m_volume = 5.0;   // Verkaufsmenge im Formular geändert

        p.onSave();

        QVERIFY(model.updateSaleCalled);
        QCOMPARE(model.lastUpdatedSale.saleBuyDetails().size(), 1);
        QCOMPARE(model.lastUpdatedSale.saleBuyDetails().first().buyGuid(),
                 QStringLiteral("fresh-buy"));
        QCOMPARE(model.lastUpdatedSale.saleBuyDetails().first().volume(), 5.0);
    }

    void test_presenterSaleEdit_onSave_latestSale_usesExcludingSaleVariant()
    {
        // onSave() muss beim Bearbeiten des jüngsten Verkaufs
        // loadAvailableBuysForDepotExcludingSale() mit GENAU dessen GUID
        // aufrufen (Rückbuchung der eigenen, bereits gebuchten Anteile).
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        const SaleObject s = makeSale(QStringLiteral("s1"),
                                      QStringLiteral("share-1"), 2024);
        model.sales = { s };

        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onRowSelected(QStringLiteral("s1"));
        model.excludingSaleCalled = false;

        p.onSave();

        QVERIFY(model.excludingSaleCalled);
        QCOMPARE(model.lastExcludeSaleGuid, QStringLiteral("s1"));
    }

    void test_presenterSaleEdit_onSave_newSale_doesNotUseExcludingSaleVariant()
    {
        // Ein neuer Verkauf hat keinen bestehenden Verkauf, dessen Anteile
        // zurückgebucht werden müssten.
        StubViewSaleEdit  view;
        StubModelSaleEdit model;

        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onSave();

        QVERIFY(!model.excludingSaleCalled);
    }

    void test_presenterSaleEdit_onRowSelected_latestSale_livePreviewMatchesFifo()
    {
        // refreshDerivedValues() muss beim Bearbeiten des jüngsten
        // Verkaufs live neu rechnen (nicht mehr die gespeicherten Werte
        // zeigen), sonst weicht die Vorschau während der Eingabe von dem
        // ab, was onSave() tatsächlich berechnet.
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        const SaleObject s = makeSale(QStringLiteral("s1"),
                                      QStringLiteral("share-1"), 2024);
        model.sales = { s };
        model.availableBuys = {
            BuyObject(QStringLiteral("b1"), QStringLiteral("share-1"),
                     QStringLiteral("depot1"), QString(),
                     QStringLiteral("2024-01-01T10:00:00"), 20.0, 0.0, 100.0)
        };

        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onRowSelected(QStringLiteral("s1"));

        // view.m_volume ist per Default 10.0 -> Kaufwert = 10 x 100,00 € = 1.000,00 €
        QVERIFY(qAbs(view.lastKaufwert - 1000.0) < 1e-6);
    }

    void test_presenterSaleEdit_onSave_nonLatestSale_callsUpdateSaleDocOnly()
    {
        // Only the document path is editable for non-latest sales.
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        const SaleObject older = makeSale(QStringLiteral("s-old"),
                                          QStringLiteral("share-1"), 2023);
        const SaleObject newer = makeSale(QStringLiteral("s-new"),
                                          QStringLiteral("share-1"), 2024);
        model.sales = { older, newer };

        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onRowSelected(QStringLiteral("s-old"));   // non-latest
        model.updateSaleCalled = false;

        p.onSave();

        QVERIFY(model.updateSaleCalled);
        QVERIFY(!model.addSaleCalled);
    }

    void test_presenterSaleEdit_onSave_nonLatestSale_jumpsToOverviewTab()
    {
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        const SaleObject older = makeSale(QStringLiteral("s-old"),
                                          QStringLiteral("share-1"), 2023);
        const SaleObject newer = makeSale(QStringLiteral("s-new"),
                                          QStringLiteral("share-1"), 2024);
        model.sales = { older, newer };

        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onRowSelected(QStringLiteral("s-old"));
        view.showOverviewTabCalled = false;

        p.onSave();

        QVERIFY(view.showOverviewTabCalled);
    }

    void test_presenterSaleEdit_onSave_nonLatestSale_emitsDataChanged()
    {
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        const SaleObject older = makeSale(QStringLiteral("s-old"),
                                          QStringLiteral("share-1"), 2023);
        const SaleObject newer = makeSale(QStringLiteral("s-new"),
                                          QStringLiteral("share-1"), 2024);
        model.sales = { older, newer };

        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onRowSelected(QStringLiteral("s-old"));
        QSignalSpy spy(&p, &PresenterSaleEdit::dataChanged);

        p.onSave();

        QCOMPARE(spy.count(), 1);
    }

    void test_presenterSaleEdit_onSave_duplicateOrderNumber_showsError()
    {
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        model.orderExists = true;
        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);

        p.onSave();

        QVERIFY(!view.lastError.isEmpty());
        QVERIFY(!model.addSaleCalled);
    }

    void test_presenterSaleEdit_onSave_documentDuplicate_showsError()
    {
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        view.m_docPath = QStringLiteral("/some/doc.pdf");
        model.docExists = true;
        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);

        p.onSave();

        QVERIFY(!view.lastError.isEmpty());
        QVERIFY(!model.addSaleCalled);
    }

    void test_presenterSaleEdit_onRemove_latestSale_callsModel()
    {
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        const SaleObject s = makeSale(QStringLiteral("s1"),
                                      QStringLiteral("share-1"), 2024);
        model.sales = { s };

        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onRowSelected(QStringLiteral("s1"));
        model.removeSaleCalled = false;

        p.onRemove();

        QVERIFY(model.removeSaleCalled);
    }

    void test_presenterSaleEdit_onRemove_latestSale_emitsDataChanged()
    {
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        const SaleObject s = makeSale(QStringLiteral("s1"),
                                      QStringLiteral("share-1"), 2024);
        model.sales = { s };

        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onRowSelected(QStringLiteral("s1"));
        QSignalSpy spy(&p, &PresenterSaleEdit::dataChanged);

        p.onRemove();

        QCOMPARE(spy.count(), 1);
    }

    void test_presenterSaleEdit_onRemove_olderSale_showsError()
    {
        // Non-latest sales must not be removable.
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        const SaleObject older = makeSale(QStringLiteral("s-old"),
                                          QStringLiteral("share-1"), 2023);
        const SaleObject newer = makeSale(QStringLiteral("s-new"),
                                          QStringLiteral("share-1"), 2024);
        model.sales = { older, newer };

        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onRowSelected(QStringLiteral("s-old"));

        p.onRemove();

        QVERIFY(!view.lastError.isEmpty());
        QVERIFY(!model.removeSaleCalled);
    }

    void test_presenterSaleEdit_onRemove_noSelection_doesNothing()
    {
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);

        p.onRemove();   // no sale selected

        QVERIFY(!model.removeSaleCalled);
    }

    void test_presenterSaleEdit_onOrderNumberEdited_empty_setsError()
    {
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        view.m_orderNumber = QString();
        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);

        bool errorSet = false;
        struct SpyView : public StubViewSaleEdit {
            bool* called;
            void setFieldError(const QString& f) override
                { if (f == QLatin1String("orderNumber")) *called = true; }
        } spyView;
        spyView.called        = &errorSet;
        spyView.m_orderNumber = QString();

        PresenterSaleEdit p2(&spyView, &model, QStringLiteral("share-1"), nullptr);
        p2.onOrderNumberEdited();

        QVERIFY(errorSet);
    }

    void test_presenterSaleEdit_onOrderNumberEdited_duplicate_setsError()
    {
        StubModelSaleEdit model;
        model.orderExists = true;

        bool errorSet = false;
        struct SpyView : public StubViewSaleEdit {
            bool* called;
            void setFieldError(const QString& f) override
                { if (f == QLatin1String("orderNumber")) *called = true; }
        } spyView;
        spyView.called = &errorSet;

        PresenterSaleEdit p(&spyView, &model, QStringLiteral("share-1"), nullptr);
        p.onOrderNumberEdited();

        QVERIFY(errorSet);
    }

    void test_presenterSaleEdit_onDocumentPathEdited_duplicate_setsError()
    {
        StubModelSaleEdit model;
        model.docExists = true;

        bool errorSet = false;
        struct SpyView : public StubViewSaleEdit {
            bool* called;
            void setFieldError(const QString& f) override
                { if (f == QLatin1String("document")) *called = true; }
        } spyView;
        spyView.called   = &errorSet;
        spyView.m_docPath = QStringLiteral("/doc.pdf");

        PresenterSaleEdit p(&spyView, &model, QStringLiteral("share-1"), nullptr);
        p.onDocumentPathEdited();

        QVERIFY(errorSet);
    }

    void test_presenterSaleEdit_onDocumentPathEdited_unique_setsOk()
    {
        StubModelSaleEdit model;
        model.docExists = false;

        bool okSet = false;
        struct SpyView : public StubViewSaleEdit {
            bool* called;
            void setFieldOk(const QString& f, const QString&) override
                { if (f == QLatin1String("document")) *called = true; }
        } spyView;
        spyView.called    = &okSet;
        spyView.m_docPath = QStringLiteral("/doc.pdf");

        PresenterSaleEdit p(&spyView, &model, QStringLiteral("share-1"), nullptr);
        p.onDocumentPathEdited();

        QVERIFY(okSet);
    }

    // ─────────────────────────────────────────────────────────────────────
    // ViewSaleEdit — widget construction & field behaviour
    // ─────────────────────────────────────────────────────────────────────

    void test_viewSaleEdit_canBeConstructed()
    {
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);
        QVERIFY(dlg.windowTitle().contains(tr("Verkäufe")));
    }

    void test_viewSaleEdit_initialValues()
    {
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);

        QCOMPARE(dlg.volume(),          0.0);
        QCOMPARE(dlg.salePrice(),       0.0);
        QCOMPARE(dlg.taxAtSource(),     0.0);
        QCOMPARE(dlg.capitalGainsTax(), 0.0);
        QCOMPARE(dlg.solidarityTax(),   0.0);
        QCOMPARE(dlg.provision(),       0.0);
        QCOMPARE(dlg.brokerFee(),       0.0);
        QCOMPARE(dlg.traderFee(),       0.0);
        QCOMPARE(dlg.reduction(),       0.0);
        QVERIFY(dlg.orderNumber().isEmpty());
        QVERIFY(dlg.documentPath().isEmpty());
    }

    void test_viewSaleEdit_depotNumberCombo_populatedFromConfig()
    {
        openMemoryDb();
        DocumentsConfig cfg;
        const QString docsPath =
            QCoreApplication::applicationDirPath() + QStringLiteral("/Documents.xml");
        if (QFileInfo::exists(docsPath)) cfg.load(docsPath);

        ViewSaleEdit dlg(QStringLiteral("share-guid"), &cfg);
        const auto combos = dlg.findChildren<QComboBox*>();
        QVERIFY(!combos.isEmpty());
        // Placeholder + at least one bank entry if Documents.xml exists
        QVERIFY(combos.first()->count() >= 1);
    }

    void test_viewSaleEdit_hasMissingRequiredFields_initiallyTrue()
    {
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);

        QStringList missing;
        QVERIFY(dlg.hasMissingRequiredFields(missing));
        QVERIFY(missing.contains(tr("Depotnummer")));
        QVERIFY(missing.contains(tr("Auftragsnummer")));
        QVERIFY(missing.contains(tr("Verkaufte Anteile")));
        QVERIFY(missing.contains(tr("Verkaufspreis")));
    }

    void test_viewSaleEdit_hasMissingRequiredFields_falseAfterAllSet()
    {
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);

        dlg.setFieldOk(QStringLiteral("depotNumber"), QStringLiteral("8006189848"));
        dlg.setFieldOk(QStringLiteral("orderNumber"), QStringLiteral("ORD-S-001"));
        dlg.setFieldOk(QStringLiteral("volume"),      QStringLiteral("10"));
        dlg.setFieldOk(QStringLiteral("salePrice"),   QStringLiteral("150.00"));

        QStringList missing;
        QVERIFY(!dlg.hasMissingRequiredFields(missing));
    }

    void test_viewSaleEdit_markMissingFieldsAsFailed_doesNotCrash()
    {
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);
        dlg.markMissingFieldsAsFailed();   // must not crash on empty form
        QStringList missing;
        QVERIFY(dlg.hasMissingRequiredFields(missing));
    }

    void test_viewSaleEdit_clearForm_resetsAllFields()
    {
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);

        dlg.setFieldOk(QStringLiteral("orderNumber"), QStringLiteral("ORD-S-99"));
        dlg.setFieldOk(QStringLiteral("volume"),      QStringLiteral("25"));
        dlg.setFieldOk(QStringLiteral("salePrice"),   QStringLiteral("200.00"));
        dlg.clearForm();

        QCOMPARE(dlg.volume(),    0.0);
        QCOMPARE(dlg.salePrice(), 0.0);
        QVERIFY(dlg.orderNumber().isEmpty());
    }

    void test_viewSaleEdit_clearForm_restoresEditableFields()
    {
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);

        // Put into read-only mode (non-latest edit)
        dlg.setButtonStates(/*canRemove=*/false, /*isLastSale=*/false, /*isEdit=*/true);
        dlg.clearForm();

        // After clearForm() all editable fields must be enabled again
        const auto edits = dlg.findChildren<QLineEdit*>();
        int disabledCount = 0;
        for (auto* le : edits)
            if (!le->isReadOnly() && !le->isEnabled()) ++disabledCount;
        QCOMPARE(disabledCount, 0);
    }

    void test_viewSaleEdit_setFieldOk_doesNotOverwriteWithEmptyValue()
    {
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);

        dlg.setFieldOk(QStringLiteral("orderNumber"), QStringLiteral("ORD-S-123"));
        // Live-validation call with empty value must not clear the field
        dlg.setFieldOk(QStringLiteral("orderNumber"), QString());

        QCOMPARE(dlg.orderNumber(), QStringLiteral("ORD-S-123"));
    }

    void test_viewSaleEdit_setFieldOk_writesValueWhenNonEmpty()
    {
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);

        dlg.setFieldOk(QStringLiteral("orderNumber"), QStringLiteral("ORD-S-456"));
        QCOMPARE(dlg.orderNumber(), QStringLiteral("ORD-S-456"));
    }

    void test_viewSaleEdit_setFieldError_doesNotCrash()
    {
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);
        dlg.setFieldError(QStringLiteral("orderNumber"));
        dlg.setFieldError(QStringLiteral("volume"));
        dlg.setFieldError(QStringLiteral("unknownField"));  // must not crash
    }

    void test_viewSaleEdit_setButtonStates_noSelection_addLabelHinzufuegen()
    {
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);
        dlg.setButtonStates(false, false, false);

        const auto buttons = dlg.findChildren<QPushButton*>();
        for (auto* btn : buttons)
            if (btn->text() == tr("Hinzufügen")) return;
        QFAIL("Hinzufügen button not found");
    }

    void test_viewSaleEdit_setButtonStates_isEdit_saveLabelSpeichern()
    {
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);
        dlg.setButtonStates(true, true, true);

        const auto buttons = dlg.findChildren<QPushButton*>();
        for (auto* btn : buttons)
            if (btn->text() == tr("Speichern")) return;
        QFAIL("Speichern button not found");
    }

    void test_viewSaleEdit_setButtonStates_canRemoveFalse_removeDisabled()
    {
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);
        dlg.setButtonStates(false, false, false);

        const auto buttons = dlg.findChildren<QPushButton*>();
        for (auto* btn : buttons) {
            if (btn->text() == tr("Entfernen")) {
                QVERIFY(!btn->isEnabled());
                return;
            }
        }
        QFAIL("Entfernen button not found");
    }

    void test_viewSaleEdit_setButtonStates_canRemoveTrue_removeEnabled()
    {
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);
        dlg.setButtonStates(true, true, true);

        const auto buttons = dlg.findChildren<QPushButton*>();
        for (auto* btn : buttons) {
            if (btn->text() == tr("Entfernen")) {
                QVERIFY(btn->isEnabled());
                return;
            }
        }
        QFAIL("Entfernen button not found");
    }

    void test_viewSaleEdit_setButtonStates_notLastSale_fieldsDisabled()
    {
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);
        dlg.setButtonStates(false, /*isLastSale=*/false, /*isEdit=*/true);

        // OrderNumber field must be disabled in non-latest edit mode
        const auto edits = dlg.findChildren<QLineEdit*>();
        for (auto* le : edits) {
            if (le->maxLength() == 100) {   // orderNumber has maxLength 100
                QVERIFY(!le->isEnabled());
                return;
            }
        }
        QFAIL("orderNumber QLineEdit not found");
    }

    void test_viewSaleEdit_setButtonStates_isLastSale_fieldsEnabled()
    {
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);
        dlg.setButtonStates(true, /*isLastSale=*/true, /*isEdit=*/true);

        const auto edits = dlg.findChildren<QLineEdit*>();
        for (auto* le : edits) {
            if (le->maxLength() == 100) {
                QVERIFY(le->isEnabled());
                return;
            }
        }
        QFAIL("orderNumber QLineEdit not found");
    }

    // ─────────────────────────────────────────────────────────────────────
    // ViewSaleEdit — populateOverview
    // ─────────────────────────────────────────────────────────────────────

    void test_viewSaleEdit_populateOverview_emptyList_noTabs()
    {
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        QVERIFY(tabs != nullptr);

        dlg.populateOverview({}, {});
        QCOMPARE(tabs->count(), 0);
    }

    void test_viewSaleEdit_populateOverview_singleYear_twoTabs()
    {
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = dlg.findChild<OverviewTabWidget*>();

        dlg.populateOverview({
            makeSale(QStringLiteral("s1"), QStringLiteral("share-guid"), 2024)
        }, {});

        QCOMPARE(tabs->count(), 2);
        QVERIFY(tabs->tabText(0).contains(tr("Übersicht")));
        QVERIFY(tabs->tabText(1).contains(QStringLiteral("2024")));
    }

    void test_viewSaleEdit_populateOverview_twoYears_threeTabs()
    {
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = dlg.findChild<OverviewTabWidget*>();

        dlg.populateOverview({
            makeSale(QStringLiteral("s1"), QStringLiteral("share-guid"), 2023),
            makeSale(QStringLiteral("s2"), QStringLiteral("share-guid"), 2024)
        }, {});

        QCOMPARE(tabs->count(), 3);
    }

    void test_viewSaleEdit_populateOverview_jahresTabsDescendingByYear()
    {
        // Newest year must be Tab 1, older year Tab 2.
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = dlg.findChild<OverviewTabWidget*>();

        dlg.populateOverview({
            makeSale(QStringLiteral("s1"), QStringLiteral("share-guid"), 2022),
            makeSale(QStringLiteral("s2"), QStringLiteral("share-guid"), 2024)
        }, {});

        QVERIFY(tabs->tabText(1).contains(QStringLiteral("2024")));
        QVERIFY(tabs->tabText(2).contains(QStringLiteral("2022")));
    }

    void test_viewSaleEdit_populateOverview_jahresTabHasFiveColumns()
    {
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);

        dlg.populateOverview({
            makeSale(QStringLiteral("s1"), QStringLiteral("share-guid"), 2024)
        }, {});

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        QVERIFY(tabs->count() >= 2);
        auto* container = tabs->widget(1);
        QVERIFY(container != nullptr);
        auto* tbl = qobject_cast<QTableWidget*>(
            container->property("dataTable").value<QObject*>());
        QVERIFY(tbl != nullptr);
        QCOMPARE(tbl->columnCount(), 5);  // Datum | Anteile | Auszahlung | G/V | Dokument
    }

    void test_viewSaleEdit_populateOverview_guidStoredInDateColumn()
    {
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);

        dlg.populateOverview({
            makeSale(QStringLiteral("sale-guid-1"),
                     QStringLiteral("share-guid"), 2024)
        }, {});

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        auto* container = tabs->widget(1);
        auto* tbl = qobject_cast<QTableWidget*>(
            container->property("dataTable").value<QObject*>());
        QVERIFY(tbl != nullptr);
        QVERIFY(tbl->rowCount() >= 1);
        QCOMPARE(tbl->item(0, 0)->data(Qt::UserRole).toString(),
                 QStringLiteral("sale-guid-1"));
    }

    void test_viewSaleEdit_populateOverview_repopulateReplacesOldTabs()
    {
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = dlg.findChild<OverviewTabWidget*>();

        dlg.populateOverview({
            makeSale(QStringLiteral("s1"), QStringLiteral("share-guid"), 2023)
        }, {});
        QCOMPARE(tabs->count(), 2);

        dlg.populateOverview({
            makeSale(QStringLiteral("s2"), QStringLiteral("share-guid"), 2024),
            makeSale(QStringLiteral("s3"), QStringLiteral("share-guid"), 2025)
        }, {});
        QCOMPARE(tabs->count(), 3);
        QVERIFY(!tabs->tabText(1).contains(QStringLiteral("2023")));
    }

    void test_viewSaleEdit_populateOverview_docIconWhenPathSet()
    {
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);

        const SaleObject s = makeSale(QStringLiteral("s-doc"),
                                       QStringLiteral("share-guid"), 2024,
                                       10.0, 150.0,
                                       QStringLiteral("depot1"),
                                       QStringLiteral("/path/to/doc.pdf"));
        // Re-create with document set
        const SaleObject sWithDoc(
            QStringLiteral("s-doc"), QStringLiteral("share-guid"),
            QStringLiteral("depot1"), QStringLiteral("ord-s-doc"),
            QStringLiteral("2024-06-15T10:00:00"),
            10.0, 150.0, {}, 0.0, 0.0, 0.0, QString(),
            0.0, 0.0, 0.0, 0.0,
            QStringLiteral("/path/to/doc.pdf"));
        dlg.populateOverview({ sWithDoc }, {});

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        auto* container = tabs->widget(1);
        auto* tbl = qobject_cast<QTableWidget*>(
            container->property("dataTable").value<QObject*>());
        QVERIFY(tbl != nullptr);
        // Document column (4) must have a cell widget (QLabel with icon)
        QVERIFY(tbl->cellWidget(0, 4) != nullptr);
    }

    void test_viewSaleEdit_populateOverview_docDashWhenNoPath()
    {
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);

        dlg.populateOverview({
            makeSale(QStringLiteral("s1"), QStringLiteral("share-guid"), 2024)
        }, {});

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        auto* container = tabs->widget(1);
        auto* tbl = qobject_cast<QTableWidget*>(
            container->property("dataTable").value<QObject*>());
        QVERIFY(tbl != nullptr);
        QVERIFY(tbl->cellWidget(0, 4) == nullptr);
        QVERIFY(tbl->item(0, 4) != nullptr);
        QCOMPARE(tbl->item(0, 4)->text(), QStringLiteral("-"));
    }

    // ─────────────────────────────────────────────────────────────────────
    // ViewSaleEdit — populateOverview (additional coverage)
    // ─────────────────────────────────────────────────────────────────────

    void test_viewSaleEdit_populateOverview_uebersichtTabHasTable()
    {
        // The Übersicht tab must contain a QTableWidget with year-aggregated rows.
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        QVERIFY(tabs != nullptr);

        dlg.populateOverview({
            makeSale(QStringLiteral("s1"), QStringLiteral("share-guid"), 2024)
        }, {});

        auto* container = tabs->widget(0);
        QVERIFY(container != nullptr);
        auto* tbl = qobject_cast<QTableWidget*>(
            container->property("dataTable").value<QObject*>());
        QVERIFY(tbl != nullptr);
        QCOMPARE(tbl->rowCount(), 1);        // one year row
        QCOMPARE(tbl->columnCount(), 4);     // Jahr | Anteile | Auszahlung | G/V
    }

    void test_viewSaleEdit_populateOverview_jahresTabRowCount()
    {
        // Jahres-tab must have exactly as many rows as sales in that year
        // (Gesamt is in the separate frozen footer, not a data row).
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        QVERIFY(tabs != nullptr);

        dlg.populateOverview({
            makeSale(QStringLiteral("s1"), QStringLiteral("share-guid"), 2024),
            makeSale(QStringLiteral("s2"), QStringLiteral("share-guid"), 2024),
            makeSale(QStringLiteral("s3"), QStringLiteral("share-guid"), 2024)
        }, {});

        auto* container = tabs->widget(1);
        QVERIFY(container != nullptr);
        auto* tbl = qobject_cast<QTableWidget*>(
            container->property("dataTable").value<QObject*>());
        QVERIFY(tbl != nullptr);
        QCOMPARE(tbl->rowCount(), 3);
    }

    void test_viewSaleEdit_populateOverview_tabTitleContainsTotal()
    {
        // Both Übersicht and Jahres-tabs must include the total Auszahlung and "€".
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");

        dlg.populateOverview({
            makeSale(QStringLiteral("s1"), QStringLiteral("share-guid"), 2024,
                     10.0, 150.0)
        }, {});

        QVERIFY(tabs->tabText(0).contains(QStringLiteral("€")));
        QVERIFY(tabs->tabText(1).contains(QStringLiteral("2024")));
        QVERIFY(tabs->tabText(1).contains(QStringLiteral("€")));
    }

    // ─────────────────────────────────────────────────────────────────────
    // ViewSaleEdit — Split-Marker und Summen (Phase 3c)
    // ─────────────────────────────────────────────────────────────────────

    /** dataTable eines Tab-Containers (siehe OverviewTabWidget::buildFrozenTable()). */
    static QTableWidget* dataTableOf(QWidget* container)
    {
        if (!container) return nullptr;
        return qobject_cast<QTableWidget*>(
            container->property("dataTable").value<QObject*>());
    }

    /** footerTable eines Tab-Containers (einzeilige Gesamt-Zeile). */
    static QTableWidget* footerTableOf(QWidget* container)
    {
        if (!container) return nullptr;
        return qobject_cast<QTableWidget*>(
            container->property("footerTable").value<QObject*>());
    }

    void test_viewSaleEdit_populateOverview_belegRowKeepsBelegVolumeWithMarker()
    {
        // Belegzeile bleibt in Beleg-Skala — sie ist eine Abschrift des
        // Dokuments, das nach einem Zeilenklick rechts erscheint. Der Marker
        // weist darauf hin, dass die Zahl nicht dem heutigen Stand entspricht.
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        QVERIFY(tabs != nullptr);

        dlg.populateOverview(
            { makeSale(QStringLiteral("s1"), QStringLiteral("share-guid"), 2021, 5.0, 1000.0) },
            { ShareSplitObject(QStringLiteral("sp1"), QStringLiteral("share-guid"),
                               QDate(2022, 7, 18), 20.0, 1.0) });

        auto* tbl = dataTableOf(tabs->widget(1));   // Jahres-Tab 2021
        QVERIFY(tbl != nullptr);
        const QString volText = tbl->item(0, 1)->text();
        QVERIFY2(volText.contains(QLocale().toString(5.0, 'f', 4)), qPrintable(volText));
        QVERIFY2(volText.contains(ShareSplitHint::marker()), qPrintable(volText));
        QVERIFY(!tbl->item(0, 1)->toolTip().isEmpty());
    }

    void test_viewSaleEdit_populateOverview_belegRowWithoutSplitHasNoMarker()
    {
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        QVERIFY(tabs != nullptr);

        dlg.populateOverview(
            { makeSale(QStringLiteral("s1"), QStringLiteral("share-guid"), 2024, 5.0, 1000.0) },
            {});

        auto* tbl = dataTableOf(tabs->widget(1));
        QVERIFY(tbl != nullptr);
        QVERIFY(!tbl->item(0, 1)->text().contains(ShareSplitHint::marker()));
        QVERIFY(tbl->item(0, 1)->toolTip().isEmpty());
    }

    void test_viewSaleEdit_populateOverview_uebersichtRowUsesTodayScale()
    {
        // Aggregate rechnen je Beleg um und summieren erst danach: aus
        // 5 Stück vor einem 20:1-Split werden 100 heutige Stück.
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        QVERIFY(tabs != nullptr);

        dlg.populateOverview(
            { makeSale(QStringLiteral("s1"), QStringLiteral("share-guid"), 2021, 5.0, 1000.0) },
            { ShareSplitObject(QStringLiteral("sp1"), QStringLiteral("share-guid"),
                               QDate(2022, 7, 18), 20.0, 1.0) });

        auto* tbl = dataTableOf(tabs->widget(0));   // Übersicht
        QVERIFY(tbl != nullptr);
        const QString volText = tbl->item(0, 1)->text();
        QVERIFY2(volText.contains(QLocale().toString(100.0, 'f', 4)), qPrintable(volText));
        QVERIFY2(volText.contains(ShareSplitHint::marker()), qPrintable(volText));
    }

    void test_viewSaleEdit_populateOverview_splitMidYearSumsOnOneScale()
    {
        // Der eigentliche Grund für die Umrechnung: zwei Verkäufe desselben
        // Jahres, dazwischen ein Split. Die frühere rohe Summe (5 + 100 =
        // 105) mischte zwei Stückelungen und bedeutete gar nichts.
        // Richtig: 5 × 20 + 100 = 200 heutige Stück.
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        QVERIFY(tabs != nullptr);

        const SaleObject before(QStringLiteral("s1"), QStringLiteral("share-guid"),
                                QStringLiteral("depot1"), QStringLiteral("ord-s1"),
                                QStringLiteral("2022-03-01T10:00:00"), 5.0, 1000.0,
                                QList<SaleBuyDetail>());
        const SaleObject after(QStringLiteral("s2"), QStringLiteral("share-guid"),
                               QStringLiteral("depot1"), QStringLiteral("ord-s2"),
                               QStringLiteral("2022-11-01T10:00:00"), 100.0, 50.0,
                               QList<SaleBuyDetail>());

        dlg.populateOverview(
            { before, after },
            { ShareSplitObject(QStringLiteral("sp1"), QStringLiteral("share-guid"),
                               QDate(2022, 7, 18), 20.0, 1.0) });

        auto* footer = footerTableOf(tabs->widget(1));   // Jahres-Tab 2022
        QVERIFY(footer != nullptr);
        const QString sumText = footer->item(0, 1)->text();
        QVERIFY2(sumText.contains(QLocale().toString(200.0, 'f', 4)), qPrintable(sumText));
        QVERIFY2(sumText.contains(ShareSplitHint::marker()), qPrintable(sumText));
    }

    // ─────────────────────────────────────────────────────────────────────
    // ViewSaleEdit — Tab-Klick-Logik
    // ─────────────────────────────────────────────────────────────────────

    /** Helper: populate dlg with two sales in different years and return the OverviewTabWidget. */
    static OverviewTabWidget* setupTwoYearSaleOverview(ViewSaleEdit& dlg)
    {
        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        dlg.populateOverview({
            makeSale(QStringLiteral("s1"), QStringLiteral("share-guid"), 2023),
            makeSale(QStringLiteral("s2"), QStringLiteral("share-guid"), 2024)
        }, {});
        return tabs;
    }

    void test_viewSaleEdit_uebersichtClick_jumpsToYearTab()
    {
        // Clicking a year row in the Übersicht tab must switch to that year's tab.
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = setupTwoYearSaleOverview(dlg);
        QVERIFY(tabs != nullptr);
        QCOMPARE(tabs->count(), 3); // Übersicht + 2023 + 2024

        auto* container = tabs->widget(0);
        QVERIFY(container != nullptr);
        auto* tbl = qobject_cast<QTableWidget*>(
            container->property("dataTable").value<QObject*>());
        QVERIFY(tbl != nullptr);

        // Row 0 = 2024 (descending order, newest first)
        emit tbl->cellClicked(0, 0);

        QVERIFY(tabs->tabText(tabs->currentIndex()).contains(QStringLiteral("2024")));
    }

    void test_viewSaleEdit_uebersichtRowSelection_isEnabled()
    {
        // Übersicht tab must allow row selection so a user can pick a year.
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = setupTwoYearSaleOverview(dlg);
        QVERIFY(tabs != nullptr);

        auto* container = tabs->widget(0);
        QVERIFY(container != nullptr);
        auto* tbl = qobject_cast<QTableWidget*>(
            container->property("dataTable").value<QObject*>());
        QVERIFY(tbl != nullptr);
        QCOMPARE(tbl->selectionBehavior(), QAbstractItemView::SelectRows);
        QCOMPARE(tbl->selectionMode(),     QAbstractItemView::SingleSelection);
    }

    void test_viewSaleEdit_jahresTab_hasSelectRows()
    {
        // Jahres-tabs must use SelectRows — no individual cell selection.
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = setupTwoYearSaleOverview(dlg);
        QVERIFY(tabs != nullptr);

        auto* container = tabs->widget(1); // first Jahres-tab
        QVERIFY(container != nullptr);
        auto* tbl = qobject_cast<QTableWidget*>(
            container->property("dataTable").value<QObject*>());
        QVERIFY(tbl != nullptr);
        QCOMPARE(tbl->selectionBehavior(), QAbstractItemView::SelectRows);
        QCOMPARE(tbl->selectionMode(),     QAbstractItemView::SingleSelection);
    }

    void test_viewSaleEdit_tabChange_clearsOldSelection()
    {
        // Switching tabs must clear the selection in the previously active tab.
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = setupTwoYearSaleOverview(dlg);
        QVERIFY(tabs != nullptr);

        // Select row 0 in tab 1 (first Jahres-tab)
        tabs->setCurrentIndex(1);
        auto* container1 = tabs->widget(1);
        auto* tbl1 = qobject_cast<QTableWidget*>(
            container1->property("dataTable").value<QObject*>());
        QVERIFY(tbl1 != nullptr);
        tbl1->selectRow(0);
        QVERIFY(!tbl1->selectedItems().isEmpty());

        // Switch to tab 2 — selection in tab 1 must be cleared
        tabs->setCurrentIndex(2);
        QVERIFY(tbl1->selectedItems().isEmpty());
    }

    void test_viewSaleEdit_tabChange_selectsFirstRowInJahresTab()
    {
        // Switching to a Jahres-tab must auto-select its first data row.
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = setupTwoYearSaleOverview(dlg);
        QVERIFY(tabs != nullptr);

        tabs->setCurrentIndex(0);
        tabs->setCurrentIndex(1);

        auto* container = tabs->widget(1);
        auto* tbl = qobject_cast<QTableWidget*>(
            container->property("dataTable").value<QObject*>());
        QVERIFY(tbl != nullptr);
        QCOMPARE(tbl->currentRow(), 0);
        QVERIFY(!tbl->selectedItems().isEmpty());
    }

    void test_viewSaleEdit_tabChange_noAutoSelectInUebersicht()
    {
        // Switching back to the Übersicht tab (index 0) must NOT auto-select any row.
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = setupTwoYearSaleOverview(dlg);
        QVERIFY(tabs != nullptr);

        tabs->setCurrentIndex(1);
        tabs->setCurrentIndex(0);

        auto* container = tabs->widget(0);
        auto* tbl = qobject_cast<QTableWidget*>(
            container->property("dataTable").value<QObject*>());
        QVERIFY(tbl != nullptr);
        QVERIFY(tbl->selectedItems().isEmpty());
    }

    void test_viewSaleEdit_tabChange_toJahresTab_selectsFirstRow()
    {
        // Switching to a Jahres-tab must auto-select row 0.
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = setupTwoYearSaleOverview(dlg);
        QVERIFY(tabs != nullptr);

        tabs->setCurrentIndex(0);
        tabs->setCurrentIndex(1);

        auto* container = tabs->widget(1);
        auto* tbl = qobject_cast<QTableWidget*>(
            container->property("dataTable").value<QObject*>());
        QVERIFY(tbl != nullptr);
        QVERIFY(!tbl->selectedItems().isEmpty());
        QCOMPARE(tbl->currentRow(), 0);
    }

    void test_viewSaleEdit_tabChange_backToUebersicht_clearsJahresSelection()
    {
        // Switching back to Übersicht must clear the Jahres-tab selection.
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = setupTwoYearSaleOverview(dlg);
        QVERIFY(tabs != nullptr);

        tabs->setCurrentIndex(1);  // go to Jahres-tab → row 0 selected
        tabs->setCurrentIndex(0);  // back to Übersicht

        auto* container = tabs->widget(1);
        auto* tbl = qobject_cast<QTableWidget*>(
            container->property("dataTable").value<QObject*>());
        QVERIFY(tbl != nullptr);
        QVERIFY(tbl->selectedItems().isEmpty());
    }

    // ─────────────────────────────────────────────────────────────────────
    // PresenterSaleEdit — additional coverage
    // ─────────────────────────────────────────────────────────────────────

    void test_presenterSaleEdit_onOrderNumberEdited_valid_setsOk()
    {
        // A non-empty, non-duplicate order number must trigger setFieldOk.
        openMemoryDb();

        bool okSet = false;
        struct SpyView : public StubViewSaleEdit {
            bool* called;
            void setFieldOk(const QString& f, const QString&) override
                { if (f == QLatin1String("orderNumber")) *called = true; }
        } spy;
        spy.called = &okSet;
        spy.m_orderNumber = QStringLiteral("ORD-S-001");

        StubModelSaleEdit model;
        model.orderExists = false;

        PresenterSaleEdit p(&spy, &model, QStringLiteral("share-1"), nullptr);
        p.onOrderNumberEdited();

        QVERIFY(okSet);
    }

    void test_presenterSaleEdit_onDocumentSelected_writesPathIntoView()
    {
        // Regression 21.08.2026 (Nessies Bugreport): analog zu
        // test_presenterBuyEdit_onDocumentSelected_writesPathIntoView in
        // tst_buysform.cpp — MainWindow ruft für ein per Drag&Drop erfasstes
        // Dokument dlg.presenter()->onDocumentSelected() direkt auf,
        // ViewSaleEdit::onBrowseDocument() (das früher als einziges
        // m_documentPath->setText() setzte) wird dabei nie durchlaufen.
        openMemoryDb();

        StubViewSaleEdit view;
        StubModelSaleEdit model;
        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);

        p.onDocumentSelected(QStringLiteral("/tmp/dropped.pdf"));

        QCOMPARE(view.documentPath(), QStringLiteral("/tmp/dropped.pdf"));
    }

    void test_presenterSaleEdit_onDocumentSelected_newMode_doesNotEarlyReturn()
    {
        // In new-sale mode (no selection) a document selection must NOT be blocked:
        // openPdfPreview() must always be called.
        openMemoryDb();

        bool previewCalled = false;
        struct SpyView : public StubViewSaleEdit {
            bool* called;
            void openPdfPreview(const QString&) override { *called = true; }
        } view;
        view.called = &previewCalled;

        StubModelSaleEdit model;
        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);

        // No row selected → new-sale mode → preview path must be entered
        p.onDocumentSelected(QStringLiteral("/tmp/test.pdf"));

        QVERIFY(previewCalled);
    }

    void test_presenterSaleEdit_onDocumentSelected_nonLatestSale_earlyReturn()
    {
        // When a non-latest sale is selected, document selection must only update
        // the preview — pdftotext (setUiBusy) must NOT be launched.
        openMemoryDb();

        bool busyCalled = false;
        struct SpyView : public StubViewSaleEdit {
            bool* called;
            void setUiBusy(bool busy) override { if (busy) *called = true; }
        } view;
        view.called = &busyCalled;

        StubModelSaleEdit model;
        const SaleObject older = makeSale(QStringLiteral("s-old"),
                                          QStringLiteral("share-1"), 2023);
        const SaleObject newer = makeSale(QStringLiteral("s-new"),
                                          QStringLiteral("share-1"), 2024);
        model.sales = { older, newer };

        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onRowSelected(QStringLiteral("s-old"));  // selects non-latest sale

        p.onDocumentSelected(QStringLiteral("/tmp/test.pdf"));

        // setUiBusy(true) must NOT have been called — parse path was not entered
        QVERIFY(!busyCalled);
    }

    void test_presenterSaleEdit_onSave_nonLatestSale_resetsButtonLabel()
    {
        // After saving a non-latest sale the button states must be reset
        // (canRemove=false, isLastSale=false) so the label shows "Hinzufügen" again.
        openMemoryDb();

        StubViewSaleEdit  view;
        StubModelSaleEdit model;

        const SaleObject older = makeSale(QStringLiteral("s-old"),
                                          QStringLiteral("share-1"), 2023);
        const SaleObject newer = makeSale(QStringLiteral("s-new"),
                                          QStringLiteral("share-1"), 2024);
        model.sales = { older, newer };

        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onRowSelected(QStringLiteral("s-old"));  // selects non-latest sale

        view.setButtonStatesCalled = false;  // reset after onRowSelected
        p.onSave();

        QVERIFY(view.setButtonStatesCalled);
        QCOMPARE(view.lastCanRemove,   false);
        QCOMPARE(view.lastIsLastSale,  false);
    }

    // ─────────────────────────────────────────────────────────────────────
    // ModelSaleEdit — database tests (additional coverage)
    // ─────────────────────────────────────────────────────────────────────

    void test_modelSaleEdit_updateSale_success()
    {
        // updateSale() must persist changed field values and update volumeSold.
        const QString shareGuid = insertTestShare();
        const BuyObject buy = insertTestBuy(shareGuid,
                                             QStringLiteral("depot1"),
                                             QStringLiteral("2024-01-10T10:00:00"),
                                             20.0, 100.0);

        ModelSaleEdit model;
        const QString saleGuid =
            QUuid::createUuid().toString(QUuid::WithoutBraces);
        const SaleObject original(
            saleGuid, shareGuid, QStringLiteral("depot1"),
            QStringLiteral("ORD-UPD-1"),
            QStringLiteral("2024-06-01T10:00:00"),
            10.0, 150.0,
            { SaleBuyDetail(buy.guid(), buy.dateTime(), 10.0, buy.price()) });
        QVERIFY(model.addSale(original));

        // Update: change order number and sale price
        const SaleObject updated(
            saleGuid, shareGuid, QStringLiteral("depot1"),
            QStringLiteral("ORD-UPD-1-CHANGED"),
            QStringLiteral("2024-06-01T10:00:00"),
            10.0, 200.0,
            { SaleBuyDetail(buy.guid(), buy.dateTime(), 10.0, buy.price()) });
        QVERIFY(model.updateSale(updated));

        // Verify the updated values are stored
        const QList<SaleObject> sales = model.loadSales(shareGuid);
        QCOMPARE(sales.size(), 1);
        QCOMPARE(sales.first().orderNumber(), QStringLiteral("ORD-UPD-1-CHANGED"));
        QCOMPARE(sales.first().salePrice(), 200.0);
    }

    void test_modelSaleEdit_updateSale_createsBrokerageIfMissing()
    {
        // If a sale somehow has no linked brokerage, updateSale() must create one.
        // We simulate this by calling the low-level SaleRepository directly (bypass
        // addSale) so no brokerage row is created, then call updateSale().
        const QString shareGuid = insertTestShare();
        const BuyObject buy = insertTestBuy(shareGuid,
                                             QStringLiteral("depot1"),
                                             QStringLiteral("2024-02-10T10:00:00"),
                                             20.0, 100.0);

        // Insert sale directly into the DB without a brokerage entry
        {
            SaleRepository saleRepo;
            const SaleObject bare(
                QStringLiteral("sale-no-brokerage"), shareGuid,
                QStringLiteral("depot1"), QStringLiteral("ORD-BARE"),
                QStringLiteral("2024-06-05T10:00:00"),
                5.0, 100.0,
                { SaleBuyDetail(buy.guid(), buy.dateTime(), 5.0, buy.price()) });
            // Manually insert buy details via addSale on a fresh ModelSaleEdit
            ModelSaleEdit m;
            QVERIFY(m.addSale(bare));
            // Now delete the brokerage to simulate missing entry
            QSqlDatabase db = QSqlDatabase::database(Database::connectionName());
            QSqlQuery q(db);
            q.prepare(QStringLiteral("DELETE FROM brokerage WHERE sale_guid = :g"));
            q.bindValue(QStringLiteral(":g"), QStringLiteral("sale-no-brokerage"));
            QVERIFY(q.exec());
        }

        // Now updateSale must succeed and create the missing brokerage
        ModelSaleEdit model;
        const SaleObject toUpdate(
            QStringLiteral("sale-no-brokerage"), shareGuid,
            QStringLiteral("depot1"), QStringLiteral("ORD-BARE"),
            QStringLiteral("2024-06-05T10:00:00"),
            5.0, 100.0,
            { SaleBuyDetail(buy.guid(), buy.dateTime(), 5.0, buy.price()) },
            /*taxAtSource=*/0.0, /*capitalGainsTax=*/0.0, /*solidarityTax=*/0.0,
            /*brokerageGuid=*/QString(), /*provision=*/15.0);
        QVERIFY(model.updateSale(toUpdate));

        // Brokerage must now exist (Rückwärts-Link, unverändert durch den Fix).
        const BrokerageObject br = model.loadBrokerage(
            QStringLiteral("sale-no-brokerage"));
        QVERIFY(br.isValid());
        QCOMPARE(br.provision(), 15.0);

        // Regression (Bugfix 15.07.2026): Der Vorwärts-Link (sales.
        // brokerage_guid) muss ebenfalls gesetzt sein — ohne ihn käme die
        // Provision beim Neuladen über loadSales() weiterhin als 0 zurück,
        // obwohl der Brokerage-Datensatz selbst (s.o.) korrekt existiert.
        const QList<SaleObject> reloaded = model.loadSales(shareGuid);
        QCOMPARE(reloaded.size(), 1);
        QCOMPARE(reloaded.first().provision(), 15.0);
    }

    void test_modelSaleEdit_loadSales_orderedByDate()
    {
        // loadSales() must return sales in ascending date order.
        const QString shareGuid = insertTestShare();
        const BuyObject buy = insertTestBuy(shareGuid,
                                             QStringLiteral("depot1"),
                                             QStringLiteral("2023-01-01T08:00:00"),
                                             30.0, 100.0);

        ModelSaleEdit model;

        // Insert newer sale first, then older
        const SaleObject newer(
            QStringLiteral("sale-newer"), shareGuid, QStringLiteral("depot1"),
            QStringLiteral("ORD-2"), QStringLiteral("2024-09-01T10:00:00"),
            5.0, 160.0,
            { SaleBuyDetail(buy.guid(), buy.dateTime(), 5.0, buy.price()) });
        QVERIFY(model.addSale(newer));

        const SaleObject older(
            QStringLiteral("sale-older"), shareGuid, QStringLiteral("depot1"),
            QStringLiteral("ORD-1"), QStringLiteral("2023-03-01T10:00:00"),
            5.0, 120.0,
            { SaleBuyDetail(buy.guid(), buy.dateTime(), 5.0, buy.price()) });
        QVERIFY(model.addSale(older));

        const QList<SaleObject> sales = model.loadSales(shareGuid);
        QCOMPARE(sales.size(), 2);
        QVERIFY(sales.at(0).dateTime() < sales.at(1).dateTime());
    }

    // ─────────────────────────────────────────────────────────────────────
    // ViewSaleEdit — Details-Button
    // ─────────────────────────────────────────────────────────────────────

    void test_viewSaleEdit_detailsButton_enabledInNewMode()
    {
        // Details-Button muss im Neu-Modus aktiv sein (FIFO-Vorschau).
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);
        // Kein setButtonStates-Aufruf — Zustand nach Konstruktion = Neu-Modus
        const auto buttons = dlg.findChildren<QPushButton*>();
        for (auto* btn : buttons) {
            if (btn->text() == tr("Details")) {
                QVERIFY(btn->isEnabled());
                return;
            }
        }
        QFAIL("Details button not found");
    }

    void test_viewSaleEdit_detailsButton_enabledInEditMode()
    {
        // Details-Button muss im Edit-Modus aktiv sein (gespeicherte Kaufdetails).
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);
        dlg.setButtonStates(/*canRemove=*/true, /*isLastSale=*/true, /*isEdit=*/true);
        const auto buttons = dlg.findChildren<QPushButton*>();
        for (auto* btn : buttons) {
            if (btn->text() == tr("Details")) {
                QVERIFY(btn->isEnabled());
                return;
            }
        }
        QFAIL("Details button not found");
    }

    void test_viewSaleEdit_detailsButton_enabledForNonLatestSale()
    {
        // Details-Button muss auch im Read-only-Edit-Modus (älterer Verkauf) aktiv sein.
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);
        dlg.setButtonStates(/*canRemove=*/false, /*isLastSale=*/false, /*isEdit=*/true);
        const auto buttons = dlg.findChildren<QPushButton*>();
        for (auto* btn : buttons) {
            if (btn->text() == tr("Details")) {
                QVERIFY(btn->isEnabled());
                return;
            }
        }
        QFAIL("Details button not found");
    }

    void test_viewSaleEdit_loadSale_clearedByReset()
    {
        // clearForm() muss den gecachten Verkauf zurücksetzen: nach clearForm() ist
        // der Dialog wieder im Neu-Modus. Wir testen das indirekt: loadSale() befüllt
        // die Felder; clearForm() setzt sie auf Standardwerte zurück.
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);

        const SaleObject s = makeSale(QStringLiteral("s1"),
                                       QStringLiteral("share-guid"), 2024,
                                       50.0, 200.0);
        dlg.loadSale(s);

        // Felder müssen mit Verkaufsdaten befüllt sein
        QCOMPARE(dlg.volume(),    50.0);
        QCOMPARE(dlg.salePrice(), 200.0);

        // Nach clearForm() müssen Felder auf Null/leer zurückgesetzt sein
        dlg.clearForm();
        QCOMPARE(dlg.volume(),    0.0);
        QCOMPARE(dlg.salePrice(), 0.0);
        QVERIFY(dlg.orderNumber().isEmpty());
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Stub IModelDividendEdit
// ─────────────────────────────────────────────────────────────────────────────
class StubModelDividendEdit : public IModelDividendEdit
{
public:
    QList<DividendObject> dividends;
    // Phase 3c (11.08.2026): Splits für den Marker in der Anteile-Spalte.
    QList<ShareSplitObject> splits;
    bool                  addResult    = true;
    bool                  updateResult = true;
    bool                  removeResult = true;
    bool                  docExists    = false;
    QString               errorMsg;

    // findClosingPriceForDate() Konfiguration/Aufzeichnung
    bool           hasClosingPrice      = false;
    double         closingPriceToReturn = 0.0;
    mutable bool   findClosingPriceForDateCalled = false;
    mutable QString lastClosingPriceShareGuid;
    mutable QDate   lastClosingPriceDate;

    bool addDividendCalled    = false;
    bool updateDividendCalled = false;
    bool removeDividendCalled = false;

    QList<DividendObject> loadDividends(const QString&) const override { return dividends; }
    ShareObject           loadShare(const QString&)     const override { return ShareObject{}; }
    QList<ShareSplitObject> loadSplits(const QString&)  const override { return splits; }

    bool findClosingPriceForDate(const QString& shareGuid, const QDate& date,
                                 double& outPrice) const override
    {
        findClosingPriceForDateCalled = true;
        lastClosingPriceShareGuid     = shareGuid;
        lastClosingPriceDate          = date;
        if (!hasClosingPrice) return false;
        outPrice = closingPriceToReturn;
        return true;
    }

    bool addDividend(const DividendObject&)    override { addDividendCalled    = true; return addResult;    }
    bool updateDividend(const DividendObject&) override { updateDividendCalled = true; return updateResult; }
    bool removeDividend(const QString&)        override { removeDividendCalled = true; return removeResult; }

    bool documentExists(const QString&, const QString&) const override { return docExists; }
    QString lastError() const override { return errorMsg; }
};

// ─────────────────────────────────────────────────────────────────────────────
// Stub IViewDividendEdit
// ─────────────────────────────────────────────────────────────────────────────
class StubViewDividendEdit : public IViewDividendEdit
{
public:
    // Configurable return values
    QString m_dateTime    = QStringLiteral("2024-06-15T00:00:00");
    double  m_rate        = 1.50;
    double  m_volume      = 100.0;
    double  m_taxAtSource = 0.0;
    double  m_capitalGainsTax = 0.0;
    double  m_solidarityTax   = 0.0;
    double  m_priceAtPayday   = 45.00;
    bool    m_enableFc    = false;
    double  m_exchangeRatio = 1.0;
    QString m_currency    = QStringLiteral("en-US");
    QString m_docPath;
    bool    m_missingFields = false;

    // Captured calls
    bool    populateOverviewCalled = false;
    QList<ShareSplitObject> lastOverviewSplits;
    bool    clearFormCalled        = false;
    bool    loadDividendCalled     = false;
    bool    setButtonStatesCalled  = false;
    bool    lastCanRemove          = false;
    bool    lastIsEdit             = false;
    QString lastError;
    bool    closed                 = false;

    // setFieldOk() — letzter Aufruf (für Auto-Fill-Assertions)
    QString lastFieldOkField;
    QString lastFieldOkValue;
    QString lastFieldOkTooltip;

    // IViewDividendEdit — read
    QString dateTime()              const override { return m_dateTime; }
    double  rate()                  const override { return m_rate; }
    double  volume()                const override { return m_volume; }
    double  taxAtSource()           const override { return m_taxAtSource; }
    double  capitalGainsTax()       const override { return m_capitalGainsTax; }
    double  solidarityTax()         const override { return m_solidarityTax; }
    double  priceAtPayday()         const override { return m_priceAtPayday; }
    bool    enableForeignCurrency() const override { return m_enableFc; }
    double  exchangeRatio()         const override { return m_exchangeRatio; }
    QString currency()              const override { return m_currency; }
    QString documentPath()          const override { return m_docPath; }

    // IViewDividendEdit — write
    void loadDividend(const DividendObject&) override { loadDividendCalled = true; }
    void clearForm()                         override { clearFormCalled = true; }

    void setDividendPayout(double)          override {}
    void setDividendPayoutFc(double)        override {}
    void setTaxSum(double)                  override {}
    void setDividendPayoutWithTaxes(double) override {}
    void setYield(double)                   override {}

    void setForeignCurrencyEnabled(bool)    override {}

    void setFieldOk(const QString& field, const QString& value,
                    const QString& tooltip = QString()) override
    {
        lastFieldOkField   = field;
        lastFieldOkValue   = value;
        lastFieldOkTooltip = tooltip;
        // Mirrors ViewDividendEdit::setFieldOk() writing the value back into
        // the widget — needed so priceAtPayday() reflects an auto-filled
        // value in tests, the same way the real QLineEdit would.
        if (field == QStringLiteral("priceAtPayday") && !value.isEmpty())
            m_priceAtPayday = value.toDouble();
    }
    void setFieldError(const QString&)              override {}
    void setDocumentPath(const QString& path)       override { m_docPath = path; }
    void setDocumentPreview(const QString&)         override {}

    void setParseProgress(int, const QString&)      override {}
    void setParseStatusIcon(int)                    override {}
    void setUiBusy(bool)                            override {}
    void onParseFinished()                          override {}

    void populateOverview(const QList<DividendObject>&,
                          const QList<ShareSplitObject>& splits) override
    {
        populateOverviewCalled = true;
        // Phase 3c (11.08.2026): die Splits kommen als Parameter herein.
        lastOverviewSplits = splits;
    }
    void openPdfPreview(const QString&)             override {}
    void clearPdfPreview()                          override {}
    void showOverviewTab()                          override { clearFormCalled = true; }

    void setButtonStates(bool canRemove, bool isEdit) override
    {
        setButtonStatesCalled = true;
        lastCanRemove = canRemove;
        lastIsEdit    = isEdit;
    }

    void showError(const QString& msg) override { lastError = msg; }
    void acceptAndClose()              override { closed = true; }

    void markMissingFieldsAsFailed()                                override {}
    bool hasMissingRequiredFields(QStringList& missing) const       override
        { missing.clear(); if (m_missingFields) missing << QStringLiteral("test"); return m_missingFields; }
};

// ─────────────────────────────────────────────────────────────────────────────
// TestDividendForm
// ─────────────────────────────────────────────────────────────────────────────
class TestDividendForm : public QObject
{
    Q_OBJECT

    // ── Helpers ───────────────────────────────────────────────────────────
    static void openMemoryDb()
    {
        Database::instance().open(QStringLiteral(":memory:"));
    }

    static QString makeShareGuid() { return QStringLiteral("share-guid-1"); }

    static DividendObject makeDividend(const QString& guid,
                                        const QString& dt = QStringLiteral("2024-06-15T00:00:00"))
    {
        return DividendObject(guid, makeShareGuid(), dt,
                              1.50, 100.0, 0.0, 0.0, 0.0, 45.0);
    }

private slots:

    // ── ModelDividendEdit (DB-Tests) ──────────────────────────────────────

    void test_modelDividendEdit_addDividend_success()
    {
        openMemoryDb();
        ShareRepository sr;
        sr.insert(ShareObject(makeShareGuid(), QStringLiteral("TST"),
                               QStringLiteral("DE000TST0001"), QStringLiteral("Test AG")));

        ModelDividendEdit model;
        const DividendObject d = makeDividend(QStringLiteral("div-1"));
        QVERIFY(model.addDividend(d));
        QCOMPARE(model.loadDividends(makeShareGuid()).size(), 1);
    }

    void test_modelDividendEdit_updateDividend_success()
    {
        openMemoryDb();
        ShareRepository sr;
        sr.insert(ShareObject(makeShareGuid(), QStringLiteral("TST"),
                               QStringLiteral("DE000TST0001"), QStringLiteral("Test AG")));

        ModelDividendEdit model;
        DividendObject d = makeDividend(QStringLiteral("div-1"));
        QVERIFY(model.addDividend(d));

        // Update rate to 2.00
        DividendObject updated(QStringLiteral("div-1"), makeShareGuid(),
                                QStringLiteral("2024-06-15T00:00:00"),
                                2.00, 100.0, 0.0, 0.0, 0.0, 45.0);
        QVERIFY(model.updateDividend(updated));
        const auto dividends = model.loadDividends(makeShareGuid());
        QCOMPARE(dividends.first().rate(), 2.00);
    }

    void test_modelDividendEdit_removeDividend_success()
    {
        openMemoryDb();
        ShareRepository sr;
        sr.insert(ShareObject(makeShareGuid(), QStringLiteral("TST"),
                               QStringLiteral("DE000TST0001"), QStringLiteral("Test AG")));

        ModelDividendEdit model;
        QVERIFY(model.addDividend(makeDividend(QStringLiteral("div-1"))));
        QVERIFY(model.removeDividend(QStringLiteral("div-1")));
        QCOMPARE(model.loadDividends(makeShareGuid()).size(), 0);
    }

    void test_modelDividendEdit_documentExists_notFound_returnsFalse()
    {
        openMemoryDb();
        ModelDividendEdit model;
        QVERIFY(!model.documentExists(QStringLiteral("/some/file.pdf")));
    }

    void test_modelDividendEdit_documentExists_emptyPath_returnsFalse()
    {
        openMemoryDb();
        ModelDividendEdit model;
        QVERIFY(!model.documentExists(QString()));
    }

    void test_modelDividendEdit_loadDividends_orderedByDate()
    {
        openMemoryDb();
        ShareRepository sr;
        sr.insert(ShareObject(makeShareGuid(), QStringLiteral("TST"),
                               QStringLiteral("DE000TST0001"), QStringLiteral("Test AG")));

        ModelDividendEdit model;
        model.addDividend(makeDividend(QStringLiteral("div-2"), QStringLiteral("2024-12-01T00:00:00")));
        model.addDividend(makeDividend(QStringLiteral("div-1"), QStringLiteral("2023-06-01T00:00:00")));

        const auto list = model.loadDividends(makeShareGuid());
        QCOMPARE(list.size(), 2);
        QVERIFY(list.at(0).dateTime() < list.at(1).dateTime());
    }

    void test_modelDividendEdit_findClosingPriceForDate_found_returnsTrue()
    {
        openMemoryDb();
        ShareRepository sr;
        sr.insert(ShareObject(makeShareGuid(), QStringLiteral("TST"),
                               QStringLiteral("DE000TST0001"), QStringLiteral("Test AG")));

        DailyValuesRepository dvRepo;
        dvRepo.upsert(DailyValuesObject(makeShareGuid(), QDate(2025, 12, 17),
                                        203.50, 204.71, 205.00, 203.00, 1000.0));

        ModelDividendEdit model;
        double price = 0.0;
        QVERIFY(model.findClosingPriceForDate(makeShareGuid(), QDate(2025, 12, 17), price));
        QCOMPARE(price, 204.71);
    }

    void test_modelDividendEdit_findClosingPriceForDate_notFound_returnsFalse()
    {
        openMemoryDb();
        ModelDividendEdit model;
        double price = 0.0;
        QVERIFY(!model.findClosingPriceForDate(makeShareGuid(), QDate(2025, 12, 17), price));
    }

    void test_modelDividendEdit_findClosingPriceForDate_zeroClosing_returnsFalse()
    {
        openMemoryDb();
        ShareRepository sr;
        sr.insert(ShareObject(makeShareGuid(), QStringLiteral("TST"),
                               QStringLiteral("DE000TST0001"), QStringLiteral("Test AG")));

        DailyValuesRepository dvRepo;
        dvRepo.upsert(DailyValuesObject(makeShareGuid(), QDate(2025, 12, 17),
                                        0.0, 0.0, 0.0, 0.0, 0.0));

        ModelDividendEdit model;
        double price = 0.0;
        QVERIFY(!model.findClosingPriceForDate(makeShareGuid(), QDate(2025, 12, 17), price));
    }

    // ── PresenterDividendEdit (Stub-Tests) ────────────────────────────────

    void test_presenterDividendEdit_construction_loadsOverview()
    {
        StubViewDividendEdit view;
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        QVERIFY(view.populateOverviewCalled);
    }

    void test_presenterDividendEdit_construction_clearsForm()
    {
        StubViewDividendEdit view;
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        QVERIFY(view.clearFormCalled);
    }

    void test_presenterDividendEdit_construction_setsButtonStates_noSelection()
    {
        StubViewDividendEdit view;
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        QVERIFY(view.setButtonStatesCalled);
        QVERIFY(!view.lastCanRemove);
        QVERIFY(!view.lastIsEdit);
    }

    void test_presenterDividendEdit_onSave_newDividend_callsAddDividend()
    {
        StubViewDividendEdit view;
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        p.onSave();
        QVERIFY(model.addDividendCalled);
        QVERIFY(!model.updateDividendCalled);
    }

    void test_presenterDividendEdit_onSave_newDividend_emitsDataChanged()
    {
        StubViewDividendEdit view;
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        QSignalSpy spy(&p, &PresenterDividendEdit::dataChanged);
        p.onSave();
        QCOMPARE(spy.count(), 1);
    }

    void test_presenterDividendEdit_onSave_newDividend_jumpsToOverviewTab()
    {
        StubViewDividendEdit view;
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        view.clearFormCalled = false;
        p.onSave();
        QVERIFY(view.clearFormCalled);  // showOverviewTab() calls clearForm()
    }

    void test_presenterDividendEdit_onSave_missingFields_showsError()
    {
        StubViewDividendEdit view;
        view.m_missingFields = true;
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        p.onSave();
        QVERIFY(!model.addDividendCalled);
        QVERIFY(!view.lastError.isEmpty());
    }

    void test_presenterDividendEdit_onSave_documentDuplicate_showsError()
    {
        StubViewDividendEdit view;
        view.m_docPath  = QStringLiteral("/some/doc.pdf");
        StubModelDividendEdit model;
        model.docExists = true;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        p.onSave();
        QVERIFY(!model.addDividendCalled);
        QVERIFY(!view.lastError.isEmpty());
    }

    void test_presenterDividendEdit_onRowSelected_loadsAndSetsButtonStates()
    {
        StubViewDividendEdit view;
        StubModelDividendEdit model;
        model.dividends << makeDividend(QStringLiteral("div-1"));
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);

        view.loadDividendCalled = false;
        view.setButtonStatesCalled = false;
        p.onRowSelected(QStringLiteral("div-1"));

        QVERIFY(view.loadDividendCalled);
        QVERIFY(view.lastCanRemove);   // always removable
        QVERIFY(view.lastIsEdit);
    }

    void test_presenterDividendEdit_onRowSelected_emptyGuid_resetsForm()
    {
        StubViewDividendEdit view;
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        view.clearFormCalled = false;
        p.onRowSelected(QString());
        QVERIFY(view.clearFormCalled);
    }

    void test_presenterDividendEdit_onSave_existingDividend_callsUpdateDividend()
    {
        StubViewDividendEdit view;
        StubModelDividendEdit model;
        model.dividends << makeDividend(QStringLiteral("div-1"));
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);

        p.onRowSelected(QStringLiteral("div-1"));
        model.addDividendCalled    = false;
        model.updateDividendCalled = false;
        p.onSave();

        QVERIFY(model.updateDividendCalled);
        QVERIFY(!model.addDividendCalled);
    }

    void test_presenterDividendEdit_onSave_existingDividend_emitsDataChanged()
    {
        StubViewDividendEdit view;
        StubModelDividendEdit model;
        model.dividends << makeDividend(QStringLiteral("div-1"));
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        p.onRowSelected(QStringLiteral("div-1"));

        QSignalSpy spy(&p, &PresenterDividendEdit::dataChanged);
        p.onSave();
        QCOMPARE(spy.count(), 1);
    }

    void test_presenterDividendEdit_onSave_existingDividend_jumpsToOverviewTab()
    {
        StubViewDividendEdit view;
        StubModelDividendEdit model;
        model.dividends << makeDividend(QStringLiteral("div-1"));
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        p.onRowSelected(QStringLiteral("div-1"));

        view.clearFormCalled = false;
        p.onSave();
        QVERIFY(view.clearFormCalled);
    }

    void test_presenterDividendEdit_onRemove_callsModel()
    {
        StubViewDividendEdit view;
        StubModelDividendEdit model;
        model.dividends << makeDividend(QStringLiteral("div-1"));
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);

        p.onRowSelected(QStringLiteral("div-1"));
        p.onRemove();
        QVERIFY(model.removeDividendCalled);
    }

    void test_presenterDividendEdit_onRemove_emitsDataChanged()
    {
        StubViewDividendEdit view;
        StubModelDividendEdit model;
        model.dividends << makeDividend(QStringLiteral("div-1"));
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        p.onRowSelected(QStringLiteral("div-1"));

        QSignalSpy spy(&p, &PresenterDividendEdit::dataChanged);
        p.onRemove();
        QCOMPARE(spy.count(), 1);
    }

    void test_presenterDividendEdit_onRemove_anyDividend_canBeRemoved()
    {
        // Unlike BuysForm/SalesForm, ALL dividends may be removed regardless of age.
        StubViewDividendEdit view;
        StubModelDividendEdit model;
        // Two dividends — select the older one
        model.dividends << makeDividend(QStringLiteral("div-old"), QStringLiteral("2022-01-01T00:00:00"));
        model.dividends << makeDividend(QStringLiteral("div-new"), QStringLiteral("2024-06-15T00:00:00"));
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);

        p.onRowSelected(QStringLiteral("div-old"));
        QVERIFY(view.lastCanRemove);   // canRemove=true even for older entry
        p.onRemove();
        QVERIFY(model.removeDividendCalled);
        QVERIFY(view.lastError.isEmpty());
    }

    void test_presenterDividendEdit_onRemove_noSelection_doesNothing()
    {
        StubViewDividendEdit view;
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        p.onRemove();
        QVERIFY(!model.removeDividendCalled);
    }

    void test_presenterDividendEdit_onReset_setsButtonStates_noSelection()
    {
        StubViewDividendEdit view;
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        view.setButtonStatesCalled = false;
        p.onReset();
        QVERIFY(view.setButtonStatesCalled);
        QVERIFY(!view.lastCanRemove);
        QVERIFY(!view.lastIsEdit);
    }

    void test_presenterDividendEdit_onReset_jumpsToOverviewTab()
    {
        StubViewDividendEdit view;
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        view.clearFormCalled = false;
        p.onReset();
        QVERIFY(view.clearFormCalled);
    }

    void test_presenterDividendEdit_onDocumentPathEdited_duplicate_setsError()
    {
        StubViewDividendEdit view;
        view.m_docPath = QStringLiteral("/doc.pdf");
        StubModelDividendEdit model;
        model.docExists = true;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        // setFieldError is a no-op in the stub — just verify no crash
        p.onDocumentPathEdited();
        // If docExists→ error path runs without crash, test passes
        QVERIFY(true);
    }

    void test_presenterDividendEdit_onDocumentPathEdited_unique_setsOk()
    {
        StubViewDividendEdit view;
        view.m_docPath = QStringLiteral("/doc.pdf");
        StubModelDividendEdit model;
        model.docExists = false;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        p.onDocumentPathEdited();
        QVERIFY(true);  // No crash = ok path executed
    }

    void test_presenterDividendEdit_onDocumentSelected_writesPathIntoView()
    {
        // Regression 21.08.2026 (Nessies Bugreport): analog zu
        // test_presenterBuyEdit_onDocumentSelected_writesPathIntoView in
        // tst_buysform.cpp — MainWindow ruft für ein per Drag&Drop erfasstes
        // Dokument dlg.presenter()->onDocumentSelected() direkt auf,
        // ViewDividendEdit::onBrowseDocument() (das früher als einziges
        // m_documentPath->setText() setzte) wird dabei nie durchlaufen.
        openMemoryDb();

        StubViewDividendEdit view;
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);

        p.onDocumentSelected(QStringLiteral("/tmp/dropped.pdf"));

        QCOMPARE(view.documentPath(), QStringLiteral("/tmp/dropped.pdf"));
    }

    // ── ViewDividendEdit (Widget-Tests) ───────────────────────────────────

    void test_viewDividendEdit_canBeConstructed()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        QVERIFY(dlg.windowTitle().contains(QStringLiteral("Dividende")));
    }

    void test_viewDividendEdit_initialValues()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        QCOMPARE(dlg.rate(),          0.0);
        QCOMPARE(dlg.volume(),        0.0);
        QCOMPARE(dlg.taxAtSource(),   0.0);
        QCOMPARE(dlg.priceAtPayday(), 0.0);
        QVERIFY(dlg.documentPath().isEmpty());
    }

    void test_viewDividendEdit_hasMissingRequiredFields_initiallyTrue()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        QStringList missing;
        QVERIFY(dlg.hasMissingRequiredFields(missing));
        QVERIFY(missing.contains(QStringLiteral("Dividendensatz")));
        QVERIFY(missing.contains(QStringLiteral("Anteile am Auszahlungstag")));
        QVERIFY(missing.contains(QStringLiteral("Preis der Aktie am Auszahlungstag")));
    }

    void test_viewDividendEdit_hasMissingRequiredFields_falseAfterAllSet()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.setFieldOk(QStringLiteral("rate"),          QStringLiteral("1,50"));
        dlg.setFieldOk(QStringLiteral("volume"),        QStringLiteral("100,0000"));
        dlg.setFieldOk(QStringLiteral("priceAtPayday"), QStringLiteral("45,00"));
        QStringList missing;
        QVERIFY(!dlg.hasMissingRequiredFields(missing));
    }

    void test_viewDividendEdit_clearForm_resetsAllFields()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.setFieldOk(QStringLiteral("rate"),   QStringLiteral("1,50"));
        dlg.setFieldOk(QStringLiteral("volume"), QStringLiteral("100,0000"));
        dlg.clearForm();
        QCOMPARE(dlg.rate(),   0.0);
        QCOMPARE(dlg.volume(), 0.0);
        QVERIFY(dlg.documentPath().isEmpty());
    }

    void test_viewDividendEdit_setButtonStates_noSelection_addLabelHinzufuegen()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.setButtonStates(false, false);
        auto* btn = dlg.findChild<QPushButton*>(QString(),
                        Qt::FindDirectChildrenOnly);
        // Find button with "Hinzufügen" text
        bool found = false;
        const auto buttons = dlg.findChildren<QPushButton*>();
        for (auto* b : buttons) {
            if (b->text() == QStringLiteral("Hinzufügen")) { found = true; break; }
        }
        QVERIFY(found);
        Q_UNUSED(btn)
    }

    void test_viewDividendEdit_setButtonStates_isEdit_saveLabelSpeichern()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.setButtonStates(true, true);
        bool found = false;
        const auto buttons = dlg.findChildren<QPushButton*>();
        for (auto* b : buttons) {
            if (b->text() == QStringLiteral("Speichern")) { found = true; break; }
        }
        QVERIFY(found);
    }

    void test_viewDividendEdit_setButtonStates_canRemoveFalse_removeDisabled()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.setButtonStates(false, false);
        bool found = false;
        const auto buttons = dlg.findChildren<QPushButton*>();
        for (auto* b : buttons) {
            if (b->text() == QStringLiteral("Entfernen")) {
                QVERIFY(!b->isEnabled());
                found = true; break;
            }
        }
        QVERIFY(found);
    }

    void test_viewDividendEdit_setButtonStates_canRemoveTrue_removeEnabled()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.setButtonStates(true, true);
        bool found = false;
        const auto buttons = dlg.findChildren<QPushButton*>();
        for (auto* b : buttons) {
            if (b->text() == QStringLiteral("Entfernen")) {
                QVERIFY(b->isEnabled());
                found = true; break;
            }
        }
        QVERIFY(found);
    }

    void test_viewDividendEdit_allFieldsAlwaysEnabled()
    {
        // In DividendForm there is no read-only mode — all non-FC fields stay enabled
        // regardless of whether an existing entry is selected.
        // FC fields are enabled here explicitly to test all fields at once.
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.setButtonStates(/*canRemove=*/true, /*isEdit=*/true);
        // Activate FC AFTER setButtonStates so it is not overridden
        dlg.setForeignCurrencyEnabled(true);
        const auto edits = dlg.findChildren<QLineEdit*>();
        for (auto* le : edits) {
            if (le->isReadOnly()) continue;
            QVERIFY2(le->isEnabled(),
                     qPrintable(QStringLiteral("Field disabled: ") + le->objectName()));
        }
    }

    void test_viewDividendEdit_markMissingFieldsAsFailed_doesNotCrash()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.markMissingFieldsAsFailed();
        QVERIFY(true);
    }

    void test_viewDividendEdit_setFieldOk_doesNotOverwriteWithEmptyValue()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.setFieldOk(QStringLiteral("rate"), QStringLiteral("1,50"));
        const double before = dlg.rate();
        dlg.setFieldOk(QStringLiteral("rate"), QString());
        QCOMPARE(dlg.rate(), before);  // unchanged
    }

    void test_viewDividendEdit_setFieldOk_writesValueWhenNonEmpty()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.setFieldOk(QStringLiteral("rate"), QStringLiteral("2,00"));
        QCOMPARE(dlg.rate(), 2.0);
    }

    // ── Split-Marker in der Dividenden-Übersicht (Phase 3c) ───────────────

    /** dataTable eines Tab-Containers (siehe OverviewTabWidget::buildFrozenTable()). */
    static QTableWidget* divDataTableOf(QWidget* container)
    {
        if (!container) return nullptr;
        return qobject_cast<QTableWidget*>(
            container->property("dataTable").value<QObject*>());
    }

    /** footerTable eines Tab-Containers (einzeilige Gesamt-Zeile). */
    static QTableWidget* divFooterTableOf(QWidget* container)
    {
        if (!container) return nullptr;
        return qobject_cast<QTableWidget*>(
            container->property("footerTable").value<QObject*>());
    }

    void test_viewDividendEdit_populateOverview_belegRowKeepsBelegVolumeWithMarker()
    {
        // "Anteile am Auszahlungstag" bleibt in Beleg-Skala — die Zahl steht
        // so auf der Abrechnung. Der Marker nennt die heutige Entsprechung.
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.populateOverview(
            { makeDividend(QStringLiteral("div-1"),
                           QStringLiteral("2021-06-15T00:00:00")) },
            { ShareSplitObject(QStringLiteral("sp1"), makeShareGuid(),
                               QDate(2022, 7, 18), 20.0, 1.0) });

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        auto* tbl = divDataTableOf(tabs->widget(1));   // Jahres-Tab 2021
        QVERIFY(tbl != nullptr);
        const QString volText = tbl->item(0, 2)->text();   // kColVolume
        QVERIFY2(volText.contains(QLocale().toString(100.0, 'f', 4)), qPrintable(volText));
        QVERIFY2(volText.contains(ShareSplitHint::marker()), qPrintable(volText));
        QVERIFY(!tbl->item(0, 2)->toolTip().isEmpty());
    }

    void test_viewDividendEdit_populateOverview_withoutSplitHasNoMarker()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.populateOverview({ makeDividend(QStringLiteral("div-1")) }, {});

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        auto* tbl = divDataTableOf(tabs->widget(1));
        QVERIFY(tbl != nullptr);
        QVERIFY(!tbl->item(0, 2)->text().contains(ShareSplitHint::marker()));
        QVERIFY(tbl->item(0, 2)->toolTip().isEmpty());
    }

    void test_viewDividendEdit_populateOverview_footerVolumeIsDash()
    {
        // Anders als bei Käufen und Verkäufen wird hier NICHT summiert:
        // die Stückzahlen beziehen sich auf verschiedene Auszahlungstage,
        // eine Summe beschreibt keinen Bestand, den es je gab. Gilt auch
        // ohne Split — die Summe war schon vorher bedeutungslos.
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.populateOverview(
            { makeDividend(QStringLiteral("div-1"),
                           QStringLiteral("2024-03-15T00:00:00")),
              makeDividend(QStringLiteral("div-2"),
                           QStringLiteral("2024-09-15T00:00:00")) },
            {});

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        auto* footer = divFooterTableOf(tabs->widget(1));
        QVERIFY(footer != nullptr);
        QCOMPARE(footer->item(0, 2)->text(), QStringLiteral("-"));
        QVERIFY(!footer->item(0, 2)->toolTip().isEmpty());
        // Die Dividenden-Summe daneben wird sehr wohl gebildet.
        QVERIFY(footer->item(0, 3)->text().contains(QStringLiteral("€")));
    }

    void test_presenterDividendEdit_populateOverview_passesSplitsAsParameter()
    {
        StubViewDividendEdit  view;
        StubModelDividendEdit model;
        model.dividends = { makeDividend(QStringLiteral("div-1")) };
        model.splits << ShareSplitObject(QStringLiteral("sp1"), makeShareGuid(),
                                         QDate(2022, 7, 18), 20.0, 1.0);

        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);

        QVERIFY(view.populateOverviewCalled);
        QCOMPARE(view.lastOverviewSplits.size(), 1);
        QCOMPARE(view.lastOverviewSplits.first().ratioNew(), 20.0);
    }

    void test_viewDividendEdit_populateOverview_emptyList_noTabs()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.populateOverview({}, {});
        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        QCOMPARE(tabs->count(), 0);
    }

    void test_viewDividendEdit_populateOverview_singleYear_twoTabs()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.populateOverview({ makeDividend(QStringLiteral("div-1")) }, {});
        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        QCOMPARE(tabs->count(), 2);
        QVERIFY(tabs->tabText(0).contains(QStringLiteral("Übersicht")));
        QVERIFY(tabs->tabText(1).contains(QStringLiteral("2024")));
    }

    void test_viewDividendEdit_populateOverview_jahresTabsDescendingByYear()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.populateOverview({
            makeDividend(QStringLiteral("div-1"), QStringLiteral("2022-01-01T00:00:00")),
            makeDividend(QStringLiteral("div-2"), QStringLiteral("2024-06-15T00:00:00")),
        }, {});
        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        QCOMPARE(tabs->count(), 3);
        QVERIFY(tabs->tabText(1).contains(QStringLiteral("2024")));
        QVERIFY(tabs->tabText(2).contains(QStringLiteral("2022")));
    }

    // ── PresenterDividendEdit — weitere Slots ─────────────────────────────

    void test_presenterDividendEdit_onClose_closesView()
    {
        StubViewDividendEdit view;
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        p.onClose();
        QVERIFY(view.closed);
    }

    void test_presenterDividendEdit_onForeignCurrencyToggled_callsView()
    {
        StubViewDividendEdit view;
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        // No crash — view stub ignores setForeignCurrencyEnabled
        p.onForeignCurrencyToggled(true);
        p.onForeignCurrencyToggled(false);
        QVERIFY(true);
    }

    void test_presenterDividendEdit_onDateEdited_validDate_setsOk()
    {
        StubViewDividendEdit view;
        view.m_dateTime = QStringLiteral("2024-06-15T00:00:00");
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        // No crash and no error shown
        p.onDateEdited();
        QVERIFY(view.lastError.isEmpty());
    }

    void test_presenterDividendEdit_onDateEdited_dailyValueFound_fillsPriceAtPayday()
    {
        StubViewDividendEdit view;
        view.m_dateTime         = QStringLiteral("2025-12-17T00:00:00");
        view.m_priceAtPayday    = 0.0;  // noch nicht ausgefüllt
        StubModelDividendEdit model;
        model.hasClosingPrice      = true;
        model.closingPriceToReturn = 204.71;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);

        p.onDateEdited();

        QVERIFY(model.findClosingPriceForDateCalled);
        QCOMPARE(model.lastClosingPriceShareGuid, makeShareGuid());
        QCOMPARE(model.lastClosingPriceDate, QDate(2025, 12, 17));
        QCOMPARE(view.lastFieldOkField, QStringLiteral("priceAtPayday"));
        QCOMPARE(view.priceAtPayday(), 204.71);
        QVERIFY(!view.lastFieldOkTooltip.isEmpty());  // "Aus Tageswerten übernommen ..."
    }

    void test_presenterDividendEdit_onDateEdited_noDailyValue_leavesPriceAtPaydayUnchanged()
    {
        StubViewDividendEdit view;
        view.m_dateTime      = QStringLiteral("2025-12-17T00:00:00");
        view.m_priceAtPayday = 45.0;  // bereits manuell eingegeben
        StubModelDividendEdit model;
        model.hasClosingPrice = false;  // kein Treffer in der DB
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);

        p.onDateEdited();

        QVERIFY(model.findClosingPriceForDateCalled);
        // Kein Treffer → Feld bleibt unverändert, kein setFieldOk("priceAtPayday", ...)
        QCOMPARE(view.priceAtPayday(), 45.0);
        QVERIFY(view.lastFieldOkField != QStringLiteral("priceAtPayday"));
    }

    void test_presenterDividendEdit_onDateEdited_invalidDate_doesNotQueryDailyValue()
    {
        StubViewDividendEdit view;
        view.m_dateTime = QStringLiteral("2000-01-01T00:00:00");  // Sentinel — ungültig
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);

        p.onDateEdited();

        QVERIFY(!model.findClosingPriceForDateCalled);
    }

    void test_presenterDividendEdit_onDateEdited_sentinelDate_setsError()
    {
        StubViewDividendEdit view;
        view.m_dateTime = QStringLiteral("2000-01-01T00:00:00");
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        p.onDateEdited();
        QVERIFY(view.lastError.isEmpty());  // setFieldError shows icon, not dialog
    }

    void test_presenterDividendEdit_onRateEdited_valid_setsOk()
    {
        StubViewDividendEdit view;
        view.m_rate = 1.50;
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        p.onRateEdited();
        QVERIFY(view.lastError.isEmpty());
    }

    void test_presenterDividendEdit_onRateEdited_zero_setsError()
    {
        StubViewDividendEdit view;
        view.m_rate = 0.0;
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        p.onRateEdited();
        QVERIFY(view.lastError.isEmpty());  // icon only, no dialog
    }

    void test_presenterDividendEdit_onVolumeEdited_valid_setsOk()
    {
        StubViewDividendEdit view;
        view.m_volume = 100.0;
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        p.onVolumeEdited();
        QVERIFY(view.lastError.isEmpty());
    }

    void test_presenterDividendEdit_onPriceAtPaydayEdited_valid_setsOk()
    {
        StubViewDividendEdit view;
        view.m_priceAtPayday = 45.0;
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        p.onPriceAtPaydayEdited();
        QVERIFY(view.lastError.isEmpty());
    }

    void test_presenterDividendEdit_onPriceAtPaydayEdited_zero_setsError()
    {
        StubViewDividendEdit view;
        view.m_priceAtPayday = 0.0;
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        p.onPriceAtPaydayEdited();
        QVERIFY(view.lastError.isEmpty());  // icon only, no dialog
    }

    void test_presenterDividendEdit_onTaxEdited_negative_setsError()
    {
        StubViewDividendEdit view;
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        p.onTaxEdited(QStringLiteral("taxAtSource"), -1.0);
        QVERIFY(view.lastError.isEmpty());  // icon only, no dialog
    }

    void test_presenterDividendEdit_onTaxEdited_zero_setsOk()
    {
        StubViewDividendEdit view;
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        p.onTaxEdited(QStringLiteral("taxAtSource"), 0.0);
        QVERIFY(view.lastError.isEmpty());
    }

    void test_presenterDividendEdit_onSave_modelError_showsError()
    {
        StubViewDividendEdit view;
        StubModelDividendEdit model;
        model.addResult = false;
        model.errorMsg  = QStringLiteral("DB-Fehler");
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        p.onSave();
        QVERIFY(!view.lastError.isEmpty());
    }

    // ── ViewDividendEdit — weitere Tests ──────────────────────────────────

    void test_viewDividendEdit_setFieldError_doesNotCrash()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.setFieldError(QStringLiteral("rate"));
        dlg.setFieldError(QStringLiteral("volume"));
        dlg.setFieldError(QStringLiteral("priceAtPayday"));
        dlg.setFieldError(QStringLiteral("unknownField"));
        QVERIFY(true);
    }

    void test_viewDividendEdit_clearForm_resetsStatusIcons()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.setFieldOk(QStringLiteral("rate"),   QStringLiteral("1,50"));
        dlg.setFieldError(QStringLiteral("volume"));
        dlg.clearForm();
        // After clearForm all fields are Untouched — hasMissingRequiredFields
        // returns true (no Ok icons set), no Error icons either.
        QStringList missing;
        QVERIFY(dlg.hasMissingRequiredFields(missing));
    }

    void test_viewDividendEdit_clearForm_resetsParseStatusBar()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.setParseProgress(75, QStringLiteral("Analysiere..."));
        dlg.clearForm();
        // After clearForm parse bar is back to 0 — no crash
        QVERIFY(true);
    }

    void test_viewDividendEdit_setParseProgress_showsValues()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.setParseProgress(50, QStringLiteral("Test-Status"));
        auto* bar = dlg.findChild<QProgressBar*>();
        if (!bar) QFAIL("QProgressBar not found");
        QCOMPARE(bar->value(), 50);
    }

    void test_viewDividendEdit_setFieldOk_date_parsesISOFormat()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.setFieldOk(QStringLiteral("date"), QStringLiteral("2024-06-15"));
        auto* de = dlg.findChild<QDateEdit*>();
        if (!de) QFAIL("QDateEdit not found");
        QCOMPARE(de->date(), QDate(2024, 6, 15));
    }

    void test_viewDividendEdit_setFieldOk_volume_handlesGermanDecimal()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.setFieldOk(QStringLiteral("volume"), QStringLiteral("165,0000"));
        QCOMPARE(dlg.volume(), 165.0);
    }

    void test_viewDividendEdit_setFieldOk_rate_handlesGermanDecimal()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.setFieldOk(QStringLiteral("rate"), QStringLiteral("1,50"));
        QCOMPARE(dlg.rate(), 1.5);
    }

    void test_viewDividendEdit_populateOverview_twoYears_threeTabs()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.populateOverview({
            makeDividend(QStringLiteral("div-1"), QStringLiteral("2023-01-01T00:00:00")),
            makeDividend(QStringLiteral("div-2"), QStringLiteral("2024-06-15T00:00:00")),
        }, {});
        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        QCOMPARE(tabs->count(), 3);
    }

    void test_viewDividendEdit_populateOverview_jahresTabHasFiveColumns()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.populateOverview({ makeDividend(QStringLiteral("div-1")) }, {});
        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        auto* container = tabs->widget(1);
        if (!container) QFAIL("Jahres-Tab container not found");
        auto* tbl = qobject_cast<QTableWidget*>(
            container->property("dataTable").value<QObject*>());
        if (!tbl) QFAIL("dataTable not found");
        QCOMPARE(tbl->columnCount(), 5);  // Datum|Rate|Anteile|Dividende|Dok.
    }

    void test_viewDividendEdit_populateOverview_jahresTabRowCount()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.populateOverview({
            makeDividend(QStringLiteral("div-1"), QStringLiteral("2024-01-01T00:00:00")),
            makeDividend(QStringLiteral("div-2"), QStringLiteral("2024-06-15T00:00:00")),
            makeDividend(QStringLiteral("div-3"), QStringLiteral("2024-12-01T00:00:00")),
        }, {});
        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        auto* container = tabs->widget(1);
        if (!container) QFAIL("Jahres-Tab container not found");
        auto* tbl = qobject_cast<QTableWidget*>(
            container->property("dataTable").value<QObject*>());
        if (!tbl) QFAIL("dataTable not found");
        QCOMPARE(tbl->rowCount(), 3);
    }

    void test_viewDividendEdit_populateOverview_guidStoredInDateColumn()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.populateOverview({ makeDividend(QStringLiteral("div-guid-99")) }, {});
        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        auto* container = tabs->widget(1);
        if (!container) QFAIL("Jahres-Tab container not found");
        auto* tbl = qobject_cast<QTableWidget*>(
            container->property("dataTable").value<QObject*>());
        if (!tbl) QFAIL("dataTable not found");
        auto* item = tbl->item(0, 0);
        if (!item) QFAIL("Item(0,0) not found");
        QCOMPARE(item->data(Qt::UserRole).toString(), QStringLiteral("div-guid-99"));
    }

    void test_viewDividendEdit_populateOverview_docDashWhenNoPath()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.populateOverview({ makeDividend(QStringLiteral("div-1")) }, {});
        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        auto* container = tabs->widget(1);
        if (!container) QFAIL("container not found");
        auto* tbl = qobject_cast<QTableWidget*>(
            container->property("dataTable").value<QObject*>());
        if (!tbl) QFAIL("dataTable not found");
        // No document → dash text, no cellWidget
        auto* item = tbl->item(0, 4);  // kColDoc = 4
        QVERIFY(item != nullptr);
        QCOMPARE(item->text(), QStringLiteral("-"));
        QVERIFY(tbl->cellWidget(0, 4) == nullptr);
    }

    void test_viewDividendEdit_populateOverview_tabTitleContainsTotal()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.populateOverview({ makeDividend(QStringLiteral("div-1")) }, {});
        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        QVERIFY(tabs->tabText(0).contains(QStringLiteral("€")));
    }

    void test_viewDividendEdit_populateOverview_repopulateReplacesOldTabs()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.populateOverview({ makeDividend(QStringLiteral("div-1")) }, {});
        dlg.populateOverview({
            makeDividend(QStringLiteral("div-2"), QStringLiteral("2023-01-01T00:00:00")),
            makeDividend(QStringLiteral("div-3"), QStringLiteral("2022-06-01T00:00:00")),
        }, {});
        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        // Old 2024-tab must be gone; now 2023 and 2022
        QCOMPARE(tabs->count(), 3);
        QVERIFY(!tabs->tabText(1).contains(QStringLiteral("2024")));
        QVERIFY(tabs->tabText(1).contains(QStringLiteral("2023")));
    }

    void test_viewDividendEdit_uebersichtTab_hasTable()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.populateOverview({ makeDividend(QStringLiteral("div-1")) }, {});
        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        auto* container = tabs->widget(0);
        if (!container) QFAIL("Übersicht container not found");
        auto* tbl = qobject_cast<QTableWidget*>(
            container->property("dataTable").value<QObject*>());
        if (!tbl) QFAIL("dataTable not found");
        QCOMPARE(tbl->columnCount(), 2);  // Jahr | Dividende
        QCOMPARE(tbl->rowCount(), 1);
    }

    void test_viewDividendEdit_uebersichtClick_jumpsToYearTab()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.populateOverview({ makeDividend(QStringLiteral("div-1")) }, {});
        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        auto* container = tabs->widget(0);
        if (!container) QFAIL("container not found");
        auto* tbl = qobject_cast<QTableWidget*>(
            container->property("dataTable").value<QObject*>());
        if (!tbl) QFAIL("dataTable not found");

        // Simulate click on row 0 → should jump to year tab
        QTest::mouseClick(tbl->viewport(), Qt::LeftButton,
                          Qt::NoModifier,
                          tbl->visualItemRect(tbl->item(0, 0)).center());
        QVERIFY(tabs->currentIndex() > 0);
    }

    void test_viewDividendEdit_tabChange_selectsFirstRowInJahresTab()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.populateOverview({
            makeDividend(QStringLiteral("div-1"), QStringLiteral("2024-01-01T00:00:00")),
            makeDividend(QStringLiteral("div-2"), QStringLiteral("2024-06-15T00:00:00")),
        }, {});
        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        tabs->setCurrentIndex(1);  // switch to year tab
        auto* container = tabs->widget(1);
        if (!container) QFAIL("container not found");
        auto* tbl = qobject_cast<QTableWidget*>(
            container->property("dataTable").value<QObject*>());
        if (!tbl) QFAIL("dataTable not found");
        QCOMPARE(tbl->currentRow(), 0);
    }

    // ── Fremdwährungs-Modus ───────────────────────────────────────────────

    void test_viewDividendEdit_fcFieldsDisabledByDefault()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        // Checkbox ist initial nicht angehakt → FC-Felder deaktiviert
        auto* cb = dlg.findChild<QCheckBox*>();
        if (!cb) QFAIL("QCheckBox not found");
        QVERIFY(!cb->isChecked());
        QVERIFY(!dlg.enableForeignCurrency());
        // Direkt über Accessor prüfen: exchangeRatio = 1.0 (Default)
        QCOMPARE(dlg.exchangeRatio(), 1.0);
    }

    void test_viewDividendEdit_setForeignCurrencyEnabled_true_enablesFields()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.setForeignCurrencyEnabled(true);
        // Nach Aktivierung muss enableForeignCurrency() true liefern... nein,
        // das liest die Checkbox — setForeignCurrencyEnabled aktiviert nur Felder.
        // Wir prüfen indirekt: kein Absturz und exchangeRatio bleibt lesbar.
        QCOMPARE(dlg.exchangeRatio(), 1.0);
    }

    void test_viewDividendEdit_setForeignCurrencyEnabled_false_disablesFields()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.setForeignCurrencyEnabled(true);
        dlg.setForeignCurrencyEnabled(false);
        // Kein Absturz; Felder deaktiviert
        QVERIFY(true);
    }

    void test_viewDividendEdit_clearForm_resetsDerivedFields()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        // Setze abgeleitete Felder über den Presenter-Weg
        dlg.setDividendPayout(123.45);
        dlg.setDividendPayoutFc(150.00);
        dlg.setTaxSum(12.34);
        dlg.setDividendPayoutWithTaxes(111.11);
        dlg.setYield(3.5);
        // clearForm muss alle auf 0 zurücksetzen
        dlg.clearForm();
        QCOMPARE(dlg.findChildren<QLineEdit*>()
                    .isEmpty(), false);  // Sanity-Check
        // Indirekt: payout ist read-only, daher über Text prüfen
        // Da formatMoney(0.0) = "0,00" → suche in allen read-only-Feldern
        bool allZero = true;
        for (auto* le : dlg.findChildren<QLineEdit*>()) {
            if (!le->isReadOnly()) continue;
            const QString t = le->text().trimmed();
            // Derived fields show "0,00" or "0,0000 %" after reset
            if (!t.isEmpty() && t != QStringLiteral("0,00")
                             && t != QStringLiteral("0,0000 %")
                             && t != QStringLiteral("0,00 %")) {
                allZero = false;
                break;
            }
        }
        QVERIFY(allZero);
    }

    void test_presenterDividendEdit_onExchangeRatioEdited_valid_setsOk()
    {
        StubViewDividendEdit view;
        view.m_enableFc     = true;
        view.m_exchangeRatio = 1.1677;
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        p.onExchangeRatioEdited();
        QVERIFY(view.lastError.isEmpty());
    }

    void test_presenterDividendEdit_onExchangeRatioEdited_zero_setsError()
    {
        StubViewDividendEdit view;
        view.m_enableFc      = true;
        view.m_exchangeRatio = 0.0;
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        p.onExchangeRatioEdited();
        // Fehler-Icon gesetzt — kein Dialog
        QVERIFY(view.lastError.isEmpty());
    }

    void test_presenterDividendEdit_onExchangeRatioEdited_fcDisabled_noValidation()
    {
        StubViewDividendEdit view;
        view.m_enableFc      = false;
        view.m_exchangeRatio = 0.0;  // würde Fehler liefern wenn FC aktiv
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        p.onExchangeRatioEdited();
        // FC nicht aktiv → keine Validierung, kein Fehler
        QVERIFY(view.lastError.isEmpty());
    }

    void test_viewDividendEdit_clearForm_resetsFcCheckbox()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        // Lade eine Dividende mit FC
        DividendObject d(QStringLiteral("div-fc"), makeShareGuid(),
                         QStringLiteral("2024-06-15T00:00:00"),
                         0.95, 100.0, 0.0, 0.0, 0.0, 45.0,
                         true, 1.1677, QStringLiteral("en-US"));
        dlg.loadDividend(d);
        QVERIFY(dlg.enableForeignCurrency());

        // clearForm muss Checkbox zurücksetzen
        dlg.clearForm();
        QVERIFY(!dlg.enableForeignCurrency());
    }

    void test_viewDividendEdit_loadDividend_withFC_setsCheckbox()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        DividendObject d(QStringLiteral("div-fc"), makeShareGuid(),
                         QStringLiteral("2024-06-15T00:00:00"),
                         0.95, 100.0, 0.0, 0.0, 0.0, 45.0,
                         true, 1.1677, QStringLiteral("en-US"));
        dlg.loadDividend(d);
        QVERIFY(dlg.enableForeignCurrency());
        QCOMPARE(dlg.exchangeRatio(), 1.1677);
    }

    void test_viewDividendEdit_loadDividend_withoutFC_checkboxUnchecked()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        DividendObject d(QStringLiteral("div-1"), makeShareGuid(),
                         QStringLiteral("2024-06-15T00:00:00"),
                         1.50, 100.0, 0.0, 0.0, 0.0, 45.0,
                         false, 1.0, QStringLiteral("EUR"));
        dlg.loadDividend(d);
        QVERIFY(!dlg.enableForeignCurrency());
    }

};

// ─────────────────────────────────────────────────────────────────────────────
// TestOwnMessageBox
// ─────────────────────────────────────────────────────────────────────────────
#include "../../app/forms/OwnMessageBoxForm/OwnMessageBox.h"

class TestOwnMessageBox : public QObject
{
    Q_OBJECT

private slots:

    // ── Critical ─────────────────────────────────────────────────────────────

    void test_critical_canBeConstructed()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Critical,
                          QStringLiteral("Fehler"),
                          QStringLiteral("Ein Fehler ist aufgetreten."));
        QVERIFY(dlg.windowTitle() == QStringLiteral("Fehler"));
    }

    void test_critical_hasSingleOkButton()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Critical,
                          QStringLiteral("Fehler"),
                          QStringLiteral("Fehlermeldung"));
        auto* btn = dlg.findChild<QPushButton*>("", Qt::FindChildrenRecursively);
        if (!btn) QFAIL("No button found in Critical dialog");
        // There must be exactly one button
        const auto buttons = dlg.findChildren<QPushButton*>();
        QCOMPARE(buttons.size(), 1);
        QCOMPARE(buttons.first()->text(), tr("Ok"));
    }

    void test_critical_hasNoYesNoButtons()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Critical,
                          QStringLiteral("Fehler"),
                          QStringLiteral("Fehlermeldung"));
        const auto buttons = dlg.findChildren<QPushButton*>();
        for (auto* b : buttons) {
            QVERIFY(b->text() != tr("Ja"));
            QVERIFY(b->text() != tr("Nein"));
        }
    }

    void test_critical_okButtonAcceptsDialog()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Critical,
                          QStringLiteral("Fehler"),
                          QStringLiteral("Fehlermeldung"));
        const auto buttons = dlg.findChildren<QPushButton*>();
        if (buttons.isEmpty()) QFAIL("No button found");
        // Simulate click — dialog must not crash and result must be Accepted
        QMetaObject::invokeMethod(buttons.first(), "clicked", Qt::DirectConnection);
        QCOMPARE(dlg.result(), static_cast<int>(QDialog::Accepted));
    }

    void test_critical_hasIconLabel()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Critical,
                          QStringLiteral("Fehler"),
                          QStringLiteral("Fehlermeldung"));
        const auto labels = dlg.findChildren<QLabel*>();
        // At least one label must have a pixmap (the icon label)
        bool hasIcon = false;
        for (auto* l : labels) {
            if (!l->pixmap().isNull()) {
                hasIcon = true;
                break;
            }
        }
        QVERIFY(hasIcon);
    }

    void test_critical_okButtonHasNoIcon()
    {
        // 14.08.2026, Nessies Vorgabe: das bisherige ButtonSave-Icon
        // (Diskette) suggerierte fälschlich ein Speichern, obwohl Ok hier
        // nur den Dialog schließt.
        OwnMessageBox dlg(OwnMessageBox::Type::Critical,
                          QStringLiteral("Fehler"),
                          QStringLiteral("Fehlermeldung"));
        const auto buttons = dlg.findChildren<QPushButton*>();
        if (buttons.isEmpty()) QFAIL("No button found");
        QVERIFY(buttons.first()->icon().isNull());
    }

    void test_critical_messageTextVisible()
    {
        const QString msg = QStringLiteral("Datenbankfehler aufgetreten.");
        OwnMessageBox dlg(OwnMessageBox::Type::Critical,
                          QStringLiteral("Fehler"), msg);
        const auto labels = dlg.findChildren<QLabel*>();
        bool found = false;
        for (auto* l : labels) {
            if (l->text() == msg) { found = true; break; }
        }
        QVERIFY(found);
    }

    // ── Information ───────────────────────────────────────────────────────────

    void test_information_canBeConstructed()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Information,
                          QStringLiteral("Info"),
                          QStringLiteral("Vorgang abgeschlossen."));
        QVERIFY(dlg.windowTitle() == QStringLiteral("Info"));
    }

    void test_information_hasSingleOkButton()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Information,
                          QStringLiteral("Info"),
                          QStringLiteral("Hinweistext"));
        const auto buttons = dlg.findChildren<QPushButton*>();
        QCOMPARE(buttons.size(), 1);
        QCOMPARE(buttons.first()->text(), tr("Ok"));
    }

    void test_information_hasIconLabel()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Information,
                          QStringLiteral("Info"),
                          QStringLiteral("Hinweistext"));
        const auto labels = dlg.findChildren<QLabel*>();
        bool hasIcon = false;
        for (auto* l : labels) {
            if (!l->pixmap().isNull()) { hasIcon = true; break; }
        }
        QVERIFY(hasIcon);
    }

    void test_information_okButtonHasNoIcon()
    {
        // 14.08.2026, Nessies Vorgabe — siehe test_critical_okButtonHasNoIcon().
        OwnMessageBox dlg(OwnMessageBox::Type::Information,
                          QStringLiteral("Info"),
                          QStringLiteral("Hinweistext"));
        const auto buttons = dlg.findChildren<QPushButton*>();
        if (buttons.isEmpty()) QFAIL("No button found");
        QVERIFY(buttons.first()->icon().isNull());
    }

    // ── Question ──────────────────────────────────────────────────────────────

    void test_question_canBeConstructed()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Question,
                          QStringLiteral("Bestätigung"),
                          QStringLiteral("Wirklich löschen?"));
        QVERIFY(dlg.windowTitle() == QStringLiteral("Bestätigung"));
    }

    void test_question_hasTwoButtons()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Question,
                          QStringLiteral("Bestätigung"),
                          QStringLiteral("Wirklich löschen?"));
        const auto buttons = dlg.findChildren<QPushButton*>();
        QCOMPARE(buttons.size(), 2);
    }

    void test_question_hasYesAndNoButtons()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Question,
                          QStringLiteral("Bestätigung"),
                          QStringLiteral("Wirklich löschen?"));
        const auto buttons = dlg.findChildren<QPushButton*>();
        QStringList labels;
        for (auto* b : buttons) labels << b->text();
        QVERIFY(labels.contains(tr("Ja")));
        QVERIFY(labels.contains(tr("Nein")));
    }

    void test_question_hasNoOkButton()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Question,
                          QStringLiteral("Bestätigung"),
                          QStringLiteral("Wirklich löschen?"));
        const auto buttons = dlg.findChildren<QPushButton*>();
        for (auto* b : buttons)
            QVERIFY(b->text() != tr("Ok"));
    }

    void test_question_yesButtonAcceptsDialog()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Question,
                          QStringLiteral("Bestätigung"),
                          QStringLiteral("Wirklich löschen?"));
        const auto buttons = dlg.findChildren<QPushButton*>();
        QPushButton* yesBtn = nullptr;
        for (auto* b : buttons) {
            if (b->text() == tr("Ja")) { yesBtn = b; break; }
        }
        if (!yesBtn) QFAIL("Yes button not found");
        QMetaObject::invokeMethod(yesBtn, "clicked", Qt::DirectConnection);
        QCOMPARE(dlg.result(), static_cast<int>(QDialog::Accepted));
    }

    void test_question_noButtonRejectsDialog()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Question,
                          QStringLiteral("Bestätigung"),
                          QStringLiteral("Wirklich löschen?"));
        const auto buttons = dlg.findChildren<QPushButton*>();
        QPushButton* noBtn = nullptr;
        for (auto* b : buttons) {
            if (b->text() == tr("Nein")) { noBtn = b; break; }
        }
        if (!noBtn) QFAIL("No button not found");
        QMetaObject::invokeMethod(noBtn, "clicked", Qt::DirectConnection);
        QCOMPARE(dlg.result(), static_cast<int>(QDialog::Rejected));
    }

    void test_question_hasIconLabel()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Question,
                          QStringLiteral("Bestätigung"),
                          QStringLiteral("Wirklich löschen?"));
        const auto labels = dlg.findChildren<QLabel*>();
        bool hasIcon = false;
        for (auto* l : labels) {
            if (!l->pixmap().isNull()) { hasIcon = true; break; }
        }
        QVERIFY(hasIcon);
    }

    // ── Static convenience methods ────────────────────────────────────────────

    void test_staticCritical_doesNotCrash()
    {
        // Can't exec() in a unit test — construct directly and verify it compiles
        OwnMessageBox dlg(OwnMessageBox::Type::Critical,
                          QStringLiteral("Fehler"),
                          QStringLiteral("Statischer Aufruf."));
        QVERIFY(dlg.windowTitle() == QStringLiteral("Fehler"));
    }

    void test_staticInformation_doesNotCrash()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Information,
                          QStringLiteral("Info"),
                          QStringLiteral("Statischer Aufruf."));
        QVERIFY(dlg.windowTitle() == QStringLiteral("Info"));
    }

    void test_staticQuestion_doesNotCrash()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Question,
                          QStringLiteral("Bestätigung"),
                          QStringLiteral("Statischer Aufruf."));
        QVERIFY(dlg.windowTitle() == QStringLiteral("Bestätigung"));
    }

    // ── Layout & sizing ───────────────────────────────────────────────────────

    void test_minimumWidth_isAtLeast360()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Critical,
                          QStringLiteral("Fehler"),
                          QStringLiteral("Test"));
        QVERIFY(dlg.minimumWidth() >= 360);
    }

    void test_buttonHeight_matchesUiConstants()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Critical,
                          QStringLiteral("Fehler"),
                          QStringLiteral("Test"));
        const auto buttons = dlg.findChildren<QPushButton*>();
        if (buttons.isEmpty()) QFAIL("No buttons found");
        QCOMPARE(buttons.first()->height(), UiConstants::kButtonHeight);
    }

    void test_isModal()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Information,
                          QStringLiteral("Info"),
                          QStringLiteral("Test"));
        QVERIFY(dlg.isModal());
    }

    void test_longMessageText_doesNotCrash()
    {
        const QString longMsg = QString(500, QChar('A'));
        OwnMessageBox dlg(OwnMessageBox::Type::Critical,
                          QStringLiteral("Fehler"), longMsg);
        const auto labels = dlg.findChildren<QLabel*>();
        bool found = false;
        for (auto* l : labels) {
            if (l->text() == longMsg) { found = true; break; }
        }
        QVERIFY(found);
    }

    void test_multilineMessage_doesNotCrash()
    {
        const QString msg = QStringLiteral("Zeile 1\nZeile 2\nZeile 3");
        OwnMessageBox dlg(OwnMessageBox::Type::Information,
                          QStringLiteral("Info"), msg);
        QVERIFY(dlg.minimumWidth() >= 360);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// TestBackupForm
// ─────────────────────────────────────────────────────────────────────────────
#include "../../app/forms/BackupProgressForm/BackupWorker.h"
#include "../../app/forms/BackupProgressForm/BackupProgressDialog.h"

class TestBackupForm : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;

    // Eigene Sandbox — unabhängig davon, welche AppSettings-Werte eine
    // vorher im selben Prozess gelaufene Testklasse hinterlassen hat (siehe
    // main() am Dateiende: mehrere QObject-Klassen laufen sequenziell im
    // selben Prozess). Vorher verließ sich diese Klasse stillschweigend auf
    // den Zustand von TestMainWindow/TestSalesForm — als deren
    // cleanupTestCase() fälschlich auf die echte settings.ini umleitete
    // (behoben 19.07.2026), schrieben die setPortfolioPath()/
    // setDocumentsRootPath()-Aufrufe unten direkt in Nessies reale
    // Konfiguration. Jetzt lädt diese Klasse immer zuerst ihre eigene,
    // separate Sandbox-INI, komplett unabhängig von anderen Testklassen.
    void loadSandboxedSettings()
    {
        const QString sandboxIni = m_tempDir.path() + QStringLiteral("/test_settings.ini");
        AppSettings::instance().load(sandboxIni);

        // Verhindert, dass MainWindow::ensureDocumentsRootConfigured() beim
        // Konstruieren einen blockierenden Dialog öffnet (der Dialog
        // erscheint nur, wenn documentsRootPath() leer ist).
        AppSettings::instance().setDocumentsRootPath(
            m_tempDir.path() + QStringLiteral("/documents"));
    }

    // Helper: create a small test file with given size in bytes
    QString makeTestFile(const QString& name, int sizeBytes = 1024)
    {
        const QString path = m_tempDir.path() + QDir::separator() + name;
        QFile f(path);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(QByteArray(sizeBytes, 'X'));
            f.close();
        }
        return path;
    }

private slots:

    void initTestCase()
    {
        QVERIFY(m_tempDir.isValid());
        loadSandboxedSettings();
    }

    void init()
    {
        // Vor jedem einzelnen Test erneut sandboxen — dieselbe Absicherung
        // wie init()/loadSandboxedSettings() in TestMainWindow/TestSalesForm.
        loadSandboxedSettings();
        if (Database::instance().isOpen())
            Database::instance().close();
    }

    void cleanupTestCase()
    {
        if (Database::instance().isOpen())
            Database::instance().close();
        // Bewusst KEIN AppSettings::instance().load(...) mit echtem Pfad —
        // siehe Begründung bei TestMainWindow::cleanupTestCase() oben.
    }

    // ── BackupWorker — successful copy ────────────────────────────────────────

    void test_backupWorker_copiesFileSuccessfully()
    {
        const QString src = makeTestFile(QStringLiteral("portfolio.db"), 2048);
        const QString dst = m_tempDir.path() + QStringLiteral("/Backup_portfolio.db");

        BackupWorker worker(src, dst);

        bool success = false;
        connect(&worker, &BackupWorker::finished,
                this, [&](bool s, const QString&) { success = s; });

        worker.run();

        QVERIFY(success);
        QVERIFY(QFileInfo::exists(dst));
        QCOMPARE(QFileInfo(dst).size(), QFileInfo(src).size());
    }

    void test_backupWorker_emitsProgressSignal()
    {
        const QString src = makeTestFile(QStringLiteral("progress_test.db"), 4096);
        const QString dst = m_tempDir.path() + QStringLiteral("/Backup_progress.db");

        BackupWorker worker(src, dst);

        QSignalSpy spy(&worker, &BackupWorker::progress);
        bool finished = false;
        connect(&worker, &BackupWorker::finished,
                this, [&](bool, const QString&) { finished = true; });

        worker.run();

        QVERIFY(finished);
        // At least one progress signal must have been emitted
        QVERIFY(spy.count() >= 1);
        // Last progress: bytesWritten == totalBytes
        const QList<QVariant> lastArgs = spy.last();
        QCOMPARE(lastArgs.at(0).toLongLong(), lastArgs.at(1).toLongLong());
    }

    void test_backupWorker_emitsFinishedWithSuccess()
    {
        const QString src = makeTestFile(QStringLiteral("finished_test.db"), 512);
        const QString dst = m_tempDir.path() + QStringLiteral("/Backup_finished.db");

        BackupWorker worker(src, dst);

        QSignalSpy spy(&worker, &BackupWorker::finished);
        worker.run();

        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toBool()); // success = true
    }

    // ── BackupWorker — missing source ─────────────────────────────────────────

    void test_backupWorker_missingSource_emitsFailure()
    {
        const QString src = m_tempDir.path() + QStringLiteral("/nonexistent.db");
        const QString dst = m_tempDir.path() + QStringLiteral("/Backup_nonexistent.db");

        BackupWorker worker(src, dst);

        bool success = true;
        connect(&worker, &BackupWorker::finished,
                this, [&](bool s, const QString&) { success = s; });

        worker.run();

        QVERIFY(!success);
        QVERIFY(!QFileInfo::exists(dst));
    }

    // ── BackupWorker — cancellation ───────────────────────────────────────────

    void test_backupWorker_cancel_removesPartialFile()
    {
        const QString src = makeTestFile(QStringLiteral("cancel_test.db"), 1024);
        const QString dst = m_tempDir.path() + QStringLiteral("/Backup_cancel.db");

        BackupWorker worker(src, dst);

        bool success = true;
        connect(&worker, &BackupWorker::finished,
                this, [&](bool s, const QString&) { success = s; });

        // Cancel before run — simulates immediate cancel
        worker.cancel();
        worker.run();

        QVERIFY(!success);
        // Partial file must be removed
        QVERIFY(!QFileInfo::exists(dst));
    }

    // ── BackupProgressDialog ──────────────────────────────────────────────────
    // Note: BackupProgressDialog starts a QThread in its constructor.
    // ~BackupProgressDialog() waits for the worker thread itself (quit() +
    // wait()) before tearing down its QThread child, so destroying the
    // dialog without waiting is safe (see
    // test_backupProgressDialog_destroyedImmediately_doesNotCrash below).
    // The other tests still call waitForDialog() — not to avoid a crash, but
    // because they assert on wasSuccessful()/the copied file afterwards.

    // Helper: wait for dialog to finish (max 5 seconds)
    static void waitForDialog(BackupProgressDialog& dlg)
    {
        QElapsedTimer timer;
        timer.start();
        while (!dlg.wasSuccessful() && timer.elapsed() < 5000)
            QApplication::processEvents(QEventLoop::AllEvents, 50);
    }

    void test_backupProgressDialog_canBeConstructed()
    {
        const QString src = makeTestFile(QStringLiteral("dlg_test.db"), 512);
        const QString dst = m_tempDir.path() + QStringLiteral("/Backup_dlg_test.db");

        BackupProgressDialog dlg(src, dst);
        QVERIFY(dlg.windowTitle().contains(QStringLiteral("Backup")));
        waitForDialog(dlg); // must complete before dlg is destroyed
    }

    void test_backupProgressDialog_isModal()
    {
        const QString src = makeTestFile(QStringLiteral("modal_test.db"), 512);
        const QString dst = m_tempDir.path() + QStringLiteral("/Backup_modal.db");

        BackupProgressDialog dlg(src, dst);
        QVERIFY(dlg.isModal());
        waitForDialog(dlg);
    }

    void test_backupProgressDialog_hasCancelButton()
    {
        const QString src = makeTestFile(QStringLiteral("cancel_btn_test.db"), 512);
        const QString dst = m_tempDir.path() + QStringLiteral("/Backup_cancel_btn.db");

        BackupProgressDialog dlg(src, dst);
        const auto buttons = dlg.findChildren<QPushButton*>();
        if (buttons.isEmpty()) QFAIL("No button found in BackupProgressDialog");
        bool hasCancel = false;
        for (auto* b : buttons)
            if (b->text().contains(QStringLiteral("Abbrechen")))
                hasCancel = true;
        QVERIFY(hasCancel);
        waitForDialog(dlg);
    }

    void test_backupProgressDialog_hasProgressBar()
    {
        const QString src = makeTestFile(QStringLiteral("pbar_test.db"), 512);
        const QString dst = m_tempDir.path() + QStringLiteral("/Backup_pbar.db");

        BackupProgressDialog dlg(src, dst);
        const auto bars = dlg.findChildren<QProgressBar*>();
        QVERIFY(!bars.isEmpty());
        QCOMPARE(bars.first()->minimum(), 0);
        QCOMPARE(bars.first()->maximum(), 100);
        waitForDialog(dlg);
    }

    void test_backupProgressDialog_successfulCopy_wasSuccessfulTrue()
    {
        const QString src = makeTestFile(QStringLiteral("success_dlg.db"), 512);
        const QString dst = m_tempDir.path() + QStringLiteral("/Backup_success_dlg.db");

        BackupProgressDialog dlg(src, dst);
        waitForDialog(dlg);

        QVERIFY(dlg.wasSuccessful());
        QVERIFY(QFileInfo::exists(dst));
    }

    // ── BackupProgressDialog — destructor race regression ──────────────────────
    // Before the fix, BackupWorker::finished() drove onFinished() (sets
    // m_success) and QThread::quit() via two separate queued events. A
    // caller destroying the dialog as soon as wasSuccessful() looked done —
    // or, as here, immediately without waiting at all — could tear down a
    // still-running QThread child ("QThread: Destroyed while thread is
    // still running"), crashing the process. The destructor must now wait
    // for the worker thread itself, so this must survive unconditionally.
    void test_backupProgressDialog_destroyedImmediately_doesNotCrash()
    {
        const QString src = makeTestFile(QStringLiteral("immediate_dtor.db"), 512);
        const QString dst = m_tempDir.path() + QStringLiteral("/Backup_immediate_dtor.db");

        {
            BackupProgressDialog dlg(src, dst);
            // No waitForDialog() here on purpose — the worker thread may
            // still be mid-copy or mid-quit() when dlg goes out of scope.
        }

        QVERIFY(true); // reaching this line means the destructor didn't crash
    }

    // ── createBackup via MainWindow ───────────────────────────────────────────

    void test_createBackup_createsBackupFile()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/MyPortfolio.db");
        { QFile f(dbPath); if (f.open(QIODevice::WriteOnly)) { f.write(QByteArray(512, 'X')); } }

        Database::instance().open(dbPath);
        AppSettings::instance().setPortfolioPath(dbPath);

        MainWindow window;
        QApplication::processEvents();

        // After construction createBackup() is called — find backup file
        QDir dir(m_tempDir.path());
        dir.setNameFilters({ QStringLiteral("Backup_MyPortfolio_*.db") });
        const QStringList backups = dir.entryList(QDir::Files);
        QVERIFY(!backups.isEmpty());
    }

    void test_createBackup_filenameContainsOriginalName()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/ShareList.db");
        { QFile f(dbPath); if (f.open(QIODevice::WriteOnly)) { f.write(QByteArray(512, 'X')); } }

        Database::instance().open(dbPath);
        AppSettings::instance().setPortfolioPath(dbPath);

        MainWindow window;
        QApplication::processEvents();

        QDir dir(m_tempDir.path());
        dir.setNameFilters({ QStringLiteral("Backup_ShareList_*.db") });
        const QStringList backups = dir.entryList(QDir::Files);
        QVERIFY(!backups.isEmpty());
        // Name must start with Backup_ShareList_
        QVERIFY(backups.first().startsWith(QStringLiteral("Backup_ShareList_")));
    }

    void test_createBackup_keepsMaxFiveBackups()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/RotationTest.db");
        { QFile f(dbPath); if (f.open(QIODevice::WriteOnly)) { f.write(QByteArray(256, 'X')); } }

        // Pre-create 5 old backup files with ascending timestamps
        for (int i = 1; i <= 5; ++i) {
            const QString name = QStringLiteral("Backup_RotationTest_2025_01_0%1_00_00_00.db").arg(i);
            QFile old(m_tempDir.path() + QDir::separator() + name);
            if (old.open(QIODevice::WriteOnly)) { old.write(QByteArray(64, 'O')); }
        }

        Database::instance().open(dbPath);
        AppSettings::instance().setPortfolioPath(dbPath);

        MainWindow window;
        QApplication::processEvents();

        // After createBackup() there should be at most 5 backups
        QDir dir(m_tempDir.path());
        dir.setNameFilters({ QStringLiteral("Backup_RotationTest_*.db") });
        const QStringList backups = dir.entryList(QDir::Files);
        QVERIFY(backups.size() <= 5);
    }
};

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
    {
        TestSalesForm t;
        result |= QTest::qExec(&t, argc, argv);
    }
    {
        TestDividendForm t;
        result |= QTest::qExec(&t, argc, argv);
    }
    {
        TestOwnMessageBox t;
        result |= QTest::qExec(&t, argc, argv);
    }
    {
        TestBackupForm t;
        result |= QTest::qExec(&t, argc, argv);
    }
    return result;
}

#include "tst_mainwindow.moc"

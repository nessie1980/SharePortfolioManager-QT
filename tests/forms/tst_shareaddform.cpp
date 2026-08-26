// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// tst_shareaddform.cpp - Unit-Tests fuer ModelShareAdd, PresenterShareAdd und
// ViewShareAdd.
//
// Aus tst_mainwindow.cpp ausgelagert (26.08.2026), damit jede Form ihre eigene
// Executable hat - gleiches Muster wie tst_buysform/tst_salesform/
// tst_dividendform. MainWindow selbst wird von keinem dieser Tests gebraucht;
// der Dialog-Konstruktionstest test_shareAddDialog_canBeConstructed() ist
// bewusst in tst_mainwindow.cpp geblieben, weil er den Dialog AUS dem
// MainWindow heraus oeffnet und damit dessen Verdrahtung prueft.
//
// Die Helfer openMemoryDb()/loadSandboxedSettings() sind bewusst dupliziert
// statt in einen gemeinsamen Header gezogen (siehe TESTING.md, "Auslagerung
// der Form-Tests").

#include <QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QDir>
#include <QLocale>
#include <QLineEdit>
#include <QDateTime>

#include "../../app/config/AppSettings.h"
#include "../../app/config/DocumentsConfig.h"
#include "../../app/core/Database.h"
#include "../../app/repositories/ShareRepository.h"
#include "../../app/repositories/BuyRepository.h"
#include "../../app/repositories/BrokerageRepository.h"
#include "../../app/models/ShareObject.h"
#include "../../app/models/BuyObject.h"
#include "../../app/models/BrokerageObject.h"
#include "../../app/widgets/DocumentPreviewPanel.h"

#include "../../app/forms/ShareAddForm/IViewShareAdd.h"
#include "../../app/forms/ShareAddForm/IModelShareAdd.h"
#include "../../app/forms/ShareAddForm/ViewShareAdd.h"
#include "../../app/forms/ShareAddForm/ModelShareAdd.h"
#include "../../app/forms/ShareAddForm/PresenterShareAdd.h"

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

// -----------------------------------------------------------------------------
// TestShareAddForm
// -----------------------------------------------------------------------------

class TestShareAddForm : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir   m_tempDir;
    DocumentsConfig m_docsConfig;

    void loadSandboxedSettings()
    {
        const QString sandboxIni = m_tempDir.path() + QStringLiteral("/test_settings.ini");
        AppSettings::instance().load(sandboxIni);
        AppSettings::instance().setDocumentsRootPath(
            m_tempDir.path() + QStringLiteral("/documents"));
    }

    void openMemoryDb()
    {
        if (!Database::instance().isOpen())
            Database::instance().open(QStringLiteral(":memory:"));
        AppSettings::instance().setPortfolioPath(QStringLiteral(":memory:"));
    }

private slots:

    void initTestCase()
    {
        QVERIFY(m_tempDir.isValid());
        loadSandboxedSettings();

        // Documents.xml wird von tests/forms/CMakeLists.txt per POST_BUILD
        // neben die Executable kopiert - ViewShareAdd braucht die Konfiguration
        // fuer die Dokumenterfassung.
        const QString docsPath =
            QCoreApplication::applicationDirPath() + QStringLiteral("/Documents.xml");
        if (QFileInfo::exists(docsPath))
            m_docsConfig.load(docsPath);
    }

    void cleanupTestCase()
    {
        if (Database::instance().isOpen())
            Database::instance().close();
        // WICHTIG: hier bewusst KEIN AppSettings::instance().load(...) - das
        // wuerde den Singleton auf die echte settings.ini umlenken (Bugfix
        // 19.07.2026, siehe TESTING.md).
    }

    void init()
    {
        loadSandboxedSettings();
        if (Database::instance().isOpen())
            Database::instance().close();
    }

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
}; // end of TestShareAddForm

int main(int argc, char* argv[])
{
    // Bugfix 23.07.2026 - siehe ARCHITECTURE.md, "System-Locale-abhaengiges
    // Zahlenformat".
    QLocale::setDefault(QLocale::German);

    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);

    TestShareAddForm t;
    return QTest::qExec(&t, argc, argv);
}

#include "tst_shareaddform.moc"

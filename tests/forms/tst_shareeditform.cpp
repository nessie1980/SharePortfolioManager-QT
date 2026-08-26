// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// tst_shareeditform.cpp — Unit tests for ViewShareEdit.
//
// Split out of tst_mainwindow.cpp so every Form has its own executable.

#include <QtTest>
#include <QSignalSpy>
#include <QDate>
#include <QApplication>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QDir>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QLabel>
#include <QUuid>
#include <QLocale>

#include "../../app/config/AppSettings.h"
#include "../../app/core/Database.h"
#include "../../app/repositories/ShareRepository.h"
#include "../../app/repositories/BuyRepository.h"
#include "../../app/repositories/BrokerageRepository.h"
#include "../../app/models/ShareObject.h"
#include "../../app/models/BuyObject.h"
#include "../../app/models/BrokerageObject.h"
#include "../../app/config/DocumentsConfig.h"

#include "../../app/forms/ShareEditForm/IViewShareEdit.h"
#include "../../app/forms/ShareEditForm/IModelShareEdit.h"
#include "../../app/forms/ShareEditForm/ViewShareEdit.h"
#include "../../app/forms/ShareEditForm/ModelShareEdit.h"
#include "../../app/forms/ShareEditForm/PresenterShareEdit.h"
#include "../../app/models/ShareSplitObject.h"

// ─────────────────────────────────────────────────────────────────────────────
// Stub IViewShareEdit / IModelShareEdit — used by PresenterShareEdit tests.
// Mit den Presenter-Tests aus tst_mainwindow.cpp uebernommen (26.08.2026).
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
// TestShareEditForm
// ─────────────────────────────────────────────────────────────────────────────

class TestShareEditForm : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir   m_tempDir;
    DocumentsConfig m_docsConfig;

    void loadSandboxedSettings()
    {
        const QString ini = m_tempDir.path() + QStringLiteral("/test_settings.ini");
        AppSettings::instance().load(ini);
    }

    void openMemoryDb()
    {
        if (!Database::instance().isOpen())
            Database::instance().open(QStringLiteral(":memory:"));
        AppSettings::instance().setPortfolioPath(QStringLiteral(":memory:"));
    }

    /** Insert a minimal share into the open in-memory DB and return its GUID. */
    QString insertTestShare(const QString& wkn = QStringLiteral("TSTSHRE"),
                            const QString& name = QStringLiteral("Test AG"))
    {
        const QString guid = QUuid::createUuid().toString(QUuid::WithoutBraces);
        ShareRepository repo;
        ShareObject s(guid, wkn, QStringLiteral("DE000TST0001"), name);
        repo.insert(s);
        return guid;
    }

    /** Build a fully populated ShareObject for use in loadShare() tests. */
    static ShareObject makeShare(const QString& guid)
    {
        ShareObject s(guid,
                      QStringLiteral("840400"),
                      QStringLiteral("DE0008404005"),
                      QStringLiteral("Allianz SE"));
        s.setUpdateType(ShareUpdateType::Both);
        s.setDetailsWebSiteUrl(QStringLiteral("https://example.com/details"));
        s.setMarketPriceUrl(QStringLiteral("https://example.com/market"));
        s.setMarketPriceParsingType(ShareParsingType::ApiYahoo);
        s.setDailyValuesUrl(QStringLiteral("https://example.com/daily"));
        s.setDailyValuesParsingType(ShareParsingType::ApiOnVista);
        s.setMarketPriceEncoding(QStringLiteral("en-US"));
        s.setShareType(ShareType::Etf);
        return s;
    }

    /** Build a ShareSplitObject for the setSplitInfo() tests (08.08.2026). */
    static ShareSplitObject makeSplit(const QString& guid, const QDate& date,
                                      double ratioNew, double ratioOld)
    {
        return ShareSplitObject(guid, QStringLiteral("share-guid"),
                                date, ratioNew, ratioOld);
    }

    /**
     * Find one of the four "Update via Internet"-Radiobuttons by its label.
     * Die Radios haben bewusst keine objectName-Vergabe — sie entstehen in
     * einer Schleife über eine Beschriftungstabelle in createGeneralGroup(),
     * die Beschriftung ist dort der einzige stabile Bezug.
     */
    static QRadioButton* updateRadio(const ViewShareEdit& dlg, const QString& label)
    {
        for (auto* rb : dlg.findChildren<QRadioButton*>())
            if (rb->text() == label)
                return rb;
        return nullptr;
    }

    /**
     * Zweiter Share-Helfer, mit den PresenterShareEdit-/ModelShareEdit-Tests
     * aus tst_mainwindow.cpp uebernommen (26.08.2026). Anders als
     * insertTestShare() oben vergibt er eine FESTE GUID ("share-test-1") und
     * oeffnet die In-Memory-DB selbst — beides setzen die uebernommenen Tests
     * voraus. Bewusst dupliziert statt zusammengelegt: der abweichende Name
     * haelt die beiden Semantiken auseinander.
     */
    QString insertFixedTestShare()
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

    /**
     * Legt einen Kauf samt zugehoeriger Kauf-Gebuehr fuer die uebergebene
     * Aktie an — Gegenstueck zu insertTestBuy() in tst_mainwindow.cpp, hier
     * wegen der Namensgebung des Hauses umbenannt.
     */
    BuyObject insertTestBuyForShare(const QString& shareGuid,
                                    const QString& depotNumber,
                                    const QString& dateTime,
                                    double volume,
                                    double price)
    {
        BuyRepository repo;
        const QString guid = QUuid::createUuid().toString(QUuid::WithoutBraces);
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
        AppSettings::instance().load(QString());
    }

    void init()
    {
        loadSandboxedSettings();
        if (Database::instance().isOpen())
            Database::instance().close();
    }

    // ── Construction ──────────────────────────────────────────────────────────

    void test_viewShareEdit_canBeConstructed()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);
        QVERIFY(dlg.windowTitle().contains("Aktie"));
    }

    void test_viewShareEdit_hasPencilButtons()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);
        const auto btns = dlg.findChildren<QPushButton*>();
        // Fünf Stift-Buttons: Käufe, Verkäufe, Dividenden, Kosten (alle in
        // "Einnahmen / Ausgabe") und Splits (in "Allgemein", seit 08.08.2026).
        // Sie haben ein Icon und keine sichtbare Beschriftung (pencilIcon + QString()).
        int pencilCount = 0;
        for (auto* b : btns) {
            if (b->text().isEmpty() && !b->icon().isNull())
                ++pencilCount;
        }
        QCOMPARE(pencilCount, 5);
    }

    void test_viewShareEdit_hasSaveAndCloseButtons()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);
        bool hasSave  = false;
        bool hasClose = false;
        for (auto* b : dlg.findChildren<QPushButton*>()) {
            if (b->text().contains("Speichern")) hasSave  = true;
            if (b->text().contains("Schlie"))    hasClose = true;
        }
        QVERIFY(hasSave);
        QVERIFY(hasClose);
    }

    // ── loadShare() ───────────────────────────────────────────────────────────

    void test_viewShareEdit_loadShare_setsWknAndIsin()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);
        const ShareObject s = makeShare(guid);
        dlg.loadShare(s);
        QCOMPARE(dlg.wkn(),  QStringLiteral("840400"));
        QCOMPARE(dlg.isin(), QStringLiteral("DE0008404005"));
    }

    void test_viewShareEdit_loadShare_setsName()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);
        dlg.loadShare(makeShare(guid));
        QCOMPARE(dlg.name(), QStringLiteral("Allianz SE"));
    }

    void test_viewShareEdit_loadShare_setsUpdateType()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);
        dlg.loadShare(makeShare(guid));
        QCOMPARE(dlg.updateType(), ShareUpdateType::Both);
    }

    void test_viewShareEdit_loadShare_setsMarketUrl()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);
        dlg.loadShare(makeShare(guid));
        QCOMPARE(dlg.marketPriceUrl(), QStringLiteral("https://example.com/market"));
    }

    void test_viewShareEdit_loadShare_setsDailyUrl()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);
        dlg.loadShare(makeShare(guid));
        QCOMPARE(dlg.dailyValuesUrl(), QStringLiteral("https://example.com/daily"));
    }

    void test_viewShareEdit_loadShare_setsMarketParsingType()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);
        dlg.loadShare(makeShare(guid));
        QCOMPARE(dlg.marketPriceParsingType(), ShareParsingType::ApiYahoo);
    }

    void test_viewShareEdit_loadShare_setsDailyParsingType()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);
        dlg.loadShare(makeShare(guid));
        QCOMPARE(dlg.dailyValuesParsingType(), ShareParsingType::ApiOnVista);
    }

    void test_viewShareEdit_loadShare_setsShareType()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);
        dlg.loadShare(makeShare(guid));
        QCOMPARE(dlg.shareType(), ShareType::Etf);
    }

    void test_viewShareEdit_loadShare_setsDetailsWebsite()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);
        dlg.loadShare(makeShare(guid));
        QCOMPARE(dlg.detailsWebsite(), QStringLiteral("https://example.com/details"));
    }

    // ── Aggregate setters ─────────────────────────────────────────────────────

    void test_viewShareEdit_setFirstBuyDate_setsText()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);
        dlg.setFirstBuyDate(QStringLiteral("2022-03-15"));
        // The date field is read-only; verify via findChildren
        bool found = false;
        for (auto* le : dlg.findChildren<QLineEdit*>())
            if (le->text() == QStringLiteral("2022-03-15")) { found = true; break; }
        QVERIFY(found);
    }

    void test_viewShareEdit_setFirstBuyDate_emptyShowsDash()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);
        dlg.setFirstBuyDate(QString());
        bool found = false;
        for (auto* le : dlg.findChildren<QLineEdit*>())
            if (le->text() == QStringLiteral("-")) { found = true; break; }
        QVERIFY(found);
    }

    void test_viewShareEdit_setCurrentVolume_formatsWithFourDecimals()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);
        dlg.setCurrentVolume(12.5);
        const QString expected = QLocale().toString(12.5, 'f', 4);
        bool found = false;
        for (auto* le : dlg.findChildren<QLineEdit*>())
            if (le->text() == expected) { found = true; break; }
        QVERIFY(found);
    }

    void test_viewShareEdit_setTotalBuys_setsEinzahlungAndKaeufe()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);
        dlg.setTotalBuys(1234.56, 3);
        const QString expected = QLocale().toString(1234.56, 'f', 2);
        int fieldCount = 0;
        for (auto* le : dlg.findChildren<QLineEdit*>())
            if (le->text() == expected) ++fieldCount;
        // setTotalBuys writes to both m_totalBuys and m_einzahlung
        QVERIFY(fieldCount >= 2);
    }

    void test_viewShareEdit_setTotalSales_setsField()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);
        dlg.setTotalSales(500.0, 1);
        const QString expected = QLocale().toString(500.0, 'f', 2);
        bool found = false;
        for (auto* le : dlg.findChildren<QLineEdit*>())
            if (le->text() == expected) { found = true; break; }
        QVERIFY(found);
    }

    void test_viewShareEdit_setTotalProfitLoss_positiveIsGreen()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);
        dlg.setTotalProfitLoss(100.0, 1);
        bool greenFound = false;
        for (auto* le : dlg.findChildren<QLineEdit*>()) {
            if (le->styleSheet().contains(QStringLiteral("green"))) {
                greenFound = true; break;
            }
        }
        QVERIFY(greenFound);
    }

    void test_viewShareEdit_setTotalProfitLoss_negativeIsRed()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);
        dlg.setTotalProfitLoss(-50.0, 1);
        bool redFound = false;
        for (auto* le : dlg.findChildren<QLineEdit*>()) {
            if (le->styleSheet().contains(QStringLiteral("red"))) {
                redFound = true; break;
            }
        }
        QVERIFY(redFound);
    }

    void test_viewShareEdit_setTotalProfitLoss_zeroNoColor()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);
        dlg.setTotalProfitLoss(-50.0, 1);  // set a color first
        dlg.setTotalProfitLoss(0.0, 0);    // then reset to zero
        bool colorFound = false;
        for (auto* le : dlg.findChildren<QLineEdit*>()) {
            if (le->styleSheet().contains(QStringLiteral("red")) ||
                le->styleSheet().contains(QStringLiteral("green"))) {
                colorFound = true; break;
            }
        }
        QVERIFY(!colorFound);
    }

    void test_viewShareEdit_setTotalDividends_setsField()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);
        dlg.setTotalDividends(75.25, 2);
        const QString expected = QLocale().toString(75.25, 'f', 2);
        bool found = false;
        for (auto* le : dlg.findChildren<QLineEdit*>())
            if (le->text() == expected) { found = true; break; }
        QVERIFY(found);
    }

    void test_viewShareEdit_setTotalBrokerages_setsField()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);
        dlg.setTotalBrokerages(18.90, 4);
        const QString expected = QLocale().toString(18.90, 'f', 2);
        bool found = false;
        for (auto* le : dlg.findChildren<QLineEdit*>())
            if (le->text() == expected) { found = true; break; }
        QVERIFY(found);
    }

    // ── API-Key-Felder ────────────────────────────────────────────────────────

    void test_viewShareEdit_marketApiKey_disabledForRegex()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);
        ShareObject s = makeShare(guid);
        s.setMarketPriceParsingType(ShareParsingType::Regex);
        dlg.loadShare(s);
        // API-Key field must be disabled (read-only) for Regex mode
        QVERIFY(dlg.marketPriceApiKey().isEmpty());
    }

    void test_viewShareEdit_marketApiKey_setFromSettingsForYahoo()
    {
        openMemoryDb();
        AppSettings::instance().setApiKeyYahoo(QStringLiteral("yahoo-test-key"));
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);
        ShareObject s = makeShare(guid);
        s.setMarketPriceParsingType(ShareParsingType::ApiYahoo);
        dlg.loadShare(s);
        QCOMPARE(dlg.marketPriceApiKey(), QStringLiteral("yahoo-test-key"));
    }

    void test_viewShareEdit_dailyApiKey_setFromSettingsForOnVista()
    {
        openMemoryDb();
        AppSettings::instance().setApiKeyOnVista(QStringLiteral("onvista-test-key"));
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);
        ShareObject s = makeShare(guid);
        s.setDailyValuesParsingType(ShareParsingType::ApiOnVista);
        dlg.loadShare(s);
        QCOMPARE(dlg.dailyValuesApiKey(), QStringLiteral("onvista-test-key"));
    }

    // ── setDailyValuesRequired (Feature 06.08.2026) ───────────────────────────

    void test_viewShareEdit_updateRadios_allPresentAndEnabledByDefault()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);

        // Ohne Käufe ist der Bestand 0 — der Presenter ruft
        // setDailyValuesRequired(false) auf, alle vier bleiben wählbar.
        for (const QString& label : { QStringLiteral("Beide"),
                                      QStringLiteral("Markt-Preis"),
                                      QStringLiteral("Tages-Werte"),
                                      QStringLiteral("Keine") })
        {
            QRadioButton* rb = updateRadio(dlg, label);
            QVERIFY2(rb, qPrintable(QStringLiteral("Radio fehlt: ") + label));
            QVERIFY2(rb->isEnabled(), qPrintable(QStringLiteral("Radio gesperrt: ") + label));
        }
    }

    void test_viewShareEdit_setDailyValuesRequired_disablesMarketPriceAndNone()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);

        dlg.setDailyValuesRequired(true);

        QVERIFY(!updateRadio(dlg, QStringLiteral("Markt-Preis"))->isEnabled());
        QVERIFY(!updateRadio(dlg, QStringLiteral("Keine"))->isEnabled());
    }

    void test_viewShareEdit_setDailyValuesRequired_keepsDailyVariantsSelectable()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);

        dlg.setDailyValuesRequired(true);

        QVERIFY(updateRadio(dlg, QStringLiteral("Beide"))->isEnabled());
        QVERIFY(updateRadio(dlg, QStringLiteral("Tages-Werte"))->isEnabled());
    }

    void test_viewShareEdit_setDailyValuesRequired_falseReEnablesAll()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);

        dlg.setDailyValuesRequired(true);
        dlg.setDailyValuesRequired(false);

        QVERIFY(updateRadio(dlg, QStringLiteral("Markt-Preis"))->isEnabled());
        QVERIFY(updateRadio(dlg, QStringLiteral("Keine"))->isEnabled());
    }

    void test_viewShareEdit_setDailyValuesRequired_keepsStoredSelectionChecked()
    {
        // Kern der Altbestands-Entscheidung: eine gespeicherte, jetzt
        // unzulässige Auswahl darf beim Öffnen NICHT still umgestellt werden.
        // Der deaktivierte Radiobutton bleibt sichtbar angehakt, updateType()
        // liefert weiterhin den gespeicherten Wert — blockiert wird erst beim
        // Speichern, in PresenterShareEdit::validateInput().
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);

        ShareObject s = makeShare(guid);
        s.setUpdateType(ShareUpdateType::None);
        dlg.loadShare(s);
        dlg.setDailyValuesRequired(true);

        QRadioButton* none = updateRadio(dlg, QStringLiteral("Keine"));
        QVERIFY(none->isChecked());
        QVERIFY(!none->isEnabled());
        QCOMPARE(dlg.updateType(), ShareUpdateType::None);
    }

    void test_viewShareEdit_updateHint_hiddenUntilRequired()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);

        auto* hint = dlg.findChild<QLabel*>(QStringLiteral("updateHint"));
        QVERIFY(hint);
        // isVisibleTo() statt isVisible(): der Dialog selbst wird im Test nie
        // gezeigt, isVisible() wäre für jedes Kind false.
        QVERIFY(!hint->isVisibleTo(&dlg));
        QVERIFY(hint->text().isEmpty());
    }

    void test_viewShareEdit_updateHint_shownWhenRequired()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);

        dlg.setDailyValuesRequired(true);

        auto* hint = dlg.findChild<QLabel*>(QStringLiteral("updateHint"));
        QVERIFY(hint);
        QVERIFY(hint->isVisibleTo(&dlg));
        QVERIFY(!hint->text().isEmpty());
    }

    // ── setSplitInfo (Phase 3 der Aktiensplit-Behandlung, 08.08.2026) ────────
    //
    // Der Hinweis sitzt bewusst unmittelbar neben dem Stift-Button, über den
    // ein Split erfasst wird (Nessies Entscheidung 08.08.2026) — nicht in
    // einer Fusszeile. Geprüft wird deshalb genau dieses eine Feld, nicht
    // irgendein QLineEdit im Dialog.

    void test_viewShareEdit_hasSplitsFieldAndButton()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);

        QVERIFY(dlg.findChild<QLineEdit*>(QStringLiteral("splitsField")));
        QVERIFY(dlg.findChild<QPushButton*>(QStringLiteral("btnEditSplits")));
    }

    void test_viewShareEdit_setSplitInfo_emptyShowsKeine()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);

        dlg.setSplitInfo({});

        auto* field = dlg.findChild<QLineEdit*>(QStringLiteral("splitsField"));
        if (!field) QFAIL("splitsField not found");
        QCOMPARE(field->text(), QStringLiteral("keine"));
    }

    void test_viewShareEdit_setSplitInfo_singleSplitShowsRatioAndDate()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);

        dlg.setSplitInfo({ makeSplit(QStringLiteral("s1"), QDate(2022, 7, 18), 20.0, 1.0) });

        auto* field = dlg.findChild<QLineEdit*>(QStringLiteral("splitsField"));
        if (!field) QFAIL("splitsField not found");
        // Ganze Verhältnisse ohne Nachkommastellen — "20:1", nicht "20,00:1,00".
        QVERIFY2(field->text().startsWith(QStringLiteral("20:1")),
                 qPrintable(field->text()));
        QVERIFY(field->text().contains(
            QLocale().toString(QDate(2022, 7, 18), QLocale::ShortFormat)));
    }

    void test_viewShareEdit_setSplitInfo_multipleShowsCountAndLatest()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);

        // Aufsteigend nach Datum, wie ShareSplitRepository::findByShare() liefert —
        // der letzte Eintrag muss als "zuletzt" erscheinen.
        dlg.setSplitInfo({ makeSplit(QStringLiteral("s1"), QDate(2014, 4, 3),  2.0,  1.0),
                           makeSplit(QStringLiteral("s2"), QDate(2022, 7, 18), 20.0, 1.0) });

        auto* field = dlg.findChild<QLineEdit*>(QStringLiteral("splitsField"));
        if (!field) QFAIL("splitsField not found");
        QVERIFY2(field->text().contains(QStringLiteral("2 Splits")),
                 qPrintable(field->text()));
        QVERIFY2(field->text().contains(QStringLiteral("20:1")),
                 qPrintable(field->text()));
    }

    void test_viewShareEdit_setSplitInfo_tooltipListsAllSplits()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);

        dlg.setSplitInfo({ makeSplit(QStringLiteral("s1"), QDate(2014, 4, 3),  2.0,  1.0),
                           makeSplit(QStringLiteral("s2"), QDate(2022, 7, 18), 20.0, 1.0) });

        auto* field = dlg.findChild<QLineEdit*>(QStringLiteral("splitsField"));
        if (!field) QFAIL("splitsField not found");
        // Das Feld ist zu schmal für mehrere Splits — die vollständige Liste
        // muss deshalb über den Tooltip erreichbar bleiben.
        QVERIFY(field->toolTip().contains(QStringLiteral("2:1")));
        QVERIFY(field->toolTip().contains(QStringLiteral("20:1")));
    }

    void test_viewShareEdit_setSplitInfo_reverseSplitKeepsRatioOrder()
    {
        // Reverse-Split 1:10 — ratioNew=1, ratioOld=10. Die Reihenfolge darf
        // nicht vertauscht dargestellt werden, sonst wäre aus einer
        // Zusammenlegung optisch eine Teilung geworden.
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);

        dlg.setSplitInfo({ makeSplit(QStringLiteral("s1"), QDate(2023, 5, 2), 1.0, 10.0) });

        auto* field = dlg.findChild<QLineEdit*>(QStringLiteral("splitsField"));
        if (!field) QFAIL("splitsField not found");
        QVERIFY2(field->text().startsWith(QStringLiteral("1:10")),
                 qPrintable(field->text()));
    }

    // ── refreshSummary ────────────────────────────────────────────────────────

    void test_viewShareEdit_refreshSummary_doesNotCrash()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);
        // refreshSummary delegates to the internal presenter; must not crash
        dlg.refreshSummary();
    }

    // ─────────────────────────────────────────────────────────────────────
    // ModelShareEdit / PresenterShareEdit — aus tst_mainwindow.cpp
    // uebernommen (26.08.2026). Sie brauchen weder MainWindow noch dessen
    // Testfixture, gehoeren also hierher.
    // ─────────────────────────────────────────────────────────────────────

    void test_modelShareEdit_loadShare_returnsValidShare()
    {
        const QString shareGuid = insertFixedTestShare();
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
        const QString shareGuid = insertFixedTestShare();
        ModelShareEdit model;
        ShareObject share = model.loadShare(shareGuid);
        share.setName(QStringLiteral("Renamed AG"));
        QVERIFY(model.saveShare(share));

        const ShareObject reloaded = model.loadShare(shareGuid);
        QCOMPARE(reloaded.name(), QStringLiteral("Renamed AG"));
    }

    void test_modelShareEdit_currentVolume_sumsBuyMinusSold()
    {
        const QString shareGuid = insertFixedTestShare();
        insertTestBuyForShare(shareGuid, QStringLiteral("depot1"),
                      QStringLiteral("2024-01-10T10:00:00"), 10.0, 50.0);
        BuyObject b2 = insertTestBuyForShare(shareGuid, QStringLiteral("depot1"),
                                     QStringLiteral("2024-02-15T10:00:00"), 20.0, 55.0);

        BuyRepository buyRepo;
        QVERIFY(buyRepo.updateVolumeSold(b2.guid(), 5.0));

        ModelShareEdit model;
        // 10 (unsold) + 20 - 5 (partially sold) = 25
        QCOMPARE(model.currentVolume(shareGuid), 25.0);
    }

    void test_modelShareEdit_currentVolume_noBuys_returnsZero()
    {
        const QString shareGuid = insertFixedTestShare();
        ModelShareEdit model;
        QCOMPARE(model.currentVolume(shareGuid), 0.0);
    }

    void test_modelShareEdit_firstBuyDate_returnsEarliestBuyDate()
    {
        const QString shareGuid = insertFixedTestShare();
        insertTestBuyForShare(shareGuid, QStringLiteral("depot1"),
                      QStringLiteral("2024-06-01T10:00:00"), 5.0, 60.0);
        const BuyObject earliest = insertTestBuyForShare(
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
        const QString shareGuid = insertFixedTestShare();
        ModelShareEdit model;
        QVERIFY(model.firstBuyDate(shareGuid).isEmpty());
    }

    void test_modelShareEdit_totalBuyValue_delegatesToRepository()
    {
        const QString shareGuid = insertFixedTestShare();
        insertTestBuyForShare(shareGuid, QStringLiteral("depot1"),
                      QStringLiteral("2024-01-10T10:00:00"), 10.0, 50.0);

        ModelShareEdit model;
        BuyRepository  buyRepo;
        QCOMPARE(model.totalBuyValue(shareGuid),
                 buyRepo.totalBuyValueBrokerageReduction(shareGuid));
        QCOMPARE(model.buyCount(shareGuid), 1);
    }

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
};


int main(int argc, char* argv[])
{
    // Bugfix 23.07.2026 — siehe ARCHITECTURE.md, "System-Locale-abhaengiges
    // Zahlenformat": muss vor jeder QLocale()-Verwendung gesetzt werden.
    QLocale::setDefault(QLocale::German);

    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);

    TestShareEditForm t;
    return QTest::qExec(&t, argc, argv);
}

#include "tst_shareeditform.moc"

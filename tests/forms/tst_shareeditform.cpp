// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// tst_shareeditform.cpp — Unit tests for ViewShareEdit.
//
// Split out of tst_mainwindow.cpp so every Form has its own executable.

#include <QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QDir>
#include <QLineEdit>
#include <QPushButton>
#include <QUuid>
#include <QLocale>

#include "../../app/config/AppSettings.h"
#include "../../app/core/Database.h"
#include "../../app/repositories/ShareRepository.h"
#include "../../app/models/ShareObject.h"
#include "../../app/config/DocumentsConfig.h"

#include "../../app/forms/ShareEditForm/IViewShareEdit.h"
#include "../../app/forms/ShareEditForm/IModelShareEdit.h"
#include "../../app/forms/ShareEditForm/ViewShareEdit.h"
#include "../../app/forms/ShareEditForm/ModelShareEdit.h"
#include "../../app/forms/ShareEditForm/PresenterShareEdit.h"

// ─────────────────────────────────────────────────────────────────────────────
// TestViewShareEdit
// ─────────────────────────────────────────────────────────────────────────────

class TestViewShareEdit : public QObject
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
        // Four pencil buttons: Käufe, Verkäufe, Dividenden, Kosten.
        // They have an icon and no visible text (created with pencilIcon + QString()).
        int pencilCount = 0;
        for (auto* b : btns) {
            if (b->text().isEmpty() && !b->icon().isNull())
                ++pencilCount;
        }
        QCOMPARE(pencilCount, 4);
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

    // ── refreshSummary ────────────────────────────────────────────────────────

    void test_viewShareEdit_refreshSummary_doesNotCrash()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareEdit dlg(guid, &m_docsConfig);
        // refreshSummary delegates to the internal presenter; must not crash
        dlg.refreshSummary();
    }
};


int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);

    TestViewShareEdit t;
    return QTest::qExec(&t, argc, argv);
}

#include "tst_shareeditform.moc"

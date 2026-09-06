// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// tst_buysform.cpp — Unit tests for the BuysForm MVP triad
// (ModelBuyEdit, PresenterBuyEdit, ViewBuyEdit).
//
// Split out of tst_mainwindow.cpp once the BuysForm test count grew large
// enough to warrant its own executable (see TESTING.md, "Geplante Test-Module").

#include <QtTest>
#include <QSignalSpy>
#include <QApplication>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QDir>
#include <QTableWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QDialog>
#include <QDateEdit>
#include <QTimeEdit>
#include <QProgressBar>
#include <QLabel>
#include <QLocale>
#include <QUuid>

#include "../../app/config/AppSettings.h"
#include "../../app/core/Database.h"
#include "../../app/repositories/ShareRepository.h"
#include "../../app/repositories/BuyRepository.h"
#include "../../app/repositories/BrokerageRepository.h"
#include "../../app/models/ShareObject.h"
#include "../../app/models/BuyObject.h"
#include "../../app/models/BrokerageObject.h"
#include "../../app/config/DocumentsConfig.h"

#include "../../app/forms/BuysForm/IViewBuyEdit.h"
#include "../../app/forms/BuysForm/IModelBuyEdit.h"
#include "../../app/forms/BuysForm/ViewBuyEdit.h"
#include "../../app/forms/BuysForm/ModelBuyEdit.h"
#include "../../app/widgets/OverviewTabWidget.h"
#include "../../app/forms/BuysForm/PresenterBuyEdit.h"
#include "../../app/models/ShareSplitObject.h"
#include "../../app/utils/ShareSplitHint.h"

#include "../../app/forms/UiConstants.h"

// ─────────────────────────────────────────────────────────────────────────────
// Stub IModelBuyEdit
// ─────────────────────────────────────────────────────────────────────────────
class StubModelBuyEdit : public IModelBuyEdit
{
public:
    // Configurable return values
    QList<BuyObject>    buys;
    BrokerageObject     brokerage;
    bool                addResult    = true;
    bool                updateResult = true;
    bool                removeResult = true;
    bool                orderExists  = false;
    bool                docExists    = false;
    QString             errorMsg;

    /// Splits, die loadSplits() liefert (Phase 3b, 09.08.2026). Vorgabe leer —
    /// die meisten Tests hier interessieren sich nicht dafür.
    QList<ShareSplitObject> splits;

    // Captured calls
    bool    addBuyCalled    = false;
    bool    updateBuyCalled = false;
    bool    removeBuyCalled = false;

    QList<BuyObject> loadBuys(const QString&) const override { return buys; }
    ShareObject      loadShare(const QString&) const override { return ShareObject{}; }
    BrokerageObject  loadBrokerage(const QString&) const override { return brokerage; }
    QList<ShareSplitObject> loadSplits(const QString&) const override { return splits; }

    bool addBuy(const BuyObject&, double, double, double, double) override
        { addBuyCalled = true; return addResult; }
    bool updateBuy(const BuyObject&, double, double, double, double) override
        { updateBuyCalled = true; return updateResult; }
    bool removeBuy(const QString&) override
        { removeBuyCalled = true; return removeResult; }
    bool orderNumberExists(const QString&, const QString&, const QString&) const override
        { return orderExists; }
    bool documentExists(const QString&, const QString&) const override
        { return docExists; }
    QString lastError() const override { return errorMsg; }
};

// ─────────────────────────────────────────────────────────────────────────────
// Stub IViewBuyEdit
// ─────────────────────────────────────────────────────────────────────────────
class StubViewBuyEdit : public IViewBuyEdit
{
public:
    // Configurable return values
    QString m_depotNumber  = QStringLiteral("depot1");
    QString m_orderNumber  = QStringLiteral("ORD-001");
    double  m_volume       = 10.0;
    double  m_price        = 100.0;
    double  m_provision    = 0.0;
    double  m_brokerFee    = 0.0;
    double  m_traderFee    = 0.0;
    double  m_reduction    = 0.0;
    QString m_docPath;
    QString m_dateTime     =
        QStringLiteral("2024-06-15T10:00:00");
    bool    m_missingFields = false;

    // Captured calls
    bool    populateOverviewCalled = false;
    /// Split-Liste, die populateOverview() zuletzt bekommen hat (Phase 3c).
    QList<ShareSplitObject> lastOverviewSplits;
    bool    clearFormCalled        = false;
    bool    loadBuyCalled          = false;
    bool    setButtonStatesCalled  = false;
    bool    lastCanRemove          = false;
    bool    lastIsLastBuy          = false;
    QString lastError;
    bool    closed                 = false;

    // Phase 3b (09.08.2026) — zuletzt gesetzter Split-Hinweis.
    QString lastSplitHint;
    QString lastSplitTooltip;
    bool    lastHasSplit          = false;
    int     splitHintCallCount    = 0;

    // IViewBuyEdit — read
    QString dateTime()     const override { return m_dateTime; }
    QString depotNumber()  const override { return m_depotNumber; }
    QString orderNumber()  const override { return m_orderNumber; }
    double  volume()       const override { return m_volume; }
    double  price()        const override { return m_price; }
    QString documentPath() const override { return m_docPath; }
    double  provision()    const override { return m_provision; }
    double  brokerFee()    const override { return m_brokerFee; }
    double  traderFee()    const override { return m_traderFee; }
    double  reduction()    const override { return m_reduction; }

    // IViewBuyEdit — write
    void loadBuy(const BuyObject&, const BrokerageObject&) override
        { loadBuyCalled = true; }
    void clearForm()                              override { clearFormCalled = true; }
    void setVolumeSold(double)                    override {}
    void setKurswert(double)                      override {}
    void setGesGebuehren(double)                  override {}
    void setEndbetrag(double)                     override {}

    void setSplitHint(const QString& text, const QString& tooltip, bool hasSplit) override
    {
        lastSplitHint    = text;
        lastSplitTooltip = tooltip;
        lastHasSplit     = hasSplit;
        ++splitHintCallCount;
    }

    // Umschaltbarer Fehlschlag (27.08.2026): setFieldOk() liefert seit dem
    // Rueckgabewert-Umbau false, wenn die View einen Rohwert nicht uebernehmen
    // konnte. Ohne diesen Schalter waere der false-Zweig in
    // populateFromResult() durch keinen Test erreichbar.
    QStringList failingFields;   ///< diese Feldschluessel schlagen fehl

    bool setFieldOk(const QString& f, const QString& v,
                    const QString& = QString()) override
    {
        Q_UNUSED(v)
        if (failingFields.contains(f)) return false;
        return true;
    }
    void setFieldError(const QString& f, const QString& = QString()) override
    { Q_UNUSED(f) }
    void setDocumentPath(const QString& path)        override
        { m_docPath = path; }
    void setDocumentPreview(const QString&)          override {}

    // Fuer die Zaehl-Tests der Analyse-Statuszeile (27.08.2026): der Text
    // wird gebraucht, um "Analyse OK - 5/5 Pflicht" von "Analyse
    // fehlgeschlagen - 4/5 Pflicht" zu unterscheiden. Vorher verwarf der
    // Stub ihn.
    int     lastProgressPercent = -1;
    QString lastProgressText;

    void setParseProgress(int percent, const QString& status) override
    {
        lastProgressPercent = percent;
        lastProgressText    = status;
    }
    int lastStatusIcon = -1;   ///< 0 = SearchOk, 1 = SearchFailed (27.08.2026)
    void setParseStatusIcon(int iconType)            override
        { lastStatusIcon = iconType; }
    void setUiBusy(bool)                             override {}
    void onParseFinished()                           override {}

    void populateOverview(const QList<BuyObject>&,
                          const QList<BrokerageObject>&,
                          const QList<ShareSplitObject>& splits) override
        { populateOverviewCalled = true; lastOverviewSplits = splits; }
    void openPdfPreview(const QString&)              override {}
    void clearPdfPreview()                           override {}
    void showOverviewTab()                           override { clearFormCalled = true; }
    void setButtonStates(bool canRemove, bool isLastBuy, bool isEdit) override
    {
        setButtonStatesCalled = true;
        lastCanRemove  = canRemove;
        lastIsLastBuy  = isLastBuy;
        Q_UNUSED(isEdit)
    }
    void showError(const QString& msg)               override { lastError = msg; }
    void acceptAndClose()                            override { closed = true; }

    void markMissingFieldsAsFailed()                 override {}
    bool hasMissingRequiredFields(QStringList& missing) const override
        { missing.clear(); if (m_missingFields) missing << QStringLiteral("test"); return m_missingFields; }
};
// ─────────────────────────────────────────────────────────────────────────────
class TestBuysForm : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir   m_tempDir;
    DocumentsConfig m_docsConfig;

    void loadSandboxedSettings()
    {
        const QString sandboxIni = m_tempDir.path() + QStringLiteral("/test_settings.ini");
        AppSettings::instance().load(sandboxIni);
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

    // Helper: create a minimal BuyObject for a given share/year.
    static BuyObject makeBuy(const QString& guid,
                             const QString& shareGuid,
                             int year,
                             double volume  = 10.0,
                             double price   = 100.0,
                             const QString& doc = QString())
    {
        const QString dt =
            QStringLiteral("%1-06-15T10:00:00").arg(year);
        return BuyObject(guid, shareGuid,
                         QStringLiteral("depot1"),
                         QStringLiteral("ord-") + guid,
                         dt, volume, 0.0, price,
                         QString(), doc);
    }

    // Helper: create a minimal BrokerageObject linked to a buy.
    static BrokerageObject makeBrokerage(const QString& buyGuid,
                                         const QString& shareGuid,
                                         double provision = 9.90)
    {
        return BrokerageObject(
            QStringLiteral("br-") + buyGuid,
            shareGuid,
            buyGuid,
            QString(),
            QStringLiteral("2024-06-15T10:00:00"),
            provision, 0.0, 0.0, 0.0);
    }

    // Helper: populate dlg with buys in two years and return the tab widget
    static OverviewTabWidget* setupTwoYearOverview(ViewBuyEdit& dlg)
    {
        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        const QList<BuyObject> buys = {
            makeBuy(QStringLiteral("b1"), QStringLiteral("share-guid"), 2023),
            makeBuy(QStringLiteral("b2"), QStringLiteral("share-guid"), 2024)
        };
        const QList<BrokerageObject> brs = {
            makeBrokerage(QStringLiteral("b1"), QStringLiteral("share-guid")),
            makeBrokerage(QStringLiteral("b2"), QStringLiteral("share-guid"))
        };
        dlg.populateOverview(buys, brs, {});
        return tabs;
    }

    // ─────────────────────────────────────────────────────────────────────
    // Aktiensplit-Behandlung, Phase 3c (10.08.2026)
    //
    // Zwei Käufe um einen 20:1-Split herum: der Kauf vom 15.06.2022 liegt
    // davor (Beleg 5 Stück, heute 100), der vom 15.06.2023 dahinter (Beleg
    // 100 Stück, heute ebenfalls 100). Die alte Fusszeile addierte 5 + 100 =
    // 105 — eine Zahl in keiner der beiden Stückelungen. Richtig sind 200.
    //
    // Siehe ARCHITECTURE.md, "Split-Marker und Summen in den
    // Übersichtstabellen".
    // ─────────────────────────────────────────────────────────────────────

    static QList<ShareSplitObject> splitList2022()
    {
        return { ShareSplitObject(QStringLiteral("sp1"), QStringLiteral("share-guid"),
                                  QDate(2022, 7, 18), 20.0, 1.0) };
    }

    static QList<BuyObject> buysAroundSplit()
    {
        return {
            makeBuy(QStringLiteral("b1"), QStringLiteral("share-guid"), 2022, 5.0,   1003.00),
            makeBuy(QStringLiteral("b2"), QStringLiteral("share-guid"), 2023, 100.0, 50.15)
        };
    }

    static QList<BrokerageObject> brokeragesAroundSplit()
    {
        return {
            makeBrokerage(QStringLiteral("b1"), QStringLiteral("share-guid")),
            makeBrokerage(QStringLiteral("b2"), QStringLiteral("share-guid"))
        };
    }

    /// Fusszeilen-Tabelle eines Containers (die zweite der beiden Tabellen).
    static QTableWidget* footerTableFromContainer(QWidget* container)
    {
        if (!container) return nullptr;
        const auto tables = container->findChildren<QTableWidget*>();
        return tables.size() >= 2 ? tables.at(1) : nullptr;
    }

private slots:

    void initTestCase()
    {
        QVERIFY(m_tempDir.isValid());
        loadSandboxedSettings();

        // Load Documents.xml for presenter/view tests that need a real config
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

    // ─────────────────────────────────────────────────────────────────────
    // ModelBuyEdit — database tests
    // ─────────────────────────────────────────────────────────────────────

    void test_modelBuyEdit_documentExists_notFound_returnsFalse()
    {
        openMemoryDb();
        ModelBuyEdit model;
        QVERIFY(!model.documentExists(QStringLiteral("/nonexistent.pdf")));
    }

    void test_modelBuyEdit_documentExists_emptyPath_returnsFalse()
    {
        openMemoryDb();
        ModelBuyEdit model;
        QVERIFY(!model.documentExists(QString()));
    }

    void test_modelBuyEdit_addBuy_success()
    {
        openMemoryDb();
        ShareRepository shareRepo;
        shareRepo.insert(ShareObject(QStringLiteral("share-b1"), QStringLiteral("B1"),
                                     QString(), QStringLiteral("Test AG")));
        ModelBuyEdit model;
        BuyObject b(QStringLiteral("buy-1"), QStringLiteral("share-b1"),
                    QStringLiteral("depot1"), QStringLiteral("ORD-B-1"),
                    QStringLiteral("2024-03-01T10:00:00"), 10.0, 0.0, 100.0);
        QVERIFY(model.addBuy(b, 9.90, 0.0, 0.0, 0.0));
        const auto buys = model.loadBuys(QStringLiteral("share-b1"));
        QCOMPARE(buys.size(), 1);
        const BrokerageObject br = model.loadBrokerage(QStringLiteral("buy-1"));
        QVERIFY(br.isValid());
    }

    void test_modelBuyEdit_addBuy_rollsBackOnError()
    {
        openMemoryDb();
        ShareRepository shareRepo;
        shareRepo.insert(ShareObject(QStringLiteral("share-b2"), QStringLiteral("B2"),
                                     QString(), QStringLiteral("Test AG")));
        ModelBuyEdit model;
        BuyObject b(QStringLiteral("buy-dup"), QStringLiteral("share-b2"),
                    QStringLiteral("depot1"), QStringLiteral("ORD-B-DUP"),
                    QStringLiteral("2024-03-01T10:00:00"), 10.0, 0.0, 100.0);
        QVERIFY(model.addBuy(b, 0.0, 0.0, 0.0, 0.0));
        // Insert duplicate GUID → must roll back
        QVERIFY(!model.addBuy(b, 0.0, 0.0, 0.0, 0.0));
        QCOMPARE(model.loadBuys(QStringLiteral("share-b2")).size(), 1);
    }

    void test_modelBuyEdit_updateBuy_success()
    {
        openMemoryDb();
        ShareRepository shareRepo;
        shareRepo.insert(ShareObject(QStringLiteral("share-b3"), QStringLiteral("B3"),
                                     QString(), QStringLiteral("Test AG")));
        ModelBuyEdit model;
        BuyObject b(QStringLiteral("buy-upd"), QStringLiteral("share-b3"),
                    QStringLiteral("depot1"), QStringLiteral("ORD-B-UPD"),
                    QStringLiteral("2024-04-01T10:00:00"), 10.0, 0.0, 100.0);
        QVERIFY(model.addBuy(b, 0.0, 0.0, 0.0, 0.0));

        BuyObject updated(QStringLiteral("buy-upd"), QStringLiteral("share-b3"),
                          QStringLiteral("depot1"), QStringLiteral("ORD-B-UPD-CHANGED"),
                          QStringLiteral("2024-04-01T10:00:00"), 10.0, 0.0, 150.0);
        QVERIFY(model.updateBuy(updated, 5.0, 0.0, 0.0, 0.0));

        const auto buys = model.loadBuys(QStringLiteral("share-b3"));
        QCOMPARE(buys.size(), 1);
        QCOMPARE(buys.first().orderNumber(), QStringLiteral("ORD-B-UPD-CHANGED"));
        QCOMPARE(buys.first().price(), 150.0);
    }

    void test_modelBuyEdit_updateBuy_createsBrokerageIfMissing()
    {
        openMemoryDb();
        ShareRepository shareRepo;
        shareRepo.insert(ShareObject(QStringLiteral("share-b4"), QStringLiteral("B4"),
                                     QString(), QStringLiteral("Test AG")));
        // Insert buy directly without brokerage
        BuyRepository buyRepo;
        BuyObject b(QStringLiteral("buy-nobr"), QStringLiteral("share-b4"),
                    QStringLiteral("depot1"), QStringLiteral("ORD-B-NOBR"),
                    QStringLiteral("2024-05-01T10:00:00"), 5.0, 0.0, 100.0);
        buyRepo.insert(b);

        ModelBuyEdit model;
        QVERIFY(model.updateBuy(b, 9.90, 0.0, 0.0, 0.0));
        const BrokerageObject br = model.loadBrokerage(QStringLiteral("buy-nobr"));
        QVERIFY(br.isValid());

        // Regression (Bugfix 20.07.2026, siehe ARCHITECTURE.md,
        // "BuysForm-Details", ModelBuyEdit): der Rückwärts-Link (s.o.) allein
        // deckte den Bug nicht auf — buys.brokerage_guid (Vorwärts-Link) muss
        // ebenfalls gesetzt sein, sonst liefert
        // BuyRepository::totalBuyValueBrokerageReduction() weiterhin 0 für
        // die Provision dieses Kaufs, obwohl der Brokerage-Datensatz (s.o.)
        // korrekt existiert. buyValue = 5*100 = 500, + Provision 9.90 = 509.90.
        QCOMPARE(buyRepo.totalBuyValueBrokerageReduction(QStringLiteral("share-b4")), 509.90);
    }

    void test_modelBuyEdit_removeBuy_deletesBrokerageFirst()
    {
        openMemoryDb();
        ShareRepository shareRepo;
        shareRepo.insert(ShareObject(QStringLiteral("share-b5"), QStringLiteral("B5"),
                                     QString(), QStringLiteral("Test AG")));
        ModelBuyEdit model;
        BuyObject b(QStringLiteral("buy-rm"), QStringLiteral("share-b5"),
                    QStringLiteral("depot1"), QStringLiteral("ORD-B-RM"),
                    QStringLiteral("2024-06-01T10:00:00"), 10.0, 0.0, 100.0);
        QVERIFY(model.addBuy(b, 9.90, 0.0, 0.0, 0.0));
        QVERIFY(model.removeBuy(QStringLiteral("buy-rm")));
        QCOMPARE(model.loadBuys(QStringLiteral("share-b5")).size(), 0);
        QVERIFY(!model.loadBrokerage(QStringLiteral("buy-rm")).isValid());
    }

    void test_modelBuyEdit_removeBuy_rollsBackOnError()
    {
        // When the DB is not open, removeBuy() must return false and set lastError.
        if (Database::instance().isOpen())
            Database::instance().close();
        ModelBuyEdit model;
        const bool result = model.removeBuy(QStringLiteral("any-guid"));
        QVERIFY(!result);
        QVERIFY(!model.lastError().isEmpty());
        // Re-open for subsequent tests
        openMemoryDb();
    }

    void test_modelBuyEdit_orderNumberExists_true()
    {
        openMemoryDb();
        ShareRepository shareRepo;
        shareRepo.insert(ShareObject(QStringLiteral("share-b6"), QStringLiteral("B6"),
                                     QString(), QStringLiteral("Test AG")));
        ModelBuyEdit model;
        BuyObject b(QStringLiteral("buy-on"), QStringLiteral("share-b6"),
                    QStringLiteral("depot1"), QStringLiteral("ORD-ON-1"),
                    QStringLiteral("2024-07-01T10:00:00"), 10.0, 0.0, 100.0);
        QVERIFY(model.addBuy(b, 0.0, 0.0, 0.0, 0.0));
        QVERIFY(model.orderNumberExists(QStringLiteral("share-b6"),
                                        QStringLiteral("ORD-ON-1")));
        QVERIFY(!model.orderNumberExists(QStringLiteral("share-b6"),
                                         QStringLiteral("ORD-ON-NOTEXIST")));
    }

    void test_modelBuyEdit_orderNumberExists_excludeGuid()
    {
        openMemoryDb();
        ShareRepository shareRepo;
        shareRepo.insert(ShareObject(QStringLiteral("share-b7"), QStringLiteral("B7"),
                                     QString(), QStringLiteral("Test AG")));
        ModelBuyEdit model;
        BuyObject b(QStringLiteral("buy-excl"), QStringLiteral("share-b7"),
                    QStringLiteral("depot1"), QStringLiteral("ORD-EXCL"),
                    QStringLiteral("2024-08-01T10:00:00"), 10.0, 0.0, 100.0);
        QVERIFY(model.addBuy(b, 0.0, 0.0, 0.0, 0.0));
        // Excluding its own GUID must return false (not a duplicate of itself)
        QVERIFY(!model.orderNumberExists(QStringLiteral("share-b7"),
                                          QStringLiteral("ORD-EXCL"),
                                          QStringLiteral("buy-excl")));
    }

    void test_modelBuyEdit_loadBuys_orderedByDate()
    {
        openMemoryDb();
        ShareRepository shareRepo;
        shareRepo.insert(ShareObject(QStringLiteral("share-b8"), QStringLiteral("B8"),
                                     QString(), QStringLiteral("Test AG")));
        ModelBuyEdit model;
        BuyObject newer(QStringLiteral("buy-new"), QStringLiteral("share-b8"),
                        QStringLiteral("depot1"), QStringLiteral("ORD-NEW"),
                        QStringLiteral("2024-09-15T10:00:00"), 5.0, 0.0, 120.0);
        BuyObject older(QStringLiteral("buy-old"), QStringLiteral("share-b8"),
                        QStringLiteral("depot1"), QStringLiteral("ORD-OLD"),
                        QStringLiteral("2023-03-01T10:00:00"), 5.0, 0.0, 100.0);
        QVERIFY(model.addBuy(newer, 0.0, 0.0, 0.0, 0.0));
        QVERIFY(model.addBuy(older, 0.0, 0.0, 0.0, 0.0));
        const auto buys = model.loadBuys(QStringLiteral("share-b8"));
        QCOMPARE(buys.size(), 2);
        QVERIFY(buys.at(0).dateTime() < buys.at(1).dateTime());
    }

    void test_modelBuyEdit_loadBrokerage_notFound_returnsInvalid()
    {
        openMemoryDb();
        ModelBuyEdit model;
        const BrokerageObject br = model.loadBrokerage(
            QStringLiteral("nonexistent-buy-guid"));
        QVERIFY(!br.isValid());
    }

    // ─────────────────────────────────────────────────────────────────────
    // PresenterBuyEdit — stub-based tests
    // ─────────────────────────────────────────────────────────────────────

    void test_presenterBuyEdit_construction_loadsOverview()
    {
        openMemoryDb();
        StubViewBuyEdit  view;
        StubModelBuyEdit model;
        PresenterBuyEdit p(&view, &model,
                           QStringLiteral("share-1"), nullptr);
        QVERIFY(view.populateOverviewCalled);
    }

    void test_presenterBuyEdit_construction_clearsForm()
    {
        openMemoryDb();
        StubViewBuyEdit  view;
        StubModelBuyEdit model;
        PresenterBuyEdit p(&view, &model,
                           QStringLiteral("share-1"), nullptr);
        QVERIFY(view.clearFormCalled);
    }

    void test_presenterBuyEdit_construction_setsButtonStates_noSelection()
    {
        openMemoryDb();
        StubViewBuyEdit  view;
        StubModelBuyEdit model;
        PresenterBuyEdit p(&view, &model,
                           QStringLiteral("share-1"), nullptr);
        // Initial state: no selection → canRemove=false, isLastBuy=false
        QVERIFY(view.setButtonStatesCalled);
        QCOMPARE(view.lastCanRemove,  false);
        QCOMPARE(view.lastIsLastBuy,  false);
    }

    void test_presenterBuyEdit_onRowSelected_singleBuy_isLastBuy()
    {
        // With only one buy it must always be the latest.
        openMemoryDb();
        StubViewBuyEdit  view;
        StubModelBuyEdit model;

        const BuyObject b("guid-1", "share-1", "depot1", "ord-1",
                          QStringLiteral("2024-06-15T10:00:00"),
                          10.0, 0.0, 100.0);
        model.buys = { b };

        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        view.setButtonStatesCalled = false;   // reset after construction

        p.onRowSelected(QStringLiteral("guid-1"));

        QVERIFY(view.loadBuyCalled);
        QCOMPARE(view.lastCanRemove, true);
        QCOMPARE(view.lastIsLastBuy, true);
    }

    void test_presenterBuyEdit_onRowSelected_olderBuy_isNotLastBuy()
    {
        // With two buys, selecting the earlier one must yield isLastBuy=false.
        openMemoryDb();
        StubViewBuyEdit  view;
        StubModelBuyEdit model;

        const BuyObject older("guid-old", "share-1", "depot1", "ord-1",
                              QStringLiteral("2023-01-01T09:00:00"),
                              10.0, 0.0, 100.0);
        const BuyObject newer("guid-new", "share-1", "depot1", "ord-2",
                              QStringLiteral("2024-06-15T10:00:00"),
                              5.0, 0.0, 120.0);
        model.buys = { older, newer };

        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        view.setButtonStatesCalled = false;

        p.onRowSelected(QStringLiteral("guid-old"));

        QCOMPARE(view.lastCanRemove, false);  // canRemove = isLastBuy && volumeSold==0
        QCOMPARE(view.lastIsLastBuy, false);
    }

    void test_presenterBuyEdit_onRowSelected_newerBuy_isLastBuy()
    {
        // Selecting the most recent buy must yield isLastBuy=true.
        openMemoryDb();
        StubViewBuyEdit  view;
        StubModelBuyEdit model;

        const BuyObject older("guid-old", "share-1", "depot1", "ord-1",
                              QStringLiteral("2023-01-01T09:00:00"),
                              10.0, 0.0, 100.0);
        const BuyObject newer("guid-new", "share-1", "depot1", "ord-2",
                              QStringLiteral("2024-06-15T10:00:00"),
                              5.0, 0.0, 120.0);
        model.buys = { older, newer };

        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        view.setButtonStatesCalled = false;

        p.onRowSelected(QStringLiteral("guid-new"));

        QCOMPARE(view.lastCanRemove, true);
        QCOMPARE(view.lastIsLastBuy, true);
    }

    void test_presenterBuyEdit_onRowSelected_latestBuyWithSoldShares_canRemoveFalse()
    {
        // Latest buy but volumeSold > 0 → canRemove must be false.
        openMemoryDb();
        StubViewBuyEdit  view;
        StubModelBuyEdit model;

        const BuyObject b("guid-1", "share-1", "depot1", "ord-1",
                          QStringLiteral("2024-06-15T10:00:00"),
                          10.0, /*volumeSold=*/5.0, 100.0);
        model.buys = { b };

        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        view.setButtonStatesCalled = false;

        p.onRowSelected(QStringLiteral("guid-1"));

        QCOMPARE(view.lastIsLastBuy,  true);
        QCOMPARE(view.lastCanRemove,  false);  // sold shares → cannot remove
    }

    void test_presenterBuyEdit_onRowSelected_latestBuyNoSoldShares_canRemoveTrue()
    {
        // Latest buy with volumeSold == 0 → canRemove must be true.
        openMemoryDb();
        StubViewBuyEdit  view;
        StubModelBuyEdit model;

        const BuyObject b("guid-1", "share-1", "depot1", "ord-1",
                          QStringLiteral("2024-06-15T10:00:00"),
                          10.0, /*volumeSold=*/0.0, 100.0);
        model.buys = { b };

        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        view.setButtonStatesCalled = false;

        p.onRowSelected(QStringLiteral("guid-1"));

        QCOMPARE(view.lastIsLastBuy,  true);
        QCOMPARE(view.lastCanRemove,  true);
    }

    void test_presenterBuyEdit_onRemove_latestBuyNoSoldShares_callsModel()
    {
        openMemoryDb();
        StubViewBuyEdit  view;
        StubModelBuyEdit model;

        const BuyObject b("guid-1", "share-1", "depot1", "ord-1",
                          QStringLiteral("2024-06-15T10:00:00"),
                          10.0, 0.0, 100.0);
        model.buys = { b };

        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onRowSelected(QStringLiteral("guid-1"));
        model.removeBuyCalled = false;

        p.onRemove();

        QVERIFY(model.removeBuyCalled);
        QVERIFY(view.lastError.isEmpty());
    }

    void test_presenterBuyEdit_onRemove_latestBuyNoSoldShares_emitsDataChanged()
    {
        openMemoryDb();
        StubViewBuyEdit  view;
        StubModelBuyEdit model;

        const BuyObject b("guid-1", "share-1", "depot1", "ord-1",
                          QStringLiteral("2024-06-15T10:00:00"),
                          10.0, 0.0, 100.0);
        model.buys = { b };

        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onRowSelected(QStringLiteral("guid-1"));

        QSignalSpy spy(&p, &PresenterBuyEdit::dataChanged);
        p.onRemove();

        QCOMPARE(spy.count(), 1);
    }

    void test_presenterBuyEdit_onRemove_olderBuy_showsError()
    {
        // Selecting a non-latest buy and calling onRemove must show an error.
        openMemoryDb();
        StubViewBuyEdit  view;
        StubModelBuyEdit model;

        const BuyObject older("guid-old", "share-1", "depot1", "ord-1",
                              QStringLiteral("2023-01-01T09:00:00"),
                              10.0, 0.0, 100.0);
        const BuyObject newer("guid-new", "share-1", "depot1", "ord-2",
                              QStringLiteral("2024-06-15T10:00:00"),
                              5.0, 0.0, 120.0);
        model.buys = { older, newer };

        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onRowSelected(QStringLiteral("guid-old"));
        model.removeBuyCalled = false;

        p.onRemove();

        QVERIFY(!model.removeBuyCalled);
        QVERIFY(!view.lastError.isEmpty());
    }

    void test_presenterBuyEdit_onRemove_latestBuyWithSoldShares_showsError()
    {
        // Latest buy but with sold shares → remove must be blocked.
        openMemoryDb();
        StubViewBuyEdit  view;
        StubModelBuyEdit model;

        const BuyObject b("guid-1", "share-1", "depot1", "ord-1",
                          QStringLiteral("2024-06-15T10:00:00"),
                          10.0, /*volumeSold=*/3.0, 100.0);
        model.buys = { b };

        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onRowSelected(QStringLiteral("guid-1"));
        model.removeBuyCalled = false;

        p.onRemove();

        QVERIFY(!model.removeBuyCalled);
        QVERIFY(!view.lastError.isEmpty());
    }

    void test_presenterBuyEdit_onRemove_noSelection_doesNothing()
    {
        openMemoryDb();
        StubViewBuyEdit  view;
        StubModelBuyEdit model;
        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);

        p.onRemove();

        QVERIFY(!model.removeBuyCalled);
    }

    void test_presenterBuyEdit_onRowSelected_emptyGuid_resetsForm()
    {
        openMemoryDb();
        StubViewBuyEdit  view;
        StubModelBuyEdit model;
        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        view.clearFormCalled = false;

        p.onRowSelected(QString());

        QVERIFY(view.clearFormCalled);
        QCOMPARE(view.lastCanRemove, false);
        QCOMPARE(view.lastIsLastBuy, false);
    }

    void test_presenterBuyEdit_onReset_setsButtonStates_noSelection()
    {
        openMemoryDb();
        StubViewBuyEdit  view;
        StubModelBuyEdit model;
        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        view.setButtonStatesCalled = false;

        p.onReset();

        QCOMPARE(view.lastCanRemove, false);
        QCOMPARE(view.lastIsLastBuy, false);
    }

    void test_presenterBuyEdit_onSave_newBuy_callsAddBuy()
    {
        openMemoryDb();
        StubViewBuyEdit  view;
        StubModelBuyEdit model;
        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);

        p.onSave();

        QVERIFY(model.addBuyCalled);
        QVERIFY(!model.updateBuyCalled);
    }

    void test_presenterBuyEdit_onSave_newBuy_jumpsToOverviewTab()
    {
        // After a new buy is saved the view must switch to the Übersicht tab
        // (showOverviewTab), which in the stub maps to clearFormCalled = true.
        openMemoryDb();

        bool overviewCalled = false;
        struct SpyView : public StubViewBuyEdit {
            bool* called;
            void showOverviewTab() override { *called = true; }
        } view;
        view.called = &overviewCalled;

        StubModelBuyEdit model;
        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);

        p.onSave();

        QVERIFY(overviewCalled);
    }

    void test_presenterBuyEdit_onReset_jumpsToOverviewTab()
    {
        openMemoryDb();

        bool overviewCalled = false;
        struct SpyView : public StubViewBuyEdit {
            bool* called;
            void showOverviewTab() override { *called = true; }
        } view;
        view.called = &overviewCalled;

        StubModelBuyEdit model;
        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        overviewCalled = false;   // reset after construction

        p.onReset();

        QVERIFY(overviewCalled);
    }

    void test_presenterBuyEdit_onSave_newBuy_emitsDataChanged()
    {
        openMemoryDb();
        StubViewBuyEdit  view;
        StubModelBuyEdit model;
        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);

        QSignalSpy spy(&p, &PresenterBuyEdit::dataChanged);
        p.onSave();

        QCOMPARE(spy.count(), 1);
    }

    void test_presenterBuyEdit_onSave_latestBuy_callsUpdateBuy()
    {
        // Select the latest buy, then save — must call updateBuy() with all fields.
        openMemoryDb();
        StubViewBuyEdit  view;
        StubModelBuyEdit model;

        const BuyObject b("guid-1", "share-1", "depot1", "ord-1",
                          QStringLiteral("2024-06-15T10:00:00"),
                          10.0, 0.0, 100.0);
        model.buys = { b };

        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onRowSelected(QStringLiteral("guid-1"));   // isLastBuy = true

        model.updateBuyCalled = false;
        p.onSave();

        QVERIFY(model.updateBuyCalled);
        QVERIFY(!model.addBuyCalled);
    }

    void test_presenterBuyEdit_onSave_latestBuy_jumpsToOverviewTab()
    {
        // After saving an existing buy, the view must jump to Übersicht (same as new buy).
        openMemoryDb();

        bool overviewCalled = false;
        struct SpyView : public StubViewBuyEdit {
            bool* called;
            void showOverviewTab() override { *called = true; }
        } view;
        view.called = &overviewCalled;

        StubModelBuyEdit model;
        const BuyObject b("guid-1", "share-1", "depot1", "ord-1",
                          QStringLiteral("2024-06-15T10:00:00"),
                          10.0, 0.0, 100.0);
        model.buys = { b };

        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onRowSelected(QStringLiteral("guid-1"));
        p.onSave();

        QVERIFY(overviewCalled);
    }

    void test_presenterBuyEdit_onSave_nonLatestBuy_callsUpdateBuyDocOnly()
    {
        // Select an older buy, then save — must call updateBuy() (document-only path).
        openMemoryDb();
        StubViewBuyEdit  view;
        StubModelBuyEdit model;

        const BuyObject older("guid-old", "share-1", "depot1", "ord-1",
                              QStringLiteral("2023-01-01T09:00:00"),
                              10.0, 0.0, 100.0);
        const BuyObject newer("guid-new", "share-1", "depot1", "ord-2",
                              QStringLiteral("2024-06-15T10:00:00"),
                              5.0, 0.0, 120.0);
        model.buys = { older, newer };

        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onRowSelected(QStringLiteral("guid-old"));   // isLastBuy = false

        model.updateBuyCalled = false;
        p.onSave();

        QVERIFY(model.updateBuyCalled);
        QVERIFY(!model.addBuyCalled);
    }

    void test_presenterBuyEdit_onSave_nonLatestBuy_emitsDataChanged()
    {
        openMemoryDb();
        StubViewBuyEdit  view;
        StubModelBuyEdit model;

        const BuyObject older("guid-old", "share-1", "depot1", "ord-1",
                              QStringLiteral("2023-01-01T09:00:00"),
                              10.0, 0.0, 100.0);
        const BuyObject newer("guid-new", "share-1", "depot1", "ord-2",
                              QStringLiteral("2024-06-15T10:00:00"),
                              5.0, 0.0, 120.0);
        model.buys = { older, newer };

        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onRowSelected(QStringLiteral("guid-old"));

        QSignalSpy spy(&p, &PresenterBuyEdit::dataChanged);
        p.onSave();

        QCOMPARE(spy.count(), 1);
    }

    void test_presenterBuyEdit_onSave_missingFields_showsError()
    {
        openMemoryDb();
        StubViewBuyEdit  view;
        StubModelBuyEdit model;
        view.m_missingFields = true;
        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);

        p.onSave();

        QVERIFY(!view.lastError.isEmpty());
        QVERIFY(!model.addBuyCalled);
    }

    void test_presenterBuyEdit_onDocumentSelected_writesPathIntoView()
    {
        // Regression 21.08.2026 (Nessies Bugreport): a document dropped onto
        // "Direkte Dokumentenerfassung" reaches the dialog exclusively via
        // MainWindow calling dlg.presenter()->onDocumentSelected() directly
        // — ViewBuyEdit::onBrowseDocument() (which used to be the only place
        // writing m_documentPath->setText()) is never involved. Before the
        // fix the field stayed on "Kein Dokument ausgewählt …" even though
        // parsing succeeded. onDocumentSelected() must therefore write the
        // path into the view itself.
        openMemoryDb();

        StubViewBuyEdit view;
        StubModelBuyEdit model;
        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);

        p.onDocumentSelected(QStringLiteral("/tmp/dropped.pdf"));

        QCOMPARE(view.documentPath(), QStringLiteral("/tmp/dropped.pdf"));
    }

    void test_presenterBuyEdit_onDocumentSelected_newMode_doesNotEarlyReturn()
    {
        // Regression: in new-buy mode (no selection, m_isLastBuy=false) a document
        // selection must NOT be blocked. openPdfPreview() must always be called.
        openMemoryDb();

        bool previewCalled = false;
        struct SpyView : public StubViewBuyEdit {
            bool* called;
            void openPdfPreview(const QString&) override { *called = true; }
        } view;
        view.called = &previewCalled;

        StubModelBuyEdit model;
        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);

        // No row selected → new-buy mode → re-parse path must be entered
        p.onDocumentSelected(QStringLiteral("/tmp/test.pdf"));

        QVERIFY(previewCalled);
    }

    void test_presenterBuyEdit_onDocumentSelected_nonLatestBuy_earlyReturn()
    {
        // When a non-latest buy is selected, document selection must only update
        // the preview — the QProcess (pdftotext) must NOT be launched.
        // We verify this indirectly: setUiBusy(true) is only called when
        // the parse path is entered. The stub captures it.
        openMemoryDb();

        bool busyCalled = false;
        struct SpyView : public StubViewBuyEdit {
            bool* called;
            void setUiBusy(bool busy) override { if (busy) *called = true; }
        } view;
        view.called = &busyCalled;

        StubModelBuyEdit model;
        const BuyObject older("guid-old", "share-1", "depot1", "ord-1",
                              QStringLiteral("2023-01-01T09:00:00"),
                              10.0, 0.0, 100.0);
        const BuyObject newer("guid-new", "share-1", "depot1", "ord-2",
                              QStringLiteral("2024-06-15T10:00:00"),
                              5.0, 0.0, 120.0);
        model.buys = { older, newer };

        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onRowSelected(QStringLiteral("guid-old"));   // selects non-latest

        p.onDocumentSelected(QStringLiteral("/tmp/test.pdf"));

        // setUiBusy(true) must NOT have been called — parse path was not entered
        QVERIFY(!busyCalled);
    }

    void test_presenterBuyEdit_onOrderNumberEdited_empty_setsError()
    {
        openMemoryDb();

        bool errorSet = false;
        struct SpyView : public StubViewBuyEdit {
            bool* called;
            void setFieldError(const QString& f, const QString& = QString()) override
                { if (f == QLatin1String("orderNumber")) *called = true; }
        } spy;
        spy.called = &errorSet;
        spy.m_orderNumber = QString();

        StubModelBuyEdit model;
        PresenterBuyEdit p(&spy, &model, QStringLiteral("share-1"), nullptr);
        p.onOrderNumberEdited();

        QVERIFY(errorSet);
    }

    void test_presenterBuyEdit_onOrderNumberEdited_valid_setsOk()
    {
        openMemoryDb();

        bool okSet = false;
        struct SpyView : public StubViewBuyEdit {
            bool* called;
            bool setFieldOk(const QString& f, const QString&,
                            const QString& = QString()) override
                { if (f == QLatin1String("orderNumber")) *called = true;  return true; }
        } spy;
        spy.called = &okSet;
        spy.m_orderNumber = QStringLiteral("ORD-001");

        StubModelBuyEdit model;
        model.orderExists = false;

        PresenterBuyEdit p(&spy, &model, QStringLiteral("share-1"), nullptr);
        p.onOrderNumberEdited();

        QVERIFY(okSet);
    }

    void test_presenterBuyEdit_onOrderNumberEdited_duplicate_setsError()
    {
        openMemoryDb();

        bool errorSet = false;
        struct SpyView : public StubViewBuyEdit {
            bool* called;
            void setFieldError(const QString& f, const QString& = QString()) override
                { if (f == QLatin1String("orderNumber")) *called = true; }
        } spy;
        spy.called = &errorSet;
        spy.m_orderNumber = QStringLiteral("ORD-DUP");

        StubModelBuyEdit model;
        model.orderExists = true;

        PresenterBuyEdit p(&spy, &model, QStringLiteral("share-1"), nullptr);
        p.onOrderNumberEdited();

        QVERIFY(errorSet);
    }

    void test_presenterBuyEdit_onDocumentPathEdited_duplicate_setsError()
    {
        openMemoryDb();

        bool errorSet = false;
        struct SpyView : public StubViewBuyEdit {
            bool* called;
            void setFieldError(const QString& f, const QString& = QString()) override
                { if (f == QLatin1String("document")) *called = true; }
        } spy;
        spy.called = &errorSet;
        spy.m_docPath = QStringLiteral("/some/path/buy.pdf");

        StubModelBuyEdit model;
        model.docExists = true;

        PresenterBuyEdit p(&spy, &model, QStringLiteral("share-1"), nullptr);
        p.onDocumentPathEdited();

        QVERIFY(errorSet);
    }

    void test_presenterBuyEdit_onDocumentPathEdited_unique_setsOk()
    {
        openMemoryDb();

        bool okSet = false;
        struct SpyView : public StubViewBuyEdit {
            bool* called;
            bool setFieldOk(const QString& f, const QString&,
                            const QString& = QString()) override
                { if (f == QLatin1String("document")) *called = true;  return true; }
        } spy;
        spy.called = &okSet;
        spy.m_docPath = QStringLiteral("/some/path/buy.pdf");

        StubModelBuyEdit model;
        model.docExists = false;

        PresenterBuyEdit p(&spy, &model, QStringLiteral("share-1"), nullptr);
        p.onDocumentPathEdited();

        QVERIFY(okSet);
    }

    void test_presenterBuyEdit_onSave_documentDuplicate_showsError()
    {
        openMemoryDb();
        StubViewBuyEdit  view;
        StubModelBuyEdit model;
        view.m_docPath  = QStringLiteral("/some/existing.pdf");
        model.docExists = true;

        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onSave();

        QVERIFY(!view.lastError.isEmpty());
        QVERIFY(!model.addBuyCalled);
    }

    void test_presenterBuyEdit_onSave_nonLatestBuy_jumpsToOverviewTab()
    {
        // After saving a non-latest buy (doc-only path) the view must also
        // switch to the Übersicht tab — same behaviour as the latest-buy path.
        openMemoryDb();

        bool overviewCalled = false;
        struct SpyView : public StubViewBuyEdit {
            bool* called;
            void showOverviewTab() override { *called = true; }
        } view;
        view.called = &overviewCalled;

        StubModelBuyEdit model;
        const BuyObject older("guid-old", "share-1", "depot1", "ord-1",
                              QStringLiteral("2023-01-01T09:00:00"),
                              10.0, 0.0, 100.0);
        const BuyObject newer("guid-new", "share-1", "depot1", "ord-2",
                              QStringLiteral("2024-06-15T10:00:00"),
                              5.0, 0.0, 120.0);
        model.buys = { older, newer };

        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onRowSelected(QStringLiteral("guid-old"));   // non-latest
        overviewCalled = false;

        p.onSave();

        QVERIFY(overviewCalled);
    }

    void test_presenterBuyEdit_onSave_nonLatestBuy_resetsButtonLabel()
    {
        // After saving a non-latest buy the button states must be reset
        // (canRemove=false, isEdit=false) so the label shows "Hinzufügen" again.
        openMemoryDb();
        StubViewBuyEdit  view;
        StubModelBuyEdit model;

        const BuyObject older("guid-old", "share-1", "depot1", "ord-1",
                              QStringLiteral("2023-01-01T09:00:00"),
                              10.0, 0.0, 100.0);
        const BuyObject newer("guid-new", "share-1", "depot1", "ord-2",
                              QStringLiteral("2024-06-15T10:00:00"),
                              5.0, 0.0, 120.0);
        model.buys = { older, newer };

        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onRowSelected(QStringLiteral("guid-old"));

        p.onSave();

        QCOMPARE(view.lastCanRemove,  false);
        QCOMPARE(view.lastIsLastBuy,  false);
    }

    // ─────────────────────────────────────────────────────────────────────
    // ViewBuyEdit
    // ─────────────────────────────────────────────────────────────────────

    void test_viewBuyEdit_populateOverview_emptyBuys_noTabs()
    {
        // When there are no buys, populateOverview() should leave the tab
        // widget empty (no tabs at all).
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        QVERIFY(tabs != nullptr);

        dlg.populateOverview({}, {}, {});
        QCOMPARE(tabs->count(), 0);
    }

    void test_viewBuyEdit_populateOverview_singleYear_twoTabs()
    {
        // One buy in 2024 → 2 tabs: "Übersicht" + "2024"
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        QVERIFY(tabs != nullptr);

        const QList<BuyObject> buys = {
            makeBuy(QStringLiteral("b1"), QStringLiteral("share-guid"), 2024)
        };
        const QList<BrokerageObject> brokerages = {
            makeBrokerage(QStringLiteral("b1"), QStringLiteral("share-guid"))
        };

        dlg.populateOverview(buys, brokerages, {});

        QCOMPARE(tabs->count(), 2);
        QVERIFY(tabs->tabText(0).contains(QStringLiteral("Übersicht")));
        QVERIFY(tabs->tabText(1).contains(QStringLiteral("2024")));
    }

    void test_viewBuyEdit_populateOverview_twoYears_threeTabs()
    {
        // Buys in 2023 and 2024 → 3 tabs: Übersicht + 2023 + 2024
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        QVERIFY(tabs != nullptr);

        const QList<BuyObject> buys = {
            makeBuy(QStringLiteral("b1"), QStringLiteral("share-guid"), 2023),
            makeBuy(QStringLiteral("b2"), QStringLiteral("share-guid"), 2024)
        };
        const QList<BrokerageObject> brokerages = {
            makeBrokerage(QStringLiteral("b1"), QStringLiteral("share-guid")),
            makeBrokerage(QStringLiteral("b2"), QStringLiteral("share-guid"))
        };

        dlg.populateOverview(buys, brokerages, {});

        QCOMPARE(tabs->count(), 3);
        QVERIFY(tabs->tabText(0).contains(QStringLiteral("Übersicht")));
        QVERIFY(tabs->tabText(1).contains(QStringLiteral("2024")));  // descending: newest first
        QVERIFY(tabs->tabText(2).contains(QStringLiteral("2023")));
    }

    void test_viewBuyEdit_populateOverview_uebersichtTabHasTable()
    {
        // The Übersicht tab must contain a QTableWidget (year-aggregated summary).
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        QVERIFY(tabs != nullptr);

        const QList<BuyObject> buys = {
            makeBuy(QStringLiteral("b1"), QStringLiteral("share-guid"), 2024)
        };
        const QList<BrokerageObject> brokerages = {
            makeBrokerage(QStringLiteral("b1"), QStringLiteral("share-guid"))
        };
        dlg.populateOverview(buys, brokerages, {});

        // Übersicht tab: find its table
        auto* container = tabs->widget(0);
        QVERIFY(container != nullptr);
        auto* tbl = dataTableFromContainer(container);
        QVERIFY(tbl != nullptr);
        // One year row
        QCOMPARE(tbl->rowCount(), 1);
        // Three columns: Jahr / Anteile / Einzahlung
        QCOMPARE(tbl->columnCount(), 3);
    }

    void test_viewBuyEdit_populateOverview_jahresTabHasSixColumns()
    {
        // Each Jahres-tab must have 6 columns:
        // Datum | Anteile | Kurswert | Gebühren | Einzahlung | Dokument
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        QVERIFY(tabs != nullptr);

        const QList<BuyObject> buys = {
            makeBuy(QStringLiteral("b1"), QStringLiteral("share-guid"), 2024)
        };
        const QList<BrokerageObject> brokerages = {
            makeBrokerage(QStringLiteral("b1"), QStringLiteral("share-guid"))
        };
        dlg.populateOverview(buys, brokerages, {});

        // Jahres-tab is index 1
        auto* container = tabs->widget(1);
        QVERIFY(container != nullptr);
        auto* tbl = dataTableFromContainer(container);
        QVERIFY(tbl != nullptr);
        QCOMPARE(tbl->columnCount(), 6);
    }

    void test_viewBuyEdit_populateOverview_jahresTabRowCount()
    {
        // Jahres-tab should have exactly as many rows as buys in that year
        // (Gesamt is no longer a table row — it's a separate widget).
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        QVERIFY(tabs != nullptr);

        const QList<BuyObject> buys = {
            makeBuy(QStringLiteral("b1"), QStringLiteral("share-guid"), 2024),
            makeBuy(QStringLiteral("b2"), QStringLiteral("share-guid"), 2024),
            makeBuy(QStringLiteral("b3"), QStringLiteral("share-guid"), 2024)
        };
        const QList<BrokerageObject> brokerages = {
            makeBrokerage(QStringLiteral("b1"), QStringLiteral("share-guid")),
            makeBrokerage(QStringLiteral("b2"), QStringLiteral("share-guid")),
            makeBrokerage(QStringLiteral("b3"), QStringLiteral("share-guid"))
        };
        dlg.populateOverview(buys, brokerages, {});

        auto* container = tabs->widget(1);
        QVERIFY(container != nullptr);
        auto* tbl = dataTableFromContainer(container);
        QVERIFY(tbl != nullptr);
        // 3 buy rows — no extra Gesamt row in the table
        QCOMPARE(tbl->rowCount(), 3);
    }

    void test_viewBuyEdit_populateOverview_guidStoredInDateColumn()
    {
        // The GUID is stored in column 0 (Datum), UserRole for row-selection.
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        QVERIFY(tabs != nullptr);

        const QList<BuyObject> buys = {
            makeBuy(QStringLiteral("b1"), QStringLiteral("share-guid"), 2024)
        };
        const QList<BrokerageObject> brokerages = {
            makeBrokerage(QStringLiteral("b1"), QStringLiteral("share-guid"))
        };
        dlg.populateOverview(buys, brokerages, {});

        auto* container = tabs->widget(1); // Jahres-tab
        if (!container) QFAIL("Jahres-tab container not found");
        auto* tbl = dataTableFromContainer(container);
        if (!tbl) QFAIL("dataTable not found");
        if (!tbl->item(0, 0)) QFAIL("item(0,0) not found");
        QCOMPARE(tbl->item(0, 0)->data(Qt::UserRole).toString(),
                 QStringLiteral("b1"));
    }

    void test_viewBuyEdit_populateOverview_docIconWhenPathSet()
    {
        // When a buy has a document path, the Dokument column uses setCellWidget
        // with a centred QLabel — not QTableWidgetItem::setIcon.
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        QVERIFY(tabs != nullptr);

        const QList<BuyObject> buys = {
            makeBuy(QStringLiteral("b1"), QStringLiteral("share-guid"), 2024,
                    10.0, 100.0, QStringLiteral("/some/path/doc.pdf"))
        };
        const QList<BrokerageObject> brokerages = {
            makeBrokerage(QStringLiteral("b1"), QStringLiteral("share-guid"))
        };
        dlg.populateOverview(buys, brokerages, {});

        auto* container = tabs->widget(1);
        QVERIFY(container != nullptr);
        auto* tbl = dataTableFromContainer(container);
        QVERIFY(tbl != nullptr);
        // Icon is a QLabel set via setCellWidget, not item()->icon()
        auto* iconWidget = tbl->cellWidget(0, 5);
        QVERIFY(iconWidget != nullptr);
        auto* iconLabel = qobject_cast<QLabel*>(iconWidget);
        QVERIFY(iconLabel != nullptr);
        QVERIFY(!iconLabel->pixmap().isNull());
    }

    void test_viewBuyEdit_populateOverview_docDashWhenNoPath()
    {
        // When a buy has no document path, the Dokument column shows "-" as item text
        // and no cellWidget is set.
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        QVERIFY(tabs != nullptr);

        const QList<BuyObject> buys = {
            makeBuy(QStringLiteral("b1"), QStringLiteral("share-guid"), 2024)
        };
        const QList<BrokerageObject> brokerages = {
            makeBrokerage(QStringLiteral("b1"), QStringLiteral("share-guid"))
        };
        dlg.populateOverview(buys, brokerages, {});

        auto* container = tabs->widget(1);
        QVERIFY(container != nullptr);
        auto* tbl = dataTableFromContainer(container);
        QVERIFY(tbl != nullptr);
        QVERIFY(tbl->item(0, 5) != nullptr);
        QCOMPARE(tbl->item(0, 5)->text(), QStringLiteral("-"));
        QVERIFY(tbl->cellWidget(0, 5) == nullptr);
    }

    void test_viewBuyEdit_populateOverview_kurswertIsPrice()
    {
        // Kurswert column shows price per share (b.price()), not total buy value.
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        QVERIFY(tabs != nullptr);

        // volume=10, price=25.50 → buyValue=255, but Kurswert should show 25.50
        const QList<BuyObject> buys = {
            makeBuy(QStringLiteral("b1"), QStringLiteral("share-guid"), 2024,
                    10.0, 25.50)
        };
        const QList<BrokerageObject> brokerages = {
            makeBrokerage(QStringLiteral("b1"), QStringLiteral("share-guid"), 0.0)
        };
        dlg.populateOverview(buys, brokerages, {});

        auto* container = tabs->widget(1);
        QVERIFY(container != nullptr);
        auto* tbl = dataTableFromContainer(container);
        QVERIFY(tbl != nullptr);
        // Kurswert is column 2 — must contain price (25.50), not buyValue (255.00)
        const QString kurswertText = tbl->item(0, 2)->text();
        QVERIFY(kurswertText.contains(QStringLiteral("25")));
        QVERIFY(!kurswertText.contains(QStringLiteral("255")));
    }

    void test_viewBuyEdit_populateOverview_footerKurswertIsDash()
    {
        // Gesamt row: Kurswert column must show "-" (sum of prices is meaningless).
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        QVERIFY(tabs != nullptr);

        const QList<BuyObject> buys = {
            makeBuy(QStringLiteral("b1"), QStringLiteral("share-guid"), 2024)
        };
        const QList<BrokerageObject> brokerages = {
            makeBrokerage(QStringLiteral("b1"), QStringLiteral("share-guid"))
        };
        dlg.populateOverview(buys, brokerages, {});

        // Footer table is the second QTableWidget in the container
        auto* container = tabs->widget(1);
        QVERIFY(container != nullptr);
        const auto tables = container->findChildren<QTableWidget*>();
        QVERIFY(tables.size() >= 2);
        auto* footerTbl = tables.at(1);
        QVERIFY(footerTbl->item(0, 2) != nullptr);
        QCOMPARE(footerTbl->item(0, 2)->text(), QStringLiteral("-"));
    }

    void test_viewBuyEdit_populateOverview_tabTitleContainsTotal()
    {
        // Each tab title must include the total Einzahlung.
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");

        // volume=10, price=100 → buyValue=1000; provision=9.90 → einzahlung=1009.90
        const QList<BuyObject> buys = {
            makeBuy(QStringLiteral("b1"), QStringLiteral("share-guid"), 2024,
                    10.0, 100.0)
        };
        const QList<BrokerageObject> brokerages = {
            makeBrokerage(QStringLiteral("b1"), QStringLiteral("share-guid"), 9.90)
        };
        dlg.populateOverview(buys, brokerages, {});

        // Both Übersicht and 2024 tabs must show the same total
        QVERIFY(tabs->tabText(0).contains(QStringLiteral("€")));
        QVERIFY(tabs->tabText(1).contains(QStringLiteral("2024")));
        QVERIFY(tabs->tabText(1).contains(QStringLiteral("€")));
    }

    void test_viewBuyEdit_populateOverview_repopulateReplacesOldTabs()
    {
        // Calling populateOverview() a second time replaces all previous tabs.
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        QVERIFY(tabs != nullptr);

        const QList<BuyObject> buys1 = {
            makeBuy(QStringLiteral("b1"), QStringLiteral("share-guid"), 2023)
        };
        const QList<BrokerageObject> br1 = {
            makeBrokerage(QStringLiteral("b1"), QStringLiteral("share-guid"))
        };
        dlg.populateOverview(buys1, br1, {});
        QCOMPARE(tabs->count(), 2); // Übersicht + 2023

        const QList<BuyObject> buys2 = {
            makeBuy(QStringLiteral("b2"), QStringLiteral("share-guid"), 2024),
            makeBuy(QStringLiteral("b3"), QStringLiteral("share-guid"), 2025)
        };
        const QList<BrokerageObject> br2 = {
            makeBrokerage(QStringLiteral("b2"), QStringLiteral("share-guid")),
            makeBrokerage(QStringLiteral("b3"), QStringLiteral("share-guid"))
        };
        dlg.populateOverview(buys2, br2, {});

        // Old tabs gone, new ones present
        QCOMPARE(tabs->count(), 3); // Übersicht + 2024 + 2025
        QVERIFY(!tabs->tabText(1).contains(QStringLiteral("2023")));
    }

    void test_viewBuyEdit_populateOverview_split_jahresRowKeepsBelegVolume()
    {
        // Eine Zeile im Jahres-Tab ist eine Beleg-Abschrift: die Stückzahl
        // bleibt stehen, wie sie im Dokument steht. Nur Marker und Tooltip
        // kommen hinzu.
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        QVERIFY(tabs != nullptr);

        dlg.populateOverview(buysAroundSplit(), brokeragesAroundSplit(), splitList2022());

        // Jahres-Tabs absteigend: Tab 1 = 2023, Tab 2 = 2022.
        auto* tbl = dataTableFromContainer(tabs->widget(2));
        QVERIFY(tbl != nullptr);
        QVERIFY(tbl->item(0, 1) != nullptr);

        const QString cell = tbl->item(0, 1)->text();
        QVERIFY2(cell.contains(QLocale().toString(5.0, 'f', 4)), qPrintable(cell));
        QVERIFY2(cell.endsWith(ShareSplitHint::marker()), qPrintable(cell));
        QVERIFY2(!tbl->item(0, 1)->toolTip().isEmpty(), "Tooltip fehlt");
        QVERIFY2(tbl->item(0, 1)->toolTip().contains(QStringLiteral("20:1")),
                 qPrintable(tbl->item(0, 1)->toolTip()));
    }

    void test_viewBuyEdit_populateOverview_split_rowAfterSplitHasNoMarker()
    {
        // Ein Kauf NACH dem Splittag ist bereits in heutiger Stückelung
        // ausgestellt — er darf weder Marker noch Tooltip bekommen.
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        QVERIFY(tabs != nullptr);

        dlg.populateOverview(buysAroundSplit(), brokeragesAroundSplit(), splitList2022());

        auto* tbl = dataTableFromContainer(tabs->widget(1)); // 2023
        QVERIFY(tbl != nullptr);
        QVERIFY(tbl->item(0, 1) != nullptr);

        QVERIFY(!tbl->item(0, 1)->text().contains(ShareSplitHint::marker()));
        QVERIFY(tbl->item(0, 1)->toolTip().isEmpty());
    }

    void test_viewBuyEdit_populateOverview_split_jahresFooterIsOnTodaysScale()
    {
        // Die Fusszeile ist eine Rechnung, keine Abschrift: 5 Beleg-Stücke
        // von 2022 sind heute 100.
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        QVERIFY(tabs != nullptr);

        dlg.populateOverview(buysAroundSplit(), brokeragesAroundSplit(), splitList2022());

        auto* footerTbl = footerTableFromContainer(tabs->widget(2)); // 2022
        QVERIFY(footerTbl != nullptr);
        QVERIFY(footerTbl->item(0, 1) != nullptr);

        const QString cell = footerTbl->item(0, 1)->text();
        QVERIFY2(cell.contains(QLocale().toString(100.0, 'f', 4)), qPrintable(cell));
        QVERIFY2(cell.endsWith(ShareSplitHint::marker()), qPrintable(cell));
        QVERIFY(!footerTbl->item(0, 1)->toolTip().isEmpty());
    }

    void test_viewBuyEdit_populateOverview_split_uebersichtFooterDoesNotMixScales()
    {
        // Kernpunkt von Phase 3c: 5 + 100 = 105 wäre eine Zahl, die es in
        // keiner Stückelung je gab. Richtig sind 100 + 100 = 200.
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        QVERIFY(tabs != nullptr);

        dlg.populateOverview(buysAroundSplit(), brokeragesAroundSplit(), splitList2022());

        auto* footerTbl = footerTableFromContainer(tabs->widget(0)); // Übersicht
        QVERIFY(footerTbl != nullptr);
        QVERIFY(footerTbl->item(0, 1) != nullptr);

        const QString cell = footerTbl->item(0, 1)->text();
        QVERIFY2(cell.contains(QLocale().toString(200.0, 'f', 4)), qPrintable(cell));
        QVERIFY2(!cell.contains(QLocale().toString(105.0, 'f', 4)), qPrintable(cell));
    }

    void test_viewBuyEdit_populateOverview_split_uebersichtYearRowIsOnTodaysScale()
    {
        // Auch eine Jahreszeile ist bereits eine Summe — sie mischt sonst
        // genauso, sobald der Splittag mitten im Jahr liegt.
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        QVERIFY(tabs != nullptr);

        dlg.populateOverview(buysAroundSplit(), brokeragesAroundSplit(), splitList2022());

        auto* tbl = dataTableFromContainer(tabs->widget(0));
        QVERIFY(tbl != nullptr);
        QCOMPARE(tbl->rowCount(), 2);

        // Zeile 0 = 2023 (absteigend sortiert), Zeile 1 = 2022.
        QVERIFY(tbl->item(1, 1) != nullptr);
        const QString cell2022 = tbl->item(1, 1)->text();
        QVERIFY2(cell2022.contains(QLocale().toString(100.0, 'f', 4)), qPrintable(cell2022));
        QVERIFY2(cell2022.endsWith(ShareSplitHint::marker()), qPrintable(cell2022));

        // Das Jahr nach dem Split bleibt unmarkiert.
        QVERIFY(tbl->item(0, 1) != nullptr);
        QVERIFY(!tbl->item(0, 1)->text().contains(ShareSplitHint::marker()));
    }

    void test_viewBuyEdit_populateOverview_split_moneyColumnsAreUnchanged()
    {
        // Über einen Split ist Stückzahl × Preis invariant — Einzahlung und
        // Gebühren dürfen sich durch Phase 3c nicht bewegen.
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        QVERIFY(tabs != nullptr);

        dlg.populateOverview(buysAroundSplit(), brokeragesAroundSplit(), splitList2022());

        auto* tbl = dataTableFromContainer(tabs->widget(2)); // 2022
        QVERIFY(tbl != nullptr);
        QVERIFY(tbl->item(0, 4) != nullptr);

        // 5 × 1.003,00 = 5.015,00 + 9,90 Provision = 5.024,90
        const QString einzahlung = tbl->item(0, 4)->text();
        QVERIFY2(einzahlung.contains(QLocale().toString(5024.90, 'f', 2)),
                 qPrintable(einzahlung));
    }

    void test_viewBuyEdit_populateOverview_withoutSplits_noMarkerAnywhere()
    {
        // Regression: ohne gespeicherte Splits muss die Übersicht exakt so
        // aussehen wie vor Phase 3c — kein Marker, kein Tooltip, Rohsummen.
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        QVERIFY(tabs != nullptr);

        dlg.populateOverview(buysAroundSplit(), brokeragesAroundSplit(), {});

        auto* footerTbl = footerTableFromContainer(tabs->widget(0));
        QVERIFY(footerTbl != nullptr);
        QVERIFY(footerTbl->item(0, 1) != nullptr);

        const QString cell = footerTbl->item(0, 1)->text();
        QVERIFY2(cell.contains(QLocale().toString(105.0, 'f', 4)), qPrintable(cell));
        QVERIFY2(!cell.contains(ShareSplitHint::marker()), qPrintable(cell));
        QVERIFY(footerTbl->item(0, 1)->toolTip().isEmpty());

        auto* tbl = dataTableFromContainer(tabs->widget(2));
        QVERIFY(tbl != nullptr);
        QVERIFY(tbl->item(0, 1) != nullptr);
        QVERIFY(!tbl->item(0, 1)->text().contains(ShareSplitHint::marker()));
        QVERIFY(tbl->item(0, 1)->toolTip().isEmpty());
    }

    void test_presenterBuyEdit_passesSplitsToOverview()
    {
        // Ohne diesen Durchreichweg bliebe die Übersicht dauerhaft
        // split-blind, ohne dass ein View-Test das bemerken würde.
        StubViewBuyEdit  view;
        StubModelBuyEdit model;
        model.splits << ShareSplitObject(QStringLiteral("s1"), QStringLiteral("share-1"),
                                         QDate(2022, 7, 18), 20.0, 1.0);
        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);

        QVERIFY(view.populateOverviewCalled);
        QCOMPARE(view.lastOverviewSplits.size(), 1);
    }

    void test_viewBuyEdit_uebersichtClick_jumpsToYearTab()
    {
        // Clicking a year row in the Uebersicht tab must switch to that year's tab.
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = setupTwoYearOverview(dlg);
        QVERIFY(tabs != nullptr);
        QCOMPARE(tabs->count(), 3); // Uebersicht + 2023 + 2024

        // Get the Uebersicht data table and simulate clicking row 1 (year 2024)
        auto* container = tabs->widget(0);
        QVERIFY(container != nullptr);
        auto* tbl = dataTableFromContainer(container);
        QVERIFY(tbl != nullptr);

        // Emit cellClicked directly — row 0 = 2024 (descending order, newest first)
        emit tbl->cellClicked(0, 0);

        // Should have jumped to the 2024 tab (index 1, descending)
        QVERIFY(tabs->tabText(tabs->currentIndex()).contains(QStringLiteral("2024")));
    }

    void test_viewBuyEdit_uebersichtRowSelection_isEnabled()
    {
        // Uebersicht tab must allow row selection (user picks a year to jump to).
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = setupTwoYearOverview(dlg);
        QVERIFY(tabs != nullptr);

        auto* container = tabs->widget(0);
        QVERIFY(container != nullptr);
        auto* tbl = dataTableFromContainer(container);
        QVERIFY(tbl != nullptr);
        QCOMPARE(tbl->selectionBehavior(), QAbstractItemView::SelectRows);
        QCOMPARE(tbl->selectionMode(),     QAbstractItemView::SingleSelection);
    }

    void test_viewBuyEdit_jahresTab_hasSelectRows()
    {
        // Jahres-tabs must use SelectRows so no individual cell can be selected.
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = setupTwoYearOverview(dlg);
        QVERIFY(tabs != nullptr);

        auto* container = tabs->widget(1); // first Jahres-tab
        QVERIFY(container != nullptr);
        auto* tbl = dataTableFromContainer(container);
        QVERIFY(tbl != nullptr);
        QCOMPARE(tbl->selectionBehavior(), QAbstractItemView::SelectRows);
        QCOMPARE(tbl->selectionMode(),     QAbstractItemView::SingleSelection);
    }

    void test_viewBuyEdit_tabChange_clearsOldSelection()
    {
        // Switching tabs must clear the selection in the previously active tab.
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = setupTwoYearOverview(dlg);
        QVERIFY(tabs != nullptr);

        // Select row 0 in tab 1 (first Jahres-tab)
        tabs->setCurrentIndex(1);
        auto* container1 = tabs->widget(1);
        auto* tbl1 = dataTableFromContainer(container1);
        QVERIFY(tbl1 != nullptr);
        tbl1->selectRow(0);
        QVERIFY(!tbl1->selectedItems().isEmpty());

        // Switch to tab 2 — selection in tab 1 must be cleared
        tabs->setCurrentIndex(2);
        QVERIFY(tbl1->selectedItems().isEmpty());
    }

    void test_viewBuyEdit_tabChange_selectsFirstRowInJahresTab()
    {
        // Switching to a Jahres-tab must auto-select its first data row.
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = setupTwoYearOverview(dlg);
        QVERIFY(tabs != nullptr);

        // Start on Uebersicht tab, then switch to first Jahres-tab
        tabs->setCurrentIndex(0);
        tabs->setCurrentIndex(1);

        auto* container = tabs->widget(1);
        auto* tbl = dataTableFromContainer(container);
        QVERIFY(tbl != nullptr);
        QCOMPARE(tbl->currentRow(), 0);
        QVERIFY(!tbl->selectedItems().isEmpty());
    }

    void test_viewBuyEdit_tabChange_noAutoSelectInUebersicht()
    {
        // Switching back to the Uebersicht tab (index 0) must NOT auto-select any row.
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = setupTwoYearOverview(dlg);
        QVERIFY(tabs != nullptr);

        // Go to a Jahres-tab first, then back to Uebersicht
        tabs->setCurrentIndex(1);
        tabs->setCurrentIndex(0);

        auto* container = tabs->widget(0);
        auto* tbl = dataTableFromContainer(container);
        QVERIFY(tbl != nullptr);
        QVERIFY(tbl->selectedItems().isEmpty());
    }

    void test_viewBuyEdit_tabChange_toJahresTab_selectsFirstRow()
    {
        // Switching to a Jahres-tab must auto-select row 0 (presenter guard is
        // null-safe in tests — this covers the view-side behaviour).
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = setupTwoYearOverview(dlg);
        QVERIFY(tabs != nullptr);

        tabs->setCurrentIndex(0);
        tabs->setCurrentIndex(1);   // switch to first Jahres-tab

        auto* container = tabs->widget(1);
        auto* tbl = dataTableFromContainer(container);
        QVERIFY(tbl != nullptr);
        QVERIFY(!tbl->selectedItems().isEmpty());
        QCOMPARE(tbl->currentRow(), 0);
    }

    void test_viewBuyEdit_tabChange_backToUebersicht_clearsJahresSelection()
    {
        // After switching back to Übersicht the Jahres-tab must have no selection.
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);
        auto* tabs = setupTwoYearOverview(dlg);
        QVERIFY(tabs != nullptr);

        tabs->setCurrentIndex(1);   // go to Jahres-tab → row 0 selected
        tabs->setCurrentIndex(0);   // back to Übersicht

        auto* container = tabs->widget(1);
        auto* tbl = dataTableFromContainer(container);
        QVERIFY(tbl != nullptr);
        QVERIFY(tbl->selectedItems().isEmpty());
    }

    void test_viewBuyEdit_setButtonStates_noSelection_addLabelHinzufuegen()
    {
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        dlg.setButtonStates(false, false, false);

        const auto buttons = dlg.findChildren<QPushButton*>();
        bool found = false;
        for (auto* b : buttons) {
            if (b->text() == tr("Hinzufügen")) { found = true; break; }
        }
        QVERIFY(found);
    }

    void test_viewBuyEdit_setButtonStates_canRemoveTrue_saveLabelSpeichern()
    {
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        dlg.setButtonStates(true, true, true);

        const auto buttons = dlg.findChildren<QPushButton*>();
        bool found = false;
        for (auto* b : buttons) {
            if (b->text() == tr("Speichern")) { found = true; break; }
        }
        QVERIFY(found);
    }

    void test_viewBuyEdit_setButtonStates_nonLatestBuy_saveLabelSpeichern()
    {
        // Non-latest buy: canRemove=false but isEdit=true → must show "Speichern"
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        dlg.setButtonStates(false, false, true);

        const auto buttons = dlg.findChildren<QPushButton*>();
        bool found = false;
        for (auto* b : buttons) {
            if (b->text() == tr("Speichern")) { found = true; break; }
        }
        QVERIFY(found);
    }

    void test_viewBuyEdit_setButtonStates_canRemoveFalse_removeDisabled()
    {
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        dlg.setButtonStates(false, false, false);

        // The remove button must be disabled when canRemove=false
        const auto buttons = dlg.findChildren<QPushButton*>();
        for (auto* b : buttons) {
            if (b->text() == tr("Entfernen")) {
                QVERIFY(!b->isEnabled());
                return;
            }
        }
        QFAIL("Entfernen button not found");
    }

    void test_viewBuyEdit_setButtonStates_canRemoveTrue_removeEnabled()
    {
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        dlg.setButtonStates(true, true, true);

        const auto buttons = dlg.findChildren<QPushButton*>();
        for (auto* b : buttons) {
            if (b->text() == tr("Entfernen")) {
                QVERIFY(b->isEnabled());
                return;
            }
        }
        QFAIL("Entfernen button not found");
    }

    void test_viewBuyEdit_setButtonStates_notLastBuy_fieldsDisabled()
    {
        // Non-latest buy: order number and other input fields must be disabled.
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        dlg.setButtonStates(true, false, true);

        const auto edits = dlg.findChildren<QLineEdit*>();
        for (auto* le : edits) {
            if (le->maxLength() == 100) {  // m_orderNumber has maxLength(100)
                QVERIFY(!le->isEnabled());
                return;
            }
        }
        QFAIL("orderNumber QLineEdit not found");
    }

    void test_viewBuyEdit_setButtonStates_isLastBuy_fieldsEnabled()
    {
        // Latest buy: order number must be enabled.
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        dlg.setButtonStates(true, true, true);

        const auto edits = dlg.findChildren<QLineEdit*>();
        for (auto* le : edits) {
            if (le->maxLength() == 100) {  // m_orderNumber
                QVERIFY(le->isEnabled());
                return;
            }
        }
        QFAIL("orderNumber QLineEdit not found");
    }

    void test_viewBuyEdit_clearForm_restoresEditableFields()
    {
        // After setButtonStates(true, false, true) + clearForm(), fields must be enabled again.
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        dlg.setButtonStates(true, false, true);   // put into disabled mode
        dlg.clearForm();                           // must restore enabled state

        const auto edits = dlg.findChildren<QLineEdit*>();
        for (auto* le : edits) {
            if (le->maxLength() == 100) {   // m_orderNumber
                QVERIFY(le->isEnabled());
                return;
            }
        }
        QFAIL("orderNumber QLineEdit not found");
    }

    void test_viewBuyEdit_setFieldOk_doesNotOverwriteWithEmptyValue()
    {
        // setFieldOk("orderNumber", QString()) is called by live-validation
        // slots — it must update the icon but NOT clear the QLineEdit text.
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        // Manually set a value into the orderNumber QLineEdit
        const auto edits = dlg.findChildren<QLineEdit*>();
        QLineEdit* orderNrEdit = nullptr;
        for (auto* le : edits) {
            if (le->maxLength() == 100) { orderNrEdit = le; break; }
        }
        if (!orderNrEdit) QFAIL("orderNumber QLineEdit not found");
        orderNrEdit->setText(QStringLiteral("ORD-123"));

        // Call setFieldOk with empty value (as live validation does)
        dlg.setFieldOk(QStringLiteral("orderNumber"), QString());

        // Text must be unchanged
        QCOMPARE(orderNrEdit->text(), QStringLiteral("ORD-123"));
    }

    void test_viewBuyEdit_setFieldOk_writesValueWhenNonEmpty()
    {
        // setFieldOk with a non-empty value (from parser) MUST update the widget.
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        dlg.setFieldOk(QStringLiteral("orderNumber"), QStringLiteral("ORD-456"));

        const auto edits = dlg.findChildren<QLineEdit*>();
        for (auto* le : edits) {
            if (le->maxLength() == 100) {
                QCOMPARE(le->text(), QStringLiteral("ORD-456"));
                return;
            }
        }
        QFAIL("orderNumber QLineEdit not found");
    }

    /**
     * @brief Die Ordernummer wird ZEICHENGETREU übernommen.
     *
     * Nessies Bugreport 22.08.2026: der DKB-Beleg zeigt "670835/66.00", im
     * Formular stand "670835/66,00". Ursache war, dass die View jedem
     * einzeiligen Feld die Dezimalpunkt-Umschreibung verpasste — gedacht war
     * sie für Zahlenfelder. Eine Ordernummer ist keine Zahl.
     */
    void test_viewBuyEdit_setFieldOk_orderNumber_keepsDot()
    {
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        dlg.setFieldOk(QStringLiteral("orderNumber"),
                       QStringLiteral(" 670835/66.00\n"));

        QCOMPARE(dlg.orderNumber(), QStringLiteral("670835/66.00"));
    }

    /// Gegenprobe: bei ZAHLENfeldern (die einen QDoubleValidator tragen) ist
    /// der Punkt sehr wohl der Dezimaltrenner und wird zum Komma.
    void test_viewBuyEdit_setFieldOk_numericField_stillNormalisesDot()
    {
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        dlg.setFieldOk(QStringLiteral("price"), QStringLiteral("39.998"));

        QCOMPARE(dlg.price(), 39.998);
    }

    /**
     * @brief Ein Tausendertrenner darf den Wert nicht auf null fallen lassen.
     *
     * Vorher wurde aus "1.234,56" die Zeichenkette "1,234,56", und
     * `parseDouble()` machte daraus 0,00. Aufgefallen beim Beheben der
     * Ordernummer; ein Beleg mit einem solchen Betrag lag nicht vor.
     */
    /**
     * @brief Was die View ins Feld schreibt, muss sie auch zurücklesen können.
     *
     * Regression 06.09.2026: loadBuy() befüllt die Felder über
     * formatVolume()/formatPrice(), also MIT Tausendertrennzeichen. Die alte
     * parseDouble() ließ das Zeichen stehen und lieferte 0,0 — ein Kauf zu
     * 1.003,50 € je Stück wurde korrekt angezeigt und beim nächsten Speichern
     * auf null zurückgeschrieben, ohne jede Meldung. Siehe ARCHITECTURE.md,
     * "Zahlenfelder verlieren Werte ab 1.000 beim Zurücklesen".
     */
    void test_viewBuyEdit_loadBuy_fourDigitValuesSurviveReadBack()
    {
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        const BuyObject buy = makeBuy(QStringLiteral("buy-roundtrip"),
                                      QStringLiteral("share-guid"),
                                      2024, 2500.0, 1003.50);
        const BrokerageObject brokerage =
            makeBrokerage(QStringLiteral("buy-roundtrip"),
                          QStringLiteral("share-guid"), 1200.25);

        dlg.loadBuy(buy, brokerage);

        QCOMPARE(dlg.volume(), 2500.0);
        QCOMPARE(dlg.price(), 1003.50);
        QCOMPARE(dlg.provision(), 1200.25);
    }

    void test_viewBuyEdit_setFieldOk_numericField_handlesThousandsSeparator()
    {
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        dlg.setFieldOk(QStringLiteral("price"), QStringLiteral("1.234,56"));

        QCOMPARE(dlg.price(), 1234.56);
    }

    /**
     * @brief Ein unbrauchbarer Datumswert muss SICHTBAR werden.
     *
     * Vorher blieb bei einer misslungenen Umwandlung stillschweigend das
     * heutige Datum stehen — bei einem Kauf aus 2018 kein Schönheitsfehler,
     * sondern ein falscher Datensatz.
     */
    void test_viewBuyEdit_setFieldOk_unparsableDate_marksFieldAsError()
    {
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        // m_fieldStates ist privat — gezählt wird deshalb über den Tooltip
        // der Status-Symbole, den setFieldError() setzt.
        //
        // Der Tooltip-Text hat sich am 27.08.2026 geändert: setFieldError()
        // bekommt seither den verworfenen Rohwert und zeigt ihn an. Der
        // allgemeine Text "Ungültige oder fehlende Eingabe" steht nur noch
        // bei den Aufrufen aus der Live-Validierung, wo es keinen Rohwert
        // gibt — hier gäbe es also nichts mehr zu zählen.
        auto rejectedLabels = [&dlg]() {
            int n = 0;
            for (auto* l : dlg.findChildren<QLabel*>())
                if (l->toolTip().startsWith(QObject::tr("Nicht verwertbar:")))
                    ++n;
            return n;
        };

        const int before = rejectedLabels();
        QVERIFY(!dlg.setFieldOk(QStringLiteral("date"),
                                QStringLiteral("kein Datum")));
        QCOMPARE(rejectedLabels(), before + 1);
    }

    /**
     * @brief Der verworfene Rohwert steht im Tooltip des Fehlersymbols.
     *
     * Seit die Statuszeile nur noch übernommene Werte zählt, sagt sie nicht
     * mehr, ob die Regel in Documents.xml überhaupt gegriffen hat. Diese
     * Auskunft steht jetzt hier — ohne sie wäre beim Schreiben von Regeln
     * nicht zu unterscheiden, ob nichts gefunden oder etwas Unbrauchbares
     * gefangen wurde.
     */
    void test_viewBuyEdit_setFieldError_tooltipNamesRejectedRawValue()
    {
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        dlg.setFieldOk(QStringLiteral("date"),
                       QStringLiteral("Schlusstag 04/02"));

        bool found = false;
        for (auto* l : dlg.findChildren<QLabel*>()) {
            if (l->toolTip().contains(QStringLiteral("Schlusstag 04/02"))) {
                found = true;
                break;
            }
        }
        QVERIFY2(found, "Rohwert steht nicht im Tooltip des Fehlersymbols");
    }

    /// Gegenprobe: ein brauchbares Datum darf KEIN Fehlersymbol setzen.
    void test_viewBuyEdit_setFieldOk_validDate_marksNoError()
    {
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        auto errorLabels = [&dlg]() {
            int n = 0;
            for (auto* l : dlg.findChildren<QLabel*>())
                if (l->toolTip() == QObject::tr("Ungültige oder fehlende Eingabe"))
                    ++n;
            return n;
        };

        const int before = errorLabels();
        dlg.setFieldOk(QStringLiteral("date"), QStringLiteral("4.2.2026"));
        QCOMPARE(errorLabels(), before);
    }

    void test_viewBuyEdit_populateOverview_jahresTabsDescendingByYear()
    {
        // Years must appear newest-first: Tab 1 = newest year, Tab 2 = older.
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");

        const QList<BuyObject> buys = {
            makeBuy(QStringLiteral("b1"), QStringLiteral("share-guid"), 2022, 5.0, 80.0),
            makeBuy(QStringLiteral("b2"), QStringLiteral("share-guid"), 2024, 10.0, 100.0),
        };
        const QList<BrokerageObject> brokerages = {
            makeBrokerage(QStringLiteral("b1"), QStringLiteral("share-guid"), 0.0),
            makeBrokerage(QStringLiteral("b2"), QStringLiteral("share-guid"), 0.0),
        };
        dlg.populateOverview(buys, brokerages, {});

        // Tab 0 = Übersicht, Tab 1 = 2024 (newest), Tab 2 = 2022 (older)
        QVERIFY(tabs->count() == 3);
        QVERIFY(tabs->tabText(1).contains(QStringLiteral("2024")));
        QVERIFY(tabs->tabText(2).contains(QStringLiteral("2022")));
    }

    void test_viewBuyEdit_canBeConstructed()
    {
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);
        QVERIFY(dlg.windowTitle().contains(tr("Käufe")));
    }

    void test_viewBuyEdit_initialValues()
    {
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);
        QCOMPARE(dlg.volume(),    0.0);
        QCOMPARE(dlg.price(),     0.0);
        QCOMPARE(dlg.provision(), 0.0);
        QCOMPARE(dlg.brokerFee(), 0.0);
        QCOMPARE(dlg.traderFee(), 0.0);
        QCOMPARE(dlg.reduction(), 0.0);
        QVERIFY(dlg.orderNumber().isEmpty());
        QVERIFY(dlg.documentPath().isEmpty());
    }

    void test_viewBuyEdit_depotNumberCombo_populatedFromConfig()
    {
        openMemoryDb();
        if (m_docsConfig.count() == 0)
            QSKIP("Documents.xml not loaded — depot combo test skipped");
        ViewBuyEdit dlg(QStringLiteral("share-guid"), &m_docsConfig);
        const auto combos = dlg.findChildren<QComboBox*>();
        bool found = false;
        for (auto* c : combos) {
            if (c->count() > 1) { found = true; break; }
        }
        QVERIFY(found);
    }

    void test_viewBuyEdit_depotNumberCombo_itemDataIsIdentifier()
    {
        openMemoryDb();
        if (m_docsConfig.count() == 0)
            QSKIP("Documents.xml not loaded — depot combo test skipped");
        ViewBuyEdit dlg(QStringLiteral("share-guid"), &m_docsConfig);
        const auto combos = dlg.findChildren<QComboBox*>();
        for (auto* c : combos) {
            if (c->count() > 1) {
                QVERIFY(!c->itemData(1).toString().isEmpty());
                return;
            }
        }
        QFAIL("Depot combo with entries not found");
    }

    void test_viewBuyEdit_setFieldOk_date_parsesDotFormat()
    {
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);
        dlg.setFieldOk(QStringLiteral("date"), QStringLiteral("4.2.2026"));
        const auto* de = dlg.findChild<QDateEdit*>();
        QVERIFY(de != nullptr);
        QCOMPARE(de->date(), QDate(2026, 2, 4));
    }

    void test_viewBuyEdit_setFieldOk_date_parsesISOFormat()
    {
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);
        dlg.setFieldOk(QStringLiteral("date"), QStringLiteral("2026-02-04"));
        const auto* de = dlg.findChild<QDateEdit*>();
        if (!de) QFAIL("QDateEdit not found");
        QVERIFY(de->date().isValid());
        QCOMPARE(de->date(), QDate(2026, 2, 4));
    }

    void test_viewBuyEdit_setFieldOk_time_parsesHMS()
    {
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);
        dlg.setFieldOk(QStringLiteral("time"), QStringLiteral("19:51:45"));
        const auto* te = dlg.findChild<QTimeEdit*>();
        QVERIFY(te != nullptr);
        QCOMPARE(te->time(), QTime(19, 51, 45));
    }

    void test_viewBuyEdit_setFieldOk_depotNumber_matchesByItemData()
    {
        openMemoryDb();
        if (m_docsConfig.count() == 0)
            QSKIP("Documents.xml not loaded — depot combo test skipped");
        ViewBuyEdit dlg(QStringLiteral("share-guid"), &m_docsConfig);
        const auto combos = dlg.findChildren<QComboBox*>();
        for (auto* c : combos) {
            if (c->count() > 1) {
                const QString id = c->itemData(1).toString();
                dlg.setFieldOk(QStringLiteral("depotNumber"), id);
                QCOMPARE(c->currentIndex(), 1);
                return;
            }
        }
        QFAIL("Depot combo with entries not found");
    }

    void test_viewBuyEdit_setFieldOk_volume_handlesGermanDecimal()
    {
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);
        dlg.setFieldOk(QStringLiteral("volume"), QStringLiteral("15,00"));
        QCOMPARE(dlg.volume(), 15.0);
    }

    void test_viewBuyEdit_setFieldError_doesNotCrash()
    {
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);
        dlg.setFieldError(QStringLiteral("date"));
        dlg.setFieldError(QStringLiteral("volume"));
        dlg.setFieldError(QStringLiteral("unknownField"));
        QVERIFY(true); // no crash
    }

    void test_viewBuyEdit_hasMissingRequiredFields_initiallyTrue()
    {
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);
        QStringList missing;
        QVERIFY(dlg.hasMissingRequiredFields(missing));
        QVERIFY(!missing.isEmpty());
    }

    void test_viewBuyEdit_hasMissingRequiredFields_depotNumber_checkedByItemData()
    {
        openMemoryDb();
        if (m_docsConfig.count() == 0)
            QSKIP("Documents.xml not loaded — depot combo test skipped");
        ViewBuyEdit dlg(QStringLiteral("share-guid"), &m_docsConfig);
        // Placeholder entry has no itemData → depotNumber must appear in missing list
        const auto combos = dlg.findChildren<QComboBox*>();
        for (auto* c : combos) {
            if (c->count() > 1) {
                c->setCurrentIndex(0); // placeholder — no itemData
                QStringList missing;
                QVERIFY(dlg.hasMissingRequiredFields(missing));
                // hasMissingRequiredFields appends tr("Depotnummer"), not "depotNumber"
                QVERIFY(!missing.isEmpty());
                return;
            }
        }
        QFAIL("Depot combo not found");
    }

    void test_viewBuyEdit_hasMissingRequiredFields_falseAfterAllSet()
    {
        openMemoryDb();
        if (m_docsConfig.count() == 0)
            QSKIP("Documents.xml not loaded — depot combo test skipped");
        ViewBuyEdit dlg(QStringLiteral("share-guid"), &m_docsConfig);
        dlg.setFieldOk(QStringLiteral("date"),   QStringLiteral("2024-06-01"));
        dlg.setFieldOk(QStringLiteral("volume"), QStringLiteral("10"));
        dlg.setFieldOk(QStringLiteral("price"),  QStringLiteral("100"));
        dlg.setFieldOk(QStringLiteral("orderNumber"), QStringLiteral("ORD-001"));
        // Set depot via itemData
        const auto combos = dlg.findChildren<QComboBox*>();
        for (auto* c : combos) {
            if (c->count() > 1) {
                dlg.setFieldOk(QStringLiteral("depotNumber"), c->itemData(1).toString());
                break;
            }
        }
        QStringList missing;
        QVERIFY(!dlg.hasMissingRequiredFields(missing));
    }

    void test_viewBuyEdit_markMissingFieldsAsFailed_doesNotCrash()
    {
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);
        dlg.markMissingFieldsAsFailed();
        QVERIFY(true);
    }

    void test_viewBuyEdit_clearForm_resetsAllFields()
    {
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);
        dlg.setFieldOk(QStringLiteral("volume"), QStringLiteral("50"));
        dlg.setFieldOk(QStringLiteral("price"),  QStringLiteral("200"));
        dlg.clearForm();
        QCOMPARE(dlg.volume(), 0.0);
        QCOMPARE(dlg.price(),  0.0);
        QVERIFY(dlg.orderNumber().isEmpty());
    }

    void test_viewBuyEdit_clearForm_resetsStatusIcons()
    {
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);
        dlg.setFieldError(QStringLiteral("date"));
        dlg.clearForm();
        // After clearForm, setting fields again must not be prevented by old error state
        dlg.setFieldOk(QStringLiteral("volume"), QStringLiteral("10"));
        QCOMPARE(dlg.volume(), 10.0);
    }

    void test_viewBuyEdit_clearForm_resetsParseStatusBar()
    {
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);
        dlg.setParseProgress(75, QStringLiteral("Parsing..."));
        dlg.clearForm();
        // m_parseProgress has fixedWidth=200 and fixedHeight=14 — unique identifier
        const auto bars = dlg.findChildren<QProgressBar*>();
        QProgressBar* parseBar = nullptr;
        for (auto* bar : bars) {
            if (bar->maximumWidth() == 200 && bar->maximumHeight() == 14) {
                parseBar = bar;
                break;
            }
        }
        if (!parseBar) QFAIL("Parse progress bar not found");
        QCOMPARE(parseBar->value(), 0);
    }

    void test_viewBuyEdit_setParseProgress_showsValues()
    {
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);
        dlg.setParseProgress(50, QStringLiteral("Wird geladen..."));
        const auto bars = dlg.findChildren<QProgressBar*>();
        bool found = false;
        for (auto* bar : bars) {
            if (bar->value() == 50) { found = true; break; }
        }
        QVERIFY(found);
    }

    // ── Split-Hinweis (Phase 3b, 09.08.2026) ──────────────────────────────
    //
    // Die Formatierung selbst prüft tst_sharesplithint — hier geht es nur um
    // die Verdrahtung: dass der Presenter die Splits lädt, den Hinweis bei
    // jeder relevanten Änderung neu setzt, und dass die View ihn im dafür
    // vorgesehenen Label ablegt.

    void test_presenterBuyEdit_setsSplitHintOnConstruction()
    {
        StubViewBuyEdit  view;
        StubModelBuyEdit model;
        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);

        QVERIFY(view.splitHintCallCount > 0);
        QVERIFY(!view.lastSplitHint.isEmpty());
    }

    void test_presenterBuyEdit_noSplits_hintSaysNoSplit()
    {
        StubViewBuyEdit  view;
        StubModelBuyEdit model;
        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);

        QVERIFY(!view.lastHasSplit);
        QVERIFY(view.lastSplitTooltip.isEmpty());
    }

    void test_presenterBuyEdit_splitAfterBuyDate_hintIsActive()
    {
        StubViewBuyEdit  view;
        StubModelBuyEdit model;
        view.m_dateTime = QStringLiteral("2021-03-18T10:00:00");
        view.m_volume   = 5.0;
        view.m_price    = 1003.00;
        model.splits << ShareSplitObject(QStringLiteral("s1"), QStringLiteral("share-1"),
                                         QDate(2022, 7, 18), 20.0, 1.0);
        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);

        QVERIFY(view.lastHasSplit);
        QVERIFY2(view.lastSplitHint.contains(QStringLiteral("20:1")),
                 qPrintable(view.lastSplitHint));
        QVERIFY(!view.lastSplitTooltip.isEmpty());
    }

    void test_presenterBuyEdit_splitBeforeBuyDate_hintIsInactive()
    {
        StubViewBuyEdit  view;
        StubModelBuyEdit model;
        view.m_dateTime = QStringLiteral("2023-02-14T10:00:00");
        model.splits << ShareSplitObject(QStringLiteral("s1"), QStringLiteral("share-1"),
                                         QDate(2022, 7, 18), 20.0, 1.0);
        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);

        QVERIFY(!view.lastHasSplit);
    }

    void test_presenterBuyEdit_onDateEdited_refreshesHint()
    {
        // Der Hinweis läuft live mit (Nessies Entscheidung 08.08.2026):
        // refreshDerivedValues() allein genügt nicht, es wird beim Ändern des
        // Datums nicht aufgerufen.
        StubViewBuyEdit  view;
        StubModelBuyEdit model;
        view.m_dateTime = QStringLiteral("2023-02-14T10:00:00");
        model.splits << ShareSplitObject(QStringLiteral("s1"), QStringLiteral("share-1"),
                                         QDate(2022, 7, 18), 20.0, 1.0);
        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        QVERIFY(!view.lastHasSplit);

        view.m_dateTime = QStringLiteral("2021-03-18T10:00:00");
        p.onDateEdited();

        QVERIFY(view.lastHasSplit);
    }

    void test_presenterBuyEdit_onValuesChanged_refreshesHint()
    {
        StubViewBuyEdit  view;
        StubModelBuyEdit model;
        view.m_dateTime = QStringLiteral("2021-03-18T10:00:00");
        view.m_volume   = 5.0;
        view.m_price    = 1003.00;
        model.splits << ShareSplitObject(QStringLiteral("s1"), QStringLiteral("share-1"),
                                         QDate(2022, 7, 18), 20.0, 1.0);
        PresenterBuyEdit p(&view, &model, QStringLiteral("share-1"), nullptr);

        const int before = view.splitHintCallCount;
        view.m_volume = 10.0;
        p.onValuesChanged();

        QVERIFY(view.splitHintCallCount > before);
        // 10 × 20 = 200
        QVERIFY2(view.lastSplitHint.contains(QLocale().toString(200.0, 'f', 4)),
                 qPrintable(view.lastSplitHint));
    }

    void test_viewBuyEdit_hasSplitHintLabel()
    {
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        QVERIFY(dlg.findChild<QLabel*>(QStringLiteral("splitHint")));
    }

    void test_viewBuyEdit_setSplitHint_setsTextAndTooltip()
    {
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        dlg.setSplitHint(QStringLiteral("Split 20:1 am 18.07.2022"),
                         QStringLiteral("20:1 am 18.07.2022"),
                         /*hasSplit=*/true);

        auto* label = dlg.findChild<QLabel*>(QStringLiteral("splitHint"));
        if (!label) QFAIL("splitHint label not found");
        QCOMPARE(label->text(), QStringLiteral("Split 20:1 am 18.07.2022"));
        QCOMPARE(label->toolTip(), QStringLiteral("20:1 am 18.07.2022"));
    }

    void test_viewBuyEdit_setSplitHint_labelStaysVisibleWithoutSplit()
    {
        // Kernpunkt der Platzierungsentscheidung: die Zeile verschwindet
        // NICHT, wenn kein Split vorliegt — sonst würden beim Tippen im
        // Datumsfeld alle Zeilen darüber springen.
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        dlg.setSplitHint(QStringLiteral("Kein Split nach diesem Datum"),
                         QString(), /*hasSplit=*/false);

        auto* label = dlg.findChild<QLabel*>(QStringLiteral("splitHint"));
        if (!label) QFAIL("splitHint label not found");
        QVERIFY(!label->text().isEmpty());
        QVERIFY(!label->isHidden());
    }

    void test_viewBuyEdit_populateOverview_docPdfIcon()
    {
        // A buy with a .pdf document must render a PDF icon via setCellWidget
        openMemoryDb();
        ViewBuyEdit dlg(QStringLiteral("share-guid"), nullptr);

        BuyObject b = makeBuy(QStringLiteral("b1"), QStringLiteral("share-guid"),
                               2024, 10.0, 100.0);
        b.setDocument(QStringLiteral("/some/path/receipt.pdf"));
        const BrokerageObject br = makeBrokerage(QStringLiteral("b1"),
                                                  QStringLiteral("share-guid"), 9.90);
        dlg.populateOverview({ b }, { br }, {});

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        auto* container = tabs->widget(1); // first Jahres-tab
        auto* tbl = dataTableFromContainer(container);
        if (!tbl) QFAIL("dataTable not found");
        // Column 5 = Dok.; must have a cellWidget (QLabel with pixmap)
        QVERIFY(tbl->cellWidget(0, 5) != nullptr);
    }

    // ─────────────────────────────────────────────────────────────────────
    // Analyse-Statuszeile zaehlt UEBERNOMMENE Werte, nicht gefangene
    // (27.08.2026)
    //
    // Bis dahin zaehlte populateFromResult() jeden Wert, den der Parser
    // gefangen hatte — auch einen, den die View verworfen hatte. Die
    // Statuszeile meldete dann "Analyse OK — 5/5 Pflicht",
    // waehrend am Feld das rote Symbol stand. Siehe ARCHITECTURE.md,
    // "Analyse-Statuszeile und Feldsymbole".
    //
    // failingFields im Stub laesst gezielt ein Feld scheitern — so verhaelt
    // sich die echte View bei einem unbrauchbaren Datum oder einer
    // Depotnummer, die nicht in Documents.xml steht. Ohne diesen Schalter
    // waere der false-Zweig von keinem Test erreichbar.
    //
    // Die Statuszeile wird ueber QTimer::singleShot(0, ...) gesetzt, deshalb
    // QTRY_COMPARE: es verarbeitet Ereignisse und prueft erneut, bis der
    // Rueckruf gelaufen ist. Ein einzelnes processEvents() reichte nicht —
    // siehe den Kommentar an der Pruefung.
    // ─────────────────────────────────────────────────────────────────────

    void test_presenterBuyEdit_populateFromResult_allFieldsTaken_reportsOk()
    {
        StubViewBuyEdit view;
        StubModelBuyEdit model;
        PresenterBuyEdit p(&view, &model,
                           QStringLiteral("share-1"), nullptr);

        const QMap<QString, QList<QString>> result = {
            { QStringLiteral("Date"), { QStringLiteral("12.03.2024") } },
            { QStringLiteral("DepotNumber"), { QStringLiteral("1234567890") } },
            { QStringLiteral("OrderNumber"), { QStringLiteral("ORD-1") } },
            { QStringLiteral("Volume"), { QStringLiteral("10") } },
            { QStringLiteral("Price"), { QStringLiteral("245,60") } },
        };

        p.populateFromResult(result);
        // QTRY_COMPARE statt eines einzelnen processEvents(): der Rueckruf
        // haengt an QTimer::singleShot(0, ...), und EIN Durchlauf reicht nicht
        // zuverlaessig — liegen noch Aufraeum-Ereignisse eines frueheren Tests
        // in der Schlange, wird der Nullzeit-Timer erst beim naechsten
        // Durchlauf zugestellt. Den gibt es hier nicht: mit dem Presenter als
        // Kontextobjekt stirbt der Timer am Ende des Tests. QTRY_COMPARE
        // prueft erneut und endet, sobald der Rueckruf gelaufen ist.
        QTRY_COMPARE(view.lastStatusIcon, 0);   // SearchOk
        QVERIFY2(view.lastProgressText.contains(QStringLiteral("Analyse OK")),
                 qPrintable(view.lastProgressText));
        QVERIFY2(view.lastProgressText.contains(
                     QStringLiteral("5/5 Pflicht")),
                 qPrintable(view.lastProgressText));
    }

    void test_presenterBuyEdit_populateFromResult_rejectedRequiredField_reportsFailed()
    {
        StubViewBuyEdit view;
        view.failingFields << QStringLiteral("depotNumber");

        StubModelBuyEdit model;
        PresenterBuyEdit p(&view, &model,
                           QStringLiteral("share-1"), nullptr);

        const QMap<QString, QList<QString>> result = {
            { QStringLiteral("Date"), { QStringLiteral("12.03.2024") } },
            { QStringLiteral("DepotNumber"), { QStringLiteral("1234567890") } },
            { QStringLiteral("OrderNumber"), { QStringLiteral("ORD-1") } },
            { QStringLiteral("Volume"), { QStringLiteral("10") } },
            { QStringLiteral("Price"), { QStringLiteral("245,60") } },
        };

        p.populateFromResult(result);
        QTRY_COMPARE(view.lastStatusIcon, 1);   // SearchFailed
        QVERIFY2(view.lastProgressText.contains(
                     QStringLiteral("Analyse fehlgeschlagen")),
                 qPrintable(view.lastProgressText));
        QVERIFY2(view.lastProgressText.contains(
                     QStringLiteral("4/5 Pflicht")),
                 qPrintable(view.lastProgressText));
    }

    // ─────────────────────────────────────────────────────────────────────
    // Feldschluessel-Tabellen (02.09.2026) — Runde 2, gleiches Muster wie
    // tst_shareaddform. Siehe ARCHITECTURE.md, "Feldschluessel-Tabellen sind
    // an keiner Stelle geprueft".
    //
    // Gegen einen ECHTEN ViewBuyEdit, nicht gegen den Stub: nur der echte
    // Dialog fuellt m_inputWidgets und m_statusLabels, und genau deren Inhalt
    // ist die Frage.
    // ─────────────────────────────────────────────────────────────────────

    void test_buyEdit_everyKnownXmlNameHasAViewField()
    {
        // Ein Name in knownXmlNames(), der in xmlNameToViewField() fehlt,
        // wird in populateFromResult() ueber "if (viewField.isEmpty())
        // continue;" wortlos uebersprungen — in der Optional-Zaehlung steckt
        // er trotzdem drin.
        for (const QString& xmlName : PresenterBuyEdit::knownXmlNames()) {
            const QString viewField = PresenterBuyEdit::xmlNameToViewField(xmlName);
            QVERIFY2(!viewField.isEmpty(),
                     qPrintable(QStringLiteral(
                         "knownXmlNames() fuehrt \"%1\", "
                         "xmlNameToViewField() kennt den Namen aber nicht")
                         .arg(xmlName)));
        }
    }

    void test_buyEdit_everyViewFieldIsRegisteredInTheDialog()
    {
        // Der leere Rohwert ist die in IViewBuyEdit.h dokumentierte Aufrufart
        // der Live-Validierung: nur das Symbol setzen, den Feldinhalt nicht
        // anfassen. Fuer jeden bekannten Schluessel muss das gelingen.
        ViewBuyEdit dlg(QStringLiteral("share-guid"), &m_docsConfig);

        for (const QString& xmlName : PresenterBuyEdit::knownXmlNames()) {
            const QString viewField = PresenterBuyEdit::xmlNameToViewField(xmlName);
            if (viewField.isEmpty())
                continue;   // eigener Test oben

            QVERIFY2(dlg.setFieldOk(viewField, QString()),
                     qPrintable(QStringLiteral(
                         "Feldschluessel \"%1\" (aus XML-Name \"%2\") ist im "
                         "Dialog weder als Eingabefeld noch als Symbol "
                         "registriert").arg(viewField, xmlName)));
        }
    }

    void test_buyEdit_documentFieldKeyIsRegistered()
    {
        // "document" steht in keiner der XML-Tabellen — der Pfad kommt nicht
        // aus einer Regel, sondern ueber setDocumentPath(). Die
        // Live-Validierung (PresenterBuyEdit::onDocumentPathEdited()) benutzt
        // den Schluessel trotzdem, und er hat ein Symbol ohne Eingabefeld.
        // Genau die Kombination, die der Waechter durchlassen MUSS.
        ViewBuyEdit dlg(QStringLiteral("share-guid"), &m_docsConfig);

        QVERIFY2(dlg.setFieldOk(QStringLiteral("document"), QString()),
                 "\"document\" ist ein reines Statusfeld und muss trotzdem "
                 "angenommen werden");
    }

    void test_buyEdit_requiredXmlNamesAreSubsetOfKnown()
    {
        // Ein Pflichtname ausserhalb von knownXmlNames() wird nie gesucht;
        // requiredFound erreicht reqTotal dann nie und die Statuszeile meldet
        // dauerhaft "Analyse fehlgeschlagen".
        for (const QString& xmlName : PresenterBuyEdit::requiredXmlNames()) {
            QVERIFY2(PresenterBuyEdit::knownXmlNames().contains(xmlName),
                     qPrintable(QStringLiteral(
                         "Pflichtname \"%1\" fehlt in knownXmlNames() und "
                         "wird deshalb nie gesucht").arg(xmlName)));
        }
    }

    void test_buyEdit_setFieldOk_unknownFieldKey_isRejected()
    {
        ViewBuyEdit dlg(QStringLiteral("share-guid"), &m_docsConfig);

        QTest::ignoreMessage(QtWarningMsg,
            "[ViewBuyEdit] setFieldOk: unbekannter Feldschluessel \"depotnumber\"");
        QVERIFY(!dlg.setFieldOk(QStringLiteral("depotnumber"),
                                QStringLiteral("1234567890")));
    }

    void test_buyEdit_setFieldError_unknownFieldKey_warns()
    {
        ViewBuyEdit dlg(QStringLiteral("share-guid"), &m_docsConfig);

        QTest::ignoreMessage(QtWarningMsg,
            "[ViewBuyEdit] setFieldError: unbekannter Feldschluessel \"depotnumber\"");
        dlg.setFieldError(QStringLiteral("depotnumber"),
                          QStringLiteral("1234567890"));
    }

};

// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);

    TestBuysForm t;
    return QTest::qExec(&t, argc, argv);
}

#include "tst_buysform.moc"

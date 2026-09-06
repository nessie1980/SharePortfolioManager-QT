// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// tst_brokeragesform.cpp - Unit-Tests fuer ModelBrokerageEdit,
// PresenterBrokerageEdit und ViewBrokerageEdit.
//
// Aus tst_mainwindow.cpp ausgelagert (26.08.2026), damit jede Form ihre eigene
// Executable hat - gleiches Muster wie die Auslagerungen von tst_buysform,
// tst_salesform und tst_dividendform. Der Block war in tst_mainwindow.cpp
// zusammenhaengend und ist unveraendert uebernommen; MainWindow selbst wird
// von keinem dieser Tests gebraucht, weshalb MainWindow.cpp hier nicht mehr
// Compile-Abhaengigkeit ist.
//
// Die Helfer openMemoryDb()/insertTestShare()/loadSandboxedSettings() sind
// bewusst dupliziert statt in einen gemeinsamen Header gezogen: eine geteilte
// Test-Basis haette jedes Test-Target an jede Aenderung eines anderen
// gekoppelt (siehe TESTING.md, "Auslagerung der Form-Tests").

#include <QtTest>
#include <QSignalSpy>
#include <QApplication>
#include <QTemporaryDir>
#include <QLocale>
#include <QUuid>
#include <QDate>
#include <QDateEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>

#include "../../app/config/AppSettings.h"
#include "../../app/core/Database.h"
#include "../../app/repositories/ShareRepository.h"
#include "../../app/models/ShareObject.h"
#include "../../app/models/BrokerageObject.h"
#include "../../app/widgets/OverviewTabWidget.h"

#include "../../app/forms/BrokeragesForm/IViewBrokerageEdit.h"
#include "../../app/forms/BrokeragesForm/IModelBrokerageEdit.h"
#include "../../app/forms/BrokeragesForm/ViewBrokerageEdit.h"
#include "../../app/forms/BrokeragesForm/ModelBrokerageEdit.h"
#include "../../app/forms/BrokeragesForm/PresenterBrokerageEdit.h"

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

// -----------------------------------------------------------------------------
// TestBrokeragesForm
// -----------------------------------------------------------------------------

class TestBrokeragesForm : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;

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

    /**
     * Liefert die Daten-QTableWidget, die OverviewTabWidget als Property
     * "dataTable" am Tab-Container ablegt. Mit den populateOverview()-Tests
     * aus tst_mainwindow.cpp uebernommen (26.08.2026) und wie die uebrigen
     * Helfer bewusst dupliziert statt geteilt.
     */
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

private slots:

    void initTestCase()
    {
        QVERIFY(m_tempDir.isValid());
        loadSandboxedSettings();
    }

    void cleanupTestCase()
    {
        if (Database::instance().isOpen())
            Database::instance().close();
        // WICHTIG: hier bewusst KEIN AppSettings::instance().load(...) - das
        // wuerde den Singleton auf die echte settings.ini umlenken und Nessies
        // reale Konfiguration ueberschreiben (Bugfix 19.07.2026, siehe
        // TESTING.md). Der Singleton stirbt ohnehin mit dem Prozess.
    }

    void init()
    {
        loadSandboxedSettings();
        if (Database::instance().isOpen())
            Database::instance().close();
    }

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

    /**
     * @brief Was loadBrokerage() ins Feld schreibt, muss zurückgelesen werden.
     *
     * Regression 06.09.2026: die Gebührenfelder werden über formatMoney()
     * befüllt, also MIT Tausendertrennzeichen, und die alte parseDouble()
     * lieferte dafür 0,0. Bei den Kosten fällt das besonders unangenehm auf,
     * weil 0,00 € dort ein völlig gültiger Wert ist — es gab keinerlei
     * Anzeichen, dass etwas verloren ging. Siehe ARCHITECTURE.md,
     * "Zahlenfelder verlieren Werte ab 1.000 beim Zurücklesen".
     */
    void test_viewBrokerageEdit_loadBrokerage_fourDigitValuesSurviveReadBack()
    {
        openMemoryDb();
        ViewBrokerageEdit dlg(QStringLiteral("share-guid"));

        // Gleiche Konstruktorform wie beim Sentinel-Test weiter oben, nur mit
        // einer Provision jenseits der Tausendermarke.
        BrokerageObject br(QStringLiteral("brok-roundtrip"),
                           QStringLiteral("share-guid"),
                           QString(), QString(),
                           QStringLiteral("2024-06-15T10:00:00"),
                           1234.56);
        dlg.loadBrokerage(br);

        QCOMPARE(dlg.provision(), 1234.56);
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
}; // end of TestBrokeragesForm

int main(int argc, char* argv[])
{
    // Bugfix 23.07.2026 - siehe ARCHITECTURE.md, "System-Locale-abhaengiges
    // Zahlenformat": muss vor jeder QLocale()-Verwendung gesetzt werden, damit
    // formatMoney() auf jedem Runner deutsch formatiert.
    QLocale::setDefault(QLocale::German);

    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);

    TestBrokeragesForm t;
    return QTest::qExec(&t, argc, argv);
}

#include "tst_brokeragesform.moc"

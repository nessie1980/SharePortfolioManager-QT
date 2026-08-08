// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// tst_sharesplitsform.cpp — Unit tests for ViewShareSplitEdit,
// ModelShareSplitEdit und PresenterShareSplitEdit.
//
// Phase 3 der Aktiensplit-Behandlung (08.08.2026), siehe ARCHITECTURE.md,
// "ShareSplitsForm-Details". Eigene Executable analog tst_buysform /
// tst_shareeditform — eine je Form.

#include <QtTest>
#include <QApplication>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QCheckBox>
#include <QDateEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>
#include <QLocale>
#include <QUuid>

#include "../../app/config/AppSettings.h"
#include "../../app/core/Database.h"
#include "../../app/models/ShareObject.h"
#include "../../app/models/BuyObject.h"
#include "../../app/models/ShareSplitObject.h"
#include "../../app/repositories/ShareRepository.h"
#include "../../app/repositories/BuyRepository.h"
#include "../../app/repositories/ShareSplitRepository.h"

#include "../../app/forms/ShareSplitsForm/IViewShareSplitEdit.h"
#include "../../app/forms/ShareSplitsForm/IModelShareSplitEdit.h"
#include "../../app/forms/ShareSplitsForm/ModelShareSplitEdit.h"
#include "../../app/forms/ShareSplitsForm/PresenterShareSplitEdit.h"
#include "../../app/forms/ShareSplitsForm/ViewShareSplitEdit.h"
#include "../../app/widgets/DocumentPreviewPanel.h"

namespace {
const QString kShareGuid = QStringLiteral("share-guid");

ShareSplitObject makeSplit(const QString& guid, const QDate& date,
                           double ratioNew, double ratioOld,
                           bool pricesAdjusted = false,
                           const QString& document = QString())
{
    return ShareSplitObject(guid, kShareGuid, date, ratioNew, ratioOld,
                            pricesAdjusted, QString(), document);
}
}

// ─────────────────────────────────────────────────────────────────────────────
// Stub IViewShareSplitEdit
//
// confirm() liefert einen vorgegebenen Wert, statt einen modalen Dialog zu
// öffnen — genau dafür sitzt die Rückfrage im View-Interface und nicht als
// direkter OwnMessageBox-Aufruf im Presenter.
// ─────────────────────────────────────────────────────────────────────────────
class StubViewShareSplitEdit : public IViewShareSplitEdit
{
public:
    // ── steuerbare Rückgabewerte ──────────────────────────────────────────
    QDate   dateToReturn           = QDate(2022, 7, 18);
    double  ratioNewToReturn       = 20.0;
    double  ratioOldToReturn       = 1.0;
    bool    pricesAdjustedToReturn = false;
    QString commentToReturn;
    QString documentPathToReturn;
    bool    confirmResult          = true;

    // ── Aufzeichnung ──────────────────────────────────────────────────────
    bool    loadSplitCalled   = false;
    bool    clearFormCalled   = false;
    bool    closedCalled      = false;
    bool    confirmCalled     = false;
    QString lastConfirmMessage;
    QString lastError;
    QString lastFactorPreview;
    QString lastDocumentPath;
    QString lastPreviewPath;
    bool    clearPdfPreviewCalled = false;
    QList<ShareSplitObject> lastOverview;
    bool    lastCanRemove = false;
    bool    lastIsEdit    = false;

    QDate   splitDate()      const override { return dateToReturn; }
    double  ratioNew()       const override { return ratioNewToReturn; }
    double  ratioOld()       const override { return ratioOldToReturn; }
    bool    pricesAdjusted() const override { return pricesAdjustedToReturn; }
    QString comment()        const override { return commentToReturn; }
    QString documentPath()   const override { return documentPathToReturn; }

    void loadSplit(const ShareSplitObject&) override { loadSplitCalled = true; }
    void clearForm()                        override { clearFormCalled = true; }
    void setFactorPreview(const QString& text) override { lastFactorPreview = text; }

    void setDocumentPath(const QString& path) override
        { lastDocumentPath = path; documentPathToReturn = path; }

    void openPdfPreview(const QString& path) override { lastPreviewPath = path; }
    void clearPdfPreview()                   override { clearPdfPreviewCalled = true; }

    void populateOverview(const QList<ShareSplitObject>& splits) override
        { lastOverview = splits; }

    void setButtonStates(bool canRemove, bool isEdit) override
        { lastCanRemove = canRemove; lastIsEdit = isEdit; }

    void showError(const QString& message) override { lastError = message; }

    bool confirm(const QString&, const QString& message) override
        { confirmCalled = true; lastConfirmMessage = message; return confirmResult; }

    void acceptAndClose() override { closedCalled = true; }
};

// ─────────────────────────────────────────────────────────────────────────────
// Stub IModelShareSplitEdit
// ─────────────────────────────────────────────────────────────────────────────
class StubModelShareSplitEdit : public IModelShareSplitEdit
{
public:
    QList<ShareSplitObject> splits;
    QList<OpenBuyLot>       lots;
    bool addResult    = true;
    bool updateResult = true;
    bool removeResult = true;
    bool dateExists     = false;
    bool documentInUse  = false;
    QString errorMsg  = QStringLiteral("Fehler");

    bool addCalled    = false;
    bool updateCalled = false;
    bool removeCalled = false;
    ShareSplitObject lastSaved;

    QList<ShareSplitObject> loadSplits(const QString&) const override { return splits; }

    bool existsForDate(const QString&, const QDate&) const override { return dateExists; }

    mutable QString lastDocumentExistsExclude;

    bool documentExists(const QString&, const QString& excludeGuid) const override
        { lastDocumentExistsExclude = excludeGuid; return documentInUse; }

    QList<OpenBuyLot> openLots(const QString&) const override { return lots; }

    bool addSplit(const ShareSplitObject& split) override
        { addCalled = true; lastSaved = split; return addResult; }

    bool updateSplit(const ShareSplitObject& split) override
        { updateCalled = true; lastSaved = split; return updateResult; }

    bool removeSplit(const QString&) override
        { removeCalled = true; return removeResult; }

    QString lastError() const override { return errorMsg; }
};

// ─────────────────────────────────────────────────────────────────────────────
// TestShareSplitsForm
// ─────────────────────────────────────────────────────────────────────────────
class TestShareSplitsForm : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;

    void loadSandboxedSettings()
    {
        AppSettings::instance().load(m_tempDir.path() + QStringLiteral("/test_settings.ini"));
    }

    void openMemoryDb()
    {
        if (!Database::instance().isOpen())
            Database::instance().open(QStringLiteral(":memory:"));
        AppSettings::instance().setPortfolioPath(QStringLiteral(":memory:"));
    }

    /** Legt eine minimale Aktie an und liefert ihre GUID. */
    QString insertTestShare()
    {
        const QString guid = QUuid::createUuid().toString(QUuid::WithoutBraces);
        ShareRepository repo;
        repo.insert(ShareObject(guid, QStringLiteral("TSTSHRE"),
                                QStringLiteral("DE000TST0001"),
                                QStringLiteral("Test AG")));
        return guid;
    }

    /** Legt einen Split mit Beleg an und liefert dessen GUID. */
    QString insertSplitWithDocument(const QString& shareGuid,
                                    ModelShareSplitEdit& model,
                                    const QDate& date,
                                    const QString& document)
    {
        const QString splitGuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
        model.addSplit(ShareSplitObject(splitGuid, shareGuid, date, 20.0, 1.0,
                                        false, QString(), document));
        return splitGuid;
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
        AppSettings::instance().load(QString());
    }

    void init()
    {
        loadSandboxedSettings();
        if (Database::instance().isOpen())
            Database::instance().close();
    }

    // ─────────────────────────────────────────────────────────────────────
    // PresenterShareSplitEdit — Konstruktion
    // ─────────────────────────────────────────────────────────────────────

    void test_presenter_populatesOverviewOnConstruction()
    {
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        model.splits << makeSplit(QStringLiteral("s1"), QDate(2022, 7, 18), 20.0, 1.0);

        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        QCOMPARE(view.lastOverview.size(), 1);
        QCOMPARE(view.lastOverview.first().guid(), QStringLiteral("s1"));
    }

    void test_presenter_startsInAddModeWithRemoveDisabled()
    {
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        QVERIFY(!view.lastCanRemove);
        QVERIFY(!view.lastIsEdit);
    }

    void test_presenter_setsFactorPreviewOnConstruction()
    {
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        QVERIFY(!view.lastFactorPreview.isEmpty());
        QVERIFY2(view.lastFactorPreview.contains(QStringLiteral("20")),
                 qPrintable(view.lastFactorPreview));
    }

    void test_presenter_factorPreview_reverseSplitUsesSingular()
    {
        // Reverse-Split 1:10 — "aus 10 Stk. wird 1 Stk.", nicht "werden".
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        view.ratioNewToReturn = 1.0;
        view.ratioOldToReturn = 10.0;
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        QVERIFY2(view.lastFactorPreview.contains(QStringLiteral("wird 1")),
                 qPrintable(view.lastFactorPreview));
    }

    void test_presenter_factorPreview_invalidRatioShowsDash()
    {
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        view.ratioNewToReturn = 0.0;
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        QCOMPARE(view.lastFactorPreview, QStringLiteral("-"));
    }

    // ─────────────────────────────────────────────────────────────────────
    // onSave — Validierung
    // ─────────────────────────────────────────────────────────────────────

    void test_presenter_onSave_validSplit_callsAdd()
    {
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        p.onSave();

        QVERIFY(model.addCalled);
        QVERIFY(!model.updateCalled);
        QVERIFY(view.lastError.isEmpty());
        QCOMPARE(model.lastSaved.ratioNew(), 20.0);
        QCOMPARE(model.lastSaved.ratioOld(), 1.0);
        QCOMPARE(model.lastSaved.shareGuid(), kShareGuid);
    }

    void test_presenter_onSave_emitsDataChanged()
    {
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        PresenterShareSplitEdit p(&view, &model, kShareGuid);
        QSignalSpy spy(&p, &PresenterShareSplitEdit::dataChanged);

        p.onSave();

        QCOMPARE(spy.count(), 1);
    }

    void test_presenter_onSave_sentinelDate_showsErrorAndDoesNotSave()
    {
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        view.dateToReturn = QDate(2000, 1, 1);   // Sentinel = "nicht gesetzt"
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        p.onSave();

        QVERIFY(!model.addCalled);
        QVERIFY(!view.lastError.isEmpty());
    }

    void test_presenter_onSave_futureDate_isAllowed()
    {
        // Nessies Entscheidung 08.08.2026: ein angekündigter Split darf sofort
        // erfasst werden. Technisch folgenlos, da volumeFactor() nur
        // Datensätze VOR dem Splittag umrechnet.
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        view.dateToReturn = QDate::currentDate().addYears(1);
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        p.onSave();

        QVERIFY(model.addCalled);
        QVERIFY(view.lastError.isEmpty());
    }

    void test_presenter_onSave_zeroRatio_showsErrorAndDoesNotSave()
    {
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        view.ratioOldToReturn = 0.0;
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        p.onSave();

        QVERIFY(!model.addCalled);
        QVERIFY(!view.lastError.isEmpty());
    }

    void test_presenter_onSave_ratioOneToOne_isRejected()
    {
        // Faktor 1,0 ist fachlich kein Split (Nessies Entscheidung 08.08.2026)
        // und würde nur in jeder Berechnung mitlaufen, ohne etwas zu bewirken.
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        view.ratioNewToReturn = 1.0;
        view.ratioOldToReturn = 1.0;
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        p.onSave();

        QVERIFY(!model.addCalled);
        QVERIFY(!view.lastError.isEmpty());
    }

    void test_presenter_onSave_equivalentRatio_isAlsoRejected()
    {
        // 2:2 ist ebenfalls Faktor 1,0 — die Prüfung darf nicht nur auf die
        // wörtliche Eingabe "1:1" schauen.
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        view.ratioNewToReturn = 2.0;
        view.ratioOldToReturn = 2.0;
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        p.onSave();

        QVERIFY(!model.addCalled);
        QVERIFY(!view.lastError.isEmpty());
    }

    void test_presenter_onSave_duplicateDate_showsErrorAndDoesNotSave()
    {
        // Vorgriff auf UNIQUE(share_guid, date) — der Presenter fängt den Fall
        // ab, bevor SQLite ihn als Fehler zurückmeldet.
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        model.dateExists = true;
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        p.onSave();

        QVERIFY(!model.addCalled);
        QVERIFY(!view.lastError.isEmpty());
    }

    void test_presenter_onSave_modelFails_showsError()
    {
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        model.addResult = false;
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        p.onSave();

        QCOMPARE(view.lastError, model.errorMsg);
    }

    // ─────────────────────────────────────────────────────────────────────
    // Bearbeiten eines bestehenden Splits
    // ─────────────────────────────────────────────────────────────────────

    void test_presenter_onRowSelected_loadsSplitAndEnablesRemove()
    {
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        model.splits << makeSplit(QStringLiteral("s1"), QDate(2022, 7, 18), 20.0, 1.0);
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        p.onRowSelected(QStringLiteral("s1"));

        QVERIFY(view.loadSplitCalled);
        QVERIFY(view.lastCanRemove);   // jeder Split ist jederzeit löschbar
        QVERIFY(view.lastIsEdit);
    }

    void test_presenter_onRowSelected_olderSplitIsAlsoRemovable()
    {
        // Keine Letzter-Eintrag-Sperre, analog DividendForm/BrokeragesForm.
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        model.splits << makeSplit(QStringLiteral("s-old"), QDate(2014, 4, 3),  2.0,  1.0);
        model.splits << makeSplit(QStringLiteral("s-new"), QDate(2022, 7, 18), 20.0, 1.0);
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        p.onRowSelected(QStringLiteral("s-old"));

        QVERIFY(view.lastCanRemove);
    }

    void test_presenter_onRowSelected_emptyGuid_resetsForm()
    {
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        PresenterShareSplitEdit p(&view, &model, kShareGuid);
        view.clearFormCalled = false;

        p.onRowSelected(QString());

        QVERIFY(view.clearFormCalled);
    }

    void test_presenter_onSave_existingSplit_callsUpdate()
    {
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        model.splits << makeSplit(QStringLiteral("s1"), QDate(2022, 7, 18), 20.0, 1.0);
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        p.onRowSelected(QStringLiteral("s1"));
        p.onSave();

        QVERIFY(model.updateCalled);
        QVERIFY(!model.addCalled);
        // GUID muss erhalten bleiben, sonst entstünde ein zweiter Datensatz.
        QCOMPARE(model.lastSaved.guid(), QStringLiteral("s1"));
    }

    void test_presenter_onSave_existingSplit_unchangedDateIsNoDuplicate()
    {
        // Beim Bearbeiten zählt das eigene, unveränderte Datum nicht als
        // Duplikat — sonst liesse sich an einem Split nur noch das Datum ändern.
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        model.splits << makeSplit(QStringLiteral("s1"), QDate(2022, 7, 18), 20.0, 1.0);
        model.dateExists = true;
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        p.onRowSelected(QStringLiteral("s1"));
        view.dateToReturn = QDate(2022, 7, 18);   // unverändert
        view.ratioNewToReturn = 10.0;             // nur das Verhältnis korrigiert
        p.onSave();

        QVERIFY(model.updateCalled);
        QVERIFY(view.lastError.isEmpty());
    }

    void test_presenter_onSave_existingSplit_changedDateToOccupiedDay_isRejected()
    {
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        model.splits << makeSplit(QStringLiteral("s1"), QDate(2022, 7, 18), 20.0, 1.0);
        model.dateExists = true;
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        p.onRowSelected(QStringLiteral("s1"));
        view.dateToReturn = QDate(2014, 4, 3);    // auf einen belegten Tag verschoben
        p.onSave();

        QVERIFY(!model.updateCalled);
        QVERIFY(!view.lastError.isEmpty());
    }

    // ─────────────────────────────────────────────────────────────────────
    // Dokument (08.08.2026)
    // ─────────────────────────────────────────────────────────────────────

    void test_presenter_onSave_storesDocumentPath()
    {
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        view.documentPathToReturn = QStringLiteral("/belege/split.pdf");
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        p.onSave();

        QCOMPARE(model.lastSaved.document(), QStringLiteral("/belege/split.pdf"));
    }

    void test_presenter_onSave_trimsDocumentPath()
    {
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        view.documentPathToReturn = QStringLiteral("  /belege/split.pdf  ");
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        p.onSave();

        QCOMPARE(model.lastSaved.document(), QStringLiteral("/belege/split.pdf"));
    }

    void test_presenter_onDocumentSelected_setsPathAndPreview()
    {
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        p.onDocumentSelected(QStringLiteral("/belege/split.pdf"));

        QCOMPARE(view.lastDocumentPath, QStringLiteral("/belege/split.pdf"));
        QCOMPARE(view.lastPreviewPath,  QStringLiteral("/belege/split.pdf"));
    }

    void test_presenter_onDocumentPathEdited_duplicateShowsHint()
    {
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        model.documentInUse = true;
        view.documentPathToReturn = QStringLiteral("/belege/split.pdf");
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        p.onDocumentPathEdited();

        QVERIFY(!view.lastError.isEmpty());
    }

    void test_presenter_onDocumentPathEdited_duplicateDoesNotBlockSaving()
    {
        // Bewusst nur ein Hinweis, keine Blockade — zwei Splits können
        // legitim auf derselben Bankmitteilung stehen.
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        model.documentInUse = true;
        view.documentPathToReturn = QStringLiteral("/belege/split.pdf");
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        p.onDocumentPathEdited();
        p.onSave();

        QVERIFY(model.addCalled);
    }

    void test_presenter_onDocumentPathEdited_emptyPath_noCheck()
    {
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        model.documentInUse = true;   // würde melden, wenn überhaupt geprüft würde
        PresenterShareSplitEdit p(&view, &model, kShareGuid);
        view.lastError.clear();

        p.onDocumentPathEdited();

        QVERIFY(view.lastError.isEmpty());
    }

    void test_presenter_onDocumentPathEdited_excludesLoadedSplit()
    {
        // Beim Bearbeiten muss die eigene GUID ausgenommen werden, sonst
        // meldete jeder geladene Split sein eigenes Dokument als Konflikt.
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        model.splits << makeSplit(QStringLiteral("s1"), QDate(2022, 7, 18), 20.0, 1.0,
                                  false, QStringLiteral("/belege/split.pdf"));
        view.documentPathToReturn = QStringLiteral("/belege/split.pdf");
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        p.onRowSelected(QStringLiteral("s1"));
        p.onDocumentPathEdited();

        QCOMPARE(model.lastDocumentExistsExclude, QStringLiteral("s1"));
    }

    void test_presenter_onRowSelected_withDocument_opensPreview()
    {
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        model.splits << makeSplit(QStringLiteral("s1"), QDate(2022, 7, 18), 20.0, 1.0,
                                  false, QStringLiteral("/belege/split.pdf"));
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        p.onRowSelected(QStringLiteral("s1"));

        QCOMPARE(view.lastPreviewPath, QStringLiteral("/belege/split.pdf"));
    }

    void test_presenter_onRowSelected_withoutDocument_clearsPreview()
    {
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        model.splits << makeSplit(QStringLiteral("s1"), QDate(2022, 7, 18), 20.0, 1.0);
        PresenterShareSplitEdit p(&view, &model, kShareGuid);
        view.clearPdfPreviewCalled = false;

        p.onRowSelected(QStringLiteral("s1"));

        QVERIFY(view.clearPdfPreviewCalled);
    }

    void test_presenter_onReset_clearsPreview()
    {
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        PresenterShareSplitEdit p(&view, &model, kShareGuid);
        view.clearPdfPreviewCalled = false;

        p.onReset();

        QVERIFY(view.clearPdfPreviewCalled);
    }

    // ─────────────────────────────────────────────────────────────────────
    // onRemove — Rückfrage und Löschfolgen
    // ─────────────────────────────────────────────────────────────────────

    void test_presenter_onRemove_withoutSelection_doesNothing()
    {
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        p.onRemove();

        QVERIFY(!model.removeCalled);
        QVERIFY(!view.confirmCalled);
    }

    void test_presenter_onRemove_asksForConfirmation()
    {
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        model.splits << makeSplit(QStringLiteral("s1"), QDate(2022, 7, 18), 20.0, 1.0);
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        p.onRowSelected(QStringLiteral("s1"));
        p.onRemove();

        QVERIFY(view.confirmCalled);
        QVERIFY(model.removeCalled);
    }

    void test_presenter_onRemove_declinedConfirmation_doesNotRemove()
    {
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        model.splits << makeSplit(QStringLiteral("s1"), QDate(2022, 7, 18), 20.0, 1.0);
        view.confirmResult = false;
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        p.onRowSelected(QStringLiteral("s1"));
        p.onRemove();

        QVERIFY(view.confirmCalled);
        QVERIFY(!model.removeCalled);
    }

    void test_presenter_onRemove_confirmationNamesTheSplit()
    {
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        model.splits << makeSplit(QStringLiteral("s1"), QDate(2022, 7, 18), 20.0, 1.0);
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        p.onRowSelected(QStringLiteral("s1"));
        p.onRemove();

        QVERIFY2(view.lastConfirmMessage.contains(QStringLiteral("20:1")),
                 qPrintable(view.lastConfirmMessage));
    }

    void test_presenter_onRemove_confirmationShowsVolumeChange()
    {
        // 100 Stück laut Beleg, gekauft vor einem 20:1-Split: heute 2.000,
        // ohne den Split wieder 100. Beide Zahlen müssen in der Rückfrage
        // stehen, damit die Tragweite sichtbar wird.
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        model.splits << makeSplit(QStringLiteral("s1"), QDate(2022, 7, 18), 20.0, 1.0);
        model.lots << OpenBuyLot{ QDate(2021, 1, 15), 100.0 };
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        p.onRowSelected(QStringLiteral("s1"));
        p.onRemove();

        const QLocale loc;
        QVERIFY2(view.lastConfirmMessage.contains(loc.toString(2000.0, 'f', 4)),
                 qPrintable(view.lastConfirmMessage));
        QVERIFY2(view.lastConfirmMessage.contains(loc.toString(100.0, 'f', 4)),
                 qPrintable(view.lastConfirmMessage));
    }

    void test_presenter_onRemove_lotAfterSplitIsUnaffected()
    {
        // Ein Kauf NACH dem Splittag liegt bereits in heutiger Skala — sein
        // Bestand darf sich beim Löschen des Splits nicht ändern.
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        model.splits << makeSplit(QStringLiteral("s1"), QDate(2022, 7, 18), 20.0, 1.0);
        model.lots << OpenBuyLot{ QDate(2023, 3, 1), 50.0 };
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        p.onRowSelected(QStringLiteral("s1"));
        p.onRemove();

        const QString fifty = QLocale().toString(50.0, 'f', 4);
        // Beide Seiten des Vergleichs sind 50 — die Zahl muss zweimal auftauchen.
        QVERIFY2(view.lastConfirmMessage.count(fifty) >= 2,
                 qPrintable(view.lastConfirmMessage));
    }

    void test_presenter_onRemove_emitsDataChanged()
    {
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        model.splits << makeSplit(QStringLiteral("s1"), QDate(2022, 7, 18), 20.0, 1.0);
        PresenterShareSplitEdit p(&view, &model, kShareGuid);
        p.onRowSelected(QStringLiteral("s1"));

        QSignalSpy spy(&p, &PresenterShareSplitEdit::dataChanged);
        p.onRemove();

        QCOMPARE(spy.count(), 1);
    }

    void test_presenter_onRemove_modelFails_showsError()
    {
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        model.splits << makeSplit(QStringLiteral("s1"), QDate(2022, 7, 18), 20.0, 1.0);
        model.removeResult = false;
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        p.onRowSelected(QStringLiteral("s1"));
        p.onRemove();

        QCOMPARE(view.lastError, model.errorMsg);
    }

    // ─────────────────────────────────────────────────────────────────────
    // onReset / onClose
    // ─────────────────────────────────────────────────────────────────────

    void test_presenter_onReset_clearsFormAndButtonStates()
    {
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        model.splits << makeSplit(QStringLiteral("s1"), QDate(2022, 7, 18), 20.0, 1.0);
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        p.onRowSelected(QStringLiteral("s1"));
        view.clearFormCalled = false;
        p.onReset();

        QVERIFY(view.clearFormCalled);
        QVERIFY(!view.lastCanRemove);
        QVERIFY(!view.lastIsEdit);
    }

    void test_presenter_onReset_forgetsSelection()
    {
        // Nach onReset() muss ein onSave() wieder anlegen statt zu aktualisieren.
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        model.splits << makeSplit(QStringLiteral("s1"), QDate(2022, 7, 18), 20.0, 1.0);
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        p.onRowSelected(QStringLiteral("s1"));
        p.onReset();
        view.dateToReturn = QDate(2014, 4, 3);
        p.onSave();

        QVERIFY(model.addCalled);
        QVERIFY(!model.updateCalled);
    }

    void test_presenter_onClose_closesView()
    {
        StubViewShareSplitEdit  view;
        StubModelShareSplitEdit model;
        PresenterShareSplitEdit p(&view, &model, kShareGuid);

        p.onClose();

        QVERIFY(view.closedCalled);
    }

    // ─────────────────────────────────────────────────────────────────────
    // ModelShareSplitEdit — gegen die echte In-Memory-Datenbank
    // ─────────────────────────────────────────────────────────────────────

    void test_model_addAndLoadSplit_roundTrip()
    {
        openMemoryDb();
        const QString shareGuid = insertTestShare();
        ModelShareSplitEdit model;

        const QString splitGuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QVERIFY(model.addSplit(ShareSplitObject(splitGuid, shareGuid,
                                                QDate(2022, 7, 18), 20.0, 1.0,
                                                false, QStringLiteral("Alphabet"))));

        const QList<ShareSplitObject> splits = model.loadSplits(shareGuid);
        QCOMPARE(splits.size(), 1);
        QCOMPARE(splits.first().ratioNew(), 20.0);
        QCOMPARE(splits.first().comment(), QStringLiteral("Alphabet"));
    }

    void test_model_loadSplits_orderedByDateAscending()
    {
        openMemoryDb();
        const QString shareGuid = insertTestShare();
        ModelShareSplitEdit model;

        // Bewusst in umgekehrter Reihenfolge eingefügt.
        model.addSplit(ShareSplitObject(QUuid::createUuid().toString(QUuid::WithoutBraces),
                                        shareGuid, QDate(2022, 7, 18), 20.0, 1.0));
        model.addSplit(ShareSplitObject(QUuid::createUuid().toString(QUuid::WithoutBraces),
                                        shareGuid, QDate(2014, 4, 3), 2.0, 1.0));

        const QList<ShareSplitObject> splits = model.loadSplits(shareGuid);
        QCOMPARE(splits.size(), 2);
        QCOMPARE(splits.first().date(), QDate(2014, 4, 3));
        QCOMPARE(splits.last().date(),  QDate(2022, 7, 18));
    }

    void test_model_existsForDate_findsInsertedSplit()
    {
        openMemoryDb();
        const QString shareGuid = insertTestShare();
        ModelShareSplitEdit model;
        model.addSplit(ShareSplitObject(QUuid::createUuid().toString(QUuid::WithoutBraces),
                                        shareGuid, QDate(2022, 7, 18), 20.0, 1.0));

        QVERIFY(model.existsForDate(shareGuid, QDate(2022, 7, 18)));
        QVERIFY(!model.existsForDate(shareGuid, QDate(2022, 7, 19)));
    }

    void test_model_updateSplit_changesStoredValues()
    {
        openMemoryDb();
        const QString shareGuid = insertTestShare();
        ModelShareSplitEdit model;

        const QString splitGuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
        model.addSplit(ShareSplitObject(splitGuid, shareGuid,
                                        QDate(2022, 7, 18), 20.0, 1.0));

        QVERIFY(model.updateSplit(ShareSplitObject(splitGuid, shareGuid,
                                                   QDate(2022, 7, 18), 10.0, 1.0, true)));

        const QList<ShareSplitObject> splits = model.loadSplits(shareGuid);
        QCOMPARE(splits.size(), 1);
        QCOMPARE(splits.first().ratioNew(), 10.0);
        QVERIFY(splits.first().pricesAdjusted());
    }

    void test_model_removeSplit_deletesRow()
    {
        openMemoryDb();
        const QString shareGuid = insertTestShare();
        ModelShareSplitEdit model;

        const QString splitGuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
        model.addSplit(ShareSplitObject(splitGuid, shareGuid,
                                        QDate(2022, 7, 18), 20.0, 1.0));

        QVERIFY(model.removeSplit(splitGuid));
        QVERIFY(model.loadSplits(shareGuid).isEmpty());
    }

    void test_model_removeSplit_leavesBuysUntouched()
    {
        // Kernaussage der Beleg-Wahrheit: ein gelöschter Split rührt die
        // Transaktionen nicht an — er war nur eine Rechenvorschrift.
        openMemoryDb();
        const QString shareGuid = insertTestShare();

        BuyRepository buyRepo;
        buyRepo.insert(BuyObject(QUuid::createUuid().toString(QUuid::WithoutBraces),
                                 shareGuid, QStringLiteral("depot1"), QStringLiteral("ord-1"),
                                 QStringLiteral("2021-01-15T10:00:00"),
                                 100.0, 0.0, 50.0));

        ModelShareSplitEdit model;
        const QString splitGuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
        model.addSplit(ShareSplitObject(splitGuid, shareGuid,
                                        QDate(2022, 7, 18), 20.0, 1.0));
        QVERIFY(model.removeSplit(splitGuid));

        const QList<BuyObject> buys = buyRepo.findByShare(shareGuid);
        QCOMPARE(buys.size(), 1);
        QCOMPARE(buys.first().volume(), 100.0);   // unverändert Beleg-Stückzahl
        QCOMPARE(buys.first().price(),  50.0);
    }

    // documentExists() sitzt in ModelShareSplitEdit, nicht im Repository —
    // dieselbe Platzierung wie ModelBuyEdit/ModelSaleEdit/ModelDividendEdit/
    // ModelBrokerageEdit (Nessies Entscheidung 08.08.2026). Die Tests dazu
    // stehen deshalb hier und nicht in tst_sharesplitrepository.cpp.

    void test_model_documentExists_findsAssignedDocument()
    {
        // Der Fall, der den NULL-Fallstrick vom 08.08.2026 aufgedeckt hat:
        // ohne excludeGuid muss ein vergebener Beleg gefunden werden.
        openMemoryDb();
        const QString shareGuid = insertTestShare();
        ModelShareSplitEdit model;
        insertSplitWithDocument(shareGuid, model, QDate(2022, 7, 18),
                                QStringLiteral("/belege/a.pdf"));

        QVERIFY(model.documentExists(QStringLiteral("/belege/a.pdf")));
    }

    void test_model_documentExists_unknownDocument_returnsFalse()
    {
        openMemoryDb();
        const QString shareGuid = insertTestShare();
        ModelShareSplitEdit model;
        insertSplitWithDocument(shareGuid, model, QDate(2022, 7, 18),
                                QStringLiteral("/belege/a.pdf"));

        QVERIFY(!model.documentExists(QStringLiteral("/belege/b.pdf")));
    }

    void test_model_documentExists_excludesOwnGuid()
    {
        // Beim Bearbeiten darf das eigene Dokument nicht als Konflikt gelten,
        // sonst meldete jede Änderung an einem gespeicherten Split fälschlich
        // eine Doppelbelegung.
        openMemoryDb();
        const QString shareGuid = insertTestShare();
        ModelShareSplitEdit model;
        const QString splitGuid = insertSplitWithDocument(
            shareGuid, model, QDate(2022, 7, 18), QStringLiteral("/belege/a.pdf"));

        QVERIFY(!model.documentExists(QStringLiteral("/belege/a.pdf"), splitGuid));
    }

    void test_model_documentExists_otherSplitStillCounts()
    {
        // Mit excludeGuid gesetzt muss ein FREMDER Split trotzdem gefunden
        // werden — sonst wäre die Ausnahme zu weit gefasst.
        openMemoryDb();
        const QString shareGuid = insertTestShare();
        ModelShareSplitEdit model;
        insertSplitWithDocument(shareGuid, model, QDate(2022, 7, 18),
                                QStringLiteral("/belege/a.pdf"));
        const QString otherGuid = insertSplitWithDocument(
            shareGuid, model, QDate(2023, 5, 2), QStringLiteral("/belege/b.pdf"));

        QVERIFY(model.documentExists(QStringLiteral("/belege/a.pdf"), otherGuid));
    }

    void test_model_documentExists_emptyPath_returnsFalse()
    {
        openMemoryDb();
        const QString shareGuid = insertTestShare();
        ModelShareSplitEdit model;
        insertSplitWithDocument(shareGuid, model, QDate(2022, 7, 18),
                                QStringLiteral("/belege/a.pdf"));

        QVERIFY(!model.documentExists(QString()));
    }

    void test_model_documentExists_trimsPath()
    {
        // Der Pfad wird sowohl für die Leerprüfung als auch für die Abfrage
        // getrimmt — sonst würde mit Leerzeichen gesucht und nichts gefunden.
        openMemoryDb();
        const QString shareGuid = insertTestShare();
        ModelShareSplitEdit model;
        insertSplitWithDocument(shareGuid, model, QDate(2022, 7, 18),
                                QStringLiteral("/belege/a.pdf"));

        QVERIFY(model.documentExists(QStringLiteral("  /belege/a.pdf  ")));
    }

    void test_model_openLots_skipsFullySoldBuys()
    {
        openMemoryDb();
        const QString shareGuid = insertTestShare();

        BuyRepository buyRepo;
        buyRepo.insert(BuyObject(QUuid::createUuid().toString(QUuid::WithoutBraces),
                                 shareGuid, QStringLiteral("depot1"), QStringLiteral("ord-1"),
                                 QStringLiteral("2021-01-15T10:00:00"),
                                 100.0, 100.0, 50.0));   // vollständig verkauft
        buyRepo.insert(BuyObject(QUuid::createUuid().toString(QUuid::WithoutBraces),
                                 shareGuid, QStringLiteral("depot1"), QStringLiteral("ord-2"),
                                 QStringLiteral("2021-06-15T10:00:00"),
                                 40.0, 10.0, 55.0));     // 30 offen

        ModelShareSplitEdit model;
        const QList<OpenBuyLot> lots = model.openLots(shareGuid);

        QCOMPARE(lots.size(), 1);
        QCOMPARE(lots.first().remainingVolume, 30.0);
        QCOMPARE(lots.first().date, QDate(2021, 6, 15));
    }

    // ─────────────────────────────────────────────────────────────────────
    // ViewShareSplitEdit — Widget-Ebene
    // ─────────────────────────────────────────────────────────────────────

    void test_view_canBeConstructed()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareSplitEdit dlg(guid);
        QVERIFY(dlg.windowTitle().contains(QStringLiteral("Split")));
    }

    void test_view_hasAllFormFields()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareSplitEdit dlg(guid);

        QVERIFY(dlg.findChild<QDateEdit*>(QStringLiteral("splitDate")));
        QVERIFY(dlg.findChild<QLineEdit*>(QStringLiteral("ratioNew")));
        QVERIFY(dlg.findChild<QLineEdit*>(QStringLiteral("ratioOld")));
        QVERIFY(dlg.findChild<QLineEdit*>(QStringLiteral("factorPreview")));
        QVERIFY(dlg.findChild<QCheckBox*>(QStringLiteral("pricesAdjusted")));
        QVERIFY(dlg.findChild<QLineEdit*>(QStringLiteral("comment")));
        QVERIFY(dlg.findChild<QTableWidget*>(QStringLiteral("splitsTable")));
    }

    void test_view_hasNoOverviewTabWidget()
    {
        // Bewusste Abweichung von BuysForm/SalesForm/DividendForm: eine Aktie
        // hat typischerweise null bis drei Splits, Jahres-Tabs wären Ballast.
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareSplitEdit dlg(guid);

        QVERIFY(!dlg.findChild<QTabWidget*>());
    }

    void test_view_loadSplit_populatesFields()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareSplitEdit dlg(guid);

        dlg.loadSplit(ShareSplitObject(QStringLiteral("s1"), guid,
                                       QDate(2022, 7, 18), 20.0, 1.0, true,
                                       QStringLiteral("Alphabet")));

        QCOMPARE(dlg.splitDate(), QDate(2022, 7, 18));
        QCOMPARE(dlg.ratioNew(), 20.0);
        QCOMPARE(dlg.ratioOld(), 1.0);
        QVERIFY(dlg.pricesAdjusted());
        QCOMPARE(dlg.comment(), QStringLiteral("Alphabet"));
    }

    void test_view_clearForm_resetsRatioToOne()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareSplitEdit dlg(guid);

        dlg.loadSplit(ShareSplitObject(QStringLiteral("s1"), guid,
                                       QDate(2022, 7, 18), 20.0, 1.0, true,
                                       QStringLiteral("Alphabet")));
        dlg.clearForm();

        QCOMPARE(dlg.ratioNew(), 1.0);
        QCOMPARE(dlg.ratioOld(), 1.0);
        QVERIFY(!dlg.pricesAdjusted());
        QVERIFY(dlg.comment().isEmpty());
    }

    void test_view_populateOverview_fillsTable()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareSplitEdit dlg(guid);

        dlg.populateOverview({ makeSplit(QStringLiteral("s1"), QDate(2014, 4, 3),  2.0,  1.0),
                               makeSplit(QStringLiteral("s2"), QDate(2022, 7, 18), 20.0, 1.0) });

        auto* table = dlg.findChild<QTableWidget*>(QStringLiteral("splitsTable"));
        if (!table) QFAIL("splitsTable not found");
        QCOMPARE(table->rowCount(), 2);
        QCOMPARE(table->item(1, 1)->text(), QStringLiteral("20:1"));
    }

    void test_view_populateOverview_storesGuidPerRow()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareSplitEdit dlg(guid);

        dlg.populateOverview({ makeSplit(QStringLiteral("s1"), QDate(2022, 7, 18), 20.0, 1.0) });

        auto* table = dlg.findChild<QTableWidget*>(QStringLiteral("splitsTable"));
        if (!table) QFAIL("splitsTable not found");
        // GUID hängt an jeder Zelle, damit die Auswahl unabhängig von der
        // angeklickten Spalte auflösbar ist.
        QCOMPARE(table->item(0, 0)->data(Qt::UserRole).toString(), QStringLiteral("s1"));
        QCOMPARE(table->item(0, 4)->data(Qt::UserRole).toString(), QStringLiteral("s1"));
    }

    void test_view_populateOverview_clearsPreviousRows()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareSplitEdit dlg(guid);

        dlg.populateOverview({ makeSplit(QStringLiteral("s1"), QDate(2022, 7, 18), 20.0, 1.0) });
        dlg.populateOverview({});

        auto* table = dlg.findChild<QTableWidget*>(QStringLiteral("splitsTable"));
        if (!table) QFAIL("splitsTable not found");
        QCOMPARE(table->rowCount(), 0);
    }

    void test_view_setButtonStates_editModeRenamesAddButton()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareSplitEdit dlg(guid);

        dlg.setButtonStates(/*canRemove=*/true, /*isEdit=*/true);

        bool hasSave = false;
        for (auto* b : dlg.findChildren<QPushButton*>())
            if (b->text().contains(QStringLiteral("Speichern"))) hasSave = true;
        QVERIFY(hasSave);
    }

    void test_view_setButtonStates_removeDisabledWithoutSelection()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareSplitEdit dlg(guid);

        dlg.setButtonStates(/*canRemove=*/false, /*isEdit=*/false);

        QPushButton* remove = nullptr;
        for (auto* b : dlg.findChildren<QPushButton*>())
            if (b->text().contains(QStringLiteral("Entfernen"))) remove = b;
        if (!remove) QFAIL("Entfernen-Button nicht gefunden");
        QVERIFY(!remove->isEnabled());
    }

    void test_view_setFactorPreview_setsField()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareSplitEdit dlg(guid);

        dlg.setFactorPreview(QStringLiteral("aus 1 Stk. werden 20 Stk."));

        auto* field = dlg.findChild<QLineEdit*>(QStringLiteral("factorPreview"));
        if (!field) QFAIL("factorPreview not found");
        QCOMPARE(field->text(), QStringLiteral("aus 1 Stk. werden 20 Stk."));
    }

    void test_view_futureDateIsAccepted()
    {
        // Die Maske darf zukünftige Ex-Tage nicht sperren — ein angekündigter
        // Split soll sofort erfassbar sein (Nessies Entscheidung 08.08.2026).
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareSplitEdit dlg(guid);

        auto* date = dlg.findChild<QDateEdit*>(QStringLiteral("splitDate"));
        if (!date) QFAIL("splitDate not found");
        const QDate future = QDate::currentDate().addYears(1);
        date->setDate(future);
        QCOMPARE(dlg.splitDate(), future);
    }

    void test_view_ratioFieldsAcceptGermanDecimalComma()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareSplitEdit dlg(guid);

        auto* ratioNew = dlg.findChild<QLineEdit*>(QStringLiteral("ratioNew"));
        if (!ratioNew) QFAIL("ratioNew not found");
        ratioNew->setText(QStringLiteral("1,5"));
        QCOMPARE(dlg.ratioNew(), 1.5);
    }

    void test_view_hasDocumentFieldAndBrowseButton()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareSplitEdit dlg(guid);

        QVERIFY(dlg.findChild<QLineEdit*>(QStringLiteral("documentPath")));
        QVERIFY(dlg.findChild<QPushButton*>(QStringLiteral("btnBrowseDocument")));
    }

    void test_view_loadSplit_populatesDocumentPath()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareSplitEdit dlg(guid);

        dlg.loadSplit(ShareSplitObject(QStringLiteral("s1"), guid,
                                       QDate(2022, 7, 18), 20.0, 1.0, false,
                                       QString(), QStringLiteral("/belege/a.pdf")));

        QCOMPARE(dlg.documentPath(), QStringLiteral("/belege/a.pdf"));
    }

    void test_view_clearForm_clearsDocumentPath()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareSplitEdit dlg(guid);

        dlg.setDocumentPath(QStringLiteral("/belege/a.pdf"));
        dlg.clearForm();

        QVERIFY(dlg.documentPath().isEmpty());
    }

    void test_view_hasPreviewPanel()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareSplitEdit dlg(guid);

        QVERIFY(dlg.findChild<DocumentPreviewPanel*>());
    }

    void test_view_clearPdfPreview_doesNotCrash()
    {
        // DocumentPreviewPanel zeigt "Datei nicht gefunden" inline statt über
        // einen modalen Dialog — deshalb ist der Aufruf im Test unbedenklich.
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareSplitEdit dlg(guid);

        dlg.clearPdfPreview();
    }

    void test_view_overviewTable_hasDocumentColumn()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareSplitEdit dlg(guid);

        auto* table = dlg.findChild<QTableWidget*>(QStringLiteral("splitsTable"));
        if (!table) QFAIL("splitsTable not found");
        QCOMPARE(table->columnCount(), 6);
        // Die Dokument-Spalte bleibt ohne Überschrift und bei 36 px —
        // projektweit vereinheitlicht am 17.07.2026.
        QVERIFY(table->horizontalHeaderItem(5)->text().isEmpty());
        QCOMPARE(table->columnWidth(5), 36);
    }

    void test_view_populateOverview_showsDocumentIcon()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareSplitEdit dlg(guid);

        dlg.populateOverview({ makeSplit(QStringLiteral("s1"), QDate(2022, 7, 18),
                                         20.0, 1.0, false,
                                         QStringLiteral("/belege/a.pdf")),
                               makeSplit(QStringLiteral("s2"), QDate(2023, 5, 2),
                                         1.0, 10.0) });

        auto* table = dlg.findChild<QTableWidget*>(QStringLiteral("splitsTable"));
        if (!table) QFAIL("splitsTable not found");
        QVERIFY(!table->item(0, 5)->icon().isNull());
        QCOMPARE(table->item(0, 5)->toolTip(), QStringLiteral("/belege/a.pdf"));
        // Zeile ohne Beleg bleibt leer — kein Platzhalter-Icon.
        QVERIFY(table->item(1, 5)->icon().isNull());
    }

    void test_view_presenterIsAccessible()
    {
        openMemoryDb();
        const QString guid = insertTestShare();
        ViewShareSplitEdit dlg(guid);
        // ViewShareEdit hängt refreshSummary() an presenter()->dataChanged().
        QVERIFY(dlg.presenter() != nullptr);
    }
};

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);

    // formatMoney() und die Zahlformatierung in Presenter und View verwenden
    // QLocale() — CI-Runner laufen nicht mit deutschem Locale, deshalb hier
    // zentral setzen (siehe ARCHITECTURE.md, "Locale in Tests").
    QLocale::setDefault(QLocale::German);

    TestShareSplitsForm t;
    return QTest::qExec(&t, argc, argv);
}

#include "tst_sharesplitsform.moc"

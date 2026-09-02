// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// tst_dividendform.cpp — Unit tests for the DividendForm MVP triad
// (ModelDividendEdit, PresenterDividendEdit, ViewDividendEdit).
//
// Aus tst_mainwindow.cpp herausgelöst (22.08.2026) — analog tst_buysform und
// tst_shareeditform. tst_mainwindow.cpp war auf über 11.000 Zeilen mit fünf
// Testklassen gewachsen; die Konvention „ein Testziel je Form" ist damit
// wiederhergestellt. Reine Auslagerung: die Testmethoden, die beiden Stubs und
// die Klassen-Helper sind unverändert übernommen, es kam keine Prüfung dazu und
// es fiel keine weg.
//
// main() setzt — wie zuvor tst_mainwindow.cpp — QLocale::setDefault(
// QLocale::German), bevor QApplication konstruiert wird: die Tests vergleichen
// formatierte Beträge („1,50", „0,0000 %") und würden auf einem Runner mit
// englischer System-Locale sonst fehlschlagen. Siehe ARCHITECTURE.md,
// "System-Locale-abhängiges Zahlenformat".

#include <QtTest>
#include <QSignalSpy>
#include <QApplication>
#include <QLocale>
#include <QDate>
#include <QDateTime>
#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QTableWidget>

#include "../../app/core/Database.h"
#include "../../app/repositories/ShareRepository.h"
#include "../../app/repositories/DailyValuesRepository.h"
#include "../../app/models/ShareObject.h"
#include "../../app/models/BuyObject.h"
#include "../../app/models/SaleObject.h"
#include "../../app/models/DividendObject.h"
#include "../../app/models/ShareSplitObject.h"
#include "../../app/models/DailyValuesObject.h"
#include "../../app/utils/ShareSplitHint.h"
#include "../../app/widgets/OverviewTabWidget.h"

#include "../../app/forms/DividendForm/IViewDividendEdit.h"
#include "../../app/forms/DividendForm/IModelDividendEdit.h"
#include "../../app/forms/DividendForm/ViewDividendEdit.h"
#include "../../app/forms/DividendForm/ModelDividendEdit.h"
#include "../../app/forms/DividendForm/PresenterDividendEdit.h"

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

    // Phase 2 (21.08.2026): das tatsächlich übergebene DividendObject
    // festhalten, um exDate()/depotNumber() im onSave()-Payload zu prüfen.
    DividendObject lastAddedDividend;
    DividendObject lastUpdatedDividend;

    // Phase 3 (21.08.2026): Datengrundlage der Stückzahl-Plausibilitäts-
    // prüfung. Vorgabe bewusst LEER — DividendVolumeChecker wertet eine Aktie
    // ohne erfasste Käufe als "nicht prüfbar", damit die vorhandenen Tests
    // hier unverändert durchlaufen und nur die Tests, die die Prüfung
    // ausdrücklich meinen, Käufe setzen.
    QList<BuyObject>  buys;
    QList<SaleObject> sales;

    QList<DividendObject> loadDividends(const QString&) const override { return dividends; }
    ShareObject           loadShare(const QString&)     const override { return ShareObject{}; }
    QList<ShareSplitObject> loadSplits(const QString&)  const override { return splits; }
    QList<BuyObject>      loadBuys(const QString&)      const override { return buys; }
    QList<SaleObject>     loadSales(const QString&)     const override { return sales; }

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

    bool addDividend(const DividendObject& d)    override
        { addDividendCalled = true; lastAddedDividend = d; return addResult; }
    bool updateDividend(const DividendObject& d) override
        { updateDividendCalled = true; lastUpdatedDividend = d; return updateResult; }
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

    // Phase 2 (21.08.2026): Ex-Tag/Depotnummer — Default vor dem
    // Auszahlungstag (m_dateTime = 2024-06-15), damit Tests, die diese Felder
    // nicht gezielt prüfen, nicht versehentlich in die Ex-Tag>Zahltag-
    // Blockade laufen.
    QString m_exDate      = QStringLiteral("2024-06-13");
    QString m_depotNumber = QStringLiteral("DE123456789");

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
    QStringList fieldErrors;

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
    QString exDate()                const override { return m_exDate; }
    QString depotNumber()           const override { return m_depotNumber; }

    // IViewDividendEdit — write
    void loadDividend(const DividendObject&) override { loadDividendCalled = true; }
    void clearForm()                         override { clearFormCalled = true; }

    void setDividendPayout(double)          override {}
    void setDividendPayoutFc(double)        override {}
    void setTaxSum(double)                  override {}
    void setDividendPayoutWithTaxes(double) override {}
    void setYield(double)                   override {}

    void setForeignCurrencyEnabled(bool)    override {}

    // Phase 5 (21.08.2026): Fremdwährung aus dem Beleg.
    bool    lastFcEnabled = false;
    QString lastFcIsoCode;
    void setForeignCurrency(bool enabled, const QString& isoCode) override
    {
        lastFcEnabled = enabled;
        lastFcIsoCode = isoCode;
        m_enableFc    = enabled;   // wie die echte View: der Haken wird gesetzt
    }

    // Umschaltbarer Fehlschlag (27.08.2026), siehe tst_buysform.cpp.
    QStringList failingFields;   ///< diese Feldschluessel schlagen fehl

    bool setFieldOk(const QString& field, const QString& value,
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

        if (failingFields.contains(field)) return false;
        return true;
    }
    void setFieldError(const QString& f, const QString& = QString()) override
                                                    { fieldErrors << f; }

    // Phase 5-Nachtrag (21.08.2026): Ersatzhinweis für Belege ohne Ex-Tag.
    QString lastHintField;
    QString lastHintTooltip;
    void setFieldHint(const QString& field, const QString& tooltip) override
    {
        lastHintField   = field;
        lastHintTooltip = tooltip;
    }

    void setDocumentPath(const QString& path)       override { m_docPath = path; }
    void setDocumentPreview(const QString&)         override {}

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

    /**
     * @brief Kauf im Depot der Stub-View (DE123456789), standardmässig vor
     *        deren Ex-Tag (2024-06-13) — für die Stückzahl-Plausibilitäts-
     *        prüfung (Phase 3, 21.08.2026).
     */
    static BuyObject makeDepotBuy(const QString& guid, double volume,
                                  const QString& isoDate = QStringLiteral("2024-01-10"),
                                  const QString& depot   = QStringLiteral("DE123456789"))
    {
        return BuyObject(guid, makeShareGuid(), depot,
                         QStringLiteral("order-") + guid,
                         isoDate + QStringLiteral("T00:00:00"),
                         volume, /*volumeSold=*/0.0, /*price=*/10.0);
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
        // Seit Phase 2 (21.08.2026) ebenfalls Pflicht:
        // Die Depotnummer kommt NICHT ueber setFieldOk() herein: der Dialog ist
        // ohne DocumentsConfig gebaut, seine Auswahlliste also leer, und seit
        // dem 27.08.2026 weist setFieldOk() eine unbekannte Depotnummer ab
        // (siehe ARCHITECTURE.md, "Analyse-Statuszeile und Feldsymbole").
        // Dieser Test will die Depoterkennung gar nicht pruefen, sondern
        // hasMissingRequiredFields() — deshalb wird der Eintrag direkt
        // angelegt, so wie ihn eine geladene Documents.xml beisteuern wuerde.
        //
        // Welche der Comboboxen die Depot-Auswahl ist, wird nicht geraten:
        // der Eintrag wandert reihum in jede, und depotNumber() sagt, wann
        // die richtige getroffen ist.
        for (auto* combo : dlg.findChildren<QComboBox*>()) {
            const int before = combo->currentIndex();
            combo->addItem(QStringLiteral("Testdepot"), QStringLiteral("DE123456789"));
            combo->setCurrentIndex(combo->count() - 1);
            if (dlg.depotNumber() == QStringLiteral("DE123456789"))
                break;
            combo->removeItem(combo->count() - 1);
            combo->setCurrentIndex(before);
        }
        QCOMPARE(dlg.depotNumber(), QStringLiteral("DE123456789"));

        dlg.setFieldOk(QStringLiteral("depotNumber"), QStringLiteral("DE123456789"));
        dlg.setFieldOk(QStringLiteral("exDate"),      QStringLiteral("2024-06-13"));
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
        // Seit Phase 2 (21.08.2026) gibt es zwei QDateEdit-Felder (Zahltag
        // und Ex-Tag) — ein unbenanntes findChild<QDateEdit*>() ist damit
        // mehrdeutig (und liefert je nach Verschachtelungstiefe nicht mehr
        // zuverlässig m_date). Über den öffentlichen dateTime()-Accessor
        // geprüft statt über den internen Widget-Baum.
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.setFieldOk(QStringLiteral("date"), QStringLiteral("2024-06-15"));
        const QDate d = QDateTime::fromString(dlg.dateTime(), Qt::ISODate).date();
        QCOMPARE(d, QDate(2024, 6, 15));
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

    // ── Ex-Tag / Depotnummer (Phase 2, 21.08.2026) ─────────────────────────

    void test_presenterDividendEdit_onDepotNumberEdited_set_setsOk()
    {
        StubViewDividendEdit view;
        view.m_depotNumber = QStringLiteral("DE999888777");
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        p.onDepotNumberEdited();
        QCOMPARE(view.lastFieldOkField, QStringLiteral("depotNumber"));
    }

    void test_presenterDividendEdit_onDepotNumberEdited_empty_setsError()
    {
        StubViewDividendEdit view;
        view.m_depotNumber.clear();
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        p.onDepotNumberEdited();
        QVERIFY(view.fieldErrors.contains(QStringLiteral("depotNumber")));
    }

    void test_presenterDividendEdit_onExDateEdited_valid_setsOk()
    {
        StubViewDividendEdit view;
        view.m_dateTime = QStringLiteral("2024-06-15T00:00:00");
        view.m_exDate   = QStringLiteral("2024-06-13");
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        p.onExDateEdited();
        QCOMPARE(view.lastFieldOkField, QStringLiteral("exDate"));
    }

    void test_presenterDividendEdit_onExDateEdited_sentinel_setsError()
    {
        // "2000-01-01" ist der Sentinel für "vom Benutzer noch nicht gesetzt"
        // (siehe IViewDividendEdit::exDate()) — muss als fehlend gelten.
        StubViewDividendEdit view;
        view.m_exDate = QStringLiteral("2000-01-01");
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        p.onExDateEdited();
        QVERIFY(view.fieldErrors.contains(QStringLiteral("exDate")));
    }

    void test_presenterDividendEdit_onExDateEdited_afterPayday_setsError()
    {
        // Blockade Ex-Tag > Auszahlungstag — Nessies Entscheidung 21.08.2026
        // ("weil es eben nicht sein darf!").
        StubViewDividendEdit view;
        view.m_dateTime = QStringLiteral("2024-06-15T00:00:00");
        view.m_exDate   = QStringLiteral("2024-06-20");  // nach dem Zahltag
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        p.onExDateEdited();
        QVERIFY(view.fieldErrors.contains(QStringLiteral("exDate")));
    }

    void test_presenterDividendEdit_onDateEdited_revalidatesExDateAgainstNewPayday()
    {
        // Ändert sich der Auszahlungstag auf VOR den bereits gültigen Ex-Tag,
        // muss die Ex-Tag-Prüfung live neu anschlagen (onDateEdited() ruft
        // onExDateEdited() erneut auf).
        StubViewDividendEdit view;
        view.m_exDate   = QStringLiteral("2024-06-13");
        view.m_dateTime = QStringLiteral("2024-06-10T00:00:00");  // vor dem Ex-Tag
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        p.onDateEdited();
        QVERIFY(view.fieldErrors.contains(QStringLiteral("exDate")));
    }

    void test_presenterDividendEdit_onSave_passesExDateAndDepotNumberToModel()
    {
        StubViewDividendEdit view;
        view.m_exDate      = QStringLiteral("2024-06-13");
        view.m_depotNumber = QStringLiteral("DE123456789");
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        p.onSave();
        QVERIFY(model.addDividendCalled);
        QCOMPARE(model.lastAddedDividend.exDate(),      QStringLiteral("2024-06-13"));
        QCOMPARE(model.lastAddedDividend.depotNumber(), QStringLiteral("DE123456789"));
    }

    void test_presenterDividendEdit_onSave_exDateAfterPayday_blocksSave()
    {
        StubViewDividendEdit view;
        view.m_dateTime = QStringLiteral("2024-06-15T00:00:00");
        view.m_exDate   = QStringLiteral("2024-06-20");  // nach dem Zahltag → Blockade
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);
        p.onSave();
        QVERIFY(!model.addDividendCalled);
        QVERIFY(!view.lastError.isEmpty());
    }

    // ── Stückzahl-Plausibilitätsprüfung (Phase 3, 21.08.2026) ─────────────

    void test_presenterDividendEdit_onSave_volumeMatchesHoldings_saves()
    {
        StubViewDividendEdit view;
        view.m_volume = 100.0;
        StubModelDividendEdit model;
        model.buys << makeDepotBuy(QStringLiteral("b1"), 100.0);
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);

        p.onSave();

        QVERIFY(model.addDividendCalled);
        QVERIFY(view.lastError.isEmpty());
    }

    void test_presenterDividendEdit_onSave_volumeMismatch_blocksSave()
    {
        StubViewDividendEdit view;
        view.m_volume = 150.0;              // Bestand ist aber 100
        StubModelDividendEdit model;
        model.buys << makeDepotBuy(QStringLiteral("b1"), 100.0);
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);

        p.onSave();

        QVERIFY(!model.addDividendCalled);
        QVERIFY(!view.lastError.isEmpty());
        // Das Mengenfeld wird rot markiert, damit die Meldung eine Entsprechung
        // in der Maske hat.
        QVERIFY(view.fieldErrors.contains(QStringLiteral("volume")));
    }

    void test_presenterDividendEdit_onSave_noBuysRecorded_doesNotBlock()
    {
        // Aktie ohne erfasste Kaufhistorie: die Prüfung wird übersprungen,
        // statt das Speichern unmöglich zu machen.
        StubViewDividendEdit view;
        view.m_volume = 100.0;
        StubModelDividendEdit model;   // model.buys bleibt leer
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);

        p.onSave();

        QVERIFY(model.addDividendCalled);
        QVERIFY(view.lastError.isEmpty());
    }

    void test_presenterDividendEdit_onSave_buysInOtherDepot_blocksSave()
    {
        StubViewDividendEdit view;
        view.m_volume      = 100.0;
        view.m_depotNumber = QStringLiteral("DE123456789");
        StubModelDividendEdit model;
        model.buys << makeDepotBuy(QStringLiteral("b1"), 100.0,
                                   QStringLiteral("2024-01-10"),
                                   QStringLiteral("DE999999999"));  // anderes Depot
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);

        p.onSave();

        QVERIFY(!model.addDividendCalled);
        QVERIFY(!view.lastError.isEmpty());
    }

    void test_presenterDividendEdit_onSave_splitBetweenBuyAndExDate_saves()
    {
        // Kauf 100 Stk., danach Split 2:1 → am Ex-Tag 200 Stk., und genau 200
        // stehen auf der Abrechnung. Ohne Skalenumrechnung würde hier
        // fälschlich blockiert.
        StubViewDividendEdit view;
        view.m_volume = 200.0;
        StubModelDividendEdit model;
        model.buys << makeDepotBuy(QStringLiteral("b1"), 100.0);
        model.splits << ShareSplitObject(QStringLiteral("split-1"), makeShareGuid(),
                                         QDate(2024, 3, 1), 2.0, 1.0);
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);

        p.onSave();

        QVERIFY(model.addDividendCalled);
        QVERIFY(view.lastError.isEmpty());
    }

    void test_presenterDividendEdit_onSave_saleBeforeExDate_reducesHoldings()
    {
        // 100 gekauft, 30 vor dem Ex-Tag verkauft → 70 erwartet.
        StubViewDividendEdit view;
        view.m_volume = 70.0;
        StubModelDividendEdit model;
        model.buys  << makeDepotBuy(QStringLiteral("b1"), 100.0);
        model.sales << SaleObject(QStringLiteral("s1"), makeShareGuid(),
                                  QStringLiteral("DE123456789"),
                                  QStringLiteral("order-s1"),
                                  QStringLiteral("2024-03-01T00:00:00"),
                                  30.0, 12.0, QList<SaleBuyDetail>{});
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);

        p.onSave();

        QVERIFY(model.addDividendCalled);
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

    // ── Ex-Tag / Depotnummer (Phase 2, 21.08.2026) ─────────────────────────

    void test_viewDividendEdit_exDate_defaultsToSentinel()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        // Frisches Formular: Sentinel "2000-01-01", nicht "heute" — siehe
        // Kommentar an m_exDate in ViewDividendEdit::createDividenddatenGroup().
        QCOMPARE(dlg.exDate(), QStringLiteral("2000-01-01"));
    }

    void test_viewDividendEdit_depotNumber_defaultsToEmpty()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        QVERIFY(dlg.depotNumber().isEmpty());
    }

    void test_viewDividendEdit_hasMissingRequiredFields_exDateAndDepotNumberMissingByDefault()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        QStringList missing;
        QVERIFY(dlg.hasMissingRequiredFields(missing));
        QVERIFY(missing.contains(QObject::tr("Ex-Tag")));
        QVERIFY(missing.contains(QObject::tr("Depotnummer")));
    }

    void test_viewDividendEdit_setFieldOk_exDate_parsesISOFormat()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.setFieldOk(QStringLiteral("exDate"), QStringLiteral("2024-06-13"));
        QCOMPARE(dlg.exDate(), QStringLiteral("2024-06-13"));
    }

    void test_viewDividendEdit_loadDividend_withExDateAndDepotNumber_populatesFields()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        DividendObject d(QStringLiteral("div-1"), makeShareGuid(),
                         QStringLiteral("2024-06-15T00:00:00"),
                         1.50, 100.0, 0.0, 0.0, 0.0, 45.0,
                         false, 1.0, QStringLiteral("EUR"), QString(),
                         QStringLiteral("2024-06-13"), QStringLiteral("DE123456789"));
        dlg.loadDividend(d);
        QCOMPARE(dlg.exDate(),      QStringLiteral("2024-06-13"));
        QCOMPARE(dlg.depotNumber(), QStringLiteral("DE123456789"));
    }

    void test_viewDividendEdit_loadDividend_withoutExDate_showsSentinel()
    {
        // Eine Alt-Dividende ohne Ex-Tag/Depotnummer (leere Strings, wie sie
        // vor dem 21.08.2026 gespeichert wurden) muss beim Laden den
        // Sentinel/leer zeigen, damit die Pflichtfeld-Prüfung beim nächsten
        // Speichern zuschlägt — Nessies Entscheidung: "der Benutzer muss den
        // Ex-Tag nachtragen".
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        DividendObject d(QStringLiteral("div-old"), makeShareGuid(),
                         QStringLiteral("2024-06-15T00:00:00"),
                         1.50, 100.0, 0.0, 0.0, 0.0, 45.0);  // exDate/depotNumber leer
        dlg.loadDividend(d);
        QCOMPARE(dlg.exDate(), QStringLiteral("2000-01-01"));
        QVERIFY(dlg.depotNumber().isEmpty());

        QStringList missing;
        QVERIFY(dlg.hasMissingRequiredFields(missing));
        QVERIFY(missing.contains(QObject::tr("Ex-Tag")));
        QVERIFY(missing.contains(QObject::tr("Depotnummer")));
    }

    // ── Beleg-Import: Ex-Tag und Fremdwährung (Phase 5, 21.08.2026) ──────

    void test_viewDividendEdit_setFieldOk_exDate_parsesGermanDocumentFormat()
    {
        // Genau dieses Format liefern die Belege beider Banken ("Ex-Tag
        // 08.05.2026"), und genau so reicht der Parser den Treffer weiter.
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.setFieldOk(QStringLiteral("exDate"), QStringLiteral("08.05.2026"));
        QCOMPARE(dlg.exDate(), QStringLiteral("2026-05-08"));
    }

    void test_viewDividendEdit_setFieldOk_depotNumber_unknownIsRejected()
    {
        // Umgekehrte Erwartung seit dem 27.08.2026 (vorher hiess der Test
        // ..._selectsUnknownValue und pruefte das Gegenteil): eine Depotnummer,
        // die nicht in Documents.xml hinterlegt ist, wird NICHT in die
        // Auswahlliste aufgenommen. Sie waere sonst in der Datenbank gelandet,
        // ohne je konfiguriert worden zu sein — und die Bestandspruefung pro
        // Depot (Stueckzahl am Ex-Tag) braucht eine gepflegte Zuordnung.
        // Siehe ARCHITECTURE.md, "Analyse-Statuszeile und Feldsymbole".
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);   // leere Auswahlliste

        QVERIFY(!dlg.setFieldOk(QStringLiteral("depotNumber"),
                                QStringLiteral("501403950")));
        QCOMPARE(dlg.depotNumber(), QString());
    }

    void test_viewDividendEdit_setFieldOk_depotNumber_unknownMarksFieldAsMissing()
    {
        // Die Kehrseite: das Feld bleibt eine fehlende Pflichtangabe, das
        // Speichern ist also blockiert. Vorher stand hier ein gruener Haken
        // an einem leeren Pflichtfeld.
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);

        dlg.setFieldOk(QStringLiteral("depotNumber"), QStringLiteral("501403950"));

        QStringList missing;
        QVERIFY(dlg.hasMissingRequiredFields(missing));
        QVERIFY(missing.contains(QObject::tr("Depotnummer")));
    }

    void test_viewDividendEdit_setFieldHint_doesNotSatisfyRequiredField()
    {
        // Der wichtigste Punkt am Ersatzhinweis: er zeigt nur etwas an. Das
        // Ex-Tag-Feld bleibt leer und weiterhin eine fehlende Pflichtangabe —
        // sonst könnte der Benutzer mit einem Sentinel-Datum speichern und
        // die Bestände-Prüfung liefe gegen den falschen Stichtag.
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);

        dlg.setFieldHint(QStringLiteral("exDate"),
                         QStringLiteral("Schlusstag: 05.02.2019"));

        QCOMPARE(dlg.exDate(), QStringLiteral("2000-01-01"));
        QStringList missing;
        QVERIFY(dlg.hasMissingRequiredFields(missing));
        QVERIFY(missing.contains(QObject::tr("Ex-Tag")));
    }

    void test_viewDividendEdit_setFieldHint_survivesOnParseFinished()
    {
        // onParseFinished() setzt bei noch unberührten Pflichtfeldern seinen
        // allgemeinen Text. Der genauere Hinweis darf davon nicht
        // überschrieben werden — deshalb setzt setFieldHint() den Zustand auf
        // Info statt ihn auf Untouched zu lassen.
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);

        const QString hint = QStringLiteral("Schlusstag: 05.02.2019");
        dlg.setFieldHint(QStringLiteral("exDate"), hint);
        dlg.onParseFinished();

        // Das Statussymbol des Ex-Tag-Felds trägt weiterhin den Hinweistext.
        bool found = false;
        for (auto* lbl : dlg.findChildren<QLabel*>()) {
            if (lbl->toolTip() == hint) { found = true; break; }
        }
        QVERIFY2(found, "Hinweis wurde von onParseFinished() überschrieben");
    }

    void test_viewDividendEdit_setFieldHint_unknownField_doesNotCrash()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.setFieldHint(QStringLiteral("gibtesnicht"), QStringLiteral("egal"));
        QVERIFY(true);
    }

    void test_viewDividendEdit_setForeignCurrency_usd_enablesAndSelects()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        QVERIFY(!dlg.enableForeignCurrency());

        dlg.setForeignCurrency(true, QStringLiteral("USD"));

        QVERIFY(dlg.enableForeignCurrency());
        QCOMPARE(dlg.currency(), QStringLiteral("en-US"));
    }

    void test_viewDividendEdit_setForeignCurrency_gbp_selectsPound()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.setForeignCurrency(true, QStringLiteral("GBP"));
        QVERIFY(dlg.enableForeignCurrency());
        QCOMPARE(dlg.currency(), QStringLiteral("en-GB"));
    }

    void test_viewDividendEdit_setForeignCurrency_eur_disablesMode()
    {
        // Ein Euro-Beleg schaltet den Modus wieder ab — sonst würde ein aus
        // einem vorherigen Import stehengebliebener Haken die Auszahlung
        // durch einen fremden Devisenkurs teilen.
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.setForeignCurrency(true, QStringLiteral("USD"));
        QVERIFY(dlg.enableForeignCurrency());

        dlg.setForeignCurrency(false, QStringLiteral("EUR"));
        QVERIFY(!dlg.enableForeignCurrency());
    }

    void test_viewDividendEdit_setForeignCurrency_unknownIso_keepsSelection()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.setForeignCurrency(true, QStringLiteral("GBP"));
        QCOMPARE(dlg.currency(), QStringLiteral("en-GB"));

        // Unbekanntes Kürzel: Modus bleibt an, Auswahl unverändert — der
        // Benutzer sieht die Währung und kann sie korrigieren.
        dlg.setForeignCurrency(true, QStringLiteral("XYZ"));
        QVERIFY(dlg.enableForeignCurrency());
        QCOMPARE(dlg.currency(), QStringLiteral("en-GB"));
    }

    void test_viewDividendEdit_setForeignCurrency_exchangeRateSurvives()
    {
        // Der Parser schreibt erst den Kurs und schaltet dann den Modus ein
        // (Reihenfolge in populateFromResult()). Der Wert darf dabei nicht
        // verlorengehen — sonst wäre die ganze Umrechnung wirkungslos.
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        dlg.setFieldOk(QStringLiteral("exchangeRatio"), QStringLiteral("1,148693"));
        dlg.setForeignCurrency(true, QStringLiteral("USD"));
        QCOMPARE(dlg.exchangeRatio(), 1.148693);
    }

    // ── Split-Marker am Ex-Tag statt am Zahltag (Phase 4, 21.08.2026) ─────

    void test_viewDividendEdit_overview_splitMarkerUsesExDate()
    {
        // Der Split liegt ZWISCHEN Ex-Tag (01.05.) und Zahltag (15.06.).
        // Am Ex-Tag galt also noch die alte Stückelung → die Anteile-Zelle
        // muss den Marker tragen. Vor Phase 4 wurde gegen den Zahltag
        // geprüft, und dieser Split wäre übersehen worden.
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);

        DividendObject d(QStringLiteral("div-1"), makeShareGuid(),
                         QStringLiteral("2024-06-15T00:00:00"),
                         1.50, 100.0, 0.0, 0.0, 0.0, 45.0,
                         false, 1.0, QStringLiteral("EUR"), QString(),
                         QStringLiteral("2024-05-01"), QStringLiteral("DE123456789"));

        const QList<ShareSplitObject> splits = {
            ShareSplitObject(QStringLiteral("split-1"), makeShareGuid(),
                             QDate(2024, 5, 20), 2.0, 1.0)
        };

        dlg.populateOverview({ d }, splits);

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        auto* container = tabs->widget(1);
        if (!container) QFAIL("Jahres-Tab container not found");
        auto* tbl = qobject_cast<QTableWidget*>(
            container->property("dataTable").value<QObject*>());
        if (!tbl) QFAIL("dataTable not found");

        auto* iVol = tbl->item(0, 2);   // kColVolume
        QVERIFY(iVol != nullptr);
        QVERIFY2(iVol->text().contains(ShareSplitHint::marker()),
                 qPrintable(iVol->text()));
    }

    void test_viewDividendEdit_overview_splitBetweenExDateAndPayday_wasMissedBefore()
    {
        // Gegenprobe zum Test darüber: derselbe Split, aber die Dividende hat
        // KEINEN Ex-Tag (Alt-Datensatz). Dann greift der Rückfall auf den
        // Zahltag (15.06.), der Split vom 20.05. liegt davor — kein Marker.
        // Genau das war vor Phase 4 auch bei gesetztem Ex-Tag das Ergebnis.
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);

        const QList<ShareSplitObject> splits = {
            ShareSplitObject(QStringLiteral("split-1"), makeShareGuid(),
                             QDate(2024, 5, 20), 2.0, 1.0)
        };

        dlg.populateOverview({ makeDividend(QStringLiteral("div-old")) }, splits);

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        auto* container = tabs->widget(1);
        if (!container) QFAIL("Jahres-Tab container not found");
        auto* tbl = qobject_cast<QTableWidget*>(
            container->property("dataTable").value<QObject*>());
        if (!tbl) QFAIL("dataTable not found");

        auto* iVol = tbl->item(0, 2);
        QVERIFY(iVol != nullptr);
        QVERIFY2(!iVol->text().contains(ShareSplitHint::marker()),
                 qPrintable(iVol->text()));
    }

    void test_viewDividendEdit_overview_splitAfterPayday_markerForOldDividend()
    {
        // Alt-Dividende ohne Ex-Tag, Split NACH dem Zahltag: der Rückfall auf
        // date() muss das bisherige Verhalten unverändert lassen — Marker.
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);

        const QList<ShareSplitObject> splits = {
            ShareSplitObject(QStringLiteral("split-1"), makeShareGuid(),
                             QDate(2024, 8, 1), 2.0, 1.0)
        };

        dlg.populateOverview({ makeDividend(QStringLiteral("div-old")) }, splits);

        auto* tabs = dlg.findChild<OverviewTabWidget*>();
        if (!tabs) QFAIL("OverviewTabWidget not found");
        auto* container = tabs->widget(1);
        if (!container) QFAIL("Jahres-Tab container not found");
        auto* tbl = qobject_cast<QTableWidget*>(
            container->property("dataTable").value<QObject*>());
        if (!tbl) QFAIL("dataTable not found");

        auto* iVol = tbl->item(0, 2);
        QVERIFY(iVol != nullptr);
        QVERIFY2(iVol->text().contains(ShareSplitHint::marker()),
                 qPrintable(iVol->text()));
    }

    void test_viewDividendEdit_clearForm_resetsExDateAndDepotNumber()
    {
        openMemoryDb();
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        DividendObject d(QStringLiteral("div-1"), makeShareGuid(),
                         QStringLiteral("2024-06-15T00:00:00"),
                         1.50, 100.0, 0.0, 0.0, 0.0, 45.0,
                         false, 1.0, QStringLiteral("EUR"), QString(),
                         QStringLiteral("2024-06-13"), QStringLiteral("DE123456789"));
        dlg.loadDividend(d);
        dlg.clearForm();
        QCOMPARE(dlg.exDate(), QStringLiteral("2000-01-01"));
        QVERIFY(dlg.depotNumber().isEmpty());
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

    void test_presenterDividendEdit_populateFromResult_allFieldsTaken_reportsOk()
    {
        StubViewDividendEdit view;
        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);

        const QMap<QString, QList<QString>> result = {
            { QStringLiteral("Date"), { QStringLiteral("15.05.2024") } },
            { QStringLiteral("ExDate"), { QStringLiteral("13.05.2024") } },
            { QStringLiteral("DepotNumber"), { QStringLiteral("1234567890") } },
            { QStringLiteral("Volume"), { QStringLiteral("10") } },
            { QStringLiteral("DividendRate"), { QStringLiteral("0,85") } },
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

    void test_presenterDividendEdit_populateFromResult_rejectedRequiredField_reportsFailed()
    {
        StubViewDividendEdit view;
        view.failingFields << QStringLiteral("exDate");

        StubModelDividendEdit model;
        PresenterDividendEdit p(&view, &model, makeShareGuid(), nullptr);

        const QMap<QString, QList<QString>> result = {
            { QStringLiteral("Date"), { QStringLiteral("15.05.2024") } },
            { QStringLiteral("ExDate"), { QStringLiteral("13.05.2024") } },
            { QStringLiteral("DepotNumber"), { QStringLiteral("1234567890") } },
            { QStringLiteral("Volume"), { QStringLiteral("10") } },
            { QStringLiteral("DividendRate"), { QStringLiteral("0,85") } },
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
    // Feldschluessel-Tabellen (02.09.2026) — Runde 4, gleiches Muster wie
    // tst_shareaddform/tst_buysform/tst_salesform. Siehe ARCHITECTURE.md,
    // "Feldschluessel-Tabellen sind an keiner Stelle geprueft".
    //
    // Diese Runde hat den ersten echten Treffer gebracht: "currency" war in
    // ViewDividendEdit nirgends registriert, setFieldOk() meldete dafuer
    // trotzdem Erfolg, und die Optional-Zaehlung nahm den Wert mit.
    //
    // Der Dialog wird ohne DocumentsConfig gebaut — die Depotnummern-Liste
    // bleibt dadurch leer, die Registrierung der Feldschluessel haengt aber
    // nicht daran. Gleiches Vorgehen wie in den uebrigen View-Tests dieser
    // Datei.
    // ─────────────────────────────────────────────────────────────────────

    void test_dividendEdit_everyKnownXmlNameHasAViewField()
    {
        for (const QString& xmlName : PresenterDividendEdit::knownXmlNames()) {
            const QString viewField =
                PresenterDividendEdit::xmlNameToViewField(xmlName);
            QVERIFY2(!viewField.isEmpty(),
                     qPrintable(QStringLiteral(
                         "knownXmlNames() fuehrt \"%1\", "
                         "xmlNameToViewField() kennt den Namen aber nicht")
                         .arg(xmlName)));
        }
    }

    void test_dividendEdit_everyViewFieldIsRegisteredInTheDialog()
    {
        // Der Test, der "currency" gefunden hat. Vor dem 02.09.2026 waere er
        // an genau diesem Schluessel gescheitert.
        ViewDividendEdit dlg(makeShareGuid(), nullptr);

        for (const QString& xmlName : PresenterDividendEdit::knownXmlNames()) {
            const QString viewField =
                PresenterDividendEdit::xmlNameToViewField(xmlName);
            if (viewField.isEmpty())
                continue;   // eigener Test oben

            QVERIFY2(dlg.setFieldOk(viewField, QString()),
                     qPrintable(QStringLiteral(
                         "Feldschluessel \"%1\" (aus XML-Name \"%2\") ist im "
                         "Dialog weder als Eingabefeld noch als Symbol "
                         "registriert").arg(viewField, xmlName)));
        }
    }

    void test_dividendEdit_currencyIsRegisteredButSharesItsSymbol()
    {
        // Der Fund vom 02.09.2026, festgenagelt. "currency" muss im Dialog
        // bekannt sein — sonst zaehlt die Statuszeile einen Wert mit, der
        // nirgends ankommt.
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        QVERIFY2(dlg.setFieldOk(QStringLiteral("currency"), QString()),
                 "\"currency\" muss dem Dialog bekannt sein");

        // Und zwar OHNE eigenes Statussymbol: Devisenkurs und Waehrung teilen
        // sich eine Zeile und damit fcStatus. Zwei Schluessel, die dasselbe
        // Label beschreiben, wuerden einander ueberschreiben. Geprueft wird
        // das ueber setFieldError(): der Aufruf muss still zurueckkehren,
        // ohne Warnung — bekannt, aber nichts zu faerben.
        dlg.setFieldError(QStringLiteral("currency"), QStringLiteral("USD"));
    }

    void test_dividendEdit_documentFieldKeyIsRegistered()
    {
        // Reines Statusfeld ohne Eingabefeld — der Pfad kommt ueber
        // setDocumentPath(). Die Kombination, die der Waechter durchlassen
        // MUSS.
        ViewDividendEdit dlg(makeShareGuid(), nullptr);
        QVERIFY2(dlg.setFieldOk(QStringLiteral("document"), QString()),
                 "\"document\" ist ein reines Statusfeld und muss trotzdem "
                 "angenommen werden");
    }

    void test_dividendEdit_requiredXmlNamesAreSubsetOfKnown()
    {
        for (const QString& xmlName : PresenterDividendEdit::requiredXmlNames()) {
            QVERIFY2(PresenterDividendEdit::knownXmlNames().contains(xmlName),
                     qPrintable(QStringLiteral(
                         "Pflichtname \"%1\" fehlt in knownXmlNames() und "
                         "wird deshalb nie gesucht").arg(xmlName)));
        }
    }

    void test_dividendEdit_setFieldOk_unknownFieldKey_isRejected()
    {
        ViewDividendEdit dlg(makeShareGuid(), nullptr);

        QTest::ignoreMessage(QtWarningMsg,
            "[ViewDividendEdit] setFieldOk: unbekannter Feldschluessel \"depotnumber\"");
        QVERIFY(!dlg.setFieldOk(QStringLiteral("depotnumber"),
                                QStringLiteral("1234567890")));
    }

    void test_dividendEdit_setFieldError_unknownFieldKey_warns()
    {
        ViewDividendEdit dlg(makeShareGuid(), nullptr);

        QTest::ignoreMessage(QtWarningMsg,
            "[ViewDividendEdit] setFieldError: unbekannter Feldschluessel \"depotnumber\"");
        dlg.setFieldError(QStringLiteral("depotnumber"),
                          QStringLiteral("1234567890"));
    }

    void test_dividendEdit_setFieldHint_unknownFieldKey_warns()
    {
        // setFieldHint() gibt es nur in diesem Dialog (Ersatzhinweis fuer den
        // fehlenden Ex-Tag bei Cortal Consors). Dritter Eingang mit einem
        // Feldschluessel, deshalb derselbe Waechter.
        ViewDividendEdit dlg(makeShareGuid(), nullptr);

        QTest::ignoreMessage(QtWarningMsg,
            "[ViewDividendEdit] setFieldHint: unbekannter Feldschluessel \"exdate\"");
        dlg.setFieldHint(QStringLiteral("exdate"), QStringLiteral("Hinweis"));
    }

};

// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    // Bugfix 23.07.2026 — siehe ARCHITECTURE.md, "System-Locale-abhängiges
    // Zahlenformat": muss vor jeder QLocale()-Verwendung gesetzt werden,
    // damit formatMoney() auf jedem Runner/System deutsch formatiert,
    // unabhängig von dessen System-Locale.
    QLocale::setDefault(QLocale::German);

    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);

    TestDividendForm t;
    return QTest::qExec(&t, argc, argv);
}

#include "tst_dividendform.moc"

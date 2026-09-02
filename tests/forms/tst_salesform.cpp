// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// tst_salesform.cpp — Unit tests for the SalesForm MVP triad
// (ModelSaleEdit, PresenterSaleEdit, ViewSaleEdit).
//
// Aus tst_mainwindow.cpp herausgelöst (22.08.2026) — analog tst_buysform und
// tst_dividendform. tst_mainwindow.cpp war auf über 11.000 Zeilen mit fünf
// Testklassen gewachsen; die Konvention „ein Testziel je Form" ist damit
// wiederhergestellt. Reine Auslagerung: die Testmethoden, die beiden Stubs
// (StubModelSaleEdit, StubViewSaleEdit) und der Klassen-Helper makeSale() sind
// unverändert übernommen, es kam keine Prüfung dazu und es fiel keine weg.
//
// main() setzt — wie zuvor tst_mainwindow.cpp — QLocale::setDefault(
// QLocale::German), bevor QApplication konstruiert wird: die Tests vergleichen
// formatierte Beträge („1,50", „0,0000 %") und würden auf einem Runner mit
// englischer System-Locale sonst fehlschlagen. Siehe ARCHITECTURE.md,
// "System-Locale-abhängiges Zahlenformat".

#include <QtTest>
#include <QSignalSpy>
#include <QApplication>
#include <QTemporaryDir>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QUuid>
#include <QLocale>
#include <QDate>
#include <QDateTime>
#include <QTime>
#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QTableWidget>
#include <QAbstractItemView>
#include <QSqlDatabase>
#include <QSqlQuery>

#include "../../app/config/AppSettings.h"
#include "../../app/config/DocumentsConfig.h"
#include "../../app/core/Database.h"
#include "../../app/repositories/ShareRepository.h"
#include "../../app/repositories/BuyRepository.h"
#include "../../app/repositories/SaleRepository.h"
#include "../../app/repositories/BrokerageRepository.h"
#include "../../app/repositories/ShareSplitRepository.h"
#include "../../app/models/ShareObject.h"
#include "../../app/models/BuyObject.h"
#include "../../app/models/SaleObject.h"
#include "../../app/models/BrokerageObject.h"
#include "../../app/models/ShareSplitObject.h"
#include "../../app/utils/ShareSplitHint.h"
#include "../../app/utils/SaleFifoAllocator.h"
#include "../../app/widgets/OverviewTabWidget.h"

#include "../../app/forms/SalesForm/SaleBuyDetailRow.h"
#include "../../app/forms/SalesForm/IViewSaleEdit.h"
#include "../../app/forms/SalesForm/IModelSaleEdit.h"
#include "../../app/forms/SalesForm/ViewSaleEdit.h"
#include "../../app/forms/SalesForm/ModelSaleEdit.h"
#include "../../app/forms/SalesForm/PresenterSaleEdit.h"

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
            void setFieldError(const QString& f, const QString& = QString()) override
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
            bool setFieldOk(const QString& f, const QString&,
                            const QString& = QString()) override
                { if (f == QLatin1String("volume")) *called = true;  return true; }
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

    // ── Split-Diagnose in der Unterdeckungs-Meldung ───────────────────────
    // Punkt 1 der Split-Plausibilitätsprüfung (22.08.2026, siehe
    // ARCHITECTURE.md, "Plausibilitätsprüfung des Split-Verhältnisses").
    // Die Mengenprüfung blockierte bereits, zeigte aber nur auf den Verkauf.
    // Naheliegend — und falsch — wäre dann, die Menge auf dem Beleg zu
    // "korrigieren", statt das Split-Verhältnis zu berichtigen.

    void test_presenterSaleEdit_onSave_splitBetween_errorNamesProposedRatio()
    {
        // Feldfall Alphabet: 10 Stück gekauft, Split am 18.07.2022 aus der
        // Bank-Schreibweise "1:19" fälschlich als 19:1 erfasst (-> 190 Stück
        // heute), Verkaufsbeleg über 200. Richtig wäre 20:1 gewesen.
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        model.availableBuys = {
            BuyObject(QStringLiteral("b1"), QStringLiteral("share-1"),
                      QStringLiteral("depot1"), QString(),
                      QStringLiteral("2021-03-18T10:00:00"), 10.0, 0.0, 971.90)
        };
        model.splits << ShareSplitObject(QStringLiteral("s1"), QStringLiteral("share-1"),
                                         QDate(2022, 7, 18), 19.0, 1.0);
        view.m_dateTime = QStringLiteral("2022-12-05T10:00:00");
        view.m_volume   = 200.0;

        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onSave();

        QVERIFY(!model.addSaleCalled);
        QVERIFY2(view.lastError.contains(QStringLiteral("20:1")),
                 qPrintable(view.lastError));
        // Die Bank-Schreibweise gehört mit in den Text — ohne sie bleibt
        // unklar, WARUM 19 statt 20 im Formular stand.
        QVERIFY2(view.lastError.contains(QStringLiteral("1:19")),
                 qPrintable(view.lastError));
    }

    void test_presenterSaleEdit_onSave_splitBetween_keepsOriginalQuantityMessage()
    {
        // Die Diagnose kommt HINZU, sie ersetzt nichts: beide Mengen müssen
        // weiterhin konkret in der Meldung stehen.
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        model.availableBuys = {
            BuyObject(QStringLiteral("b1"), QStringLiteral("share-1"),
                      QStringLiteral("depot1"), QString(),
                      QStringLiteral("2021-03-18T10:00:00"), 10.0, 0.0, 971.90)
        };
        model.splits << ShareSplitObject(QStringLiteral("s1"), QStringLiteral("share-1"),
                                         QDate(2022, 7, 18), 19.0, 1.0);
        view.m_dateTime = QStringLiteral("2022-12-05T10:00:00");
        view.m_volume   = 200.0;

        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onSave();

        QVERIFY2(view.lastError.contains(QLocale().toString(200.0, 'f', 4)),
                 qPrintable(view.lastError));
        QVERIFY2(view.lastError.contains(QLocale().toString(190.0, 'f', 4)),
                 qPrintable(view.lastError));
    }

    void test_presenterSaleEdit_onSave_noSplit_errorMentionsNoSplit()
    {
        // Ohne Split dazwischen ist die Unterdeckung schlicht eine zu hohe
        // Menge — ein Split-Hinweis wäre hier irreführend.
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
        QVERIFY2(!view.lastError.contains(QStringLiteral("Split")),
                 qPrintable(view.lastError));
    }

    void test_presenterSaleEdit_onSave_volumeTypo_namesSplitButProposesNoRatio()
    {
        // 2.000 statt 200 getippt. Die Rückrechnung ergäbe formal ein
        // sauberes Verhältnis (190:1), das aber vollkommen irreführend wäre.
        // Der Split darf genannt werden, eine Zahl nicht.
        StubViewSaleEdit  view;
        StubModelSaleEdit model;
        model.availableBuys = {
            BuyObject(QStringLiteral("b1"), QStringLiteral("share-1"),
                      QStringLiteral("depot1"), QString(),
                      QStringLiteral("2021-03-18T10:00:00"), 10.0, 0.0, 971.90)
        };
        model.splits << ShareSplitObject(QStringLiteral("s1"), QStringLiteral("share-1"),
                                         QDate(2022, 7, 18), 19.0, 1.0);
        view.m_dateTime = QStringLiteral("2022-12-05T10:00:00");
        view.m_volume   = 2000.0;

        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);
        p.onSave();

        QVERIFY(!model.addSaleCalled);
        QVERIFY2(view.lastError.contains(QStringLiteral("19:1")),
                 qPrintable(view.lastError));
        QVERIFY2(!view.lastError.contains(QStringLiteral("190:1")),
                 qPrintable(view.lastError));
        QVERIFY2(!view.lastError.contains(QStringLiteral("Zuteilungsverhältnis")),
                 qPrintable(view.lastError));
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
            void setFieldError(const QString& f, const QString& = QString()) override
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
            void setFieldError(const QString& f, const QString& = QString()) override
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
            void setFieldError(const QString& f, const QString& = QString()) override
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
            bool setFieldOk(const QString& f, const QString&,
                            const QString& = QString()) override
                { if (f == QLatin1String("document")) *called = true;  return true; }
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
            combo->addItem(QStringLiteral("Testdepot"), QStringLiteral("8006189848"));
            combo->setCurrentIndex(combo->count() - 1);
            if (dlg.depotNumber() == QStringLiteral("8006189848"))
                break;
            combo->removeItem(combo->count() - 1);
            combo->setCurrentIndex(before);
        }
        QCOMPARE(dlg.depotNumber(), QStringLiteral("8006189848"));

        dlg.setFieldOk(QStringLiteral("depotNumber"), QStringLiteral("8006189848"));
        dlg.setFieldOk(QStringLiteral("orderNumber"), QStringLiteral("ORD-S-001"));
        dlg.setFieldOk(QStringLiteral("volume"),      QStringLiteral("10"));
        dlg.setFieldOk(QStringLiteral("salePrice"),   QStringLiteral("150.00"));

        QStringList missing;
        QVERIFY(!dlg.hasMissingRequiredFields(missing));
    }

    /**
     * @brief Der DKB-Verkaufsbeleg liefert Datum UND Uhrzeit in einem Fang.
     *
     * Nessies Bugreport 22.08.2026: "Es wird auch das Kaufdatum nicht aus dem
     * Dokument ausgelesen." Die Bank beschriftet beides gemeinsam
     * ("Schlusstag/-Zeit  27.02.2020 19:16:37"), die Regeln `Date` und `Time`
     * fangen deshalb denselben Text. Vorher ging der ganze Fang an
     * `QDate::fromString(…, "d.M.yyyy")`, schlug fehl, und im Formular blieb
     * still das HEUTIGE Datum stehen — das ging als Verkaufstag in die
     * FIFO-Zuordnung ein.
     */
    void test_viewSaleEdit_setFieldOk_combinedDateAndTime()
    {
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);

        const QString combined = QStringLiteral(" 27.02.2020 19:16:37 ");
        dlg.setFieldOk(QStringLiteral("date"), combined);
        dlg.setFieldOk(QStringLiteral("time"), combined);

        const QDateTime dt = QDateTime::fromString(dlg.dateTime(), Qt::ISODate);
        QVERIFY(dt.isValid());
        QCOMPARE(dt.date(), QDate(2020, 2, 27));
        QCOMPARE(dt.time(), QTime(19, 16, 37));
    }

    /// Ordernummer 1:1 — auch im Verkaufsdialog (Nessies zweiter Screenshot).
    void test_viewSaleEdit_setFieldOk_orderNumber_keepsDot()
    {
        openMemoryDb();
        ViewSaleEdit dlg(QStringLiteral("share-guid"), nullptr);

        dlg.setFieldOk(QStringLiteral("orderNumber"),
                       QStringLiteral(" 267621/08.00\n"));

        QCOMPARE(dlg.orderNumber(), QStringLiteral("267621/08.00"));
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
            bool setFieldOk(const QString& f, const QString&,
                            const QString& = QString()) override
                { if (f == QLatin1String("orderNumber")) *called = true;  return true; }
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

    void test_presenterSaleEdit_populateFromResult_allFieldsTaken_reportsOk()
    {
        StubViewSaleEdit view;
        StubModelSaleEdit model;
        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);

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

    void test_presenterSaleEdit_populateFromResult_rejectedRequiredField_reportsFailed()
    {
        StubViewSaleEdit view;
        view.failingFields << QStringLiteral("depotNumber");

        StubModelSaleEdit model;
        PresenterSaleEdit p(&view, &model, QStringLiteral("share-1"), nullptr);

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
    // Feldschluessel-Tabellen (02.09.2026) — Runde 3, gleiches Muster wie
    // tst_shareaddform/tst_buysform. Siehe ARCHITECTURE.md,
    // "Feldschluessel-Tabellen sind an keiner Stelle geprueft".
    //
    // Gegen einen ECHTEN ViewSaleEdit, nicht gegen den Stub: nur der echte
    // Dialog fuellt m_inputWidgets und m_statusLabels, und genau deren Inhalt
    // ist die Frage.
    //
    // Fuer dieses Formular sind die Pruefungen wertvoller als fuer die
    // beiden anderen: hier weichen zwei Feldschluessel von den Namen im
    // Beleg ab ("Price" wird zu "salePrice"), und drei Steuerfelder kommen
    // hinzu, die es sonst nirgends gibt.
    // ─────────────────────────────────────────────────────────────────────

    void test_saleEdit_everyKnownXmlNameHasAViewField()
    {
        for (const QString& xmlName : PresenterSaleEdit::knownXmlNames()) {
            const QString viewField = PresenterSaleEdit::xmlNameToViewField(xmlName);
            QVERIFY2(!viewField.isEmpty(),
                     qPrintable(QStringLiteral(
                         "knownXmlNames() fuehrt \"%1\", "
                         "xmlNameToViewField() kennt den Namen aber nicht")
                         .arg(xmlName)));
        }
    }

    void test_saleEdit_everyViewFieldIsRegisteredInTheDialog()
    {
        // Der leere Rohwert ist die in IViewSaleEdit.h dokumentierte
        // Aufrufart der Live-Validierung: nur das Symbol setzen, den
        // Feldinhalt nicht anfassen.
        ViewSaleEdit dlg(QStringLiteral("share-guid"), &m_docsConfig);

        for (const QString& xmlName : PresenterSaleEdit::knownXmlNames()) {
            const QString viewField = PresenterSaleEdit::xmlNameToViewField(xmlName);
            if (viewField.isEmpty())
                continue;   // eigener Test oben

            QVERIFY2(dlg.setFieldOk(viewField, QString()),
                     qPrintable(QStringLiteral(
                         "Feldschluessel \"%1\" (aus XML-Name \"%2\") ist im "
                         "Dialog weder als Eingabefeld noch als Symbol "
                         "registriert").arg(viewField, xmlName)));
        }
    }

    void test_saleEdit_priceMapsToSalePriceNotPrice()
    {
        // Die eine Abweichung, die dieses Formular von Kauf und ShareAdd
        // unterscheidet. Sie ist leicht zu uebersehen, wenn jemand die
        // Tabellen zwischen den Formularen kopiert — und "price" waere in
        // dieser Maske ein unbekannter Schluessel, der Verkaufspreis ginge
        // still verloren.
        QCOMPARE(PresenterSaleEdit::xmlNameToViewField(QStringLiteral("Price")),
                 QStringLiteral("salePrice"));

        ViewSaleEdit dlg(QStringLiteral("share-guid"), &m_docsConfig);
        QTest::ignoreMessage(QtWarningMsg,
            "[ViewSaleEdit] setFieldOk: unbekannter Feldschluessel \"price\"");
        QVERIFY2(!dlg.setFieldOk(QStringLiteral("price"), QString()),
                 "\"price\" darf in dieser Maske gerade NICHT registriert sein");
    }

    void test_saleEdit_documentFieldKeyIsRegistered()
    {
        // "document" steht in keiner der XML-Tabellen — der Pfad kommt ueber
        // setDocumentPath(), nicht aus einer Regel. Die Live-Validierung
        // (PresenterSaleEdit::onDocumentPathEdited()) benutzt den Schluessel
        // trotzdem, und er hat ein Symbol ohne Eingabefeld. Genau die
        // Kombination, die der Waechter durchlassen MUSS.
        ViewSaleEdit dlg(QStringLiteral("share-guid"), &m_docsConfig);

        QVERIFY2(dlg.setFieldOk(QStringLiteral("document"), QString()),
                 "\"document\" ist ein reines Statusfeld und muss trotzdem "
                 "angenommen werden");
    }

    void test_saleEdit_requiredXmlNamesAreSubsetOfKnown()
    {
        for (const QString& xmlName : PresenterSaleEdit::requiredXmlNames()) {
            QVERIFY2(PresenterSaleEdit::knownXmlNames().contains(xmlName),
                     qPrintable(QStringLiteral(
                         "Pflichtname \"%1\" fehlt in knownXmlNames() und "
                         "wird deshalb nie gesucht").arg(xmlName)));
        }
    }

    void test_saleEdit_setFieldOk_unknownFieldKey_isRejected()
    {
        ViewSaleEdit dlg(QStringLiteral("share-guid"), &m_docsConfig);

        QTest::ignoreMessage(QtWarningMsg,
            "[ViewSaleEdit] setFieldOk: unbekannter Feldschluessel \"depotnumber\"");
        QVERIFY(!dlg.setFieldOk(QStringLiteral("depotnumber"),
                                QStringLiteral("1234567890")));
    }

    void test_saleEdit_setFieldError_unknownFieldKey_warns()
    {
        ViewSaleEdit dlg(QStringLiteral("share-guid"), &m_docsConfig);

        QTest::ignoreMessage(QtWarningMsg,
            "[ViewSaleEdit] setFieldError: unbekannter Feldschluessel \"depotnumber\"");
        dlg.setFieldError(QStringLiteral("depotnumber"),
                          QStringLiteral("1234567890"));
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

    TestSalesForm t;
    return QTest::qExec(&t, argc, argv);
}

#include "tst_salesform.moc"

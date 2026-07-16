// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include <QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>

#include "../../app/models/SaleObject.h"
#include "../../app/models/BrokerageObject.h"
#include "../../app/repositories/SaleRepository.h"
#include "../../app/repositories/BuyRepository.h"
#include "../../app/repositories/BrokerageRepository.h"
#include "../../app/core/Database.h"

class TestSaleRepository : public QObject
{
    Q_OBJECT

private:
    QString newGuid() const { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
    const QString k_shareGuid = QStringLiteral("test-share-sale-0001");

    // Helper: create a typical SaleBuyDetail list
    QList<SaleBuyDetail> makeBuyDetails(const QString& buyGuid) const {
        return { SaleBuyDetail(buyGuid, "2024-01-01T10:00:00", 5.0, 100.0, 1.5, 4.0) };
    }

private slots:

    void initTestCase()
    {
        QVERIFY(Database::instance().open(":memory:"));
        Database::instance().execute(
            QString("INSERT INTO shares (guid, wkn, name) VALUES ('%1', 'TEST01', 'Test Share')")
                .arg(k_shareGuid));
    }

    void cleanupTestCase()
    {
        Database::instance().close();
    }

    void init()
    {
        // Reihenfolge nach Abhängigkeit: sale_buy_details (Kind von sales/buys)
        // und brokerage (Kind von sales via brokerage.sale_guid, siehe
        // test_updateBrokerageGuid) zuerst, sonst schlägt "DELETE FROM sales"
        // mit "FOREIGN KEY constraint failed" fehl und die betroffene Zeile
        // bleibt stehen — kontaminiert dann nachfolgende Tests (Bugfix
        // 16.07.2026, siehe ARCHITECTURE.md: test_totalVolume lieferte durch
        // eine so liegen gebliebene Sale-Zeile aus test_updateBrokerageGuid
        // 13.0 statt 8.0).
        Database::instance().execute("DELETE FROM sale_buy_details");
        Database::instance().execute("DELETE FROM brokerage");
        Database::instance().execute("DELETE FROM sales");
        Database::instance().execute("DELETE FROM buys");
    }

    // ── SaleBuyDetail ─────────────────────────────────────────────────────
    void test_saleBuyDetail_calculateValues()
    {
        SaleBuyDetail d("buy-001", "2024-01-01T10:00:00",
                        5.0,    // volume
                        100.0,  // buyPrice
                        1.5,    // reductionPart
                        4.0);   // brokeragePart

        QCOMPARE(d.saleBuyValue(),                   500.0); // 5*100
        QCOMPARE(d.saleBuyValueReduction(),          498.5); // 500-1.5
        QCOMPARE(d.saleBuyValueBrokerage(),          504.0); // 500+4
        QCOMPARE(d.saleBuyValueBrokerageReduction(), 502.5); // 500+4-1.5
    }

    // ── SaleObject ────────────────────────────────────────────────────────
    void test_saleObject_calculateValues()
    {
        const auto details = makeBuyDetails("buy-001");

        SaleObject sale(newGuid(), k_shareGuid, "D001", "O001",
                        "2024-06-01T10:00:00",
                        5.0,    // volume
                        110.0,  // salePrice
                        details,
                        10.0,   // taxAtSource
                        5.0,    // capitalGainsTax
                        1.0,    // solidarityTax
                        QString(), // brokerageGuid
                        3.0,    // provision
                        1.0,    // brokerFee
                        0.5,    // traderFee
                        2.0);   // reduction

        QCOMPARE(sale.taxSum(),    16.0);  // 10+5+1
        QCOMPARE(sale.brokerage(), 4.5);   // 3+1+0.5
        QCOMPARE(sale.saleValue(), 550.0); // 5*110

        // ProfitLoss = saleValue - buyValue - taxSum
        // buyValue = 500, saleValue = 550, taxSum = 16
        QCOMPARE(sale.profitLoss(), 34.0); // 550-500-16

        // PayoutBrokerageReduction = saleValue - brokerage + reduction - taxSum
        // = 550 - 4.5 + 2 - 16 = 531.5
        QCOMPARE(sale.payoutBrokerageReduction(), 531.5);
    }

    void test_saleObject_isValid()
    {
        SaleObject valid(newGuid(), k_shareGuid, "", "", "2024-01-01T10:00:00",
                         5.0, 110.0, {});
        QVERIFY(valid.isValid());

        SaleObject invalid;
        QVERIFY(!invalid.isValid());
    }

    void test_saleObject_year()
    {
        SaleObject sale(newGuid(), k_shareGuid, "", "", "2024-06-15T10:00:00",
                        5.0, 110.0, {});
        QCOMPARE(sale.year(), 2024);
    }

    // ── SaleRepository ────────────────────────────────────────────────────
    void test_insert_and_findByGuid()
    {
        SaleRepository repo;
        const QString guid    = newGuid();
        const QString buyGuid = newGuid();

        // Insert parent buy first (FK constraint)
        BuyRepository buyRepo;
        buyRepo.insert(BuyObject(buyGuid, k_shareGuid, "", "BO001",
                                 "2024-01-01T10:00:00", 10.0, 0.0, 100.0));

        SaleObject sale(guid, k_shareGuid, "D001", "SO001",
                        "2024-06-01T10:00:00",
                        5.0, 110.0, makeBuyDetails(buyGuid));

        QVERIFY(repo.insert(sale));

        const auto found = repo.findByGuid(guid);
        QVERIFY(found.isValid());
        QCOMPARE(found.guid(),      guid);
        QCOMPARE(found.volume(),    5.0);
        QCOMPARE(found.salePrice(), 110.0);
        QCOMPARE(found.saleBuyDetails().size(), 1);
        QCOMPARE(found.saleBuyDetails().constFirst().buyGuid(), buyGuid);
    }

    void test_findByShare_orderedByDate()
    {
        SaleRepository repo;
        BuyRepository  buyRepo;

        const QString b1 = newGuid(), b2 = newGuid();
        buyRepo.insert(BuyObject(b1, k_shareGuid, "", "BO002", "2024-01-01T10:00:00", 10.0, 0.0, 100.0));
        buyRepo.insert(BuyObject(b2, k_shareGuid, "", "BO003", "2024-01-02T10:00:00", 10.0, 0.0, 100.0));

        repo.insert(SaleObject(newGuid(), k_shareGuid, "", "SO003",
                               "2024-09-01T10:00:00", 5.0, 115.0, makeBuyDetails(b2)));
        repo.insert(SaleObject(newGuid(), k_shareGuid, "", "SO002",
                               "2024-06-01T10:00:00", 5.0, 110.0, makeBuyDetails(b1)));

        const auto sales = repo.findByShare(k_shareGuid);
        QCOMPARE(sales.size(), 2);
        QVERIFY(sales[0].dateTime() < sales[1].dateTime());
    }

    void test_findByShareAndYear()
    {
        SaleRepository repo;
        BuyRepository  buyRepo;

        const QString b1 = newGuid(), b2 = newGuid();
        buyRepo.insert(BuyObject(b1, k_shareGuid, "", "BO004", "2023-01-01T10:00:00", 10.0, 0.0, 90.0));
        buyRepo.insert(BuyObject(b2, k_shareGuid, "", "BO005", "2024-01-01T10:00:00", 10.0, 0.0, 100.0));

        repo.insert(SaleObject(newGuid(), k_shareGuid, "", "SO004",
                               "2023-06-01T10:00:00", 5.0, 95.0, makeBuyDetails(b1)));
        repo.insert(SaleObject(newGuid(), k_shareGuid, "", "SO005",
                               "2024-06-01T10:00:00", 5.0, 110.0, makeBuyDetails(b2)));

        const auto sales2024 = repo.findByShareAndYear(k_shareGuid, 2024);
        QCOMPARE(sales2024.size(), 1);
        QCOMPARE(sales2024.first().year(), 2024);
    }

    void test_update()
    {
        SaleRepository repo;
        BuyRepository  buyRepo;

        const QString guid = newGuid(), buyGuid = newGuid();
        buyRepo.insert(BuyObject(buyGuid, k_shareGuid, "", "BO006",
                                 "2024-01-01T10:00:00", 10.0, 0.0, 100.0));
        repo.insert(SaleObject(guid, k_shareGuid, "D001", "SO006",
                               "2024-06-01T10:00:00", 5.0, 110.0, makeBuyDetails(buyGuid)));

        SaleObject updated(guid, k_shareGuid, "D002", "SO006",
                           "2024-06-01T10:00:00", 8.0, 115.0, makeBuyDetails(buyGuid));
        QVERIFY(repo.update(updated));

        const auto found = repo.findByGuid(guid);
        QCOMPARE(found.volume(),      8.0);
        QCOMPARE(found.salePrice(),   115.0);
        QCOMPARE(found.depotNumber(), QString("D002"));
    }

    void test_updateBrokerageGuid()
    {
        // Regressionstest für den Bugfix vom 15.07.2026 (siehe ARCHITECTURE.md,
        // "SalesForm-Details", ModelSaleEdit): sales.brokerage_guid ist der
        // Vorwärts-Link, den findByGuid()/findByShare()/findByShareAndYear()
        // für ihren Brokerage-JOIN nutzen (kSelectWithBrokerage). Ohne ihn
        // liefert provision() beim Laden immer 0 zurück, selbst wenn ein
        // gültiger Brokerage-Datensatz existiert und über den Rückwärts-Link
        // (brokerage.sale_guid, z.B. BrokerageRepository::findBySaleGuid())
        // durchaus auffindbar wäre.
        SaleRepository      repo;
        BuyRepository       buyRepo;
        BrokerageRepository brokerageRepo;

        const QString guid          = newGuid();
        const QString buyGuid       = newGuid();
        const QString brokerageGuid = newGuid();

        buyRepo.insert(BuyObject(buyGuid, k_shareGuid, "", "BO010",
                                 "2024-01-01T10:00:00", 10.0, 0.0, 100.0));
        repo.insert(SaleObject(guid, k_shareGuid, "", "SO010",
                               "2024-06-01T10:00:00", 5.0, 110.0, makeBuyDetails(buyGuid)));

        // Brokerage separat anlegen, zunächst nur über den Rückwärts-Link
        // (sale_guid) verknüpft — wie ModelSaleEdit::addSale() es vor dem Fix
        // tat, ohne sales.brokerage_guid zu setzen.
        brokerageRepo.insert(BrokerageObject(brokerageGuid, k_shareGuid, QString(), guid,
                                             "2024-06-01T10:00:00",
                                             /*provision=*/7.5));

        // Vor dem Vorwärts-Link: der JOIN in findByGuid() findet nichts.
        QCOMPARE(repo.findByGuid(guid).provision(), 0.0);

        QVERIFY(repo.updateBrokerageGuid(guid, brokerageGuid));

        // Nach dem Vorwärts-Link: provision() kommt korrekt über den JOIN zurück.
        QCOMPARE(repo.findByGuid(guid).provision(), 7.5);
    }

    void test_remove()
    {
        SaleRepository repo;
        BuyRepository  buyRepo;

        const QString guid = newGuid(), buyGuid = newGuid();
        buyRepo.insert(BuyObject(buyGuid, k_shareGuid, "", "BO007",
                                 "2024-01-01T10:00:00", 10.0, 0.0, 100.0));
        repo.insert(SaleObject(guid, k_shareGuid, "", "SO007",
                               "2024-06-01T10:00:00", 5.0, 110.0, makeBuyDetails(buyGuid)));

        QVERIFY(repo.remove(guid));
        QVERIFY(!repo.findByGuid(guid).isValid());
    }

    void test_totalVolume()
    {
        SaleRepository repo;
        BuyRepository  buyRepo;

        const QString b1 = newGuid(), b2 = newGuid();
        buyRepo.insert(BuyObject(b1, k_shareGuid, "", "BO008", "2024-01-01T10:00:00", 10.0, 0.0, 100.0));
        buyRepo.insert(BuyObject(b2, k_shareGuid, "", "BO009", "2024-01-02T10:00:00", 10.0, 0.0, 100.0));

        repo.insert(SaleObject(newGuid(), k_shareGuid, "", "SO008",
                               "2024-06-01T10:00:00", 5.0, 110.0, makeBuyDetails(b1)));
        repo.insert(SaleObject(newGuid(), k_shareGuid, "", "SO009",
                               "2024-09-01T10:00:00", 3.0, 115.0, makeBuyDetails(b2)));

        QCOMPARE(repo.totalVolume(k_shareGuid), 8.0);
    }
};

QTEST_MAIN(TestSaleRepository)
#include "tst_salerepository.moc"

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include <QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>

#include "../../app/models/BuyObject.h"
#include "../../app/models/BrokerageObject.h"
#include "../../app/repositories/BuyRepository.h"
#include "../../app/repositories/BrokerageRepository.h"
#include "../../app/core/Database.h"

class TestBuyRepository : public QObject
{
    Q_OBJECT

private:
    QString newGuid() const { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
    const QString k_shareGuid = QStringLiteral("test-share-0001");

private slots:

    void initTestCase()
    {
        // Use in-memory SQLite database for tests
        QVERIFY(Database::instance().open(":memory:"));

        // Insert a parent share record — required by foreign key constraint
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
        // Clean buys and brokerage tables before each test
        Database::instance().execute("DELETE FROM brokerage");
        Database::instance().execute("DELETE FROM buys");
    }

    // ── BuyObject ─────────────────────────────────────────────────────────
    void test_buyObject_calculateValues()
    {
        // Brokerage fields were removed from BuyObject — they live in BrokerageObject.
        // BuyObject only calculates buyValue = volume * price.
        BuyObject buy(newGuid(), k_shareGuid, "D001", "O001",
                      "2024-01-15T10:00:00",
                      10.0,   // volume
                      0.0,    // volumeSold
                      100.0); // price

        QCOMPARE(buy.buyValue(), 1000.0);   // 10 * 100
    }

    void test_buyObject_isValid()
    {
        BuyObject valid(newGuid(), k_shareGuid, "", "", "2024-01-15T10:00:00",
                        10.0, 0.0, 100.0);
        QVERIFY(valid.isValid());

        BuyObject invalid;
        QVERIFY(!invalid.isValid());
    }

    void test_buyObject_year()
    {
        BuyObject buy(newGuid(), k_shareGuid, "", "", "2024-06-15T10:00:00",
                      10.0, 0.0, 100.0);
        QCOMPARE(buy.year(), 2024);
    }

    // ── BuyRepository ─────────────────────────────────────────────────────
    void test_insert_and_findByGuid()
    {
        BuyRepository repo;
        const QString guid = newGuid();
        BuyObject buy(guid, k_shareGuid, "D001", "O001",
                      "2024-01-15T10:00:00", 10.0, 0.0, 100.0);

        QVERIFY(repo.insert(buy));

        const auto found = repo.findByGuid(guid);
        QVERIFY(found.isValid());
        QCOMPARE(found.guid(),       guid);
        QCOMPARE(found.shareGuid(),  k_shareGuid);
        QCOMPARE(found.volume(),     10.0);
        QCOMPARE(found.price(),      100.0);
    }

    void test_findByShare_orderedByDate()
    {
        BuyRepository repo;
        repo.insert(BuyObject(newGuid(), k_shareGuid, "", "O002",
                              "2024-03-01T10:00:00", 5.0, 0.0, 110.0));
        repo.insert(BuyObject(newGuid(), k_shareGuid, "", "O001",
                              "2024-01-01T10:00:00", 10.0, 0.0, 100.0));

        const auto buys = repo.findByShare(k_shareGuid);
        QCOMPARE(buys.size(), 2);
        // Should be ordered by date ascending
        QVERIFY(buys[0].dateTime() < buys[1].dateTime());
    }

    void test_findByShareAndYear()
    {
        BuyRepository repo;
        repo.insert(BuyObject(newGuid(), k_shareGuid, "", "O2024",
                              "2024-06-01T10:00:00", 10.0, 0.0, 100.0));
        repo.insert(BuyObject(newGuid(), k_shareGuid, "", "O2023",
                              "2023-06-01T10:00:00", 5.0, 0.0, 90.0));

        const auto buys2024 = repo.findByShareAndYear(k_shareGuid, 2024);
        QCOMPARE(buys2024.size(), 1);
        QCOMPARE(buys2024.first().year(), 2024);
    }

    void test_orderNumberExists()
    {
        BuyRepository repo;
        repo.insert(BuyObject(newGuid(), k_shareGuid, "", "ORDER-001",
                              "2024-01-01T10:00:00", 10.0, 0.0, 100.0));

        QVERIFY(repo.orderNumberExists(k_shareGuid, "ORDER-001"));
        QVERIFY(!repo.orderNumberExists(k_shareGuid, "ORDER-999"));
    }

    void test_update()
    {
        BuyRepository repo;
        const QString guid = newGuid();
        repo.insert(BuyObject(guid, k_shareGuid, "D001", "O001",
                              "2024-01-01T10:00:00", 10.0, 0.0, 100.0));

        BuyObject updated(guid, k_shareGuid, "D002", "O001",
                          "2024-01-01T10:00:00", 20.0, 0.0, 105.0);
        QVERIFY(repo.update(updated));

        const auto found = repo.findByGuid(guid);
        QCOMPARE(found.volume(),      20.0);
        QCOMPARE(found.price(),       105.0);
        QCOMPARE(found.depotNumber(), QString("D002"));
    }

    void test_updateVolumeSold()
    {
        BuyRepository repo;
        const QString guid = newGuid();
        repo.insert(BuyObject(guid, k_shareGuid, "", "O001",
                              "2024-01-01T10:00:00", 10.0, 0.0, 100.0));

        QVERIFY(repo.updateVolumeSold(guid, 5.0));
        QCOMPARE(repo.findByGuid(guid).volumeSold(), 5.0);
    }

    void test_updateDocument()
    {
        BuyRepository repo;
        const QString guid = newGuid();
        repo.insert(BuyObject(guid, k_shareGuid, "", "O001",
                              "2024-01-01T10:00:00", 10.0, 0.0, 100.0));

        QVERIFY(repo.updateDocument(guid, "/docs/invoice.pdf"));
        QCOMPARE(repo.findByGuid(guid).document(), QString("/docs/invoice.pdf"));
    }

    void test_remove()
    {
        BuyRepository repo;
        const QString guid = newGuid();
        repo.insert(BuyObject(guid, k_shareGuid, "", "O001",
                              "2024-01-01T10:00:00", 10.0, 0.0, 100.0));

        QVERIFY(repo.remove(guid));
        QVERIFY(!repo.findByGuid(guid).isValid());
    }

    void test_totalVolume()
    {
        BuyRepository repo;
        repo.insert(BuyObject(newGuid(), k_shareGuid, "", "O001",
                              "2024-01-01T10:00:00", 10.0, 0.0, 100.0));
        repo.insert(BuyObject(newGuid(), k_shareGuid, "", "O002",
                              "2024-06-01T10:00:00", 5.0, 0.0, 110.0));

        QCOMPARE(repo.totalVolume(k_shareGuid), 15.0);
    }

    void test_totalBuyValueBrokerageReduction()
    {
        BuyRepository buyRepo;
        BrokerageRepository brokerageRepo;

        // Buy1: 10 * 100 = 1000, brokerage 5+2+1-3 = 5 -> total 1005
        const QString buyGuid1 = newGuid();
        const QString brGuid1  = newGuid();
        buyRepo.insert(BuyObject(buyGuid1, k_shareGuid, "", "O001",
                                 "2024-01-01T10:00:00", 10.0, 0.0, 100.0, brGuid1));
        brokerageRepo.insert(BrokerageObject(brGuid1, k_shareGuid, buyGuid1, QString(),
                                             "2024-01-01T10:00:00",
                                             5.0, 2.0, 1.0, 3.0));

        // Buy2: 5 * 110 = 550, no brokerage
        const QString buyGuid2 = newGuid();
        buyRepo.insert(BuyObject(buyGuid2, k_shareGuid, "", "O002",
                                 "2024-06-01T10:00:00", 5.0, 0.0, 110.0));

        QCOMPARE(buyRepo.totalBuyValueBrokerageReduction(k_shareGuid), 1555.0);
    }
};

QTEST_MAIN(TestBuyRepository)
#include "tst_buyrepository.moc"

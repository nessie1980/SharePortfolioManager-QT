// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include <QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>

#include "../../app/models/BrokerageObject.h"
#include "../../app/repositories/BrokerageRepository.h"
#include "../../app/core/Database.h"

class TestBrokerageRepository : public QObject
{
    Q_OBJECT

private:
    QString newGuid() const { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
    const QString k_shareGuid = QStringLiteral("test-share-0001");
    const QString k_buyGuid   = QStringLiteral("test-buy-0001");
    const QString k_saleGuid  = QStringLiteral("test-sale-0001");

private slots:

    void initTestCase()
    {
        // Use in-memory SQLite database for tests
        QVERIFY(Database::instance().open(":memory:"));

        // Insert parent share — required by foreign key constraint
        Database::instance().execute(
            QString("INSERT INTO shares (guid, wkn, name) VALUES ('%1', 'TEST01', 'Test Share')")
                .arg(k_shareGuid));

        // Insert a buy and a sale for FK reference tests
        Database::instance().execute(
            QString("INSERT INTO buys (guid, share_guid, datetime, volume, price) "
                    "VALUES ('%1', '%2', '2024-01-01T10:00:00', 10.0, 100.0)")
                .arg(k_buyGuid, k_shareGuid));
        Database::instance().execute(
            QString("INSERT INTO sales (guid, share_guid, datetime, volume, sale_price) "
                    "VALUES ('%1', '%2', '2024-06-01T10:00:00', 5.0, 120.0)")
                .arg(k_saleGuid, k_shareGuid));
    }

    void cleanupTestCase()
    {
        Database::instance().close();
    }

    void init()
    {
        // Clean brokerage table before each test
        Database::instance().execute("DELETE FROM brokerage");
    }

    // ── BrokerageObject ───────────────────────────────────────────────────

    void test_brokerageObject_calculateValues()
    {
        BrokerageObject brokerage(newGuid(), k_shareGuid, k_buyGuid, QString(),
                                  "2024-01-15T10:00:00",
                                  5.0,   // provision
                                  2.0,   // brokerFee
                                  1.0,   // traderFee
                                  3.0);  // reduction

        QCOMPARE(brokerage.brokerage(),          8.0);  // 5+2+1
        QCOMPARE(brokerage.brokerageReduction(), 5.0);  // 8-3
    }

    void test_brokerageObject_isValid()
    {
        BrokerageObject valid(newGuid(), k_shareGuid, QString(), QString(),
                              "2024-01-15T10:00:00");
        QVERIFY(valid.isValid());

        BrokerageObject invalid;
        QVERIFY(!invalid.isValid());
    }

    void test_brokerageObject_year()
    {
        BrokerageObject brokerage(newGuid(), k_shareGuid, QString(), QString(),
                                  "2024-06-15T10:00:00");
        QCOMPARE(brokerage.year(), 2024);
    }

    // ── BrokerageRepository ───────────────────────────────────────────────

    void test_insert_and_findByGuid()
    {
        BrokerageRepository repo;
        const QString guid = newGuid();
        BrokerageObject brokerage(guid, k_shareGuid, k_buyGuid, QString(),
                                  "2024-01-15T10:00:00", 5.0, 2.0, 1.0, 3.0);

        QVERIFY(repo.insert(brokerage));

        const auto found = repo.findByGuid(guid);
        QVERIFY(found.isValid());
        QCOMPARE(found.guid(),       guid);
        QCOMPARE(found.shareGuid(),  k_shareGuid);
        QCOMPARE(found.buyGuid(),    k_buyGuid);
        QCOMPARE(found.provision(),  5.0);
        QCOMPARE(found.brokerFee(),  2.0);
        QCOMPARE(found.traderFee(),  1.0);
        QCOMPARE(found.reduction(),  3.0);
    }

    void test_findByShare_orderedByDate()
    {
        BrokerageRepository repo;
        repo.insert(BrokerageObject(newGuid(), k_shareGuid, QString(), QString(),
                                    "2024-03-01T10:00:00", 5.0));
        repo.insert(BrokerageObject(newGuid(), k_shareGuid, QString(), QString(),
                                    "2024-01-01T10:00:00", 3.0));

        const auto records = repo.findByShare(k_shareGuid);
        QCOMPARE(records.size(), 2);
        QVERIFY(records[0].dateTime() < records[1].dateTime());
    }

    void test_findByBuyGuid()
    {
        BrokerageRepository repo;
        const QString guid = newGuid();
        repo.insert(BrokerageObject(guid, k_shareGuid, k_buyGuid, QString(),
                                    "2024-01-15T10:00:00", 5.0));

        const auto found = repo.findByBuyGuid(k_buyGuid);
        QVERIFY(found.isValid());
        QCOMPARE(found.guid(),    guid);
        QCOMPARE(found.buyGuid(), k_buyGuid);
    }

    void test_findBySaleGuid()
    {
        BrokerageRepository repo;
        const QString guid = newGuid();
        repo.insert(BrokerageObject(guid, k_shareGuid, QString(), k_saleGuid,
                                    "2024-06-15T10:00:00", 4.0));

        const auto found = repo.findBySaleGuid(k_saleGuid);
        QVERIFY(found.isValid());
        QCOMPARE(found.guid(),     guid);
        QCOMPARE(found.saleGuid(), k_saleGuid);
    }

    void test_findByShareAndYear()
    {
        BrokerageRepository repo;
        repo.insert(BrokerageObject(newGuid(), k_shareGuid, QString(), QString(),
                                    "2024-06-01T10:00:00", 5.0));
        repo.insert(BrokerageObject(newGuid(), k_shareGuid, QString(), QString(),
                                    "2023-06-01T10:00:00", 3.0));

        const auto records2024 = repo.findByShareAndYear(k_shareGuid, 2024);
        QCOMPARE(records2024.size(), 1);
        QCOMPARE(records2024.first().year(), 2024);
    }

    void test_update()
    {
        BrokerageRepository repo;
        const QString guid = newGuid();
        repo.insert(BrokerageObject(guid, k_shareGuid, k_buyGuid, QString(),
                                    "2024-01-15T10:00:00", 5.0, 2.0, 1.0, 0.0));

        BrokerageObject updated(guid, k_shareGuid, k_buyGuid, QString(),
                                "2024-01-15T10:00:00", 6.0, 3.0, 1.5, 2.0);
        QVERIFY(repo.update(updated));

        const auto found = repo.findByGuid(guid);
        QCOMPARE(found.provision(), 6.0);
        QCOMPARE(found.brokerFee(), 3.0);
        QCOMPARE(found.reduction(), 2.0);
    }

    void test_updateDocument()
    {
        BrokerageRepository repo;
        const QString guid = newGuid();
        repo.insert(BrokerageObject(guid, k_shareGuid, QString(), QString(),
                                    "2024-01-15T10:00:00"));

        QVERIFY(repo.updateDocument(guid, "/docs/brokerage.pdf"));
        QCOMPARE(repo.findByGuid(guid).document(), QString("/docs/brokerage.pdf"));
    }

    void test_remove()
    {
        BrokerageRepository repo;
        const QString guid = newGuid();
        repo.insert(BrokerageObject(guid, k_shareGuid, QString(), QString(),
                                    "2024-01-15T10:00:00"));

        QVERIFY(repo.remove(guid));
        QVERIFY(!repo.findByGuid(guid).isValid());
    }

    void test_removeByShare()
    {
        BrokerageRepository repo;
        repo.insert(BrokerageObject(newGuid(), k_shareGuid, QString(), QString(),
                                    "2024-01-01T10:00:00", 5.0));
        repo.insert(BrokerageObject(newGuid(), k_shareGuid, QString(), QString(),
                                    "2024-06-01T10:00:00", 3.0));

        QCOMPARE(repo.removeByShare(k_shareGuid), 2);
        QCOMPARE(repo.findByShare(k_shareGuid).size(), 0);
    }

    void test_totalBrokerage()
    {
        BrokerageRepository repo;
        // Record 1: 5 + 2 + 1 = 8
        repo.insert(BrokerageObject(newGuid(), k_shareGuid, QString(), QString(),
                                    "2024-01-01T10:00:00", 5.0, 2.0, 1.0, 0.0));
        // Record 2: 3 + 1 + 0 = 4
        repo.insert(BrokerageObject(newGuid(), k_shareGuid, QString(), QString(),
                                    "2024-06-01T10:00:00", 3.0, 1.0, 0.0, 0.0));

        QCOMPARE(repo.totalBrokerage(k_shareGuid), 12.0);
    }

    void test_totalBrokerageReduction()
    {
        BrokerageRepository repo;
        // Record 1: 8 - 3 = 5
        repo.insert(BrokerageObject(newGuid(), k_shareGuid, QString(), QString(),
                                    "2024-01-01T10:00:00", 5.0, 2.0, 1.0, 3.0));
        // Record 2: 4 - 1 = 3
        repo.insert(BrokerageObject(newGuid(), k_shareGuid, QString(), QString(),
                                    "2024-06-01T10:00:00", 3.0, 1.0, 0.0, 1.0));

        QCOMPARE(repo.totalBrokerageReduction(k_shareGuid), 8.0);
    }
};

QTEST_MAIN(TestBrokerageRepository)
#include "tst_brokeragerepository.moc"

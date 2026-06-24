// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include <QtTest>
#include <QSqlDatabase>
#include <QUuid>

#include "../../app/models/DividendObject.h"
#include "../../app/repositories/DividendRepository.h"
#include "../../app/core/Database.h"

class TestDividendRepository : public QObject
{
    Q_OBJECT

private:
    QString newGuid() const { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
    const QString k_shareGuid = QStringLiteral("test-share-div-0001");

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
        Database::instance().execute("DELETE FROM dividends");
    }

    // ── DividendObject ────────────────────────────────────────────────────
    void test_calculateValues_domestic()
    {
        DividendObject d(newGuid(), k_shareGuid, "2024-05-15T00:00:00",
                         1.50,   // rate
                         100.0,  // volume
                         5.0,    // taxAtSource
                         3.0,    // capitalGainsTax
                         0.5,    // solidarityTax
                         45.0,   // priceAtPayday
                         false,  // no FC
                         1.0);   // exchangeRatio

        QCOMPARE(d.taxSum(),   8.5);   // 5+3+0.5
        QCOMPARE(d.dividendPayout(),   150.0); // 1.5*100
        QCOMPARE(d.dividendPayoutFc(), 0.0);   // no FC
        QCOMPARE(d.dividendPayoutWithTaxes(), 141.5); // 150-8.5

        // Yield = 1.5 / 45 * 100 = 3.333...
        QVERIFY(qAbs(d.yield() - 3.3333) < 0.001);
    }

    void test_calculateValues_foreign_currency()
    {
        DividendObject d(newGuid(), k_shareGuid, "2024-05-15T00:00:00",
                         2.00,   // rate in FC (USD)
                         100.0,  // volume
                         0.0, 0.0, 0.0,
                         50.0,   // priceAtPayday
                         true,   // FC enabled
                         1.10);  // exchange ratio USD→EUR

        // payoutFc = 2.0 * 100 = 200 USD
        QCOMPARE(d.dividendPayoutFc(), 200.0);
        // payout = 200 / 1.10 ≈ 181.82 EUR
        QVERIFY(qAbs(d.dividendPayout() - 181.82) < 0.01);
        QCOMPARE(d.dividendPayoutWithTaxes(), d.dividendPayout()); // no taxes
    }

    void test_isValid()
    {
        DividendObject valid(newGuid(), k_shareGuid, "2024-05-15T00:00:00",
                             1.5, 100.0);
        QVERIFY(valid.isValid());

        DividendObject invalid;
        QVERIFY(!invalid.isValid());
    }

    void test_year()
    {
        DividendObject d(newGuid(), k_shareGuid, "2024-05-15T00:00:00",
                         1.5, 100.0);
        QCOMPARE(d.year(), 2024);
    }

    // ── DividendRepository ────────────────────────────────────────────────
    void test_insert_and_findByGuid()
    {
        DividendRepository repo;
        const QString guid = newGuid();

        DividendObject d(guid, k_shareGuid, "2024-05-15T00:00:00",
                         1.50, 100.0, 5.0, 3.0, 0.5, 45.0);
        QVERIFY(repo.insert(d));

        const auto found = repo.findByGuid(guid);
        QVERIFY(found.isValid());
        QCOMPARE(found.guid(),       guid);
        QCOMPARE(found.rate(),       1.50);
        QCOMPARE(found.volume(),     100.0);
        QCOMPARE(found.taxAtSource(), 5.0);
    }

    void test_findByShare_orderedByDate()
    {
        DividendRepository repo;
        repo.insert(DividendObject(newGuid(), k_shareGuid, "2024-11-15T00:00:00", 1.6, 100.0));
        repo.insert(DividendObject(newGuid(), k_shareGuid, "2024-05-15T00:00:00", 1.5, 100.0));

        const auto divs = repo.findByShare(k_shareGuid);
        QCOMPARE(divs.size(), 2);
        QVERIFY(divs[0].dateTime() < divs[1].dateTime());
    }

    void test_findByShareAndYear()
    {
        DividendRepository repo;
        repo.insert(DividendObject(newGuid(), k_shareGuid, "2023-05-15T00:00:00", 1.2, 100.0));
        repo.insert(DividendObject(newGuid(), k_shareGuid, "2024-05-15T00:00:00", 1.5, 100.0));

        const auto divs2024 = repo.findByShareAndYear(k_shareGuid, 2024);
        QCOMPARE(divs2024.size(), 1);
        QCOMPARE(divs2024.first().year(), 2024);
    }

    void test_update()
    {
        DividendRepository repo;
        const QString guid = newGuid();
        repo.insert(DividendObject(guid, k_shareGuid, "2024-05-15T00:00:00", 1.5, 100.0));

        DividendObject updated(guid, k_shareGuid, "2024-05-15T00:00:00", 1.8, 120.0);
        QVERIFY(repo.update(updated));

        const auto found = repo.findByGuid(guid);
        QCOMPARE(found.rate(),   1.8);
        QCOMPARE(found.volume(), 120.0);
    }

    void test_updateDocument()
    {
        DividendRepository repo;
        const QString guid = newGuid();
        repo.insert(DividendObject(guid, k_shareGuid, "2024-05-15T00:00:00", 1.5, 100.0));

        QVERIFY(repo.updateDocument(guid, "/docs/dividend.pdf"));
        QCOMPARE(repo.findByGuid(guid).document(), QString("/docs/dividend.pdf"));
    }

    void test_remove()
    {
        DividendRepository repo;
        const QString guid = newGuid();
        repo.insert(DividendObject(guid, k_shareGuid, "2024-05-15T00:00:00", 1.5, 100.0));

        QVERIFY(repo.remove(guid));
        QVERIFY(!repo.findByGuid(guid).isValid());
    }

    void test_totalPayout()
    {
        DividendRepository repo;
        // 1.5 * 100 = 150
        repo.insert(DividendObject(newGuid(), k_shareGuid, "2024-05-15T00:00:00", 1.5, 100.0));
        // 1.0 * 50 = 50
        repo.insert(DividendObject(newGuid(), k_shareGuid, "2024-11-15T00:00:00", 1.0, 50.0));

        QCOMPARE(repo.totalPayout(k_shareGuid), 200.0);
    }

    void test_totalPayoutWithTaxes()
    {
        DividendRepository repo;
        // 150 - 8.5 = 141.5
        repo.insert(DividendObject(newGuid(), k_shareGuid, "2024-05-15T00:00:00",
                                   1.5, 100.0, 5.0, 3.0, 0.5, 45.0));
        // 50 - 0 = 50
        repo.insert(DividendObject(newGuid(), k_shareGuid, "2024-11-15T00:00:00",
                                   1.0, 50.0));

        QCOMPARE(repo.totalPayoutWithTaxes(k_shareGuid), 191.5);
    }
};

QTEST_MAIN(TestDividendRepository)
#include "tst_dividendrepository.moc"

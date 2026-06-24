// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include <QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>

#include "../../app/core/Database.h"

class TestDatabase : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase()
    {
        QVERIFY(Database::instance().open(":memory:"));
    }

    void cleanupTestCase()
    {
        Database::instance().close();
    }

    // ── Schema creation ───────────────────────────────────────────────────

    void test_shares_table_exists()
    {
        QSqlQuery q(QSqlDatabase::database("spm_main"));
        QVERIFY(q.exec("SELECT COUNT(*) FROM sqlite_master "
                        "WHERE type='table' AND name='shares'"));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 1);
    }

    void test_buys_table_exists()
    {
        QSqlQuery q(QSqlDatabase::database("spm_main"));
        QVERIFY(q.exec("SELECT COUNT(*) FROM sqlite_master "
                        "WHERE type='table' AND name='buys'"));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 1);
    }

    void test_sales_table_exists()
    {
        QSqlQuery q(QSqlDatabase::database("spm_main"));
        QVERIFY(q.exec("SELECT COUNT(*) FROM sqlite_master "
                        "WHERE type='table' AND name='sales'"));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 1);
    }

    void test_sale_buy_details_table_exists()
    {
        QSqlQuery q(QSqlDatabase::database("spm_main"));
        QVERIFY(q.exec("SELECT COUNT(*) FROM sqlite_master "
                        "WHERE type='table' AND name='sale_buy_details'"));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 1);
    }

    void test_dividends_table_exists()
    {
        QSqlQuery q(QSqlDatabase::database("spm_main"));
        QVERIFY(q.exec("SELECT COUNT(*) FROM sqlite_master "
                        "WHERE type='table' AND name='dividends'"));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 1);
    }

    void test_daily_values_table_exists()
    {
        QSqlQuery q(QSqlDatabase::database("spm_main"));
        QVERIFY(q.exec("SELECT COUNT(*) FROM sqlite_master "
                        "WHERE type='table' AND name='daily_values'"));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 1);
    }

    // ── Indexes ───────────────────────────────────────────────────────────

    void test_indexes_exist()
    {
        const QStringList expectedIndexes = {
            "idx_buys_share",
            "idx_buys_datetime",
            "idx_sales_share",
            "idx_dividends_share",
            "idx_daily_date"
        };

        QSqlQuery q(QSqlDatabase::database("spm_main"));
        for (const QString& idx : expectedIndexes) {
            QVERIFY(q.exec(QString("SELECT COUNT(*) FROM sqlite_master "
                                   "WHERE type='index' AND name='%1'").arg(idx)));
            QVERIFY(q.next());
            QCOMPARE(q.value(0).toInt(), 1);
        }
    }

    // ── Foreign key constraints ───────────────────────────────────────────

    void test_foreign_keys_enabled()
    {
        // PRAGMA foreign_keys should be ON
        QSqlQuery q(QSqlDatabase::database("spm_main"));
        QVERIFY(q.exec("PRAGMA foreign_keys"));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 1);
    }

    void test_buys_foreign_key_constraint()
    {
        // Inserting a buy with a non-existent share_guid should fail
        QSqlQuery q(QSqlDatabase::database("spm_main"));
        bool ok = q.exec("INSERT INTO buys (guid, share_guid, datetime, volume, price) "
                         "VALUES ('test-fk-1', 'non-existent-guid', "
                         "'2024-01-01T10:00:00', 10, 100)");
        QVERIFY(!ok); // Must fail due to FK constraint
    }

    void test_dividends_foreign_key_constraint()
    {
        QSqlQuery q(QSqlDatabase::database("spm_main"));
        bool ok = q.exec("INSERT INTO dividends (guid, share_guid, datetime, rate, volume) "
                         "VALUES ('test-fk-2', 'non-existent-guid', "
                         "'2024-01-01T10:00:00', 1.5, 100)");
        QVERIFY(!ok);
    }

    // ── Default values ────────────────────────────────────────────────────

    void test_shares_default_values()
    {
        // Insert minimal share and check defaults
        QSqlQuery q(QSqlDatabase::database("spm_main"));
        QVERIFY(q.exec("INSERT INTO shares (guid, wkn, name) "
                        "VALUES ('def-test-guid', 'DEF001', 'Default Test')"));

        QVERIFY(q.exec("SELECT currency, cur_price, update_type FROM shares "
                        "WHERE guid = 'def-test-guid'"));
        QVERIFY(q.next());
        QCOMPARE(q.value("currency").toString(), QString("EUR"));
        QCOMPARE(q.value("cur_price").toDouble(), 0.0);
        QCOMPARE(q.value("update_type").toInt(), 3); // ShareUpdateType::Both

        // Cleanup
        q.exec("DELETE FROM shares WHERE guid = 'def-test-guid'");
    }

    void test_buys_default_values()
    {
        // Need a parent share first
        QSqlQuery q(QSqlDatabase::database("spm_main"));
        q.exec("INSERT INTO shares (guid, wkn, name) "
               "VALUES ('def-share-guid', 'DEF002', 'Default Share')");

        QVERIFY(q.exec("INSERT INTO buys (guid, share_guid, datetime, volume, price) "
                        "VALUES ('def-buy-guid', 'def-share-guid', "
                        "'2024-01-01T10:00:00', 10, 100)"));

        // Brokerage fields (provision, broker_fee, trader_fee, reduction) were
        // removed from the buys table — they live in the brokerage table now.
        // Only volume_sold remains with a DEFAULT value.
        QVERIFY(q.exec("SELECT volume_sold FROM buys WHERE guid = 'def-buy-guid'"));
        QVERIFY(q.next());
        QCOMPARE(q.value("volume_sold").toDouble(), 0.0);

        // Cleanup
        q.exec("DELETE FROM shares WHERE guid = 'def-share-guid'");
    }

    // ── WAL mode ──────────────────────────────────────────────────────────

    void test_wal_mode_enabled()
    {
        QSqlQuery q(QSqlDatabase::database("spm_main"));
        QVERIFY(q.exec("PRAGMA journal_mode"));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(), QString("memory")); // :memory: uses memory journal
    }

    // ── Transaction ───────────────────────────────────────────────────────

    void test_transaction_commit()
    {
        QVERIFY(Database::instance().beginTransaction());

        QSqlQuery q(QSqlDatabase::database("spm_main"));
        q.exec("INSERT INTO shares (guid, wkn, name) "
               "VALUES ('tx-commit-guid', 'TXC001', 'TX Commit Test')");

        QVERIFY(Database::instance().commitTransaction());

        // Record should exist after commit
        QVERIFY(q.exec("SELECT COUNT(*) FROM shares WHERE guid = 'tx-commit-guid'"));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 1);

        // Cleanup
        q.exec("DELETE FROM shares WHERE guid = 'tx-commit-guid'");
    }

    void test_transaction_rollback()
    {
        QVERIFY(Database::instance().beginTransaction());

        QSqlQuery q(QSqlDatabase::database("spm_main"));
        q.exec("INSERT INTO shares (guid, wkn, name) "
               "VALUES ('tx-rollback-guid', 'TXR001', 'TX Rollback Test')");

        QVERIFY(Database::instance().rollbackTransaction());

        // Record should NOT exist after rollback
        QVERIFY(q.exec("SELECT COUNT(*) FROM shares WHERE guid = 'tx-rollback-guid'"));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 0);
    }
};

QTEST_MAIN(TestDatabase)
#include "tst_database.moc"

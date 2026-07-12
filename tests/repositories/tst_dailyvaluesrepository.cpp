// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include <QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>

#include "../../app/models/DailyValuesObject.h"
#include "../../app/repositories/DailyValuesRepository.h"
#include "../../app/core/Database.h"

class TestDailyValuesRepository : public QObject
{
    Q_OBJECT

private:
    QString newGuid() const { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
    const QString k_shareGuid = QStringLiteral("test-share-0001");

    DailyValuesObject makeEntry(const QDate& date,
                                double opening = 100.0, double closing = 105.0,
                                double top = 106.0, double bottom = 99.0,
                                double volume = 100000.0) const
    {
        return DailyValuesObject(k_shareGuid, date, opening, closing, top, bottom, volume);
    }

private slots:

    void initTestCase()
    {
        // Use in-memory SQLite database for tests
        QVERIFY(Database::instance().open(":memory:"));

        // Insert parent share — required by foreign key constraint
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
        // Clean daily_values table before each test
        Database::instance().execute("DELETE FROM daily_values");
    }

    // ── DailyValuesObject ─────────────────────────────────────────────────

    void test_dailyValuesObject_isValid()
    {
        DailyValuesObject valid(k_shareGuid, QDate(2024, 6, 15));
        QVERIFY(valid.isValid());

        DailyValuesObject noGuid(QString(), QDate(2024, 6, 15));
        QVERIFY(!noGuid.isValid());

        DailyValuesObject noDate(k_shareGuid, QDate());
        QVERIFY(!noDate.isValid());

        DailyValuesObject empty;
        QVERIFY(!empty.isValid());
    }

    void test_dailyValuesObject_setValues()
    {
        DailyValuesObject entry(k_shareGuid, QDate(2024, 6, 15));
        entry.setValues(142.5, 144.8, 145.2, 141.9, 1250000.0);

        QCOMPARE(entry.openingPrice(), 142.5);
        QCOMPARE(entry.closingPrice(), 144.8);
        QCOMPARE(entry.top(),          145.2);
        QCOMPARE(entry.bottom(),       141.9);
        QCOMPARE(entry.volume(),       1250000.0);
    }

    // ── DailyValuesRepository ─────────────────────────────────────────────

    void test_upsert_and_findByShareAndDate()
    {
        DailyValuesRepository repo;
        const QDate date(2024, 6, 15);
        DailyValuesObject entry = makeEntry(date, 100.0, 105.0, 106.0, 99.0, 100000.0);

        QVERIFY(repo.upsert(entry));

        const auto found = repo.findByShareAndDate(k_shareGuid, date);
        QVERIFY(found.isValid());
        QCOMPARE(found.shareGuid(),   k_shareGuid);
        QCOMPARE(found.date(),        date);
        QCOMPARE(found.openingPrice(), 100.0);
        QCOMPARE(found.closingPrice(), 105.0);
        QCOMPARE(found.top(),          106.0);
        QCOMPARE(found.bottom(),        99.0);
        QCOMPARE(found.volume(),       100000.0);
    }

    void test_upsert_replaces_existing()
    {
        DailyValuesRepository repo;
        const QDate date(2024, 6, 15);

        // Insert initial values
        QVERIFY(repo.upsert(makeEntry(date, 100.0, 105.0)));

        // Upsert with updated values for the same date
        QVERIFY(repo.upsert(makeEntry(date, 110.0, 115.0)));

        // Only one record should exist and it should have the updated values
        QCOMPARE(repo.count(k_shareGuid), 1);
        QCOMPARE(repo.findByShareAndDate(k_shareGuid, date).openingPrice(), 110.0);
        QCOMPARE(repo.findByShareAndDate(k_shareGuid, date).closingPrice(), 115.0);
    }

    void test_upsertList()
    {
        DailyValuesRepository repo;
        QList<DailyValuesObject> entries = {
            makeEntry(QDate(2024, 6, 13)),
            makeEntry(QDate(2024, 6, 14)),
            makeEntry(QDate(2024, 6, 15))
        };

        QVERIFY(repo.upsertList(entries));
        QCOMPARE(repo.count(k_shareGuid), 3);
    }

    // ── DailyValuesRepository::UpsertStats ─────────────────────────────────
    //
    // upsertList() always compares each incoming record against the existing
    // DB row (regardless of whether `stats` is passed) and skips the DB
    // write entirely for rows whose values are unchanged. `stats` is a pure
    // reporting out-parameter for the caller (see MainWindow refresh flow).

    void test_upsertList_stats_allInserted()
    {
        DailyValuesRepository repo;
        QList<DailyValuesObject> entries = {
            makeEntry(QDate(2024, 6, 13)),
            makeEntry(QDate(2024, 6, 14)),
            makeEntry(QDate(2024, 6, 15))
        };

        DailyValuesRepository::UpsertStats stats;
        QVERIFY(repo.upsertList(entries, &stats));

        QCOMPARE(stats.fetched,   3);
        QCOMPARE(stats.inserted,  3);
        QCOMPARE(stats.updated,   0);
        QCOMPARE(stats.unchanged, 0);
        QCOMPARE(repo.count(k_shareGuid), 3);
    }

    void test_upsertList_stats_updatedAndUnchanged()
    {
        DailyValuesRepository repo;
        const QDate d1(2024, 6, 13);
        const QDate d2(2024, 6, 14);
        const QDate d3(2024, 6, 15);

        // Seed the DB with two existing rows (d1, d2).
        QVERIFY(repo.upsertList({ makeEntry(d1), makeEntry(d2) }));

        // Simulate a re-fetch of an overlapping window:
        //  - d1: identical values          -> unchanged
        //  - d2: changed values            -> updated
        //  - d3: not present in DB yet     -> inserted
        QList<DailyValuesObject> refreshed = {
            makeEntry(d1),
            makeEntry(d2, 111.0, 116.0, 117.0, 108.0, 200000.0),
            makeEntry(d3)
        };

        DailyValuesRepository::UpsertStats stats;
        QVERIFY(repo.upsertList(refreshed, &stats));

        QCOMPARE(stats.fetched,   3);
        QCOMPARE(stats.inserted,  1);
        QCOMPARE(stats.updated,   1);
        QCOMPARE(stats.unchanged, 1);

        // Unchanged row must still hold its original values.
        QCOMPARE(repo.findByShareAndDate(k_shareGuid, d1).openingPrice(), 100.0);
        // Updated row must hold the new values.
        QCOMPARE(repo.findByShareAndDate(k_shareGuid, d2).openingPrice(), 111.0);
        QCOMPARE(repo.findByShareAndDate(k_shareGuid, d2).volume(),       200000.0);
    }

    void test_upsertList_stats_toleratesFloatingPointNoise()
    {
        DailyValuesRepository repo;
        const QDate date(2024, 6, 15);

        QVERIFY(repo.upsert(makeEntry(date, 123.45678, 124.56789)));

        // Differs only in the 10th decimal place — pure floating-point
        // representation noise, must be treated as "unchanged".
        DailyValuesObject noisy = makeEntry(date, 123.45678 + 1e-10, 124.56789);

        DailyValuesRepository::UpsertStats stats;
        QVERIFY(repo.upsertList({ noisy }, &stats));

        QCOMPARE(stats.unchanged, 1);
        QCOMPARE(stats.updated,   0);
    }

    void test_upsertList_stats_detectsFifthDecimalChange()
    {
        DailyValuesRepository repo;
        const QDate date(2024, 6, 15);

        QVERIFY(repo.upsert(makeEntry(date, 123.45678, 124.56789)));

        // Real change in the 5th decimal place (upstream API precision) must
        // be detected as "updated", not swallowed by the tolerance.
        DailyValuesObject changed = makeEntry(date, 123.45679, 124.56789);

        DailyValuesRepository::UpsertStats stats;
        QVERIFY(repo.upsertList({ changed }, &stats));

        QCOMPARE(stats.updated,   1);
        QCOMPARE(stats.unchanged, 0);
        QCOMPARE(repo.findByShareAndDate(k_shareGuid, date).openingPrice(), 123.45679);
    }

    void test_upsertList_backwardCompatible_withoutStats()
    {
        DailyValuesRepository repo;
        QList<DailyValuesObject> entries = {
            makeEntry(QDate(2024, 6, 13)),
            makeEntry(QDate(2024, 6, 14))
        };

        // No stats pointer passed — call still works exactly as before.
        QVERIFY(repo.upsertList(entries));
        QCOMPARE(repo.count(k_shareGuid), 2);
    }

    void test_findByShare_orderedByDate()
    {
        DailyValuesRepository repo;
        repo.upsert(makeEntry(QDate(2024, 6, 15)));
        repo.upsert(makeEntry(QDate(2024, 6, 13)));
        repo.upsert(makeEntry(QDate(2024, 6, 14)));

        const auto entries = repo.findByShare(k_shareGuid);
        QCOMPARE(entries.size(), 3);
        // Should be ordered ascending
        QVERIFY(entries[0].date() < entries[1].date());
        QVERIFY(entries[1].date() < entries[2].date());
    }

    void test_findByShareAndDateRange()
    {
        DailyValuesRepository repo;
        repo.upsert(makeEntry(QDate(2024, 6, 10)));
        repo.upsert(makeEntry(QDate(2024, 6, 13)));
        repo.upsert(makeEntry(QDate(2024, 6, 14)));
        repo.upsert(makeEntry(QDate(2024, 6, 15)));
        repo.upsert(makeEntry(QDate(2024, 6, 20)));

        const auto entries = repo.findByShareAndDateRange(
            k_shareGuid, QDate(2024, 6, 13), QDate(2024, 6, 15));
        QCOMPARE(entries.size(), 3);
        QCOMPARE(entries.first().date(), QDate(2024, 6, 13));
        QCOMPARE(entries.last().date(),  QDate(2024, 6, 15));
    }

    void test_latestDate()
    {
        DailyValuesRepository repo;
        QVERIFY(!repo.latestDate(k_shareGuid).isValid()); // empty → invalid date

        repo.upsert(makeEntry(QDate(2024, 6, 13)));
        repo.upsert(makeEntry(QDate(2024, 6, 15)));
        repo.upsert(makeEntry(QDate(2024, 6, 14)));

        QCOMPARE(repo.latestDate(k_shareGuid), QDate(2024, 6, 15));
    }

    void test_earliestDate()
    {
        DailyValuesRepository repo;
        QVERIFY(!repo.earliestDate(k_shareGuid).isValid()); // empty → invalid date

        repo.upsert(makeEntry(QDate(2024, 6, 15)));
        repo.upsert(makeEntry(QDate(2024, 6, 13)));
        repo.upsert(makeEntry(QDate(2024, 6, 14)));

        QCOMPARE(repo.earliestDate(k_shareGuid), QDate(2024, 6, 13));
    }

    void test_count()
    {
        DailyValuesRepository repo;
        QCOMPARE(repo.count(k_shareGuid), 0);

        repo.upsert(makeEntry(QDate(2024, 6, 13)));
        repo.upsert(makeEntry(QDate(2024, 6, 14)));
        QCOMPARE(repo.count(k_shareGuid), 2);
    }

    void test_remove()
    {
        DailyValuesRepository repo;
        const QDate date(2024, 6, 15);
        repo.upsert(makeEntry(date));

        QVERIFY(repo.remove(k_shareGuid, date));
        QVERIFY(!repo.findByShareAndDate(k_shareGuid, date).isValid());
    }

    void test_removeByShare()
    {
        DailyValuesRepository repo;
        repo.upsert(makeEntry(QDate(2024, 6, 13)));
        repo.upsert(makeEntry(QDate(2024, 6, 14)));
        repo.upsert(makeEntry(QDate(2024, 6, 15)));

        QCOMPARE(repo.removeByShare(k_shareGuid), 3);
        QCOMPARE(repo.count(k_shareGuid), 0);
    }

    void test_findByShareAndDate_notFound()
    {
        DailyValuesRepository repo;
        const auto found = repo.findByShareAndDate(k_shareGuid, QDate(2024, 1, 1));
        QVERIFY(!found.isValid());
    }
};

QTEST_MAIN(TestDailyValuesRepository)
#include "tst_dailyvaluesrepository.moc"

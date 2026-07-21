// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include <QtTest>
#include <QSqlDatabase>
#include <QUuid>

#include "../../app/models/ShareObject.h"
#include "../../app/repositories/ShareRepository.h"
#include "../../app/core/Database.h"

class TestShareRepository : public QObject
{
    Q_OBJECT

private:
    QString newGuid() const { return QUuid::createUuid().toString(QUuid::WithoutBraces); }

    ShareObject makeShare(const QString& guid, const QString& wkn,
                          const QString& name) const {
        return ShareObject(guid, wkn, "DE000" + wkn, name,
                           ShareType::Share, "EUR");
    }

private slots:

    void initTestCase()
    {
        QVERIFY(Database::instance().open(":memory:"));
    }

    void cleanupTestCase()
    {
        Database::instance().close();
    }

    void init()
    {
        Database::instance().execute("DELETE FROM shares");
    }

    // ── ShareObject ───────────────────────────────────────────────────────
    void test_shareObject_isValid()
    {
        ShareObject valid(newGuid(), "BASF11", "DE000BASF111", "BASF SE");
        QVERIFY(valid.isValid());

        ShareObject invalid;
        QVERIFY(!invalid.isValid());
    }

    void test_shareObject_priceDifference()
    {
        ShareObject share(newGuid(), "BASF11", "", "BASF SE");
        share.setCurPrice(42.50);
        share.setPrevDayPrice(41.80);

        QVERIFY(qAbs(share.priceDifference() - 0.70) < 0.001);
    }

    void test_shareObject_pricePerformance()
    {
        ShareObject share(newGuid(), "BASF11", "", "BASF SE");
        share.setCurPrice(42.50);
        share.setPrevDayPrice(40.00);

        // (42.5 - 40) / 40 * 100 = 6.25%
        QVERIFY(qAbs(share.pricePerformance() - 6.25) < 0.001);
    }

    void test_shareObject_pricePerformance_zero_prev()
    {
        ShareObject share(newGuid(), "BASF11", "", "BASF SE");
        share.setCurPrice(42.50);
        share.setPrevDayPrice(0.0);

        QCOMPARE(share.pricePerformance(), 0.0);
    }

    // ── ShareRepository ───────────────────────────────────────────────────
    void test_insert_and_findByGuid()
    {
        ShareRepository repo;
        const QString guid = newGuid();
        ShareObject share = makeShare(guid, "BASF11", "BASF SE");

        QVERIFY(repo.insert(share));

        const auto found = repo.findByGuid(guid);
        QVERIFY(found.isValid());
        QCOMPARE(found.guid(), guid);
        QCOMPARE(found.wkn(),  QString("BASF11"));
        QCOMPARE(found.name(), QString("BASF SE"));
    }

    void test_findAll_orderedByName()
    {
        ShareRepository repo;
        repo.insert(makeShare(newGuid(), "SAP001", "SAP SE"));
        repo.insert(makeShare(newGuid(), "BASF11", "BASF SE"));
        repo.insert(makeShare(newGuid(), "ALV000", "Allianz SE"));

        const auto shares = repo.findAll();
        QCOMPARE(shares.size(), 3);
        QCOMPARE(shares[0].name(), QString("Allianz SE"));
        QCOMPARE(shares[1].name(), QString("BASF SE"));
        QCOMPARE(shares[2].name(), QString("SAP SE"));
    }

    void test_findByWkn()
    {
        ShareRepository repo;
        repo.insert(makeShare(newGuid(), "BASF11", "BASF SE"));

        const auto found = repo.findByWkn("BASF11");
        QVERIFY(found.isValid());
        QCOMPARE(found.name(), QString("BASF SE"));
    }

    void test_findByIsin()
    {
        ShareRepository repo;
        const QString guid = newGuid();
        repo.insert(ShareObject(guid, "BASF11", "DE000BASF111", "BASF SE"));

        const auto found = repo.findByIsin("DE000BASF111");
        QVERIFY(found.isValid());
        QCOMPARE(found.wkn(), QString("BASF11"));
    }

    void test_wknExists()
    {
        ShareRepository repo;
        repo.insert(makeShare(newGuid(), "BASF11", "BASF SE"));

        QVERIFY(repo.wknExists("BASF11"));
        QVERIFY(!repo.wknExists("XXXXX"));
    }

    void test_update()
    {
        ShareRepository repo;
        const QString guid = newGuid();
        repo.insert(makeShare(guid, "BASF11", "BASF SE"));

        auto share = repo.findByGuid(guid);
        share.setName("BASF SE (updated)");
        share.setCurPrice(45.00);
        QVERIFY(repo.update(share));

        const auto found = repo.findByGuid(guid);
        QCOMPARE(found.name(),     QString("BASF SE (updated)"));
        QCOMPARE(found.curPrice(), 45.00);
    }

    void test_updatePrice()
    {
        ShareRepository repo;
        const QString guid = newGuid();
        repo.insert(makeShare(guid, "BASF11", "BASF SE"));

        QVERIFY(repo.updatePrice(guid, 45.00, 42.50, "2024-06-15T12:00:00"));

        const auto found = repo.findByGuid(guid);
        QCOMPARE(found.curPrice(),      45.00);
        QCOMPARE(found.prevDayPrice(),  42.50);
        QCOMPARE(found.lastPriceUpdate(), QString("2024-06-15T12:00:00"));
    }

    void test_updateLastInternetUpdate()
    {
        ShareRepository repo;
        const QString guid = newGuid();
        repo.insert(makeShare(guid, "BASF11", "BASF SE"));

        QVERIFY(repo.updateLastInternetUpdate(guid, "2024-06-15T12:00:00"));
        QCOMPARE(repo.findByGuid(guid).lastInternetUpdate(),
                 QString("2024-06-15T12:00:00"));
    }

    // ── maxLastInternetUpdate (Feature 21.07.2026) ─────────────────────────
    void test_maxLastInternetUpdate_emptyPortfolio_returnsEmpty()
    {
        ShareRepository repo;
        QVERIFY(repo.maxLastInternetUpdate().isEmpty());
    }

    void test_maxLastInternetUpdate_noShareEverUpdated_returnsEmpty()
    {
        ShareRepository repo;
        repo.insert(makeShare(newGuid(), "NUP001", "Never Updated AG"));

        QVERIFY(repo.maxLastInternetUpdate().isEmpty());
    }

    void test_maxLastInternetUpdate_returnsLatestAcrossShares()
    {
        ShareRepository repo;
        const QString g1 = newGuid();
        const QString g2 = newGuid();
        repo.insert(makeShare(g1, "OLD001", "Older AG"));
        repo.insert(makeShare(g2, "NEW001", "Newer AG"));

        repo.updateLastInternetUpdate(g1, "2026-07-01T10:00:00");
        repo.updateLastInternetUpdate(g2, "2026-07-15T09:30:00");

        QCOMPARE(repo.maxLastInternetUpdate(), QString("2026-07-15T09:30:00"));
    }

    void test_maxLastInternetUpdate_ignoresSharesNeverUpdated()
    {
        // Eine Aktie ohne jemals gesetzten last_internet_update darf das
        // Ergebnis nicht auf einen leeren String zurückfallen lassen, solange
        // mindestens eine andere Aktie aktualisiert wurde.
        ShareRepository repo;
        const QString gUpdated   = newGuid();
        const QString gNeverUsed = newGuid();
        repo.insert(makeShare(gUpdated,   "UPD001", "Updated AG"));
        repo.insert(makeShare(gNeverUsed, "NUP002", "Never Updated 2 AG"));

        repo.updateLastInternetUpdate(gUpdated, "2026-07-10T08:00:00");

        QCOMPARE(repo.maxLastInternetUpdate(), QString("2026-07-10T08:00:00"));
    }

    void test_remove()
    {
        ShareRepository repo;
        const QString guid = newGuid();
        repo.insert(makeShare(guid, "BASF11", "BASF SE"));

        QVERIFY(repo.remove(guid));
        QVERIFY(!repo.findByGuid(guid).isValid());
    }

    void test_remove_cascades_to_children()
    {
        // Verify ON DELETE CASCADE works — inserting a buy and removing the share
        ShareRepository repo;
        const QString guid = newGuid();
        repo.insert(makeShare(guid, "BASF11", "BASF SE"));

        // Insert a buy referencing this share
        Database::instance().execute(
            QString("INSERT INTO buys (guid, share_guid, datetime, volume, price) "
                    "VALUES ('buy-001', '%1', '2024-01-01T10:00:00', 10, 100)").arg(guid));

        // Remove the share — buy should be cascade-deleted
        QVERIFY(repo.remove(guid));

        QSqlQuery q(QSqlDatabase::database("spm_main"));
        q.exec("SELECT COUNT(*) FROM buys WHERE share_guid = '" + guid + "'");
        q.next();
        QCOMPARE(q.value(0).toInt(), 0);
    }
};

QTEST_MAIN(TestShareRepository)
#include "tst_sharerepository.moc"

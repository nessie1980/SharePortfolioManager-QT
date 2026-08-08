// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// Unit tests for ShareSplitRepository — CRUD operations on the
// `share_splits` table. Same in-memory-SQLite pattern as the other
// repository tests (see tst_dailyvaluesrepository.cpp / tst_brokeragerepository.cpp).
#include <QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>

#include "../../app/models/ShareSplitObject.h"
#include "../../app/repositories/ShareSplitRepository.h"
#include "../../app/core/Database.h"

class TestShareSplitRepository : public QObject
{
    Q_OBJECT

private:
    QString newGuid() const { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
    const QString k_shareGuid = QStringLiteral("test-share-0001");

    void insertParentShare() const
    {
        Database::instance().execute(
            QString("INSERT INTO shares (guid, wkn, name) VALUES ('%1', 'TEST01', 'Test Share')")
                .arg(k_shareGuid));
    }

    ShareSplitObject makeSplit(const QDate& date, double ratioNew = 20.0, double ratioOld = 1.0,
                               bool pricesAdjusted = false,
                               const QString& comment = QString(),
                               const QString& document = QString()) const
    {
        return ShareSplitObject(newGuid(), k_shareGuid, date, ratioNew, ratioOld,
                                pricesAdjusted, comment, document);
    }

private slots:

    void initTestCase()
    {
        QVERIFY(Database::instance().open(":memory:"));
        insertParentShare();
    }

    void cleanupTestCase()
    {
        Database::instance().close();
    }

    void init()
    {
        Database::instance().execute("DELETE FROM share_splits");
    }

    // ── insert / findByShare ────────────────────────────────────────────

    void test_insert_andFindByShare_returnsSplit()
    {
        ShareSplitRepository repo;
        const ShareSplitObject split = makeSplit(QDate(2022, 7, 18));

        QVERIFY(repo.insert(split));

        const QList<ShareSplitObject> found = repo.findByShare(k_shareGuid);
        QCOMPARE(found.size(), 1);
        QCOMPARE(found.constFirst().guid(),     split.guid());
        QCOMPARE(found.constFirst().date(),     QDate(2022, 7, 18));
        QCOMPARE(found.constFirst().ratioNew(), 20.0);
        QCOMPARE(found.constFirst().ratioOld(), 1.0);
        QVERIFY(!found.constFirst().pricesAdjusted());
    }

    void test_findByShare_orderedByDateAscending()
    {
        ShareSplitRepository repo;
        QVERIFY(repo.insert(makeSplit(QDate(2022, 7, 18))));
        QVERIFY(repo.insert(makeSplit(QDate(2018, 3, 1), 4.0, 1.0)));

        const QList<ShareSplitObject> found = repo.findByShare(k_shareGuid);
        QCOMPARE(found.size(), 2);
        QCOMPARE(found.at(0).date(), QDate(2018, 3, 1));
        QCOMPARE(found.at(1).date(), QDate(2022, 7, 18));
    }

    void test_findByShare_noSplits_returnsEmpty()
    {
        ShareSplitRepository repo;
        QVERIFY(repo.findByShare(k_shareGuid).isEmpty());
    }

    // ── findByGuid ──────────────────────────────────────────────────────

    void test_findByGuid_found()
    {
        ShareSplitRepository repo;
        const ShareSplitObject split = makeSplit(QDate(2022, 7, 18));
        QVERIFY(repo.insert(split));

        const ShareSplitObject found = repo.findByGuid(split.guid());
        QVERIFY(found.isValid());
        QCOMPARE(found.date(), QDate(2022, 7, 18));
    }

    void test_findByGuid_notFound_returnsInvalid()
    {
        ShareSplitRepository repo;
        QVERIFY(!repo.findByGuid(newGuid()).isValid());
    }

    // ── existsForDate ───────────────────────────────────────────────────

    void test_existsForDate_true()
    {
        ShareSplitRepository repo;
        QVERIFY(repo.insert(makeSplit(QDate(2022, 7, 18))));
        QVERIFY(repo.existsForDate(k_shareGuid, QDate(2022, 7, 18)));
    }

    void test_existsForDate_false()
    {
        ShareSplitRepository repo;
        QVERIFY(!repo.existsForDate(k_shareGuid, QDate(2022, 7, 18)));
    }

    // ── UNIQUE(share_guid, date) ────────────────────────────────────────

    void test_insert_duplicateDate_fails()
    {
        ShareSplitRepository repo;
        QVERIFY(repo.insert(makeSplit(QDate(2022, 7, 18))));
        QVERIFY(!repo.insert(makeSplit(QDate(2022, 7, 18), 2.0, 1.0)));
    }

    // ── update ──────────────────────────────────────────────────────────

    void test_update_changesFields()
    {
        ShareSplitRepository repo;
        const ShareSplitObject split = makeSplit(QDate(2022, 7, 18), 20.0, 1.0, false,
                                                  QStringLiteral("Ursprünglich"));
        QVERIFY(repo.insert(split));

        const ShareSplitObject updated(split.guid(), k_shareGuid, QDate(2022, 7, 19),
                                       20.0, 1.0, true, QStringLiteral("Korrigiert"));
        QVERIFY(repo.update(updated));

        const ShareSplitObject reloaded = repo.findByGuid(split.guid());
        QCOMPARE(reloaded.date(), QDate(2022, 7, 19));
        QVERIFY(reloaded.pricesAdjusted());
        QCOMPARE(reloaded.comment(), QStringLiteral("Korrigiert"));
    }

    // ── Dokument (08.08.2026) ───────────────────────────────────────────
    //
    // documentExists() wird hier bewusst NICHT geprüft: die Abfrage sitzt in
    // ModelShareSplitEdit, nicht im Repository — dieselbe Platzierung wie bei
    // ModelBuyEdit/ModelSaleEdit/ModelDividendEdit/ModelBrokerageEdit. Die
    // zugehörigen Tests stehen entsprechend in tst_sharesplitsform.cpp.
    // Das Repository führt nur updateDocument(), das DocumentRootMigrator
    // beim Wechsel des Dokument-Roots aufruft.

    void test_insert_storesDocumentPath()
    {
        ShareSplitRepository repo;
        const ShareSplitObject split = makeSplit(QDate(2022, 7, 18), 20.0, 1.0, false,
                                                 QString(), QStringLiteral("/belege/split.pdf"));
        QVERIFY(repo.insert(split));

        QCOMPARE(repo.findByGuid(split.guid()).document(),
                 QStringLiteral("/belege/split.pdf"));
    }

    void test_insert_withoutDocument_returnsEmptyString()
    {
        ShareSplitRepository repo;
        const ShareSplitObject split = makeSplit(QDate(2022, 7, 18));
        QVERIFY(repo.insert(split));

        QVERIFY(repo.findByGuid(split.guid()).document().isEmpty());
    }

    void test_update_changesDocumentPath()
    {
        ShareSplitRepository repo;
        const ShareSplitObject split = makeSplit(QDate(2022, 7, 18), 20.0, 1.0, false,
                                                 QString(), QStringLiteral("/belege/alt.pdf"));
        QVERIFY(repo.insert(split));

        const ShareSplitObject updated(split.guid(), k_shareGuid, QDate(2022, 7, 18),
                                       20.0, 1.0, false, QString(),
                                       QStringLiteral("/belege/neu.pdf"));
        QVERIFY(repo.update(updated));

        QCOMPARE(repo.findByGuid(split.guid()).document(),
                 QStringLiteral("/belege/neu.pdf"));
    }

    void test_updateDocument_changesOnlyDocument()
    {
        // Wird von DocumentRootMigrator beim Root-Wechsel aufgerufen — dabei
        // darf ausschliesslich der Pfad angefasst werden.
        ShareSplitRepository repo;
        const ShareSplitObject split = makeSplit(QDate(2022, 7, 18), 20.0, 1.0, true,
                                                 QStringLiteral("Kommentar"),
                                                 QStringLiteral("/alt/root/a.pdf"));
        QVERIFY(repo.insert(split));

        QVERIFY(repo.updateDocument(split.guid(), QStringLiteral("/neu/root/a.pdf")));

        const ShareSplitObject reloaded = repo.findByGuid(split.guid());
        QCOMPARE(reloaded.document(), QStringLiteral("/neu/root/a.pdf"));
        QCOMPARE(reloaded.ratioNew(), 20.0);
        QCOMPARE(reloaded.comment(),  QStringLiteral("Kommentar"));
        QVERIFY(reloaded.pricesAdjusted());
    }

    // ── remove / removeByShare ──────────────────────────────────────────

    void test_remove_deletesSplit()
    {
        ShareSplitRepository repo;
        const ShareSplitObject split = makeSplit(QDate(2022, 7, 18));
        QVERIFY(repo.insert(split));

        QVERIFY(repo.remove(split.guid()));
        QVERIFY(repo.findByShare(k_shareGuid).isEmpty());
    }

    void test_removeByShare_deletesAllSplitsOfShare()
    {
        ShareSplitRepository repo;
        QVERIFY(repo.insert(makeSplit(QDate(2018, 3, 1), 4.0, 1.0)));
        QVERIFY(repo.insert(makeSplit(QDate(2022, 7, 18))));

        QCOMPARE(repo.removeByShare(k_shareGuid), 2);
        QVERIFY(repo.findByShare(k_shareGuid).isEmpty());
    }

    // ── ON DELETE CASCADE ───────────────────────────────────────────────
    // Muss als letzter Test laufen (löscht die für initTestCase() angelegte
    // Aktie) — QtTest ruft private slots in Deklarationsreihenfolge auf.

    void test_deletingShare_cascadesToSplits()
    {
        ShareSplitRepository repo;
        QVERIFY(repo.insert(makeSplit(QDate(2022, 7, 18))));

        Database::instance().execute(
            QString("DELETE FROM shares WHERE guid = '%1'").arg(k_shareGuid));

        QVERIFY(repo.findByShare(k_shareGuid).isEmpty());

        insertParentShare(); // Aktie für eventuelle weitere Läufe wiederherstellen
    }
};

QTEST_MAIN(TestShareSplitRepository)
#include "tst_sharesplitrepository.moc"

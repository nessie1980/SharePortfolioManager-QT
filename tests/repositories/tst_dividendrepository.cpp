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

    // ── Ex-Tag / Depotnummer (21.08.2026) ───────────────────────────────────
    //
    // Grundlage für die Plausibilitätsprüfung der Dividenden-Stückzahl, siehe
    // ARCHITECTURE.md, "Plausibilitätsprüfung der Dividenden-Stückzahl".

    void test_exDate_and_depotNumber_defaultToEmpty()
    {
        // Alt-Dividenden, die vor diesem Feld angelegt wurden, dürfen nicht
        // plötzlich anders aussehen — beide Felder bleiben ohne explizite
        // Angabe leer/ungesetzt.
        DividendObject d(newGuid(), k_shareGuid, "2024-05-15T00:00:00", 1.5, 100.0);

        QVERIFY(d.exDate().isEmpty());
        QVERIFY(!d.hasExDate());
        QVERIFY(!d.exDateAsDate().isValid());
        QCOMPARE(d.exDateAsStr(), QStringLiteral("-"));

        QVERIFY(d.depotNumber().isEmpty());
        QVERIFY(!d.hasDepotNumber());
    }

    void test_exDate_accessors_withValue()
    {
        DividendObject d(newGuid(), k_shareGuid, "2024-05-15T00:00:00",
                         1.5, 100.0, 0.0, 0.0, 0.0, 0.0, false, 1.0,
                         QStringLiteral("EUR"), QString(),
                         QStringLiteral("2024-05-13"), QStringLiteral("1234567"));

        QCOMPARE(d.exDate(), QStringLiteral("2024-05-13"));
        QVERIFY(d.hasExDate());
        QCOMPARE(d.exDateAsDate(), QDate(2024, 5, 13));
        QVERIFY2(!d.exDateAsStr().isEmpty() && d.exDateAsStr() != QStringLiteral("-"),
                  qPrintable(d.exDateAsStr()));

        QCOMPARE(d.depotNumber(), QStringLiteral("1234567"));
        QVERIFY(d.hasDepotNumber());
    }

    // ── volumeReferenceDate (Phase 4, 21.08.2026) ─────────────────────────

    void test_volumeReferenceDate_withExDate_returnsExDate()
    {
        DividendObject d(newGuid(), k_shareGuid, "2024-05-15T00:00:00",
                         1.5, 100.0, 0.0, 0.0, 0.0, 0.0, false, 1.0,
                         QStringLiteral("EUR"), QString(),
                         QStringLiteral("2024-05-13"), QStringLiteral("1234567"));

        QCOMPARE(d.volumeReferenceDate(), QDate(2024, 5, 13));
        // Ausdrücklich NICHT der Zahltag — darauf beruht Phase 4.
        QVERIFY(d.volumeReferenceDate() != d.date());
    }

    void test_volumeReferenceDate_withoutExDate_fallsBackToPayday()
    {
        // Alt-Dividende ohne Ex-Tag: der Rückfall bildet exakt das Verhalten
        // vor Phase 4 ab, damit die Zeile nicht plötzlich anders aussieht.
        DividendObject d(newGuid(), k_shareGuid, "2024-05-15T00:00:00",
                         1.5, 100.0, 0.0, 0.0, 0.0, 0.0);

        QVERIFY(!d.hasExDate());
        QCOMPARE(d.volumeReferenceDate(), QDate(2024, 5, 15));
        QCOMPARE(d.volumeReferenceDate(), d.date());
    }

    void test_isValid_ignoresMissingExDateAndDepotNumber()
    {
        // Zentrale Design-Entscheidung: isValid() bleibt an guid/rate/volume
        // geknüpft, NICHT an exDate/depotNumber — sonst würde eine bereits
        // gespeicherte Alt-Dividende beim Laden fälschlich als "nicht
        // gefunden" erscheinen. Die "Muss"-Prüfung für neue/bearbeitete
        // Dividenden sitzt in der Formularvalidierung, nicht hier.
        DividendObject d(newGuid(), k_shareGuid, "2024-05-15T00:00:00", 1.5, 100.0);
        QVERIFY(!d.hasExDate());
        QVERIFY(!d.hasDepotNumber());
        QVERIFY(d.isValid());
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

    void test_insert_and_findByGuid_persistsExDateAndDepotNumber()
    {
        DividendRepository repo;
        const QString guid = newGuid();

        DividendObject d(guid, k_shareGuid, "2024-05-15T00:00:00",
                         1.5, 100.0, 0.0, 0.0, 0.0, 0.0, false, 1.0,
                         QStringLiteral("EUR"), QString(),
                         QStringLiteral("2024-05-13"), QStringLiteral("1234567"));
        QVERIFY(repo.insert(d));

        const auto found = repo.findByGuid(guid);
        QVERIFY(found.isValid());
        QCOMPARE(found.exDate(),      QStringLiteral("2024-05-13"));
        QCOMPARE(found.depotNumber(), QStringLiteral("1234567"));
    }

    void test_insert_withoutExDateAndDepotNumber_roundTripsEmpty()
    {
        // Deckt den Migrationsfall ab: eine Dividende ohne diese Felder muss
        // nach dem Speichern/Laden weiterhin als "nicht gesetzt" erkennbar
        // sein, nicht als leerer String, der wie ein gültiges Datum aussieht.
        DividendRepository repo;
        const QString guid = newGuid();
        QVERIFY(repo.insert(DividendObject(guid, k_shareGuid, "2024-05-15T00:00:00", 1.5, 100.0)));

        const auto found = repo.findByGuid(guid);
        QVERIFY(found.isValid());
        QVERIFY(!found.hasExDate());
        QVERIFY(!found.hasDepotNumber());
    }

    void test_update_persistsExDateAndDepotNumber()
    {
        DividendRepository repo;
        const QString guid = newGuid();
        // Alt-Datensatz ohne Ex-Tag/Depotnummer anlegen — genau der Fall
        // "bestehende Dividende wird geöffnet und nachgepflegt".
        QVERIFY(repo.insert(DividendObject(guid, k_shareGuid, "2024-05-15T00:00:00", 1.5, 100.0)));

        DividendObject updated(guid, k_shareGuid, "2024-05-15T00:00:00",
                               1.5, 100.0, 0.0, 0.0, 0.0, 0.0, false, 1.0,
                               QStringLiteral("EUR"), QString(),
                               QStringLiteral("2024-05-13"), QStringLiteral("1234567"));
        QVERIFY(repo.update(updated));

        const auto found = repo.findByGuid(guid);
        QCOMPARE(found.exDate(),      QStringLiteral("2024-05-13"));
        QCOMPARE(found.depotNumber(), QStringLiteral("1234567"));
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

    void test_totalPayoutWithTaxes_matchesDoubleRoundedDividendObjectSum()
    {
        DividendRepository repo;

        DividendObject d1(newGuid(), k_shareGuid, "2024-07-01T00:00:00",
                          0.485, 125.0, 0.0, 0.0, 0.0, 60.0,
                          true, 1.07907, "USD");
        DividendObject d2(newGuid(), k_shareGuid, "2024-10-01T00:00:00",
                          0.485, 125.0, 0.0, 0.0, 0.0, 60.0,
                          true, 1.10526, "USD");
        QVERIFY(repo.insert(d1));
        QVERIFY(repo.insert(d2));

        const double expected = d1.dividendPayoutWithTaxes() + d2.dividendPayoutWithTaxes();

        QCOMPARE(repo.totalPayoutWithTaxes(k_shareGuid), expected);
    }
};

QTEST_MAIN(TestDividendRepository)
#include "tst_dividendrepository.moc"

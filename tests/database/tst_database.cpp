// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include <QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QDebug>
#include <QTemporaryDir>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "../../app/core/Database.h"
#include "../../app/core/DepotNumberNormalizer.h"

#include <utility>   // std::as_const

namespace {

// ── Hilfen für die Depotnummer-Migration (18.09.2026) ─────────────────────────

/// Legt ein Portfolio an und befüllt es mit Werten im alten C#-Format
/// "<Nummer> - <Bank>" sowie mit Gegenproben, die unverändert bleiben müssen.
/// Schliesst die Datenbank danach — das nächste open() ist der Prüfgegenstand.
///
/// Muss eine DATEI sein, kein :memory: (gleiche Begründung wie bei den
/// Spaltenmigrationen unten): der Altzustand muss das Schliessen überleben.
bool seedLegacyDepotNumbers(const QString& dbPath)
{
    if (!Database::instance().open(dbPath))
        return false;

    const QStringList statements = {
        "INSERT INTO shares (guid, wkn, name) VALUES ('dep-share', 'DEP001', 'Depot AG')",

        // Zwei Käufe im alten Format, zwei Depots — wie in Nessies ShareList.xml.
        "INSERT INTO buys (guid, share_guid, depot_number, order_number, datetime, volume, price) "
        "VALUES ('b-ing', 'dep-share', '8006189848 - ING diba', 'O-1', '2024-01-10T00:00:00', 10, 100)",
        "INSERT INTO buys (guid, share_guid, depot_number, order_number, datetime, volume, price) "
        "VALUES ('b-dkb', 'dep-share', '501403950 - DKB', 'O-2', '2024-02-10T00:00:00', 5, 100)",

        // Gegenproben: schon im neuen Format bzw. kein Ziffernblock vor " - ".
        "INSERT INTO buys (guid, share_guid, depot_number, order_number, datetime, volume, price) "
        "VALUES ('b-new', 'dep-share', '8006189848', 'O-3', '2024-03-10T00:00:00', 1, 100)",
        "INSERT INTO buys (guid, share_guid, depot_number, order_number, datetime, volume, price) "
        "VALUES ('b-odd', 'dep-share', 'ING - diba', 'O-4', '2024-04-10T00:00:00', 1, 100)",

        "INSERT INTO sales (guid, share_guid, depot_number, order_number, datetime, volume, sale_price) "
        "VALUES ('s-ing', 'dep-share', '8006189848 - ING diba', 'O-S1', '2024-05-10T00:00:00', 2, 120)",

        "INSERT INTO dividends (guid, share_guid, datetime, rate, volume, depot_number) "
        "VALUES ('d-ing', 'dep-share', '2024-06-10T00:00:00', 1.5, 8, '8006189848 - ING diba')",
    };

    QSqlQuery q(QSqlDatabase::database("spm_main"));
    for (const QString& sql : statements) {
        if (!q.exec(sql)) {
            qWarning() << "seedLegacyDepotNumbers:" << q.lastError().text() << sql;
            Database::instance().close();
            return false;
        }
    }
    Database::instance().close();
    return true;
}

/// depot_number einer Zeile der offenen Hauptverbindung.
QString depotNumberOf(const QString& table, const QString& guid)
{
    QSqlQuery q(QSqlDatabase::database("spm_main"));
    q.prepare(QStringLiteral("SELECT depot_number FROM %1 WHERE guid = :guid").arg(table));
    q.bindValue(QStringLiteral(":guid"), guid);
    if (!q.exec() || !q.next())
        return QStringLiteral("<nicht gefunden>");
    return q.value(0).toString();
}

/// Alle Sicherungen der Depotnummer-Migration im Ordner @p dir.
QStringList migrationBackupsIn(const QString& dir)
{
    return QDir(dir).entryList({ QStringLiteral("*_vor_Depotnummer_Migration_*") },
                               QDir::Files);
}

} // namespace

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

    void test_share_splits_table_exists()
    {
        QSqlQuery q(QSqlDatabase::database("spm_main"));
        QVERIFY(q.exec("SELECT COUNT(*) FROM sqlite_master "
                        "WHERE type='table' AND name='share_splits'"));
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
            "idx_daily_date",
            "idx_splits_share"
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

    // ── Schema migration (08.08.2026) ─────────────────────────────────────
    //
    // migrateSchema() und ensureColumn() sind privat, also nicht direkt
    // aufrufbar. Geprüft wird deshalb ihr Ergebnis: nach open() muss die
    // Spalte da sein — egal ob sie aus createSchema() oder aus einem
    // nachgezogenen ALTER TABLE stammt.
    //
    // Den eigentlich interessanten Fall — eine Datenbank, die share_splits
    // noch OHNE document führt — stellen die beiden letzten Tests her, indem
    // sie die Tabelle von Hand im alten Zustand neu anlegen und dann ein
    // zweites open() ausführen. Genau das passiert bei einem Portfolio, das
    // zwischen Phase 1 und Phase 3a geöffnet wurde.

    void test_share_splits_has_document_column()
    {
        QSqlQuery q(QSqlDatabase::database("spm_main"));
        QVERIFY(q.exec("PRAGMA table_info(share_splits)"));

        bool found = false;
        while (q.next()) {
            if (q.value(1).toString() == QStringLiteral("document")) {
                found = true;
                break;
            }
        }
        QVERIFY2(found, "Spalte 'document' fehlt in share_splits");
    }

    void test_migration_addsMissingDocumentColumn()
    {
        // Der eigentliche Prüfgegenstand: eine Datenbank, die share_splits
        // noch ohne document führt — genau der Zustand eines Portfolios, das
        // zwischen Phase 1 und Phase 3a geöffnet wurde.
        //
        // Muss eine DATEI sein, kein :memory: — beim Schliessen einer
        // In-Memory-Datenbank verschwindet der gesamte Inhalt, der alte
        // Zustand wäre beim erneuten Öffnen also gar nicht mehr vorhanden
        // und der Test würde nichts belegen.
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString dbPath = tempDir.path() + QStringLiteral("/migration.db");

        QVERIFY(Database::instance().open(dbPath));
        {
            QSqlQuery q(QSqlDatabase::database("spm_main"));
            QVERIFY(q.exec("DROP TABLE IF EXISTS share_splits"));
            QVERIFY(q.exec("CREATE TABLE share_splits ("
                           "guid TEXT PRIMARY KEY, "
                           "share_guid TEXT NOT NULL REFERENCES shares(guid) ON DELETE CASCADE, "
                           "date TEXT NOT NULL, "
                           "ratio_new REAL NOT NULL CHECK(ratio_new > 0), "
                           "ratio_old REAL NOT NULL CHECK(ratio_old > 0), "
                           "prices_adjusted INTEGER DEFAULT 0, "
                           "comment TEXT, "
                           "UNIQUE(share_guid, date))"));
            QVERIFY(q.exec("INSERT INTO shares (guid, wkn, name) "
                           "VALUES ('mig-share', 'MIG001', 'Migration AG')"));
            QVERIFY(q.exec("INSERT INTO share_splits "
                           "(guid, share_guid, date, ratio_new, ratio_old) "
                           "VALUES ('mig-split', 'mig-share', '2022-07-18', 20, 1)"));
        }
        Database::instance().close();

        // Erneutes Öffnen zieht die fehlende Spalte nach. createSchema()
        // allein täte das NICHT — CREATE TABLE IF NOT EXISTS sieht die
        // vorhandene Tabelle und vergleicht die Spaltenliste nicht.
        QVERIFY(Database::instance().open(dbPath));

        QSqlQuery q(QSqlDatabase::database("spm_main"));
        QVERIFY(q.exec("PRAGMA table_info(share_splits)"));
        bool found = false;
        while (q.next()) {
            if (q.value(1).toString() == QStringLiteral("document")) { found = true; break; }
        }
        QVERIFY2(found, "Spalte 'document' wurde nicht nachgezogen");

        // Und der bereits erfasste Split ist noch da — die Migration darf
        // keine Daten kosten, das ist ihr ganzer Zweck.
        QVERIFY(q.exec("SELECT ratio_new FROM share_splits WHERE guid = 'mig-split'"));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toDouble(), 20.0);

        Database::instance().close();
        QVERIFY(Database::instance().open(":memory:")); // Ausgangszustand wiederherstellen
    }

    void test_migration_isIdempotent()
    {
        // Zweimaliges Öffnen nach der Migration darf die Spalte nicht erneut
        // anlegen — ein zweites ALTER TABLE mit demselben Spaltennamen wäre
        // ein SQL-Fehler und würde open() fehlschlagen lassen.
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString dbPath = tempDir.path() + QStringLiteral("/idempotent.db");

        QVERIFY(Database::instance().open(dbPath));
        {
            QSqlQuery q(QSqlDatabase::database("spm_main"));
            QVERIFY(q.exec("DROP TABLE IF EXISTS share_splits"));
            QVERIFY(q.exec("CREATE TABLE share_splits ("
                           "guid TEXT PRIMARY KEY, "
                           "share_guid TEXT NOT NULL, "
                           "date TEXT NOT NULL, "
                           "ratio_new REAL NOT NULL, "
                           "ratio_old REAL NOT NULL, "
                           "prices_adjusted INTEGER DEFAULT 0, "
                           "comment TEXT, "
                           "UNIQUE(share_guid, date))"));
        }
        Database::instance().close();

        QVERIFY(Database::instance().open(dbPath));   // migriert
        Database::instance().close();
        QVERIFY(Database::instance().open(dbPath));   // darf nicht erneut migrieren

        QSqlQuery q(QSqlDatabase::database("spm_main"));
        QVERIFY(q.exec("PRAGMA table_info(share_splits)"));
        int documentColumns = 0;
        while (q.next()) {
            if (q.value(1).toString() == QStringLiteral("document"))
                ++documentColumns;
        }
        QCOMPARE(documentColumns, 1);

        Database::instance().close();
        QVERIFY(Database::instance().open(":memory:")); // Ausgangszustand wiederherstellen
    }

    // ── Schema migration: dividends.ex_date / dividends.depot_number (21.08.2026) ──
    //
    // Grundlage für die Plausibilitätsprüfung der Dividenden-Stückzahl, siehe
    // ARCHITECTURE.md. Gleiches Vorgehen wie bei share_splits.document oben.

    void test_dividends_has_ex_date_and_depot_number_columns()
    {
        QSqlQuery q(QSqlDatabase::database("spm_main"));
        QVERIFY(q.exec("PRAGMA table_info(dividends)"));

        bool foundExDate = false, foundDepotNumber = false;
        while (q.next()) {
            const QString column = q.value(1).toString();
            if (column == QStringLiteral("ex_date"))      foundExDate = true;
            if (column == QStringLiteral("depot_number")) foundDepotNumber = true;
        }
        QVERIFY2(foundExDate,      "Spalte 'ex_date' fehlt in dividends");
        QVERIFY2(foundDepotNumber, "Spalte 'depot_number' fehlt in dividends");
    }

    void test_migration_addsMissingDividendColumns()
    {
        // Der eigentliche Prüfgegenstand: eine Datenbank, die dividends noch
        // ohne ex_date/depot_number führt — der Zustand eines Portfolios, das
        // vor diesem Migrationsschritt zuletzt geöffnet wurde.
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString dbPath = tempDir.path() + QStringLiteral("/migration-dividends.db");

        QVERIFY(Database::instance().open(dbPath));
        {
            QSqlQuery q(QSqlDatabase::database("spm_main"));
            QVERIFY(q.exec("DROP TABLE IF EXISTS dividends"));
            QVERIFY(q.exec("CREATE TABLE dividends ("
                           "guid TEXT PRIMARY KEY, "
                           "share_guid TEXT NOT NULL REFERENCES shares(guid) ON DELETE CASCADE, "
                           "datetime TEXT NOT NULL, "
                           "rate REAL NOT NULL CHECK(rate >= 0), "
                           "volume REAL NOT NULL CHECK(volume > 0), "
                           "tax_at_source REAL DEFAULT 0, "
                           "capital_gains_tax REAL DEFAULT 0, "
                           "solidarity_tax REAL DEFAULT 0, "
                           "price_at_payday REAL DEFAULT 0, "
                           "enable_fc INTEGER DEFAULT 0, "
                           "exchange_ratio REAL DEFAULT 1, "
                           "currency TEXT DEFAULT 'EUR', "
                           "document TEXT)"));
            QVERIFY(q.exec("INSERT INTO shares (guid, wkn, name) "
                           "VALUES ('mig-div-share', 'MIGD01', 'Migration Dividend AG')"));
            QVERIFY(q.exec("INSERT INTO dividends "
                           "(guid, share_guid, datetime, rate, volume) "
                           "VALUES ('mig-div', 'mig-div-share', '2024-05-15T00:00:00', 1.5, 100)"));
        }
        Database::instance().close();

        // Erneutes Öffnen zieht die fehlenden Spalten nach.
        QVERIFY(Database::instance().open(dbPath));

        QSqlQuery q(QSqlDatabase::database("spm_main"));
        QVERIFY(q.exec("PRAGMA table_info(dividends)"));
        bool foundExDate = false, foundDepotNumber = false;
        while (q.next()) {
            const QString column = q.value(1).toString();
            if (column == QStringLiteral("ex_date"))      foundExDate = true;
            if (column == QStringLiteral("depot_number")) foundDepotNumber = true;
        }
        QVERIFY2(foundExDate,      "Spalte 'ex_date' wurde nicht nachgezogen");
        QVERIFY2(foundDepotNumber, "Spalte 'depot_number' wurde nicht nachgezogen");

        // Die bereits erfasste Dividende ist noch da, mit leerem ex_date/
        // depot_number — die Migration darf keine Daten kosten.
        QVERIFY(q.exec("SELECT rate, ex_date, depot_number FROM dividends WHERE guid = 'mig-div'"));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toDouble(), 1.5);
        QVERIFY(!q.value(1).isValid() || q.value(1).toString().isEmpty());
        QVERIFY(!q.value(2).isValid() || q.value(2).toString().isEmpty());

        Database::instance().close();
        QVERIFY(Database::instance().open(":memory:")); // Ausgangszustand wiederherstellen
    }

    void test_migration_dividendColumns_isIdempotent()
    {
        // Zweimaliges Öffnen nach der Migration darf die Spalten nicht
        // erneut anlegen — ein zweites ALTER TABLE mit demselben Spaltennamen
        // wäre ein SQL-Fehler und würde open() fehlschlagen lassen.
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString dbPath = tempDir.path() + QStringLiteral("/idempotent-dividends.db");

        QVERIFY(Database::instance().open(dbPath));
        {
            QSqlQuery q(QSqlDatabase::database("spm_main"));
            QVERIFY(q.exec("DROP TABLE IF EXISTS dividends"));
            QVERIFY(q.exec("CREATE TABLE dividends ("
                           "guid TEXT PRIMARY KEY, "
                           "share_guid TEXT NOT NULL, "
                           "datetime TEXT NOT NULL, "
                           "rate REAL NOT NULL, "
                           "volume REAL NOT NULL, "
                           "document TEXT)"));
        }
        Database::instance().close();

        QVERIFY(Database::instance().open(dbPath));   // migriert
        Database::instance().close();
        QVERIFY(Database::instance().open(dbPath));   // darf nicht erneut migrieren

        QSqlQuery q(QSqlDatabase::database("spm_main"));
        QVERIFY(q.exec("PRAGMA table_info(dividends)"));
        int exDateColumns = 0, depotNumberColumns = 0;
        while (q.next()) {
            const QString column = q.value(1).toString();
            if (column == QStringLiteral("ex_date"))      ++exDateColumns;
            if (column == QStringLiteral("depot_number")) ++depotNumberColumns;
        }
        QCOMPARE(exDateColumns,      1);
        QCOMPARE(depotNumberColumns, 1);

        Database::instance().close();
        QVERIFY(Database::instance().open(":memory:")); // Ausgangszustand wiederherstellen
    }

    // ── DepotNumberNormalizer (Bugfix 18.09.2026) ─────────────────────────
    //
    // Die Regel selbst, ohne Datenbank. Sie ist die einzige Stelle, die über
    // die Umstellung entscheidet — Database::migrateDepotNumbers() und
    // PortfolioImporter rufen beide nur sie auf.

    void test_depotNormalizer_data()
    {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("expected");

        // Das Feldformat aus Nessies ShareList.xml.
        QTest::newRow("ING")            << QStringLiteral("8006189848 - ING diba")   << QStringLiteral("8006189848");
        QTest::newRow("DKB")            << QStringLiteral("501403950 - DKB")         << QStringLiteral("501403950");
        // Führende Null bleibt — die Nummer ist Text, keine Zahl (Consors).
        QTest::newRow("leadingZero")    << QStringLiteral("0878031421 - Consors")    << QStringLiteral("0878031421");
        // Umgebende Leerzeichen gehören nicht zur Nummer.
        QTest::newRow("outerSpaces")    << QStringLiteral("  8006189848 - ING diba") << QStringLiteral("8006189848");
        // Bankname mit eigenem " - ": geschnitten wird am ERSTEN Vorkommen.
        QTest::newRow("dashInBankName") << QStringLiteral("8006189848 - ING - diba") << QStringLiteral("8006189848");

        // Gegenproben: alles, was nicht eindeutig das alte Format ist, bleibt.
        QTest::newRow("alreadyClean")   << QStringLiteral("8006189848")              << QStringLiteral("8006189848");
        QTest::newRow("empty")          << QStringLiteral("")                        << QStringLiteral("");
        QTest::newRow("nonNumeric")     << QStringLiteral("ING - diba")              << QStringLiteral("ING - diba");
        QTest::newRow("mixedPrefix")    << QStringLiteral("DE8006 - ING")            << QStringLiteral("DE8006 - ING");
        QTest::newRow("noNumber")       << QStringLiteral(" - DKB")                  << QStringLiteral(" - DKB");
        QTest::newRow("noSpaces")       << QStringLiteral("8006189848-ING")          << QStringLiteral("8006189848-ING");
    }

    void test_depotNormalizer()
    {
        QFETCH(QString, input);
        QFETCH(QString, expected);
        QCOMPARE(DepotNumberNormalizer::normalize(input), expected);
        QCOMPARE(DepotNumberNormalizer::needsNormalization(input), input != expected);
    }

    // ── Depotnummer-Migration (Bugfix 18.09.2026) ─────────────────────────
    //
    // Nessies Bugreport: der Dividenden-Dialog meldete "Bestand 0,0000 Stk."
    // bei 25 gehaltenen Anteilen. Die importierten Käufe trugen
    // "8006189848 - ING diba", der Dialog vergleicht gegen "8006189848".
    // Siehe ARCHITECTURE.md, "Depotnummern im alten Format (Nummer - Bank)".

    void test_depotMigration_memoryDatabase_nothingToReport()
    {
        // Die In-Memory-Datenbank aus initTestCase() ist frisch: kein Wert im
        // alten Format, also kein Bericht.
        QVERIFY(!Database::instance().depotNumberMigrationReport().attempted);
    }

    void test_depotMigration_normalizesBuysSalesAndDividends()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString dbPath = tempDir.path() + QStringLiteral("/ShareList.db");
        QVERIFY(seedLegacyDepotNumbers(dbPath));

        QVERIFY(Database::instance().open(dbPath));

        QCOMPARE(depotNumberOf("buys",      "b-ing"), QStringLiteral("8006189848"));
        QCOMPARE(depotNumberOf("buys",      "b-dkb"), QStringLiteral("501403950"));
        QCOMPARE(depotNumberOf("sales",     "s-ing"), QStringLiteral("8006189848"));
        QCOMPARE(depotNumberOf("dividends", "d-ing"), QStringLiteral("8006189848"));

        // Gegenproben bleiben, wie sie waren.
        QCOMPARE(depotNumberOf("buys", "b-new"), QStringLiteral("8006189848"));
        QCOMPARE(depotNumberOf("buys", "b-odd"), QStringLiteral("ING - diba"));

        Database::instance().close();
        QVERIFY(Database::instance().open(":memory:")); // Ausgangszustand wiederherstellen
    }

    void test_depotMigration_reportNamesCountsAndReplacements()
    {
        // Der Bericht ist die Grundlage des Hinweisdialogs in MainWindow — er
        // muss WAS (Anzahl je Tabelle) und WIE (alter → neuer Wert) enthalten.
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString dbPath = tempDir.path() + QStringLiteral("/ShareList.db");
        QVERIFY(seedLegacyDepotNumbers(dbPath));

        QVERIFY(Database::instance().open(dbPath));
        const DepotNumberMigrationReport report = Database::instance().depotNumberMigrationReport();

        QVERIFY(report.attempted);
        QVERIFY(report.succeeded);
        QVERIFY(report.errorText.isEmpty());
        QCOMPARE(report.buys,      2);   // b-new und b-odd zählen NICHT mit
        QCOMPARE(report.sales,     1);
        QCOMPARE(report.dividends, 1);
        QCOMPARE(report.total(),   4);

        QCOMPARE(report.replacements.size(), 2);
        QCOMPARE(report.replacements.value(QStringLiteral("8006189848 - ING diba")),
                 QStringLiteral("8006189848"));
        QCOMPARE(report.replacements.value(QStringLiteral("501403950 - DKB")),
                 QStringLiteral("501403950"));

        Database::instance().close();
        QVERIFY(Database::instance().open(":memory:"));
    }

    void test_depotMigration_backupHoldsPreviousState()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString dbPath = tempDir.path() + QStringLiteral("/ShareList.db");
        QVERIFY(seedLegacyDepotNumbers(dbPath));

        QVERIFY(Database::instance().open(dbPath));
        const QString backupPath = Database::instance().depotNumberMigrationReport().backupPath;
        Database::instance().close();

        QVERIFY(!backupPath.isEmpty());
        QVERIFY(QFileInfo::exists(backupPath));
        QCOMPARE(QFileInfo(backupPath).absolutePath(), QFileInfo(dbPath).absolutePath());

        // Die Backup-Rotation in MainWindow::createBackup() filtert nach
        // "*_ShareList_*.db". Enthielte der Name "_ShareList_", würde die
        // Sicherung beim nächsten Start mit wegrotiert.
        const QString fileName = QFileInfo(backupPath).fileName();
        QVERIFY(fileName.startsWith(QStringLiteral("ShareList_vor_Depotnummer_Migration_")));
        QVERIFY2(!fileName.contains(QStringLiteral("_ShareList_")),
                 qPrintable(fileName));

        // Inhalt: der Stand VOR der Umstellung. Eigene Verbindung, damit die
        // Hauptverbindung unberührt bleibt; eigener Gültigkeitsbereich, damit
        // removeDatabase() keine offene Referenz mehr vorfindet.
        const QString connection = QStringLiteral("depot_backup_check");
        {
            QSqlDatabase backup = QSqlDatabase::addDatabase("QSQLITE", connection);
            backup.setDatabaseName(backupPath);
            QVERIFY(backup.open());

            QSqlQuery q(backup);
            QVERIFY(q.exec("SELECT depot_number FROM buys WHERE guid = 'b-ing'"));
            QVERIFY(q.next());
            QCOMPARE(q.value(0).toString(), QStringLiteral("8006189848 - ING diba"));

            QVERIFY(q.exec("SELECT depot_number FROM sales WHERE guid = 's-ing'"));
            QVERIFY(q.next());
            QCOMPARE(q.value(0).toString(), QStringLiteral("8006189848 - ING diba"));

            q.finish();
            backup.close();
        }
        QSqlDatabase::removeDatabase(connection);

        QVERIFY(Database::instance().open(":memory:"));
    }

    void test_depotMigration_isIdempotent()
    {
        // Ab dem zweiten Öffnen gibt es nichts mehr umzustellen: kein
        // Bericht (also kein erneuter Hinweis beim Start) und vor allem
        // keine weitere Sicherung.
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString dbPath = tempDir.path() + QStringLiteral("/ShareList.db");
        QVERIFY(seedLegacyDepotNumbers(dbPath));

        QVERIFY(Database::instance().open(dbPath));   // migriert
        QVERIFY(Database::instance().depotNumberMigrationReport().attempted);
        Database::instance().close();

        QVERIFY(Database::instance().open(dbPath));   // nichts mehr zu tun
        QVERIFY(!Database::instance().depotNumberMigrationReport().attempted);
        QCOMPARE(depotNumberOf("buys", "b-ing"), QStringLiteral("8006189848"));
        Database::instance().close();

        QCOMPARE(migrationBackupsIn(tempDir.path()).size(), 1);

        QVERIFY(Database::instance().open(":memory:"));
    }

    void test_depotMigration_nothingToDo_noBackup()
    {
        // Ein Portfolio ganz ohne Altwerte bekommt weder Bericht noch
        // Sicherung — der Schritt darf im Normalfall keine Spuren hinterlassen.
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString dbPath = tempDir.path() + QStringLiteral("/Clean.db");

        QVERIFY(Database::instance().open(dbPath));
        {
            QSqlQuery q(QSqlDatabase::database("spm_main"));
            QVERIFY(q.exec("INSERT INTO shares (guid, wkn, name) VALUES ('c-share', 'CLN001', 'Clean AG')"));
            QVERIFY(q.exec("INSERT INTO buys (guid, share_guid, depot_number, order_number, datetime, volume, price) "
                           "VALUES ('c-buy', 'c-share', '8006189848', 'C-1', '2024-01-10T00:00:00', 10, 100)"));
        }
        Database::instance().close();

        QVERIFY(Database::instance().open(dbPath));
        QVERIFY(!Database::instance().depotNumberMigrationReport().attempted);
        Database::instance().close();

        QVERIFY(migrationBackupsIn(tempDir.path()).isEmpty());

        QVERIFY(Database::instance().open(":memory:"));
    }

    void test_depotMigration_backupFails_dataUnchanged_retriedOnNextOpen()
    {
        // Zusage: ohne Sicherung keine Umstellung. Herbeigeführt, indem die
        // Sicherungspfade der nächsten Sekunden vorab belegt werden —
        // migrationBackupPath() ist dafür mit ausdrücklichem Zeitpunkt
        // öffentlich. Zehn Sekunden Puffer, damit ein langsamer CI-Runner
        // zwischen Belegen und open() nicht aus dem Fenster läuft.
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString dbPath = tempDir.path() + QStringLiteral("/ShareList.db");
        QVERIFY(seedLegacyDepotNumbers(dbPath));

        const QDateTime now = QDateTime::currentDateTime();
        QStringList blocked;
        for (int s = 0; s <= 10; ++s) {
            const QString path = Database::migrationBackupPath(dbPath, now.addSecs(s));
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("belegt");
            blocked.append(path);
        }

        QVERIFY(Database::instance().open(dbPath));   // open() selbst scheitert NICHT
        const DepotNumberMigrationReport failed = Database::instance().depotNumberMigrationReport();
        QVERIFY(failed.attempted);
        QVERIFY(!failed.succeeded);
        QVERIFY(!failed.errorText.isEmpty());
        QVERIFY(failed.backupPath.isEmpty());
        QCOMPARE(failed.total(), 4);                 // der Bericht nennt trotzdem den Umfang

        // Nichts umgestellt, und die fremde Datei am Sicherungspfad ist
        // unangetastet — sie wurde weder überschrieben noch gelöscht.
        QCOMPARE(depotNumberOf("buys", "b-ing"), QStringLiteral("8006189848 - ING diba"));
        for (const QString& path : std::as_const(blocked)) {
            QFile f(path);
            QVERIFY(f.open(QIODevice::ReadOnly));
            QCOMPARE(f.readAll(), QByteArray("belegt"));
        }
        Database::instance().close();

        // Nächstes Öffnen: Weg frei, Umstellung wird nachgeholt.
        for (const QString& path : std::as_const(blocked))
            QVERIFY(QFile::remove(path));

        QVERIFY(Database::instance().open(dbPath));
        QVERIFY(Database::instance().depotNumberMigrationReport().succeeded);
        QCOMPARE(depotNumberOf("buys", "b-ing"), QStringLiteral("8006189848"));
        Database::instance().close();

        QVERIFY(Database::instance().open(":memory:"));
    }

    void test_dividends_columnOrder_matchesBetweenFreshAndMigratedSchema()
    {
        // ensureColumn() haengt eine nachgezogene Spalte immer ans ENDE der
        // Tabelle an (SQLite-Vorgabe, siehe Kommentar über der Tabelle in
        // Database.cpp). Damit ein frisch angelegtes Portfolio und ein
        // migriertes dieselbe Spaltenreihenfolge in `dividends` haben, muss
        // die Reihenfolge in createSchema() das exakt vorwegnehmen — dieser
        // Test belegt das, statt es nur als Kommentar zu behaupten.
        QSqlQuery freshQuery(QSqlDatabase::database("spm_main")); // :memory:, frisch aus initTestCase()
        QVERIFY(freshQuery.exec("PRAGMA table_info(dividends)"));
        QStringList freshColumns;
        while (freshQuery.next())
            freshColumns << freshQuery.value(1).toString();

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString dbPath = tempDir.path() + QStringLiteral("/column-order.db");

        QVERIFY(Database::instance().open(dbPath));
        {
            QSqlQuery q(QSqlDatabase::database("spm_main"));
            QVERIFY(q.exec("DROP TABLE IF EXISTS dividends"));
            QVERIFY(q.exec("CREATE TABLE dividends ("
                           "guid TEXT PRIMARY KEY, "
                           "share_guid TEXT NOT NULL, "
                           "datetime TEXT NOT NULL, "
                           "rate REAL NOT NULL, "
                           "volume REAL NOT NULL, "
                           "tax_at_source REAL DEFAULT 0, "
                           "capital_gains_tax REAL DEFAULT 0, "
                           "solidarity_tax REAL DEFAULT 0, "
                           "price_at_payday REAL DEFAULT 0, "
                           "enable_fc INTEGER DEFAULT 0, "
                           "exchange_ratio REAL DEFAULT 1, "
                           "currency TEXT DEFAULT 'EUR', "
                           "document TEXT)"));
        }
        Database::instance().close();
        QVERIFY(Database::instance().open(dbPath)); // migriert ex_date/depot_number nach

        QSqlQuery migratedQuery(QSqlDatabase::database("spm_main"));
        QVERIFY(migratedQuery.exec("PRAGMA table_info(dividends)"));
        QStringList migratedColumns;
        while (migratedQuery.next())
            migratedColumns << migratedQuery.value(1).toString();

        QCOMPARE(migratedColumns, freshColumns);

        Database::instance().close();
        QVERIFY(Database::instance().open(":memory:")); // Ausgangszustand wiederherstellen
    }
};

QTEST_MAIN(TestDatabase)
#include "tst_database.moc"

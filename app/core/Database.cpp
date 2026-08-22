// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "Database.h"

#include <QSqlQuery>
#include <QDebug>

// ── Singleton ────────────────────────────────────────────────────────────────
Database& Database::instance()
{
    static Database db;
    return db;
}

Database::Database(QObject* parent)
    : QObject(parent)
{}

Database::~Database()
{
    close();
}

// ── Public API ────────────────────────────────────────────────────────────────
bool Database::open(const QString& path)
{
    // If a connection with this name still exists (e.g. from a previous open/close
    // cycle), remove it first so Qt doesn't warn about a duplicate connection name.
    if (QSqlDatabase::contains(k_connectionName)) {
        m_db = QSqlDatabase(); // drop our reference before removing
        QSqlDatabase::removeDatabase(k_connectionName);
    }

    m_db = QSqlDatabase::addDatabase("QSQLITE", k_connectionName);
    m_db.setDatabaseName(path);

    if (!m_db.open()) {
        qCritical() << "[Database] Failed to open:" << m_db.lastError().text();
        return false;
    }

    // Enable WAL mode and foreign keys for every connection
    execute("PRAGMA journal_mode=WAL");
    execute("PRAGMA foreign_keys=ON");

    if (!createSchema()) {
        qCritical() << "[Database] Schema creation failed";
        return false;
    }

    // Bestehende Portfolios auf den aktuellen Stand bringen (08.08.2026).
    // createSchema() legt fehlende TABELLEN an, aber keine fehlenden SPALTEN
    // in bereits vorhandenen Tabellen — siehe migrateSchema().
    if (!migrateSchema()) {
        qCritical() << "[Database] Schema migration failed";
        return false;
    }

    qInfo() << "[Database] Opened:" << path;
    return true;
}

void Database::close()
{
    if (m_db.isOpen())
        m_db.close();

    // Reset m_db to a default-constructed (invalid) instance so Qt's internal
    // reference count drops to zero before removeDatabase() is called.
    // open() performs the same reset before addDatabase() to handle rapid
    // close/open cycles without Qt warning about duplicate connection names.
    m_db = QSqlDatabase();

    if (QSqlDatabase::contains(k_connectionName))
        QSqlDatabase::removeDatabase(k_connectionName);
}

bool Database::isOpen() const
{
    return m_db.isOpen();
}

QSqlError Database::lastError() const
{
    return m_db.lastError();
}

bool Database::execute(const QString& sql)
{
    QSqlQuery sqlQuery(m_db);
    if (!sqlQuery.exec(sql)) {
        qWarning() << "[Database] SQL error:" << sqlQuery.lastError().text()
                   << "\nStatement:" << sql;
        return false;
    }
    return true;
}

bool Database::beginTransaction()    { return m_db.transaction(); }
bool Database::commitTransaction()   { return m_db.commit(); }
bool Database::rollbackTransaction() { return m_db.rollback(); }

// ── Schema ────────────────────────────────────────────────────────────────────
bool Database::createSchema()
{
    const QStringList ddl = {

        // ── shares ───────────────────────────────────────────────────────
        R"(
        CREATE TABLE IF NOT EXISTS shares (
            guid                        TEXT    PRIMARY KEY,
            wkn                         TEXT    NOT NULL UNIQUE,
            isin                        TEXT,
            name                        TEXT    NOT NULL,
            share_type                  INTEGER DEFAULT 0,
            currency                    TEXT    DEFAULT 'EUR',
            add_datetime                TEXT,
            cur_price                   REAL    DEFAULT 0,
            prev_day_price              REAL    DEFAULT 0,
            last_internet_update        TEXT,
            last_price_update           TEXT,
            update_type                 INTEGER DEFAULT 3,
            market_price_parsing_type   INTEGER DEFAULT 0,
            market_price_url            TEXT,
            market_price_encoding       TEXT    DEFAULT 'UTF-8',
            daily_values_parsing_type   INTEGER DEFAULT 0,
            daily_values_url            TEXT,
            daily_values_encoding       TEXT    DEFAULT 'UTF-8',
            details_website_url         TEXT,
            image_path                  TEXT
        ))",

        // ── buys ─────────────────────────────────────────────────────────
        R"(
        CREATE TABLE IF NOT EXISTS buys (
            guid            TEXT    PRIMARY KEY,
            share_guid      TEXT    NOT NULL REFERENCES shares(guid) ON DELETE CASCADE,
            depot_number    TEXT,
            order_number    TEXT    UNIQUE,
            datetime        TEXT    NOT NULL,
            volume          REAL    NOT NULL CHECK(volume > 0),
            volume_sold     REAL    DEFAULT 0,
            price           REAL    NOT NULL CHECK(price > 0),
            brokerage_guid  TEXT,
            document        TEXT
        ))",

        // ── sales ────────────────────────────────────────────────────────
        R"(
        CREATE TABLE IF NOT EXISTS sales (
            guid                TEXT    PRIMARY KEY,
            share_guid          TEXT    NOT NULL REFERENCES shares(guid) ON DELETE CASCADE,
            depot_number        TEXT,
            order_number        TEXT    UNIQUE,
            datetime            TEXT    NOT NULL,
            volume              REAL    NOT NULL CHECK(volume > 0),
            sale_price          REAL    NOT NULL CHECK(sale_price > 0),
            tax_at_source       REAL    DEFAULT 0,
            capital_gains_tax   REAL    DEFAULT 0,
            solidarity_tax      REAL    DEFAULT 0,
            brokerage_guid      TEXT,
            document            TEXT
        ))",

        // ── sale_buy_details ──────────────────────────────────────────────
        R"(
        CREATE TABLE IF NOT EXISTS sale_buy_details (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            sale_guid       TEXT    NOT NULL REFERENCES sales(guid) ON DELETE CASCADE,
            buy_guid        TEXT    REFERENCES buys(guid),
            datetime        TEXT    NOT NULL,
            volume          REAL    NOT NULL,
            buy_price       REAL    NOT NULL,
            reduction_part  REAL    DEFAULT 0,
            brokerage_part  REAL    DEFAULT 0
        ))",

        // ── brokerage ────────────────────────────────────────────────────
        R"(
        CREATE TABLE IF NOT EXISTS brokerage (
            guid            TEXT    PRIMARY KEY,
            share_guid      TEXT    NOT NULL REFERENCES shares(guid) ON DELETE CASCADE,
            buy_guid        TEXT    REFERENCES buys(guid),
            sale_guid       TEXT    REFERENCES sales(guid),
            datetime        TEXT    NOT NULL,
            provision       REAL    DEFAULT 0,
            broker_fee      REAL    DEFAULT 0,
            trader_fee      REAL    DEFAULT 0,
            reduction       REAL    DEFAULT 0,
            document        TEXT
        ))",

        // ── dividends ────────────────────────────────────────────────────
        // ex_date/depot_number stehen bewusst als LETZTE Spalten hier, obwohl
        // sie inhaltlich eher zu datetime gehören würden: ensureColumn()
        // haengt eine nachgezogene Spalte per ALTER TABLE ADD COLUMN immer
        // ans Ende an, das kann SQLite nicht anders. Stünden sie hier weiter
        // vorn, hätte ein frisch angelegtes Portfolio eine andere
        // Spaltenreihenfolge in `dividends` als ein migriertes — verwirrend
        // bei jedem manuellen `PRAGMA table_info`/`SELECT *`, auch wenn der
        // Code selbst (DividendRepository::fromQuery()) ausschliesslich über
        // Spaltennamen zugreift und davon nicht betroffen ist.
        R"(
        CREATE TABLE IF NOT EXISTS dividends (
            guid                TEXT    PRIMARY KEY,
            share_guid          TEXT    NOT NULL REFERENCES shares(guid) ON DELETE CASCADE,
            datetime            TEXT    NOT NULL,
            rate                REAL    NOT NULL CHECK(rate >= 0),
            volume              REAL    NOT NULL CHECK(volume > 0),
            tax_at_source       REAL    DEFAULT 0,
            capital_gains_tax   REAL    DEFAULT 0,
            solidarity_tax      REAL    DEFAULT 0,
            price_at_payday     REAL    DEFAULT 0,
            enable_fc           INTEGER DEFAULT 0,
            exchange_ratio      REAL    DEFAULT 1,
            currency            TEXT    DEFAULT 'EUR',
            document            TEXT,
            ex_date             TEXT,
            depot_number        TEXT
        ))",

        // ── daily_values ─────────────────────────────────────────────────
        R"(
        CREATE TABLE IF NOT EXISTS daily_values (
            share_guid  TEXT    NOT NULL REFERENCES shares(guid) ON DELETE CASCADE,
            date        TEXT    NOT NULL,
            opening     REAL    DEFAULT 0,
            closing     REAL    DEFAULT 0,
            top         REAL    DEFAULT 0,
            bottom      REAL    DEFAULT 0,
            volume      REAL    DEFAULT 0,
            PRIMARY KEY (share_guid, date)
        ))",

        // ── share_splits ─────────────────────────────────────────────────
        // Grundlage für die Aktiensplit-Behandlung (ARCHITECTURE.md, "Offene
        // Punkte", "Aktiensplits werden nicht behandelt"). Eigene GUID je
        // Split (wie buys/brokerage, nicht wie daily_values' zusammengesetzter
        // Primärschlüssel) plus UNIQUE(share_guid, date), da zwei Splits
        // derselben Aktie am selben Tag fachlich keinen Sinn ergeben.
        // prices_adjusted ist bewusst je Split gesetzt, nicht je Aktie — bei
        // mehreren Splits kann die Kurshistorie unterschiedlich weit
        // bereinigt sein.
        R"(
        CREATE TABLE IF NOT EXISTS share_splits (
            guid            TEXT    PRIMARY KEY,
            share_guid      TEXT    NOT NULL REFERENCES shares(guid) ON DELETE CASCADE,
            date            TEXT    NOT NULL,
            ratio_new       REAL    NOT NULL CHECK(ratio_new > 0),
            ratio_old       REAL    NOT NULL CHECK(ratio_old > 0),
            prices_adjusted INTEGER DEFAULT 0,
            comment         TEXT,
            document        TEXT,
            UNIQUE(share_guid, date)
        ))",

        // ── Indexes ───────────────────────────────────────────────────────
        "CREATE INDEX IF NOT EXISTS idx_buys_share     ON buys(share_guid)",
        "CREATE INDEX IF NOT EXISTS idx_buys_datetime  ON buys(datetime)",
        "CREATE INDEX IF NOT EXISTS idx_sales_share    ON sales(share_guid)",
        "CREATE INDEX IF NOT EXISTS idx_dividends_share ON dividends(share_guid)",
        "CREATE INDEX IF NOT EXISTS idx_daily_date     ON daily_values(date)",
        "CREATE INDEX IF NOT EXISTS idx_splits_share   ON share_splits(share_guid)",
    };

    beginTransaction();
    for (const QString& stmt : ddl) {
        if (!execute(stmt)) {
            rollbackTransaction();
            return false;
        }
    }
    return commitTransaction();
}

// ── Migration ─────────────────────────────────────────────────────────────────

bool Database::migrateSchema()
{
    // Eine Zeile je nachgerüsteter Spalte. Reihenfolge ist unerheblich, jeder
    // Eintrag ist für sich idempotent.
    //
    // 08.08.2026 — share_splits.document: Splits bekommen einen Beleg wie
    // Käufe, Verkäufe, Dividenden und Kosten auch. Portfolios, die zwischen
    // Phase 1 und Phase 3a geöffnet wurden, haben die Tabelle bereits ohne
    // diese Spalte angelegt.
    //
    // 21.08.2026 — dividends.ex_date / dividends.depot_number: Grundlage für
    // die Plausibilitätsprüfung der Dividenden-Stückzahl, siehe
    // ARCHITECTURE.md, "Plausibilitätsprüfung der Dividenden-Stückzahl".
    // Beide Spalten bleiben auf DB-Ebene bewusst NULLable — ein bestehendes
    // Portfolio hat für seine alten Dividenden keinen echten Wert dafür, und
    // ein per ALTER TABLE untergeschobener Platzhalter wäre falsche Angabe
    // statt fehlender Angabe. Die "Muss"-Eigenschaft, die Nessie für beide
    // Felder festgelegt hat, sitzt stattdessen ausschliesslich in der
    // Formularvalidierung (`PresenterDividendEdit::validateInput()`,
    // Phase 2) — eine bereits vorhandene Dividende ohne die beiden Felder
    // lässt sich weiterhin öffnen und ansehen, nur das erneute Speichern
    // verlangt dann die Nachpflege.
    struct ColumnMigration {
        const char* table;
        const char* column;
        const char* definition;
    };

    static const ColumnMigration migrations[] = {
        { "share_splits", "document",     "TEXT" },
        { "dividends",    "ex_date",      "TEXT" },
        { "dividends",    "depot_number", "TEXT" },
    };

    for (const ColumnMigration& migration : migrations) {
        if (!ensureColumn(QString::fromLatin1(migration.table),
                          QString::fromLatin1(migration.column),
                          QString::fromLatin1(migration.definition)))
        {
            return false;
        }
    }
    return true;
}

bool Database::ensureColumn(const QString& table,
                            const QString& column,
                            const QString& definition)
{
    if (hasColumn(table, column))
        return true;

    // Tabellen-, Spalten- und Typnamen lassen sich in DDL nicht binden —
    // sie kommen ausschliesslich aus der obigen, fest einkompilierten
    // Tabelle, nie aus Benutzereingaben.
    const QString sql = QStringLiteral("ALTER TABLE %1 ADD COLUMN %2 %3")
                            .arg(table, column, definition);

    if (!execute(sql)) {
        qCritical() << "[Database] Could not add column" << column << "to" << table;
        return false;
    }

    qInfo() << "[Database] Migrated: added column" << column << "to" << table;
    return true;
}

bool Database::hasColumn(const QString& table, const QString& column) const
{
    QSqlQuery sqlQuery(m_db);

    // PRAGMA table_info liefert eine leere Ergebnismenge, wenn die Tabelle
    // gar nicht existiert — der Aufrufer bekommt dann false und würde ein
    // ALTER TABLE auf eine fehlende Tabelle versuchen. Das kann hier nicht
    // eintreten, weil migrateSchema() erst nach createSchema() läuft.
    if (!sqlQuery.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table))) {
        qWarning() << "[Database] PRAGMA table_info failed for" << table
                   << ":" << sqlQuery.lastError().text();
        return false;
    }

    while (sqlQuery.next()) {
        // Spalte 1 der PRAGMA-Ausgabe ist der Spaltenname.
        if (sqlQuery.value(1).toString().compare(column, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

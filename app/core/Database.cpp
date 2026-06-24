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
            document            TEXT
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

        // ── Indexes ───────────────────────────────────────────────────────
        "CREATE INDEX IF NOT EXISTS idx_buys_share     ON buys(share_guid)",
        "CREATE INDEX IF NOT EXISTS idx_buys_datetime  ON buys(datetime)",
        "CREATE INDEX IF NOT EXISTS idx_sales_share    ON sales(share_guid)",
        "CREATE INDEX IF NOT EXISTS idx_dividends_share ON dividends(share_guid)",
        "CREATE INDEX IF NOT EXISTS idx_daily_date     ON daily_values(date)",
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

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "DatabaseVersion.h"

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>

/**
 * @brief Central database access class.
 *
 * Wraps QSqlDatabase for SQLite, handles schema creation/migration,
 * and provides a shared connection for all repositories.
 *
 * Usage:
 *   Database::instance().open("/path/to/portfolio.db");
 *   // Repositories use Database::instance().connection()
 */
class Database : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Returns the Database library version string.
     *
     * Independent of the SharePortfolioManager app version — see
     * app/core/CMakeLists.txt (project(Database VERSION ...)) and
     * ARCHITECTURE.md, Abschnitt "Versionierung".
     * @return Version string (e.g. "1.0.0").
     */
    static QString version() { return QStringLiteral(DATABASE_VERSION_STRING); }

    /**
     * @brief Returns the named connection string used for all database access.
     *
     * Always use this getter instead of hardcoding the string "spm_main".
     * Pass the result to QSqlDatabase::database() and QSqlQuery constructors
     * wherever a connection name is required outside the Database class itself.
     *
     * @code
     * QSqlDatabase db = QSqlDatabase::database(Database::connectionName());
     * @endcode
     *
     * @return The connection name string.
     */
    static QString connectionName() { return QStringLiteral("spm_main"); }

    /**
     * @brief Returns the singleton instance.
     * @return Reference to the single Database instance.
     */
    static Database& instance();

    /**
     * @brief Open or create the SQLite database at the given path.
     * @param path  Full file path to the SQLite database file.
     * @return true on success, false if the connection could not be opened.
     */
    bool open(const QString& path);

    /**
     * @brief Close the database connection and release all resources.
     *
     * Resets the internal QSqlDatabase member to a default-constructed
     * (invalid) instance before calling QSqlDatabase::removeDatabase().
     * This ensures Qt's internal reference count drops to zero first,
     * preventing the "connection still in use" warning.
     */
    void close();

    /**
     * @brief Returns true if the database connection is currently open.
     * @return true if open, false otherwise.
     */
    bool isOpen() const;

    /**
     * @brief Returns the last database error, if any.
     * @return The most recent QSqlError; invalid if no error has occurred.
     */
    QSqlError lastError() const;

    /**
     * @brief Execute a raw SQL statement (DDL, pragmas, etc.).
     * @param sql  The SQL string to execute.
     * @return true on success, false on error.
     */
    bool execute(const QString& sql);

    /**
     * @brief Begin a database transaction.
     * @return true on success.
     */
    bool beginTransaction();

    /**
     * @brief Commit the current transaction.
     * @return true on success.
     */
    bool commitTransaction();

    /**
     * @brief Roll back the current transaction.
     * @return true on success.
     */
    bool rollbackTransaction();

private:
    explicit Database(QObject* parent = nullptr);
    ~Database() override;

    Database(const Database&)            = delete;
    Database& operator=(const Database&) = delete;

    /**
     * @brief Create all tables if they do not exist yet.
     * @return true on success, false if any DDL statement fails.
     */
    bool createSchema();

    /**
     * @brief Bring an existing database up to the current schema.
     *
     * Ergänzt 08.08.2026. Läuft in open() unmittelbar nach createSchema()
     * und ist für bereits bestehende Portfolios gedacht.
     *
     * Hintergrund: createSchema() arbeitet durchgehend mit
     * `CREATE TABLE IF NOT EXISTS`. Eine komplett NEUE Tabelle wird dadurch
     * beim nächsten Öffnen automatisch angelegt — so kam `share_splits` in
     * bestehende Portfolios (Phase 1 der Aktiensplit-Behandlung). Eine neue
     * SPALTE in einer bereits vorhandenen Tabelle bekommt man auf diesem Weg
     * aber nicht: SQLite sieht die Tabelle, vergleicht die Spaltenliste
     * nicht und tut nichts. Genau dieser Fall trat mit
     * `share_splits.document` zum ersten Mal im Projekt auf.
     *
     * Bewusst kein Versionszähler in der Datenbank: die Prüfung "existiert
     * die Spalte?" ist idempotent, kommt ohne zusätzlichen Zustand aus und
     * kann auch dann nicht aus dem Tritt geraten, wenn ein Portfolio eine
     * Version übersprungen hat.
     *
     * @return true on success, false if any migration step fails.
     */
    bool migrateSchema();

    /**
     * @brief Add a column to an existing table if it isn't there yet.
     *
     * Liest `PRAGMA table_info(<table>)` und führt bei fehlender Spalte
     * `ALTER TABLE <table> ADD COLUMN <column> <definition>` aus.
     *
     * @note SQLite kann per ALTER TABLE nur Spalten ANHÄNGEN, und die
     * Definition darf weder `PRIMARY KEY` noch `UNIQUE` enthalten und bei
     * `NOT NULL` keinen fehlenden Default haben. Für alles Weitergehende
     * (Spalte umbenennen, Constraint ändern) wäre der Umweg über eine
     * Ersatztabelle nötig — das ist hier bewusst nicht abgedeckt, solange
     * es keinen konkreten Anlass gibt.
     *
     * @param table       Name der bestehenden Tabelle.
     * @param column      Name der zu ergänzenden Spalte.
     * @param definition  SQL-Typ und Default, z. B. "TEXT" oder "INTEGER DEFAULT 0".
     * @return true, wenn die Spalte danach existiert (auch wenn sie schon vorher da war).
     */
    bool ensureColumn(const QString& table,
                      const QString& column,
                      const QString& definition);

    /**
     * @brief Returns true if @p table already has a column named @p column.
     * @param table   Name der Tabelle.
     * @param column  Name der gesuchten Spalte.
     */
    bool hasColumn(const QString& table, const QString& column) const;

    QSqlDatabase m_db;

    static constexpr const char* k_connectionName = "spm_main";
};

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

    QSqlDatabase m_db;

    static constexpr const char* k_connectionName = "spm_main";
};

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "DatabaseVersion.h"

#include <QDateTime>
#include <QMap>
#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>

/**
 * @brief Ergebnis der Depotnummer-Migration beim Öffnen eines Portfolios.
 *
 * Bugfix 18.09.2026. `Database` hat keine Oberfläche; der Bericht wird
 * deshalb nur festgehalten und von `MainWindow` nach dem Öffnen als Hinweis
 * gezeigt — der Benutzer soll erfahren, DASS, WARUM und WIE seine Daten
 * angepasst wurden. Siehe `DepotNumberNormalizer` und ARCHITECTURE.md,
 * "Depotnummern im alten Format (Nummer - Bank)".
 */
struct DepotNumberMigrationReport
{
    /// true, wenn beim letzten open() überhaupt Werte im alten Format gefunden
    /// wurden. false = nichts zu tun; alle übrigen Felder sind dann leer.
    bool attempted = false;

    /// true, wenn Sicherung UND Umstellung erfolgreich waren.
    bool succeeded = false;

    /// Pfad der vorab angelegten Sicherung (`VACUUM INTO`), leer ohne Sicherung
    /// (In-Memory-Datenbank) oder wenn sie fehlschlug.
    QString backupPath;

    /// Fehlerbeschreibung, wenn `succeeded == false`.
    QString errorText;

    /// Umgestellte (bzw. bei Fehlschlag: umzustellende) Zeilen je Tabelle.
    int buys      = 0;
    int sales     = 0;
    int dividends = 0;

    /// Vorkommende Ersetzungen, alter Wert → neuer Wert (je Wert einmal).
    QMap<QString, QString> replacements;

    /// Summe über alle drei Tabellen.
    int total() const { return buys + sales + dividends; }
};

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
     * Schliesst die Verbindung und meldet sie bei Qt ab. Die dafuer noetige
     * QSqlDatabase wird nur fuer die Dauer des Aufrufs geholt, damit beim
     * anschliessenden QSqlDatabase::removeDatabase() keine Referenz mehr
     * offen ist ("connection still in use").
     *
     * @note Muss ausdruecklich aufgerufen werden, solange die
     * QCoreApplication noch lebt — main() tut das vor dem Ende, die
     * Testziele in cleanupTestCase(). Der Destruktor holt das NICHT nach,
     * siehe dessen Begruendung in Database.cpp.
     */
    void close();

    /**
     * @brief Bericht der Depotnummer-Migration des letzten open().
     *
     * Wird bei jedem open() zurückgesetzt. `attempted == false` heisst, es
     * gab nichts umzustellen — der Normalfall ab dem zweiten Öffnen.
     */
    const DepotNumberMigrationReport& depotNumberMigrationReport() const
    {
        return m_depotNumberMigrationReport;
    }

    /**
     * @brief Pfad der Sicherung vor der Depotnummer-Migration.
     *
     * `<Name>_vor_Depotnummer_Migration_<yyyy_MM_dd_HH_mm_ss>.<Endung>` im
     * Ordner der Portfolio-Datei. Der Name BEGINNT bewusst mit dem
     * Portfolionamen, statt ihn hinter ein Präfix zu stellen, und enthält
     * damit kein `_<Name>_`: die Backup-Rotation in
     * `MainWindow::createBackup()` filtert nach `*_<Name>_*.<Endung>` und
     * würde eine solche Sicherung beim nächsten Start mit wegrotieren.
     *
     * Öffentlich und mit ausdrücklichem Zeitpunkt, damit ein Test den Pfad
     * vorab belegen und so den Fehlschlag der Sicherung herbeiführen kann —
     * der Zweig "keine Sicherung, also keine Umstellung" ist sonst nicht
     * erreichbar.
     *
     * @param portfolioPath  Pfad der Portfolio-Datei.
     * @param when           Zeitstempel für den Dateinamen.
     * @return Absoluter Pfad der Sicherungsdatei.
     */
    static QString migrationBackupPath(const QString& portfolioPath, const QDateTime& when);

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
     * @param path  Pfad der geöffneten Datenbank, für die Sicherung vor der
     *              Depotnummer-Migration (siehe migrateDepotNumbers()).
     * @return true on success, false if any migration step fails.
     */
    bool migrateSchema(const QString& path);

    /**
     * @brief Stellt Depotnummern im alten Format `<Nummer> - <Bank>` auf die
     * reine Nummer um (Bugfix 18.09.2026).
     *
     * Läuft am Ende von migrateSchema(). Ablauf:
     * 1. Kandidaten in `buys`, `sales` und `dividends` sammeln — die Regel
     *    selbst steht ausschliesslich in `DepotNumberNormalizer::normalize()`,
     *    SQL filtert nur grob vor. Gibt es keine, endet der Schritt hier:
     *    kein Bericht, keine Sicherung (idempotent).
     * 2. Sicherung per `VACUUM INTO` neben die Portfolio-Datei, siehe
     *    migrationBackupPath(). Schlägt sie fehl, wird NICHT umgestellt.
     * 3. Umstellung in einer Transaktion, bei Fehler vollständiger Rollback.
     *
     * Ein Fehlschlag lässt open() bewusst NICHT scheitern: das Portfolio
     * bleibt benutzbar wie vor diesem Bugfix, der Bericht meldet den Fehler,
     * und beim nächsten Öffnen wird es erneut versucht.
     *
     * @param path  Pfad der geöffneten Datenbank (":memory:" → keine Sicherung).
     */
    void migrateDepotNumbers(const QString& path);

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

    /**
     * @brief Die benannte Verbindung, geholt fuer die Dauer des Aufrufs.
     *
     * Bewusst Rueckgabe per Wert und kein Member (12.09.2026): eine als
     * Member gehaltene QSqlDatabase haelt Qts interne Verbindung bis zur
     * Zerstoerung dieses Singletons fest — und die findet nach dem Abbau der
     * QCoreApplication statt. Jeder QSqlDatabase-Zugriff zu diesem Zeitpunkt
     * schreibt "QSqlDatabase requires a QCoreApplication" ins Protokoll.
     * Siehe ARCHITECTURE.md, "QSqlDatabase-Warnung am Ende jedes Testlaufs".
     *
     * @return Die Verbindung k_connectionName; ungueltig, wenn keine offen ist.
     */
    QSqlDatabase connection() const;

    static constexpr const char* k_connectionName = "spm_main";

    /// Siehe depotNumberMigrationReport().
    DepotNumberMigrationReport m_depotNumberMigrationReport;
};

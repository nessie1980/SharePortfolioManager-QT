// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QString>
#include <QFile>
#include <QTextStream>
#include <QMap>

/**
 * @brief Simple file + console logger for the XML importer tool.
 *
 * Every log entry includes a timestamp, an entity type, a source
 * identifier (from the XML) and the action that was taken. A summary
 * table (counts per entity/action) is printed at the end of a run via
 * writeSummary().
 *
 * The log file is opened in append mode, so re-running the importer
 * against the same log path keeps the full history of all runs.
 */
class ImportLogger
{
public:
    enum class Action { Inserted, Skipped, Reused, Error, Info };

    explicit ImportLogger(const QString& logFilePath);
    ~ImportLogger();

    /// true if the log file could be opened (console output works regardless).
    bool isOpen() const { return m_file.isOpen(); }

    /**
     * @brief Log one entity-level event and update the summary counters.
     * @param entity    e.g. "Share", "Buy", "Sale", "Brokerage", "Dividend", "DailyValue"
     * @param sourceId  Identifying value from the XML (WKN, GUID, OrderNumber, ...)
     * @param action    What happened.
     * @param detail    Optional human-readable detail / reason.
     */
    void log(const QString& entity, const QString& sourceId,
             Action action, const QString& detail = QString());

    /** @brief Free-form info line, not counted in the summary. */
    void info(const QString& message);

    /** @brief Write the final summary table to both log file and console. */
    void writeSummary();

private:
    QFile       m_file;
    QTextStream m_stream;
    QMap<QString, QMap<Action, int>> m_counts; ///< entity -> action -> count

    static QString actionToString(Action action);
    void writeLine(const QString& line);
};

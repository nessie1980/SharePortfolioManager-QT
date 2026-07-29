// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "LogEntry.h"
#include "LoggerVersion.h"

#include <QObject>
#include <QList>
#include <QStringList>
#include <QColor>
#include <QFile>
#include <QTextStream>

namespace Logging {

/**
 * @brief Level-based logger with configurable states and component names.
 *
 * Level-based logger with configurable states and component names.
 *
 * States and component names are free QStringLists — define them at init time.
 * Bit-flag filtering: logLevelStates and logLevelComponents are int bitmasks.
 * The entryAdded() signal notifies the GUI whenever a new entry is logged.
 * File logging appends to a single file; CleanUpLogFiles() handles rotation.
 *
 * Usage:
 *   Logger logger;
 *   logger.loggerInitialize(
 *       logLevelStates, logLevelComponents,
 *       {"Start","Info","Warning","Error","Fatal"},
 *       {"Application","Parser","Database"},
 *       {Qt::black, Qt::black, QColor("OrangeRed"), Qt::red, QColor("DarkRed")},
 *       true, 50, "/logs/app.log", "Application started"
 *   );
 *   logger.addEntry("Something happened",
 *                   Logger::StateLevel::State2,   // Info
 *                   Logger::ComponentLevel::Component1);
 */
class Logger : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Returns the Logger library version string.
     *
     * Independent of the SharePortfolioManager app version — see
     * libs/logger/CMakeLists.txt (project(Logger VERSION ...)) and
     * ARCHITECTURE.md, Abschnitt "Versionierung".
     * @return Version string (e.g. "1.0.0").
     */
    static QString version() { return QStringLiteral(LOGGER_VERSION_STRING); }

    // ── State levels (bit flags, powers of 2) ───────────────────────────
    enum class StateLevel : int {
        State0  = 0,
        State1  = 1,
        State2  = 2,
        State3  = 4,
        State4  = 8,
        State5  = 16,
        State6  = 32,
        State7  = 64,
        State8  = 128,
        State9  = 256,
        State10 = 512,
        State11 = 1024,
        State12 = 2048,
        State13 = 4096,
        State14 = 8192,
        State15 = 16384,
        State16 = 32768
    };
    Q_ENUM(StateLevel)

    // ── Component levels (bit flags, powers of 2) ───────────────────────
    enum class ComponentLevel : int {
        Component0  = 0,
        Component1  = 1,
        Component2  = 2,
        Component3  = 4,
        Component4  = 8,
        Component5  = 16,
        Component6  = 32,
        Component7  = 64,
        Component8  = 128,
        Component9  = 256,
        Component10 = 512,
        Component11 = 1024,
        Component12 = 2048,
        Component13 = 4096,
        Component14 = 8192,
        Component15 = 16384,
        Component16 = 32768
    };
    Q_ENUM(ComponentLevel)

    // ── Initialization result codes ─────────────────────────────────────
    enum class InitState : int {
        InitializationFailed    = -9,
        LogPathCreationFailed   = -8,
        WriteStartupFailed      = -7,
        WrongSize               = -6,
        ColorsMaxCount          = -5,
        ComponentNamesMaxCount  = -4,
        ComponentLevelInvalid   = -3,
        StateLevelInvalid       = -2,
        StatesMaxCount          = -1,
        NotInitialized          =  0,
        Initialized             =  1
    };
    Q_ENUM(InitState)

    // ── Logger state result codes ────────────────────────────────────────
    enum class LoggerState : int {
        CleanUpLogFilesFailed       = -5,
        NewEntryAddFailed           = -4,
        ComponentNameIndexInvalid   = -3,
        StateIndexInvalid           = -2,
        NotInitialized              = -1,
        Initialized                 =  0,
        LoggingDisabled             =  1,
        NewEntryAddSuccessful       =  2,
        CleanUpLogFilesSuccessful   =  3
    };
    Q_ENUM(LoggerState)

    static constexpr int MaxLogLevelsState      = 16;
    static constexpr int MaxLogLevelsComponents = 16;
    static constexpr int DefaultMaxSize         = 50;

    // ── Construction ──────────────────────────────────────────────────────
    explicit Logger(QObject* parent = nullptr);

    /**
     * @brief Initialize the logger.
     *
     * @param logLevelStates        Bitmask of StateLevel values to log
     * @param logLevelComponents    Bitmask of ComponentLevel values to log
     * @param stateList             State name strings, e.g. {"Start","Info","Warning","Error","Fatal"}
     * @param componentNameList     Component name strings, e.g. {"Application","Parser"}
     * @param colorList             One QColor per state (index matches stateList)
     * @param fileLogging           Enable writing to file
     * @param size                  Max entries kept in memory for GUI
     * @param loggingPathAndFileName Full path + filename for log file
     * @param startupMessage        First entry written on init
     * @param appendToLogFile       Append to existing file (true) or overwrite (false)
     */
    InitState loggerInitialize(
        int                logLevelStates       = 0,
        int                logLevelComponents   = 0,
        const QStringList& stateList            = {},
        const QStringList& componentNameList    = {},
        const QList<QColor>& colorList          = {},
        bool               fileLogging          = false,
        int                size                 = 0,
        const QString&     loggingPathAndFileName = QString(),
        const QString&     startupMessage        = QString(),
        bool               appendToLogFile       = false
    );

    /**
     * @brief Add a new log entry.
     *
     * @param logMessage        The log message text
     * @param logStateId        StateLevel bitmask value (e.g. StateLevel::State2)
     * @param logComponentNameId ComponentLevel bitmask value
     */
    LoggerState addEntry(
        const QString& logMessage,
        StateLevel     logStateId          = StateLevel::State0,
        ComponentLevel logComponentNameId  = ComponentLevel::Component0
    );

    /**
     * @brief Returns the display color for a given StateLevel.
     */
    QColor getColorOfStateLevel(StateLevel stateLevel) const;

    /**
     * @brief Clean up old log files, keep iStoredLogFiles most recent.
     * @return Number of deleted files, or -1 on error.
     */
    int cleanUpLogFiles(int iStoredLogFiles);

    // ── State accessors ───────────────────────────────────────────────────
    InitState   initState()   const { return m_initState; }
    LoggerState loggerState() const { return m_loggerState; }
    int         loggerSize()  const { return m_logSize; }

    QStringList      statesList()         const { return m_logStates; }
    QStringList      componentNamesList() const { return m_logComponentNames; }
    QList<QColor>    colorList()          const { return m_logColors; }
    QList<LogEntry>  entryList()          const { return m_logEntryList; }

    QString loggerPath()            const { return m_loggingPath; }
    QString loggerPathAndFileName() const { return m_loggingPathAndFileName; }

signals:
    /// Emitted whenever a new entry passes the level filter.
    /// Connect to GUI log-list widget for automatic updates.
    void entryAdded(const Logging::LogEntry& entry);

    /// Emitted after successful cleanup.
    void cleanUpFinished(int filesDeleted);

private:
    bool writeLogEntry(QTextStream& stream, const LogEntry& entry);
    void setLoggerPathAndFileName(const QString& pathAndFileName);

    InitState   m_initState   = InitState::NotInitialized;
    LoggerState m_loggerState = LoggerState::NotInitialized;

    int  m_logLevelStates    = 0;
    int  m_logLevelComponent = 0;

    QStringList   m_logStates;
    QStringList   m_logComponentNames;
    QList<QColor> m_logColors;

    int  m_logSize              = -1;
    bool m_loggingToFileEnabled = false;
    bool m_loggingAppendToFile  = false;

    QString m_loggingPathAndFileName;
    QString m_loggingPath;

    QList<LogEntry> m_logEntryList;
};

} // namespace Logging

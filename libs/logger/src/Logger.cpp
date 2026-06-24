// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "Logger.h"

#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QtMath>
#include <QCoreApplication>
#include <QDebug>

namespace Logging {

// ── Construction ──────────────────────────────────────────────────────────────
Logger::Logger(QObject* parent)
    : QObject(parent)
{}

// ── loggerInitialize ──────────────────────────────────────────────────────────
Logger::InitState Logger::loggerInitialize(
    int                logLevelStates,
    int                logLevelComponents,
    const QStringList& stateList,
    const QStringList& componentNameList,
    const QList<QColor>& colorList,
    bool               fileLogging,
    int                size,
    const QString&     loggingPathAndFileName,
    const QString&     startupMessage,
    bool               appendToLogFile)
{
    m_logLevelStates    = logLevelStates;
    m_logLevelComponent = logLevelComponents;
    m_logStates         = stateList;
    m_logComponentNames = componentNameList;
    m_logColors         = colorList;
    m_logSize           = (size > 0) ? size : DefaultMaxSize;
    m_loggingToFileEnabled = fileLogging;
    m_loggingAppendToFile  = appendToLogFile;
    setLoggerPathAndFileName(loggingPathAndFileName);

    // ── Validate states count ─────────────────────────────────────────────
    if (!m_logStates.isEmpty() && m_logStates.count() > MaxLogLevelsState) {
        m_initState   = InitState::StatesMaxCount;
        m_loggerState = LoggerState::NotInitialized;
        return m_initState;
    }

    // ── Validate state log level ──────────────────────────────────────────
    if (!m_logStates.isEmpty() &&
        m_logLevelStates >= static_cast<int>(qPow(2, m_logStates.count()))) {
        m_initState   = InitState::StateLevelInvalid;
        m_loggerState = LoggerState::NotInitialized;
        return m_initState;
    }

    // ── Validate component count ──────────────────────────────────────────
    if (!m_logComponentNames.isEmpty() && m_logComponentNames.count() > MaxLogLevelsComponents) {
        m_initState   = InitState::ComponentNamesMaxCount;
        m_loggerState = LoggerState::NotInitialized;
        return m_initState;
    }

    // ── Validate component log level ──────────────────────────────────────
    if (!m_logComponentNames.isEmpty() &&
        m_logLevelComponent >= static_cast<int>(qPow(2, m_logComponentNames.count()))) {
        m_initState   = InitState::ComponentLevelInvalid;
        m_loggerState = LoggerState::NotInitialized;
        return m_initState;
    }

    // ── Logging disabled (both levels == 0) ───────────────────────────────
    if (m_logLevelStates == 0 && m_logLevelComponent == 0) {
        m_initState   = InitState::Initialized;
        m_loggerState = LoggerState::LoggingDisabled;
        return m_initState;
    }

    // ── Size must be > 0 ─────────────────────────────────────────────────
    if (m_logSize <= 0) {
        m_initState   = InitState::WrongSize;
        m_loggerState = LoggerState::NotInitialized;
        return m_initState;
    }

    // ── Build startup entry ───────────────────────────────────────────────
    m_logEntryList.clear();
    const QString msg = (startupMessage.isEmpty()) ? QStringLiteral("-") : startupMessage;
    m_logEntryList.append(LogEntry(0, QDateTime::currentDateTime(),
                                   QStringLiteral("Start"),
                                   QStringLiteral("Logger"),
                                   Qt::black, msg));

    // ── File logging ──────────────────────────────────────────────────────
    if (m_loggingToFileEnabled) {
        if (!QDir(m_loggingPath).exists()) {
            if (!QDir().mkpath(m_loggingPath)) {
                m_initState   = InitState::LogPathCreationFailed;
                m_loggerState = LoggerState::NotInitialized;
                return m_initState;
            }
        }

        QFile file(m_loggingPathAndFileName);
        const QIODeviceBase::OpenMode mode =
            QIODevice::Text | QIODevice::WriteOnly |
            (m_loggingAppendToFile ? QIODevice::Append : QIODevice::Truncate);

        if (!file.open(mode)) {
            m_initState   = InitState::WriteStartupFailed;
            m_loggerState = LoggerState::NotInitialized;
            return m_initState;
        }

        QTextStream stream(&file);
        stream.setEncoding(QStringConverter::Utf8);
        for (const LogEntry& entry : std::as_const(m_logEntryList)) {
            if (!writeLogEntry(stream, entry)) {
                m_initState   = InitState::WriteStartupFailed;
                m_loggerState = LoggerState::NotInitialized;
                return m_initState;
            }
        }
    }

    m_initState   = InitState::Initialized;
    m_loggerState = LoggerState::Initialized;
    return m_initState;
}

// ── addEntry ──────────────────────────────────────────────────────────────────
Logger::LoggerState Logger::addEntry(
    const QString& logMessage,
    StateLevel     logStateId,
    ComponentLevel logComponentNameId)
{
    if (m_initState != InitState::Initialized) {
        m_loggerState = LoggerState::NotInitialized;
        return m_loggerState;
    }
    if (m_loggerState == LoggerState::LoggingDisabled)
        return m_loggerState;

    m_loggerState = LoggerState::Initialized;

    // ── Compute list indices from bitmask values ──────────────────────────
    // index = (int)(log2(value)) + 1
    int indexStateList = -1;
    if (static_cast<int>(logStateId) > 0)
        indexStateList = static_cast<int>(
            qLn(static_cast<double>(logStateId)) / qLn(2.0)) + 1;

    int indexComponentList = -1;
    if (static_cast<int>(logComponentNameId) > 0)
        indexComponentList = static_cast<int>(
            qLn(static_cast<double>(logComponentNameId)) / qLn(2.0)) + 1;

    // ── Resolve state name ────────────────────────────────────────────────
    QString stateName = QStringLiteral("-");
    if (!m_logStates.isEmpty() && indexStateList > 0) {
        if (m_logStates.count() >= indexStateList)
            stateName = m_logStates.at(indexStateList - 1);
        else {
            m_loggerState = LoggerState::StateIndexInvalid;
            return m_loggerState;
        }
    }

    // ── Resolve component name ────────────────────────────────────────────
    QString componentName = QStringLiteral("-");
    if (!m_logComponentNames.isEmpty() && indexComponentList > 0) {
        if (m_logComponentNames.count() >= indexComponentList)
            componentName = m_logComponentNames.at(indexComponentList - 1);
        else {
            m_loggerState = LoggerState::ComponentNameIndexInvalid;
            return m_loggerState;
        }
    }

    // ── Resolve color ─────────────────────────────────────────────────────
    QColor color = Qt::black;
    if (!m_logColors.isEmpty() && indexStateList > 0 &&
        m_logColors.count() >= indexStateList)
        color = m_logColors.at(indexStateList - 1);

    // ── Check bitmask filters ─────────────────────────────────────────────
    if ((m_logLevelStates    & static_cast<int>(logStateId))         != static_cast<int>(logStateId))
        return m_loggerState; // filtered out — not an error
    if ((m_logLevelComponent & static_cast<int>(logComponentNameId)) != static_cast<int>(logComponentNameId))
        return m_loggerState; // filtered out

    // ── Build entry ───────────────────────────────────────────────────────
    int nextId = m_logEntryList.isEmpty() ? 0 :
                 m_logEntryList.last().id() + 1;

    // Ring buffer: remove oldest if full
    if (m_logEntryList.count() >= m_logSize)
        m_logEntryList.removeFirst();

    LogEntry entry(nextId, QDateTime::currentDateTime(),
                   stateName, componentName, color, logMessage);
    m_logEntryList.append(entry);

    // ── Write to file ─────────────────────────────────────────────────────
    if (m_loggingToFileEnabled) {
        QFile file(m_loggingPathAndFileName);
        if (!file.open(QIODevice::Text | QIODevice::WriteOnly | QIODevice::Append)) {
            m_loggerState = LoggerState::NewEntryAddFailed;
            return m_loggerState;
        }
        QTextStream stream(&file);
        stream.setEncoding(QStringConverter::Utf8);
        if (!writeLogEntry(stream, m_logEntryList.last())) {
            m_loggerState = LoggerState::NewEntryAddFailed;
            return m_loggerState;
        }
    }

    m_loggerState = LoggerState::NewEntryAddSuccessful;
    emit entryAdded(m_logEntryList.last());
    return m_loggerState;
}

// ── getColorOfStateLevel ──────────────────────────────────────────────────────
QColor Logger::getColorOfStateLevel(StateLevel stateLevel) const
{
    int indexStateList = -1;
    if (static_cast<int>(stateLevel) > 0)
        indexStateList = static_cast<int>(
            qLn(static_cast<double>(stateLevel)) / qLn(2.0)) + 1;

    if (!m_logColors.isEmpty() && indexStateList > 0 &&
        m_logColors.count() >= indexStateList)
        return m_logColors.at(indexStateList - 1);

    return Qt::black;
}

// ── cleanUpLogFiles ───────────────────────────────────────────────────────────
int Logger::cleanUpLogFiles(int iStoredLogFiles)
{
    if (m_initState != InitState::Initialized) {
        m_loggerState = LoggerState::NotInitialized;
        return -1;
    }

    QDir dir(m_loggingPath);
    if (!dir.exists()) {
        m_loggerState = LoggerState::CleanUpLogFilesFailed;
        return -1;
    }

    // Sort descending (newest first)
    dir.setSorting(QDir::Name | QDir::Reversed);
    QStringList files = dir.entryList(QDir::Files);

    int deletedCount = 0;
    for (int i = iStoredLogFiles; i < files.count(); ++i) {
        if (dir.remove(files.at(i)))
            ++deletedCount;
    }

    m_loggerState = LoggerState::CleanUpLogFilesSuccessful;
    emit cleanUpFinished(deletedCount);
    return deletedCount;
}

// ── Private helpers ───────────────────────────────────────────────────────────

/// Log entry format:
/// "{0:0000}\t{1} {2,-15} {3,-20} {4}"
bool Logger::writeLogEntry(QTextStream& stream, const LogEntry& entry)
{
    stream << QString("%1\t%2 %3 %4 %5\n")
              .arg(entry.id(), 4, 10, QChar('0'))
              .arg(entry.timeStamp().toString("dd.MM.yyyy hh:mm:ss"))
              .arg(entry.state(), -15)
              .arg(entry.componentName(), -20)
              .arg(entry.message());
    return stream.status() == QTextStream::Ok;
}

void Logger::setLoggerPathAndFileName(const QString& pathAndFileName)
{
    if (pathAndFileName.isEmpty()) {
        // Default: executable directory + "Log.txt"
        m_loggingPathAndFileName =
            QCoreApplication::applicationDirPath() + "/Log.txt";
    } else {
        m_loggingPathAndFileName = pathAndFileName;
    }
    m_loggingPath = QFileInfo(m_loggingPathAndFileName).absolutePath();
}

} // namespace Logging

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QDateTime>
#include <QColor>
#include <QString>

namespace Logging {

/**
 * @brief Represents a single log entry.
 *
 * Each entry contains an ID, timestamp, state name, component name,
 * display color and the log message text.
 *
 * Fields:
 *   - id            Unique sequential entry ID
 *   - timeStamp     Date and time the entry was created
 *   - state         State name string (e.g. "Info", "Warning")
 *   - componentName Component name string (e.g. "Application", "Parser")
 *   - color         Display color associated with the state
 *   - message       The log message text
 */
class LogEntry
{
public:
    LogEntry() = default;

    /**
     * @brief Full constructor.
     * @param id             Unique sequential entry ID.
     * @param timeStamp      Date and time the entry was created.
     * @param state          State name string (e.g. "Info", "Warning").
     * @param componentName  Component name string (e.g. "Application", "Parser").
     * @param color          Display color associated with the state.
     * @param message        The log message text.
     */
    LogEntry(int id, const QDateTime& timeStamp, const QString& state,
             const QString& componentName, const QColor& color, const QString& message)
        : m_id(id)
        , m_timeStamp(timeStamp)
        , m_state(state)
        , m_componentName(componentName)
        , m_color(color)
        , m_message(message)
    {}

    /**
     * @brief Short constructor — timestamp defaults to now, componentName to "-".
     * @param id       Unique sequential entry ID.
     * @param state    State name string (e.g. "Info", "Warning").
     * @param color    Display color associated with the state.
     * @param message  The log message text.
     */
    LogEntry(int id, const QString& state, const QColor& color, const QString& message)
        : m_id(id)
        , m_timeStamp(QDateTime::currentDateTime())
        , m_state(state)
        , m_componentName(QStringLiteral("-"))
        , m_color(color)
        , m_message(message)
    {}

    int             id()            const { return m_id; }            ///< Unique sequential entry ID
    QDateTime       timeStamp()     const { return m_timeStamp; }     ///< Date and time of the entry
    QString         state()         const { return m_state; }         ///< State name string
    QString         componentName() const { return m_componentName; } ///< Component name string
    QColor          color()         const { return m_color; }         ///< Display color for this state
    QString         message()       const { return m_message; }       ///< Log message text

    /**
     * @brief Set the entry ID.
     * @param value  New ID value.
     */
    void setId(int value)                       { m_id = value; }
    /**
     * @brief Set the timestamp.
     * @param value  New timestamp.
     */
    void setTimeStamp(const QDateTime& value)   { m_timeStamp = value; }
    /**
     * @brief Set the state name.
     * @param value  New state name string.
     */
    void setState(const QString& value)         { m_state = value; }
    /**
     * @brief Set the component name.
     * @param value  New component name string.
     */
    void setComponentName(const QString& value) { m_componentName = value; }
    /**
     * @brief Set the display color.
     * @param value  New color.
     */
    void setColor(const QColor& value)          { m_color = value; }
    /**
     * @brief Set the log message text.
     * @param value  New message string.
     */
    void setMessage(const QString& value)       { m_message = value; }

private:
    int       m_id            = 0;
    QDateTime m_timeStamp;
    QString   m_state;
    QString   m_componentName;
    QColor    m_color         = Qt::black;
    QString   m_message;
};

} // namespace Logging

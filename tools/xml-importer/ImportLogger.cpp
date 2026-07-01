// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ImportLogger.h"

#include <QDateTime>
#include <QStringList>
#include <iostream>

ImportLogger::ImportLogger(const QString& logFilePath)
    : m_file(logFilePath)
{
    if (m_file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append))
        m_stream.setDevice(&m_file);
}

ImportLogger::~ImportLogger()
{
    if (m_file.isOpen())
        m_file.close();
}

QString ImportLogger::actionToString(Action action)
{
    switch (action) {
    case Action::Inserted: return QStringLiteral("INSERTED");
    case Action::Skipped:  return QStringLiteral("SKIPPED");
    case Action::Reused:   return QStringLiteral("REUSED");
    case Action::Error:    return QStringLiteral("ERROR");
    case Action::Info:     return QStringLiteral("INFO");
    }
    return QStringLiteral("UNKNOWN");
}

void ImportLogger::writeLine(const QString& line)
{
    std::cout << line.toStdString() << std::endl;
    if (m_stream.device())
        m_stream << line << '\n';
}

void ImportLogger::log(const QString& entity, const QString& sourceId,
                       Action action, const QString& detail)
{
    const QString ts = QDateTime::currentDateTime().toString(Qt::ISODate);
    QString line = QStringLiteral("[%1] %2 %3 \"%4\"")
                       .arg(ts, actionToString(action), entity, sourceId);
    if (!detail.isEmpty())
        line += QStringLiteral(" — %1").arg(detail);

    writeLine(line);
    m_counts[entity][action]++;
}

void ImportLogger::info(const QString& message)
{
    const QString ts = QDateTime::currentDateTime().toString(Qt::ISODate);
    writeLine(QStringLiteral("[%1] INFO %2").arg(ts, message));
}

void ImportLogger::writeSummary()
{
    writeLine(QStringLiteral("──────────────────────────────────────────────"));
    writeLine(QStringLiteral("Zusammenfassung:"));
    for (auto entityIt = m_counts.constBegin(); entityIt != m_counts.constEnd(); ++entityIt) {
        QStringList parts;
        for (auto actionIt = entityIt.value().constBegin();
             actionIt != entityIt.value().constEnd(); ++actionIt) {
            parts << QStringLiteral("%1=%2")
                         .arg(actionToString(actionIt.key())).arg(actionIt.value());
        }
        writeLine(QStringLiteral("  %1: %2").arg(entityIt.key(), parts.join(QStringLiteral(", "))));
    }
    writeLine(QStringLiteral("──────────────────────────────────────────────"));
}

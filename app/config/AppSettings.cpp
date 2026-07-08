// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "AppSettings.h"

#include <QSettings>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>

// ── Singleton ─────────────────────────────────────────────────────────────────

AppSettings& AppSettings::instance()
{
    static AppSettings s_instance;
    return s_instance;
}

AppSettings::AppSettings(QObject* parent)
    : QObject(parent)
{}

// ── Load / Save ───────────────────────────────────────────────────────────────

void AppSettings::load(const QString& path)
{
    // Default to settings.ini next to the executable if no path is given.
    m_settingsPath = path.isEmpty()
        ? QCoreApplication::applicationDirPath() + QStringLiteral("/settings.ini")
        : path;

    QSettings settings(m_settingsPath, QSettings::IniFormat);

    // Each settings.value() call passes the current member as default,
    // so missing keys in the INI file leave the in-memory defaults intact.
    // ── General ──────────────────────────────────────────────────────────
    m_portfolioPath        = settings.value(QStringLiteral("General/PortfolioPath"),      m_portfolioPath).toString();
    m_language             = settings.value(QStringLiteral("General/Language"),           m_language).toString();
    m_startNextShareUpdate = settings.value(QStringLiteral("General/StartNextShareUpdate"), m_startNextShareUpdate).toInt();
    m_statusMessageClear   = settings.value(QStringLiteral("General/StatusMessageClear"), m_statusMessageClear).toInt();
    m_showExceptions       = settings.value(QStringLiteral("General/ShowExceptions"),     m_showExceptions).toBool();

    // ── Window ───────────────────────────────────────────────────────────
    m_windowPos   = settings.value(QStringLiteral("Window/Pos"),   m_windowPos).toPoint();
    m_windowSize  = settings.value(QStringLiteral("Window/Size"),  m_windowSize).toSize();
    m_windowState = settings.value(QStringLiteral("Window/State"), m_windowState).toString();

    // ── Logger ───────────────────────────────────────────────────────────
    m_logGuiEntries       = settings.value(QStringLiteral("Logger/GuiEntries"),       m_logGuiEntries).toInt();
    m_logToFile           = settings.value(QStringLiteral("Logger/LogToFile"),        m_logToFile).toBool();
    m_logStoredFiles      = settings.value(QStringLiteral("Logger/StoredFiles"),      m_logStoredFiles).toInt();
    m_logCleanupAtStartup = settings.value(QStringLiteral("Logger/CleanupAtStartup"), m_logCleanupAtStartup).toBool();
    m_logComponents       = settings.value(QStringLiteral("Logger/Components"),       m_logComponents).toInt();
    m_logLevels           = settings.value(QStringLiteral("Logger/Levels"),           m_logLevels).toInt();

    // Colors are stored as a QSettings array of "#RRGGBB" strings, indexed by
    // state level position — matching Logger::stateList order.
    // Only replace the defaults if the INI actually contains color entries.
    const int colorCount = settings.beginReadArray(QStringLiteral("Logger/Colors"));
    if (colorCount > 0) {
        m_logColors.clear();
        for (int colorIndex = 0; colorIndex < colorCount; ++colorIndex) {
            settings.setArrayIndex(colorIndex);
            m_logColors.append(QColor(settings.value(QStringLiteral("color")).toString()));
        }
    }
    settings.endArray();

    // ── Sounds ───────────────────────────────────────────────────────────
    m_soundUpdateEnabled = settings.value(QStringLiteral("Sounds/UpdateEnabled"), m_soundUpdateEnabled).toBool();
    m_soundErrorEnabled  = settings.value(QStringLiteral("Sounds/ErrorEnabled"),  m_soundErrorEnabled).toBool();
    m_soundUpdateFile    = settings.value(QStringLiteral("Sounds/UpdateFile"),     m_soundUpdateFile).toString();
    m_soundErrorFile     = settings.value(QStringLiteral("Sounds/ErrorFile"),      m_soundErrorFile).toString();

    // ── API Keys ─────────────────────────────────────────────────────────
    m_apiKeyYahoo   = settings.value(QStringLiteral("API/KeyYahoo"),   m_apiKeyYahoo).toString();
    m_apiKeyOnVista = settings.value(QStringLiteral("API/KeyOnVista"), m_apiKeyOnVista).toString();

    // ── Backup ───────────────────────────────────────────────────────────
    m_backupEnabled    = settings.value(QStringLiteral("Backup/Enabled"),    m_backupEnabled).toBool();
    m_backupMaxCount   = settings.value(QStringLiteral("Backup/MaxCount"),   m_backupMaxCount).toInt();
    m_backupNamePrefix = settings.value(QStringLiteral("Backup/NamePrefix"), m_backupNamePrefix).toString();
    m_backupDateFormat = settings.value(QStringLiteral("Backup/DateFormat"), m_backupDateFormat).toString();
    m_backupDirectory  = settings.value(QStringLiteral("Backup/Directory"),  m_backupDirectory).toString();
}

void AppSettings::save()
{
    QSettings settings(m_settingsPath, QSettings::IniFormat);

    // ── General ──────────────────────────────────────────────────────────
    settings.setValue(QStringLiteral("General/PortfolioPath"),       m_portfolioPath);
    settings.setValue(QStringLiteral("General/Language"),            m_language);
    settings.setValue(QStringLiteral("General/StartNextShareUpdate"), m_startNextShareUpdate);
    settings.setValue(QStringLiteral("General/StatusMessageClear"),  m_statusMessageClear);
    settings.setValue(QStringLiteral("General/ShowExceptions"),      m_showExceptions);

    // ── Window ───────────────────────────────────────────────────────────
    settings.setValue(QStringLiteral("Window/Pos"),   m_windowPos);
    settings.setValue(QStringLiteral("Window/Size"),  m_windowSize);
    settings.setValue(QStringLiteral("Window/State"), m_windowState);

    // ── Logger ───────────────────────────────────────────────────────────
    settings.setValue(QStringLiteral("Logger/GuiEntries"),       m_logGuiEntries);
    settings.setValue(QStringLiteral("Logger/LogToFile"),        m_logToFile);
    settings.setValue(QStringLiteral("Logger/StoredFiles"),      m_logStoredFiles);
    settings.setValue(QStringLiteral("Logger/CleanupAtStartup"), m_logCleanupAtStartup);
    settings.setValue(QStringLiteral("Logger/Components"),       m_logComponents);
    settings.setValue(QStringLiteral("Logger/Levels"),           m_logLevels);

    settings.beginWriteArray(QStringLiteral("Logger/Colors"), m_logColors.size());
    for (int colorIndex = 0; colorIndex < m_logColors.size(); ++colorIndex) {
        settings.setArrayIndex(colorIndex);
        settings.setValue(QStringLiteral("color"), m_logColors.at(colorIndex).name());
    }
    settings.endArray();

    // ── Sounds ───────────────────────────────────────────────────────────
    settings.setValue(QStringLiteral("Sounds/UpdateEnabled"), m_soundUpdateEnabled);
    settings.setValue(QStringLiteral("Sounds/ErrorEnabled"),  m_soundErrorEnabled);
    settings.setValue(QStringLiteral("Sounds/UpdateFile"),    m_soundUpdateFile);
    settings.setValue(QStringLiteral("Sounds/ErrorFile"),     m_soundErrorFile);

    // ── API Keys ─────────────────────────────────────────────────────────
    settings.setValue(QStringLiteral("API/KeyYahoo"),   m_apiKeyYahoo);
    settings.setValue(QStringLiteral("API/KeyOnVista"), m_apiKeyOnVista);

    // ── Backup ───────────────────────────────────────────────────────────
    settings.setValue(QStringLiteral("Backup/Enabled"),    m_backupEnabled);
    settings.setValue(QStringLiteral("Backup/MaxCount"),   m_backupMaxCount);
    settings.setValue(QStringLiteral("Backup/NamePrefix"), m_backupNamePrefix);
    settings.setValue(QStringLiteral("Backup/DateFormat"), m_backupDateFormat);
    settings.setValue(QStringLiteral("Backup/Directory"),  m_backupDirectory);
}

// ── Setters ───────────────────────────────────────────────────────────────────
// Each setter persists immediately so the INI is always in sync,
// even if the application exits without an explicit save() call.

void AppSettings::setPortfolioPath(const QString& value)
{
    m_portfolioPath = value;
    save();
}

void AppSettings::setLanguage(const QString& value)
{
    m_language = value;
    save();
}

void AppSettings::setWindowGeometry(const QPoint& pos, const QSize& size, const QString& state)
{
    m_windowPos   = pos;
    m_windowSize  = size;
    m_windowState = state;
    save();
}

void AppSettings::setApiKeyYahoo(const QString& key)
{
    m_apiKeyYahoo = key;
    save();
}

void AppSettings::setApiKeyOnVista(const QString& key)
{
    m_apiKeyOnVista = key;
    save();
}

// ── Helpers ───────────────────────────────────────────────────────────────────

QColor AppSettings::logColorAt(int stateIndex) const
{
    // Return black as safe fallback for any out-of-range index.
    if (stateIndex < 0 || stateIndex >= m_logColors.size())
        return Qt::black;
    return m_logColors.at(stateIndex);
}

void AppSettings::setLogGuiEntries(int value)
{
    m_logGuiEntries = value;
    save();
}

void AppSettings::setLogToFile(bool value)
{
    m_logToFile = value;
    save();
}

void AppSettings::setLogStoredFiles(int value)
{
    m_logStoredFiles = value;
    save();
}

void AppSettings::setLogCleanupAtStartup(bool value)
{
    m_logCleanupAtStartup = value;
    save();
}

void AppSettings::setLogComponents(int value)
{
    m_logComponents = value;
    save();
}

void AppSettings::setLogLevels(int value)
{
    m_logLevels = value;
    save();
}

void AppSettings::setLogColors(const QList<QColor>& colors)
{
    m_logColors = colors;
    save();
}

void AppSettings::setSoundUpdateEnabled(bool value)
{
    m_soundUpdateEnabled = value;
    save();
}

void AppSettings::setSoundErrorEnabled(bool value)
{
    m_soundErrorEnabled = value;
    save();
}

void AppSettings::setSoundUpdateFile(const QString& filename)
{
    m_soundUpdateFile = filename;
    save();
}

void AppSettings::setSoundErrorFile(const QString& filename)
{
    m_soundErrorFile = filename;
    save();
}

void AppSettings::setBackupEnabled(bool value)
{
    m_backupEnabled = value;
    save();
}

void AppSettings::setBackupMaxCount(int value)
{
    m_backupMaxCount = value;
    save();
}

void AppSettings::setBackupNamePrefix(const QString& value)
{
    m_backupNamePrefix = value;
    save();
}

void AppSettings::setBackupDateFormat(const QString& value)
{
    m_backupDateFormat = value;
    save();
}

void AppSettings::setBackupDirectory(const QString& value)
{
    m_backupDirectory = value;
    save();
}

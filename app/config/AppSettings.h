// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QObject>
#include <QColor>
#include <QString>
#include <QSize>
#include <QPoint>
#include <QList>

/**
 * @brief Application settings — replaces Settings.xml.
 *
 * Stored as settings.ini (QSettings IniFormat) in the user's standard
 * application-config directory (see AppStartup::settingsPath() /
 * QStandardPaths::AppConfigLocation — bugfix 29.07.2026, see
 * ARCHITECTURE.md, "settings.ini nicht persistent im AppImage"). The
 * production code path (main.cpp → AppStartup::loadSettings()) always
 * resolves this path itself and passes it explicitly to load(); the
 * fallback below (path.isEmpty()) uses the same standard location so that
 * calling load() directly — e.g. from ad-hoc tooling — behaves consistently.
 * Log colors are stored as a QList<QColor> indexed by state level,
 * matching Logger's stateList order — no Logger dependency needed here.
 */
class AppSettings : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Returns the singleton instance.
     * @return Reference to the single AppSettings instance.
     */
    static AppSettings& instance();

    /**
     * @brief Returns the full path to the settings.ini file currently in use.
     * @return Absolute path to the settings file.
     */
    QString settingsPath() const { return m_settingsPath; }

    /**
     * @brief Load settings from the INI file.
     * @param path  Full path to the settings file.
     *              If empty, defaults to the user's standard app-config
     *              directory (see class documentation above).
     */
    void load(const QString& path = {});

    /**
     * @brief Save all current settings back to the INI file.
     */
    void save();

    // ── General ──────────────────────────────────────────────────────────
    QString portfolioPath()       const { return m_portfolioPath; }
    QString language()            const { return m_language; }
    int     startNextShareUpdate()const { return m_startNextShareUpdate; }
    int     statusMessageClear()  const { return m_statusMessageClear; }
    bool    showExceptions()      const { return m_showExceptions; }

    /**
     * @brief Set the portfolio database file path and save.
     * @param value  Full path to the portfolio SQLite file.
     */
    void setPortfolioPath(const QString& value);

    /**
     * @brief Set the UI language code and save.
     * @param value  Language code (e.g. "de", "en").
     */
    void setLanguage(const QString& value);

    // ── Window ───────────────────────────────────────────────────────────
    QPoint  windowPos()   const { return m_windowPos; }
    QSize   windowSize()  const { return m_windowSize; }
    QString windowState() const { return m_windowState; }
    /**
     * @brief Update and save the main window geometry.
     * @param pos    Window position (top-left corner).
     * @param size   Window size.
     * @param state  Window state string (e.g. "Normal", "Maximized").
     */
    void setWindowGeometry(const QPoint& pos, const QSize& size,
                              const QString& state);

    // ── Logger ───────────────────────────────────────────────────────────
    int  logGuiEntries()       const { return m_logGuiEntries; }
    bool logToFile()           const { return m_logToFile; }
    int  logStoredFiles()      const { return m_logStoredFiles; }
    bool logCleanupAtStartup() const { return m_logCleanupAtStartup; }
    int  logComponents()       const { return m_logComponents; }
    int  logLevels()           const { return m_logLevels; }

    /// Colors indexed by state level position (matches Logger stateList order)
    QList<QColor> logColors()  const { return m_logColors; }
    /**
     * @brief Returns the display color for a given state index.
     * @param stateIndex  Zero-based index matching the Logger stateList order.
     * @return The configured QColor, or Qt::black if the index is out of range.
     */
    QColor logColorAt(int stateIndex) const;

    // ── Sounds ───────────────────────────────────────────────────────────
    bool    soundUpdateEnabled() const { return m_soundUpdateEnabled; }  ///< Update sound enabled
    bool    soundErrorEnabled()  const { return m_soundErrorEnabled; }   ///< Error sound enabled
    QString soundUpdateFile()    const { return m_soundUpdateFile; }     ///< Update sound filename
    QString soundErrorFile()     const { return m_soundErrorFile; }      ///< Error sound filename

    /**
     * @brief Enable or disable the update sound and save.
     * @param value  true to enable.
     */
    void setSoundUpdateEnabled(bool value);

    /**
     * @brief Enable or disable the error sound and save.
     * @param value  true to enable.
     */
    void setSoundErrorEnabled(bool value);

    /**
     * @brief Set the update sound filename and save.
     * @param filename  WAV filename (e.g. "UpdateFinished.wav").
     */
    void setSoundUpdateFile(const QString& filename);

    /**
     * @brief Set the error sound filename and save.
     * @param filename  WAV filename (e.g. "Error.wav").
     */
    void setSoundErrorFile(const QString& filename);

    // ── API Keys ─────────────────────────────────────────────────────────
    QString apiKeyYahoo()   const { return m_apiKeyYahoo; }
    QString apiKeyOnVista() const { return m_apiKeyOnVista; }
    /**
     * @brief Set the Yahoo Finance API key and save.
     * @param key  Yahoo Finance API key string.
     */
    void    setApiKeyYahoo(const QString& key);

    /**
     * @brief Set the OnVista API key and save.
     * @param key  OnVista API key string.
     */
    void    setApiKeyOnVista(const QString& key);

    // ── Logger setters ────────────────────────────────────────────────────
    /**
     * @brief Set the maximum number of log entries shown in the GUI and save.
     * @param value  Number of entries.
     */
    void setLogGuiEntries(int value);

    /**
     * @brief Enable or disable file logging and save.
     * @param value  true to enable file logging.
     */
    void setLogToFile(bool value);

    /**
     * @brief Set the number of stored log files and save.
     * @param value  Number of files to keep.
     */
    void setLogStoredFiles(int value);

    /**
     * @brief Enable or disable log file cleanup at startup and save.
     * @param value  true to clean up log files on startup.
     */
    void setLogCleanupAtStartup(bool value);

    /**
     * @brief Set the active log components bitmask and save.
     * @param value  Bitmask of active components.
     */
    void setLogComponents(int value);

    /**
     * @brief Set the active log levels bitmask and save.
     * @param value  Bitmask of active levels.
     */
    void setLogLevels(int value);

    /**
     * @brief Set the full list of log level colors and save.
     * @param colors  List of 6 QColor values (indices 0-5).
     */
    void setLogColors(const QList<QColor>& colors);

    // ── Backup ───────────────────────────────────────────────────────────
    bool    backupEnabled()    const { return m_backupEnabled; }    ///< Backup beim Öffnen ein-/ausschalten
    int     backupMaxCount()   const { return m_backupMaxCount; }   ///< Wie viele Backups vorgehalten werden
    QString backupNamePrefix() const { return m_backupNamePrefix; } ///< Präfix des Backup-Dateinamens (z. B. "Backup")
    QString backupDateFormat() const { return m_backupDateFormat; } ///< Qt-Datumsformat für den Zeitstempel
    QString backupDirectory()  const { return m_backupDirectory; }  ///< Zielverzeichnis, leer = Portfolio-Ordner

    /**
     * @brief Enable or disable automatic backup on portfolio open and save.
     * @param value  true to enable.
     */
    void setBackupEnabled(bool value);

    /**
     * @brief Set the maximum number of backups to keep and save.
     * @param value  Number of backups (rotation keeps the most recent N).
     */
    void setBackupMaxCount(int value);

    /**
     * @brief Set the prefix used in generated backup filenames and save.
     * @param value  Prefix string (e.g. "Backup"). Empty falls back to "Backup".
     */
    void setBackupNamePrefix(const QString& value);

    /**
     * @brief Set the Qt date format used for the backup filename timestamp and save.
     * @param value  Qt date format string (e.g. "yyyy_MM_dd_HH_mm_ss").
     *               Empty falls back to "yyyy_MM_dd_HH_mm_ss".
     */
    void setBackupDateFormat(const QString& value);

    /**
     * @brief Set the target directory for backups and save.
     * @param value  Absolute directory path, or empty to use the portfolio's own folder.
     */
    void setBackupDirectory(const QString& value);

    // ── Documents ────────────────────────────────────────────────────────
    /**
     * @brief Returns the configured document root directory.
     *
     * Going forward, every buy/sale/brokerage/dividend document must live
     * under this directory (see DocumentsSettingsForm and
     * DocumentRootMigrator). Empty means "not yet configured" — MainWindow
     * treats that as a mandatory first-run setup that must be completed
     * before the application can be used normally.
     * @return Absolute path to the document root directory, or empty.
     */
    QString documentsRootPath() const { return m_documentsRootPath; }

    /**
     * @brief Set the document root directory and save.
     *
     * This setter only persists the configured path — it does NOT rewrite
     * any existing document paths stored in the database. Callers (see
     * DocumentsSettingsForm::onSave()) run DocumentRootMigrator explicitly
     * *before* calling this setter, so the DB and the setting change
     * together as one logical step.
     * @param value  Absolute path to the new document root directory.
     */
    void setDocumentsRootPath(const QString& value);

    // ── Tray ─────────────────────────────────────────────────────────────
    /**
     * @brief Returns whether minimizing the main window moves it to the
     * system tray instead of the taskbar.
     *
     * Read by MainWindow::shouldMinimizeToTray() / MainWindow::changeEvent().
     * Default is false (opt-in) so existing installations keep the previous
     * minimize-to-taskbar behavior unchanged after an update.
     * @return true if minimize-to-tray is enabled.
     */
    bool trayOnMinimizeEnabled() const { return m_trayOnMinimizeEnabled; }

    /**
     * @brief Enable or disable minimize-to-tray and save.
     * @param value  true to minimize to the tray instead of the taskbar.
     */
    void setTrayOnMinimizeEnabled(bool value);

private:
    explicit AppSettings(QObject* parent = nullptr);

    QString m_settingsPath;

    // General
    QString m_portfolioPath;
    QString m_language            = "en";
    int     m_startNextShareUpdate = 2000;
    int     m_statusMessageClear   = 3000;
    bool    m_showExceptions       = true;

    // Window
    QPoint  m_windowPos   = {0, 0};
    QSize   m_windowSize  = {1024, 768};
    QString m_windowState = "Normal";

    // Logger
    int  m_logGuiEntries       = 20;
    bool m_logToFile           = true;
    int  m_logStoredFiles      = 25;
    bool m_logCleanupAtStartup = true;
    int  m_logComponents       = 7;
    int  m_logLevels           = 31;

    // Colors indexed by state position.
    //
    // Bugfix (24.07.2026): die vorherigen Defaults waren laut Kommentar
    // "optimiert für Dark Theme" (u. a. #e0e0e0 — praktisch weiß). Die App
    // setzt selbst nirgends aktiv ein Theme/eine Palette — sie übernimmt
    // ungefiltert, was die jeweilige Laufzeitumgebung an Qt-Theme liefert
    // (siehe ARCHITECTURE.md, "Log-Meldungsfarben theme-neutral"). Das
    // funktionierte auf Nessies Windows-Entwicklungsrechner (dunkles
    // System-/Qt-Theme), war aber auf hellem Theme unlesbar — reproduzierbar
    // z. B. im Linux-AppImage, dessen gebündelte Qt-Laufzeit (aqtinstall-
    // Binärpaket, nicht gegen GTK3 gelinkt) mangels Platform-Theme-Plugin
    // gar nicht erst an ein System-Theme herankommt und auf Qts helle
    // Standardpalette zurückfällt — unabhängig vom tatsächlichen Desktop.
    // Die neuen Defaults sind bewusst mittelton/kontrastreich auf HELLEM
    // wie DUNKLEM Hintergrund lesbar (grobe Kontrastprüfung nach WCAG-
    // Luminanzformel, jeweils ca. 4:1 zu Schwarz UND Weiß) statt auf ein
    // bestimmtes Theme hin optimiert. Bleiben über
    // Einstellungen → Logger... (LoggerSettingsForm) weiterhin frei
    // änderbar; die alten Dark-Theme-Werte bleiben dort in der Farbliste
    // wählbar (siehe LoggerSettingsForm.cpp, k_colorNames), damit bereits
    // gespeicherte alte Einstellungen weiter korrekt angezeigt werden.
    QList<QColor> m_logColors = {
        QColor("#808080"),   // 0: Start       — neutrales Grau
        QColor("#808080"),   // 1: Info         — neutrales Grau
        QColor("#b36b00"),   // 2: Warning      — dunkles Amber/Orange
        QColor("#d32f2f"),   // 3: Error        — mittleres Rot
        QColor("#b71c1c"),   // 4: FatalError   — dunkleres, kräftigeres Rot
        QColor("#388e3c")    // 5: Success      — mittleres Grün
    };

    // Sounds
    bool    m_soundUpdateEnabled = true;
    bool    m_soundErrorEnabled  = true;
    QString m_soundUpdateFile    = QStringLiteral("UpdateFinished.wav");
    QString m_soundErrorFile     = QStringLiteral("Error.wav");

    // API keys
    QString m_apiKeyYahoo;
    QString m_apiKeyOnVista;

    // Backup
    bool    m_backupEnabled    = true;
    int     m_backupMaxCount   = 5;
    QString m_backupNamePrefix = QStringLiteral("Backup");
    QString m_backupDateFormat = QStringLiteral("yyyy_MM_dd_HH_mm_ss");
    QString m_backupDirectory;   // leer = gleicher Ordner wie die Portfolio-Datei

    // Documents
    QString m_documentsRootPath; // leer = noch nicht konfiguriert (Erstlauf)

    // Tray
    bool m_trayOnMinimizeEnabled = false; // opt-in, siehe trayOnMinimizeEnabled()
};

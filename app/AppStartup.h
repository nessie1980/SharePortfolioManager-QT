// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QApplication>
#include <QTranslator>
#include <QString>
#include <QWidget>

/**
 * @brief Startup helper — groups all application initialisation steps.
 *
 * Extracted from main() so that each step can be unit-tested independently.
 * All methods are static; the class is never instantiated.
 *
 * ### Startup sequence (called from main())
 * 1. `settingsPath()`      — resolve the settings.ini path
 * 2. `installTranslator()` — load the configured .qm file
 * 3. `openDatabase()`      — open or create the portfolio SQLite database
 */
class AppStartup
{
public:
    AppStartup() = delete;

    /**
     * @brief Returns the full path to the settings.ini file.
     *
     * The file is always placed next to the executable, keeping the
     * application self-contained and portable.
     * @return Absolute path to settings.ini.
     */
    static QString settingsPath();

    /**
     * @brief Installs the Qt translator for the configured language.
     *
     * Looks for a `spm_<language>.qm` file in a `translations/` subdirectory
     * next to the executable. Falls back silently to the source language
     * (English) if the .qm file is not found.
     * @param app         The QApplication instance to install the translator on.
     * @param translator  The QTranslator instance to load into.
     * @param language    Language code (e.g. "de", "en").
     * @return true if the .qm file was loaded and installed, false on fallback.
     */
    static bool installTranslator(QApplication& app, QTranslator& translator,
                                   const QString& language);

    /**
     * @brief Opens the portfolio SQLite database.
     *
     * Uses the portfolio path from AppSettings. If no path is configured yet,
     * a default path (`portfolio.db` next to the executable) is set and saved.
     * Shows a critical error dialog if opening fails.
     * @param parent  Parent widget for error dialogs (nullptr is allowed).
     * @return true on success, false if the database could not be opened.
     */
    static bool openDatabase(QWidget* parent = nullptr);
};

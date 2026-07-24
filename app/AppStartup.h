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
 * 2. `loadSettings()`      — load settings.ini, creating it with defaults if missing
 * 3. `installTranslator()` — load the configured .qm file
 * 4. `openDatabase()`      — open or create the portfolio SQLite database
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
     * @brief Loads settings.ini, creating it with default values if it
     *        didn't exist yet.
     *
     * Bugfix (24.07.2026): a fresh install (no settings.ini shipped by the
     * installer/AppImage — see ARCHITECTURE.md, "Erstlauf ohne
     * settings.ini") previously left the file missing until the first
     * AppSettings-Setter call, which happened long after MainWindow's
     * startup checks had already run. Persisting the in-memory defaults
     * immediately here means a real settings.ini exists on disk right after
     * the very first start (as long as the target directory is writable —
     * if it isn't, AppSettings::save() fails silently and the app simply
     * keeps running on in-memory defaults, exactly as before this fix).
     * @param path  Full path to the settings file. If empty, defaults to
     *              settingsPath().
     * @return true if settings.ini already existed before this call,
     *         false if it was just newly created with defaults.
     */
    static bool loadSettings(const QString& path = {});

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
     * Shows a critical error dialog if opening fails (unless suppressed).
     * @param parent          Parent widget for error dialogs (nullptr is allowed).
     * @param showErrorDialog If false, no dialog is shown on failure — only
     *                        the qCritical() log entry. Used by unit tests to
     *                        exercise the failure path without blocking the
     *                        (headless) test run on a modal dialog; see
     *                        tests/app/tst_appstartup.cpp,
     *                        test_openDatabase_invalidPath_returnsFalse()
     *                        (added 19.07.2026 — the real QMessageBox::critical()
     *                        call previously blocked that test indefinitely
     *                        whenever a real display was attached).
     * @return true on success, false if the database could not be opened.
     */
    static bool openDatabase(QWidget* parent = nullptr, bool showErrorDialog = true);
};

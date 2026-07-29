// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "AppStartup.h"
#include "config/AppSettings.h"
#include "core/Database.h"

#include <QCoreApplication>
#include <QStandardPaths>
#include <QFileInfo>
#include <QMessageBox>
#include <QDir>
#include <QDebug>

// ── settingsPath ──────────────────────────────────────────────────────────────

QString AppStartup::settingsPath()
{
    // Bugfix (29.07.2026): QCoreApplication::applicationDirPath() pointed
    // into the AppImage's FUSE mount (/tmp/.mount_<random>/usr/bin), which
    // is a fresh, random directory on every launch — settings.ini was
    // written there correctly, but the next start looked in a different,
    // now-nonexistent mount directory and never found it again. See
    // ARCHITECTURE.md, "settings.ini nicht persistent im AppImage".
    // QStandardPaths::AppConfigLocation resolves to a stable, per-user
    // config directory (e.g. ~/.config/<OrganizationName>/<ApplicationName>
    // on Linux) that does not depend on where the executable itself lives,
    // so it works the same for AppImage, a normal Linux install, and the
    // Windows installer.
    const QString configDir =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(configDir);
    return configDir + QStringLiteral("/settings.ini");
}

// ── loadSettings ──────────────────────────────────────────────────────────────

bool AppStartup::loadSettings(const QString& path)
{
    const QString resolvedPath = path.isEmpty() ? settingsPath() : path;
    const bool existedBefore = QFileInfo::exists(resolvedPath);

    AppSettings::instance().load(resolvedPath);

    if (!existedBefore) {
        // Fresh install / first run — persist the in-memory defaults right
        // away instead of waiting for the first AppSettings-Setter call, so
        // MainWindow's startup check (checkAndLoadConfigurations()) finds a
        // real file. See AppStartup.h and ARCHITECTURE.md, "Erstlauf ohne
        // settings.ini". If the directory isn't writable, save() fails
        // silently here — that's fine, MainWindow now treats a still-missing
        // settings.ini as a warning, not a fatal error.
        AppSettings::instance().save();
        qInfo() << "[AppStartup] No settings.ini found — created one with defaults at:"
                << resolvedPath;
    }

    return existedBefore;
}

// ── installTranslator ─────────────────────────────────────────────────────────

bool AppStartup::installTranslator(QApplication& app, QTranslator& translator,
                                    const QString& language)
{
    const QString translationsDir =
        QCoreApplication::applicationDirPath() + QStringLiteral("/translations");

    if (translator.load(QStringLiteral("spm_%1").arg(language), translationsDir)) {
        app.installTranslator(&translator);
        qInfo() << "[AppStartup] Loaded translation:" << language;
        return true;
    }

    // Missing .qm file is not a fatal error — fall back to source language silently.
    qInfo() << "[AppStartup] No translation file for language:" << language
            << "— falling back to source language.";
    return false;
}

// ── openDatabase ──────────────────────────────────────────────────────────────

bool AppStartup::openDatabase(QWidget* parent, bool showErrorDialog)
{
    const QString portfolioPath = AppSettings::instance().portfolioPath();

    // If no portfolio is configured, return true so the app starts normally.
    // MainWindow will show the appropriate hint in the status messages.
    if (portfolioPath.isEmpty()) {
        qInfo() << "[AppStartup] No portfolio path configured — user must create or open one.";
        return true;
    }

    // If the configured file no longer exists, do NOT create a new one.
    // MainWindow will show the error in the status messages.
    if (!QFileInfo::exists(portfolioPath)) {
        qWarning() << "[AppStartup] Configured portfolio not found:" << portfolioPath;
        return true;
    }

    if (!Database::instance().open(portfolioPath)) {
        if (showErrorDialog) {
            const QString errorText = QObject::tr(
                "The portfolio database could not be opened:\n\n%1\n\n"
                "Please check the path in the settings.")
                .arg(portfolioPath);

            QMessageBox::critical(parent,
                                  QObject::tr("Database Error"),
                                  errorText);
        }
        qCritical() << "[AppStartup] Failed to open database:" << portfolioPath;
        return false;
    }

    qInfo() << "[AppStartup] Database opened:" << portfolioPath;
    return true;
}

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "AppStartup.h"
#include "config/AppSettings.h"
#include "core/Database.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QMessageBox>
#include <QDebug>

// ── settingsPath ──────────────────────────────────────────────────────────────

QString AppStartup::settingsPath()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/settings.ini");
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

bool AppStartup::openDatabase(QWidget* parent)
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
        const QString errorText = QObject::tr(
            "The portfolio database could not be opened:\n\n%1\n\n"
            "Please check the path in the settings.")
            .arg(portfolioPath);

        QMessageBox::critical(parent,
                              QObject::tr("Database Error"),
                              errorText);
        qCritical() << "[AppStartup] Failed to open database:" << portfolioPath;
        return false;
    }

    qInfo() << "[AppStartup] Database opened:" << portfolioPath;
    return true;
}

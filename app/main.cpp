// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include <QApplication>
#include <QTranslator>
#include <QDebug>

#include "AppStartup.h"
#include "config/AppSettings.h"
#include "core/Database.h"
#include "forms/MainForm/MainWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("SharePortfolioManager"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));
    app.setOrganizationName(QStringLiteral("nessie1980"));

    // ── Load settings ──────────────────────────────────────────────────────
    // Must happen before anything else so all components get their config.
    AppSettings::instance().load(AppStartup::settingsPath());
    qInfo() << "[main] Settings loaded from:" << AppStartup::settingsPath();

    // ── Install translator ─────────────────────────────────────────────────
    QTranslator translator;
    AppStartup::installTranslator(app, translator, AppSettings::instance().language());

    // ── Open database ──────────────────────────────────────────────────────
    if (!AppStartup::openDatabase())
        return 1;

    // ── Launch main window ─────────────────────────────────────────────────
    MainWindow mainWindow;
    mainWindow.show();

    const int returnCode = app.exec();

    // ── Cleanup ────────────────────────────────────────────────────────────
    // Save window geometry before exit so it is restored on next launch.
    AppSettings::instance().setWindowGeometry(
        mainWindow.pos(),
        mainWindow.size(),
        mainWindow.isMaximized() ? QStringLiteral("Maximized")
                                 : QStringLiteral("Normal"));

    Database::instance().close();
    qInfo() << "[main] Application exiting with code:" << returnCode;

    return returnCode;
}

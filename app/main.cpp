// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QDebug>

#include "AppStartup.h"
#include "config/AppSettings.h"
#include "core/Database.h"
#include "forms/MainForm/MainWindow.h"

int main(int argc, char* argv[])
{
    // Bugfix 23.07.2026: formatMoney() in den Views (ViewBrokerageEdit,
    // ViewShareEdit, ViewBuyEdit, ViewDividendEdit, ViewShareAdd) verwendet
    // durchgängig QLocale().toString(...) — der No-Argument-Konstruktor
    // QLocale() greift ohne diesen Aufruf auf die System-Locale des
    // ausführenden Rechners zurück, nicht fest auf Deutsch. Auf Systemen mit
    // anderer Standard-Locale (z. B. englischsprachiges Windows, oder der
    // GitHub-Actions-CI-Runner) wurden Beträge dadurch mit Punkt statt Komma
    // angezeigt — inkonsistent mit der sonst komplett deutschsprachigen
    // Oberfläche. Muss vor jeder Verwendung von QLocale() gesetzt werden,
    // daher als Allererstes in main(). Siehe ARCHITECTURE.md, Abschnitt
    // "System-Locale-abhängiges Zahlenformat — Bugfix (23.07.2026)".
    QLocale::setDefault(QLocale::German);

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("SharePortfolioManager"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));
    app.setOrganizationName(QStringLiteral("nessie1980"));

    // ── Load settings ──────────────────────────────────────────────────────
    // Must happen before anything else so all components get their config.
    // loadSettings() (statt AppSettings::instance().load() direkt) persistiert
    // bei einer frischen Installation sofort die Defaults, siehe AppStartup.h
    // und ARCHITECTURE.md, "Erstlauf ohne settings.ini".
    AppStartup::loadSettings();
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

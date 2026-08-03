// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QMessageBox>
#include <QDebug>

#include "AppStartup.h"
#include "Version.h"
#include "config/AppSettings.h"
#include "core/Database.h"
#include "core/SingleInstanceGuard.h"
#include "IconProvider.h"
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
    // Bugfix (29.07.2026): zuvor ein Hardcoded-Literal ("1.0.0"/"1.0.1"),
    // unabhängig von project(SharePortfolioManager VERSION ...) in der
    // Root-CMakeLists.txt gepflegt — ein Versionsbump musste dadurch an
    // zwei Stellen erfolgen, was schon beim ersten PATCH-Bump (1.0.1)
    // prompt vergessen wurde. SPM_VERSION_STRING kommt jetzt aus der von
    // CMake generierten Version.h (siehe app/Version.h.in,
    // app/CMakeLists.txt und ARCHITECTURE.md, Abschnitt "Versionierung") —
    // die Versionsnummer hat damit nur noch eine einzige Quelle.
    app.setApplicationVersion(QStringLiteral(SPM_VERSION_STRING));
    // OrganizationName (29.07.2026): "BT" statt des zuvor verwendeten
    // GitHub-Handles "nessie1980" — QStandardPaths::AppConfigLocation baut
    // den Konfigurationspfad unter Linux als
    // ~/.config/<OrganizationName>/<ApplicationName>/ auf (siehe
    // AppStartup::settingsPath() und ARCHITECTURE.md, "settings.ini nicht
    // persistent im AppImage"); ein neutraler, nicht-persönlicher Name ist
    // dort auf Nessies Wunsch dem persönlichen Handle vorgezogen worden.
    app.setOrganizationName(QStringLiteral("BT"));

    // Feature (03.08.2026): Titelleiste/Taskleiste zeigten bislang das
    // generische Qt-Standardsymbol statt eines eigenen App-Icons — fiel im
    // Zuge des Tray-Icon-Features auf ("Minimieren wahlweise in Taskleiste
    // oder Tray"), als für den Tray ohnehin ein mehrstufiges App-Icon nötig
    // wurde (IconProvider::appIcon(), siehe dort). setWindowIcon() auf
    // QApplication statt auf MainWindow, damit auch Dialoge (z. B. unter
    // manchen Linux-Fenstermanagern) dasselbe Icon erben.
    app.setWindowIcon(IconProvider::appIcon());

    // ── Single instance ──────────────────────────────────────────────────
    // Feature (03.08.2026): "Die Anwendung darf nur einmal gestartet
    // werden". Muss vor AppStartup::loadSettings()/openDatabase() laufen,
    // damit eine zweite gestartete Instanz die Portfolio-SQLite-Datei gar
    // nicht erst parallel öffnet (Datenintegrität) — siehe
    // ARCHITECTURE.md, "Die Anwendung darf nur einmal gestartet werden".
    SingleInstanceGuard singleInstanceGuard(
        SingleInstanceGuard::buildServerName(app.organizationName(),
                                              app.applicationName()));
    if (!singleInstanceGuard.tryAcquire()) {
        // tryAcquire() hat der bereits laufenden Instanz schon ein
        // Aktivierungssignal geschickt (siehe deren Verbindung zu
        // MainWindow::restoreFromTray() weiter unten) — hier nur noch der
        // kurze Hinweis für den Benutzer dieser zweiten Instanz, dann
        // sofort beenden, ohne Settings/DB/MainWindow anzurühren.
        QMessageBox::information(nullptr, QObject::tr("Bereits gestartet"),
            QObject::tr("Share Portfolio Manager läuft bereits.\n"
                        "Das geöffnete Fenster wurde in den Vordergrund geholt."));
        return 0;
    }

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
    QObject::connect(&singleInstanceGuard, &SingleInstanceGuard::activationRequested,
                      &mainWindow, &MainWindow::restoreFromTray);
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

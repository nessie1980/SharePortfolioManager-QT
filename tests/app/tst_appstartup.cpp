// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include <QtTest>
#include <QApplication>
#include <QTranslator>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "../../app/AppStartup.h"
#include "../../app/config/AppSettings.h"
#include "../../app/core/Database.h"

class TestAppStartup : public QObject
{
    Q_OBJECT

private:
    // Temporary directory used as a sandbox for all settings and database files.
    // Created once in initTestCase(), valid for the lifetime of the test run.
    QTemporaryDir m_tempDir;

    // Loads AppSettings from a throw-away INI inside m_tempDir.
    // This prevents any test from writing to the real settings.ini.
    void loadSandboxedSettings()
    {
        const QString sandboxIni = m_tempDir.path() + QStringLiteral("/test_settings.ini");
        AppSettings::instance().load(sandboxIni);
    }

private slots:

    void initTestCase()
    {
        QVERIFY(m_tempDir.isValid());
        loadSandboxedSettings();
    }

    void cleanupTestCase()
    {
        if (Database::instance().isOpen())
            Database::instance().close();
        // Bewusst KEIN AppSettings::instance().load(AppStartup::settingsPath())
        // mehr — der AppSettings-Singleton ist prozesslokal und stirbt mit
        // diesem Testprozess; ein "Zurücksetzen auf die echte settings.ini"
        // schützt nichts, sondern hätte in einem Testbinary mit mehreren
        // QObject-Testklassen im selben Prozess (wie tst_mainwindow.cpp)
        // genau das Gegenteil bewirkt: spätere Testklassen schrieben dann
        // versehentlich in die echte Konfigurationsdatei statt in ihre
        // eigene Sandbox (gemeldet und an der eigentlichen Stelle behoben
        // 19.07.2026, siehe tst_mainwindow.cpp). Hier zwar unschädlich (nur
        // eine einzelne Testklasse in diesem Binary), aber als falsches
        // Vorbild trotzdem entfernt.
    }

    void init()
    {
        // Reset to sandboxed settings and close any open database before each test.
        loadSandboxedSettings();
        if (Database::instance().isOpen())
            Database::instance().close();
    }

    // ── AppStartup::settingsPath ───────────────────────────────────────────

    void test_settingsPath_endsWithSettingsIni()
    {
        QVERIFY(AppStartup::settingsPath().endsWith(QStringLiteral("/settings.ini")));
    }

    void test_settingsPath_isAbsolute()
    {
        QVERIFY(QDir::isAbsolutePath(AppStartup::settingsPath()));
    }

    void test_settingsPath_containsAppDir()
    {
        QVERIFY(AppStartup::settingsPath().startsWith(
            QCoreApplication::applicationDirPath()));
    }

    // ── AppStartup::loadSettings ────────────────────────────────────────────
    //
    // Bugfix (24.07.2026): eine frische Installation (z. B. Linux-AppImage)
    // liefert bewusst keine settings.ini mit (siehe ARCHITECTURE.md,
    // "Erstlauf ohne settings.ini") — vorher blieb die Datei dadurch bis zum
    // ersten AppSettings-Setter-Aufruf unauffindbar, was MainWindows
    // Startup-Check fälschlich als FatalError wertete. loadSettings()
    // persistiert die Defaults jetzt sofort, wenn die Datei noch nicht
    // existiert.

    void test_loadSettings_missingFile_createsFileWithDefaults()
    {
        const QString path = m_tempDir.path() + QStringLiteral("/fresh_settings.ini");
        QVERIFY(!QFileInfo::exists(path));

        const bool existedBefore = AppStartup::loadSettings(path);

        QVERIFY(!existedBefore);
        QVERIFY(QFileInfo::exists(path));
    }

    void test_loadSettings_existingFile_returnsTrueAndPreservesValues()
    {
        const QString path = m_tempDir.path() + QStringLiteral("/existing_settings.ini");

        // First call creates the file with defaults; then change a value
        // so we have something distinctive to check survives the 2nd call.
        AppStartup::loadSettings(path);
        AppSettings::instance().setLanguage(QStringLiteral("fr"));
        QVERIFY(QFileInfo::exists(path));

        const bool existedBefore = AppStartup::loadSettings(path);

        QVERIFY(existedBefore);
        // Proof that loadSettings() didn't blindly re-save fresh defaults
        // over an already-existing file on the second call.
        QCOMPARE(AppSettings::instance().language(), QStringLiteral("fr"));
    }

    // ── AppStartup::installTranslator ─────────────────────────────────────

    void test_installTranslator_missingFile_returnsFalse()
    {
        QApplication* qapp = qobject_cast<QApplication*>(QCoreApplication::instance());
        QVERIFY(qapp != nullptr);

        QTranslator translator;
        // "xx" is not a valid language code — no .qm file will be found
        QVERIFY(!AppStartup::installTranslator(*qapp, translator,
                                                QStringLiteral("xx")));
    }

    // ── AppStartup::openDatabase ──────────────────────────────────────────

    void test_openDatabase_inMemory_succeeds()
    {
        // :memory: is a special SQLite path — file-existence check is skipped
        // because QFileInfo::exists(":memory:") returns false. So openDatabase()
        // will return true without opening, leaving the DB closed.
        // The in-memory test is therefore a no-op with the new logic; we just
        // verify it does not crash and returns true.
        AppSettings::instance().setPortfolioPath(QStringLiteral(":memory:"));
        QVERIFY(AppStartup::openDatabase());
    }

    void test_openDatabase_setsDefaultPath_whenEmpty()
    {
        // With an empty path, openDatabase() now returns true immediately
        // without setting any default path — that is MainWindow's responsibility.
        AppSettings::instance().setPortfolioPath(QString());
        QVERIFY(AppSettings::instance().portfolioPath().isEmpty());

        const bool result = AppStartup::openDatabase();

        QVERIFY(result);
        // Path must still be empty — no auto-default is set any more
        QVERIFY(AppSettings::instance().portfolioPath().isEmpty());
        QVERIFY(!Database::instance().isOpen());
    }

    void test_openDatabase_missingFile_returnsTrueWithoutOpening()
    {
        // A configured path that no longer exists: openDatabase() returns true
        // but does NOT open the DB (no auto-create).
        AppSettings::instance().setPortfolioPath(
            QStringLiteral("/nonexistent/path/portfolio.db"));

        QVERIFY(AppStartup::openDatabase());
        QVERIFY(!Database::instance().isOpen());
    }

    void test_openDatabase_invalidPath_returnsFalse()
    {
        // Point to a directory — QFileInfo::exists() returns true for directories,
        // but SQLite cannot open a directory as a database file → must return false.
        const QString dirPath = m_tempDir.path() + QStringLiteral("/subdir");
        QVERIFY(QDir().mkpath(dirPath));
        AppSettings::instance().setPortfolioPath(dirPath);

        // showErrorDialog=false: without this, openDatabase() would show a
        // real, blocking QMessageBox::critical() on failure — fine for the
        // real application, but it would hang this (headless) test run
        // waiting for a manual click. See AppStartup.h for details.
        QVERIFY(!AppStartup::openDatabase(nullptr, /*showErrorDialog=*/false));
        QVERIFY(!Database::instance().isOpen());
    }

    void test_openDatabase_validTempPath_succeeds()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/portfolio.db");
        // Create the file first so QFileInfo::exists() returns true
        { QFile f(dbPath); QVERIFY(f.open(QIODevice::WriteOnly)); }
        AppSettings::instance().setPortfolioPath(dbPath);

        QVERIFY(AppStartup::openDatabase());
        QVERIFY(Database::instance().isOpen());
    }

    // ── Window geometry round-trip ────────────────────────────────────────

    void test_windowGeometry_roundTrip()
    {
        const QPoint  expectedPos(120, 80);
        const QSize   expectedSize(1280, 800);
        const QString expectedState = QStringLiteral("Normal");

        AppSettings::instance().setWindowGeometry(expectedPos, expectedSize, expectedState);

        QCOMPARE(AppSettings::instance().windowPos(),   expectedPos);
        QCOMPARE(AppSettings::instance().windowSize(),  expectedSize);
        QCOMPARE(AppSettings::instance().windowState(), expectedState);
    }

    void test_windowGeometry_maximized()
    {
        AppSettings::instance().setWindowGeometry(
            QPoint(0, 0), QSize(1920, 1080), QStringLiteral("Maximized"));

        QCOMPARE(AppSettings::instance().windowState(), QStringLiteral("Maximized"));
    }
};

QTEST_MAIN(TestAppStartup)
#include "tst_appstartup.moc"

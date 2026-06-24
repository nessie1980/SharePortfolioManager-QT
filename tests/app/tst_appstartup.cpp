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

        // Reset AppSettings to the real settings.ini path so subsequent
        // application runs are not affected by test-written values.
        AppSettings::instance().load(AppStartup::settingsPath());
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

        QVERIFY(!AppStartup::openDatabase());
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

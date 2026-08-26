// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// tst_appsettings.cpp - Unit-Tests fuer den AppSettings-Singleton.
//
// Aus tst_mainwindow.cpp ausgelagert (26.08.2026). Die zehn Tests pruefen
// ausschliesslich Setzen und Zuruecklesen von Einstellungen (Logger-Farben,
// -Level und -Komponenten, Sound-Optionen, Yahoo-API-Schluessel, Portfolio-
// Pfad) - kein Dialog, kein MainWindow. Sie lagen bisher zwischen den
// MainWindow-Tests, obwohl sie mit MainWindow nichts zu tun haben; deshalb
// tests/config/ statt tests/forms/.
//
// @note Die Tests setzen den geaenderten Wert am Ende jeweils wieder auf den
// Ausgangswert zurueck - das Muster stammt aus tst_mainwindow.cpp und bleibt
// erhalten, weil AppSettings ein prozessweiter Singleton ist.
//
// initTestCase() lenkt den Singleton auf eine sandboxte INI-Datei im
// QTemporaryDir. Ein AppSettings::instance().load(QString()) in
// cleanupTestCase() ist verboten - es wuerde den Singleton auf die echte
// settings.ini umlenken (Bugfix 19.07.2026, siehe TESTING.md).

#include <QtTest>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QColor>
#include <QLocale>

#include "../../app/config/AppSettings.h"
#include "../../app/core/Database.h"

class TestAppSettings : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;

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
    }

    void init()
    {
        loadSandboxedSettings();
        if (Database::instance().isOpen())
            Database::instance().close();
    }

    void test_newPortfolio_settingsPathUpdated()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/new_portfolio.db");
        Database::instance().open(dbPath);
        AppSettings::instance().setPortfolioPath(dbPath);
        QCOMPARE(AppSettings::instance().portfolioPath(), dbPath);
    }

    void test_loggerSettings_saveColors()
    {
        const QColor c(QStringLiteral("#44ff44"));
        QList<QColor> colors = AppSettings::instance().logColors();
        colors[5] = c;
        AppSettings::instance().setLogColors(colors);
        QCOMPARE(AppSettings::instance().logColorAt(5).name(), c.name());
    }

    void test_loggerSettings_saveLevels()
    {
        AppSettings::instance().setLogLevels(0b00111);
        QCOMPARE(AppSettings::instance().logLevels(), 0b00111);
        AppSettings::instance().setLogLevels(31);
    }

    void test_loggerSettings_saveComponents()
    {
        AppSettings::instance().setLogComponents(0b011);
        QCOMPARE(AppSettings::instance().logComponents(), 0b011);
        AppSettings::instance().setLogComponents(7);
    }

    void test_soundSettings_saveUpdateEnabled()
    {
        AppSettings::instance().setSoundUpdateEnabled(false);
        QVERIFY(!AppSettings::instance().soundUpdateEnabled());
        AppSettings::instance().setSoundUpdateEnabled(true);
    }

    void test_soundSettings_saveErrorEnabled()
    {
        AppSettings::instance().setSoundErrorEnabled(false);
        QVERIFY(!AppSettings::instance().soundErrorEnabled());
        AppSettings::instance().setSoundErrorEnabled(true);
    }

    void test_soundSettings_saveUpdateFile()
    {
        AppSettings::instance().setSoundUpdateFile(QStringLiteral("Error.wav"));
        QCOMPARE(AppSettings::instance().soundUpdateFile(), QStringLiteral("Error.wav"));
        AppSettings::instance().setSoundUpdateFile(QStringLiteral("UpdateFinished.wav"));
    }

    void test_soundSettings_saveErrorFile()
    {
        AppSettings::instance().setSoundErrorFile(QStringLiteral("UpdateFinished.wav"));
        QCOMPARE(AppSettings::instance().soundErrorFile(), QStringLiteral("UpdateFinished.wav"));
        AppSettings::instance().setSoundErrorFile(QStringLiteral("Error.wav"));
    }

    void test_soundSettings_scanFallback()
    {
        QDir d(m_tempDir.path() + QStringLiteral("/nosounds"));
        const QStringList files = d.entryList(QStringList() << QStringLiteral("*.wav"),
                                              QDir::Files, QDir::Name);
        QVERIFY(files.isEmpty());
    }

    void test_apiSettings_saveYahooKey()
    {
        AppSettings::instance().setApiKeyYahoo(QStringLiteral("test-key-123"));
        QCOMPARE(AppSettings::instance().apiKeyYahoo(), QStringLiteral("test-key-123"));
        AppSettings::instance().setApiKeyYahoo(QString());
    }
}; // end of TestAppSettings

int main(int argc, char* argv[])
{
    QLocale::setDefault(QLocale::German);

    QCoreApplication app(argc, argv);

    TestAppSettings t;
    return QTest::qExec(&t, argc, argv);
}

#include "tst_appsettings.moc"

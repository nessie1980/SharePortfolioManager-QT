// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// tst_backupform.cpp — Unit tests for BackupWorker, BackupProgressDialog und
// die createBackup()-Regression über eine echte MainWindow-Konstruktion.
//
// Aus tst_mainwindow.cpp herausgelöst (22.08.2026) — analog tst_buysform,
// tst_dividendform und tst_salesform. TestBackupForm hatte keinen eigenen
// Stub-Block (anders als TestSalesForm/TestDividendForm) — der einzige Helfer
// ist die klasseneigene Sandbox (loadSandboxedSettings()/makeTestFile()), die
// unverändert mitgezogen ist.
//
// createBackup() ist eine private Methode von MainWindow und braucht dessen
// volle Konstruktion (siehe Kommentar in tst_backupsettingsform.cpp) — die
// drei test_createBackup_*-Fälle bauen deshalb ein echtes MainWindow auf,
// genau wie TestMainWindow. Die Quellenliste in CMakeLists.txt spiegelt daher
// bewusst die von tst_mainwindow (gleiche MainWindow.cpp-Abhängigkeiten),
// minus FakeNetworkAccessManager — das wird nur von TestMainWindow selbst
// benutzt, nicht von MainWindow.cpp oder von TestBackupForm.
//
// main() setzt — wie tst_mainwindow.cpp/tst_salesform.cpp/tst_dividendform.cpp
// — QLocale::setDefault(QLocale::German), bevor QApplication konstruiert
// wird: MainWindow formatiert beim Aufbau Beträge, und auf einem Runner mit
// englischer System-Locale soll das nicht von den Testannahmen abweichen.
// Siehe ARCHITECTURE.md, "System-Locale-abhängiges Zahlenformat".
//
// Bewusst NICHT mitgenommen (siehe Status-Doku, "Stolperstellen"):
// Version.h/SPM_VERSION_STRING und ${CMAKE_BINARY_DIR}/app — die braucht nur
// der Fenstertitel-Versionstest in TestMainWindow, den es hier nicht gibt.

#include <QtTest>
#include <QSignalSpy>
#include <QApplication>
#include <QTemporaryDir>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QLocale>
#include <QElapsedTimer>
#include <QProgressBar>
#include <QPushButton>

#include "../../app/forms/MainForm/MainWindow.h"
#include "../../app/forms/BackupProgressForm/BackupWorker.h"
#include "../../app/forms/BackupProgressForm/BackupProgressDialog.h"
#include "../../app/config/AppSettings.h"
#include "../../app/core/Database.h"

// ─────────────────────────────────────────────────────────────────────────────
// TestBackupForm
// ─────────────────────────────────────────────────────────────────────────────
class TestBackupForm : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;

    // Eigene Sandbox — unabhängig davon, welche AppSettings-Werte eine
    // vorher im selben Prozess gelaufene Testklasse hinterlassen hat (siehe
    // main() am Dateiende: mehrere QObject-Klassen liefen früher sequenziell
    // im selben Prozess in tst_mainwindow.cpp). Vorher verließ sich diese
    // Klasse stillschweigend auf den Zustand von TestMainWindow/TestSalesForm
    // — als deren cleanupTestCase() fälschlich auf die echte settings.ini
    // umleitete (behoben 19.07.2026), schrieben die setPortfolioPath()/
    // setDocumentsRootPath()-Aufrufe unten direkt in Nessies reale
    // Konfiguration. Jetzt lädt diese Klasse immer zuerst ihre eigene,
    // separate Sandbox-INI, komplett unabhängig von anderen Testklassen.
    void loadSandboxedSettings()
    {
        const QString sandboxIni = m_tempDir.path() + QStringLiteral("/test_settings.ini");
        AppSettings::instance().load(sandboxIni);

        // Verhindert, dass MainWindow::ensureDocumentsRootConfigured() beim
        // Konstruieren einen blockierenden Dialog öffnet (der Dialog
        // erscheint nur, wenn documentsRootPath() leer ist).
        AppSettings::instance().setDocumentsRootPath(
            m_tempDir.path() + QStringLiteral("/documents"));
    }

    // Helper: create a small test file with given size in bytes
    QString makeTestFile(const QString& name, int sizeBytes = 1024)
    {
        const QString path = m_tempDir.path() + QDir::separator() + name;
        QFile f(path);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(QByteArray(sizeBytes, 'X'));
            f.close();
        }
        return path;
    }

private slots:

    void initTestCase()
    {
        QVERIFY(m_tempDir.isValid());
        loadSandboxedSettings();
    }

    void init()
    {
        // Vor jedem einzelnen Test erneut sandboxen — dieselbe Absicherung
        // wie init()/loadSandboxedSettings() in TestMainWindow/TestSalesForm.
        loadSandboxedSettings();
        if (Database::instance().isOpen())
            Database::instance().close();
    }

    void cleanupTestCase()
    {
        if (Database::instance().isOpen())
            Database::instance().close();
        // Bewusst KEIN AppSettings::instance().load(...) mit echtem Pfad —
        // siehe Begründung bei TestMainWindow::cleanupTestCase().
    }

    // ── BackupWorker — successful copy ────────────────────────────────────────

    void test_backupWorker_copiesFileSuccessfully()
    {
        const QString src = makeTestFile(QStringLiteral("portfolio.db"), 2048);
        const QString dst = m_tempDir.path() + QStringLiteral("/Backup_portfolio.db");

        BackupWorker worker(src, dst);

        bool success = false;
        connect(&worker, &BackupWorker::finished,
                this, [&](bool s, const QString&) { success = s; });

        worker.run();

        QVERIFY(success);
        QVERIFY(QFileInfo::exists(dst));
        QCOMPARE(QFileInfo(dst).size(), QFileInfo(src).size());
    }

    void test_backupWorker_emitsProgressSignal()
    {
        const QString src = makeTestFile(QStringLiteral("progress_test.db"), 4096);
        const QString dst = m_tempDir.path() + QStringLiteral("/Backup_progress.db");

        BackupWorker worker(src, dst);

        QSignalSpy spy(&worker, &BackupWorker::progress);
        bool finished = false;
        connect(&worker, &BackupWorker::finished,
                this, [&](bool, const QString&) { finished = true; });

        worker.run();

        QVERIFY(finished);
        // At least one progress signal must have been emitted
        QVERIFY(spy.count() >= 1);
        // Last progress: bytesWritten == totalBytes
        const QList<QVariant> lastArgs = spy.last();
        QCOMPARE(lastArgs.at(0).toLongLong(), lastArgs.at(1).toLongLong());
    }

    void test_backupWorker_emitsFinishedWithSuccess()
    {
        const QString src = makeTestFile(QStringLiteral("finished_test.db"), 512);
        const QString dst = m_tempDir.path() + QStringLiteral("/Backup_finished.db");

        BackupWorker worker(src, dst);

        QSignalSpy spy(&worker, &BackupWorker::finished);
        worker.run();

        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toBool()); // success = true
    }

    // ── BackupWorker — missing source ─────────────────────────────────────────

    void test_backupWorker_missingSource_emitsFailure()
    {
        const QString src = m_tempDir.path() + QStringLiteral("/nonexistent.db");
        const QString dst = m_tempDir.path() + QStringLiteral("/Backup_nonexistent.db");

        BackupWorker worker(src, dst);

        bool success = true;
        connect(&worker, &BackupWorker::finished,
                this, [&](bool s, const QString&) { success = s; });

        worker.run();

        QVERIFY(!success);
        QVERIFY(!QFileInfo::exists(dst));
    }

    // ── BackupWorker — cancellation ───────────────────────────────────────────

    void test_backupWorker_cancel_removesPartialFile()
    {
        const QString src = makeTestFile(QStringLiteral("cancel_test.db"), 1024);
        const QString dst = m_tempDir.path() + QStringLiteral("/Backup_cancel.db");

        BackupWorker worker(src, dst);

        bool success = true;
        connect(&worker, &BackupWorker::finished,
                this, [&](bool s, const QString&) { success = s; });

        // Cancel before run — simulates immediate cancel
        worker.cancel();
        worker.run();

        QVERIFY(!success);
        // Partial file must be removed
        QVERIFY(!QFileInfo::exists(dst));
    }

    // ── BackupProgressDialog ──────────────────────────────────────────────────
    // Note: BackupProgressDialog starts a QThread in its constructor.
    // ~BackupProgressDialog() waits for the worker thread itself (quit() +
    // wait()) before tearing down its QThread child, so destroying the
    // dialog without waiting is safe (see
    // test_backupProgressDialog_destroyedImmediately_doesNotCrash below).
    // The other tests still call waitForDialog() — not to avoid a crash, but
    // because they assert on wasSuccessful()/the copied file afterwards.

    // Helper: wait for dialog to finish (max 5 seconds)
    static void waitForDialog(BackupProgressDialog& dlg)
    {
        QElapsedTimer timer;
        timer.start();
        while (!dlg.wasSuccessful() && timer.elapsed() < 5000)
            QApplication::processEvents(QEventLoop::AllEvents, 50);
    }

    void test_backupProgressDialog_canBeConstructed()
    {
        const QString src = makeTestFile(QStringLiteral("dlg_test.db"), 512);
        const QString dst = m_tempDir.path() + QStringLiteral("/Backup_dlg_test.db");

        BackupProgressDialog dlg(src, dst);
        QVERIFY(dlg.windowTitle().contains(QStringLiteral("Backup")));
        waitForDialog(dlg); // must complete before dlg is destroyed
    }

    void test_backupProgressDialog_isModal()
    {
        const QString src = makeTestFile(QStringLiteral("modal_test.db"), 512);
        const QString dst = m_tempDir.path() + QStringLiteral("/Backup_modal.db");

        BackupProgressDialog dlg(src, dst);
        QVERIFY(dlg.isModal());
        waitForDialog(dlg);
    }

    void test_backupProgressDialog_hasCancelButton()
    {
        const QString src = makeTestFile(QStringLiteral("cancel_btn_test.db"), 512);
        const QString dst = m_tempDir.path() + QStringLiteral("/Backup_cancel_btn.db");

        BackupProgressDialog dlg(src, dst);
        const auto buttons = dlg.findChildren<QPushButton*>();
        if (buttons.isEmpty()) QFAIL("No button found in BackupProgressDialog");
        bool hasCancel = false;
        for (auto* b : buttons)
            if (b->text().contains(QStringLiteral("Abbrechen")))
                hasCancel = true;
        QVERIFY(hasCancel);
        waitForDialog(dlg);
    }

    void test_backupProgressDialog_hasProgressBar()
    {
        const QString src = makeTestFile(QStringLiteral("pbar_test.db"), 512);
        const QString dst = m_tempDir.path() + QStringLiteral("/Backup_pbar.db");

        BackupProgressDialog dlg(src, dst);
        const auto bars = dlg.findChildren<QProgressBar*>();
        QVERIFY(!bars.isEmpty());
        QCOMPARE(bars.first()->minimum(), 0);
        QCOMPARE(bars.first()->maximum(), 100);
        waitForDialog(dlg);
    }

    void test_backupProgressDialog_successfulCopy_wasSuccessfulTrue()
    {
        const QString src = makeTestFile(QStringLiteral("success_dlg.db"), 512);
        const QString dst = m_tempDir.path() + QStringLiteral("/Backup_success_dlg.db");

        BackupProgressDialog dlg(src, dst);
        waitForDialog(dlg);

        QVERIFY(dlg.wasSuccessful());
        QVERIFY(QFileInfo::exists(dst));
    }

    // ── BackupProgressDialog — destructor race regression ──────────────────────
    // Before the fix, BackupWorker::finished() drove onFinished() (sets
    // m_success) and QThread::quit() via two separate queued events. A
    // caller destroying the dialog as soon as wasSuccessful() looked done —
    // or, as here, immediately without waiting at all — could tear down a
    // still-running QThread child ("QThread: Destroyed while thread is
    // still running"), crashing the process. The destructor must now wait
    // for the worker thread itself, so this must survive unconditionally.
    void test_backupProgressDialog_destroyedImmediately_doesNotCrash()
    {
        const QString src = makeTestFile(QStringLiteral("immediate_dtor.db"), 512);
        const QString dst = m_tempDir.path() + QStringLiteral("/Backup_immediate_dtor.db");

        {
            BackupProgressDialog dlg(src, dst);
            // No waitForDialog() here on purpose — the worker thread may
            // still be mid-copy or mid-quit() when dlg goes out of scope.
        }

        QVERIFY(true); // reaching this line means the destructor didn't crash
    }

    // ── createBackup via MainWindow ───────────────────────────────────────────

    void test_createBackup_createsBackupFile()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/MyPortfolio.db");
        { QFile f(dbPath); if (f.open(QIODevice::WriteOnly)) { f.write(QByteArray(512, 'X')); } }

        Database::instance().open(dbPath);
        AppSettings::instance().setPortfolioPath(dbPath);

        MainWindow window;
        QApplication::processEvents();

        // After construction createBackup() is called — find backup file
        QDir dir(m_tempDir.path());
        dir.setNameFilters({ QStringLiteral("Backup_MyPortfolio_*.db") });
        const QStringList backups = dir.entryList(QDir::Files);
        QVERIFY(!backups.isEmpty());
    }

    void test_createBackup_filenameContainsOriginalName()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/ShareList.db");
        { QFile f(dbPath); if (f.open(QIODevice::WriteOnly)) { f.write(QByteArray(512, 'X')); } }

        Database::instance().open(dbPath);
        AppSettings::instance().setPortfolioPath(dbPath);

        MainWindow window;
        QApplication::processEvents();

        QDir dir(m_tempDir.path());
        dir.setNameFilters({ QStringLiteral("Backup_ShareList_*.db") });
        const QStringList backups = dir.entryList(QDir::Files);
        QVERIFY(!backups.isEmpty());
        // Name must start with Backup_ShareList_
        QVERIFY(backups.first().startsWith(QStringLiteral("Backup_ShareList_")));
    }

    void test_createBackup_keepsMaxFiveBackups()
    {
        const QString dbPath = m_tempDir.path() + QStringLiteral("/RotationTest.db");
        { QFile f(dbPath); if (f.open(QIODevice::WriteOnly)) { f.write(QByteArray(256, 'X')); } }

        // Pre-create 5 old backup files with ascending timestamps
        for (int i = 1; i <= 5; ++i) {
            const QString name = QStringLiteral("Backup_RotationTest_2025_01_0%1_00_00_00.db").arg(i);
            QFile old(m_tempDir.path() + QDir::separator() + name);
            if (old.open(QIODevice::WriteOnly)) { old.write(QByteArray(64, 'O')); }
        }

        Database::instance().open(dbPath);
        AppSettings::instance().setPortfolioPath(dbPath);

        MainWindow window;
        QApplication::processEvents();

        // After createBackup() there should be at most 5 backups
        QDir dir(m_tempDir.path());
        dir.setNameFilters({ QStringLiteral("Backup_RotationTest_*.db") });
        const QStringList backups = dir.entryList(QDir::Files);
        QVERIFY(backups.size() <= 5);
    }
};

// Custom main() — siehe Kommentarblock am Dateianfang für die Begründung
// (QLocale vor QApplication, kein Version.h/SPM_VERSION_STRING nötig).
int main(int argc, char* argv[])
{
    QLocale::setDefault(QLocale::German);

    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);

    TestBackupForm t;
    return QTest::qExec(&t, argc, argv);
}

#include "tst_backupform.moc"

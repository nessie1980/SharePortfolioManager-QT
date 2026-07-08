// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// Eigene Test-Executable statt Erweiterung von tst_mainwindow.cpp — analog zu
// tst_buysform/tst_shareeditform (siehe ARCHITECTURE.md, "Neue Forms bekommen
// ihre eigene Test-Executable"). BackupSettingsForm braucht weder Datenbank
// noch MainWindow, ein schlanker eigener Test reicht.
#include <QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QTemporaryDir>

#include "BackupSettingsForm.h"
#include "../../config/AppSettings.h"

/**
 * @brief Tests für BackupSettingsForm und die Backup-Sektion von AppSettings.
 *
 * AppSettings ist ein Singleton — jeder Test, der Werte ändert, stellt am
 * Ende den ursprünglichen Wert wieder her, damit spätere Tests nicht von der
 * Ausführungsreihenfolge abhängen (gleiches Muster wie
 * test_loggerSettings_* / test_soundSettings_* in tst_mainwindow.cpp).
 *
 * Regressionstests für createBackup() selbst (Rotation, Präfix-Änderung,
 * mkpath() bei konfiguriertem Verzeichnis, Enable/Disable) bleiben bewusst
 * in tst_mainwindow.cpp (TestBackupForm) — createBackup() ist eine private
 * Methode von MainWindow und braucht dessen volle Konstruktion inkl. Backup-
 * Erstellung beim Start.
 */
class TestBackupSettingsForm : public QObject
{
    Q_OBJECT

private slots:

    // ─────────────────────────────────────────────────────────────────────
    // AppSettings — Backup-Sektion: reiner Speichern/Laden-Roundtrip
    // ─────────────────────────────────────────────────────────────────────

    void test_backupSettings_saveEnabled()
    {
        const bool original = AppSettings::instance().backupEnabled();
        AppSettings::instance().setBackupEnabled(false);
        QVERIFY(!AppSettings::instance().backupEnabled());
        AppSettings::instance().setBackupEnabled(original);
    }

    void test_backupSettings_saveMaxCount()
    {
        const int original = AppSettings::instance().backupMaxCount();
        AppSettings::instance().setBackupMaxCount(10);
        QCOMPARE(AppSettings::instance().backupMaxCount(), 10);
        AppSettings::instance().setBackupMaxCount(original);
    }

    void test_backupSettings_saveNamePrefix()
    {
        const QString original = AppSettings::instance().backupNamePrefix();
        AppSettings::instance().setBackupNamePrefix(QStringLiteral("Sicherung"));
        QCOMPARE(AppSettings::instance().backupNamePrefix(), QStringLiteral("Sicherung"));
        AppSettings::instance().setBackupNamePrefix(original);
    }

    void test_backupSettings_saveDateFormat()
    {
        const QString original = AppSettings::instance().backupDateFormat();
        AppSettings::instance().setBackupDateFormat(QStringLiteral("dd_MM_yyyy_HH_mm_ss"));
        QCOMPARE(AppSettings::instance().backupDateFormat(), QStringLiteral("dd_MM_yyyy_HH_mm_ss"));
        AppSettings::instance().setBackupDateFormat(original);
    }

    void test_backupSettings_saveDirectory()
    {
        const QString original = AppSettings::instance().backupDirectory();
        AppSettings::instance().setBackupDirectory(QStringLiteral("/tmp/my-backups"));
        QCOMPARE(AppSettings::instance().backupDirectory(), QStringLiteral("/tmp/my-backups"));
        AppSettings::instance().setBackupDirectory(original);
    }

    // ─────────────────────────────────────────────────────────────────────
    // Dialog — Konstruktion & Laden
    // ─────────────────────────────────────────────────────────────────────

    void test_dialog_constructsWithoutCrash()
    {
        BackupSettingsForm dialog;
        QVERIFY(true); // reaching this line means construction didn't crash
    }

    void test_dialog_loadSettings_populatesEnabledCheckbox()
    {
        const bool original = AppSettings::instance().backupEnabled();
        AppSettings::instance().setBackupEnabled(true);

        BackupSettingsForm dialog;
        auto* chk = dialog.findChild<QCheckBox*>(QStringLiteral("chkBackupEnabled"));
        if (!chk) QFAIL("chkBackupEnabled not found");
        QVERIFY(chk->isChecked());

        AppSettings::instance().setBackupEnabled(original);
    }

    void test_dialog_loadSettings_populatesPrefixAndDateFormat()
    {
        const QString origPrefix = AppSettings::instance().backupNamePrefix();
        const QString origFormat = AppSettings::instance().backupDateFormat();
        AppSettings::instance().setBackupNamePrefix(QStringLiteral("MeinPraefix"));
        AppSettings::instance().setBackupDateFormat(QStringLiteral("dd_MM_yyyy"));

        BackupSettingsForm dialog;
        auto* editPrefix = dialog.findChild<QLineEdit*>(QStringLiteral("editNamePrefix"));
        auto* editFormat = dialog.findChild<QLineEdit*>(QStringLiteral("editDateFormat"));
        if (!editPrefix) QFAIL("editNamePrefix not found");
        if (!editFormat) QFAIL("editDateFormat not found");
        QCOMPARE(editPrefix->text(), QStringLiteral("MeinPraefix"));
        QCOMPARE(editFormat->text(), QStringLiteral("dd_MM_yyyy"));

        AppSettings::instance().setBackupNamePrefix(origPrefix);
        AppSettings::instance().setBackupDateFormat(origFormat);
    }

    void test_dialog_loadSettings_populatesMaxCountFromKnownValue()
    {
        const int original = AppSettings::instance().backupMaxCount();
        AppSettings::instance().setBackupMaxCount(10); // in der vordefinierten Liste enthalten

        BackupSettingsForm dialog;
        auto* cmb = dialog.findChild<QComboBox*>(QStringLiteral("cmbMaxCount"));
        if (!cmb) QFAIL("cmbMaxCount not found");
        QCOMPARE(cmb->currentText().toInt(), 10);

        AppSettings::instance().setBackupMaxCount(original);
    }

    void test_dialog_loadSettings_populatesMaxCountFromCustomValue()
    {
        // Wert außerhalb der vordefinierten Liste {1,3,5,10,20,50} — testet
        // den setCurrentText()-Fallback in loadSettings() für editierbare
        // Comboboxen mit freiem Wert.
        const int original = AppSettings::instance().backupMaxCount();
        AppSettings::instance().setBackupMaxCount(7);

        BackupSettingsForm dialog;
        auto* cmb = dialog.findChild<QComboBox*>(QStringLiteral("cmbMaxCount"));
        if (!cmb) QFAIL("cmbMaxCount not found");
        QCOMPARE(cmb->currentText().toInt(), 7);

        AppSettings::instance().setBackupMaxCount(original);
    }

    // ─────────────────────────────────────────────────────────────────────
    // Dialog — Speichern
    // ─────────────────────────────────────────────────────────────────────

    void test_dialog_save_persistsAllFieldsToAppSettings()
    {
        const bool    origEnabled = AppSettings::instance().backupEnabled();
        const int     origMax     = AppSettings::instance().backupMaxCount();
        const QString origPrefix  = AppSettings::instance().backupNamePrefix();
        const QString origFormat  = AppSettings::instance().backupDateFormat();
        const QString origDir     = AppSettings::instance().backupDirectory();

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        {
            BackupSettingsForm dialog;

            auto* chk = dialog.findChild<QCheckBox*>(QStringLiteral("chkBackupEnabled"));
            auto* cmb = dialog.findChild<QComboBox*>(QStringLiteral("cmbMaxCount"));
            auto* editPrefix = dialog.findChild<QLineEdit*>(QStringLiteral("editNamePrefix"));
            auto* editFormat = dialog.findChild<QLineEdit*>(QStringLiteral("editDateFormat"));
            auto* editDir    = dialog.findChild<QLineEdit*>(QStringLiteral("editDirectory"));
            auto* btnSave    = dialog.findChild<QPushButton*>(QStringLiteral("btnSave"));
            if (!chk || !cmb || !editPrefix || !editFormat || !editDir || !btnSave)
                QFAIL("Expected widget not found");

            chk->setChecked(false);
            cmb->setCurrentText(QStringLiteral("12"));
            editPrefix->setText(QStringLiteral("Sicherung"));
            editFormat->setText(QStringLiteral("dd_MM_yyyy_HH_mm_ss"));
            editDir->setText(tempDir.path());

            btnSave->click();
        }

        QCOMPARE(AppSettings::instance().backupEnabled(), false);
        QCOMPARE(AppSettings::instance().backupMaxCount(), 12);
        QCOMPARE(AppSettings::instance().backupNamePrefix(), QStringLiteral("Sicherung"));
        QCOMPARE(AppSettings::instance().backupDateFormat(), QStringLiteral("dd_MM_yyyy_HH_mm_ss"));
        QCOMPARE(AppSettings::instance().backupDirectory(), tempDir.path());

        AppSettings::instance().setBackupEnabled(origEnabled);
        AppSettings::instance().setBackupMaxCount(origMax);
        AppSettings::instance().setBackupNamePrefix(origPrefix);
        AppSettings::instance().setBackupDateFormat(origFormat);
        AppSettings::instance().setBackupDirectory(origDir);
    }

    void test_dialog_save_emptyPrefixFallsBackToDefault()
    {
        const QString origPrefix = AppSettings::instance().backupNamePrefix();

        {
            BackupSettingsForm dialog;
            auto* editPrefix = dialog.findChild<QLineEdit*>(QStringLiteral("editNamePrefix"));
            auto* btnSave    = dialog.findChild<QPushButton*>(QStringLiteral("btnSave"));
            if (!editPrefix || !btnSave) QFAIL("Expected widget not found");

            editPrefix->setText(QStringLiteral("   ")); // nur Leerzeichen → trimmt zu leer
            btnSave->click();
        }

        QCOMPARE(AppSettings::instance().backupNamePrefix(), QStringLiteral("Backup"));
        AppSettings::instance().setBackupNamePrefix(origPrefix);
    }

    void test_dialog_save_emptyDateFormatFallsBackToDefault()
    {
        const QString origFormat = AppSettings::instance().backupDateFormat();

        {
            BackupSettingsForm dialog;
            auto* editFormat = dialog.findChild<QLineEdit*>(QStringLiteral("editDateFormat"));
            auto* btnSave    = dialog.findChild<QPushButton*>(QStringLiteral("btnSave"));
            if (!editFormat || !btnSave) QFAIL("Expected widget not found");

            editFormat->setText(QString());
            btnSave->click();
        }

        QCOMPARE(AppSettings::instance().backupDateFormat(), QStringLiteral("yyyy_MM_dd_HH_mm_ss"));
        AppSettings::instance().setBackupDateFormat(origFormat);
    }

    void test_dialog_save_invalidMaxCountFallsBackToFive()
    {
        // currentText().toInt() liefert bei nicht-numerischem Text 0 mit
        // ok == false — saveSettings() muss das auf den Default (5) abfangen,
        // nicht auf 0 speichern (0 Backups würde jede Rotation ins Leere laufen
        // lassen, siehe qMax(1, ...) in MainWindow::createBackup()).
        const int original = AppSettings::instance().backupMaxCount();

        {
            BackupSettingsForm dialog;
            auto* cmb     = dialog.findChild<QComboBox*>(QStringLiteral("cmbMaxCount"));
            auto* btnSave = dialog.findChild<QPushButton*>(QStringLiteral("btnSave"));
            if (!cmb || !btnSave) QFAIL("Expected widget not found");

            cmb->setCurrentText(QStringLiteral("abc"));
            btnSave->click();
        }

        QCOMPARE(AppSettings::instance().backupMaxCount(), 5);
        AppSettings::instance().setBackupMaxCount(original);
    }

    void test_dialog_cancel_doesNotPersistChanges()
    {
        const bool original = AppSettings::instance().backupEnabled();
        AppSettings::instance().setBackupEnabled(true);

        {
            BackupSettingsForm dialog;
            auto* chk       = dialog.findChild<QCheckBox*>(QStringLiteral("chkBackupEnabled"));
            auto* btnCancel = dialog.findChild<QPushButton*>(QStringLiteral("btnCancel"));
            if (!chk || !btnCancel) QFAIL("Expected widget not found");

            chk->setChecked(false); // Änderung im Dialog, aber nicht gespeichert
            btnCancel->click();
        }

        // Abbrechen darf AppSettings nicht verändert haben
        QVERIFY(AppSettings::instance().backupEnabled());
        AppSettings::instance().setBackupEnabled(original);
    }

    // ─────────────────────────────────────────────────────────────────────
    // Dateinamen-Vorschau
    // ─────────────────────────────────────────────────────────────────────

    void test_preview_updatesOnPrefixChange()
    {
        BackupSettingsForm dialog;
        auto* editPrefix = dialog.findChild<QLineEdit*>(QStringLiteral("editNamePrefix"));
        auto* lblPreview = dialog.findChild<QLabel*>(QStringLiteral("lblPreview"));
        if (!editPrefix || !lblPreview) QFAIL("Expected widget not found");

        editPrefix->setText(QStringLiteral("Sicherung"));
        QVERIFY(lblPreview->text().startsWith(QStringLiteral("Sicherung_")));
    }

    void test_preview_containsPortfolioPlaceholder()
    {
        // "<Portfolioname>" muss als erkennbarer Platzhalter erscheinen, nicht
        // als konkreter Beispielname — sonst wirkt er wie Teil des Präfix
        // (siehe Nutzer-Rückmeldung 08.07.2026).
        BackupSettingsForm dialog;
        auto* lblPreview = dialog.findChild<QLabel*>(QStringLiteral("lblPreview"));
        if (!lblPreview) QFAIL("lblPreview not found");
        QVERIFY(lblPreview->text().contains(QStringLiteral("<Portfolioname>")));
    }

    void test_preview_emptyPrefixShowsDefaultInPreview()
    {
        BackupSettingsForm dialog;
        auto* editPrefix = dialog.findChild<QLineEdit*>(QStringLiteral("editNamePrefix"));
        auto* lblPreview = dialog.findChild<QLabel*>(QStringLiteral("lblPreview"));
        if (!editPrefix || !lblPreview) QFAIL("Expected widget not found");

        editPrefix->setText(QString());
        QVERIFY(lblPreview->text().startsWith(QStringLiteral("Backup_")));
    }
};

QTEST_MAIN(TestBackupSettingsForm)
#include "tst_backupsettingsform.moc"

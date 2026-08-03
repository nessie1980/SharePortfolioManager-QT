// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include <QtTest>
#include <QCheckBox>
#include <QPushButton>

#include "../../app/forms/TraySettingsForm/TraySettingsForm.h"
#include "../../app/config/AppSettings.h"

// ─────────────────────────────────────────────────────────────────────────────
// tst_traysettingsform — TraySettingsForm (einzelner QDialog, kein
// IView/IModel/Presenter-Triple, analog BackupSettingsForm/SoundSettingsForm).
// Eigene Executable statt Erweiterung von tst_mainwindow.cpp (03.08.2026,
// gleiche Begründung wie tst_backupsettingsform) — braucht weder Datenbank
// noch MainWindow, daher nur AppSettings + IconProvider als
// Compile-Abhängigkeiten.
//
// QDialog::exec() wird hier — wie überall sonst in diesem Projekt — nicht
// aufgerufen; stattdessen werden Checkbox/Buttons direkt über findChild()
// angesprochen (Guard-Clause-Muster statt echtem exec()).
// ─────────────────────────────────────────────────────────────────────────────

class TestTraySettingsForm : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        // Jeder Test startet mit deaktivierter Option, unabhängig vom
        // Zustand vorheriger Tests (AppSettings ist ein Singleton).
        AppSettings::instance().setTrayOnMinimizeEnabled(false);
    }

    void test_loadSettings_disabled_checkboxUnchecked()
    {
        TraySettingsForm dialog;
        auto* chk = dialog.findChild<QCheckBox*>(QStringLiteral("chkTrayOnMinimizeEnabled"));
        if (!chk) QFAIL("chkTrayOnMinimizeEnabled not found");
        QVERIFY(!chk->isChecked());
    }

    void test_loadSettings_enabled_checkboxChecked()
    {
        AppSettings::instance().setTrayOnMinimizeEnabled(true);

        TraySettingsForm dialog;
        auto* chk = dialog.findChild<QCheckBox*>(QStringLiteral("chkTrayOnMinimizeEnabled"));
        if (!chk) QFAIL("chkTrayOnMinimizeEnabled not found");
        QVERIFY(chk->isChecked());
    }

    void test_save_checkedThenSave_persistsEnabled()
    {
        TraySettingsForm dialog;
        auto* chk = dialog.findChild<QCheckBox*>(QStringLiteral("chkTrayOnMinimizeEnabled"));
        auto* btnSave = dialog.findChild<QPushButton*>(QStringLiteral("btnSave"));
        if (!chk || !btnSave) QFAIL("Expected widget not found");

        chk->setChecked(true);
        btnSave->click();

        QVERIFY(AppSettings::instance().trayOnMinimizeEnabled());
    }

    void test_save_uncheckedThenSave_persistsDisabled()
    {
        AppSettings::instance().setTrayOnMinimizeEnabled(true);

        TraySettingsForm dialog;
        auto* chk = dialog.findChild<QCheckBox*>(QStringLiteral("chkTrayOnMinimizeEnabled"));
        auto* btnSave = dialog.findChild<QPushButton*>(QStringLiteral("btnSave"));
        if (!chk || !btnSave) QFAIL("Expected widget not found");

        chk->setChecked(false);
        btnSave->click();

        QVERIFY(!AppSettings::instance().trayOnMinimizeEnabled());
    }

    void test_cancel_doesNotPersistChange()
    {
        TraySettingsForm dialog;
        auto* chk = dialog.findChild<QCheckBox*>(QStringLiteral("chkTrayOnMinimizeEnabled"));
        auto* btnCancel = dialog.findChild<QPushButton*>(QStringLiteral("btnCancel"));
        if (!chk || !btnCancel) QFAIL("Expected widget not found");

        chk->setChecked(true);
        btnCancel->click();

        QVERIFY(!AppSettings::instance().trayOnMinimizeEnabled());
    }
};

QTEST_MAIN(TestTraySettingsForm)
#include "tst_traysettingsform.moc"

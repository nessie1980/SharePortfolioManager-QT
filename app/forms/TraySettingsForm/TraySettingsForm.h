// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QDialog>
#include <QCheckBox>
#include <QPushButton>

/**
 * @brief Dialog for configuring the minimize-to-tray behavior.
 *
 * Allows the user to configure whether minimizing the main window moves it
 * to the taskbar as usual, or hides it and shows an icon in the system tray
 * instead (AppSettings::trayOnMinimizeEnabled(), read by
 * MainWindow::shouldMinimizeToTray() / MainWindow::changeEvent()).
 *
 * Follows the same lightweight single-QDialog pattern as
 * BackupSettingsForm/SoundSettingsForm (no separate IView/IModel/Presenter
 * triad — appropriate for a pure settings dialog with no independent
 * business logic of its own).
 */
class TraySettingsForm : public QDialog
{
    Q_OBJECT

public:
    explicit TraySettingsForm(QWidget* parent = nullptr);

private slots:
    void onSave();

private:
    void setupUi();
    void loadSettings();
    void saveSettings();

    // ── Tray beim Minimieren ─────────────────────────────────────────────
    QCheckBox*   m_chkEnabled = nullptr;

    // ── Buttons ───────────────────────────────────────────────────────────
    QPushButton* m_btnSave    = nullptr;
    QPushButton* m_btnCancel  = nullptr;
};

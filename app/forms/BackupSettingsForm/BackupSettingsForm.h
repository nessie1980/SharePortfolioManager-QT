// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QDialog>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>

/**
 * @brief Dialog for configuring backup settings.
 *
 * Allows the user to configure:
 * - Whether a backup is created automatically when a portfolio is opened
 *   (MainWindow::createBackup(), called at startup and after "Öffnen")
 * - Maximum number of backups to keep (oldest are rotated out)
 * - Name scheme of backup files: prefix + Qt date format for the timestamp
 * - Target directory for backups (empty = same folder as the portfolio file)
 *
 * All changes are written to AppSettings on Save and read back by
 * MainWindow::createBackup() on the next backup run. Follows the same
 * lightweight single-QDialog pattern as LoggerSettingsForm/SoundSettingsForm
 * (no separate IView/IModel/Presenter triad — appropriate for a pure
 * settings dialog with no independent business logic of its own).
 */
class BackupSettingsForm : public QDialog
{
    Q_OBJECT

public:
    explicit BackupSettingsForm(QWidget* parent = nullptr);

private slots:
    void onSave();
    void onBrowseDirectory();

private:
    void setupUi();
    void loadSettings();
    void saveSettings();
    void updateNamePreview();

    // ── Backup aktivieren ────────────────────────────────────────────────
    QCheckBox*   m_chkEnabled     = nullptr;

    // ── Max. Anzahl Backups ──────────────────────────────────────────────
    QComboBox*   m_cmbMaxCount    = nullptr;

    // ── Namensschema ─────────────────────────────────────────────────────
    QLineEdit*   m_editNamePrefix = nullptr;
    QLineEdit*   m_editDateFormat = nullptr;
    QLabel*      m_lblPreview     = nullptr;

    // ── Backup-Verzeichnis ───────────────────────────────────────────────
    QLineEdit*   m_editDirectory  = nullptr;
    QPushButton* m_btnBrowse      = nullptr;

    // ── Buttons ───────────────────────────────────────────────────────────
    QPushButton* m_btnSave        = nullptr;
    QPushButton* m_btnCancel      = nullptr;
};

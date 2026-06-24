// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>

/**
 * @brief Dialog for configuring sound settings.
 *
 * Allows the user to configure:
 * - Sound played at the end of a share update (enabled + file selection)
 * - Sound played when an error occurs (enabled + file selection)
 *
 * All changes are written to AppSettings on Save.
 */
class SoundSettingsForm : public QDialog
{
    Q_OBJECT

public:
    explicit SoundSettingsForm(QWidget* parent = nullptr);

private slots:
    void onSave();

private:
    void setupUi();
    void loadSettings();
    void saveSettings();

    // ── Sound für Aktualisierung ──────────────────────────────────────────
    QComboBox*   m_cmbUpdateSound    = nullptr;
    QCheckBox*   m_chkUpdateEnabled  = nullptr;

    // ── Sound für Fehler ──────────────────────────────────────────────────
    QComboBox*   m_cmbErrorSound     = nullptr;
    QCheckBox*   m_chkErrorEnabled   = nullptr;

    // ── Buttons ───────────────────────────────────────────────────────────
    QPushButton* m_btnSave           = nullptr;
    QPushButton* m_btnCancel         = nullptr;

    // Available sound files (loaded from Qt resources)
    /**
     * @brief Scan the sounds/ directory next to the executable for WAV files.
     *
     * Returns all .wav files found sorted by name. Falls back to the two
     * built-in resource files if the directory is missing or empty.
     * @return List of WAV filenames (without path).
     */
    static QStringList scanSoundFiles();
};

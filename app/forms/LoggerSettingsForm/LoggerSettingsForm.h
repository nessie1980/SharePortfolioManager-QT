// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QDialog>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QColor>
#include <QList>

/**
 * @brief Dialog for configuring logger settings.
 *
 * Allows the user to configure:
 * - GUI log entry count
 * - File logging (enabled, stored files count, cleanup at startup)
 * - Log components (bitmask)
 * - Log levels (bitmask)
 * - Log level colors (one color per level)
 *
 * All changes are written to AppSettings on Save.
 */
class LoggerSettingsForm : public QDialog
{
    Q_OBJECT

public:
    explicit LoggerSettingsForm(QWidget* parent = nullptr);

private slots:
    void onSave();
    void onColorChanged(int index);

private:
    void setupUi();
    void loadSettings();
    void saveSettings();

    /**
     * @brief Create a color ComboBox pre-populated with named Qt colors.
     * @param currentColor  The color to pre-select.
     * @return Configured QComboBox.
     */
    QComboBox* createColorComboBox(const QColor& currentColor);

    /**
     * @brief Update the background color of a color ComboBox to preview the selection.
     * @param comboBox  The ComboBox to update.
     */
    void updateColorPreview(QComboBox* comboBox);

    // ── GUI-Log-Eintraggröße ──────────────────────────────────────────────
    QComboBox*   m_cmbGuiEntries      = nullptr;

    // ── In Datei schreiben ────────────────────────────────────────────────
    QCheckBox*   m_chkLogToFile       = nullptr;

    // ── Vorgehaltene Log-Dateien ──────────────────────────────────────────
    QComboBox*   m_cmbStoredFiles     = nullptr;
    QPushButton* m_btnCleanup         = nullptr;

    // ── Dateien beim Start löschen ────────────────────────────────────────
    QCheckBox*   m_chkCleanupAtStart  = nullptr;

    // ── Log-Komponenten ───────────────────────────────────────────────────
    QCheckBox*   m_chkCompApp         = nullptr; ///< Bit 0: Application
    QCheckBox*   m_chkCompParser      = nullptr; ///< Bit 1: Parser
    QCheckBox*   m_chkCompLanguage    = nullptr; ///< Bit 2: LanguageHandler

    // ── Log-Level ─────────────────────────────────────────────────────────
    QCheckBox*   m_chkLvlStart        = nullptr; ///< Bit 0
    QCheckBox*   m_chkLvlInfo         = nullptr; ///< Bit 1
    QCheckBox*   m_chkLvlWarning      = nullptr; ///< Bit 2
    QCheckBox*   m_chkLvlError        = nullptr; ///< Bit 3
    QCheckBox*   m_chkLvlFatal        = nullptr; ///< Bit 4

    // ── Log-Level-Farben ──────────────────────────────────────────────────
    QComboBox*   m_cmbColorStart      = nullptr; ///< Index 0
    QComboBox*   m_cmbColorInfo       = nullptr; ///< Index 1
    QComboBox*   m_cmbColorWarning    = nullptr; ///< Index 2
    QComboBox*   m_cmbColorError      = nullptr; ///< Index 3
    QComboBox*   m_cmbColorFatal      = nullptr; ///< Index 4
    QComboBox*   m_cmbColorSuccess    = nullptr; ///< Index 5

    // ── Buttons ───────────────────────────────────────────────────────────
    QPushButton* m_btnSave            = nullptr;
    QPushButton* m_btnCancel          = nullptr;

    // Available color names for the color ComboBoxes
    static const QStringList k_colorNames;
};

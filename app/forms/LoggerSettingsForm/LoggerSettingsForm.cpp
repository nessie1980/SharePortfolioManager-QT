// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "LoggerSettingsForm.h"
#include "../../config/AppSettings.h"
#include "../../IconProvider.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QDir>
#include <QStandardPaths>
#include "../OwnMessageBoxForm/OwnMessageBox.h"
#include <QDebug>

// ── Static color names ────────────────────────────────────────────────────────

const QStringList LoggerSettingsForm::k_colorNames = {
    QStringLiteral("Black"),
    QStringLiteral("White"),
    QStringLiteral("Gray"),
    QStringLiteral("Silver"),
    QStringLiteral("Red"),
    QStringLiteral("DarkRed"),
    QStringLiteral("OrangeRed"),
    QStringLiteral("Orange"),
    QStringLiteral("Yellow"),
    QStringLiteral("Green"),
    QStringLiteral("DarkGreen"),
    QStringLiteral("Lime"),
    QStringLiteral("Blue"),
    QStringLiteral("DarkBlue"),
    QStringLiteral("Cyan"),
    QStringLiteral("Magenta"),
    QStringLiteral("Purple"),
    QStringLiteral("#e0e0e0"),
    QStringLiteral("#44ff44"),
    QStringLiteral("#ffa500"),
    QStringLiteral("#ff4444"),
    QStringLiteral("#ff0000"),
};

// ── Constructor ───────────────────────────────────────────────────────────────

LoggerSettingsForm::LoggerSettingsForm(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Logger"));
    setMinimumWidth(600);
    setModal(true);

    setupUi();
    loadSettings();
}

// ── setupUi ───────────────────────────────────────────────────────────────────

QComboBox* LoggerSettingsForm::createColorComboBox(const QColor& currentColor)
{
    auto* cmb = new QComboBox(this);
    int selectedIndex = 0;

    for (int idx = 0; idx < k_colorNames.size(); ++idx) {
        const QColor color(k_colorNames.at(idx));
        cmb->addItem(k_colorNames.at(idx));

        // Color swatch as item icon
        QPixmap pixmap(16, 16);
        pixmap.fill(color);
        cmb->setItemIcon(idx, QIcon(pixmap));

        if (color.name() == currentColor.name())
            selectedIndex = idx;
    }

    cmb->setCurrentIndex(selectedIndex);
    updateColorPreview(cmb);

    connect(cmb, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LoggerSettingsForm::onColorChanged);

    return cmb;
}

void LoggerSettingsForm::updateColorPreview(QComboBox* comboBox)
{
    const QColor color(comboBox->currentText());
    if (color.isValid()) {
        const QString textColor = color.lightness() > 128
            ? QStringLiteral("black") : QStringLiteral("white");
        comboBox->setStyleSheet(
            QStringLiteral("QComboBox { background-color: %1; color: %2; }")
                .arg(color.name(), textColor));
    }
}

void LoggerSettingsForm::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    // ── Row 1: GUI entries + Log to file ─────────────────────────────────
    auto* row1Layout = new QHBoxLayout();

    // GUI-Log-Eintraggröße
    auto* grpGui = new QGroupBox(tr("  GUI- Log- Eintraggröße"), this);
    auto* guiLayout = new QHBoxLayout(grpGui);
    guiLayout->addStretch();
    guiLayout->addWidget(new QLabel(tr("Anzahl an Zeilen:"), grpGui));
    m_cmbGuiEntries = new QComboBox(grpGui);
    for (int val : {10, 20, 30, 50, 100, 200, 500})
        m_cmbGuiEntries->addItem(QString::number(val), val);
    guiLayout->addWidget(m_cmbGuiEntries);
    row1Layout->addWidget(grpGui, 3);

    // In Datei schreiben
    auto* grpFile = new QGroupBox(tr("  In Datei schreiben"), this);
    auto* fileLayout = new QHBoxLayout(grpFile);
    m_chkLogToFile = new QCheckBox(tr("aktiviert"), grpFile);
    fileLayout->addWidget(m_chkLogToFile);
    row1Layout->addWidget(grpFile, 1);

    mainLayout->addLayout(row1Layout);

    // ── Row 2: Stored files + Cleanup at startup ──────────────────────────
    auto* row2Layout = new QHBoxLayout();

    // Vorgehaltene Log-Dateien
    auto* grpStored = new QGroupBox(tr("  Vorgehaltene Log- Dateien"), this);
    auto* storedLayout = new QHBoxLayout(grpStored);
    storedLayout->addStretch();
    storedLayout->addWidget(new QLabel(tr("Anzahl an Dateien:"), grpStored));
    m_cmbStoredFiles = new QComboBox(grpStored);
    for (int val : {5, 10, 20, 30, 50})
        m_cmbStoredFiles->addItem(QString::number(val), val);
    storedLayout->addWidget(m_cmbStoredFiles);
    m_btnCleanup = new QPushButton(tr("Aufräumen"), grpStored);
    storedLayout->addWidget(m_btnCleanup);
    row2Layout->addWidget(grpStored, 3);

    // Dateien beim Start löschen
    auto* grpCleanup = new QGroupBox(tr("  Dateien beim Start löschen"), this);
    auto* cleanupLayout = new QHBoxLayout(grpCleanup);
    m_chkCleanupAtStart = new QCheckBox(tr("aktiviert"), grpCleanup);
    cleanupLayout->addWidget(m_chkCleanupAtStart);
    row2Layout->addWidget(grpCleanup, 1);

    mainLayout->addLayout(row2Layout);

    // ── Row 3: Components + Levels + Colors ───────────────────────────────
    auto* row3Layout = new QHBoxLayout();

    // Log-Komponenten
    auto* grpComp = new QGroupBox(tr("  Log- Komponenten"), this);
    auto* compLayout = new QVBoxLayout(grpComp);
    m_chkCompApp      = new QCheckBox(tr("Programm"),        grpComp);
    m_chkCompParser   = new QCheckBox(tr("Parser"),          grpComp);
    m_chkCompLanguage = new QCheckBox(tr("LanguageHandler"), grpComp);
    compLayout->addWidget(m_chkCompApp);
    compLayout->addWidget(m_chkCompParser);
    compLayout->addWidget(m_chkCompLanguage);
    compLayout->addStretch();
    row3Layout->addWidget(grpComp);

    // Log-Level
    auto* grpLevel = new QGroupBox(tr("  Log- Level"), this);
    auto* levelLayout = new QVBoxLayout(grpLevel);
    m_chkLvlStart   = new QCheckBox(tr("Start"),         grpLevel);
    m_chkLvlInfo    = new QCheckBox(tr("Info"),          grpLevel);
    m_chkLvlWarning = new QCheckBox(tr("Warnung"),       grpLevel);
    m_chkLvlError   = new QCheckBox(tr("Fehler"),        grpLevel);
    m_chkLvlFatal   = new QCheckBox(tr("Fataler Fehler"),grpLevel);
    levelLayout->addWidget(m_chkLvlStart);
    levelLayout->addWidget(m_chkLvlInfo);
    levelLayout->addWidget(m_chkLvlWarning);
    levelLayout->addWidget(m_chkLvlError);
    levelLayout->addWidget(m_chkLvlFatal);
    levelLayout->addStretch();
    row3Layout->addWidget(grpLevel);

    // Log-Level-Farben
    auto* grpColors = new QGroupBox(tr("  Log- Level- Farben"), this);
    auto* colorsLayout = new QGridLayout(grpColors);
    colorsLayout->setColumnStretch(0, 1);
    colorsLayout->setColumnStretch(1, 2);

    const auto& colors = AppSettings::instance().logColors();

    auto addColorRow = [&](int row, const QString& label, QComboBox*& cmb, int colorIdx) {
        auto* lbl = new QLabel(label, grpColors);
        lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        cmb = createColorComboBox(colorIdx < colors.size()
                                  ? colors.at(colorIdx)
                                  : QColor(Qt::black));
        colorsLayout->addWidget(lbl, row, 0);
        colorsLayout->addWidget(cmb, row, 1);
    };

    addColorRow(0, tr("Start"),         m_cmbColorStart,   0);
    addColorRow(1, tr("Info"),          m_cmbColorInfo,    1);
    addColorRow(2, tr("Warnung"),       m_cmbColorWarning, 2);
    addColorRow(3, tr("Fehler"),        m_cmbColorError,   3);
    addColorRow(4, tr("Fataler Fehler"),m_cmbColorFatal,   4);
    addColorRow(5, tr("Erfolg"),        m_cmbColorSuccess, 5);

    row3Layout->addWidget(grpColors);
    mainLayout->addLayout(row3Layout);

    // ── Buttons ───────────────────────────────────────────────────────────
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    m_btnSave   = new QPushButton(
        IconProvider::icon(IconProvider::ButtonSave), tr("Speichern"), this);
    m_btnCancel = new QPushButton(
        IconProvider::icon(IconProvider::ButtonCancel), tr("Abbrechen"), this);
    m_btnSave->setDefault(true);
    btnLayout->addWidget(m_btnSave);
    btnLayout->addWidget(m_btnCancel);
    mainLayout->addLayout(btnLayout);

    connect(m_btnSave,   &QPushButton::clicked, this, &LoggerSettingsForm::onSave);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_btnCleanup,&QPushButton::clicked, this, [this]() {
        // Cleanup stub — to be connected to Logger later
        OwnMessageBox::information(this, tr("Aufräumen"),
            tr("Log-Dateien wurden aufgeräumt."));
    });
}

// ── loadSettings ──────────────────────────────────────────────────────────────

void LoggerSettingsForm::loadSettings()
{
    const auto& s = AppSettings::instance();

    // GUI entries
    for (int i = 0; i < m_cmbGuiEntries->count(); ++i) {
        if (m_cmbGuiEntries->itemData(i).toInt() == s.logGuiEntries()) {
            m_cmbGuiEntries->setCurrentIndex(i);
            break;
        }
    }

    // File logging
    m_chkLogToFile->setChecked(s.logToFile());

    // Stored files
    for (int i = 0; i < m_cmbStoredFiles->count(); ++i) {
        if (m_cmbStoredFiles->itemData(i).toInt() == s.logStoredFiles()) {
            m_cmbStoredFiles->setCurrentIndex(i);
            break;
        }
    }

    // Cleanup at startup
    m_chkCleanupAtStart->setChecked(s.logCleanupAtStartup());

    // Components (bitmask)
    m_chkCompApp->setChecked(s.logComponents() & (1 << 0));
    m_chkCompParser->setChecked(s.logComponents() & (1 << 1));
    m_chkCompLanguage->setChecked(s.logComponents() & (1 << 2));

    // Levels (bitmask)
    m_chkLvlStart->setChecked(s.logLevels() & (1 << 0));
    m_chkLvlInfo->setChecked(s.logLevels() & (1 << 1));
    m_chkLvlWarning->setChecked(s.logLevels() & (1 << 2));
    m_chkLvlError->setChecked(s.logLevels() & (1 << 3));
    m_chkLvlFatal->setChecked(s.logLevels() & (1 << 4));
}

// ── onColorChanged ────────────────────────────────────────────────────────────

void LoggerSettingsForm::onColorChanged(int /*index*/)
{
    // Update color preview for the sender ComboBox
    if (auto* cmb = qobject_cast<QComboBox*>(sender()))
        updateColorPreview(cmb);
}

// ── onSave ────────────────────────────────────────────────────────────────────

void LoggerSettingsForm::onSave()
{
    saveSettings();
    accept();
}

void LoggerSettingsForm::saveSettings()
{
    auto& s = AppSettings::instance();

    // GUI entries
    s.setLogGuiEntries(m_cmbGuiEntries->currentData().toInt());

    // File logging
    s.setLogToFile(m_chkLogToFile->isChecked());

    // Stored files
    s.setLogStoredFiles(m_cmbStoredFiles->currentData().toInt());

    // Cleanup at startup
    s.setLogCleanupAtStartup(m_chkCleanupAtStart->isChecked());

    // Components bitmask
    int components = 0;
    if (m_chkCompApp->isChecked())      components |= (1 << 0);
    if (m_chkCompParser->isChecked())   components |= (1 << 1);
    if (m_chkCompLanguage->isChecked()) components |= (1 << 2);
    s.setLogComponents(components);

    // Levels bitmask
    int levels = 0;
    if (m_chkLvlStart->isChecked())   levels |= (1 << 0);
    if (m_chkLvlInfo->isChecked())    levels |= (1 << 1);
    if (m_chkLvlWarning->isChecked()) levels |= (1 << 2);
    if (m_chkLvlError->isChecked())   levels |= (1 << 3);
    if (m_chkLvlFatal->isChecked())   levels |= (1 << 4);
    s.setLogLevels(levels);

    // Colors
    QList<QColor> colors;
    colors.append(QColor(m_cmbColorStart->currentText()));
    colors.append(QColor(m_cmbColorInfo->currentText()));
    colors.append(QColor(m_cmbColorWarning->currentText()));
    colors.append(QColor(m_cmbColorError->currentText()));
    colors.append(QColor(m_cmbColorFatal->currentText()));
    colors.append(QColor(m_cmbColorSuccess->currentText()));
    s.setLogColors(colors);

    qInfo() << "[LoggerSettingsForm] Settings saved.";
}

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "SoundSettingsForm.h"
#include "../../config/AppSettings.h"
#include "../../IconProvider.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDir>
#include <QCoreApplication>
#include <QDebug>

// ── Constructor ───────────────────────────────────────────────────────────────

SoundSettingsForm::SoundSettingsForm(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Sounds"));
    setMinimumWidth(500);
    setModal(true);

    setupUi();
    loadSettings();
}

// ── scanSoundFiles ────────────────────────────────────────────────────────────

QStringList SoundSettingsForm::scanSoundFiles()
{
    // Look for WAV files in the sounds/ subdirectory next to the executable.
    // Falls back to the built-in resource files if the directory is empty
    // or does not exist.
    const QString soundsDir =
        QCoreApplication::applicationDirPath() + QStringLiteral("/sounds");

    QDir dir(soundsDir);
    QStringList files = dir.entryList(
        QStringList() << QStringLiteral("*.wav") << QStringLiteral("*.WAV"),
        QDir::Files, QDir::Name);

    if (files.isEmpty()) {
        qInfo() << "[SoundSettingsForm] No WAV files found in" << soundsDir
                << "— using built-in sounds.";
        files << QStringLiteral("UpdateFinished.wav")
              << QStringLiteral("Error.wav");
    } else {
        qInfo() << "[SoundSettingsForm] Found" << files.size()
                << "WAV file(s) in" << soundsDir;
    }

    return files;
}

// ── setupUi ───────────────────────────────────────────────────────────────────

void SoundSettingsForm::setupUi()
{
    const QStringList soundFiles = scanSoundFiles();

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    // ── Sound für Aktualisierung ──────────────────────────────────────────
    auto* grpUpdate = new QGroupBox(
        tr("  Sound für das Ende einer Aktien- Aktualisierung"), this);
    auto* updateLayout = new QHBoxLayout(grpUpdate);

    m_cmbUpdateSound = new QComboBox(grpUpdate);
    for (const auto& file : soundFiles)
        m_cmbUpdateSound->addItem(file);
    m_cmbUpdateSound->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_chkUpdateEnabled = new QCheckBox(tr("Aktiv"), grpUpdate);

    updateLayout->addWidget(m_cmbUpdateSound);
    updateLayout->addWidget(m_chkUpdateEnabled);
    mainLayout->addWidget(grpUpdate);

    // ── Sound für Fehler ──────────────────────────────────────────────────
    auto* grpError = new QGroupBox(
        tr("  Sound für das Anzeigen eines Fehlers"), this);
    auto* errorLayout = new QHBoxLayout(grpError);

    m_cmbErrorSound = new QComboBox(grpError);
    for (const auto& file : soundFiles)
        m_cmbErrorSound->addItem(file);
    m_cmbErrorSound->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_chkErrorEnabled = new QCheckBox(tr("Aktiv"), grpError);

    errorLayout->addWidget(m_cmbErrorSound);
    errorLayout->addWidget(m_chkErrorEnabled);
    mainLayout->addWidget(grpError);

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

    connect(m_btnSave,   &QPushButton::clicked, this, &SoundSettingsForm::onSave);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
}

// ── loadSettings ──────────────────────────────────────────────────────────────

void SoundSettingsForm::loadSettings()
{
    const auto& s = AppSettings::instance();

    const int updateIdx = m_cmbUpdateSound->findText(s.soundUpdateFile());
    m_cmbUpdateSound->setCurrentIndex(updateIdx >= 0 ? updateIdx : 0);
    m_chkUpdateEnabled->setChecked(s.soundUpdateEnabled());

    const int errorIdx = m_cmbErrorSound->findText(s.soundErrorFile());
    m_cmbErrorSound->setCurrentIndex(errorIdx >= 0 ? errorIdx : 0);
    m_chkErrorEnabled->setChecked(s.soundErrorEnabled());
}

// ── onSave ────────────────────────────────────────────────────────────────────

void SoundSettingsForm::onSave()
{
    saveSettings();
    accept();
}

void SoundSettingsForm::saveSettings()
{
    auto& s = AppSettings::instance();

    s.setSoundUpdateFile(m_cmbUpdateSound->currentText());
    s.setSoundUpdateEnabled(m_chkUpdateEnabled->isChecked());
    s.setSoundErrorFile(m_cmbErrorSound->currentText());
    s.setSoundErrorEnabled(m_chkErrorEnabled->isChecked());

    qInfo() << "[SoundSettingsForm] Settings saved.";
}

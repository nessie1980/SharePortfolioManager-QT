// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "BackupSettingsForm.h"
#include "../../config/AppSettings.h"
#include "../../IconProvider.h"
#include "../UiConstants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QIntValidator>
#include <QDateTime>
#include <QDir>
#include <QDebug>

// ── Constructor ───────────────────────────────────────────────────────────────

BackupSettingsForm::BackupSettingsForm(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Backup"));
    setMinimumWidth(560);
    setModal(true);

    setupUi();
    loadSettings();
    updateNamePreview();
}

// ── setupUi ───────────────────────────────────────────────────────────────────

void BackupSettingsForm::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    // ── Row 1: Backup aktivieren + Max. Anzahl ─────────────────────────────
    auto* row1Layout = new QHBoxLayout();

    auto* grpEnabled = new QGroupBox(tr("  Backup aktivieren"), this);
    auto* enabledLayout = new QHBoxLayout(grpEnabled);
    m_chkEnabled = new QCheckBox(tr("aktiviert"), grpEnabled);
    m_chkEnabled->setObjectName(QStringLiteral("chkBackupEnabled"));
    enabledLayout->addWidget(m_chkEnabled);
    row1Layout->addWidget(grpEnabled, 1);

    auto* grpMaxCount = new QGroupBox(tr("  Max. Anzahl Backups"), this);
    auto* maxCountLayout = new QHBoxLayout(grpMaxCount);
    maxCountLayout->addStretch();
    maxCountLayout->addWidget(new QLabel(tr("Anzahl:"), grpMaxCount));
    m_cmbMaxCount = new QComboBox(grpMaxCount);
    m_cmbMaxCount->setObjectName(QStringLiteral("cmbMaxCount"));
    m_cmbMaxCount->setEditable(true);
    for (int val : {1, 3, 5, 10, 20, 50})
        m_cmbMaxCount->addItem(QString::number(val), val);
    m_cmbMaxCount->setValidator(new QIntValidator(1, 999, m_cmbMaxCount));
    m_cmbMaxCount->setFixedHeight(UiConstants::kFieldHeight);
    maxCountLayout->addWidget(m_cmbMaxCount);
    row1Layout->addWidget(grpMaxCount, 1);

    mainLayout->addLayout(row1Layout);

    // ── Namensschema ────────────────────────────────────────────────────────
    auto* grpName = new QGroupBox(tr("  Namensschema"), this);
    auto* nameLayout = new QGridLayout(grpName);
    nameLayout->setColumnStretch(1, 1);
    int nameRow = 0;

    auto* lblPrefix = new QLabel(tr("Präfix:"), grpName);
    m_editNamePrefix = new QLineEdit(grpName);
    m_editNamePrefix->setObjectName(QStringLiteral("editNamePrefix"));
    m_editNamePrefix->setFixedHeight(UiConstants::kFieldHeight);
    nameLayout->addWidget(lblPrefix,        nameRow, 0);
    nameLayout->addWidget(m_editNamePrefix, nameRow, 1);
    ++nameRow;

    auto* lblDateFormat = new QLabel(tr("Datumsformat:"), grpName);
    m_editDateFormat = new QLineEdit(grpName);
    m_editDateFormat->setObjectName(QStringLiteral("editDateFormat"));
    m_editDateFormat->setFixedHeight(UiConstants::kFieldHeight);
    nameLayout->addWidget(lblDateFormat,    nameRow, 0);
    nameLayout->addWidget(m_editDateFormat, nameRow, 1);
    ++nameRow;

    auto* lblPreviewCaption = new QLabel(tr("Beispiel:"), grpName);
    m_lblPreview = new QLabel(grpName);
    m_lblPreview->setObjectName(QStringLiteral("lblPreview"));
    m_lblPreview->setStyleSheet(QStringLiteral("color: #a0a0a0; font-style: italic;"));
    nameLayout->addWidget(lblPreviewCaption, nameRow, 0);
    nameLayout->addWidget(m_lblPreview,      nameRow, 1);

    connect(m_editNamePrefix, &QLineEdit::textChanged, this, &BackupSettingsForm::updateNamePreview);
    connect(m_editDateFormat, &QLineEdit::textChanged, this, &BackupSettingsForm::updateNamePreview);

    mainLayout->addWidget(grpName);

    // ── Backup-Verzeichnis ──────────────────────────────────────────────────
    auto* grpDir = new QGroupBox(tr("  Backup-Verzeichnis"), this);
    auto* dirLayout = new QHBoxLayout(grpDir);

    m_editDirectory = new QLineEdit(grpDir);
    m_editDirectory->setObjectName(QStringLiteral("editDirectory"));
    m_editDirectory->setFixedHeight(UiConstants::kFieldHeight);
    m_editDirectory->setPlaceholderText(tr("Leer = gleicher Ordner wie das Portfolio"));
    dirLayout->addWidget(m_editDirectory, 1);

    m_btnBrowse = new QPushButton(
        IconProvider::icon(IconProvider::MenuFolderOpen16), QString(), grpDir);
    m_btnBrowse->setObjectName(QStringLiteral("btnBrowse"));
    m_btnBrowse->setFixedSize(UiConstants::kFieldHeight, UiConstants::kFieldHeight);
    connect(m_btnBrowse, &QPushButton::clicked, this, &BackupSettingsForm::onBrowseDirectory);
    dirLayout->addWidget(m_btnBrowse);

    mainLayout->addWidget(grpDir);

    // ── Buttons ───────────────────────────────────────────────────────────
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    m_btnSave   = new QPushButton(
        IconProvider::icon(IconProvider::ButtonSave), tr("Speichern"), this);
    m_btnSave->setObjectName(QStringLiteral("btnSave"));
    m_btnCancel = new QPushButton(
        IconProvider::icon(IconProvider::ButtonCancel), tr("Abbrechen"), this);
    m_btnCancel->setObjectName(QStringLiteral("btnCancel"));
    m_btnSave->setDefault(true);
    btnLayout->addWidget(m_btnSave);
    btnLayout->addWidget(m_btnCancel);
    mainLayout->addLayout(btnLayout);

    connect(m_btnSave,   &QPushButton::clicked, this, &BackupSettingsForm::onSave);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
}

// ── updateNamePreview ─────────────────────────────────────────────────────────

void BackupSettingsForm::updateNamePreview()
{
    // Leere Eingaben werden — wie beim Speichern (saveSettings()) — durch die
    // Standardwerte ersetzt, damit die Vorschau nie mit einem ungültigen
    // Format hängen bleibt.
    const QString prefix = m_editNamePrefix->text().trimmed().isEmpty()
        ? QStringLiteral("Backup") : m_editNamePrefix->text().trimmed();
    const QString fmt = m_editDateFormat->text().trimmed().isEmpty()
        ? QStringLiteral("yyyy_MM_dd_HH_mm_ss") : m_editDateFormat->text().trimmed();

    const QString timestamp = QDateTime::currentDateTime().toString(fmt);

    // "<Portfolioname>" ist bewusst ein erkennbarer Platzhalter statt eines
    // konkreten Beispiels wie "ShareList" — der tatsächliche Dateiname des
    // Portfolios wird zur Laufzeit von MainWindow::createBackup() eingesetzt
    // und ist hier nicht konfigurierbar. Ohne diese Kennzeichnung sah es wie
    // ein fester Bestandteil des Präfix aus (siehe Nutzer-Rückmeldung 08.07.2026).
    m_lblPreview->setText(
        QStringLiteral("%1_<Portfolioname>_%2.db").arg(prefix, timestamp));
}

// ── onBrowseDirectory ─────────────────────────────────────────────────────────

void BackupSettingsForm::onBrowseDirectory()
{
    const QString startDir = m_editDirectory->text().isEmpty()
        ? QDir::homePath() : m_editDirectory->text();
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Backup-Verzeichnis auswählen"), startDir);
    if (!dir.isEmpty())
        m_editDirectory->setText(dir);
}

// ── loadSettings ──────────────────────────────────────────────────────────────

void BackupSettingsForm::loadSettings()
{
    const auto& s = AppSettings::instance();

    m_chkEnabled->setChecked(s.backupEnabled());

    const int idx = m_cmbMaxCount->findData(s.backupMaxCount());
    if (idx >= 0)
        m_cmbMaxCount->setCurrentIndex(idx);
    else
        m_cmbMaxCount->setCurrentText(QString::number(s.backupMaxCount()));

    m_editNamePrefix->setText(s.backupNamePrefix());
    m_editDateFormat->setText(s.backupDateFormat());
    m_editDirectory->setText(s.backupDirectory());
}

// ── onSave ────────────────────────────────────────────────────────────────────

void BackupSettingsForm::onSave()
{
    saveSettings();
    accept();
}

void BackupSettingsForm::saveSettings()
{
    auto& s = AppSettings::instance();

    s.setBackupEnabled(m_chkEnabled->isChecked());

    bool ok = false;
    const int maxCount = m_cmbMaxCount->currentText().toInt(&ok);
    s.setBackupMaxCount((ok && maxCount > 0) ? maxCount : 5);

    // Leere Eingaben fallen auf die Standardwerte zurück statt einen leeren
    // Präfix bzw. ein leeres (und damit für QDateTime::toString() bedeutungsloses)
    // Datumsformat zu speichern.
    const QString prefix = m_editNamePrefix->text().trimmed();
    s.setBackupNamePrefix(prefix.isEmpty() ? QStringLiteral("Backup") : prefix);

    const QString dateFormat = m_editDateFormat->text().trimmed();
    s.setBackupDateFormat(dateFormat.isEmpty()
        ? QStringLiteral("yyyy_MM_dd_HH_mm_ss") : dateFormat);

    s.setBackupDirectory(m_editDirectory->text().trimmed());

    qInfo() << "[BackupSettingsForm] Settings saved.";
}

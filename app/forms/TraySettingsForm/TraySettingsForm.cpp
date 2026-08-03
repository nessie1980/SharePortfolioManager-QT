// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "TraySettingsForm.h"
#include "../../config/AppSettings.h"
#include "../../IconProvider.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QDebug>

// ── Constructor ───────────────────────────────────────────────────────────────

TraySettingsForm::TraySettingsForm(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Tray"));
    setMinimumWidth(420);
    setModal(true);

    setupUi();
    loadSettings();
}

// ── setupUi ───────────────────────────────────────────────────────────────────

void TraySettingsForm::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    auto* grpEnabled = new QGroupBox(tr("  Minimieren"), this);
    auto* enabledLayout = new QVBoxLayout(grpEnabled);

    m_chkEnabled = new QCheckBox(
        tr("Beim Minimieren in den Infobereich (Tray) legen, statt in die Taskleiste"),
        grpEnabled);
    m_chkEnabled->setObjectName(QStringLiteral("chkTrayOnMinimizeEnabled"));
    enabledLayout->addWidget(m_chkEnabled);

    auto* lblHint = new QLabel(
        tr("Ist diese Option aktiviert, verschwindet das Fenster beim Minimieren "
           "vollständig und ist über ein Symbol im Infobereich wieder erreichbar "
           "(einfacher Klick auf das Symbol stellt das Fenster wieder her). Steht "
           "auf dem System kein Infobereich zur Verfügung, wird weiterhin ganz "
           "normal in die Taskleiste minimiert."),
        grpEnabled);
    lblHint->setWordWrap(true);
    lblHint->setStyleSheet(QStringLiteral("color: #a0a0a0; font-style: italic;"));
    enabledLayout->addWidget(lblHint);

    mainLayout->addWidget(grpEnabled);

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

    connect(m_btnSave,   &QPushButton::clicked, this, &TraySettingsForm::onSave);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
}

// ── loadSettings ──────────────────────────────────────────────────────────────

void TraySettingsForm::loadSettings()
{
    m_chkEnabled->setChecked(AppSettings::instance().trayOnMinimizeEnabled());
}

// ── onSave ────────────────────────────────────────────────────────────────────

void TraySettingsForm::onSave()
{
    saveSettings();
    accept();
}

void TraySettingsForm::saveSettings()
{
    AppSettings::instance().setTrayOnMinimizeEnabled(m_chkEnabled->isChecked());

    qInfo() << "[TraySettingsForm] Settings saved.";
}

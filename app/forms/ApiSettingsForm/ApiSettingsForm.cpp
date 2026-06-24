// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ApiSettingsForm.h"
#include "../../IconProvider.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDebug>

// ── Constructor ───────────────────────────────────────────────────────────────

ApiSettingsForm::ApiSettingsForm(const QString& serviceName,
                                 const QString& currentKey,
                                 QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("API- Schlüssel konfigurieren für \"%1\"").arg(serviceName));
    setMinimumWidth(450);
    setModal(true);
    setupUi(serviceName, currentKey);
}

// ── apiKey ────────────────────────────────────────────────────────────────────

QString ApiSettingsForm::apiKey() const
{
    return m_editApiKey->text().trimmed();
}

// ── setupUi ───────────────────────────────────────────────────────────────────

void ApiSettingsForm::setupUi(const QString& serviceName,
                               const QString& currentKey)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    // ── API key group ─────────────────────────────────────────────────────
    auto* grpKey = new QGroupBox(tr("  API key"), this);
    auto* keyLayout = new QVBoxLayout(grpKey);

    m_editApiKey = new QLineEdit(grpKey);
    m_editApiKey->setText(currentKey);
    m_editApiKey->setPlaceholderText(
        tr("API-Schlüssel für %1 eingeben...").arg(serviceName));
    // Show key as plain text — the user needs to see/copy it
    m_editApiKey->setEchoMode(QLineEdit::Normal);

    keyLayout->addWidget(m_editApiKey);
    mainLayout->addWidget(grpKey);

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

    connect(m_btnSave,   &QPushButton::clicked, this, &ApiSettingsForm::onSave);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
}

// ── onSave ────────────────────────────────────────────────────────────────────

void ApiSettingsForm::onSave()
{
    accept();
    qInfo() << "[ApiSettingsForm] API key saved.";
}

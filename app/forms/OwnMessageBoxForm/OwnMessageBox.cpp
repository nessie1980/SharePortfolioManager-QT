// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "OwnMessageBox.h"
#include "../../IconProvider.h"
#include "../UiConstants.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>

// ── Constructor ───────────────────────────────────────────────────────────────

OwnMessageBox::OwnMessageBox(Type           type,
                             const QString& title,
                             const QString& message,
                             QWidget*       parent)
    : QDialog(parent)
    , m_type(type)
{
    setWindowTitle(title);
    setModal(true);
    setMinimumWidth(360);
    setupUi(message);
}

// ── setupUi ───────────────────────────────────────────────────────────────────

void OwnMessageBox::setupUi(const QString& message)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 12);

    // ── Icon + message row ────────────────────────────────────────────────
    auto* contentRow = new QHBoxLayout();
    contentRow->setSpacing(12);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(24, 24);

    const IconProvider::IconName iconName =
        (m_type == Type::Critical) ? IconProvider::SearchFailed
                                   : IconProvider::SearchInfo;

    const QIcon ico = IconProvider::icon(iconName);
    if (!ico.isNull())
        m_iconLabel->setPixmap(ico.pixmap(24, 24));

    m_iconLabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    m_msgLabel = new QLabel(message, this);
    m_msgLabel->setWordWrap(true);
    m_msgLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_msgLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    contentRow->addWidget(m_iconLabel);
    contentRow->addWidget(m_msgLabel);
    mainLayout->addLayout(contentRow);

    // ── Separator ─────────────────────────────────────────────────────────
    auto* separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(separator);

    // ── Button row ────────────────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);
    btnRow->addStretch();

    if (m_type == Type::Critical || m_type == Type::Information) {
        // Kein Icon auf dem Ok-Knopf (14.08.2026, Nessies Vorgabe): das
        // bisherige ButtonSave-Icon (Diskette) suggerierte fälschlich ein
        // Speichern — Ok schließt hier nur den Hinweis-/Fehlerdialog.
        m_btnOk = new QPushButton(tr("Ok"), this);
        m_btnOk->setFixedHeight(UiConstants::kButtonHeight);
        m_btnOk->setMinimumWidth(110);
        m_btnOk->setDefault(true);
        connect(m_btnOk, &QPushButton::clicked, this, &QDialog::accept);
        btnRow->addWidget(m_btnOk);
    } else {
        // Question
        m_btnYes = new QPushButton(tr("Ja"), this);
        m_btnYes->setIcon(IconProvider::icon(IconProvider::ButtonSave));
        m_btnYes->setFixedHeight(UiConstants::kButtonHeight);
        m_btnYes->setMinimumWidth(110);
        m_btnYes->setDefault(true);
        connect(m_btnYes, &QPushButton::clicked, this, &QDialog::accept);

        m_btnNo = new QPushButton(tr("Nein"), this);
        m_btnNo->setIcon(IconProvider::icon(IconProvider::ButtonCancel));
        m_btnNo->setFixedHeight(UiConstants::kButtonHeight);
        m_btnNo->setMinimumWidth(110);
        connect(m_btnNo, &QPushButton::clicked, this, &QDialog::reject);

        btnRow->addWidget(m_btnYes);
        btnRow->addWidget(m_btnNo);
    }

    mainLayout->addLayout(btnRow);

    adjustSize();
}

// ── Static convenience methods ────────────────────────────────────────────────

void OwnMessageBox::critical(QWidget*       parent,
                             const QString& title,
                             const QString& message)
{
    OwnMessageBox dlg(Type::Critical, title, message, parent);
    dlg.exec();
}

void OwnMessageBox::information(QWidget*       parent,
                                const QString& title,
                                const QString& message)
{
    OwnMessageBox dlg(Type::Information, title, message, parent);
    dlg.exec();
}

bool OwnMessageBox::question(QWidget*       parent,
                             const QString& title,
                             const QString& message)
{
    OwnMessageBox dlg(Type::Question, title, message, parent);
    return dlg.exec() == QDialog::Accepted;
}

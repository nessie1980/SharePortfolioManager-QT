// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "BackupProgressDialog.h"
#include "../UiConstants.h"
#include "../../IconProvider.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QFileInfo>
#include <QTimer>

// ── Constructor ───────────────────────────────────────────────────────────────

BackupProgressDialog::BackupProgressDialog(const QString& source,
                                           const QString& destination,
                                           QWidget*       parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Backup wird erstellt"));
    setModal(true);
    setFixedWidth(420);
    setWindowFlags(windowFlags() & ~Qt::WindowCloseButtonHint);

    setupUi(source, destination);

    // ── Set up worker thread ──────────────────────────────────────────────
    m_worker = new BackupWorker(source, destination);
    m_thread = new QThread(this);
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::started,       m_worker, &BackupWorker::run);
    connect(m_worker, &BackupWorker::progress,  this,    &BackupProgressDialog::onProgress);
    connect(m_worker, &BackupWorker::finished,  this,    &BackupProgressDialog::onFinished);
    connect(m_worker, &BackupWorker::finished,  m_thread, &QThread::quit);
    connect(m_worker, &BackupWorker::finished,  m_worker, &QObject::deleteLater);
    connect(m_thread, &QThread::finished,       m_thread, &QObject::deleteLater);

    m_thread->start();
}

// ── Destructor ────────────────────────────────────────────────────────────────

BackupProgressDialog::~BackupProgressDialog()
{
    // m_thread (QPointer) may already be nullptr if it finished and ran its
    // own QThread::finished -> deleteLater() chain. If it's still alive,
    // block until it has actually stopped before ~QObject() tears down its
    // QThread child — quit()/wait() are idempotent, so this is safe even if
    // the thread already received a quit() request or already finished.
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
    }
}

// ── setupUi ───────────────────────────────────────────────────────────────────

void BackupProgressDialog::setupUi(const QString& source, const QString& destination)
{
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->setContentsMargins(16, 16, 16, 12);

    // Info label — which file is being backed up
    const QString srcName = QFileInfo(source).fileName();
    const QString dstName = QFileInfo(destination).fileName();

    m_fileLabel = new QLabel(
        tr("Erstelle Backup von:\n%1\n\nnach:\n%2").arg(srcName, dstName), this);
    m_fileLabel->setWordWrap(true);
    layout->addWidget(m_fileLabel);

    // Progress bar
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setFixedHeight(UiConstants::kFieldHeight);
    m_progressBar->setTextVisible(true);
    layout->addWidget(m_progressBar);

    // Status label
    m_statusLabel = new QLabel(tr("Bitte warten..."), this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_statusLabel);

    // Separator
    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    layout->addWidget(sep);

    // Cancel button — bottom right, fixed size matching other dialog buttons
    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    m_btnCancel = new QPushButton(tr("Abbrechen"), this);
    m_btnCancel->setIcon(IconProvider::icon(IconProvider::ButtonCancel));
    m_btnCancel->setFixedHeight(UiConstants::kButtonHeight);
    m_btnCancel->setMinimumWidth(110);
    connect(m_btnCancel, &QPushButton::clicked, this, &BackupProgressDialog::onCancel);
    btnRow->addWidget(m_btnCancel);
    layout->addLayout(btnRow);
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void BackupProgressDialog::onProgress(qint64 bytesWritten, qint64 totalBytes)
{
    if (totalBytes <= 0)
        return;

    const int pct = static_cast<int>((bytesWritten * 100LL) / totalBytes);
    m_progressBar->setValue(pct);

    const double mbWritten = bytesWritten / (1024.0 * 1024.0);
    const double mbTotal   = totalBytes   / (1024.0 * 1024.0);
    m_statusLabel->setText(tr("%1 MB von %2 MB kopiert...")
                               .arg(mbWritten, 0, 'f', 1)
                               .arg(mbTotal,   0, 'f', 1));
}

void BackupProgressDialog::onFinished(bool success, const QString& message)
{
    m_success = success;
    m_progressBar->setValue(success ? 100 : m_progressBar->value());
    m_statusLabel->setText(message);
    m_btnCancel->setEnabled(false);

    // Close the dialog automatically after a short moment
    QTimer::singleShot(800, this, &QDialog::accept);
}

void BackupProgressDialog::onCancel()
{
    m_btnCancel->setEnabled(false);
    m_statusLabel->setText(tr("Breche Backup ab..."));
    if (m_worker)
        QMetaObject::invokeMethod(m_worker, "cancel", Qt::QueuedConnection);
}

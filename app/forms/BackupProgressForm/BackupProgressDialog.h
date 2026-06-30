// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
#include <QPointer>
#include "BackupWorker.h"

/**
 * @brief Modal dialog that shows backup progress and allows cancellation.
 *
 * Starts a BackupWorker in a background thread. The dialog closes
 * automatically when the copy is complete. If the user clicks "Abbrechen",
 * the worker is signalled to stop and the partial file is removed.
 *
 * Usage:
 * @code
 *   BackupProgressDialog dlg(sourcePath, backupPath, this);
 *   dlg.exec();   // blocks until done or cancelled
 *   if (dlg.wasSuccessful()) { ... }
 * @endcode
 */
class BackupProgressDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Construct the dialog and start the background copy.
     * @param source       Full path of the portfolio file to back up.
     * @param destination  Full path of the backup file to create.
     * @param parent       Parent widget.
     */
    explicit BackupProgressDialog(const QString& source,
                                  const QString& destination,
                                  QWidget*       parent = nullptr);

    /**
     * @brief Ensure the worker thread has fully stopped before the dialog
     *        (and its QThread child) is destroyed.
     *
     * onFinished() and QThread::quit() are both connected to the same
     * BackupWorker::finished() signal but execute as separate queued
     * events in the GUI thread — wasSuccessful() can flip true before the
     * worker thread has actually unwound from its event loop. Destroying a
     * still-running QThread triggers "QThread: Destroyed while thread is
     * still running" and can crash. Waiting here makes destruction safe
     * regardless of caller timing (production code and tests alike).
     */
    ~BackupProgressDialog() override;

    /** @brief Returns true if the backup completed successfully. */
    bool wasSuccessful() const { return m_success; }

private slots:
    void onProgress(qint64 bytesWritten, qint64 totalBytes);
    void onFinished(bool success, const QString& message);
    void onCancel();

private:
    void setupUi(const QString& source, const QString& destination);

    QLabel*      m_fileLabel    = nullptr;
    QLabel*      m_statusLabel  = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QPushButton* m_btnCancel    = nullptr;

    // QPointer (not a raw QThread*): the thread connects its own finished()
    // signal to QObject::deleteLater(), so it may already have deleted
    // itself by the time the dialog is destroyed. QPointer automatically
    // becomes nullptr in that case, making the destructor's check safe.
    QPointer<QThread> m_thread;
    BackupWorker* m_worker = nullptr;
    bool          m_success = false;
};

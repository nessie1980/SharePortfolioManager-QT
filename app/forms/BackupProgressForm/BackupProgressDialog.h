// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
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

    QThread*      m_thread = nullptr;
    BackupWorker* m_worker = nullptr;
    bool          m_success = false;
};

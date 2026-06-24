// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QObject>
#include <QString>

/**
 * @brief Worker object that copies a file in a background thread.
 *
 * Emits progress signals as it copies the source file chunk by chunk,
 * so the UI can display a progress bar without blocking.
 *
 * Designed to run in a QThread via moveToThread():
 * @code
 *   auto* worker = new BackupWorker(src, dst);
 *   auto* thread = new QThread;
 *   worker->moveToThread(thread);
 *   connect(thread, &QThread::started,  worker, &BackupWorker::run);
 *   connect(worker, &BackupWorker::finished, thread, &QThread::quit);
 *   connect(worker, &BackupWorker::finished, worker, &QObject::deleteLater);
 *   connect(thread, &QThread::finished, thread, &QObject::deleteLater);
 *   thread->start();
 * @endcode
 */
class BackupWorker : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Construct the worker.
     * @param source       Full path of the file to copy.
     * @param destination  Full path of the backup file to create.
     * @param parent       Optional parent object.
     */
    explicit BackupWorker(const QString& source,
                          const QString& destination,
                          QObject*       parent = nullptr);

public slots:
    /** @brief Start the copy operation. Called when the thread starts. */
    void run();

    /** @brief Request cancellation — the copy is aborted and the partial file removed. */
    void cancel();

signals:
    /**
     * @brief Emitted periodically during the copy.
     * @param bytesWritten  Bytes copied so far.
     * @param totalBytes    Total file size in bytes.
     */
    void progress(qint64 bytesWritten, qint64 totalBytes);

    /**
     * @brief Emitted when the copy has completed (successfully or after cancel/error).
     * @param success  true if the backup file is complete and valid.
     * @param message  Human-readable status message.
     */
    void finished(bool success, const QString& message);

private:
    QString m_source;
    QString m_destination;
    bool    m_cancelled = false;

    static constexpr qint64 kChunkSize = 512LL * 1024; ///< 512 KB per chunk
};

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "BackupWorker.h"

#include <QFile>
#include <QDebug>

// ── Constructor ───────────────────────────────────────────────────────────────

BackupWorker::BackupWorker(const QString& source,
                           const QString& destination,
                           QObject*       parent)
    : QObject(parent)
    , m_source(source)
    , m_destination(destination)
{}

// ── cancel ────────────────────────────────────────────────────────────────────

void BackupWorker::cancel()
{
    m_cancelled = true;
}

// ── run ───────────────────────────────────────────────────────────────────────

void BackupWorker::run()
{
    QFile src(m_source);
    if (!src.open(QIODevice::ReadOnly)) {
        emit finished(false, tr("Quelldatei konnte nicht geöffnet werden:\n%1").arg(m_source));
        return;
    }

    // Remove any existing partial destination file
    if (QFile::exists(m_destination))
        QFile::remove(m_destination);

    QFile dst(m_destination);
    if (!dst.open(QIODevice::WriteOnly)) {
        emit finished(false, tr("Backup-Datei konnte nicht erstellt werden:\n%1").arg(m_destination));
        return;
    }

    const qint64 totalBytes = src.size();
    qint64 bytesWritten = 0;

    while (!src.atEnd() && !m_cancelled) {
        const QByteArray chunk = src.read(kChunkSize);
        if (chunk.isEmpty())
            break;

        const qint64 written = dst.write(chunk);
        if (written != chunk.size()) {
            dst.close();
            QFile::remove(m_destination);
            emit finished(false, tr("Schreibfehler beim Erstellen des Backups."));
            return;
        }

        bytesWritten += written;
        emit progress(bytesWritten, totalBytes);
    }

    src.close();
    dst.close();

    if (m_cancelled) {
        // Remove incomplete backup file
        QFile::remove(m_destination);
        qInfo() << "[BackupWorker] Backup cancelled, partial file removed.";
        emit finished(false, tr("Backup wurde abgebrochen."));
    } else {
        qInfo() << "[BackupWorker] Backup completed:" << m_destination;
        emit finished(true, tr("Backup erfolgreich erstellt."));
    }
}

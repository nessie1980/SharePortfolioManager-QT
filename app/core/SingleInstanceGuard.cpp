// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "SingleInstanceGuard.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

// ── Constructor ───────────────────────────────────────────────────────────────

SingleInstanceGuard::SingleInstanceGuard(const QString& serverName, QObject* parent)
    : QObject(parent)
    , m_serverName(serverName)
    , m_lockFile(lockFilePath(serverName))
{
}

// ── buildServerName ───────────────────────────────────────────────────────────

QString SingleInstanceGuard::buildServerName(const QString& organizationName,
                                              const QString& applicationName)
{
    // "_SingleInstance" suffix keeps this identifier visually distinct from
    // AppStartup::settingsPath()'s "settings.ini" living in the same
    // directory, and from any other QLocalServer name a future feature
    // might add. Spaces are replaced defensively — real org/app names in
    // this project never contain any, but QLocalServer names and lock-file
    // filenames are safest without whitespace regardless.
    return QStringLiteral("%1_%2_SingleInstance")
        .arg(organizationName, applicationName)
        .simplified()
        .replace(QLatin1Char(' '), QLatin1Char('_'));
}

// ── lockFilePath ────────────────────────────────────────────────────────────────

QString SingleInstanceGuard::lockFilePath(const QString& serverName)
{
    // Same QStandardPaths::AppConfigLocation directory as
    // AppStartup::settingsPath() — stable across launches regardless of how
    // the executable is packaged (AppImage/Windows installer/portable
    // build), see ARCHITECTURE.md, "settings.ini nicht persistent im
    // AppImage".
    const QString configDir =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(configDir);
    return configDir + QStringLiteral("/") + serverName + QStringLiteral(".lock");
}

// ── tryAcquire ──────────────────────────────────────────────────────────────────

bool SingleInstanceGuard::tryAcquire()
{
    // QLockFile::tryLock() recognizes a stale lock left behind by a
    // crashed previous instance itself (it checks whether the PID stored
    // in the lock file still exists) and discards it automatically — no
    // manual cleanup needed here, unlike the classic QSharedMemory-based
    // single-instance trick.
    if (m_lockFile.tryLock(100)) {
        m_server = new QLocalServer(this);

        // A previous crash can leave a stale socket/pipe entry behind on
        // some platforms even when the QLockFile above was correctly
        // recognized as stale — removeServer() clears any such leftover
        // before listen() so the primary instance doesn't silently fail to
        // listen for later activation pings.
        QLocalServer::removeServer(m_serverName);
        if (!m_server->listen(m_serverName)) {
            qWarning() << "[SingleInstanceGuard] Could not listen on"
                       << m_serverName << "-" << m_server->errorString()
                       << "- activation from a second launch attempt will"
                          " not work, but this remains the primary instance.";
        } else {
            connect(m_server, &QLocalServer::newConnection,
                    this, &SingleInstanceGuard::onNewConnection);
        }
        return true;
    }

    // Another instance already holds the lock — best-effort ping so it can
    // bring its window to the foreground; the caller exits regardless of
    // whether this ping actually reaches it.
    QLocalSocket socket;
    socket.connectToServer(m_serverName);
    if (socket.waitForConnected(200)) {
        socket.write("activate");
        socket.waitForBytesWritten(200);
        socket.disconnectFromServer();
    } else {
        qWarning() << "[SingleInstanceGuard] Another instance is running but"
                       " could not be reached to bring it to the foreground:"
                    << socket.errorString();
    }
    return false;
}

// ── onNewConnection ─────────────────────────────────────────────────────────────

void SingleInstanceGuard::onNewConnection()
{
    while (QLocalSocket* client = m_server->nextPendingConnection()) {
        // The connection attempt itself is the signal — the message
        // content is not inspected. deleteLater() once the second
        // instance's short-lived socket disconnects again.
        connect(client, &QLocalSocket::disconnected,
                client, &QLocalSocket::deleteLater);
        emit activationRequested();
    }
}

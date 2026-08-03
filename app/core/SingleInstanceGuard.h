// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QObject>
#include <QString>
#include <QLockFile>

class QLocalServer;

/**
 * @brief Ensures only one instance of the application runs at a time.
 *
 * Feature 03.08.2026 ("Die Anwendung darf nur einmal gestartet werden"):
 * uses a `QLockFile` (not the classic `QSharedMemory` trick) to detect
 * whether another instance is already running — `QLockFile` recognizes and
 * discards a stale lock left behind by a crashed previous instance itself
 * (it checks whether the PID stored in the lock file still exists), so no
 * manual cleanup is required here. A `QLocalServer`/`QLocalSocket` pair is
 * used purely for cross-process notification: when a second launch attempt
 * finds the lock already held, it briefly connects to the primary
 * instance's local server to request activation, then the caller
 * (`main.cpp`) shows a short message and exits. See ARCHITECTURE.md, "Die
 * Anwendung darf nur einmal gestartet werden".
 *
 * ### Usage (see main.cpp)
 * @code
 * SingleInstanceGuard guard(SingleInstanceGuard::buildServerName(
 *     app.organizationName(), app.applicationName()));
 * if (!guard.tryAcquire()) {
 *     // tryAcquire() already pinged the running instance — just inform
 *     // the user of this second launch attempt and exit. Do NOT open the
 *     // database or show any window in this branch.
 *     return 0;
 * }
 * // ... construct MainWindow as usual ...
 * QObject::connect(&guard, &SingleInstanceGuard::activationRequested,
 *                   &mainWindow, &MainWindow::restoreFromTray);
 * @endcode
 */
class SingleInstanceGuard : public QObject
{
    Q_OBJECT

public:
    explicit SingleInstanceGuard(const QString& serverName, QObject* parent = nullptr);

    /**
     * @brief Try to become the primary (first) instance.
     *
     * On success: holds the lock for the lifetime of this object and starts
     * listening for activation pings from later launch attempts.
     *
     * On failure: another instance is already running. This method has
     * already sent that instance a best-effort activation ping before
     * returning — the caller only needs to inform the user and exit (must
     * not open the database or show any window in this case, to avoid two
     * processes touching the same portfolio SQLite file concurrently).
     * @return true if this is now the primary instance.
     */
    bool tryAcquire();

    /**
     * @brief Builds the lock-file/local-server identifier from app identity.
     *
     * Pure string-building, no file or socket I/O — deliberately kept
     * separate from tryAcquire() so it is directly unit-testable without a
     * real lock file or local socket involved (same testability pattern as
     * MainWindow::buildDailyValuesUrl()/resolveShareGuidForDocument()).
     * @param organizationName  Typically QApplication::organizationName().
     * @param applicationName   Typically QApplication::applicationName().
     * @return Identifier string safe for both QLockFile's filename and
     *         QLocalServer::listen()'s name (no whitespace).
     */
    static QString buildServerName(const QString& organizationName,
                                    const QString& applicationName);

signals:
    /**
     * @brief Emitted on the primary instance when a later launch attempt
     * requests activation.
     *
     * Connected in main.cpp to MainWindow::restoreFromTray() (made public
     * for exactly this reuse) so the existing window is brought to the
     * foreground — the same "un-hide from tray, raise, activate" logic
     * already used for the tray icon's own restore path applies here
     * whether the window is currently hidden in the tray, minimized, or
     * simply behind other windows.
     */
    void activationRequested();

private slots:
    void onNewConnection();

private:
    static QString lockFilePath(const QString& serverName);

    QString       m_serverName;
    QLockFile     m_lockFile;
    QLocalServer* m_server = nullptr; ///< nullptr until tryAcquire() succeeds; QObject-child of `this`
};

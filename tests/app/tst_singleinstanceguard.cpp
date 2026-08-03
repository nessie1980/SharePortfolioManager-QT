// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include <QtTest>

#include "../../app/core/SingleInstanceGuard.h"

// ─────────────────────────────────────────────────────────────────────────────
// tst_singleinstanceguard — SingleInstanceGuard::buildServerName()
//
// Nur die reine String-Logik ist hier unit-getestet — tryAcquire()/
// activationRequested() selbst (echtes QLockFile + QLocalServer/
// QLocalSocket über mehrere Prozesse) werden bewusst NICHT getestet: es
// gibt keinen sauberen Weg, zwei echte, unabhängige Prozessinstanzen
// deterministisch in einem einzelnen QTest-Lauf zu simulieren, analog zu
// anderen bereits akzeptierten Testlücken bei echten System-Interaktionen
// in diesem Projekt (z. B. QDialog::exec(), echte QSoundEffect-Wiedergabe).
// Siehe ARCHITECTURE.md, "Die Anwendung darf nur einmal gestartet werden".
// ─────────────────────────────────────────────────────────────────────────────

class TestSingleInstanceGuard : public QObject
{
    Q_OBJECT

private slots:
    void test_buildServerName_containsOrgAndAppName()
    {
        const QString name = SingleInstanceGuard::buildServerName(
            QStringLiteral("BT"), QStringLiteral("SharePortfolioManager"));

        QVERIFY(name.contains(QStringLiteral("BT")));
        QVERIFY(name.contains(QStringLiteral("SharePortfolioManager")));
        QVERIFY(name.contains(QStringLiteral("SingleInstance")));
    }

    void test_buildServerName_differentAppNames_produceDifferentNames()
    {
        const QString nameA = SingleInstanceGuard::buildServerName(
            QStringLiteral("BT"), QStringLiteral("AppA"));
        const QString nameB = SingleInstanceGuard::buildServerName(
            QStringLiteral("BT"), QStringLiteral("AppB"));

        QVERIFY(nameA != nameB);
    }

    void test_buildServerName_sameInputs_areDeterministic()
    {
        const QString first = SingleInstanceGuard::buildServerName(
            QStringLiteral("BT"), QStringLiteral("SharePortfolioManager"));
        const QString second = SingleInstanceGuard::buildServerName(
            QStringLiteral("BT"), QStringLiteral("SharePortfolioManager"));

        QCOMPARE(first, second);
    }

    void test_buildServerName_containsNoSpaces()
    {
        // QLocalServer::listen()-Namen und QLockFile-Dateinamen sind ohne
        // Leerzeichen am sichersten — buildServerName() ersetzt Leerzeichen
        // defensiv, auch wenn die echten Org-/App-Namen in diesem Projekt
        // nie welche enthalten.
        const QString name = SingleInstanceGuard::buildServerName(
            QStringLiteral("My Org"), QStringLiteral("My App"));

        QVERIFY(!name.contains(QStringLiteral(" ")));
    }
};

QTEST_MAIN(TestSingleInstanceGuard)
#include "tst_singleinstanceguard.moc"

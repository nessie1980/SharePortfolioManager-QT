// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// spm-xml-importer
// -----------------
// Eigenständiges Console-Tool: importiert eine Portfolio.xml der alten
// C#-SharePortfolioManager-Anwendung in eine spm-qt SQLite-Datenbank.
// Kein Bestandteil der Hauptanwendung SharePortfolioManager.
//
// Aufruf:
//   spm-xml-importer <input.xml> <portfolio.db> [--dry-run] [--log <path>]
//
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QFileInfo>
#include <QDateTime>
#include <QTextStream>

#include "../../app/core/Database.h"
#include "XmlPortfolioParser.h"
#include "PortfolioImporter.h"
#include "ImportLogger.h"

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("spm-xml-importer"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Importiert eine Portfolio.xml der alten C#-SharePortfolioManager-Anwendung "
                       "in eine spm-qt SQLite-Datenbank."));
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addPositionalArgument(QStringLiteral("input.xml"),
                                 QStringLiteral("Pfad zur alten Portfolio-XML-Datei"));
    parser.addPositionalArgument(QStringLiteral("portfolio.db"),
                                 QStringLiteral("Pfad zur Ziel-SQLite-Datenbank "
                                               "(wird angelegt, falls nicht vorhanden)"));

    const QCommandLineOption dryRunOption(
        QStringList() << QStringLiteral("dry-run"),
        QStringLiteral("Nur simulieren — es werden keine Daten in die Datenbank geschrieben."));
    parser.addOption(dryRunOption);

    const QCommandLineOption logOption(
        QStringList() << QStringLiteral("log"),
        QStringLiteral("Pfad zur Log-Datei (Standard: import_<Zeitstempel>.log "
                       "im aktuellen Verzeichnis)."),
        QStringLiteral("path"));
    parser.addOption(logOption);

    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if (args.size() != 2) {
        QTextStream(stderr) << "Fehler: Es werden genau 2 Argumente erwartet "
                               "(input.xml, portfolio.db).\n\n";
        parser.showHelp(1); // beendet den Prozess
    }

    const QString xmlPath = args.at(0);
    const QString dbPath  = args.at(1);
    const bool    dryRun  = parser.isSet(dryRunOption);

    if (!QFileInfo::exists(xmlPath)) {
        QTextStream(stderr) << "Fehler: Eingabedatei nicht gefunden: " << xmlPath << "\n";
        return 1;
    }

    const QString logPath = parser.isSet(logOption)
        ? parser.value(logOption)
        : QStringLiteral("import_%1.log")
              .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy_MM_dd_HH_mm_ss")));

    ImportLogger logger(logPath);
    if (!logger.isOpen()) {
        QTextStream(stderr) << "Warnung: Log-Datei konnte nicht geöffnet werden: " << logPath
                            << " — Ausgabe erfolgt nur auf der Konsole.\n";
    }

    logger.info(QStringLiteral("spm-xml-importer gestartet"));
    logger.info(QStringLiteral("Quelle:  %1").arg(xmlPath));
    logger.info(QStringLiteral("Ziel-DB: %1").arg(dbPath));
    if (dryRun)
        logger.info(QStringLiteral("Modus:   DRY-RUN (keine Schreibzugriffe)"));

    // ── XML einlesen ─────────────────────────────────────────────────────
    RawPortfolio portfolio;
    QString parseError;
    if (!XmlPortfolioParser::parse(xmlPath, portfolio, parseError)) {
        logger.info(QStringLiteral("FEHLER beim Einlesen der XML-Datei: %1").arg(parseError));
        return 2;
    }
    logger.info(QStringLiteral("XML eingelesen: %1 Aktie(n) gefunden.").arg(portfolio.shares.size()));

    // ── Datenbank öffnen (legt Schema an, falls die Datei neu ist) ─────────
    if (!Database::instance().open(dbPath)) {
        logger.info(QStringLiteral("FEHLER: Datenbank konnte nicht geöffnet werden: %1")
                        .arg(Database::instance().lastError().text()));
        return 3;
    }

    // ── Import (validiert zuerst die gesamte Datei — siehe PortfolioImporter) ──
    PortfolioImporter importer(logger, dryRun);
    const bool importOk = importer.importPortfolio(portfolio);

    logger.writeSummary();
    Database::instance().close();

    logger.info(QStringLiteral("Log-Datei: %1").arg(QFileInfo(logPath).absoluteFilePath()));

    // Exit-Code 4: Validierung ist fehlgeschlagen, nichts wurde importiert —
    // unterscheidbar von 0 (Erfolg) für Skripte/CI, die den Importer aufrufen.
    return importOk ? 0 : 4;
}

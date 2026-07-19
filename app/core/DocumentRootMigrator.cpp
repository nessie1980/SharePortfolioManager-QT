// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "DocumentRootMigrator.h"

#include "../repositories/ShareRepository.h"
#include "../repositories/BuyRepository.h"
#include "../repositories/SaleRepository.h"
#include "../repositories/BrokerageRepository.h"
#include "../repositories/DividendRepository.h"

#include <QDir>
#include <QDebug>

// ── collectAllDocuments ─────────────────────────────────────────────────────

QList<DocumentRootMigrator::DocumentEntry> DocumentRootMigrator::collectAllDocuments()
{
    QList<DocumentEntry> result;

    ShareRepository     shareRepo;
    BuyRepository       buyRepo;
    SaleRepository      saleRepo;
    BrokerageRepository brokerageRepo;
    DividendRepository  dividendRepo;

    // Es gibt keine tabellenübergreifende "findAll()"-Abfrage für Dokumente —
    // Repositories liefern Transaktionen nur pro Aktie zurück (findByShare()).
    // Daher iterieren wir über alle Aktien und sammeln je Tabelle ein.
    const QList<ShareObject> shares = shareRepo.findAll();
    for (const ShareObject& share : shares) {
        const QString shareGuid = share.guid();

        for (const BuyObject& buy : buyRepo.findByShare(shareGuid)) {
            if (!buy.document().isEmpty())
                result.append({DocumentEntry::Table::Buy, buy.guid(), buy.document()});
        }
        for (const SaleObject& sale : saleRepo.findByShare(shareGuid)) {
            if (!sale.document().isEmpty())
                result.append({DocumentEntry::Table::Sale, sale.guid(), sale.document()});
        }
        for (const BrokerageObject& brokerage : brokerageRepo.findByShare(shareGuid)) {
            if (!brokerage.document().isEmpty())
                result.append({DocumentEntry::Table::Brokerage, brokerage.guid(), brokerage.document()});
        }
        for (const DividendObject& dividend : dividendRepo.findByShare(shareGuid)) {
            if (!dividend.document().isEmpty())
                result.append({DocumentEntry::Table::Dividend, dividend.guid(), dividend.document()});
        }
    }

    return result;
}

// ── normalizePathSeparators / isAbsoluteDocumentPath ────────────────────────

QString DocumentRootMigrator::normalizePathSeparators(const QString& path)
{
    QString result = path;
    result.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return result;
}

bool DocumentRootMigrator::isAbsoluteDocumentPath(const QString& normalizedPath)
{
    if (normalizedPath.startsWith(QLatin1Char('/')))
        return true; // Unix-Pfad, oder UNC-Pfad "\\server\..." nach Normalisierung "//server/..."

    // Windows-Laufwerksbuchstabe: ein Buchstabe gefolgt von ":", z. B.
    // "B:/Depot/..." — bewusst manuell geprüft statt QDir::isAbsolutePath(),
    // da Letzteres Laufwerksbuchstaben nur erkennt, wenn Qt selbst für
    // Windows gebaut wurde (siehe Klassendoku).
    if (normalizedPath.length() >= 2
        && normalizedPath.at(0).isLetter()
        && normalizedPath.at(1) == QLatin1Char(':')) {
        return true;
    }

    return false;
}

// ── longestCommonDirectory ───────────────────────────────────────────────────

QString DocumentRootMigrator::longestCommonDirectory(const QStringList& paths)
{
    QList<QStringList> componentLists;
    componentLists.reserve(paths.size());
    for (const QString& rawPath : paths) {
        const QString normalized = normalizePathSeparators(rawPath);
        if (!isAbsoluteDocumentPath(normalized))
            continue;

        const QString cleaned = QDir::cleanPath(normalized);
        const int lastSlash = cleaned.lastIndexOf(QLatin1Char('/'));
        const QString dirPath = lastSlash > 0 ? cleaned.left(lastSlash) : QStringLiteral("/");
        componentLists.append(dirPath.split(QLatin1Char('/'), Qt::SkipEmptyParts));
    }

    if (componentLists.isEmpty())
        return {};

    QStringList common = componentLists.first();
    for (int i = 1; i < componentLists.size(); ++i) {
        const QStringList& current = componentLists.at(i);
        int matchLen = 0;
        while (matchLen < common.size() && matchLen < current.size()
               && common.at(matchLen) == current.at(matchLen)) {
            ++matchLen;
        }
        common = common.mid(0, matchLen);
        if (common.isEmpty())
            return {};
    }

    if (common.isEmpty())
        return {};

    // Unix: Zusammenfügen ergibt ohne führenden "/" einen relativen Pfad,
    // daher wird er explizit vorangestellt. Windows: die erste Komponente
    // ist bereits der Laufwerksbuchstabe (z. B. "C:") — ein zusätzliches
    // führendes "/" davor wäre falsch ("/C:/Users/..." ist kein gültiger
    // Windows-Pfad), daher hier ausgelassen.
    const bool startsWithDriveLetter = common.first().length() == 2
        && common.first().endsWith(QLatin1Char(':'));
    QString result = common.join(QLatin1Char('/'));
    if (!startsWithDriveLetter)
        result.prepend(QLatin1Char('/'));
    return QDir::cleanPath(result);
}

// ── changeRoot ────────────────────────────────────────────────────────────────

DocumentRootMigrator::Result DocumentRootMigrator::changeRoot(const QString& oldRoot, const QString& newRoot)
{
    Result result;

    const QString cleanOldRoot = QDir::cleanPath(normalizePathSeparators(oldRoot));
    const QString cleanNewRoot = QDir::cleanPath(normalizePathSeparators(newRoot));

    const QList<DocumentEntry> entries = collectAllDocuments();

    for (const DocumentEntry& entry : entries) {
        const QString cleanPath = QDir::cleanPath(normalizePathSeparators(entry.path));

        // "Innerhalb" heißt: identisch zum alten Root oder mit "<Root>/" als
        // Präfix — ein reiner startsWith() ohne Trennzeichen-Prüfung würde
        // z. B. "/data/Belege2" fälschlich als Teil von "/data/Belege" behandeln.
        const bool insideOldRoot = !cleanOldRoot.isEmpty()
            && (cleanPath == cleanOldRoot || cleanPath.startsWith(cleanOldRoot + QLatin1Char('/')));

        if (!insideOldRoot) {
            ++result.outsideRoot;
            result.outsidePaths.append(entry.path);
            continue;
        }

        const QString relative = cleanPath.mid(cleanOldRoot.length());
        const QString newPath  = QDir::cleanPath(cleanNewRoot + relative);

        if (newPath == cleanPath) {
            ++result.alreadyInRoot;
            continue;
        }

        if (updateDocument(entry, newPath)) {
            ++result.rewritten;
        } else {
            ++result.updateFailed;
            qWarning() << "[DocumentRootMigrator] updateDocument failed for guid"
                       << entry.guid << "old path:" << entry.path;
        }
    }

    return result;
}

// ── updateDocument ───────────────────────────────────────────────────────────

bool DocumentRootMigrator::updateDocument(const DocumentEntry& entry, const QString& newPath)
{
    switch (entry.table) {
    case DocumentEntry::Table::Buy:
        return BuyRepository().updateDocument(entry.guid, newPath);
    case DocumentEntry::Table::Sale:
        return SaleRepository().updateDocument(entry.guid, newPath);
    case DocumentEntry::Table::Brokerage:
        return BrokerageRepository().updateDocument(entry.guid, newPath);
    case DocumentEntry::Table::Dividend:
        return DividendRepository().updateDocument(entry.guid, newPath);
    }
    return false;
}

// ── detectCommonRoot ─────────────────────────────────────────────────────────

DocumentRootMigrator::DetectionResult DocumentRootMigrator::detectCommonRoot()
{
    DetectionResult result;

    const QList<DocumentEntry> entries = collectAllDocuments();

    QStringList absolutePaths;
    for (const DocumentEntry& entry : entries) {
        if (isAbsoluteDocumentPath(normalizePathSeparators(entry.path))) {
            absolutePaths.append(entry.path);
            ++result.absoluteCount;
        } else {
            ++result.relativeCount;
        }
    }

    if (absolutePaths.isEmpty())
        return result; // suggestedRoot bleibt leer — nichts Auswertbares vorhanden.

    const QString common = longestCommonDirectory(absolutePaths);
    if (common.isEmpty()) {
        // Mehrere absolute Pfade vorhanden, aber ohne gemeinsamen Ordner.
        result.ambiguous = absolutePaths.size() > 1;
        return result;
    }

    result.suggestedRoot = common;
    return result;
}

// ── isPathWithinRoot ─────────────────────────────────────────────────────────

bool DocumentRootMigrator::isPathWithinRoot(const QString& path, const QString& root)
{
    if (root.isEmpty())
        return true; // Kein Root konfiguriert — keine Einschränkung.

    const QString cleanRoot = QDir::cleanPath(normalizePathSeparators(root));
    const QString cleanPath = QDir::cleanPath(normalizePathSeparators(path));

    // "Innerhalb" heißt: identisch zum Root oder mit "<Root>/" als Präfix —
    // ein reiner startsWith() ohne Trennzeichen-Prüfung würde z. B.
    // "/data/Belege2" fälschlich als Teil von "/data/Belege" behandeln.
    return cleanPath == cleanRoot || cleanPath.startsWith(cleanRoot + QLatin1Char('/'));
}

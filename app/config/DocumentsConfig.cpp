// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "DocumentsConfig.h"

#include <QFile>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QDebug>

#include <utility>   // std::as_const

// ── Helpers ───────────────────────────────────────────────────────────────────

DocumentType DocumentsConfig::documentTypeFromString(const QString& typeStr)
{
    if (typeStr == QStringLiteral("Sale"))      return DocumentType::Sale;
    if (typeStr == QStringLiteral("Dividend"))  return DocumentType::Dividend;
    if (typeStr == QStringLiteral("Brokerage")) return DocumentType::Brokerage;
    return DocumentType::Buy;
}

QList<QRegularExpression::PatternOption> DocumentsConfig::parseRegexOptions(
    const QString& optionsStr)
{
    QList<QRegularExpression::PatternOption> options;

    const QStringList tokens = optionsStr.split(
        QRegularExpression(QStringLiteral("[,\\s]+")),
        Qt::SkipEmptyParts);

    for (const QString& token : tokens) {
        const QString lower = token.toLower();
        if (lower == QStringLiteral("multiline"))
            options.append(QRegularExpression::MultilineOption);
        else if (lower == QStringLiteral("singleline"))
            options.append(QRegularExpression::DotMatchesEverythingOption);
        else if (lower == QStringLiteral("caseinsensitive"))
            options.append(QRegularExpression::CaseInsensitiveOption);
    }

    return options;
}

// ── load ──────────────────────────────────────────────────────────────────────

DocumentsConfig::LoadResult DocumentsConfig::load(const QString& filePath)
{
    m_entries.clear();
    m_lastError.clear();

    if (!QFile::exists(filePath)) {
        m_lastError = QStringLiteral("File not found: %1").arg(filePath);
        qWarning() << "[DocumentsConfig]" << m_lastError;
        return LoadResult::FileNotFound;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = QStringLiteral("Cannot open file: %1").arg(filePath);
        qWarning() << "[DocumentsConfig]" << m_lastError;
        return LoadResult::LoadFailed;
    }

    QXmlStreamReader xml(&file);

    while (!xml.atEnd() && !xml.hasError()) {
        if (xml.readNext() != QXmlStreamReader::StartElement)
            continue;
        if (xml.name() != QStringLiteral("Bank"))
            continue;

        // ── Read Bank attributes ──────────────────────────────────────────
        const auto   bankAttrs = xml.attributes();
        const QString bankName = bankAttrs.value(QStringLiteral("Name")).toString();
        const QString bankId   = bankAttrs.value(QStringLiteral("BankIdentifierValue")).toString();
        const QString bankEnc  = bankAttrs.value(QStringLiteral("Encoding")).toString();

        if (bankName.isEmpty() || bankId.isEmpty() || bankEnc.isEmpty()) {
            m_lastError = QStringLiteral(
                "Missing Name, BankIdentifierValue or Encoding on <Bank>");
            qWarning() << "[DocumentsConfig]" << m_lastError;
            return LoadResult::BankAttributeError;
        }

        // Die Depotnummer ist der eineindeutige Schlüssel eines Eintrags
        // (siehe DepotEntry). Ein zweites Vorkommen ist ein
        // Konfigurationsfehler und wird hier abgewiesen — nicht stillschweigend
        // übergangen: welcher der beiden Regelsätze für einen Beleg gilt, wäre
        // sonst nicht mehr entscheidbar, und die Erkennung würde eine falsche
        // Antwort geben statt gar keiner.
        for (const auto& existing : std::as_const(m_entries)) {
            if (existing.depotNumber == bankId) {
                m_lastError = QStringLiteral(
                    "Duplicate BankIdentifierValue \"%1\" (banks \"%2\" and \"%3\")")
                    .arg(bankId, existing.bankName, bankName);
                qWarning() << "[DocumentsConfig]" << m_lastError;
                return LoadResult::DuplicateDepotNumber;
            }
        }

        DepotEntry depot;
        depot.bankName    = bankName;
        depot.depotNumber = bankId;
        depot.encoding    = bankEnc;

        // ── Read Bank children ────────────────────────────────────────────
        while (!xml.atEnd()) {
            xml.readNext();

            if (xml.isEndElement() && xml.name() == QStringLiteral("Bank"))
                break;
            if (!xml.isStartElement())
                continue;

            const QString elementName = xml.name().toString();

            // ── Identifier elements ───────────────────────────────────────
            if (elementName == QStringLiteral("BankIdentifier")     ||
                elementName == QStringLiteral("BuyIdentifier")      ||
                elementName == QStringLiteral("SaleIdentifier")     ||
                elementName == QStringLiteral("DividendIdentifier") ||
                elementName == QStringLiteral("BrokerageIdentifier"))
            {
                const auto   idAttrs   = xml.attributes();
                const QString idName   = idAttrs.value(QStringLiteral("Name")).toString();
                const QString idFound  = idAttrs.value(QStringLiteral("FoundIndex")).toString();
                const QString idEmpty  = idAttrs.value(QStringLiteral("ResultEmpty")).toString();
                const QString idOpts   = idAttrs.value(QStringLiteral("RegexOptions")).toString();

                if (idName.isEmpty()) {
                    m_lastError = QStringLiteral(
                        "Missing Name attribute in <%1> of bank \"%2\"")
                        .arg(elementName, bankName);
                    qWarning() << "[DocumentsConfig]" << m_lastError;
                    return LoadResult::IdentifierAttributeError;
                }

                ParserLib::RegExElement element;
                element.regexExpression    = xml.readElementText();
                element.regexFoundPosition = idFound.toInt();
                element.resultEmpty        = (idEmpty.toLower() == QStringLiteral("true"));
                element.regexOptions       = parseRegexOptions(idOpts);

                depot.identifierRegexList.insert(elementName, element);
                continue;
            }

            // ── Document sections ─────────────────────────────────────────
            if (elementName == QStringLiteral("Document")) {
                const auto   docAttrs    = xml.attributes();
                const QString docTypeStr = docAttrs.value(QStringLiteral("Type")).toString();
                const QString docTypeId  = docAttrs.value(QStringLiteral("TypeIdentifierValue")).toString();
                const QString docEnc     = docAttrs.value(QStringLiteral("Encoding")).toString();

                if (docTypeStr.isEmpty() || docTypeId.isEmpty() || docEnc.isEmpty()) {
                    m_lastError = QStringLiteral(
                        "Missing Type, TypeIdentifierValue or Encoding on <Document>"
                        " in bank \"%1\"").arg(bankName);
                    qWarning() << "[DocumentsConfig]" << m_lastError;
                    return LoadResult::DocumentAttributeError;
                }

                DocumentEntry docEntry;
                docEntry.type           = documentTypeFromString(docTypeStr);
                docEntry.typeIdentifier = docTypeId;
                docEntry.encoding       = docEnc;

                // Read document child regex elements
                while (!xml.atEnd()) {
                    xml.readNext();

                    if (xml.isEndElement() && xml.name() == QStringLiteral("Document"))
                        break;
                    if (!xml.isStartElement())
                        continue;

                    const auto   rAttrs   = xml.attributes();
                    const QString rName   = rAttrs.value(QStringLiteral("Name")).toString();
                    const QString rFound  = rAttrs.value(QStringLiteral("FoundIndex")).toString();
                    const QString rEmpty  = rAttrs.value(QStringLiteral("ResultEmpty")).toString();
                    const QString rOpts   = rAttrs.value(QStringLiteral("RegexOptions")).toString();

                    if (rName.isEmpty()) {
                        m_lastError = QStringLiteral(
                            "Missing Name attribute in <%1> inside Document \"%2\""
                            " of bank \"%3\"")
                            .arg(xml.name().toString(), docTypeStr, bankName);
                        qWarning() << "[DocumentsConfig]" << m_lastError;
                        return LoadResult::DocumentAttributeError;
                    }

                    ParserLib::RegExElement element;
                    element.regexExpression    = xml.readElementText();
                    element.regexFoundPosition = rFound.toInt();
                    element.resultEmpty        = (rEmpty.toLower() == QStringLiteral("true"));
                    element.regexOptions       = parseRegexOptions(rOpts);

                    docEntry.regexList.insert(rName, element);
                }

                if (docEntry.regexList.isEmpty()) {
                    m_lastError = QStringLiteral(
                        "Document \"%1\" of bank \"%2\" has no regex elements")
                        .arg(docTypeStr, bankName);
                    qWarning() << "[DocumentsConfig]" << m_lastError;
                    return LoadResult::DocumentElementError;
                }

                depot.documents.insert(docEntry.type, docEntry);
            }
        }

        m_entries.append(depot);
    }

    if (xml.hasError()) {
        m_lastError = QStringLiteral("XML parse error in %1: %2")
                          .arg(filePath, xml.errorString());
        qWarning() << "[DocumentsConfig]" << m_lastError;
        return LoadResult::XmlParseError;
    }

    if (m_entries.isEmpty()) {
        m_lastError = QStringLiteral("No <Bank> entries found in %1").arg(filePath);
        qWarning() << "[DocumentsConfig]" << m_lastError;
        return LoadResult::EmptyConfig;
    }

    qInfo() << "[DocumentsConfig] Loaded" << m_entries.size()
            << "depot(s) from" << filePath;
    return LoadResult::Success;
}

// ── findByDepotNumber ─────────────────────────────────────────────────────────

const DepotEntry* DocumentsConfig::findByDepotNumber(const QString& depotNumber) const
{
    for (const auto& entry : std::as_const(m_entries)) {
        if (entry.depotNumber == depotNumber)
            return &entry;
    }
    return nullptr;
}

// ── findDocument ──────────────────────────────────────────────────────────────

const DocumentEntry* DocumentsConfig::findDocument(const DepotEntry& depot,
                                                     DocumentType type)
{
    auto it = depot.documents.find(type);
    if (it == depot.documents.end())
        return nullptr;
    return &it.value();
}

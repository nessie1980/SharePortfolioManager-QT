// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "DocumentClassifier.h"

#include <QRegularExpression>

namespace {

/**
 * @brief Apply a RegExElement's pattern + options and test it against text.
 */
bool regexMatches(const ParserLib::RegExElement& element, const QString& text)
{
    QRegularExpression re(element.regexExpression);
    for (const auto option : element.regexOptions)
        re.setPatternOptions(re.patternOptions() | option);
    return re.isValid() && re.match(text).hasMatch();
}

/**
 * @brief Shared implementation for DocumentClassifier::detectDocumentType()
 * and the internal step-2 lookup inside classify() — same key order as the
 * (pre-refactoring) duplicated logic in the four presenters.
 * @return true and sets @p outType if one of the four identifiers matches.
 */
bool findMatchingType(const QString& pdfText, const BankEntry& bank, DocumentType& outType)
{
    static const struct { const char* key; DocumentType type; } kTypeChecks[] = {
        { "BuyIdentifier",       DocumentType::Buy       },
        { "SaleIdentifier",      DocumentType::Sale      },
        { "DividendIdentifier",  DocumentType::Dividend  },
        { "BrokerageIdentifier", DocumentType::Brokerage }
    };

    for (const auto& check : kTypeChecks) {
        const auto it = bank.identifierRegexList.constFind(QString::fromLatin1(check.key));
        if (it == bank.identifierRegexList.constEnd())
            continue;
        if (regexMatches(*it, pdfText)) {
            outType = check.type;
            return true;
        }
    }
    return false;
}

} // namespace

// ── matchBankIndex ────────────────────────────────────────────────────────────

bool DocumentClassifier::matchBankIndex(const QString& pdfText,
                                        const DocumentsConfig& config,
                                        int& outIndex)
{
    const QList<BankEntry> entries = config.entries();
    for (int i = 0; i < entries.size(); ++i) {
        const BankEntry& bank = entries.at(i);
        const auto it = bank.identifierRegexList.constFind(QStringLiteral("BankIdentifier"));
        if (it == bank.identifierRegexList.constEnd())
            continue;
        if (regexMatches(*it, pdfText)) {
            outIndex = i;
            return true;
        }
    }
    return false;
}

// ── detectDocumentType ────────────────────────────────────────────────────────

DocumentType DocumentClassifier::detectDocumentType(const QString& pdfText,
                                                     const BankEntry& bank,
                                                     DocumentType fallbackType)
{
    DocumentType type = fallbackType;
    findMatchingType(pdfText, bank, type); // leaves `type` at fallbackType if nothing matches
    return type;
}

// ── classify ──────────────────────────────────────────────────────────────────

DocumentClassifier::Result DocumentClassifier::classify(const QString& pdfText,
                                                         const DocumentsConfig& config)
{
    Result result;

    int bankIndex = -1;
    if (!matchBankIndex(pdfText, config, bankIndex))
        return result; // matched stays false

    // Re-fetch entries() here rather than threading the list through
    // matchBankIndex() — keeps that helper's signature simple (index only).
    // Cheap: QList<BankEntry> is implicitly shared, and this runs once per
    // dropped document, not in a hot loop.
    const QList<BankEntry> entries = config.entries();
    const BankEntry& matchedBank = entries.at(bankIndex);

    DocumentType matchedType = DocumentType::Buy;
    if (!findMatchingType(pdfText, matchedBank, matchedType))
        return result; // matched stays false — we deliberately do not guess
                        // (unlike detectDocumentType(), which the four
                        // presenters use with their own dialog-specific
                        // fallback — classify() has no such context)

    const DocumentEntry* docEntry = DocumentsConfig::findDocument(matchedBank, matchedType);
    if (!docEntry)
        return result;

    // Copy into the result now, while `entries` (and therefore `matchedBank`/
    // `docEntry`, which points into it) is still alive within this function.
    result.matched  = true;
    result.bank     = matchedBank;
    result.docEntry = *docEntry;
    result.type     = matchedType;
    return result;
}

// ── extractFieldValue ─────────────────────────────────────────────────────────

QString DocumentClassifier::extractFieldValue(const QString& pdfText,
                                              const ParserLib::RegExList& regexList,
                                              const QString& fieldName)
{
    const auto it = regexList.constFind(fieldName);
    if (it == regexList.constEnd())
        return QString();

    if (!regexMatches(*it, pdfText))
        return QString();

    QRegularExpression re(it->regexExpression);
    for (const auto option : it->regexOptions)
        re.setPatternOptions(re.patternOptions() | option);

    const QRegularExpressionMatch match = re.match(pdfText);
    // Prefer the first capture group if the pattern defines one, otherwise
    // fall back to the full match.
    const QString value = match.lastCapturedIndex() >= 1
        ? match.captured(1)
        : match.captured(0);
    return value.trimmed();
}

// ── extractWkn / extractIsin ──────────────────────────────────────────────────

QString DocumentClassifier::extractWkn(const QString& pdfText, const DocumentEntry& docEntry)
{
    return extractFieldValue(pdfText, docEntry.regexList, QStringLiteral("Wkn"));
}

QString DocumentClassifier::extractIsin(const QString& pdfText, const DocumentEntry& docEntry)
{
    return extractFieldValue(pdfText, docEntry.regexList, QStringLiteral("Isin"));
}

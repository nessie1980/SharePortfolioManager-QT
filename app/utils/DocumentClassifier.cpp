// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "DocumentClassifier.h"

#include <QRegularExpression>

namespace {

/**
 * @brief Apply a RegExElement's pattern + options and test it against text.
 *
 * Bugfix 21.08.2026: Ein LEERES Muster identifiziert nichts und liefert
 * deshalb false.
 *
 * `QRegularExpression("")` ist gültig und trifft jeden Text (leerer Treffer
 * an Position 0). Ohne diese Abfrage wurde jedes leer gelassene
 * `<SaleIdentifier></SaleIdentifier>` & Co. zu einem "passt immer" — und weil
 * `findMatchingType()` die vier Kennungen in fester Reihenfolge (Buy, Sale,
 * Dividend, Brokerage) durchgeht, gewann eine leere Kennung gegen jede
 * später geprüfte, tatsächlich passende.
 *
 * Im Feldfall (Nessies Bugreport "Consors-Dividenden werden überhaupt nicht
 * gelesen"): Cortal Consors hat eine leere `SaleIdentifier`-Regel, weil für
 * diese Bank keine Verkaufsbelege konfiguriert sind. Eine Consors-
 * Dividendengutschrift lief daher nicht in `DividendIdentifier`, sondern
 * blieb schon bei `SaleIdentifier` hängen und wurde als `DocumentType::Sale`
 * eingestuft. Da die Bank folgerichtig auch keinen `<Document Type="Sale">`-
 * Block hat, lieferte `DocumentsConfig::findDocument()` einen Nullzeiger, und
 * `startParserForText()` brach ab, ohne ein einziges Feld zu lesen.
 *
 * Die Prüfung sitzt bewusst hier und nicht im Lader: `DocumentsConfig` gibt
 * die Konfiguration unverfälscht wieder (ein leeres Element bleibt ein leeres
 * Element), und die Bedeutung "leer = identifiziert nichts" gehört zur
 * Auswertung. Gleichzeitig schützt sie beide Aufrufwege — `matchBankIndex()`
 * ebenso, wo eine leere `BankIdentifier`-Regel sonst jedes Dokument dieser
 * Bank zugeschlagen hätte.
 */
bool regexMatches(const ParserLib::RegExElement& element, const QString& text)
{
    if (element.regexExpression.isEmpty())
        return false;

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

    // Bank ab hier festhalten, damit ein Aufrufer im Fehlerfall unterscheiden
    // kann, WORAN es lag (siehe Result::bankMatched). BankEntry ist ein
    // Wertetyp; die Kopie bleibt auch nach dem Verlassen dieser Funktion gültig.
    result.bankMatched = true;
    result.bank        = matchedBank;

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
    // `bank` steht bereits oben; hier kommen nur die typabhängigen Felder dazu.
    result.matched  = true;
    result.docEntry = *docEntry;
    result.type     = matchedType;
    return result;
}

// ── extractFieldValue ─────────────────────────────────────────────────────────

/**
 * @brief Liest ein einzelnes Feld aus dem Belegtext — mit DERSELBEN
 * Auswahlregel wie `ParserLib::Parser::doRegexParsing()`.
 *
 * Diese Gleichheit ist der ganze Zweck der Funktion und war bis zum
 * 21.08.2026 nicht gegeben (Bugreport Nessie: ein DKB-Dividendenbeleg, der
 * im Dialog einwandfrei gelesen wird, meldete per Drag&Drop "Keine passende
 * Aktie im Portfolio gefunden"). Die alte Fassung nahm immer den ERSTEN
 * Treffer und daraus starr die Fanggruppe 1; `FoundIndex` wurde ignoriert.
 *
 * Für die DKB lautet die WKN-Regel `[(]((?:[A-Za-z0-9]{1,}))[)]` mit
 * `FoundIndex="1"` — "das ZWEITE Klammerpaar im Text". Das erste ist die
 * Spaltenüberschrift `(WKN)`; die alte Fassung lieferte also buchstäblich
 * die Zeichenkette "WKN" an `ShareRepository::findByWkn()`, was zwangsläufig
 * ins Leere lief. Betroffen waren alle drei DKB-Belegarten (Kauf, Verkauf,
 * Dividende), die diese Regel teilen — nicht nur Dividenden. ING und Cortal
 * Consors blieben unauffällig, weil ihre Regeln auf `FoundIndex="0"` stehen
 * und damit zufällig mit der falschen Auswahl übereinstimmten.
 *
 * @note Die Rückfall-Logik "keine Fanggruppe → ganzer Treffer" ist mit
 * entfallen, ebenfalls zugunsten der Gleichheit: `Parser` sammelt
 * ausschliesslich die Fanggruppen 1..n. Eine Regel ohne Fanggruppe lieferte
 * also im Dialog ohnehin nichts, und zwei Wege, die aus derselben
 * Konfiguration verschiedene Werte lesen, sind genau der Fehler, der hier
 * behoben wird.
 */
QString DocumentClassifier::extractFieldValue(const QString& pdfText,
                                              const ParserLib::RegExList& regexList,
                                              const QString& fieldName)
{
    const auto it = regexList.constFind(fieldName);
    if (it == regexList.constEnd() || it->regexExpression.isEmpty())
        return QString();

    QRegularExpression re(it->regexExpression);
    for (const auto option : it->regexOptions)
        re.setPatternOptions(re.patternOptions() | option);
    if (!re.isValid())
        return QString();

    // Auswahlregel wie in ParserLib::Parser::doRegexParsing():
    //   regexFoundPosition >= 0 → genau dieser Treffer,
    //   regexFoundPosition <  0 → alle Treffer (hier: der erste, der einen
    //                             Wert liefert — gesucht ist ein Einzelwert).
    // Aus dem gewählten Treffer die erste NICHT-LEERE Fanggruppe; das ist
    // bei Ausdrücken mit Alternativen (`a(x)|b(y)`) der Unterschied zwischen
    // einem Wert und einer leeren Zeichenkette.
    const int wantedIndex = it->regexFoundPosition;

    auto matchIt   = re.globalMatch(pdfText);
    int  matchIndex = 0;
    while (matchIt.hasNext()) {
        const QRegularExpressionMatch match = matchIt.next();

        if (wantedIndex < 0 || matchIndex == wantedIndex) {
            for (int capture = 1; capture <= match.lastCapturedIndex(); ++capture) {
                const QString captured = match.captured(capture);
                if (!captured.isEmpty())
                    return captured.trimmed();
            }
            if (wantedIndex >= 0)
                return QString(); // gewünschter Treffer hat keine gefüllte Gruppe
        }
        ++matchIndex;
    }
    return QString();
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

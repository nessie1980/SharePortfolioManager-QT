// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../../libs/parser/src/DataTypes.h"

#include <QString>
#include <QList>
#include <QMap>

/**
 * @brief The type of a broker document.
 */
enum class DocumentType {
    Buy,       ///< Purchase confirmation
    Sale,      ///< Sale confirmation
    Dividend,  ///< Dividend payment notice
    Brokerage  ///< Standalone brokerage/fee notice
};

/**
 * @brief Parsing configuration for one document type of one depot.
 *
 * Maps to one `<Document>` element inside a `<Bank>` in `Documents.xml`.
 * Contains the regex rules to extract field values (WKN, ISIN, Date, Price, etc.)
 * from a PDF document that has been converted to text via pdftotext.
 */
struct DocumentEntry
{
    DocumentType       type;            ///< Document type (Buy / Sale / Dividend / Brokerage)
    QString            typeIdentifier;  ///< String that identifies this document type in the PDF text
    QString            encoding;        ///< Text encoding of the PDF document
    ParserLib::RegExList regexList;     ///< Named regex rules for field extraction
};

/**
 * @brief Parsing configuration for exactly ONE DEPOT at one bank.
 *
 * Maps to one `<Bank>` element in `Documents.xml`. Despite the XML element's
 * name, an entry does not describe a bank in general — it describes a single
 * depot, and @ref depotNumber is its unique key (Nessie, 25.08.2026):
 *
 * - A second depot at an already-configured bank needs its OWN `<Bank>`
 *   element, carrying the same @ref bankName but a different @ref depotNumber.
 * - The regex rules are cut to the layout of the documents that THIS depot
 *   produces. If a bank changes its layout or extends its depot numbers from
 *   nine to ten digits, the rules of that entry have to follow.
 *
 * @note @ref bankName is display text only and therefore NOT unique. Anything
 * that has to identify an entry — bank detection, the depot-number combo
 * boxes of the four edit dialogs, findByDepotNumber() — goes through
 * @ref depotNumber. See ARCHITECTURE.md, "Bankerkennung: Mehrdeutigkeit ueber
 * die Depotnummer".
 *
 * @note The struct was called `BankEntry` with members `name`/`identifier`
 * until 25.08.2026. The old names suggested a per-bank entry and were part of
 * why bank detection never compared the depot number in the first place.
 * The XML attribute names (`Name`, `BankIdentifierValue`) stay untouched —
 * they describe the file format, which is deliberately unchanged.
 */
struct DepotEntry
{
    QString            bankName;       ///< Bank display name (e.g. "DKB") — NOT unique
    QString            depotNumber;    ///< Depot number from `BankIdentifierValue` — the unique key
    QString            encoding;       ///< Default encoding for this depot's documents
    ParserLib::RegExList identifierRegexList; ///< Bank + transaction type identifier regexes
    QMap<DocumentType, DocumentEntry> documents; ///< Parsing rules per document type
};

/**
 * @brief Loads and provides access to the document parsing configuration.
 *
 * Reads `Documents.xml` and exposes the entries as a list of `DepotEntry`
 * objects — one per DEPOT, not per bank (see DepotEntry). Each entry contains
 * identifier regexes and per-document regex rule sets used by the PDF parser
 * to extract transaction data.
 *
 * Depot numbers are unique across the whole file; load() rejects a duplicate
 * with LoadResult::DuplicateDepotNumber rather than silently keeping both.
 *
 * ### XML structure
 * ```xml
 * <Documents>
 *   <Bank Name="DKB" BankIdentifierValue="501403950" Encoding="UTF-8">
 *     <BankIdentifier Name="BankIdentifier" FoundIndex="0" ResultEmpty="true"
 *                     RegexOptions="None">Depotnummer\s+([0-9]{1,9})</BankIdentifier>
 *     <BuyIdentifier  Name="BuyIdentifier"  FoundIndex="0" ResultEmpty="true"
 *                     RegexOptions="None">(Wertpapier Abrechnung Kauf)</BuyIdentifier>
 *     ...
 *     <Document Type="Buy" TypeIdentifierValue="Wertpapier Abrechnung Kauf" Encoding="UTF-8">
 *       <Wkn  Name="Wkn"  FoundIndex="0" ResultEmpty="true" RegexOptions="None">...</Wkn>
 *       <Date Name="Date" FoundIndex="0" ResultEmpty="false" RegexOptions="None">...</Date>
 *       ...
 *     </Document>
 *   </Bank>
 * </Documents>
 * ```
 *
 * ### Usage
 * @code
 * DocumentsConfig config;
 * if (config.load("/path/to/Documents.xml") == DocumentsConfig::LoadResult::Success) {
 *     const auto* depot = config.findByDepotNumber("501403950");
 *     if (depot) {
 *         const auto* doc = config.findDocument(*depot, DocumentType::Buy);
 *         if (doc)
 *             parser.setParsingValues(ParsingValues(text, doc->encoding, doc->regexList));
 *     }
 * }
 * @endcode
 */
class DocumentsConfig
{
public:
    /**
     * @brief Load error codes returned by load().
     */
    enum class LoadResult {
        Success                     =  0, ///< File loaded successfully
        FileNotFound                = -1, ///< XML file does not exist
        EmptyConfig                 = -2, ///< XML file contains no Bank entries
        BankAttributeError          = -3, ///< A required Bank attribute is missing
        BankElementError            = -4, ///< Expected child elements missing in Bank
        DocumentElementError        = -5, ///< Expected child elements missing in Document
        DocumentAttributeError      = -6, ///< A required Document attribute is missing
        IdentifierAttributeError    = -7, ///< A required identifier attribute is missing
        XmlParseError               = -8, ///< XML is malformed
        LoadFailed                  = -9, ///< Unspecified load failure

        /**
         * @brief Two `<Bank>` elements carry the same `BankIdentifierValue`.
         *
         * Added 25.08.2026 together with the depot-number based bank
         * detection. The depot number is the unique key of an entry; if it
         * appears twice, no caller can tell which set of regex rules a
         * document belongs to. Loading is refused rather than keeping the
         * first and dropping the second, because a silently halved
         * configuration would produce wrong parse results instead of a
         * visible error.
         */
        DuplicateDepotNumber        = -10
    };

    DocumentsConfig() = default;

    /**
     * @brief Load the document parsing configuration from an XML file.
     * @param filePath  Full path to the Documents.xml file.
     * @return LoadResult::Success on success, error code otherwise.
     */
    LoadResult load(const QString& filePath);

    /**
     * @brief Returns all loaded depot entries.
     * @return List of DepotEntry objects, empty if not loaded.
     */
    QList<DepotEntry> entries() const { return m_entries; }

    /**
     * @brief Find a depot entry by its depot number.
     *
     * Replaces the former `findByName()` (removed 25.08.2026). Looking an
     * entry up by bank name cannot work once a second depot exists at the
     * same bank: both entries carry the same name, and the lookup would
     * silently return whichever comes first in the file. The depot number is
     * the unique key — see DepotEntry.
     *
     * @param depotNumber  Depot number as written in `BankIdentifierValue`.
     * @return Pointer to the matching entry, or nullptr if not found.
     */
    const DepotEntry* findByDepotNumber(const QString& depotNumber) const;

    /**
     * @brief Find a document entry within a depot by document type.
     * @param depot  The depot entry to search in.
     * @param type   The document type to find.
     * @return Pointer to the matching DocumentEntry, or nullptr if not found.
     */
    static const DocumentEntry* findDocument(const DepotEntry& depot, DocumentType type);

    /**
     * @brief Convert a document type string to the DocumentType enum.
     * @param typeStr  Type string from XML (e.g. "Buy", "Sale", "Dividend", "Brokerage").
     * @return Corresponding DocumentType, or DocumentType::Buy as fallback.
     */
    static DocumentType documentTypeFromString(const QString& typeStr);

    /**
     * @brief Returns true if the config was loaded and contains at least one depot.
     * @return true if valid.
     */
    bool isValid() const { return !m_entries.isEmpty(); }

    /**
     * @brief Returns the number of loaded depot entries.
     * @return Entry count.
     */
    int count() const { return m_entries.size(); }

    /**
     * @brief Returns the last error message, if any.
     * @return Human-readable error description, empty if no error.
     */
    QString lastError() const { return m_lastError; }

private:
    /**
     * @brief Parse a regex options string into a list of PatternOption flags.
     * @param optionsStr  Options string from XML attribute.
     * @return List of PatternOption flags (empty for "None").
     */
    static QList<QRegularExpression::PatternOption> parseRegexOptions(
        const QString& optionsStr);

    QList<DepotEntry> m_entries;
    QString          m_lastError;
};

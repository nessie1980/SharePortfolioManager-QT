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
 * @brief Parsing configuration for one document type of a bank.
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
 * @brief Parsing configuration for one bank.
 *
 * Maps to one `<Bank>` element in `Documents.xml`.
 * Contains identifier regexes to recognise which bank and document type
 * a given PDF belongs to, plus per-document regex rule sets.
 */
struct BankEntry
{
    QString            name;           ///< Bank display name (e.g. "DKB")
    QString            identifier;     ///< Regex value to identify the bank's depot number
    QString            encoding;       ///< Default encoding for this bank's documents
    ParserLib::RegExList identifierRegexList; ///< Bank + transaction type identifier regexes
    QMap<DocumentType, DocumentEntry> documents; ///< Parsing rules per document type
};

/**
 * @brief Loads and provides access to the document parsing configuration.
 *
 * Reads `Documents.xml` and exposes the entries as a list of `BankEntry` objects.
 * Each bank entry contains identifier regexes and per-document regex rule sets
 * used by the PDF parser to extract transaction data.
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
 *     const auto* bank = config.findByName("DKB");
 *     if (bank) {
 *         const auto* doc = config.findDocument(*bank, DocumentType::Buy);
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
        LoadFailed                  = -9  ///< Unspecified load failure
    };

    DocumentsConfig() = default;

    /**
     * @brief Load the document parsing configuration from an XML file.
     * @param filePath  Full path to the Documents.xml file.
     * @return LoadResult::Success on success, error code otherwise.
     */
    LoadResult load(const QString& filePath);

    /**
     * @brief Returns all loaded bank entries.
     * @return List of BankEntry objects, empty if not loaded.
     */
    QList<BankEntry> entries() const { return m_entries; }

    /**
     * @brief Find a bank entry by its name.
     * @param name  Bank name (e.g. "DKB").
     * @return Pointer to the matching entry, or nullptr if not found.
     */
    const BankEntry* findByName(const QString& name) const;

    /**
     * @brief Find a document entry within a bank by document type.
     * @param bank  The bank entry to search in.
     * @param type  The document type to find.
     * @return Pointer to the matching DocumentEntry, or nullptr if not found.
     */
    static const DocumentEntry* findDocument(const BankEntry& bank, DocumentType type);

    /**
     * @brief Convert a document type string to the DocumentType enum.
     * @param typeStr  Type string from XML (e.g. "Buy", "Sale", "Dividend", "Brokerage").
     * @return Corresponding DocumentType, or DocumentType::Buy as fallback.
     */
    static DocumentType documentTypeFromString(const QString& typeStr);

    /**
     * @brief Returns true if the config was loaded and contains at least one bank.
     * @return true if valid.
     */
    bool isValid() const { return !m_entries.isEmpty(); }

    /**
     * @brief Returns the number of loaded bank entries.
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

    QList<BankEntry> m_entries;
    QString          m_lastError;
};

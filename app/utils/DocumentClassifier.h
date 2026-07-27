// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../config/DocumentsConfig.h"

#include <QString>

/**
 * @brief Determines which bank and document type a PDF text belongs to.
 *
 * Extracted from the (previously four times duplicated) `startParserForText()`
 * bank-/type-detection step in `PresenterBuyEdit`, `PresenterSaleEdit`,
 * `PresenterDividendEdit` and `PresenterShareAdd` (see ARCHITECTURE.md,
 * "PDF-Erkennungslogik gebündelt in DocumentClassifier"). Those presenters
 * still run the same detection internally, since they already know which
 * dialog they belong to and only need the matching `DocumentEntry` to feed
 * into `ParserLib::Parser`.
 *
 * `DocumentClassifier` is used stand-alone by the "Direkte
 * Dokumentenerfassung" drop feature in `MainWindow`, where the document type
 * is *not* known ahead of time and must be determined purely from the PDF
 * content before deciding which edit dialog to open.
 *
 * Pure, static, no Qt object — trivially unit-testable without a GUI.
 *
 * @note `DocumentsConfig::entries()` returns `QList<BankEntry>` *by value*.
 * `Result` therefore stores `BankEntry`/`DocumentEntry` **copies**, not
 * pointers into that temporary list — a pointer would dangle the moment
 * classify() returns (or even sooner, at the end of the internal loop, since
 * a range-based for's implicit range temporary is only kept alive for the
 * loop itself). Copies are cheap enough here (once per dropped document) and
 * make the result safe to keep around after the call.
 */
class DocumentClassifier
{
public:
    /**
     * @brief Result of classify(): the matched bank/document type, if any.
     */
    struct Result
    {
        bool          matched = false;             ///< true if both bank and document type were identified
        BankEntry     bank;                         ///< Matched bank entry (copy — see class note)
        DocumentEntry docEntry;                     ///< Matched document entry within that bank (copy)
        DocumentType  type = DocumentType::Buy;     ///< Only meaningful when matched == true
    };

    DocumentClassifier() = delete; // static-only utility class

    /**
     * @brief Identify the bank and document type for a PDF's extracted text.
     *
     * Step 1: find the bank whose `BankIdentifier` regex matches the text.
     * Step 2: within that bank, check `BuyIdentifier` / `SaleIdentifier` /
     *         `DividendIdentifier` / `BrokerageIdentifier` in that order and
     *         take the first one that matches.
     * Step 3: look up the corresponding `DocumentEntry` for that type.
     *
     * Unlike the per-form presenters (which default to their own dialog's
     * type when no identifier matches, since the user already chose that
     * dialog), this generic classifier reports `matched == false` if either
     * the bank or the document type cannot be determined — callers with no
     * prior context must not guess.
     *
     * @param pdfText  Plain text extracted from the PDF via pdftotext.
     * @param config   Loaded DocumentsConfig (Documents.xml already parsed).
     * @return Result with matched == true only if bank, type and the
     *         corresponding DocumentEntry were all found.
     */
    static Result classify(const QString& pdfText, const DocumentsConfig& config);

    /**
     * @brief Extract a single named field's value from PDF text via regex.
     *
     * Applies the `RegExElement` stored under @p fieldName in @p regexList
     * to @p pdfText. If the pattern has at least one capture group, the
     * first capture group is returned; otherwise the full match is returned.
     *
     * @param pdfText    Plain text extracted from the PDF.
     * @param regexList  Regex rules to search in (typically docEntry.regexList).
     * @param fieldName  Key to look up in @p regexList (e.g. "Wkn", "Isin").
     * @return Trimmed matched value, or an empty string if @p fieldName is
     *         not present in @p regexList, the pattern is invalid, or there
     *         is no match.
     */
    static QString extractFieldValue(const QString& pdfText,
                                     const ParserLib::RegExList& regexList,
                                     const QString& fieldName);

    /**
     * @brief Convenience wrapper: extractFieldValue(pdfText, docEntry.regexList, "Wkn").
     */
    static QString extractWkn(const QString& pdfText, const DocumentEntry& docEntry);

    /**
     * @brief Convenience wrapper: extractFieldValue(pdfText, docEntry.regexList, "Isin").
     */
    static QString extractIsin(const QString& pdfText, const DocumentEntry& docEntry);

    // ── Lower-level helpers (used internally by classify(), and by the four
    //    edit-dialog presenters after their PdfTextExtractor/DocumentClassifier
    //    refactoring — see ARCHITECTURE.md, "PDF-Erkennungslogik gebündelt in
    //    DocumentClassifier") ─────────────────────────────────────────────

    /**
     * @brief Find the index (within `config.entries()`) of the bank whose
     * `BankIdentifier` regex matches @p pdfText.
     *
     * Exposed separately from classify() because the four edit-dialog
     * presenters (`PresenterBuyEdit`, `PresenterSaleEdit`,
     * `PresenterDividendEdit`, `PresenterShareAdd`) need the matched
     * `BankEntry` even when the document *type* can't be determined — each
     * dialog falls back to its own type in that case (see
     * detectDocumentType()), unlike classify() which reports no match at all.
     *
     * @param pdfText   Plain text extracted from the PDF.
     * @param config    Loaded DocumentsConfig.
     * @param outIndex  Set to the matching index on success; left unchanged
     *                  on failure.
     * @return true if a bank was found, false otherwise.
     */
    static bool matchBankIndex(const QString& pdfText,
                               const DocumentsConfig& config,
                               int& outIndex);

    /**
     * @brief Determine the document type for an already-matched bank.
     *
     * Checks `BuyIdentifier` / `SaleIdentifier` / `DividendIdentifier` /
     * `BrokerageIdentifier` in that order and returns the first one that
     * matches @p pdfText. If none match, returns @p fallbackType — this is
     * what lets each edit-dialog presenter keep assuming its own document
     * type when the PDF's identifier is inconclusive (the user already
     * chose that dialog, e.g. "Verkäufe hinzufügen", so defaulting to Sale
     * there is reasonable; classify() has no such context and must not
     * default).
     *
     * @param pdfText       Plain text extracted from the PDF.
     * @param bank          The already-matched bank entry.
     * @param fallbackType  Returned when no identifier matches.
     */
    static DocumentType detectDocumentType(const QString& pdfText,
                                           const BankEntry& bank,
                                           DocumentType fallbackType);
};

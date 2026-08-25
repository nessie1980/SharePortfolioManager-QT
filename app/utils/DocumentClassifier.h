// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../config/DocumentsConfig.h"

#include <QString>

/**
 * @brief Determines which depot and document type a PDF text belongs to.
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
 * @note `DocumentsConfig::entries()` returns `QList<DepotEntry>` *by value*.
 * `Result` therefore stores `DepotEntry`/`DocumentEntry` **copies**, not
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
     * @brief Result of classify(): the matched depot/document type, if any.
     */
    struct Result
    {
        bool          matched = false;             ///< true if both depot and document type were identified

        /**
         * @brief true, sobald das DEPOT erkannt wurde — auch dann, wenn der
         *        Dokumenttyp anschliessend nicht zugeordnet werden konnte.
         *
         * Ergänzt 21.08.2026 (damals `bankMatched`): Ohne diese Unterscheidung
         * konnte der Aufrufer einem Benutzer nur mitteilen, dass "irgendetwas"
         * nicht passte. Die beiden Fälle verlangen aber ganz verschiedene
         * Reaktionen — bei einem unbekannten Depot fehlt ein Eintrag in
         * `Documents.xml`, bei einem unbekannten Dokumenttyp handelt es sich
         * schlicht um eine Belegart, die die Anwendung nicht verarbeitet
         * (z. B. eine Vorabpauschale-Abrechnung für thesaurierende Fonds).
         * `depot` ist in diesem Fall bereits gefüllt, und sein `bankName`
         * kann in der Meldung genannt werden.
         */
        bool          depotMatched = false;

        DepotEntry    depot;                        ///< Gefüllt, sobald depotMatched true ist (Kopie — siehe Klassennotiz)
        DocumentEntry docEntry;                     ///< Matched document entry within that depot (copy)
        DocumentType  type = DocumentType::Buy;     ///< Only meaningful when matched == true
    };

    DocumentClassifier() = delete; // static-only utility class

    /**
     * @brief Identify the depot and document type for a PDF's extracted text.
     *
     * Step 1: find the depot whose `BankIdentifier` regex matches the text
     *         AND whose configured depot number is the one it captured
     *         (see matchDepotIndex()).
     * Step 2: within that depot, check `BuyIdentifier` / `SaleIdentifier` /
     *         `DividendIdentifier` / `BrokerageIdentifier` in that order and
     *         take the first one that matches.
     * Step 3: look up the corresponding `DocumentEntry` for that type.
     *
     * Unlike the per-form presenters (which default to their own dialog's
     * type when no identifier matches, since the user already chose that
     * dialog), this generic classifier reports `matched == false` if either
     * the depot or the document type cannot be determined — callers with no
     * prior context must not guess.
     *
     * @param pdfText  Plain text extracted from the PDF via pdftotext.
     * @param config   Loaded DocumentsConfig (Documents.xml already parsed).
     * @return Result with matched == true only if depot, type and the
     *         corresponding DocumentEntry were all found.
     */
    static Result classify(const QString& pdfText, const DocumentsConfig& config);

    /**
     * @brief Extract a single named field's value from PDF text via regex.
     *
     * Applies the `RegExElement` stored under @p fieldName in @p regexList
     * to @p pdfText, using the same selection rule as
     * `ParserLib::Parser::doRegexParsing()`: the match at `FoundIndex`, and
     * within it the first NON-EMPTY capture group. A rule without capture
     * groups yields nothing — see the implementation comment; the former
     * "no group → whole match" fallback was dropped on 21.08.2026 so that
     * both readers of a rule agree.
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
     * @brief Find the index (within `config.entries()`) of the depot whose
     * `BankIdentifier` regex matches @p pdfText **and** whose configured
     * depot number equals the value that regex captured.
     *
     * Both halves are required. Matching the label alone was the behaviour
     * until 25.08.2026 and mis-assigned every Cortal Consors document to the
     * DKB, because both banks label their depot number identically and the
     * DKB entry comes first in `Documents.xml` — see the implementation
     * comment and ARCHITECTURE.md, "Bankerkennung: Mehrdeutigkeit ueber die
     * Depotnummer".
     *
     * Exposed separately from classify() because the four edit-dialog
     * presenters (`PresenterBuyEdit`, `PresenterSaleEdit`,
     * `PresenterDividendEdit`, `PresenterShareAdd`) need the matched
     * `DepotEntry` even when the document *type* can't be determined — each
     * dialog falls back to its own type in that case (see
     * detectDocumentType()), unlike classify() which reports no match at all.
     *
     * @param pdfText   Plain text extracted from the PDF.
     * @param config    Loaded DocumentsConfig.
     * @param outIndex  Set to the matching index on success; left unchanged
     *                  on failure.
     * @return true if a depot was found, false otherwise.
     */
    static bool matchDepotIndex(const QString& pdfText,
                                const DocumentsConfig& config,
                                int& outIndex);

    /**
     * @brief Determine the document type for an already-matched depot.
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
     * @param depot         The already-matched depot entry.
     * @param fallbackType  Returned when no identifier matches.
     */
    static DocumentType detectDocumentType(const QString& pdfText,
                                           const DepotEntry& depot,
                                           DocumentType fallbackType);
};

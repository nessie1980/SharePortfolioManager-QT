// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "XmlPortfolioParser.h"

#include <QList>
#include <QString>

/**
 * @brief One structured, human-readable validation problem.
 *
 * Deliberately flat/simple (no severity levels) — as of 05.07.2026 every
 * validation issue is blocking: if PortfolioValidator::validate() finds
 * anything at all, the entire import is aborted before a single row is
 * written (see PortfolioImporter::importPortfolio()). There is no
 * warning-only tier here; auto-corrected formatting details (e.g. double-
 * escaped ampersands) are handled separately as RawShare::parseWarnings
 * and never reach this struct.
 */
struct ValidationIssue
{
    QString shareWkn;    ///< WKN of the affected share (may be empty if the share itself has none)
    QString shareName;   ///< Name of the affected share, for display when WKN is empty/ambiguous
    QString category;    ///< "Share" | "Buy" | "Sale" | "Brokerage" | "Dividend" | "DailyValue"
    QString recordId;    ///< GUID / OrderNumber / date, whichever identifies the record best (may be empty)
    QString message;     ///< Human-readable description of the problem
};

/**
 * @brief Validates an entire RawPortfolio BEFORE any of it is imported.
 *
 * Introduced 05.07.2026 to replace the previous per-record
 * "log ERROR and skip, but keep going" behavior, which could leave the
 * database in an inconsistent state (some records of a share imported,
 * others silently missing). The new contract is strict: validate()
 * inspects every share and every child record across the whole file; if
 * it finds ANY problem anywhere, PortfolioImporter must import nothing at
 * all — not even the shares that were themselves fine.
 *
 * Checks performed (all blocking — see ValidationIssue):
 * - Share: missing WKN; unrecognized `Update`/`ShareType`/`Parsing` values;
 *   unparsable date fields (`StockMarketLaunchDate`, `LastUpdateInternet`,
 *   `LastUpdateShareDate`); any XmlPortfolioParser::RawShare::parseErrors
 *   (e.g. the `<MarketValues>` tag-name error, see XmlPortfolioParser.h).
 * - Buy/Sale/Brokerage/Dividend: missing GUID; unparsable `Date`; duplicate
 *   GUID within the same share (across all four categories — GUIDs are
 *   meant to be globally unique identifiers); duplicate `OrderNumber`
 *   within the same share for Buys, and separately for Sales (checked both
 *   within the current file and against already-imported records in the DB).
 * - Brokerage: missing/unresolvable `GuidBuySale` (must match exactly one
 *   Buy or Sale of the same share — either already in the DB from a
 *   previous import, or present in the current file).
 * - DailyValue: unparsable `D` (date).
 *
 * Deliberately NOT covered (out of scope for this pass, flagged for a
 * possible future extension): parseability of numeric fields other than
 * dates (e.g. `SharePrice`, `Volume`, `Provision` — these still silently
 * fall back to 0.0 via PortfolioImporter::toDouble() on invalid input).
 *
 * Pure read-only: only ever queries the database (to check for collisions
 * with already-imported data), never writes to it.
 */
class PortfolioValidator
{
public:
    /**
     * @brief Validate the whole portfolio.
     * @param portfolio  Parsed portfolio to check.
     * @param issues     Receives every problem found, across all shares.
     * @return true if no problems were found (safe to import), false otherwise.
     */
    static bool validate(const RawPortfolio& portfolio, QList<ValidationIssue>& issues);

private:
    static void validateShare(const RawShare& share, QList<ValidationIssue>& issues);
    static void validateBuys(const RawShare& share, const QString& existingShareGuid,
                             QList<ValidationIssue>& issues);
    static void validateSales(const RawShare& share, const QString& existingShareGuid,
                              QList<ValidationIssue>& issues);
    static void validateBrokerages(const RawShare& share, const QString& existingShareGuid,
                                   QList<ValidationIssue>& issues);
    static void validateDividends(const RawShare& share, QList<ValidationIssue>& issues);
    static void validateDailyValues(const RawShare& share, QList<ValidationIssue>& issues);
    static void validateDuplicateGuids(const RawShare& share, QList<ValidationIssue>& issues);

    /// "dd.MM.yyyy" or "dd.MM.yyyy HH:mm" (time part ignored, mirrors
    /// PortfolioImporter::toDate()'s handling of mixed date/date-time fields).
    static bool isParsableGermanDate(const QString& raw);
};

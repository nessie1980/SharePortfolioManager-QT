// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "XmlPortfolioParser.h"
#include "PortfolioValidator.h"
#include "ImportLogger.h"

#include <QDate>

/**
 * @brief Imports a parsed RawPortfolio into the spm-qt SQLite database.
 *
 * Uses the same repository/model classes as the main application
 * (ShareRepository, BuyRepository, SaleRepository, BrokerageRepository,
 * DividendRepository, DailyValuesRepository) so the resulting data is
 * indistinguishable from data entered through the UI.
 *
 * ### Validate-then-import (since 05.07.2026)
 * importPortfolio() first runs PortfolioValidator::validate() over the
 * *entire* file. If it finds ANY problem anywhere — in any share, any
 * record — nothing at all is imported: no database writes happen, and
 * importPortfolio() returns false. All issues found are logged as a
 * structured, per-share report (see logValidationIssues()) so the person
 * running the import can see immediately which shares/records need fixing
 * in the source XML. This replaces the previous per-record
 * "log ERROR and skip, but keep going" behavior, which could leave the
 * database in an inconsistent state (some records of a share imported,
 * others silently missing because of a data error elsewhere in the file).
 *
 * ### Idempotency
 * - Shares are matched by WKN. If a share with the same WKN already exists,
 *   its GUID is reused and only missing child records are imported; the
 *   existing share's master data is left untouched.
 * - Buys/Sales/Dividends/Brokerages reuse the GUID from the source XML
 *   directly. Before inserting, the importer checks whether a record with
 *   that GUID already exists and skips it if so — re-running the importer
 *   on the same (or an updated) export file is therefore safe.
 * - Daily values use INSERT OR REPLACE on the composite (share_guid, date)
 *   key, so they are always safe to re-import and simply refresh existing rows.
 *
 * ### Insert order (required by foreign keys)
 * shares -> buys -> sales (+ sale_buy_details) -> brokerage -> dividends -> daily_values
 *
 * The brokerage table is inserted last because its buy_guid/sale_guid columns
 * carry real foreign keys (REFERENCES buys(guid) / sales(guid)), whereas
 * buys.brokerage_guid and sales.brokerage_guid are plain TEXT columns without
 * a FK constraint — so those can safely be set before the brokerage row exists.
 *
 * Once validate-then-import has passed, the per-record checks below (missing
 * GUID, unresolvable GuidBuySale, etc.) should never trigger in practice —
 * they are kept as a defensive fallback, not the primary safety net anymore.
 * The caller decides via the constructor's `dryRun` flag whether any database
 * writes happen at all (validation itself never writes, regardless of dryRun).
 */
class PortfolioImporter
{
public:
    PortfolioImporter(ImportLogger& logger, bool dryRun);

    /**
     * @brief Validates, then imports every share in the portfolio.
     * @return true if validation passed and the import ran (or would have,
     *         in dry-run mode); false if validation found problems, in
     *         which case nothing was imported and all problems were logged.
     */
    bool importPortfolio(const RawPortfolio& portfolio);

private:
    void importShare(const RawShare& share);
    void importBuys(const RawShare& share, const QString& shareGuid);
    void importSales(const RawShare& share, const QString& shareGuid);
    void importBrokerages(const RawShare& share, const QString& shareGuid);
    void importDividends(const RawShare& share, const QString& shareGuid);
    void importDailyValues(const RawShare& share, const QString& shareGuid);

    /// Logs every issue from a failed validate() call as a structured,
    /// per-share report (grouped so every problem for a given share is
    /// visible together), followed by a one-line summary count.
    void logValidationIssues(const QList<ValidationIssue>& issues);

    ImportLogger& m_logger;
    bool          m_dryRun;

    // ── Conversion helpers (German XML formats -> Qt/SQLite formats) ───────
    static double  toDouble(const QString& germanNumber);
    static QDate   toDate(const QString& germanDate);                  ///< "dd.MM.yyyy[ HH:mm]" -> date part only
    static QString toIsoDate(const QString& germanDate);                ///< -> "yyyy-MM-dd"
    static QString toIsoDateTime(const QString& germanDateOrDateTime);  ///< -> "yyyy-MM-ddTHH:mm:ss"
};

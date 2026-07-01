// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "XmlPortfolioParser.h"
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
 * Errors on individual records are logged and the import continues with the
 * next record; only structural problems (missing GUID, missing share) skip
 * the affected record's children. The caller decides via the constructor's
 * `dryRun` flag whether any database writes happen at all.
 */
class PortfolioImporter
{
public:
    PortfolioImporter(ImportLogger& logger, bool dryRun);

    /// Imports every share in the portfolio, continuing past per-record errors.
    void importPortfolio(const RawPortfolio& portfolio);

private:
    void importShare(const RawShare& share);
    void importBuys(const RawShare& share, const QString& shareGuid);
    void importSales(const RawShare& share, const QString& shareGuid);
    void importBrokerages(const RawShare& share, const QString& shareGuid);
    void importDividends(const RawShare& share, const QString& shareGuid);
    void importDailyValues(const RawShare& share, const QString& shareGuid);

    ImportLogger& m_logger;
    bool          m_dryRun;

    // ── Conversion helpers (German XML formats -> Qt/SQLite formats) ───────
    static double  toDouble(const QString& germanNumber);
    static QDate   toDate(const QString& germanDate);                  ///< "dd.MM.yyyy[ HH:mm]" -> date part only
    static QString toIsoDate(const QString& germanDate);                ///< -> "yyyy-MM-dd"
    static QString toIsoDateTime(const QString& germanDateOrDateTime);  ///< -> "yyyy-MM-ddTHH:mm:ss"
};

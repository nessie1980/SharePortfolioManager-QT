// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../models/DailyValuesObject.h"
#include <QList>
#include <QString>
#include <QDate>
#include <QSqlQuery>
#include <QSqlError>

/**
 * @brief Repository for daily OHLCV price data.
 *
 * Provides CRUD operations for the `daily_values` table in SQLite.
 * The primary key is the composite (share_guid, date) — there can be
 * at most one entry per share per trading day.
 *
 * All methods operate on the shared Database::instance() connection.
 *
 * ### Typical usage
 * @code
 * DailyValuesRepository repo;
 * repo.upsert(DailyValuesObject(shareGuid, QDate(2024,6,15), 142.5, 144.8, 145.2, 141.9, 1250000));
 * auto history = repo.findByShare(shareGuid);
 * @endcode
 */
class DailyValuesRepository
{
public:
    DailyValuesRepository() = default;

    /**
     * @brief Result counters for an upsertList() call with change tracking.
     *
     * Only populated when upsertList() is called with a non-null stats
     * out-parameter. In that case each incoming record is compared against
     * the existing DB row (if any) before writing:
     *  - no existing row            -> inserted
     *  - existing row, values differ -> updated
     *  - existing row, values equal  -> unchanged (row is NOT re-written)
     */
    struct UpsertStats
    {
        int fetched   = 0; ///< Number of records passed in (i.e. size of the input list)
        int inserted  = 0; ///< Number of rows newly inserted
        int updated   = 0; ///< Number of existing rows whose values changed
        int unchanged = 0; ///< Number of existing rows left untouched (values identical)
    };

    // ── Create / Update ───────────────────────────────────────────────────
    /**
     * @brief Insert a new daily values record, or replace it if (shareGuid, date) already exists.
     * @param dailyValues  Fully populated DailyValuesObject.
     * @return true on success.
     */
    bool upsert(const DailyValuesObject& dailyValues);

    /**
     * @brief Insert a list of daily values records, replacing existing ones.
     *
     * Wraps all inserts in a single transaction for performance.
     *
     * Each record is compared against the existing DB row (via
     * findByShareAndDate()) before writing: rows with identical values
     * (within a tight floating-point tolerance) are skipped entirely — no
     * DB write occurs for them. Rows that are new or whose values changed
     * go through upsert() as before. This comparison always runs,
     * regardless of whether @p stats is provided.
     *
     * @param dailyValuesList  List of DailyValuesObjects to insert.
     * @param stats            Optional out-parameter receiving change counts
     *                         (fetched/inserted/updated/unchanged), e.g. for
     *                         a detailed status message after a refresh.
     * @return true if all inserts succeeded, false on first error.
     */
    bool upsertList(const QList<DailyValuesObject>& dailyValuesList,
                     UpsertStats* stats = nullptr);

    // ── Read ──────────────────────────────────────────────────────────────
    /**
     * @brief Find all daily values for a share, ordered by date ascending.
     * @param shareGuid  GUID of the parent share.
     * @return List of DailyValuesObjects, empty if none found.
     */
    QList<DailyValuesObject> findByShare(const QString& shareGuid) const;

    /**
     * @brief Find all daily values for a share within a date range.
     * @param shareGuid  GUID of the parent share.
     * @param from       Start date (inclusive).
     * @param to         End date (inclusive).
     * @return List of DailyValuesObjects in the range, empty if none found.
     */
    QList<DailyValuesObject> findByShareAndDateRange(const QString& shareGuid,
                                                      const QDate& from,
                                                      const QDate& to) const;

    /**
     * @brief Find a single daily values record by share GUID and date.
     * @param shareGuid  GUID of the parent share.
     * @param date       The trading date.
     * @return DailyValuesObject; isValid() == false if not found.
     */
    DailyValuesObject findByShareAndDate(const QString& shareGuid, const QDate& date) const;

    /**
     * @brief Returns the most recent trading date for a share.
     * @param shareGuid  GUID of the share.
     * @return The latest date, or an invalid QDate if no records exist.
     */
    QDate latestDate(const QString& shareGuid) const;

    /**
     * @brief Returns the number of stored daily value records for a share.
     * @param shareGuid  GUID of the share.
     * @return Record count, 0 if none.
     */
    int count(const QString& shareGuid) const;

    // ── Delete ────────────────────────────────────────────────────────────
    /**
     * @brief Delete a single daily values record by share GUID and date.
     * @param shareGuid  GUID of the parent share.
     * @param date       The trading date to delete.
     * @return true on success.
     */
    bool remove(const QString& shareGuid, const QDate& date);

    /**
     * @brief Delete all daily values records for a share.
     * @param shareGuid  GUID of the parent share.
     * @return Number of deleted rows, or -1 on error.
     */
    int removeByShare(const QString& shareGuid);

    // ── Error handling ────────────────────────────────────────────────────
    /**
     * @brief Returns the last SQL error, if any.
     * @return The most recent QSqlError; invalid if no error has occurred.
     */
    QSqlError lastError() const { return m_lastError; }

private:
    /**
     * @brief Construct a DailyValuesObject from the current row of a QSqlQuery result.
     * @param sqlQuery  An executed query positioned on the row to read.
     * @return Populated DailyValuesObject.
     */
    DailyValuesObject fromQuery(const QSqlQuery& sqlQuery) const;

    mutable QSqlError m_lastError; ///< Last SQL error, set on any failed operation
};

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../models/ShareSplitObject.h"
#include <QList>
#include <QString>
#include <QDate>
#include <QSqlQuery>
#include <QSqlError>

/**
 * @brief Repository for share split records.
 *
 * Provides CRUD operations for the `share_splits` table in SQLite. Splits
 * are keyed by their own GUID (analogous to BuyObject/BrokerageObject, not
 * DailyValuesObject's composite key), with an additional
 * `UNIQUE(share_guid, date)` constraint — two splits of the same share on
 * the same day make no business sense.
 *
 * All methods operate on the shared Database::instance() connection.
 */
class ShareSplitRepository
{
public:
    ShareSplitRepository() = default;

    // ── Create ────────────────────────────────────────────────────────────
    /**
     * @brief Insert a new split into the database.
     * @param split  Fully populated ShareSplitObject (must have valid guid and shareGuid).
     * @return true on success, false also when share_guid+date already exists (UNIQUE violation).
     */
    bool insert(const ShareSplitObject& split);

    // ── Read ──────────────────────────────────────────────────────────────
    /**
     * @brief Find all splits for a share, ordered by date ascending.
     * @param shareGuid  GUID of the parent share.
     * @return List of ShareSplitObjects, empty if none found.
     */
    QList<ShareSplitObject> findByShare(const QString& shareGuid) const;

    /**
     * @brief Find a single split by its GUID.
     * @param guid  GUID of the split.
     * @return ShareSplitObject; isValid() == false if not found.
     */
    ShareSplitObject findByGuid(const QString& guid) const;

    /**
     * @brief Check whether a split already exists for a share on a given date.
     * @param shareGuid  GUID of the share.
     * @param date       Date to check.
     * @return true if a split exists on that date for that share.
     */
    bool existsForDate(const QString& shareGuid, const QDate& date) const;

    // ── Update ────────────────────────────────────────────────────────────
    /**
     * @brief Update all fields of an existing split.
     * @param split  ShareSplitObject with updated values (identified by guid).
     * @return true on success.
     */
    bool update(const ShareSplitObject& split);

    // ── Delete ────────────────────────────────────────────────────────────
    /**
     * @brief Delete a split by its GUID.
     * @param guid  GUID of the record to delete.
     * @return true on success.
     */
    bool remove(const QString& guid);

    /**
     * @brief Delete all splits for a share.
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
     * @brief Construct a ShareSplitObject from the current row of a QSqlQuery result.
     * @param sqlQuery  An executed query positioned on the row to read.
     * @return Populated ShareSplitObject.
     */
    ShareSplitObject fromQuery(const QSqlQuery& sqlQuery) const;

    mutable QSqlError m_lastError; ///< Last SQL error, set on any failed operation
};

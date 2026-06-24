// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../models/DividendObject.h"
#include <QList>
#include <QString>
#include <QSqlQuery>
#include <QSqlError>

/**
 * @brief Repository for dividend payments.
 *
 * Provides CRUD operations for the `dividends` table in SQLite.
 * All methods operate on the shared Database::instance() connection.
 */
class DividendRepository
{
public:
    DividendRepository() = default;

    // ── Create ────────────────────────────────────────────────────────────
    /**
     * @brief Insert a new dividend record into the database.
     * @param dividend  Fully populated DividendObject (must have valid guid and shareGuid).
     * @return true on success.
     */
    bool insert(const DividendObject& dividend);

    // ── Read ──────────────────────────────────────────────────────────────
    /**
     * @brief Find all dividends for a share, ordered by date ascending.
     * @param shareGuid  GUID of the parent share.
     * @return List of DividendObjects, empty if none found.
     */
    QList<DividendObject> findByShare(const QString& shareGuid) const;

    /**
     * @brief Find a single dividend by its GUID.
     * @param guid  GUID of the dividend record.
     * @return DividendObject; isValid() == false if not found.
     */
    DividendObject findByGuid(const QString& guid) const;

    /**
     * @brief Find all dividends for a share in a given year.
     * @param shareGuid  GUID of the parent share.
     * @param year       4-digit year (e.g. 2024).
     * @return List of DividendObjects for that year, empty if none found.
     */
    QList<DividendObject> findByShareAndYear(const QString& shareGuid, int year) const;

    // ── Update ────────────────────────────────────────────────────────────
    /**
     * @brief Update all fields of an existing dividend record.
     * @param dividend  DividendObject with updated values (identified by guid).
     * @return true on success.
     */
    bool update(const DividendObject& dividend);

    /**
     * @brief Update only the document path of a dividend record.
     * @param guid      GUID of the dividend.
     * @param document  New document file path.
     * @return true on success.
     */
    bool updateDocument(const QString& guid, const QString& document);

    // ── Delete ────────────────────────────────────────────────────────────
    /**
     * @brief Delete a dividend record by its GUID.
     * @param guid  GUID of the dividend to delete.
     * @return true on success.
     */
    bool remove(const QString& guid);

    /**
     * @brief Delete all dividend records for a share.
     * @param shareGuid  GUID of the parent share.
     * @return Number of deleted rows, or -1 on error.
     */
    int removeByShare(const QString& shareGuid);

    // ── Aggregates ────────────────────────────────────────────────────────
    /**
     * @brief Total net payout (dividendPayoutWithTaxes) for a share.
     * @param shareGuid  GUID of the share.
     * @return Sum of net payouts across all dividends, 0.0 if none.
     */
    double totalPayoutWithTaxes(const QString& shareGuid) const;

    /**
     * @brief Total gross payout (dividendPayout) for a share.
     * @param shareGuid  GUID of the share.
     * @return Sum of gross payouts across all dividends, 0.0 if none.
     */
    double totalPayout(const QString& shareGuid) const;

    // ── Error handling ────────────────────────────────────────────────────
    QSqlError lastError() const { return m_lastError; }

private:
    /**
     * @brief Construct a DividendObject from the current row of a QSqlQuery result.
     */
    DividendObject fromQuery(const QSqlQuery& query) const;

    mutable QSqlError m_lastError; ///< Last SQL error
};

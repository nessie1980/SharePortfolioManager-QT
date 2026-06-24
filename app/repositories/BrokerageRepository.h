// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../models/BrokerageObject.h"
#include <QList>
#include <QString>
#include <QSqlQuery>
#include <QSqlError>

/**
 * @brief Repository for standalone brokerage records.
 *
 * Provides CRUD operations for the `brokerage` table in SQLite.
 * A brokerage record is optionally linked to a buy or sale transaction
 * via buyGuid / saleGuid. All methods operate on the shared
 * Database::instance() connection.
 */
class BrokerageRepository
{
public:
    BrokerageRepository() = default;

    // ── Create ────────────────────────────────────────────────────────────
    /**
     * @brief Insert a new brokerage record into the database.
     * @param brokerage  Fully populated BrokerageObject (must have valid guid and shareGuid).
     * @return true on success.
     */
    bool insert(const BrokerageObject& brokerage);

    // ── Read ──────────────────────────────────────────────────────────────
    /**
     * @brief Find all brokerage records for a share, ordered by date ascending.
     * @param shareGuid  GUID of the parent share.
     * @return List of BrokerageObjects, empty if none found.
     */
    QList<BrokerageObject> findByShare(const QString& shareGuid) const;

    /**
     * @brief Find a single brokerage record by its GUID.
     * @param guid  GUID of the brokerage record.
     * @return BrokerageObject; isValid() == false if not found.
     */
    BrokerageObject findByGuid(const QString& guid) const;

    /**
     * @brief Find the brokerage record associated with a buy transaction.
     * @param buyGuid  GUID of the buy transaction.
     * @return BrokerageObject; isValid() == false if not found.
     */
    BrokerageObject findByBuyGuid(const QString& buyGuid) const;

    /**
     * @brief Find the brokerage record associated with a sale transaction.
     * @param saleGuid  GUID of the sale transaction.
     * @return BrokerageObject; isValid() == false if not found.
     */
    BrokerageObject findBySaleGuid(const QString& saleGuid) const;

    /**
     * @brief Find all brokerage records for a share in a given year.
     * @param shareGuid  GUID of the parent share.
     * @param year       4-digit year (e.g. 2024).
     * @return List of BrokerageObjects for that year, empty if none found.
     */
    QList<BrokerageObject> findByShareAndYear(const QString& shareGuid, int year) const;

    // ── Update ────────────────────────────────────────────────────────────
    /**
     * @brief Update all fields of an existing brokerage record.
     * @param brokerage  BrokerageObject with updated values (identified by guid).
     * @return true on success.
     */
    bool update(const BrokerageObject& brokerage);

    /**
     * @brief Update only the document path of a brokerage record.
     * @param guid      GUID of the brokerage record.
     * @param document  New document file path.
     * @return true on success.
     */
    bool updateDocument(const QString& guid, const QString& document);

    // ── Delete ────────────────────────────────────────────────────────────
    /**
     * @brief Delete a brokerage record by its GUID.
     * @param guid  GUID of the record to delete.
     * @return true on success.
     */
    bool remove(const QString& guid);

    /**
     * @brief Delete all brokerage records for a share.
     * @param shareGuid  GUID of the parent share.
     * @return Number of deleted rows, or -1 on error.
     */
    int removeByShare(const QString& shareGuid);

    // ── Aggregates ────────────────────────────────────────────────────────
    /**
     * @brief Total brokerage (provision + brokerFee + traderFee) for a share.
     * @param shareGuid  GUID of the share.
     * @return Sum of all brokerage values, 0.0 if none.
     */
    double totalBrokerage(const QString& shareGuid) const;

    /**
     * @brief Total brokerage minus reduction for a share.
     * @param shareGuid  GUID of the share.
     * @return Sum of brokerageReduction across all records, 0.0 if none.
     */
    double totalBrokerageReduction(const QString& shareGuid) const;

    // ── Error handling ────────────────────────────────────────────────────
    /**
     * @brief Returns the last SQL error, if any.
     * @return The most recent QSqlError; invalid if no error has occurred.
     */
    QSqlError lastError() const { return m_lastError; }

private:
    /**
     * @brief Construct a BrokerageObject from the current row of a QSqlQuery result.
     * @param sqlQuery  An executed query positioned on the row to read.
     * @return Populated BrokerageObject.
     */
    BrokerageObject fromQuery(const QSqlQuery& sqlQuery) const;

    mutable QSqlError m_lastError; ///< Last SQL error, set on any failed operation
};

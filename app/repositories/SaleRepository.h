// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../models/SaleObject.h"
#include <QList>
#include <QString>
#include <QSqlQuery>
#include <QSqlError>

/**
 * @brief Repository for sale transactions.
 *
 * Provides CRUD operations for the `sales` table in SQLite.
 * Since a sale can reference multiple buys, the `sale_buy_details`
 * table stores the individual buy contributions per sale.
 *
 * All methods operate on the shared Database::instance() connection.
 */
class SaleRepository
{
public:
    SaleRepository() = default;

    // ── Create ────────────────────────────────────────────────────────────
    /**
     * @brief Insert a new sale and its buy details into the database.
     * @param sale  Fully populated SaleObject
     * @return true on success
     */
    bool insert(const SaleObject& sale);

    // ── Read ──────────────────────────────────────────────────────────────
    /**
     * @brief Find all sales for a share, ordered by date ascending.
     * @param shareGuid  GUID of the parent share.
     * @return List of SaleObjects, empty if none found.
     */
    QList<SaleObject> findByShare(const QString& shareGuid) const;

    /**
     * @brief Find a single sale by its GUID.
     * @return SaleObject (isValid() == false if not found)
     */
    SaleObject findByGuid(const QString& guid) const;

    /**
     * @brief Find all sales for a share in a given year.
     * @param shareGuid  GUID of the parent share.
     * @param year       4-digit year (e.g. 2024).
     * @return List of SaleObjects for that year, empty if none found.
     */
    QList<SaleObject> findByShareAndYear(const QString& shareGuid, int year) const;

    /**
     * @brief Check if an order number already exists for a share.
     * @param shareGuid    GUID of the share.
     * @param orderNumber  Order number to check.
     * @return true if the order number exists, false otherwise.
     */
    bool orderNumberExists(const QString& shareGuid, const QString& orderNumber) const;

    // ── Update ────────────────────────────────────────────────────────────
    /**
     * @brief Update all fields of an existing sale (replaces buy details too).
     * @param sale  SaleObject with updated values (identified by guid).
     * @return true on success.
     */
    bool update(const SaleObject& sale);

    /**
     * @brief Update only the document path of a sale.
     * @param guid      GUID of the sale.
     * @param document  New document file path.
     * @return true on success.
     */
    bool updateDocument(const QString& guid, const QString& document);

    // ── Delete ────────────────────────────────────────────────────────────
    /**
     * @brief Delete a sale and its buy details by GUID.
     * @param guid  GUID of the sale to delete.
     * @return true on success.
     */
    bool remove(const QString& guid);

    /**
     * @brief Delete all sales for a share.
     * @param shareGuid  GUID of the parent share.
     * @return Number of deleted rows, or -1 on error.
     */
    int removeByShare(const QString& shareGuid);

    // ── Aggregates ────────────────────────────────────────────────────────
    /**
     * @brief Total sold volume for a share.
     * @param shareGuid  GUID of the share.
     * @return Sum of all sold volumes, 0.0 if none.
     */
    double totalVolume(const QString& shareGuid) const;

    /**
     * @brief Total payout (saleValue - brokerage + reduction - taxSum) for a share.
     * @param shareGuid  GUID of the share.
     * @return Sum of payoutBrokerageReduction across all sales, 0.0 if none.
     */
    double totalPayoutBrokerageReduction(const QString& shareGuid) const;

    /**
     * @brief Total profit/loss including brokerage and reduction for a share.
     * @param shareGuid  GUID of the share.
     * @return Sum of profitLossBrokerageReduction across all sales, 0.0 if none.
     */
    double totalProfitLossBrokerageReduction(const QString& shareGuid) const;

    // ── Error handling ────────────────────────────────────────────────────
    QSqlError lastError() const { return m_lastError; }

private:
    SaleObject           fromQuery(const QSqlQuery& query) const;
    QList<SaleBuyDetail> loadBuyDetails(const QString& saleGuid) const;
    bool                 insertBuyDetails(const QString& saleGuid,
                                          const QList<SaleBuyDetail>& details) const;
    bool                 deleteBuyDetails(const QString& saleGuid) const;

    mutable QSqlError m_lastError; ///< Last SQL error
};

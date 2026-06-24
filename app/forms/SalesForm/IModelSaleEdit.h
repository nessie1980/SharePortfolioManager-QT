// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../../models/SaleObject.h"
#include "../../models/BuyObject.h"
#include "../../models/BrokerageObject.h"
#include "../../models/ShareObject.h"

#include <QList>
#include <QString>

/**
 * @brief Abstract model interface for the "Verkäufe" dialog.
 */
class IModelSaleEdit
{
public:
    virtual ~IModelSaleEdit() = default;

    // ── Read ──────────────────────────────────────────────────────────────

    /** Load all sales for a share, ordered by date ascending. */
    virtual QList<SaleObject>  loadSales(const QString& shareGuid) const = 0;

    /** Load the share this dialog was opened for (used for WKN/ISIN validation). */
    virtual ShareObject        loadShare(const QString& shareGuid) const = 0;

    /** Load all buys for a share that still have remaining volume. */
    virtual QList<BuyObject>   loadAvailableBuys(const QString& shareGuid) const = 0;

    /** Load all buys for a share (regardless of remaining volume). */
    virtual QList<BuyObject>   loadAllBuys(const QString& shareGuid) const = 0;

    /**
     * @brief Load available buys for a share, filtered to a specific depot
     *        and sorted oldest → newest (FIFO order).
     * @param shareGuid    GUID of the share to query.
     * @param depotNumber  Empty string returns all available buys unfiltered.
     */
    virtual QList<BuyObject>   loadAvailableBuysForDepot(const QString& shareGuid,
                                                         const QString& depotNumber) const = 0;

    /** Load the brokerage linked to a specific sale. */
    virtual BrokerageObject    loadBrokerage(const QString& saleGuid) const = 0;

    /** Load the brokerage linked to a specific buy. */
    virtual BrokerageObject    loadBrokerageForBuy(const QString& buyGuid) const = 0;

    // ── Create ────────────────────────────────────────────────────────────

    /**
     * @brief Insert a new sale + linked brokerage + sale_buy_details atomically.
     * @return true on success; rolls back on failure.
     */
    virtual bool addSale(const SaleObject& sale) = 0;

    // ── Update ────────────────────────────────────────────────────────────

    /**
     * @brief Update an existing sale + brokerage atomically.
     * @return true on success; rolls back on failure.
     */
    virtual bool updateSale(const SaleObject& sale) = 0;

    // ── Delete ────────────────────────────────────────────────────────────

    /**
     * @brief Delete a sale, its brokerage and buy details atomically.
     * @return true on success.
     */
    virtual bool removeSale(const QString& saleGuid) = 0;

    // ── Checks ────────────────────────────────────────────────────────────

    /**
     * @brief Check whether an order number is already used for this share.
     * @param shareGuid    GUID of the share to search within.
     * @param orderNumber  Order number to look up.
     * @param excludeGuid  Exclude this sale GUID from the check (use when editing).
     */
    virtual bool orderNumberExists(const QString& shareGuid,
                                   const QString& orderNumber,
                                   const QString& excludeGuid = QString()) const = 0;

    /**
     * @brief Check whether a document path is already used by any sale
     *        (across all shares).
     * @param document    Absolute file path to check.
     * @param excludeGuid Exclude this sale GUID (use when editing).
     */
    virtual bool documentExists(const QString& document,
                                const QString& excludeGuid = QString()) const = 0;

    // ── Error handling ────────────────────────────────────────────────────
    virtual QString lastError() const = 0;
};

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../../models/BuyObject.h"
#include "../../models/BrokerageObject.h"
#include "../../models/ShareObject.h"

#include <QList>
#include <QString>

/**
 * @brief Abstract model interface for the "Käufe" dialog.
 */
class IModelBuyEdit
{
public:
    virtual ~IModelBuyEdit() = default;

    // ── Read ──────────────────────────────────────────────────────────────

    /** Load all buys for a share, ordered by date ascending. */
    virtual QList<BuyObject> loadBuys(const QString& shareGuid)    const = 0;

    /** Load the share this dialog was opened for (used for WKN/ISIN validation). */
    virtual ShareObject      loadShare(const QString& shareGuid)   const = 0;

    /** Load the brokerage linked to a specific buy. */
    virtual BrokerageObject  loadBrokerage(const QString& buyGuid) const = 0;

    // ── Create ────────────────────────────────────────────────────────────

    /**
     * @brief Insert a new buy + linked brokerage atomically.
     * @return true on success; rolls back on failure.
     */
    virtual bool addBuy(const BuyObject& buy,
                        double provision = 0.0,
                        double brokerFee = 0.0,
                        double traderFee = 0.0,
                        double reduction = 0.0) = 0;

    // ── Update ────────────────────────────────────────────────────────────

    /**
     * @brief Update an existing buy + linked brokerage atomically.
     * @return true on success; rolls back on failure.
     */
    virtual bool updateBuy(const BuyObject& buy,
                           double provision = 0.0,
                           double brokerFee = 0.0,
                           double traderFee = 0.0,
                           double reduction = 0.0) = 0;

    // ── Delete ────────────────────────────────────────────────────────────

    /**
     * @brief Delete a buy and its linked brokerage atomically.
     * @return true on success.
     */
    virtual bool removeBuy(const QString& buyGuid) = 0;

    // ── Checks ────────────────────────────────────────────────────────────

    /**
     * @brief Check whether an order number is already used for this share.
     * @param shareGuid    GUID of the share to search within.
     * @param orderNumber  Order number to look up.
     * @param excludeGuid  Exclude this buy GUID from the check (use when editing).
     */
    virtual bool orderNumberExists(const QString& shareGuid,
                                   const QString& orderNumber,
                                   const QString& excludeGuid = QString()) const = 0;

    /**
     * @brief Check whether a document path is already used by any buy
     *        (across all shares).
     * @param document    Absolute file path to check.
     * @param excludeGuid Exclude this buy GUID (use when editing an existing buy).
     */
    virtual bool documentExists(const QString& document,
                                const QString& excludeGuid = QString()) const = 0;

    // ── Error handling ────────────────────────────────────────────────────
    virtual QString lastError() const = 0;
};

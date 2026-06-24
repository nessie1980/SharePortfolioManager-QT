// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../../models/BrokerageObject.h"

#include <QList>
#include <QString>

/**
 * @brief Abstract model interface for the "Kosten" dialog.
 *
 * Standalone brokerage records (no buy_guid / sale_guid set) are managed
 * here. Records that belong to a buy or sale transaction are read-only in
 * this dialog — the user may only set their document path.
 */
class IModelBrokerageEdit
{
public:
    virtual ~IModelBrokerageEdit() = default;

    // ── Read ──────────────────────────────────────────────────────────────

    /** Load all brokerage records for a share, ordered by date ascending. */
    virtual QList<BrokerageObject> loadBrokerages(const QString& shareGuid) const = 0;

    // ── Create ────────────────────────────────────────────────────────────

    /**
     * @brief Insert a new standalone brokerage record.
     * @return true on success.
     */
    virtual bool addBrokerage(const BrokerageObject& brokerage) = 0;

    // ── Update ────────────────────────────────────────────────────────────

    /**
     * @brief Update an existing brokerage record.
     * @return true on success.
     */
    virtual bool updateBrokerage(const BrokerageObject& brokerage) = 0;

    /**
     * @brief Update only the document path of a brokerage record.
     * @param guid      GUID of the record to update.
     * @param document  New document file path.
     * @return true on success.
     */
    virtual bool updateDocument(const QString& guid, const QString& document) = 0;

    // ── Delete ────────────────────────────────────────────────────────────

    /**
     * @brief Delete a standalone brokerage record by its GUID.
     * @return true on success.
     */
    virtual bool removeBrokerage(const QString& guid) = 0;

    // ── Checks ────────────────────────────────────────────────────────────

    /**
     * @brief Check whether a document path is already used by any brokerage
     *        record (across all shares).
     * @param document    Absolute file path to check.
     * @param excludeGuid Exclude this brokerage GUID (use when editing).
     */
    virtual bool documentExists(const QString& document,
                                const QString& excludeGuid = QString()) const = 0;

    // ── Error handling ────────────────────────────────────────────────────
    virtual QString lastError() const = 0;
};

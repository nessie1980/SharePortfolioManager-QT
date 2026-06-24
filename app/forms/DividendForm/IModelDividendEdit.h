// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../../models/DividendObject.h"
#include "../../models/ShareObject.h"

#include <QList>
#include <QString>

/**
 * @brief Abstract model interface for the "Dividenden" dialog.
 */
class IModelDividendEdit
{
public:
    virtual ~IModelDividendEdit() = default;

    // ── Read ──────────────────────────────────────────────────────────────

    /** Load all dividends for a share, ordered by date ascending. */
    virtual QList<DividendObject> loadDividends(const QString& shareGuid) const = 0;

    /** Load the share this dialog was opened for (used for WKN/ISIN validation). */
    virtual ShareObject           loadShare(const QString& shareGuid)     const = 0;

    // ── Create ────────────────────────────────────────────────────────────

    /**
     * @brief Insert a new dividend record.
     * @return true on success.
     */
    virtual bool addDividend(const DividendObject& dividend) = 0;

    // ── Update ────────────────────────────────────────────────────────────

    /**
     * @brief Update an existing dividend record.
     * @return true on success.
     */
    virtual bool updateDividend(const DividendObject& dividend) = 0;

    // ── Delete ────────────────────────────────────────────────────────────

    /**
     * @brief Delete a dividend record by its GUID.
     * @return true on success.
     */
    virtual bool removeDividend(const QString& dividendGuid) = 0;

    // ── Checks ────────────────────────────────────────────────────────────

    /**
     * @brief Check whether a document path is already used by any dividend
     *        (across all shares).
     * @param document    Absolute file path to check.
     * @param excludeGuid Exclude this dividend GUID (use when editing).
     */
    virtual bool documentExists(const QString& document,
                                const QString& excludeGuid = QString()) const = 0;

    // ── Error handling ────────────────────────────────────────────────────
    virtual QString lastError() const = 0;
};

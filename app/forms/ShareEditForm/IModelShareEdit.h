// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../../models/ShareObject.h"
#include "../../models/ShareSplitObject.h"

#include <QList>

/**
 * @brief Abstract model interface for the "Aktie editieren" dialog.
 *
 * Declares the data operations the Presenter needs without depending
 * on the concrete ModelShareEdit implementation or the repository layer.
 */
class IModelShareEdit
{
public:
    virtual ~IModelShareEdit() = default;

    /**
     * @brief Load a share from the database by its GUID.
     * @param guid  GUID of the share to load.
     * @return The ShareObject; isValid() == false if not found.
     */
    virtual ShareObject loadShare(const QString& guid) const = 0;

    /**
     * @brief Persist the updated master data of an existing share.
     * @param share  ShareObject with updated values (identified by guid).
     * @return true on success.
     */
    virtual bool saveShare(const ShareObject& share) = 0;

    // ── Aggregates ────────────────────────────────────────────────────────

    /** Total buy value (incl. brokerage, net reduction) for the share. */
    virtual double totalBuyValue(const QString& shareGuid) const = 0;

    /** Number of buy records for the share. */
    virtual int buyCount(const QString& shareGuid) const = 0;

    /** Total sale payout (net brokerage, reduction, taxes). */
    virtual double totalSaleValue(const QString& shareGuid) const = 0;

    /** Total profit / loss across all sales (may be negative). */
    virtual double totalProfitLoss(const QString& shareGuid) const = 0;

    /** Number of sale records for the share. */
    virtual int saleCount(const QString& shareGuid) const = 0;

    /** Total net dividend payout (after taxes). */
    virtual double totalDividendValue(const QString& shareGuid) const = 0;

    /** Number of dividend records for the share. */
    virtual int dividendCount(const QString& shareGuid) const = 0;

    /** Total brokerage costs (net of reduction). */
    virtual double totalBrokerageValue(const QString& shareGuid) const = 0;

    /** Number of standalone brokerage records for the share. */
    virtual int brokerageCount(const QString& shareGuid) const = 0;

    /**
     * @brief Currently held volume = SUM(volume) - SUM(volume_sold) across all buys.
     * This is the number of shares currently in the depot.
     */
    virtual double currentVolume(const QString& shareGuid) const = 0;

    /**
     * @brief All splits of the share, ordered by date ascending.
     *
     * Hinzugefügt 08.08.2026 (Phase 3 der Aktiensplit-Behandlung) als Grundlage
     * für die Split-Zeile in der GroupBox "Allgemein". Reine Weiterleitung an
     * ShareSplitRepository::findByShare() — die Aufbereitung passiert in der View.
     */
    virtual QList<ShareSplitObject> loadSplits(const QString& shareGuid) const = 0;

    /**
     * @brief Date of the first (earliest) buy, formatted for display.
     * Returns an empty string if no buys exist.
     */
    virtual QString firstBuyDate(const QString& shareGuid) const = 0;

    /** Human-readable description of the last error. */
    virtual QString lastError() const = 0;
};

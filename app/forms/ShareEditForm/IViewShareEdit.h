// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../../models/ShareObject.h"
#include <QString>
#include <QDate>

/**
 * @brief Abstract view interface for the "Aktie editieren" dialog.
 *
 * Defines the contract between PresenterShareEdit and ViewShareEdit.
 * All UI read/write operations go through this interface so the
 * Presenter stays independent of Qt widget internals.
 *
 * ### Field groups
 * - **Allgemein** — master data editable by the user
 * - **Einnahmen / Ausgabe** — computed summary values (read-only in view)
 */
class IViewShareEdit
{
public:
    virtual ~IViewShareEdit() = default;

    // ── Allgemein — read (user input) ─────────────────────────────────────
    virtual QString   wkn()              const = 0;
    virtual QString   isin()             const = 0;
    virtual QString   name()             const = 0;
    virtual QDate     listingDate()      const = 0;  ///< Börsennotierung
    virtual ShareType shareType()        const = 0;
    virtual QString   dividendInterval() const = 0;
    virtual QString   countryInfo()      const = 0;
    virtual QString   detailsWebsite()   const = 0;

    virtual QString          marketPriceUrl()         const = 0;
    virtual ShareParsingType marketPriceParsingType() const = 0;
    virtual QString          marketPriceApiKey()      const = 0;
    virtual QString          dailyValuesUrl()          const = 0;
    virtual ShareParsingType dailyValuesParsingType() const = 0;
    virtual QString          dailyValuesApiKey()      const = 0;

    virtual ShareUpdateType  updateType()             const = 0;

    // ── Allgemein — write (Presenter → View, populate on open) ────────────

    /**
     * @brief Pre-fill all "Allgemein" fields from the existing ShareObject.
     * @param share  The share loaded from the database.
     */
    virtual void loadShare(const ShareObject& share) = 0;

    // ── Einnahmen / Ausgabe — write (computed by Presenter) ───────────────

    /**
     * @brief Set the read-only "Datum" field (date of first buy).
     */
    virtual void setFirstBuyDate(const QString& dateStr) = 0;

    /**
     * @brief Set the read-only "Anteile" field (shares currently in depot).
     */
    virtual void setCurrentVolume(double volume) = 0;

    /**
     * @brief Display the total buy value (incl. brokerage, net of reduction).
     * @param value  Sum of buyValueBrokerageReduction across all buys.
     * @param count  Number of buy records.
     */
    virtual void setTotalBuys(double value, int count) = 0;

    /**
     * @brief Display the total sale payout (net of brokerage, reduction, taxes).
     * @param value  Sum of payoutBrokerageReduction across all sales.
     * @param count  Number of sale records.
     */
    virtual void setTotalSales(double value, int count) = 0;

    /**
     * @brief Display the total profit / loss across all sales.
     * @param value  Sum of profitLossBrokerageReduction — may be negative.
     * @param count  Number of sale records (same as setTotalSales count).
     */
    virtual void setTotalProfitLoss(double value, int count) = 0;

    /**
     * @brief Display the total net dividend payout (after taxes).
     * @param value  Sum of dividendPayoutWithTaxes across all dividends.
     * @param count  Number of dividend records.
     */
    virtual void setTotalDividends(double value, int count) = 0;

    /**
     * @brief Display the total brokerage costs (net of reduction).
     * @param value  Sum of brokerageReduction across all brokerage records.
     * @param count  Number of brokerage records.
     */
    virtual void setTotalBrokerages(double value, int count) = 0;

    // ── Feedback ──────────────────────────────────────────────────────────

    /**
     * @brief Show a user-visible error message (e.g. save failed).
     */
    virtual void showError(const QString& message) = 0;

    /**
     * @brief Close the dialog and signal acceptance.
     */
    virtual void acceptAndClose() = 0;
};

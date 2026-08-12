// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../../models/DividendObject.h"
#include "../../models/ShareObject.h"
#include "../../models/ShareSplitObject.h"

#include <QDate>
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

    /**
     * @brief Alle Splits der Aktie, aufsteigend nach Datum.
     *
     * Ergänzt 11.08.2026 (Phase 3c der Aktiensplit-Behandlung) für den
     * Split-Marker in der Anteile-Spalte der Dividenden-Übersicht. Reine
     * Weiterleitung an ShareSplitRepository::findByShare(); die Aufbereitung
     * passiert in der View. Wortgleich zu IModelBuyEdit::loadSplits().
     */
    virtual QList<ShareSplitObject> loadSplits(const QString& shareGuid) const = 0;

    /**
     * @brief Look up the closing price for a share on a given date from the
     *        `daily_values` table, e.g. to auto-fill "Preis der Aktie am
     *        Auszahlungstag" when the payout date changes.
     * @param shareGuid  GUID of the share.
     * @param date       Trading date to look up.
     * @param outPrice   Out-parameter receiving the closing price on success.
     * @return true if a daily value entry with a closing price > 0 exists
     *         for that exact date; false otherwise (@p outPrice untouched).
     */
    virtual bool findClosingPriceForDate(const QString& shareGuid,
                                         const QDate&    date,
                                         double&         outPrice) const = 0;

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

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../models/ShareObject.h"
#include <QList>
#include <QString>
#include <QSqlQuery>
#include <QSqlError>

/**
 * @brief Repository for share master data.
 *
 * Provides CRUD operations for the `shares` table in SQLite.
 * The ShareRepository is the root of the data hierarchy — all other
 * repositories (BuyRepository, SaleRepository, etc.) reference shares
 * via their `share_guid` foreign key.
 *
 * All methods operate on the shared Database::instance() connection.
 */
class ShareRepository
{
public:
    ShareRepository() = default;

    // ── Create ────────────────────────────────────────────────────────────
    /**
     * @brief Insert a new share into the database.
     * @param share  Fully populated ShareObject (must have valid guid and wkn)
     * @return true on success
     */
    bool insert(const ShareObject& share);

    // ── Read ──────────────────────────────────────────────────────────────
    /**
     * @brief Load all shares, ordered by name ascending.
     * @return List of all ShareObjects in the database.
     */
    QList<ShareObject> findAll() const;

    /**
     * @brief Find a single share by its GUID.
     * @return ShareObject (isValid() == false if not found)
     */
    ShareObject findByGuid(const QString& guid) const;

    /**
     * @brief Find a share by its WKN.
     * @return ShareObject (isValid() == false if not found)
     */
    ShareObject findByWkn(const QString& wkn) const;

    /**
     * @brief Find a share by its ISIN.
     * @return ShareObject (isValid() == false if not found)
     */
    ShareObject findByIsin(const QString& isin) const;

    /**
     * @brief Check if a WKN already exists in the database.
     * @param wkn  WKN string to check.
     * @return true if the WKN exists, false otherwise.
     */
    bool wknExists(const QString& wkn) const;

    /**
     * @brief Check if an ISIN already exists in the database.
     * @param isin  ISIN string to check.
     * @return true if the ISIN exists, false otherwise.
     */
    bool isinExists(const QString& isin) const;

    // ── Update ────────────────────────────────────────────────────────────
    /**
     * @brief Update all fields of an existing share.
     * @param share  ShareObject with updated values (identified by guid)
     * @return true on success
     */
    bool update(const ShareObject& share);

    /**
     * @brief Update only the current and previous day price.
     */
    bool updatePrice(const QString& guid, double curPrice, double prevDayPrice,
                     const QString& lastPriceUpdate);

    /**
     * @brief Update only the last internet update timestamp.
     */
    bool updateLastInternetUpdate(const QString& guid, const QString& lastUpdate);

    // ── Delete ────────────────────────────────────────────────────────────
    /**
     * @brief Delete a share by GUID.
     *
     * Due to `ON DELETE CASCADE`, all related buys, sales, dividends
     * and daily values are deleted automatically.
     * @return true on success
     */
    bool remove(const QString& guid);

    // ── Error handling ────────────────────────────────────────────────────
    QSqlError lastError() const { return m_lastError; }

private:
    /**
     * @brief Construct a ShareObject from the current row of a QSqlQuery result.
     */
    ShareObject fromQuery(const QSqlQuery& query) const;

    mutable QSqlError m_lastError; ///< Last SQL error
};

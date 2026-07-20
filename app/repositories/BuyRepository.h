// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../models/BuyObject.h"
#include <QList>
#include <QString>
#include <QSqlQuery>
#include <QSqlError>

/**
 * @brief Repository for buy transactions.
 *
 * Provides CRUD operations for the `buys` table in SQLite.
 * All methods operate on the shared Database::instance() connection.
 *
 * ### Typical usage
 * @code
 * BuyRepository repo;
 * auto buys = repo.findByShare("share-guid-123");
 * for (const auto& buy : buys)
 *     qDebug() << buy.dateTime() << buy.buyValueBrokerageReduction();
 * @endcode
 */
class BuyRepository
{
public:
    BuyRepository() = default;

    // ── Create ────────────────────────────────────────────────────────────
    /**
     * @brief Insert a new buy record into the database.
     * @param buy  Fully populated BuyObject (must have valid guid and shareGuid)
     * @return true on success
     */
    bool insert(const BuyObject& buy);

    // ── Read ──────────────────────────────────────────────────────────────
    /**
     * @brief Find all buys for a given share, ordered by date ascending.
     * @param shareGuid  GUID of the parent share.
     * @return List of BuyObjects, empty if none found.
     */
    QList<BuyObject> findByShare(const QString& shareGuid) const;

    /**
     * @brief Find a single buy by its GUID.
     * @return BuyObject (isValid() == false if not found)
     */
    BuyObject findByGuid(const QString& guid) const;

    /**
     * @brief Find all buys for a given share and year.
     * @param shareGuid  GUID of the parent share.
     * @param year       4-digit year (e.g. 2024).
     * @return List of BuyObjects for that year, empty if none found.
     */
    QList<BuyObject> findByShareAndYear(const QString& shareGuid, int year) const;

    /**
     * @brief Check if an order number already exists for a given share.
     * @param shareGuid    GUID of the share
     * @param orderNumber  Order number to check
     */
    bool orderNumberExists(const QString& shareGuid, const QString& orderNumber) const;

    // ── Update ────────────────────────────────────────────────────────────
    /**
     * @brief Update all fields of an existing buy record.
     * @param buy  BuyObject with updated values (identified by guid)
     * @return true on success
     */
    bool update(const BuyObject& buy);

    /**
     * @brief Update only the volumeSold field of a buy.
     * @param guid        GUID of the buy
     * @param volumeSold  New sold volume value
     */
    bool updateVolumeSold(const QString& guid, double volumeSold);

    /**
     * @brief Update only the document path of a buy.
     * @param guid      GUID of the buy
     * @param document  New document path
     */
    bool updateDocument(const QString& guid, const QString& document);

    /**
     * @brief Update only the brokerage_guid link of a buy — the forward FK
     * that totalBuyValueBrokerageReduction() JOINt ("LEFT JOIN brokerage br
     * ON br.guid = b.brokerage_guid"). Bugfix 20.07.2026, analog zu
     * SaleRepository::updateBrokerageGuid() (15.07.2026): ModelBuyEdit::
     * updateBuy() legte im "kein Brokerage vorhanden"-Zweig bisher einen
     * neuen Brokerage-Eintrag nur mit dem Rückwärts-Link (brokerage.buy_guid)
     * an, ohne buys.brokerage_guid zu setzen — der JOIN in
     * totalBuyValueBrokerageReduction() lieferte dadurch weiterhin 0 für
     * Provision/Gebühren/Rabatt dieses Kaufs, obwohl der Brokerage-Datensatz
     * selbst korrekt existierte (siehe ARCHITECTURE.md).
     * @param guid           GUID des Kaufs.
     * @param brokerageGuid  GUID des verknüpften Brokerage-Eintrags.
     * @return true on success.
     */
    bool updateBrokerageGuid(const QString& guid, const QString& brokerageGuid);

    // ── Delete ────────────────────────────────────────────────────────────
    /**
     * @brief Delete a buy by its GUID.
     * @param guid      GUID of the buy
     * @return true on success
     */
    bool remove(const QString& guid);

    /**
     * @brief Delete all buys for a given share.
     * @return Number of deleted rows, or -1 on error
     */
    int removeByShare(const QString& shareGuid);

    // ── Aggregates ────────────────────────────────────────────────────────
    /**
     * @brief Total volume of all buys for a share (sum of volume).
     */
    double totalVolume(const QString& shareGuid) const;

    /**
     * @brief Total buy value including brokerage minus reduction for a share.
     */
    double totalBuyValueBrokerageReduction(const QString& shareGuid) const;

    // ── Error handling ────────────────────────────────────────────────────
    /// Returns the last SQL error, if any.
    QSqlError lastError() const { return m_lastError; }

private:
    /**
     * @brief Construct a BuyObject from the current row of a QSqlQuery result.
     * @param query  An executed query positioned on the row to read
     * @return Populated BuyObject
     */
    BuyObject fromQuery(const QSqlQuery& query) const;

    mutable QSqlError m_lastError; ///< Last SQL error, set on any failed operation
};

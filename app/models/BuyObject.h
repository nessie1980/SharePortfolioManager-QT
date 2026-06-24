// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QString>
#include <QDateTime>
#include <QLocale>

/**
 * @brief Represents a single share purchase transaction.
 *
 * Stores the core transaction data. Brokerage details (provision, broker_fee,
 * trader_fee, reduction) are stored exclusively in the linked BrokerageObject
 * (referenced via brokerageGuid). This avoids duplicate data storage.
 *
 * ### Calculated values
 * | Property | Formula |
 * |----------|---------|
 * | `buyValue()` | volume × price |
 */
class BuyObject
{
public:
    BuyObject() = default;

    /**
     * @brief Full constructor.
     * @param guid          Unique identifier (UUID string)
     * @param shareGuid     GUID of the parent share
     * @param depotNumber   Depot number
     * @param orderNumber   Broker order number (unique per share)
     * @param dateTime      Date and time (ISO 8601)
     * @param volume        Number of shares bought (> 0)
     * @param volumeSold    Shares already sold from this buy
     * @param price         Price per share (> 0)
     * @param brokerageGuid GUID of the linked BrokerageObject
     * @param document      Path to purchase document (optional)
     */
    BuyObject(const QString& guid,
              const QString& shareGuid,
              const QString& depotNumber,
              const QString& orderNumber,
              const QString& dateTime,
              double volume,
              double volumeSold,
              double price,
              const QString& brokerageGuid = QString(),
              const QString& document      = QString());

    // ── Identity ──────────────────────────────────────────────────────────
    QString guid()          const { return m_guid; }
    QString shareGuid()     const { return m_shareGuid; }
    QString depotNumber()   const { return m_depotNumber; }
    QString orderNumber()   const { return m_orderNumber; }

    // ── Date / Time ───────────────────────────────────────────────────────
    QString dateTime()  const { return m_dateTime; }
    QDate   date()      const { return QDateTime::fromString(m_dateTime, Qt::ISODate).date(); }
    QString dateAsStr() const { return QLocale().toString(date(), QLocale::ShortFormat); }
    int     year()      const { return date().year(); }

    // ── Transaction values ────────────────────────────────────────────────
    double volume()     const { return m_volume; }
    double volumeSold() const { return m_volumeSold; }
    double price()      const { return m_price; }

    void setVolumeSold(double value) { m_volumeSold = value; }

    // ── Brokerage link ────────────────────────────────────────────────────
    /// GUID of the linked BrokerageObject. Use BrokerageRepository::findByBuyGuid()
    /// to retrieve brokerage details.
    QString brokerageGuid() const { return m_brokerageGuid; }

    // ── Calculated values ─────────────────────────────────────────────────
    /// Total buy value = volume × price
    double buyValue() const { return m_volume * m_price; }

    // ── Document ──────────────────────────────────────────────────────────
    QString document()                    const { return m_document; }
    void    setDocument(const QString& d)       { m_document = d; }

    // ── Validity ──────────────────────────────────────────────────────────
    bool isValid() const { return !m_guid.isEmpty() && m_volume > 0 && m_price > 0; }

private:
    QString m_guid;
    QString m_shareGuid;
    QString m_depotNumber;
    QString m_orderNumber;
    QString m_dateTime;

    double  m_volume     = 0.0;
    double  m_volumeSold = 0.0;
    double  m_price      = 0.0;

    QString m_brokerageGuid;
    QString m_document;
};

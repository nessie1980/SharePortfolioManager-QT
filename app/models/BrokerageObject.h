// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QString>
#include <QDateTime>
#include <QLocale>

/**
 * @brief Represents a standalone brokerage record.
 *
 * A brokerage record captures the fees for a single buy or sale transaction.
 * It is optionally linked to a buy (via buyGuid) or a sale (via saleGuid).
 *
 * ### Calculated values
 * | Property | Formula |
 * |----------|---------||
 * | `brokerage()` | provision + brokerFee + traderFee |
 * | `brokerageReduction()` | brokerage - reduction |
 */
class BrokerageObject
{
public:
    BrokerageObject() = default;

    /**
     * @brief Full constructor — calculates all derived values automatically.
     * @param guid         Unique identifier of this brokerage record (UUID string).
     * @param shareGuid    GUID of the parent share.
     * @param buyGuid      GUID of the associated buy transaction (optional).
     * @param saleGuid     GUID of the associated sale transaction (optional).
     * @param dateTime     Date and time of the brokerage record (ISO 8601).
     * @param provision    Broker provision fee (default 0).
     * @param brokerFee    Broker fee (default 0).
     * @param traderFee    Trading venue fee (default 0).
     * @param reduction    Reduction applied to brokerage (default 0).
     * @param document     File path to the brokerage document (optional).
     */
    BrokerageObject(const QString& guid,
                    const QString& shareGuid,
                    const QString& buyGuid,
                    const QString& saleGuid,
                    const QString& dateTime,
                    double provision  = 0.0,
                    double brokerFee  = 0.0,
                    double traderFee  = 0.0,
                    double reduction  = 0.0,
                    const QString& document = QString());

    // ── Identity ──────────────────────────────────────────────────────────
    QString guid()      const { return m_guid; }      ///< Unique identifier (UUID)
    QString shareGuid() const { return m_shareGuid; } ///< Parent share GUID
    QString buyGuid()   const { return m_buyGuid; }   ///< Associated buy GUID (may be empty)
    QString saleGuid()  const { return m_saleGuid; }  ///< Associated sale GUID (may be empty)

    // ── Date / Time ───────────────────────────────────────────────────────
    QString dateTime()  const { return m_dateTime; }                                            ///< ISO 8601 datetime string
    QDate   date()      const { return QDateTime::fromString(m_dateTime, Qt::ISODate).date(); } ///< Record date as QDate
    QString dateAsStr() const { return QLocale().toString(date(), QLocale::ShortFormat); }      ///< Date formatted for display
    int     year()      const { return date().year(); }                                         ///< Year of the record

    // ── Fee inputs ────────────────────────────────────────────────────────
    double provision()  const { return m_provision; }  ///< Broker provision fee
    double brokerFee()  const { return m_brokerFee; }  ///< Broker fee
    double traderFee()  const { return m_traderFee; }  ///< Trading venue fee
    double reduction()  const { return m_reduction; }  ///< Reduction on brokerage

    // ── Calculated values ─────────────────────────────────────────────────
    /// Total brokerage = provision + brokerFee + traderFee
    double brokerage()          const { return m_brokerage; }
    /// Total brokerage minus reduction = brokerage - reduction
    double brokerageReduction() const { return m_brokerageReduction; }

    // ── Document ──────────────────────────────────────────────────────────
    QString document()                            const { return m_document; }     ///< Path to brokerage document
    void    setDocument(const QString& document)        { m_document = document; } ///< Update document path

    // ── Validity ──────────────────────────────────────────────────────────
    /**
     * @brief Returns true if this BrokerageObject contains valid data.
     *
     * A BrokerageObject is valid when it has a non-empty GUID and shareGuid.
     * @return true if valid.
     */
    bool isValid() const { return !m_guid.isEmpty() && !m_shareGuid.isEmpty(); }

private:
    /// Recalculates all derived monetary values.
    void calculateValues();

    QString m_guid;
    QString m_shareGuid;
    QString m_buyGuid;
    QString m_saleGuid;
    QString m_dateTime;

    double  m_provision  = 0.0;
    double  m_brokerFee  = 0.0;
    double  m_traderFee  = 0.0;
    double  m_reduction  = 0.0;

    double  m_brokerage          = 0.0;
    double  m_brokerageReduction = 0.0;

    QString m_document;
};

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QString>
#include <QDateTime>
#include <QList>
#include <QLocale>

/**
 * @brief Details of a single buy that contributed to a sale.
 *
 * Brokerage fields (reductionPart, brokeragePart) remain here because they
 * represent the proportional share of the original buy's brokerage that is
 * attributed to this partial sale — they are not duplicates of BrokerageObject.
 */
class SaleBuyDetail
{
public:
    SaleBuyDetail() = default;

    SaleBuyDetail(const QString& buyGuid,
                  const QString& dateTime,
                  double volume,
                  double buyPrice,
                  double reductionPart  = 0.0,
                  double brokeragePart  = 0.0);

    QString buyGuid()       const { return m_buyGuid; }
    QString dateTime()      const { return m_dateTime; }
    double  volume()        const { return m_volume; }
    double  buyPrice()      const { return m_buyPrice; }
    double  reductionPart() const { return m_reductionPart; }
    double  brokeragePart() const { return m_brokeragePart; }

    double saleBuyValue()                   const { return m_saleBuyValue; }
    double saleBuyValueReduction()          const { return m_saleBuyValueReduction; }
    double saleBuyValueBrokerage()          const { return m_saleBuyValueBrokerage; }
    double saleBuyValueBrokerageReduction() const { return m_saleBuyValueBrokerageReduction; }

private:
    QString m_buyGuid;
    QString m_dateTime;
    double  m_volume         = 0.0;
    double  m_buyPrice       = 0.0;
    double  m_reductionPart  = 0.0;
    double  m_brokeragePart  = 0.0;

    double  m_saleBuyValue                   = 0.0;
    double  m_saleBuyValueReduction          = 0.0;
    double  m_saleBuyValueBrokerage          = 0.0;
    double  m_saleBuyValueBrokerageReduction = 0.0;
};

// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Represents a single share sale transaction.
 *
 * Brokerage details (provision, broker_fee, trader_fee, reduction) are stored
 * exclusively in the linked BrokerageObject. When the repository loads a sale
 * it performs a JOIN on the brokerage table and passes the brokerage values
 * directly into the constructor so all calculated values are correct.
 *
 * ### Calculated values
 * | Property | Formula |
 * |----------|---------|
 * | `taxSum()` | taxAtSource + capitalGainsTax + solidarityTax |
 * | `saleValue()` | volume × salePrice |
 * | `brokerage()` | provision + brokerFee + traderFee (from BrokerageObject) |
 * | `profitLossBrokerageReduction()` | (saleValue - brokerage + reduction) - buyValueBrokerageReduction - taxSum |
 * | `payoutBrokerageReduction()` | saleValue - brokerage + reduction - taxSum |
 */
class SaleObject
{
public:
    SaleObject() = default;

    /**
     * @brief Full constructor.
     *
     * The brokerage parameters (provision, brokerFee, traderFee, reduction)
     * are supplied by SaleRepository from a JOIN on the brokerage table.
     * They are NOT stored in the sales table.
     */
    SaleObject(const QString& guid,
               const QString& shareGuid,
               const QString& depotNumber,
               const QString& orderNumber,
               const QString& dateTime,
               double volume,
               double salePrice,
               const QList<SaleBuyDetail>& saleBuyDetails,
               double taxAtSource       = 0.0,
               double capitalGainsTax   = 0.0,
               double solidarityTax     = 0.0,
               const QString& brokerageGuid = QString(),
               double provision         = 0.0,
               double brokerFee         = 0.0,
               double traderFee         = 0.0,
               double reduction         = 0.0,
               const QString& document  = QString());

    // ── Identity ──────────────────────────────────────────────────────────
    QString guid()        const { return m_guid; }
    QString shareGuid()   const { return m_shareGuid; }
    QString depotNumber() const { return m_depotNumber; }
    QString orderNumber() const { return m_orderNumber; }

    // ── Date / Time ───────────────────────────────────────────────────────
    QString dateTime()  const { return m_dateTime; }
    QDate   date()      const { return QDateTime::fromString(m_dateTime, Qt::ISODate).date(); }
    QString dateAsStr() const { return QLocale().toString(date(), QLocale::ShortFormat); }
    int     year()      const { return date().year(); }

    // ── Transaction values ────────────────────────────────────────────────
    double volume()    const { return m_volume; }
    double salePrice() const { return m_salePrice; }

    // ── Tax values ────────────────────────────────────────────────────────
    double taxAtSource()     const { return m_taxAtSource; }
    double capitalGainsTax() const { return m_capitalGainsTax; }
    double solidarityTax()   const { return m_solidarityTax; }
    double taxSum()          const { return m_taxSum; }

    // ── Brokerage link + values (loaded via JOIN in SaleRepository) ───────
    QString brokerageGuid() const { return m_brokerageGuid; }
    double  provision()     const { return m_provision; }
    double  brokerFee()     const { return m_brokerFee; }
    double  traderFee()     const { return m_traderFee; }
    double  reduction()     const { return m_reduction; }
    double  brokerage()     const { return m_brokerage; }

    // ── Sale buy details ──────────────────────────────────────────────────
    QList<SaleBuyDetail> saleBuyDetails() const { return m_saleBuyDetails; }

    // ── Calculated sale values ────────────────────────────────────────────
    double saleValue()                   const { return m_saleValue; }
    double saleValueBrokerage()          const { return m_saleValueBrokerage; }
    double saleValueReduction()          const { return m_saleValueReduction; }
    double saleValueBrokerageReduction() const { return m_saleValueBrokerageReduction; }

    // ── Calculated buy values (from saleBuyDetails) ───────────────────────
    double buyValue()                   const { return m_buyValue; }
    double buyValueReduction()          const { return m_buyValueReduction; }
    double buyValueBrokerage()          const { return m_buyValueBrokerage; }
    double buyValueBrokerageReduction() const { return m_buyValueBrokerageReduction; }

    // ── Profit / Loss ─────────────────────────────────────────────────────
    double profitLoss()                   const { return m_profitLoss; }
    double profitLossBrokerage()          const { return m_profitLossBrokerage; }
    double profitLossReduction()          const { return m_profitLossReduction; }
    double profitLossBrokerageReduction() const { return m_profitLossBrokerageReduction; }

    // ── Payout ────────────────────────────────────────────────────────────
    double payout()                   const { return m_payout; }
    double payoutBrokerage()          const { return m_payoutBrokerage; }
    double payoutReduction()          const { return m_payoutReduction; }
    double payoutBrokerageReduction() const { return m_payoutBrokerageReduction; }

    // ── Document ──────────────────────────────────────────────────────────
    QString document()                           const { return m_document; }
    void    setDocument(const QString& document)       { m_document = document; }

    // ── Validity ──────────────────────────────────────────────────────────
    bool isValid() const { return !m_guid.isEmpty() && m_volume > 0 && m_salePrice > 0; }

private:
    void calculateValues();

    QString m_guid;
    QString m_shareGuid;
    QString m_depotNumber;
    QString m_orderNumber;
    QString m_dateTime;

    double  m_volume     = 0.0;
    double  m_salePrice  = 0.0;

    double  m_taxAtSource     = 0.0;
    double  m_capitalGainsTax = 0.0;
    double  m_solidarityTax   = 0.0;
    double  m_taxSum          = 0.0;

    // Brokerage values — loaded via JOIN from brokerage table, NOT stored in sales table
    QString m_brokerageGuid;
    double  m_provision  = 0.0;
    double  m_brokerFee  = 0.0;
    double  m_traderFee  = 0.0;
    double  m_reduction  = 0.0;
    double  m_brokerage  = 0.0;

    QList<SaleBuyDetail> m_saleBuyDetails;

    double  m_saleValue                   = 0.0;
    double  m_saleValueBrokerage          = 0.0;
    double  m_saleValueReduction          = 0.0;
    double  m_saleValueBrokerageReduction = 0.0;

    double  m_buyValue                   = 0.0;
    double  m_buyValueReduction          = 0.0;
    double  m_buyValueBrokerage          = 0.0;
    double  m_buyValueBrokerageReduction = 0.0;

    double  m_profitLoss                   = 0.0;
    double  m_profitLossBrokerage          = 0.0;
    double  m_profitLossReduction          = 0.0;
    double  m_profitLossBrokerageReduction = 0.0;

    double  m_payout                   = 0.0;
    double  m_payoutBrokerage          = 0.0;
    double  m_payoutReduction          = 0.0;
    double  m_payoutBrokerageReduction = 0.0;

    QString m_document;
};

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "SaleObject.h"

// ── SaleBuyDetail ─────────────────────────────────────────────────────────────

SaleBuyDetail::SaleBuyDetail(const QString& buyGuid,
                              const QString& dateTime,
                              double volume,
                              double buyPrice,
                              double reductionPart,
                              double brokeragePart)
    : m_buyGuid(buyGuid)
    , m_dateTime(dateTime)
    , m_volume(volume)
    , m_buyPrice(buyPrice)
    , m_reductionPart(reductionPart)
    , m_brokeragePart(brokeragePart)
{
    m_saleBuyValue                   = volume * buyPrice;
    m_saleBuyValueReduction          = volume * buyPrice - reductionPart;
    m_saleBuyValueBrokerage          = volume * buyPrice + brokeragePart;
    m_saleBuyValueBrokerageReduction = volume * buyPrice + brokeragePart - reductionPart;
}

// ── SaleObject ────────────────────────────────────────────────────────────────

SaleObject::SaleObject(const QString& guid,
                       const QString& shareGuid,
                       const QString& depotNumber,
                       const QString& orderNumber,
                       const QString& dateTime,
                       double volume,
                       double salePrice,
                       const QList<SaleBuyDetail>& saleBuyDetails,
                       double taxAtSource,
                       double capitalGainsTax,
                       double solidarityTax,
                       const QString& brokerageGuid,
                       double provision,
                       double brokerFee,
                       double traderFee,
                       double reduction,
                       const QString& document)
    : m_guid(guid)
    , m_shareGuid(shareGuid)
    , m_depotNumber(depotNumber)
    , m_orderNumber(orderNumber)
    , m_dateTime(dateTime)
    , m_volume(volume)
    , m_salePrice(salePrice)
    , m_taxAtSource(taxAtSource)
    , m_capitalGainsTax(capitalGainsTax)
    , m_solidarityTax(solidarityTax)
    , m_taxSum(taxAtSource + capitalGainsTax + solidarityTax)
    , m_brokerageGuid(brokerageGuid)
    , m_provision(provision)
    , m_brokerFee(brokerFee)
    , m_traderFee(traderFee)
    , m_reduction(reduction)
    , m_brokerage(provision + brokerFee + traderFee)
    , m_saleBuyDetails(saleBuyDetails)
    , m_document(document)
{
    calculateValues();
}

void SaleObject::calculateValues()
{
    // Sale values
    m_saleValue                   = m_volume * m_salePrice;
    m_saleValueBrokerage          = m_saleValue - m_brokerage;
    m_saleValueReduction          = m_saleValue + m_reduction;
    m_saleValueBrokerageReduction = m_saleValue - m_brokerage + m_reduction;

    // Buy values — sum over all contributing buy details
    m_buyValue                   = 0.0;
    m_buyValueReduction          = 0.0;
    m_buyValueBrokerage          = 0.0;
    m_buyValueBrokerageReduction = 0.0;

    for (const auto& detail : std::as_const(m_saleBuyDetails)) {
        m_buyValue                   += detail.saleBuyValue();
        m_buyValueReduction          += detail.saleBuyValueReduction();
        m_buyValueBrokerage          += detail.saleBuyValueBrokerage();
        m_buyValueBrokerageReduction += detail.saleBuyValueBrokerageReduction();
    }

    // Profit / loss
    m_profitLoss                   = m_saleValue                   - m_buyValue                   - m_taxSum;
    m_profitLossBrokerage          = m_saleValueBrokerage          - m_buyValueBrokerage          - m_taxSum;
    m_profitLossReduction          = m_saleValueReduction          - m_buyValueReduction          - m_taxSum;
    m_profitLossBrokerageReduction = m_saleValueBrokerageReduction - m_buyValueBrokerageReduction - m_taxSum;

    // Payout
    m_payout                   = m_saleValue                   - m_taxSum;
    m_payoutBrokerage          = m_saleValueBrokerage          - m_taxSum;
    m_payoutReduction          = m_saleValueReduction          - m_taxSum;
    m_payoutBrokerageReduction = m_saleValueBrokerageReduction - m_taxSum;
}

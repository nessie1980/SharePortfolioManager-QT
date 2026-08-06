// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ModelPortfolioChart.h"

#include "../../models/ShareObject.h"
#include "../../models/BuyObject.h"
#include "../../models/SaleObject.h"
#include "../../models/DividendObject.h"
#include "../../models/BrokerageObject.h"
#include "../../models/DailyValuesObject.h"

// -- loadPortfolioInput --------------------------------------------------------

QList<PortfolioShareSeriesInput> ModelPortfolioChart::loadPortfolioInput() const
{
    QList<PortfolioShareSeriesInput> result;

    const QList<ShareObject> shares = m_shareRepo.findAll();
    result.reserve(shares.size());

    for (const ShareObject& share : shares) {
        PortfolioShareSeriesInput input;
        input.shareGuid = share.guid();
        input.name      = share.name();

        for (const BuyObject& buy : m_buyRepo.findByShare(share.guid()))
            input.buys.append(PortfolioBuyEvent{ buy.date(), buy.volume(), buy.price() });

        // Die per JOIN mitgeladene Brokerage der Sale wird hier bewusst NICHT
        // verwendet: sämtliche Gebühren kommen über die Brokerage-Einträge
        // unten herein. Würde der Erlös sie zusätzlich abziehen, wären sie
        // doppelt gerechnet (siehe PortfolioSeriesCalculator.h).
        for (const SaleObject& sale : m_saleRepo.findByShare(share.guid()))
            input.sales.append(PortfolioSaleEvent{ sale.date(), sale.volume(),
                                                   sale.salePrice(), sale.taxSum() });

        for (const DividendObject& dividend : m_dividendRepo.findByShare(share.guid()))
            input.dividends.append(PortfolioDividendEvent{ dividend.date(),
                                                           dividend.dividendPayoutWithTaxes() });

        // findByShare() liefert kauf-, verkaufsgebundene UND freistehende
        // Einträge. Alle drei gehören in den Kosten-Term (Nessies Vorgabe
        // 05.08.2026) — brokerageReduction() ist bereits
        // provision + brokerFee + traderFee - reduction.
        for (const BrokerageObject& brokerage : m_brokerageRepo.findByShare(share.guid()))
            input.costs.append(PortfolioCostEvent{ brokerage.date(),
                                                   brokerage.brokerageReduction() });

        for (const DailyValuesObject& daily : m_dailyValuesRepo.findByShare(share.guid()))
            input.prices.append(PortfolioPriceEvent{ daily.date(), daily.closingPrice() });

        result.append(input);
    }

    return result;
}

// -- earliestRelevantDate ------------------------------------------------------

QDate ModelPortfolioChart::earliestRelevantDate() const
{
    const QList<ShareObject> shares = m_shareRepo.findAll();

    QDate earliestBuy;
    for (const ShareObject& share : shares) {
        for (const BuyObject& buy : m_buyRepo.findByShare(share.guid())) {
            const QDate date = buy.date();
            if (!date.isValid())
                continue;
            if (!earliestBuy.isValid() || date < earliestBuy)
                earliestBuy = date;
        }
    }

    if (earliestBuy.isValid())
        return earliestBuy;

    // Noch kein Kauf vorhanden — dann bleibt als Anhaltspunkt nur die
    // Kurshistorie, damit "Anzahl" überhaupt einen sinnvollen Spielraum hat.
    QDate earliestPrice;
    for (const ShareObject& share : shares) {
        const QDate date = m_dailyValuesRepo.earliestDate(share.guid());
        if (!date.isValid())
            continue;
        if (!earliestPrice.isValid() || date < earliestPrice)
            earliestPrice = date;
    }

    return earliestPrice;
}

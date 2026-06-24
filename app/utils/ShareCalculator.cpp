// MIT License — spm-qt
#include "ShareCalculator.h"
#include "../repositories/BuyRepository.h"
#include "../repositories/SaleRepository.h"
#include "../repositories/DividendRepository.h"
#include "../repositories/BrokerageRepository.h"
#include "../models/BuyObject.h"
#include "../models/SaleObject.h"

#include <QList>
#include <QMap>

// ── compute ───────────────────────────────────────────────────────────────────

ShareValues ShareCalculator::compute(const QString& guid,
                                     double curPrice,
                                     double prevDayPrice)
{
    ShareValues v;
    v.curPrice     = curPrice;
    v.prevDayPrice = prevDayPrice;

    BuyRepository       buyRepo;
    SaleRepository      saleRepo;
    DividendRepository  divRepo;
    BrokerageRepository brokerageRepo;

    // ── Previous day ──────────────────────────────────────────────────────
    v.prevDayDiff = curPrice - prevDayPrice;
    v.prevDayPct  = (prevDayPrice > 0.0)
                    ? (v.prevDayDiff / prevDayPrice * 100.0)
                    : 0.0;

    // ── Load all buys and sales ───────────────────────────────────────────
    const QList<BuyObject>  buys  = buyRepo.findByShare(guid);
    const QList<SaleObject> sales = saleRepo.findByShare(guid);

    // Build a map: buyGuid → volume already sold (from SaleBuyDetails)
    QMap<QString, double> soldVolumeByBuy;
    for (const SaleObject& sale : sales) {
        for (const SaleBuyDetail& detail : sale.saleBuyDetails()) {
            soldVolumeByBuy[detail.buyGuid()] += detail.volume();
        }
    }

    // ── Marktwert-Tab calculations ────────────────────────────────────────
    //
    // purchaseValueMarket  = Σ(buyVolume × buyPrice)           [pure buy cost, no brokerage]
    // profitLossMarket     = Σ((curPrice - buyPrice) × remaining volume) per buy
    // curValue             = remaining net volume × curPrice
    // saleProfitLossMarket = Σ sale.profitLoss()               [saleValue - buyValue, no brokerage]
    // marketValue          = curValue + saleProfitLossMarket

    double purchaseValueMarket  = 0.0;
    double profitLossMarket     = 0.0;
    double netVolume            = 0.0;

    for (const BuyObject& buy : buys) {
        const double soldVol      = soldVolumeByBuy.value(buy.guid(), 0.0);
        const double remainingVol = buy.volume() - soldVol;

        // Total buy cost (no brokerage) — used for Einzahlung
        purchaseValueMarket += buy.volume() * buy.price();

        if (remainingVol > 0.0) {
            // Aktuelle Entwicklung: (curPrice - buyPrice) × remaining
            profitLossMarket += (curPrice - buy.price()) * remainingVol;
            netVolume        += remainingVol;
        }
    }

    // Realized profit/loss from sales (pure: saleValue - buyValue, no brokerage)
    double saleProfitLossMarket = 0.0;
    for (const SaleObject& sale : sales)
        saleProfitLossMarket += sale.profitLoss();

    v.volume              = netVolume;
    v.purchaseValue       = purchaseValueMarket;
    v.curValue            = netVolume * curPrice;
    v.profitLoss          = profitLossMarket;
    v.profitLossPct       = (purchaseValueMarket > 0.0)
                            ? (profitLossMarket / purchaseValueMarket * 100.0)
                            : 0.0;
    v.saleProfitLoss      = saleProfitLossMarket;
    v.marketValue         = v.curValue + saleProfitLossMarket;
    v.marketValuePct      = (purchaseValueMarket > 0.0)
                            ? (v.marketValue / purchaseValueMarket * 100.0 - 100.0)
                            : 0.0;

    // ── Depotwert-Tab extras ──────────────────────────────────────────────
    v.totalBrokerage  = brokerageRepo.totalBrokerage(guid);
    v.totalDividend   = divRepo.totalPayoutWithTaxes(guid);

    // completePurchase = purchaseValueMarket (same base)
    v.completePurchase   = purchaseValueMarket;
    // completeCurValue = curValue + dividends + saleProfitLoss (no brokerage)
    v.completeCurValue   = v.curValue + v.totalDividend + saleProfitLossMarket;
    v.completeProfitLoss = v.completeCurValue - v.completePurchase;
    v.completeProfitPct  = (v.completePurchase > 0.0)
                           ? (v.completeProfitLoss / v.completePurchase * 100.0)
                           : 0.0;

    return v;
}

// ── portfolioTotalsMarket ─────────────────────────────────────────────────────

void ShareCalculator::portfolioTotalsMarket(const QList<ShareValues>& values,
                                             double& totalPurchase,
                                             double& totalCurValue,
                                             double& totalMarketValue,
                                             double& totalProfitLoss,
                                             double& totalProfitPct,
                                             double& totalMarketValuePct)
{
    totalPurchase     = 0.0;
    totalCurValue     = 0.0;
    totalMarketValue  = 0.0;
    totalProfitLoss   = 0.0;
    totalProfitPct    = 0.0;
    totalMarketValuePct = 0.0;

    for (const ShareValues& v : values) {
        totalPurchase    += v.purchaseValue;
        totalCurValue    += v.curValue;
        totalMarketValue += v.marketValue;
        totalProfitLoss  += v.profitLoss;
    }

    totalProfitPct     = (totalPurchase > 0.0)
                         ? (totalProfitLoss / totalPurchase * 100.0)
                         : 0.0;
    totalMarketValuePct = (totalPurchase > 0.0)
                          ? (totalMarketValue / totalPurchase * 100.0 - 100.0)
                          : 0.0;
}

// ── portfolioTotalsFinal ──────────────────────────────────────────────────────

void ShareCalculator::portfolioTotalsFinal(const QList<ShareValues>& values,
                                            double& totalBrokDividend,
                                            double& totalPurchase,
                                            double& totalCurValue,
                                            double& totalProfitLoss,
                                            double& totalProfitPct,
                                            double& completePurchase,
                                            double& completeCurValue,
                                            double& completeProfitLoss,
                                            double& completeProfitPct)
{
    totalBrokDividend  = 0.0;
    totalPurchase      = 0.0;
    totalCurValue      = 0.0;
    totalProfitLoss    = 0.0;
    totalProfitPct     = 0.0;
    completePurchase   = 0.0;
    completeCurValue   = 0.0;
    completeProfitLoss = 0.0;
    completeProfitPct  = 0.0;

    for (const ShareValues& v : values) {
        totalBrokDividend += v.totalBrokerage + v.totalDividend;
        totalPurchase     += v.purchaseValue;
        totalCurValue     += v.curValue;
        completePurchase  += v.completePurchase;
        completeCurValue  += v.completeCurValue;
    }

    totalProfitLoss    = totalCurValue - totalPurchase;
    totalProfitPct     = (totalPurchase > 0.0)
                         ? (totalProfitLoss / totalPurchase * 100.0)
                         : 0.0;
    completeProfitLoss = completeCurValue - completePurchase;
    completeProfitPct  = (completePurchase > 0.0)
                         ? (completeProfitLoss / completePurchase * 100.0)
                         : 0.0;
}

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QString>
#include <QList>

/**
 * @brief Aggregated financial values for a single share.
 *
 * ### Marktwert-Tab formulas (pure share values, no brokerage/dividend)
 *
 * - volume         = Σ buyVolume − Σ soldVolume (FIFO via SaleBuyDetail)
 * - purchaseValue  = Σ(buyVolume × buyPrice)   [all buys, no brokerage]
 * - curValue       = volume × curPrice
 * - profitLoss     = Σ((curPrice − buyPrice) × remainingVolume) per buy
 * - saleProfitLoss = Σ sale.profitLoss()        [saleValue − buyValue, no brokerage]
 * - marketValue    = curValue + saleProfitLoss
 *
 * ### Depotwert-Tab extras
 *
 * - totalBrokerage   = all brokerage entries (buy + sale)
 * - totalDividend    = dividends after tax
 * - completePurchase = purchaseValue
 * - completeCurValue = curValue + totalDividend + saleProfitLoss
 * - completeProfitLoss = completeCurValue − completePurchase
 */
struct ShareValues
{
    // ── Volume / price ────────────────────────────────────────────────────
    double volume       = 0.0;
    double curPrice     = 0.0;
    double prevDayPrice = 0.0;
    double prevDayDiff  = 0.0;
    double prevDayPct   = 0.0;

    // ── Marktwert-Tab ─────────────────────────────────────────────────────
    double purchaseValue    = 0.0; ///< Σ(buyVol × buyPrice), no brokerage
    double curValue         = 0.0; ///< volume × curPrice
    double profitLoss       = 0.0; ///< Aktuelle Entwicklung €
    double profitLossPct    = 0.0; ///< Aktuelle Entwicklung %
    double saleProfitLoss   = 0.0; ///< Realized gain/loss from sales (no brokerage)
    double marketValue      = 0.0; ///< curValue + saleProfitLoss
    double marketValuePct   = 0.0; ///< (marketValue / purchaseValue − 1) × 100

    // ── Depotwert-Tab extras ──────────────────────────────────────────────
    double totalBrokerage    = 0.0;
    double totalDividend     = 0.0;
    double completePurchase  = 0.0; ///< = purchaseValue
    double completeCurValue  = 0.0; ///< curValue + totalDividend + saleProfitLoss
    double completeProfitLoss = 0.0;
    double completeProfitPct  = 0.0;
};

/**
 * @brief Computes aggregated financial values for a single share.
 */
class ShareCalculator
{
public:
    ShareCalculator() = delete;

    static ShareValues compute(const QString& guid,
                               double curPrice,
                               double prevDayPrice);

    static void portfolioTotalsMarket(const QList<ShareValues>& values,
                                      double& totalPurchase,
                                      double& totalCurValue,
                                      double& totalMarketValue,
                                      double& totalProfitLoss,
                                      double& totalProfitPct,
                                      double& totalMarketValuePct);

    static void portfolioTotalsFinal(const QList<ShareValues>& values,
                                     double& totalBrokDividend,
                                     double& totalPurchase,
                                     double& totalCurValue,
                                     double& totalProfitLoss,
                                     double& totalProfitPct,
                                     double& completePurchase,
                                     double& completeCurValue,
                                     double& completeProfitLoss,
                                     double& completeProfitPct);
};

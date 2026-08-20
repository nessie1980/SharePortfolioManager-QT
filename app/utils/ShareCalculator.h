// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QString>
#include <QList>

/**
 * @brief Aggregated financial values for a single share.
 *
 * The portfolio grid shows two tabs that share the same "current" columns
 * (Einzahlung / Marktwert, Aktuelle Entwicklung) but fill them with two
 * different value sets:
 *
 * - Marktwert-Tab: held-share cost basis WITHOUT brokerage AND WITHOUT
 *   reduction (Rabatt is a discount on brokerage costs, so it is excluded
 *   together with brokerage — corrected 10.07.2026, previously reduction
 *   was still subtracted here even though brokerage itself was excluded).
 * - Depotwert-Tab: held-share cost basis WITH brokerage (with reduction),
 *   plus dividends and realized sale payouts in the complete columns.
 *
 * Both bases are computed over the CURRENTLY HELD shares only (FIFO: each
 * buy contributes its remaining volume), so they stay consistent with
 * curValue, which also counts held shares only.
 *
 * ### Rounding contract (cent-exact, unified)
 *
 * Every monetary amount is rounded to 2 decimals using round-half-away-from-zero
 * (see ShareCalculator::roundAway). This is applied uniformly:
 * - each buy value (volume x price) before adding brokerage / subtracting reduction,
 * - each sale value (volume x salePrice) before fee/tax adjustments,
 * - each proportional brokerage / reduction part of a held position,
 * - the current market value (held volume x curPrice),
 * - and every aggregate that feeds a displayed cell.
 *
 * ### Marktwert-Tab (no brokerage, no reduction)
 *
 * - purchaseValue  = held basis: sum(round(remVol x price))
 * - curValue       = round(heldVolume x curPrice)
 * - profitLoss      = curValue - purchaseValue          [Aktuelle Entwicklung EUR]
 * - profitLossPct   = profitLoss / purchaseValue x 100  [Aktuelle Entwicklung %]
 * - saleProfitLoss  = realized gain/loss (no brokerage, no reduction)
 * - marketValue     = curValue + saleProfitLoss         [footer aggregate only]
 *
 * Marktwert complete columns (brokerage- and reduction-free, internally consistent):
 * - completePurchaseMarket   = all buys: sum(round(vol x price))
 * - completeCurValueMarket   = completePurchaseMarket + completeProfitLossMarket
 * - completeProfitLossMarket = (curValue - purchaseValue) + realized P/L WITH brokerage
 * - completeProfitPctMarket  = completeProfitLossMarket / completePurchaseMarket x 100
 *
 * ### Depotwert-Tab (with brokerage + dividends)
 *
 * - purchaseValueFinal = held basis: sum(round(remVol x price) + brokeragePart - reductionPart)
 * - profitLossFinal     = curValue - purchaseValueFinal          [Aktuelle Entwicklung EUR]
 * - profitLossPctFinal  = profitLossFinal / purchaseValueFinal x 100
 * - totalBrokerage      = all brokerage entries (buy + sale + freestanding)
 * - totalDividend       = dividends after tax
 * - completePurchase    = all buys: sum(round(vol x price) + brokerage - reduction)
 *                         + freestanding brokerage entries (no buyGuid/saleGuid,
 *                         angelegt über die Kosten-Verwaltung; fixed 20.08.2026 —
 *                         see ARCHITECTURE.md "Footer-Lücke bei freistehenden
 *                         Kosteneinträgen"). completePurchaseMarket stays
 *                         brokerage-free by design and is NOT affected.
 * - completeCurValue    = curValue + sum(sale payout incl. brokerage/reduction) + totalDividend
 * - completeProfitLoss  = completeCurValue - completePurchase
 * - completeProfitPct   = completeProfitLoss / completePurchase x 100
 */
struct ShareValues
{
    // -- Volume / price ----------------------------------------------------
    double volume       = 0.0;
    double curPrice     = 0.0;
    double prevDayPrice = 0.0;
    double prevDayDiff  = 0.0;
    double prevDayPct   = 0.0;

    // -- Marktwert-Tab (no brokerage) --------------------------------------
    double purchaseValue    = 0.0; ///< Held basis without brokerage, without reduction
    double curValue         = 0.0; ///< round(heldVolume x curPrice)
    double profitLoss       = 0.0; ///< Aktuelle Entwicklung EUR (market)
    double profitLossPct    = 0.0; ///< Aktuelle Entwicklung % (market)
    double saleProfitLoss   = 0.0; ///< Realized gain/loss from sales (no brokerage)
    double marketValue      = 0.0; ///< curValue + saleProfitLoss
    double marketValuePct   = 0.0; ///< (marketValue / purchaseValue - 1) x 100

    /// Raw sale proceeds WITHOUT brokerage AND WITHOUT reduction (ShareDetailsForm
    /// Marktwert-Box "+ Verkäufe" row) — Marktwert-Pendant to salePayoutFinal.
    double salePayoutMarket = 0.0;

    // -- Marktwert-Tab complete columns (brokerage- and reduction-free) ----
    double completePurchaseMarket   = 0.0; ///< All buys, no brokerage, no reduction
    double completeCurValueMarket   = 0.0; ///< completePurchaseMarket + completeProfitLossMarket
    double completeProfitLossMarket = 0.0;
    double completeProfitPctMarket  = 0.0;

    // -- Depotwert-Tab (with brokerage) ------------------------------------
    double purchaseValueFinal = 0.0; ///< Held basis WITH brokerage (with reduction)
    double profitLossFinal    = 0.0; ///< Aktuelle Entwicklung EUR (final)
    double profitLossPctFinal = 0.0; ///< Aktuelle Entwicklung % (final)

    double totalBrokerage     = 0.0;
    double totalDividend      = 0.0;
    double completePurchase   = 0.0; ///< All buys, with brokerage - reduction
    double completeCurValue   = 0.0; ///< curValue + sale payouts + dividends
    double completeProfitLoss = 0.0;
    double completeProfitPct  = 0.0;

    /// Raw sale proceeds WITH brokerage (ShareDetailsForm "+ Verkäufe" row) —
    /// same underlying aggregate that feeds completeCurValue, exposed as its
    /// own field so callers don't have to re-derive it. Depotwert-Pendant to
    /// nothing in the Marktwert section (Marktwert has no such raw-proceeds row).
    double salePayoutFinal = 0.0;

    /// Realized gain/loss from sales WITH brokerage — Depotwert-Pendant to
    /// saleProfitLoss (which is the brokerage-free Marktwert variant).
    double saleProfitLossFinal = 0.0;
};

/**
 * @brief Computes aggregated financial values for a single share.
 *
 * Stateless: every call reads buys, sales, brokerage, dividends and splits
 * fresh from the repositories and returns a fully populated ShareValues. No
 * running balances, no sentinel guards. The proportional brokerage /
 * reduction of a partly-sold buy is reconstructed here from the buy's linked
 * brokerage record, independent of any values stored on SaleBuyDetail.
 *
 * @note **Split-Umrechnung (Phase 2 der Aktiensplit-Behandlung, 07.08.2026,
 * siehe ARCHITECTURE.md "Offene Punkte"):** `buys`/`sales` liegen in der
 * Beleg-Skala des jeweiligen Transaktionsdatums vor. Vor jeder Berechnung
 * werden Stückzahl und Preis je Transaktion über `ShareSplitAdjuster` auf
 * die heutige, nach allen bekannten Splits gültige Skala umgerechnet —
 * `curPrice`/`prevDayPrice` sind bereits heutige Skala (Live-Kurs). Ohne
 * gespeicherte Splits liefert `ShareSplitAdjuster` überall den Faktor 1.0,
 * das Verhalten ist dann bitgenau identisch zum Stand vor dieser Änderung.
 * Brokerage, Rabatt und Steuern sind reine Geldbeträge und werden nicht
 * skaliert.
 */
class ShareCalculator
{
public:
    ShareCalculator() = delete;

    /**
     * @brief Round to @p digits decimals, half-away-from-zero (matches the
     *        C# reference's MidpointRounding.AwayFromZero and decimal display).
     *
     * A tiny magnitude-direction epsilon is added before rounding so that a
     * value that should sit exactly on a half-cent boundary but is represented
     * slightly low in binary (e.g. 2.4999999998 for 2.50) still rounds away
     * from zero, giving the same cent as the C# decimal computation.
     */
    static double roundAway(double value, int digits = 2);

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

    /**
     * @brief Complete (Kpl.) column totals for the Marktwert footer.
     *
     * Separate additive function so existing call sites keep compiling while
     * the Marktwert tab gains its complete columns.
     */
    static void portfolioCompleteTotalsMarket(const QList<ShareValues>& values,
                                              double& completePurchase,
                                              double& completeCurValue,
                                              double& completeProfitLoss,
                                              double& completeProfitPct);

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

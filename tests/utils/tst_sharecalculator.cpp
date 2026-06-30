// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// Unit tests for ShareCalculator::compute() and roundAway().
//
// compute() reads buys, sales, brokerage and dividends fresh from the
// repositories, so these tests populate a real in-memory SQLite database
// (same pattern as the repository tests) and assert the resulting
// ShareValues against reference values verified against the C# application.
//
// Focus is the Marktwert tab and the fixes made while porting it:
//   - realized P/L in the "Komplette Entwicklung" includes brokerage,
//   - that realized P/L is derived from aggregates (all buys minus held
//     buys), NOT from SaleBuyDetail records — so an empty / unpopulated
//     detail list must NOT overstate the gain,
//   - the column identity  Kpl. Marktwert = Kpl. Einzahlung + Kpl. Entwicklung,
//   - cent-exact half-away-from-zero rounding,
//   - edge cases volume = 0 (fully sold) and no sales.

#include <QtTest>
#include <QSqlDatabase>
#include <QUuid>

#include "../../app/core/Database.h"
#include "../../app/models/BuyObject.h"
#include "../../app/models/SaleObject.h"
#include "../../app/models/BrokerageObject.h"
#include "../../app/repositories/BuyRepository.h"
#include "../../app/repositories/SaleRepository.h"
#include "../../app/repositories/BrokerageRepository.h"
#include "../../app/utils/ShareCalculator.h"

// Compare two monetary doubles to within a tiny tolerance. Avoids the
// qFuzzyCompare-with-zero pitfall and gives a readable failure message.
#define CMP_MONEY(actual, expected)                                            \
    QVERIFY2(qAbs((actual) - (expected)) < 1e-6,                               \
             qPrintable(QStringLiteral("got %1, expected %2")                  \
                            .arg(actual, 0, 'f', 4).arg(expected, 0, 'f', 4)))

class TestShareCalculator : public QObject
{
    Q_OBJECT

private:
    const QString k_shareGuid = QStringLiteral("test-share-calc-0001");

    QString newGuid() const { return QUuid::createUuid().toString(QUuid::WithoutBraces); }

    // Insert a buy and (if non-zero) its linked brokerage record.
    // Returns the buy's guid so it can be referenced by SaleBuyDetails.
    QString addBuy(double volume, double volumeSold, double price,
                   double brokerage, double reduction)
    {
        BuyRepository       buyRepo;
        BrokerageRepository brokRepo;

        const QString buyGuid = newGuid();
        buyRepo.insert(BuyObject(buyGuid, k_shareGuid, QString(), newGuid(),
                                 QStringLiteral("2024-01-01T10:00:00"),
                                 volume, volumeSold, price));

        if (brokerage != 0.0 || reduction != 0.0) {
            // brokerage() == provision + brokerFee + traderFee
            brokRepo.insert(BrokerageObject(newGuid(), k_shareGuid, buyGuid, QString(),
                                            QStringLiteral("2024-01-01T10:00:00"),
                                            brokerage, 0.0, 0.0, reduction));
        }
        return buyGuid;
    }

    // Insert a sale with its linked brokerage record (the sale's brokerage and
    // reduction are loaded via JOIN on brokerage_guid) and optional details.
    void addSale(double volume, double salePrice, double brokerage,
                 double reduction, double tax, const QList<SaleBuyDetail>& details)
    {
        SaleRepository      saleRepo;
        BrokerageRepository brokRepo;

        const QString saleGuid = newGuid();
        const QString brokGuid = newGuid();

        // sale_guid stays empty: the sale references this record via
        // brokerage_guid, which is what findByShare JOINs on.
        brokRepo.insert(BrokerageObject(brokGuid, k_shareGuid, QString(), QString(),
                                        QStringLiteral("2024-06-01T10:00:00"),
                                        brokerage, 0.0, 0.0, reduction));

        saleRepo.insert(SaleObject(saleGuid, k_shareGuid, QString(), newGuid(),
                                   QStringLiteral("2024-06-01T10:00:00"),
                                   volume, salePrice, details,
                                   tax, 0.0, 0.0,        // taxAtSource / capitalGains / solidarity
                                   brokGuid,             // brokerageGuid -> JOIN source
                                   0.0, 0.0, 0.0,        // provision/fees (ignored on insert)
                                   0.0));                // reduction (ignored on insert; from brokerage)
    }

    // Insert a dividend. payout = rate * volume (no FX), net = payout - tax.
    // Other tax columns and FX use their schema defaults (0 / 1). Enough to
    // exercise the net-dividend contribution to the Depotwert complete columns.
    void addDividend(double rate, double volume, double tax)
    {
        Database::instance().execute(
            QStringLiteral("INSERT INTO dividends "
                           "(guid, share_guid, datetime, rate, volume, tax_at_source) "
                           "VALUES ('%1', '%2', '2024-03-01T10:00:00', %3, %4, %5)")
                .arg(newGuid())
                .arg(k_shareGuid)
                .arg(rate,   0, 'f', 6)
                .arg(volume, 0, 'f', 6)
                .arg(tax,    0, 'f', 6));
    }

private slots:

    void initTestCase()
    {
        QVERIFY(Database::instance().open(QStringLiteral(":memory:")));
        Database::instance().execute(
            QStringLiteral("INSERT INTO shares (guid, wkn, name) "
                           "VALUES ('%1', 'TEST01', 'Test Share')")
                .arg(k_shareGuid));
    }

    void cleanupTestCase()
    {
        Database::instance().close();
    }

    void init()
    {
        // Delete in FK-safe order: child / referencing tables first.
        // brokerage.buy_guid -> buys.guid and sales.brokerage_guid -> brokerage.guid,
        // so buys must be cleared AFTER brokerage.
        Database::instance().execute(QStringLiteral("DELETE FROM sale_buy_details"));
        Database::instance().execute(QStringLiteral("DELETE FROM sales"));
        Database::instance().execute(QStringLiteral("DELETE FROM brokerage"));
        Database::instance().execute(QStringLiteral("DELETE FROM buys"));
        Database::instance().execute(QStringLiteral("DELETE FROM dividends"));
    }

    // ── roundAway ─────────────────────────────────────────────────────────
    void test_roundAway_halfAwayFromZero()
    {
        CMP_MONEY(ShareCalculator::roundAway(2.345), 2.35);
        CMP_MONEY(ShareCalculator::roundAway(2.344), 2.34);
        CMP_MONEY(ShareCalculator::roundAway(-2.345), -2.35);
        CMP_MONEY(ShareCalculator::roundAway(2.5, 0), 3.0);
        CMP_MONEY(ShareCalculator::roundAway(-2.5, 0), -3.0);
    }

    // ── Core scenario (one buy partly sold, a second held, one sale) ──────
    // Buy1: 10 @ 100, 4 sold, brokerage 9.90
    // Buy2:  5 @ 120, held,   brokerage 5.00, reduction 2.00
    // Sale:  4 @ 130, brokerage 7.00, tax 3.00, details reference Buy1
    // curPrice 125, prevDay 120
    void test_marktwert_coreScenario()
    {
        const QString b1 = addBuy(10.0, 4.0, 100.0, 9.90, 0.0);
        addBuy(5.0, 0.0, 120.0, 5.00, 2.00);
        addSale(4.0, 130.0, 7.0, 0.0, 3.0,
                { SaleBuyDetail(b1, QStringLiteral("2024-06-01T10:00:00"),
                                4.0, 100.0, 0.0, 0.0) });

        const ShareValues v = ShareCalculator::compute(k_shareGuid, 125.0, 120.0);

        CMP_MONEY(v.volume,                   11.0);
        CMP_MONEY(v.curValue,                 1375.0);
        CMP_MONEY(v.purchaseValue,            1198.0);  // held basis, no brokerage
        CMP_MONEY(v.profitLoss,               177.0);   // Aktuelle Entwicklung
        CMP_MONEY(v.completePurchaseMarket,   1598.0);  // Kpl. Einzahlung
        CMP_MONEY(v.completeProfitLossMarket, 283.04);  // realized P/L incl. fees
        CMP_MONEY(v.completeCurValueMarket,   1881.04); // Kpl. Marktwert

        // Depotwert basics (with brokerage)
        CMP_MONEY(v.purchaseValueFinal,       1208.94);
        CMP_MONEY(v.completePurchase,         1612.90);
        CMP_MONEY(v.completeProfitLoss,       272.10);
    }

    // ── Depotwert tab: …Final fields (with brokerage), per-lot attribution ──
    // Same core fixture. Verifies the exact values the Depotwert tab displays —
    // "Aktuelle Entwicklung" (profitLossFinal / profitLossPctFinal) and the
    // Depotwert complete columns. Buy-brokerage is attributed per held lot:
    //   Buy1 (6/10 held): 600 + round(9.90*0.6)=5.94             = 605.94
    //   Buy2 (5/5 held):  600 + 5.00 - 2.00                      = 603.00
    //   purchaseValueFinal = 1208.94 ; curValue = 1375
    //   profitLossFinal    = 1375 - 1208.94                      = 166.06
    //   completeCurValue   = curValue + salePayoutFinal(510) + 0 = 1885.00
    void test_depotwert_finalFields()
    {
        const QString b1 = addBuy(10.0, 4.0, 100.0, 9.90, 0.0);
        addBuy(5.0, 0.0, 120.0, 5.00, 2.00);
        addSale(4.0, 130.0, 7.0, 0.0, 3.0,
                { SaleBuyDetail(b1, QStringLiteral("2024-06-01T10:00:00"),
                                4.0, 100.0, 0.0, 0.0) });

        const ShareValues v = ShareCalculator::compute(k_shareGuid, 125.0, 120.0);

        // Aktuelle Entwicklung (Depotwert, WITH brokerage)
        CMP_MONEY(v.purchaseValueFinal, 1208.94);
        CMP_MONEY(v.profitLossFinal,    166.06);
        CMP_MONEY(v.profitLossPctFinal, 166.06 / 1208.94 * 100.0);

        // Depotwert complete columns
        CMP_MONEY(v.completePurchase,   1612.90);
        CMP_MONEY(v.completeCurValue,   1885.00);
        CMP_MONEY(v.completeProfitLoss, 272.10);
        CMP_MONEY(v.completeProfitPct,  272.10 / 1612.90 * 100.0);
    }

    // ── Depotwert: brokerage AND reduction prorated per held lot ───────────
    // One buy partly sold (6 of 10 held) carrying both brokerage and a
    // reduction. Both are prorated to the held fraction (0.6) and rounded per
    // lot:  heldBrokerage = round(9.94*0.6) = 5.96,
    //       heldReduction = round(5.51*0.6) = 3.31.
    //   purchaseValueFinal = 600 + 5.96 - 3.31 = 602.65
    // The full-attribution model (600 + 9.94 - 5.51 = 604.43) is explicitly
    // wrong; this pins the per-lot behaviour for both terms.
    void test_depotwert_partialLotBrokerageAndReduction()
    {
        addBuy(10.0, 4.0, 100.0, 9.94, 5.51);
        addSale(4.0, 110.0, 0.0, 0.0, 0.0, {});

        const ShareValues v = ShareCalculator::compute(k_shareGuid, 100.0, 100.0);

        CMP_MONEY(v.purchaseValueFinal, 602.65);  // per-lot, NOT 604.43
        CMP_MONEY(v.purchaseValue,      596.69);  // market basis (reduction only)
        CMP_MONEY(v.profitLossFinal,    -2.65);
        CMP_MONEY(v.profitLossPctFinal, -2.65 / 602.65 * 100.0);
    }

    // ── Depotwert: net dividends contribute to the complete columns ────────
    // One held buy, no sale, a single dividend (2.00 * 6 = 12.00, no tax).
    //   completeCurValue = curValue(1000) + salePayout(0) + dividend(12) = 1012
    // Without the dividend term this would be 1000.00.
    void test_depotwert_dividendInCompleteValue()
    {
        addBuy(10.0, 0.0, 100.0, 0.0, 0.0);
        addDividend(2.0, 6.0, 0.0);

        const ShareValues v = ShareCalculator::compute(k_shareGuid, 100.0, 100.0);

        CMP_MONEY(v.completeCurValue,   1012.00); // 1000.00 without the dividend
        CMP_MONEY(v.completePurchase,   1000.00);
        CMP_MONEY(v.completeProfitLoss, 12.00);
    }

    // ── Regression: empty SaleBuyDetails must NOT overstate the gain ──────
    // Same inputs as the core scenario, but the sale has NO detail records.
    // Because compute() derives the sold cost basis from aggregates (all buys
    // minus still-held buys) the result must be identical — not the full sale
    // proceeds (which was the "viel zu hoch" bug).
    void test_marktwert_emptyDetails_sameResult()
    {
        addBuy(10.0, 4.0, 100.0, 9.90, 0.0);
        addBuy(5.0, 0.0, 120.0, 5.00, 2.00);
        addSale(4.0, 130.0, 7.0, 0.0, 3.0, {}); // no details

        const ShareValues v = ShareCalculator::compute(k_shareGuid, 125.0, 120.0);

        CMP_MONEY(v.completeProfitLossMarket, 283.04);
        CMP_MONEY(v.completeCurValueMarket,   1881.04);
    }

    // ── Column identity: Kpl. Marktwert == Kpl. Einzahlung + Kpl. Entwicklung
    void test_marktwert_columnIdentity()
    {
        addBuy(10.0, 4.0, 100.0, 9.90, 0.0);
        addBuy(5.0, 0.0, 120.0, 5.00, 2.00);
        addSale(4.0, 130.0, 7.0, 0.0, 3.0, {});

        const ShareValues v = ShareCalculator::compute(k_shareGuid, 125.0, 120.0);

        CMP_MONEY(v.completeCurValueMarket,
                  v.completePurchaseMarket + v.completeProfitLossMarket);
    }

    // ── Edge case: position fully sold (held volume 0) ───────────────────
    void test_marktwert_fullySold()
    {
        addBuy(10.0, 10.0, 100.0, 10.0, 0.0);
        addSale(10.0, 130.0, 8.0, 0.0, 0.0, {});

        const ShareValues v = ShareCalculator::compute(k_shareGuid, 125.0, 120.0);

        CMP_MONEY(v.volume,                   0.0);
        CMP_MONEY(v.curValue,                 0.0);
        CMP_MONEY(v.purchaseValue,            0.0);
        CMP_MONEY(v.profitLossPct,            0.0);   // guarded division
        CMP_MONEY(v.completeProfitLossMarket, 282.0); // realized only, incl. fees
        CMP_MONEY(v.completeCurValueMarket,   1282.0);
    }

    // ── Edge case: no sales — complete == current, identity trivially holds
    void test_marktwert_noSales()
    {
        addBuy(10.0, 0.0, 100.0, 10.0, 0.0);

        const ShareValues v = ShareCalculator::compute(k_shareGuid, 125.0, 120.0);

        CMP_MONEY(v.purchaseValue,            1000.0);
        CMP_MONEY(v.curValue,                 1250.0);
        CMP_MONEY(v.completeProfitLossMarket, 250.0);
        CMP_MONEY(v.completeCurValueMarket,   1250.0);
        CMP_MONEY(v.completeCurValueMarket,   v.curValue); // nothing realized
    }

    // ── Previous-day figures ─────────────────────────────────────────────
    void test_prevDay_diffAndPct()
    {
        addBuy(10.0, 0.0, 100.0, 0.0, 0.0);

        const ShareValues v = ShareCalculator::compute(k_shareGuid, 125.0, 120.0);

        CMP_MONEY(v.prevDayDiff, 5.0);
        QVERIFY(qAbs(v.prevDayPct - (5.0 / 120.0 * 100.0)) < 1e-6);
    }
};

QTEST_MAIN(TestShareCalculator)
#include "tst_sharecalculator.moc"

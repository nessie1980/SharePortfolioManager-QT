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

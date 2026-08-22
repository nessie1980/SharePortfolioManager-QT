// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// tst_dividendvolumechecker.cpp — Unit tests für DividendVolumeChecker
// (Phase 3 der Ex-Tag-Behandlung bei Dividenden, 21.08.2026).
//
// Zustandslos und datenbankfrei, gleicher Aufbau wie tst_salefifoallocator.cpp:
// Käufe, Verkäufe und Splits werden direkt als Objekte gebaut.

#include <QtTest>

#include "../../app/models/BuyObject.h"
#include "../../app/models/SaleObject.h"
#include "../../app/models/ShareSplitObject.h"
#include "../../app/utils/DividendVolumeChecker.h"

namespace {

const QString kShare = QStringLiteral("share-1");
const QString kDepot = QStringLiteral("DE111");
const QString kOther = QStringLiteral("DE222");

BuyObject makeBuy(const QString& guid, const QString& depot,
                  const QString& isoDate, double volume)
{
    return BuyObject(guid, kShare, depot, QStringLiteral("order-") + guid,
                     isoDate + QStringLiteral("T00:00:00"),
                     volume, /*volumeSold=*/0.0, /*price=*/10.0);
}

SaleObject makeSale(const QString& guid, const QString& depot,
                    const QString& isoDate, double volume)
{
    return SaleObject(guid, kShare, depot, QStringLiteral("order-") + guid,
                      isoDate + QStringLiteral("T00:00:00"),
                      volume, /*salePrice=*/12.0, QList<SaleBuyDetail>{});
}

ShareSplitObject makeSplit(const QString& isoDate, double ratioNew, double ratioOld)
{
    return ShareSplitObject(QStringLiteral("split-") + isoDate, kShare,
                            QDate::fromString(isoDate, Qt::ISODate),
                            ratioNew, ratioOld);
}

} // namespace

class TestDividendVolumeChecker : public QObject
{
    Q_OBJECT

private slots:

    // ── holdingsAtExDate: Grundfälle ohne Splits ──────────────────────────

    void test_holdings_singleBuyBeforeExDate_counts()
    {
        const QList<BuyObject> buys = { makeBuy("b1", kDepot, "2024-01-10", 100.0) };
        const double h = DividendVolumeChecker::holdingsAtExDate(
            buys, {}, {}, kDepot, QDate(2024, 5, 15));
        QCOMPARE(h, 100.0);
    }

    void test_holdings_buyAfterExDate_ignored()
    {
        const QList<BuyObject> buys = {
            makeBuy("b1", kDepot, "2024-01-10", 100.0),
            makeBuy("b2", kDepot, "2024-06-01", 50.0),   // nach dem Ex-Tag
        };
        const double h = DividendVolumeChecker::holdingsAtExDate(
            buys, {}, {}, kDepot, QDate(2024, 5, 15));
        QCOMPARE(h, 100.0);
    }

    void test_holdings_buyOnExDate_ignored()
    {
        // Stichtagsregel: wer AM Ex-Tag kauft, ist nicht mehr
        // dividendenberechtigt — der Kauf zählt also nicht mit.
        const QList<BuyObject> buys = {
            makeBuy("b1", kDepot, "2024-01-10", 100.0),
            makeBuy("b2", kDepot, "2024-05-15", 50.0),   // genau am Ex-Tag
        };
        const double h = DividendVolumeChecker::holdingsAtExDate(
            buys, {}, {}, kDepot, QDate(2024, 5, 15));
        QCOMPARE(h, 100.0);
    }

    void test_holdings_saleBeforeExDate_subtracted()
    {
        const QList<BuyObject>  buys  = { makeBuy("b1", kDepot, "2024-01-10", 100.0) };
        const QList<SaleObject> sales = { makeSale("s1", kDepot, "2024-03-01", 30.0) };
        const double h = DividendVolumeChecker::holdingsAtExDate(
            buys, sales, {}, kDepot, QDate(2024, 5, 15));
        QCOMPARE(h, 70.0);
    }

    void test_holdings_saleOnExDate_notSubtracted()
    {
        // Wer AM Ex-Tag verkauft, erhält die Dividende noch.
        const QList<BuyObject>  buys  = { makeBuy("b1", kDepot, "2024-01-10", 100.0) };
        const QList<SaleObject> sales = { makeSale("s1", kDepot, "2024-05-15", 30.0) };
        const double h = DividendVolumeChecker::holdingsAtExDate(
            buys, sales, {}, kDepot, QDate(2024, 5, 15));
        QCOMPARE(h, 100.0);
    }

    // ── Depot-Filterung ───────────────────────────────────────────────────

    void test_holdings_otherDepotBuy_ignored()
    {
        const QList<BuyObject> buys = {
            makeBuy("b1", kDepot, "2024-01-10", 100.0),
            makeBuy("b2", kOther, "2024-01-10", 900.0),   // anderes Depot
        };
        const double h = DividendVolumeChecker::holdingsAtExDate(
            buys, {}, {}, kDepot, QDate(2024, 5, 15));
        QCOMPARE(h, 100.0);
    }

    void test_holdings_otherDepotSale_ignored()
    {
        const QList<BuyObject>  buys  = { makeBuy("b1", kDepot, "2024-01-10", 100.0) };
        const QList<SaleObject> sales = { makeSale("s1", kOther, "2024-03-01", 30.0) };
        const double h = DividendVolumeChecker::holdingsAtExDate(
            buys, sales, {}, kDepot, QDate(2024, 5, 15));
        QCOMPARE(h, 100.0);
    }

    void test_holdings_depotComparisonIsTrimmed()
    {
        const QList<BuyObject> buys = {
            makeBuy("b1", QStringLiteral("  DE111  "), "2024-01-10", 100.0),
        };
        const double h = DividendVolumeChecker::holdingsAtExDate(
            buys, {}, {}, QStringLiteral(" DE111 "), QDate(2024, 5, 15));
        QCOMPARE(h, 100.0);
    }

    // ── Splits ────────────────────────────────────────────────────────────

    void test_holdings_splitBetweenBuyAndExDate_scalesToExDate()
    {
        // Kauf 100 Stk. (Beleg-Skala 2024-01), danach Split 2:1 →
        // am Ex-Tag hält der Anleger 200 Stk., und genau 200 steht auf der
        // Dividendenabrechnung. Ein naives Aufsummieren ergäbe 100.
        const QList<BuyObject>        buys   = { makeBuy("b1", kDepot, "2024-01-10", 100.0) };
        const QList<ShareSplitObject> splits = { makeSplit("2024-03-01", 2.0, 1.0) };
        const double h = DividendVolumeChecker::holdingsAtExDate(
            buys, {}, splits, kDepot, QDate(2024, 5, 15));
        QCOMPARE(h, 200.0);
    }

    void test_holdings_splitAfterExDate_doesNotAffectResult()
    {
        // Der Split liegt NACH dem Ex-Tag — die Abrechnung von damals nennt
        // weiterhin die ungesplittete Stückzahl.
        const QList<BuyObject>        buys   = { makeBuy("b1", kDepot, "2024-01-10", 100.0) };
        const QList<ShareSplitObject> splits = { makeSplit("2024-08-01", 2.0, 1.0) };
        const double h = DividendVolumeChecker::holdingsAtExDate(
            buys, {}, splits, kDepot, QDate(2024, 5, 15));
        QCOMPARE(h, 100.0);
    }

    void test_holdings_reverseSplitBetweenBuyAndExDate()
    {
        // Reverse-Split 1:10 → aus 1000 Stk. werden 100.
        const QList<BuyObject>        buys   = { makeBuy("b1", kDepot, "2024-01-10", 1000.0) };
        const QList<ShareSplitObject> splits = { makeSplit("2024-03-01", 1.0, 10.0) };
        const double h = DividendVolumeChecker::holdingsAtExDate(
            buys, {}, splits, kDepot, QDate(2024, 5, 15));
        QCOMPARE(h, 100.0);
    }

    void test_holdings_buyBeforeAndAfterSplit_mixedScales()
    {
        // Kauf 100 vor dem Split (→ 200 nach Split), Kauf 50 nach dem Split
        // (bleibt 50) → 250 am Ex-Tag. Genau der Fall, den ein direktes
        // Aufsummieren (150) falsch beantworten würde.
        const QList<BuyObject> buys = {
            makeBuy("b1", kDepot, "2024-01-10", 100.0),
            makeBuy("b2", kDepot, "2024-04-01", 50.0),
        };
        const QList<ShareSplitObject> splits = { makeSplit("2024-03-01", 2.0, 1.0) };
        const double h = DividendVolumeChecker::holdingsAtExDate(
            buys, {}, splits, kDepot, QDate(2024, 5, 15));
        QCOMPARE(h, 250.0);
    }

    void test_holdings_saleBeforeSplit_scaledToo()
    {
        // Kauf 100 (→200), Verkauf 20 vor dem Split (→40) → 160.
        const QList<BuyObject>  buys  = { makeBuy("b1", kDepot, "2024-01-10", 100.0) };
        const QList<SaleObject> sales = { makeSale("s1", kDepot, "2024-02-01", 20.0) };
        const QList<ShareSplitObject> splits = { makeSplit("2024-03-01", 2.0, 1.0) };
        const double h = DividendVolumeChecker::holdingsAtExDate(
            buys, sales, splits, kDepot, QDate(2024, 5, 15));
        QCOMPARE(h, 160.0);
    }

    void test_holdings_counters_reportConsideredRows()
    {
        const QList<BuyObject> buys = {
            makeBuy("b1", kDepot, "2024-01-10", 100.0),
            makeBuy("b2", kDepot, "2024-06-01", 50.0),    // nach Ex-Tag
            makeBuy("b3", kOther, "2024-01-10", 10.0),    // anderes Depot
        };
        const QList<SaleObject> sales = { makeSale("s1", kDepot, "2024-03-01", 30.0) };

        int nBuys = -1, nSales = -1;
        DividendVolumeChecker::holdingsAtExDate(
            buys, sales, {}, kDepot, QDate(2024, 5, 15), &nBuys, &nSales);
        QCOMPARE(nBuys,  1);
        QCOMPARE(nSales, 1);
    }

    void test_holdings_invalidExDate_returnsZero()
    {
        const QList<BuyObject> buys = { makeBuy("b1", kDepot, "2024-01-10", 100.0) };
        const double h = DividendVolumeChecker::holdingsAtExDate(
            buys, {}, {}, kDepot, QDate());
        QCOMPARE(h, 0.0);
    }

    // ── check(): nicht prüfbare Fälle ─────────────────────────────────────

    void test_check_noExDate_notCheckable()
    {
        const QList<BuyObject> buys = { makeBuy("b1", kDepot, "2024-01-10", 100.0) };
        const auto r = DividendVolumeChecker::check(
            100.0, QDate(), kDepot, buys, {}, {});
        QVERIFY(!r.checkable);
    }

    void test_check_noDepotNumber_notCheckable()
    {
        const QList<BuyObject> buys = { makeBuy("b1", kDepot, "2024-01-10", 100.0) };
        const auto r = DividendVolumeChecker::check(
            100.0, QDate(2024, 5, 15), QString(), buys, {}, {});
        QVERIFY(!r.checkable);
    }

    void test_check_noBuysAtAll_notCheckable()
    {
        // Aktie ohne erfasste Kaufhistorie: die Prüfung wird übersprungen,
        // statt jede Dividende zu blockieren.
        const auto r = DividendVolumeChecker::check(
            100.0, QDate(2024, 5, 15), kDepot, {}, {}, {});
        QVERIFY(!r.checkable);
    }

    // ── check(): Treffer und Abweichung ───────────────────────────────────

    void test_check_matchingVolume_matches()
    {
        const QList<BuyObject> buys = { makeBuy("b1", kDepot, "2024-01-10", 100.0) };
        const auto r = DividendVolumeChecker::check(
            100.0, QDate(2024, 5, 15), kDepot, buys, {}, {});
        QVERIFY(r.checkable);
        QVERIFY(r.matches);
        QCOMPARE(r.expectedVolume, 100.0);
        QCOMPARE(r.enteredVolume,  100.0);
        QCOMPARE(r.deviation(),    0.0);
    }

    void test_check_tooManyShares_doesNotMatch()
    {
        const QList<BuyObject> buys = { makeBuy("b1", kDepot, "2024-01-10", 100.0) };
        const auto r = DividendVolumeChecker::check(
            150.0, QDate(2024, 5, 15), kDepot, buys, {}, {});
        QVERIFY(r.checkable);
        QVERIFY(!r.matches);
        QCOMPARE(r.deviation(), 50.0);
    }

    void test_check_tooFewShares_doesNotMatch()
    {
        const QList<BuyObject> buys = { makeBuy("b1", kDepot, "2024-01-10", 100.0) };
        const auto r = DividendVolumeChecker::check(
            80.0, QDate(2024, 5, 15), kDepot, buys, {}, {});
        QVERIFY(!r.matches);
        QCOMPARE(r.deviation(), -20.0);
    }

    void test_check_wrongDepotSelected_doesNotMatch()
    {
        // Käufe liegen im anderen Depot → Bestand 0, eingetragen 100.
        // Genau das Signal, für das die Prüfung da ist.
        const QList<BuyObject> buys = { makeBuy("b1", kOther, "2024-01-10", 100.0) };
        const auto r = DividendVolumeChecker::check(
            100.0, QDate(2024, 5, 15), kDepot, buys, {}, {});
        QVERIFY(r.checkable);
        QVERIFY(!r.matches);
        QCOMPARE(r.expectedVolume, 0.0);
    }

    void test_check_withinTolerance_matches()
    {
        // Reverse-Split 1:3 auf 100 Stk. → 33,33333…; der Benutzer trägt die
        // vom Beleg abgelesenen 33,3333 ein. Muss als Treffer gelten.
        const QList<BuyObject>        buys   = { makeBuy("b1", kDepot, "2024-01-10", 100.0) };
        const QList<ShareSplitObject> splits = { makeSplit("2024-03-01", 1.0, 3.0) };
        const auto r = DividendVolumeChecker::check(
            33.3333, QDate(2024, 5, 15), kDepot, buys, {}, splits);
        QVERIFY(r.checkable);
        QVERIFY(r.matches);
    }

    void test_check_justOutsideTolerance_doesNotMatch()
    {
        const QList<BuyObject> buys = { makeBuy("b1", kDepot, "2024-01-10", 100.0) };
        const auto r = DividendVolumeChecker::check(
            100.0 + 10 * DividendVolumeChecker::kVolumeTolerance,
            QDate(2024, 5, 15), kDepot, buys, {}, {});
        QVERIFY(!r.matches);
    }

    void test_check_splitBetweenBuyAndExDate_matchesSplitAdjustedVolume()
    {
        // Der eigentliche Praxisfall: 100 gekauft, Split 2:1, Abrechnung
        // nennt 200. Ohne Skalenumrechnung würde hier fälschlich blockiert.
        const QList<BuyObject>        buys   = { makeBuy("b1", kDepot, "2024-01-10", 100.0) };
        const QList<ShareSplitObject> splits = { makeSplit("2024-03-01", 2.0, 1.0) };
        const auto r = DividendVolumeChecker::check(
            200.0, QDate(2024, 5, 15), kDepot, buys, {}, splits);
        QVERIFY(r.checkable);
        QVERIFY(r.matches);
    }

    void test_check_fullySoldBeforeExDate_expectsZero()
    {
        const QList<BuyObject>  buys  = { makeBuy("b1", kDepot, "2024-01-10", 100.0) };
        const QList<SaleObject> sales = { makeSale("s1", kDepot, "2024-02-01", 100.0) };
        const auto r = DividendVolumeChecker::check(
            100.0, QDate(2024, 5, 15), kDepot, buys, sales, {});
        QVERIFY(r.checkable);
        QVERIFY(!r.matches);
        QCOMPARE(r.expectedVolume, 0.0);
    }
};

QTEST_MAIN(TestDividendVolumeChecker)
#include "tst_dividendvolumechecker.moc"

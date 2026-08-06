// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// Unit tests for PortfolioSeriesCalculator — the calculation core behind the
// Depotwert chart (Feature 05.08.2026).
//
// The calculator is deliberately database-free: all input arrives as plain
// structs, so these tests need neither SQLite nor widgets. That is the whole
// point of splitting it out of the Model.
//
// The central test is test_referenceScenario_twoSharesWithPartialSale — the
// two-share example that was worked through with Nessie step by step and whose
// expected values (-10 / +82 / +114 / +160 / +259) he confirmed. If that test
// ever fails, the formula changed, not the test.

#include <QtTest>

#include "../../app/utils/PortfolioSeriesCalculator.h"

// Cent-exact comparison, same tolerance idea as tst_sharecalculator.
#define CMP_MONEY(actual, expected) \
    QVERIFY2(qAbs((actual) - (expected)) < 0.005, \
             qPrintable(QStringLiteral("expected %1, got %2") \
                        .arg(expected, 0, 'f', 2).arg(actual, 0, 'f', 2)))

class TestPortfolioSeriesCalculator : public QObject
{
    Q_OBJECT

private:
    static QDate d(int year, int month, int day) { return QDate(year, month, day); }

    // ── Reference fixture ────────────────────────────────────────────────
    //
    // Share A: 10.01. buy 20 x 50 EUR = 1000 EUR, fee 12, reduction 2 -> cost 10
    //          10.03. dividend gross 30 EUR, taxes 8 EUR      -> net 22
    //          10.04. sale 8 x 60 EUR = 480 EUR, fee 4, tax 15
    // Share B: 01.02. buy 5 x 200 EUR = 1000 EUR, fee 8
    //
    // Prices A: 50 / 55 / 58 / 60 / 62   Prices B: - / 200 / 190 / 195 / 210
    static QList<PortfolioShareSeriesInput> referenceFixture()
    {
        PortfolioShareSeriesInput a;
        a.shareGuid = QStringLiteral("guid-a");
        a.name      = QStringLiteral("Aktie A");
        a.buys      = { { d(2026, 1, 10), 20.0, 50.0 } };
        a.sales     = { { d(2026, 4, 10),  8.0, 60.0, 15.0 } };
        a.dividends = { { d(2026, 3, 10), 22.0 } };
        a.costs     = { { d(2026, 1, 10), 10.0 },
                        { d(2026, 4, 10),  4.0 } };
        a.prices    = { { d(2026, 1, 10),  50.0 },
                        { d(2026, 2,  1),  55.0 },
                        { d(2026, 3, 10),  58.0 },
                        { d(2026, 4, 10),  60.0 },
                        { d(2026, 4, 30),  62.0 } };

        PortfolioShareSeriesInput b;
        b.shareGuid = QStringLiteral("guid-b");
        b.name      = QStringLiteral("Aktie B");
        b.buys      = { { d(2026, 2, 1), 5.0, 200.0 } };
        b.costs     = { { d(2026, 2, 1), 8.0 } };
        b.prices    = { { d(2026, 2,  1), 200.0 },
                        { d(2026, 3, 10), 190.0 },
                        { d(2026, 4, 10), 195.0 },
                        { d(2026, 4, 30), 210.0 } };

        return { a, b };
    }

private slots:

    // ── Date grid ────────────────────────────────────────────────────────

    void test_buildDateGrid_unionOfPriceAndTransactionDates()
    {
        const QList<QDate> grid =
            PortfolioSeriesCalculator::buildDateGrid(referenceFixture());

        // Union of both shares' price dates; every transaction date already
        // coincides with a trading day here.
        QCOMPARE(grid.size(), 5);
        QCOMPARE(grid.at(0), d(2026, 1, 10));
        QCOMPARE(grid.at(1), d(2026, 2,  1));
        QCOMPARE(grid.at(2), d(2026, 3, 10));
        QCOMPARE(grid.at(3), d(2026, 4, 10));
        QCOMPARE(grid.at(4), d(2026, 4, 30));
    }

    void test_buildDateGrid_includesTransactionDateWithoutPrice()
    {
        // A cost entry on a day that has no price entry must still become a
        // grid point, otherwise the line would skip the step it causes.
        PortfolioShareSeriesInput share;
        share.name   = QStringLiteral("Aktie");
        share.buys   = { { d(2026, 1, 5), 10.0, 100.0 } };
        share.costs  = { { d(2026, 1, 7),  25.0 } }; // no price on the 7th
        share.prices = { { d(2026, 1, 5), 100.0 },
                         { d(2026, 1, 9), 100.0 } };

        const QList<QDate> grid = PortfolioSeriesCalculator::buildDateGrid({ share });

        QCOMPARE(grid.size(), 3);
        QCOMPARE(grid.at(1), d(2026, 1, 7));
    }

    void test_buildDateGrid_respectsWindow()
    {
        const QList<QDate> grid = PortfolioSeriesCalculator::buildDateGrid(
            referenceFixture(), d(2026, 2, 1), d(2026, 4, 10));

        QCOMPARE(grid.size(), 3);
        QCOMPARE(grid.constFirst(), d(2026, 2,  1));
        QCOMPARE(grid.constLast(),  d(2026, 4, 10));
    }

    // ── Forward fill ─────────────────────────────────────────────────────

    void test_closingPriceAt_forwardFillsLastKnownPrice()
    {
        const QList<PortfolioPriceEvent> prices = { { d(2026, 5, 30), 50.0 },
                                                    { d(2026, 6,  3), 51.0 } };

        CMP_MONEY(PortfolioSeriesCalculator::closingPriceAt(prices, d(2026, 5, 30)), 50.0);
        CMP_MONEY(PortfolioSeriesCalculator::closingPriceAt(prices, d(2026, 6,  2)), 50.0);
        CMP_MONEY(PortfolioSeriesCalculator::closingPriceAt(prices, d(2026, 6,  3)), 51.0);
        CMP_MONEY(PortfolioSeriesCalculator::closingPriceAt(prices, d(2026, 7,  1)), 51.0);
    }

    void test_closingPriceAt_zeroBeforeFirstEntry()
    {
        const QList<PortfolioPriceEvent> prices = { { d(2026, 5, 30), 50.0 } };
        CMP_MONEY(PortfolioSeriesCalculator::closingPriceAt(prices, d(2026, 5, 1)), 0.0);
    }

    void test_forwardFill_gapDayDoesNotDentThePortfolioSum()
    {
        // Share B has no entry on the 2nd. Its 50 EUR must be carried forward
        // from the 1st, otherwise the sum would dip on a day where nothing
        // actually happened.
        PortfolioShareSeriesInput a;
        a.name   = QStringLiteral("A");
        a.buys   = { { d(2026, 6, 1), 1.0, 100.0 } };
        a.prices = { { d(2026, 6, 1), 100.0 },
                     { d(2026, 6, 2), 100.0 } };

        PortfolioShareSeriesInput b;
        b.name   = QStringLiteral("B");
        b.buys   = { { d(2026, 6, 1), 1.0, 50.0 } };
        b.prices = { { d(2026, 6, 1), 50.0 } }; // nothing on the 2nd

        const PortfolioSeriesResult r = PortfolioSeriesCalculator::compute({ a, b });

        QCOMPARE(r.points.size(), 2);
        CMP_MONEY(r.points.at(1).holdingsValue, 150.0);
        CMP_MONEY(r.points.at(1).development,     0.0);
    }

    // ── Reference scenario ───────────────────────────────────────────────

    void test_referenceScenario_twoSharesWithPartialSale()
    {
        const PortfolioSeriesResult r =
            PortfolioSeriesCalculator::compute(referenceFixture());

        QCOMPARE(r.points.size(), 5);
        QVERIFY(r.sharesWithoutHistory.isEmpty());

        // The five values confirmed by Nessie on 05.08.2026.
        CMP_MONEY(r.points.at(0).development, -10.0);
        CMP_MONEY(r.points.at(1).development,  82.0);
        CMP_MONEY(r.points.at(2).development, 114.0);
        CMP_MONEY(r.points.at(3).development, 160.0);
        CMP_MONEY(r.points.at(4).development, 259.0);
    }

    void test_referenceScenario_componentsOfLastPoint()
    {
        const PortfolioSeriesResult r =
            PortfolioSeriesCalculator::compute(referenceFixture());
        const PortfolioSeriesPoint& last = r.points.constLast();

        CMP_MONEY(last.holdingsValue,      1794.0); // 12 x 62 + 5 x 210
        CMP_MONEY(last.realizedGain,         65.0); // (480 - 15) - 8 x 50
        CMP_MONEY(last.dividends,            22.0); // 30 gross - 8 taxes
        CMP_MONEY(last.costs,                22.0); // 10 + 8 + 4
        CMP_MONEY(last.purchaseValueHeld,  1600.0); // 12 x 50 + 5 x 200
        CMP_MONEY(last.purchaseValueTotal, 2000.0); // 20 x 50 + 5 x 200
    }

    void test_referenceScenario_percentUsesTotalPurchaseValue()
    {
        const PortfolioSeriesResult r =
            PortfolioSeriesCalculator::compute(referenceFixture());

        // 259 / 2000 x 100 — all buys, not only the held ones.
        QVERIFY(qAbs(r.points.constLast().developmentPct - 12.95) < 0.01);
    }

    void test_buyDoesNotMoveTheLine()
    {
        // The whole point of the "reine Wertentwicklung" definition: paying in
        // is not a gain. Share B is bought on the 2nd at exactly its price, so
        // the line must not jump.
        PortfolioShareSeriesInput a;
        a.name   = QStringLiteral("A");
        a.buys   = { { d(2026, 6, 1), 10.0, 100.0 } };
        a.prices = { { d(2026, 6, 1), 100.0 },
                     { d(2026, 6, 2), 110.0 } };

        PortfolioShareSeriesInput b;
        b.name   = QStringLiteral("B");
        b.buys   = { { d(2026, 6, 2), 25.0, 200.0 } }; // 5000 EUR paid in
        b.prices = { { d(2026, 6, 2), 200.0 } };

        const PortfolioSeriesResult r = PortfolioSeriesCalculator::compute({ a, b });

        CMP_MONEY(r.points.at(0).development,   0.0);
        CMP_MONEY(r.points.at(1).development, 100.0); // only A's 10 x 10 EUR gain
    }

    // ── Complete sale ────────────────────────────────────────────────────

    void test_completeSale_lineStaysFlatAfterwards()
    {
        // Single-share walkthrough from the discussion: buy 10 x 100, fee 10;
        // dividend gross 20 minus 5 tax = 15; sale 10 x 120, fee 5, tax 30.
        PortfolioShareSeriesInput s;
        s.name      = QStringLiteral("Aktie");
        s.buys      = { { d(2026, 3, 1), 10.0, 100.0 } };
        s.sales     = { { d(2026, 5, 15), 10.0, 120.0, 30.0 } };
        s.dividends = { { d(2026, 4, 15), 15.0 } };
        s.costs     = { { d(2026, 3,  1), 10.0 },
                        { d(2026, 5, 15),  5.0 } };
        s.prices    = { { d(2026, 3,  1), 100.0 },
                        { d(2026, 4,  1), 110.0 },
                        { d(2026, 4, 15), 110.0 },
                        { d(2026, 5,  1), 120.0 },
                        { d(2026, 5, 15), 120.0 },
                        { d(2026, 6,  1), 120.0 } };

        const PortfolioSeriesResult r = PortfolioSeriesCalculator::compute({ s });

        QCOMPARE(r.points.size(), 6);
        CMP_MONEY(r.points.at(0).development, -10.0); // buy fee only
        CMP_MONEY(r.points.at(1).development,  90.0);
        CMP_MONEY(r.points.at(2).development, 105.0); // + net dividend
        CMP_MONEY(r.points.at(3).development, 205.0);
        CMP_MONEY(r.points.at(4).development, 170.0); // - sale fee 5 - tax 30
        CMP_MONEY(r.points.at(5).development, 170.0); // nothing held, line flat

        // Held purchase value is 0 afterwards, so the percentage has to fall
        // back on the total purchase value to stay meaningful.
        CMP_MONEY(r.points.constLast().purchaseValueHeld,     0.0);
        CMP_MONEY(r.points.constLast().purchaseValueTotal, 1000.0);
        QVERIFY(qAbs(r.points.constLast().developmentPct - 17.0) < 0.01);
    }

    void test_fifo_usesOldestLotFirst()
    {
        // Two lots at different prices, one share sold. FIFO must consume the
        // 100 EUR lot, not the 200 EUR lot and not an average of the two.
        PortfolioShareSeriesInput s;
        s.name   = QStringLiteral("Aktie");
        s.buys   = { { d(2026, 1, 1), 1.0, 100.0 },
                     { d(2026, 2, 1), 1.0, 200.0 } };
        s.sales  = { { d(2026, 3, 1), 1.0, 300.0, 0.0 } };
        s.prices = { { d(2026, 1, 1), 100.0 },
                     { d(2026, 2, 1), 200.0 },
                     { d(2026, 3, 1), 300.0 } };

        const PortfolioSeriesResult r = PortfolioSeriesCalculator::compute({ s });
        const PortfolioSeriesPoint& last = r.points.constLast();

        CMP_MONEY(last.realizedGain,      200.0); // 300 - 100 (FIFO), not 300 - 200
        CMP_MONEY(last.purchaseValueHeld, 200.0); // the 200 EUR lot remains
        CMP_MONEY(last.holdingsValue,     300.0); // 1 x 300
        CMP_MONEY(last.development,       300.0); // 300 + 200 - 200
    }

    // ── Shares without price history ─────────────────────────────────────

    void test_shareWithoutHistory_isExcludedAndReported()
    {
        PortfolioShareSeriesInput withHistory;
        withHistory.name   = QStringLiteral("Mit Historie");
        withHistory.buys   = { { d(2026, 6, 1), 10.0, 100.0 } };
        withHistory.prices = { { d(2026, 6, 1), 110.0 } };

        PortfolioShareSeriesInput withoutHistory;
        withoutHistory.name  = QStringLiteral("Ohne Historie");
        withoutHistory.buys  = { { d(2026, 6, 1), 10.0, 500.0 } };
        withoutHistory.costs = { { d(2026, 6, 1), 99.0 } };
        // prices intentionally empty — update type "Nur Kurs" / "Kein Update"

        const PortfolioSeriesResult r =
            PortfolioSeriesCalculator::compute({ withHistory, withoutHistory });

        QCOMPARE(r.sharesWithoutHistory.size(), 1);
        QCOMPARE(r.sharesWithoutHistory.constFirst(), QStringLiteral("Ohne Historie"));

        // Neither its purchase value nor its 99 EUR cost may leak into the line.
        QCOMPARE(r.points.size(), 1);
        CMP_MONEY(r.points.constFirst().purchaseValueTotal, 1000.0);
        CMP_MONEY(r.points.constFirst().costs,                 0.0);
        CMP_MONEY(r.points.constFirst().development,         100.0);
    }

    void test_shareContributesNothingBeforeItsFirstPriceDate()
    {
        // A was bought on the 1st but its history only starts on the 3rd. On
        // the 1st it must contribute nothing — neither holdings nor purchase
        // value — otherwise the line would show a phantom loss of 1000 EUR.
        PortfolioShareSeriesInput a;
        a.name   = QStringLiteral("A");
        a.buys   = { { d(2026, 6, 1), 10.0, 100.0 } };
        a.prices = { { d(2026, 6, 3), 100.0 } };

        PortfolioShareSeriesInput b;
        b.name   = QStringLiteral("B");
        b.buys   = { { d(2026, 6, 1), 1.0, 500.0 } };
        b.prices = { { d(2026, 6, 1), 500.0 },
                     { d(2026, 6, 3), 500.0 } };

        const PortfolioSeriesResult r = PortfolioSeriesCalculator::compute({ a, b });

        QCOMPARE(r.points.size(), 2);
        CMP_MONEY(r.points.at(0).purchaseValueTotal,  500.0); // only B
        CMP_MONEY(r.points.at(0).development,           0.0);
        CMP_MONEY(r.points.at(1).purchaseValueTotal, 1500.0); // now both
        CMP_MONEY(r.points.at(1).development,           0.0);
    }

    // ── Edge cases ───────────────────────────────────────────────────────

    void test_invalidDates_areIgnoredNotBookedAtTheFirstPoint()
    {
        // Regression 06.08.2026: ein ungültiges QDate ist in Qt kleiner als
        // jedes gültige, die "Datum <= Stichtag"-Schleifen hätten solche
        // Einträge sonst alle am allerersten Stichtag verbucht. Im Feldtest
        // zeigte die Kurve dadurch Jahre vor dem ersten Kauf einen
        // Kostenblock.
        PortfolioShareSeriesInput share;
        share.name   = QStringLiteral("Aktie");
        share.buys   = { { d(2026, 6, 2), 10.0, 100.0 } };
        share.costs  = { { QDate(), 1000.0 } }; // Kosteneintrag ohne Datum
        share.prices = { { d(2026, 6, 1), 100.0 },
                         { d(2026, 6, 2), 100.0 } };

        const PortfolioSeriesResult r = PortfolioSeriesCalculator::compute({ share });

        QCOMPARE(r.points.size(), 2);
        CMP_MONEY(r.points.at(0).costs,       0.0);
        CMP_MONEY(r.points.at(0).development, 0.0);
        CMP_MONEY(r.points.at(1).costs,       0.0);
        CMP_MONEY(r.points.at(1).development, 0.0);

        QCOMPARE(r.diagnostics.size(), 1);
        QCOMPARE(r.diagnostics.constFirst().invalidDates, 1);
        QCOMPARE(r.diagnostics.constFirst().costs,        0);
    }

    void test_invalidDates_doNotCreateGridPoints()
    {
        PortfolioShareSeriesInput share;
        share.name   = QStringLiteral("Aktie");
        share.buys   = { { QDate(), 10.0, 100.0 } };
        share.prices = { { d(2026, 6, 1), 100.0 } };

        const QList<QDate> grid = PortfolioSeriesCalculator::buildDateGrid({ share });

        QCOMPARE(grid.size(), 1);
        QCOMPARE(grid.constFirst(), d(2026, 6, 1));
    }

    void test_diagnostics_countLoadedRecords()
    {
        const PortfolioSeriesResult r =
            PortfolioSeriesCalculator::compute(referenceFixture());

        QCOMPARE(r.diagnostics.size(), 2);

        const PortfolioShareDiagnostics& a = r.diagnostics.constFirst();
        QCOMPARE(a.name,         QStringLiteral("Aktie A"));
        QCOMPARE(a.buys,         1);
        QCOMPARE(a.sales,        1);
        QCOMPARE(a.dividends,    1);
        QCOMPARE(a.costs,        2);
        QCOMPARE(a.prices,       5);
        QCOMPARE(a.invalidDates, 0);
        QCOMPARE(a.firstBuy,     d(2026, 1, 10));
        QCOMPARE(a.firstPrice,   d(2026, 1, 10));
        QCOMPARE(a.lastPrice,    d(2026, 4, 30));
        QVERIFY(!a.excluded);
    }

    void test_diagnostics_markExcludedShare()
    {
        PortfolioShareSeriesInput withoutHistory;
        withoutHistory.name = QStringLiteral("Ohne Historie");
        withoutHistory.buys = { { d(2026, 6, 1), 10.0, 100.0 } };

        const PortfolioSeriesResult r =
            PortfolioSeriesCalculator::compute({ withoutHistory });

        QCOMPARE(r.diagnostics.size(), 1);
        QVERIFY(r.diagnostics.constFirst().excluded);
        QCOMPARE(r.diagnostics.constFirst().prices, 0);
    }

    void test_perShareDetail_isOnlyFilledWhenRequested()
    {
        const PortfolioSeriesResult without =
            PortfolioSeriesCalculator::compute(referenceFixture());
        QVERIFY(without.sharePoints.isEmpty());

        const PortfolioSeriesResult with = PortfolioSeriesCalculator::compute(
            referenceFixture(), QDate(), QDate(), /*withPerShareDetail*/ true);

        // Aktie A traegt an allen fuenf Stichtagen bei, Aktie B erst ab dem
        // zweiten — vor ihrem ersten Kursdatum liefert sie nichts.
        QCOMPARE(with.sharePoints.size(), 9);

        const PortfolioSharePoint& first = with.sharePoints.constFirst();
        QCOMPARE(first.name, QStringLiteral("Aktie A"));
        CMP_MONEY(first.volume,            20.0);
        CMP_MONEY(first.price,             50.0);
        CMP_MONEY(first.holdingsValue,   1000.0);
        CMP_MONEY(first.purchaseValueHeld, 1000.0);
    }

    void test_emptyInput_yieldsEmptyResult()
    {
        const PortfolioSeriesResult r = PortfolioSeriesCalculator::compute({});
        QVERIFY(r.points.isEmpty());
        QVERIFY(r.sharesWithoutHistory.isEmpty());
    }

    void test_percentIsZeroWhenNothingWasEverBought()
    {
        // Guard against division by zero, same pattern as ShareCalculator.
        PortfolioShareSeriesInput s;
        s.name   = QStringLiteral("Aktie");
        s.costs  = { { d(2026, 6, 1), 12.0 } };
        s.prices = { { d(2026, 6, 1), 100.0 } };

        const PortfolioSeriesResult r = PortfolioSeriesCalculator::compute({ s });

        CMP_MONEY(r.points.constFirst().purchaseValueTotal,   0.0);
        CMP_MONEY(r.points.constFirst().development,        -12.0);
        CMP_MONEY(r.points.constFirst().developmentPct,       0.0);
    }

    void test_unsortedInputIsSortedInternally()
    {
        PortfolioShareSeriesInput s;
        s.name   = QStringLiteral("Aktie");
        s.buys   = { { d(2026, 2, 1), 1.0, 200.0 },
                     { d(2026, 1, 1), 1.0, 100.0 } }; // deliberately reversed
        s.sales  = { { d(2026, 3, 1), 1.0, 300.0, 0.0 } };
        s.prices = { { d(2026, 3, 1), 300.0 },
                     { d(2026, 1, 1), 100.0 },
                     { d(2026, 2, 1), 200.0 } };      // deliberately reversed

        const PortfolioSeriesResult r = PortfolioSeriesCalculator::compute({ s });

        QCOMPARE(r.points.size(), 3);
        QCOMPARE(r.points.constFirst().date, d(2026, 1, 1));
        // FIFO still consumes the January lot despite the input order.
        CMP_MONEY(r.points.constLast().realizedGain, 200.0);
    }

    void test_windowLimitsPointsButNotAccumulatedState()
    {
        // Starting the window in March must not lose the January buy: the
        // first visible point already carries the full held position.
        const PortfolioSeriesResult r = PortfolioSeriesCalculator::compute(
            referenceFixture(), d(2026, 3, 10), d(2026, 4, 30));

        // Three trading days fall inside the window: 10.03., 10.04., 30.04.
        QCOMPARE(r.points.size(), 3);
        QCOMPARE(r.points.constFirst().date, d(2026, 3, 10));
        QCOMPARE(r.points.constLast().date,  d(2026, 4, 30));

        // Values are identical to the unwindowed run — the window only trims
        // the visible points, it does not reset the accumulated state.
        CMP_MONEY(r.points.at(0).development, 114.0);
        CMP_MONEY(r.points.at(1).development, 160.0);
        CMP_MONEY(r.points.at(2).development, 259.0);

        // The January buy is still fully accounted for in the first point.
        CMP_MONEY(r.points.constFirst().purchaseValueTotal, 2000.0);
    }
};

QTEST_MAIN(TestPortfolioSeriesCalculator)
#include "tst_portfolioseriescalculator.moc"

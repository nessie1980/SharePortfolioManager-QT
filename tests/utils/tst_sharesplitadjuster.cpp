// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// Unit tests for ShareSplitAdjuster — the stateless, database-free
// conversion between the Beleg-Skala (as stored in buys/sales/daily_values)
// and today's scale (after all known splits). Fixture values mirror the
// Alphabet Inc. Cl. A case documented in ARCHITECTURE.md ("Aktiensplits
// werden nicht behandelt"): a buy of 5 shares at 1003.00 EUR on 18.03.2020,
// 20:1 split ex-date 18.07.2022.
#include <QtTest>

#include "../../app/utils/ShareSplitAdjuster.h"

// Compare two doubles to within a tiny tolerance — same pattern as
// tst_sharecalculator.cpp / tst_portfolioseriescalculator.cpp.
#define CMP_MONEY(actual, expected)                                            \
    QVERIFY2(qAbs((actual) - (expected)) < 1e-6,                               \
             qPrintable(QStringLiteral("got %1, expected %2")                  \
                            .arg(actual, 0, 'f', 6).arg(expected, 0, 'f', 6)))

namespace {
QDate d(int y, int m, int day) { return QDate(y, m, day); }
}

class TestShareSplitAdjuster : public QObject
{
    Q_OBJECT

private:
    static ShareSplitObject makeSplit(const QDate& date, double ratioNew, double ratioOld,
                                      bool pricesAdjusted = false)
    {
        return ShareSplitObject(QStringLiteral("split-guid"), QStringLiteral("share-guid"),
                                date, ratioNew, ratioOld, pricesAdjusted);
    }

private slots:

    // ── volumeFactor ────────────────────────────────────────────────────

    void test_volumeFactor_noSplits_returnsOne()
    {
        CMP_MONEY(ShareSplitAdjuster::volumeFactor({}, d(2020, 1, 1)), 1.0);
    }

    void test_volumeFactor_splitAfterDate_applies()
    {
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 20.0, 1.0) };
        CMP_MONEY(ShareSplitAdjuster::volumeFactor(splits, d(2020, 3, 18)), 20.0);
    }

    void test_volumeFactor_splitOnOrBeforeDate_doesNotApply()
    {
        // Der Beleg des Splittags selbst liegt fachlich vor dem Split.
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 20.0, 1.0) };
        CMP_MONEY(ShareSplitAdjuster::volumeFactor(splits, d(2022, 7, 18)), 1.0);
        CMP_MONEY(ShareSplitAdjuster::volumeFactor(splits, d(2022, 7, 19)), 1.0);
    }

    void test_volumeFactor_multipleSplits_cumulate()
    {
        // 4:1 in 2018, dann 20:1 in 2022 — ein Kauf von 2015 sieht beide.
        const QList<ShareSplitObject> splits = {
            makeSplit(d(2018, 3, 1),  4.0, 1.0),
            makeSplit(d(2022, 7, 18), 20.0, 1.0),
        };
        CMP_MONEY(ShareSplitAdjuster::volumeFactor(splits, d(2015, 1, 1)), 80.0);
    }

    void test_volumeFactor_dateBetweenTwoSplits_onlyLaterApplies()
    {
        const QList<ShareSplitObject> splits = {
            makeSplit(d(2018, 3, 1),  4.0, 1.0),
            makeSplit(d(2022, 7, 18), 20.0, 1.0),
        };
        CMP_MONEY(ShareSplitAdjuster::volumeFactor(splits, d(2020, 1, 1)), 20.0);
    }

    void test_volumeFactor_reverseSplit_isFractional()
    {
        // Reverse-Split 1:10 -> Faktor 0.1
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 1.0, 10.0) };
        CMP_MONEY(ShareSplitAdjuster::volumeFactor(splits, d(2020, 1, 1)), 0.1);
    }

    void test_volumeFactor_unsortedInput_stillCumulatesCorrectly()
    {
        const QList<ShareSplitObject> splits = {
            makeSplit(d(2022, 7, 18), 20.0, 1.0),
            makeSplit(d(2018, 3, 1),  4.0, 1.0),
        };
        CMP_MONEY(ShareSplitAdjuster::volumeFactor(splits, d(2015, 1, 1)), 80.0);
    }

    void test_volumeFactor_dateAfterAllSplits_returnsOne()
    {
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 20.0, 1.0) };
        CMP_MONEY(ShareSplitAdjuster::volumeFactor(splits, d(2023, 1, 1)), 1.0);
    }

    // ── priceFactorForHistory ───────────────────────────────────────────

    void test_priceFactorForHistory_unadjustedSplit_applies()
    {
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 20.0, 1.0, false) };
        CMP_MONEY(ShareSplitAdjuster::priceFactorForHistory(splits, d(2020, 1, 1)), 20.0);
    }

    void test_priceFactorForHistory_adjustedSplit_doesNotApply()
    {
        // Split, dessen Tageswert-Historie bereits split-bereinigt vorliegt —
        // das Gegenstück zum Alphabet-Fall aus ARCHITECTURE.md (dort: false).
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 20.0, 1.0, true) };
        CMP_MONEY(ShareSplitAdjuster::priceFactorForHistory(splits, d(2020, 1, 1)), 1.0);
    }

    void test_priceFactorForHistory_mixedSplits_onlyUnadjustedCumulate()
    {
        const QList<ShareSplitObject> splits = {
            makeSplit(d(2018, 3, 1),  4.0, 1.0, true),   // Historie bereits bereinigt
            makeSplit(d(2022, 7, 18), 20.0, 1.0, false), // Historie nicht bereinigt
        };
        CMP_MONEY(ShareSplitAdjuster::priceFactorForHistory(splits, d(2015, 1, 1)), 20.0);
    }

    // ── adjustedVolume / adjustedTransactionPrice ───────────────────────

    void test_adjustedVolume_scalesUp()
    {
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 20.0, 1.0) };
        CMP_MONEY(ShareSplitAdjuster::adjustedVolume(5.0, splits, d(2020, 3, 18)), 100.0);
    }

    void test_adjustedTransactionPrice_scalesDown()
    {
        // Alphabet-Fall: Kauf vom 18.03.2020 zu 1003,00 € (Beleg, vor dem
        // Split am 18.07.2022) -> heutige Skala: 50,15 €.
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 20.0, 1.0) };
        CMP_MONEY(ShareSplitAdjuster::adjustedTransactionPrice(1003.0, splits, d(2020, 3, 18)),
                  50.15);
    }

    void test_adjustedTransactionPrice_valueInvariant()
    {
        // v * p bleibt über den Split hinweg exakt gleich — Splits schaffen
        // keinen Gewinn und keinen Verlust.
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 20.0, 1.0) };
        const double volumeBefore = 5.0;
        const double priceBefore  = 1003.0;

        const double volumeAfter = ShareSplitAdjuster::adjustedVolume(volumeBefore, splits,
                                                                       d(2020, 3, 18));
        const double priceAfter  = ShareSplitAdjuster::adjustedTransactionPrice(
            priceBefore, splits, d(2020, 3, 18));

        CMP_MONEY(volumeBefore * priceBefore, volumeAfter * priceAfter);
    }

    void test_adjustedTransactionPrice_noSplits_isUnchanged()
    {
        CMP_MONEY(ShareSplitAdjuster::adjustedTransactionPrice(50.15, {}, d(2020, 3, 18)), 50.15);
    }

    // ── adjustedHistoryPrice ─────────────────────────────────────────────

    void test_adjustedHistoryPrice_unadjustedHistory_isScaledDown()
    {
        // Der Alphabet-Fall: Tageswert vom 18.03.2020 stand unbereinigt bei
        // rund 1003 € -> heutige Skala: 50,15 €.
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 20.0, 1.0, false) };
        CMP_MONEY(ShareSplitAdjuster::adjustedHistoryPrice(1003.0, splits, d(2020, 3, 18)), 50.15);
    }

    void test_adjustedHistoryPrice_alreadyAdjustedHistory_isUnchanged()
    {
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 20.0, 1.0, true) };
        CMP_MONEY(ShareSplitAdjuster::adjustedHistoryPrice(50.15, splits, d(2020, 3, 18)), 50.15);
    }

    // ── Datum nach dem Split ─────────────────────────────────────────────

    void test_dateAfterSplit_pricesAndVolumesUnchanged()
    {
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 20.0, 1.0) };
        CMP_MONEY(ShareSplitAdjuster::adjustedVolume(100.0, splits, d(2023, 1, 1)), 100.0);
        CMP_MONEY(ShareSplitAdjuster::adjustedTransactionPrice(50.15, splits, d(2023, 1, 1)),
                  50.15);
        CMP_MONEY(ShareSplitAdjuster::adjustedHistoryPrice(50.15, splits, d(2023, 1, 1)), 50.15);
    }
};

QTEST_MAIN(TestShareSplitAdjuster)
#include "tst_sharesplitadjuster.moc"

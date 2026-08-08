// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// Unit tests for SaleFifoAllocator — die gemeinsame, split-bewusste
// FIFO-Verkaufszuteilung (Aktiensplit-Behandlung, Phase 2c, 07.08.2026,
// siehe ARCHITECTURE.md "Offene Punkte"). Zustandslos und datenbankfrei,
// gleicher Ansatz wie tst_sharesplitadjuster.cpp. Fixture-Werte für die
// split-übergreifenden Fälle sind gegen eine unabhängige Python-Simulation
// der Zuteilungslogik gegengerechnet.
#include <QtTest>
#include <QDateTime>

#include "../../app/utils/SaleFifoAllocator.h"

#define CMP_MONEY(actual, expected)                                            \
    QVERIFY2(qAbs((actual) - (expected)) < 1e-6,                               \
             qPrintable(QStringLiteral("got %1, expected %2")                  \
                            .arg(actual, 0, 'f', 6).arg(expected, 0, 'f', 6)))

namespace {
QDate d(int y, int m, int day) { return QDate(y, m, day); }

BuyObject makeBuy(const QString& guid, const QDate& date, double volume,
                  double volumeSold, double price)
{
    return BuyObject(guid, QStringLiteral("share-guid"), QString(), QString(),
                     QDateTime(date, QTime(10, 0)).toString(Qt::ISODate),
                     volume, volumeSold, price);
}

ShareSplitObject makeSplit(const QDate& date, double ratioNew, double ratioOld,
                           bool pricesAdjusted = false)
{
    return ShareSplitObject(QStringLiteral("split-guid"), QStringLiteral("share-guid"),
                            date, ratioNew, ratioOld, pricesAdjusted);
}
}

class TestSaleFifoAllocator : public QObject
{
    Q_OBJECT

private slots:

    // ── Grundfälle, ohne Splits ───────────────────────────────────────────

    void test_allocate_singleBuy_fullyCovers()
    {
        const QList<BuyObject> buys = { makeBuy("b1", d(2024, 1, 1), 10.0, 0.0, 100.0) };

        const QList<FifoAllocationRow> rows =
            SaleFifoAllocator::allocate(5.0, d(2024, 6, 1), buys, {});

        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows.first().buyGuid, QStringLiteral("b1"));
        CMP_MONEY(rows.first().volume, 5.0);
        CMP_MONEY(rows.first().buyPrice, 100.0);
    }

    void test_allocate_multipleBuys_fifoOrder()
    {
        // Verkauf von 15 über zwei Käufe (je 10 verfügbar) — der ältere
        // zuerst vollständig, der Rest vom jüngeren.
        const QList<BuyObject> buys = {
            makeBuy("older", d(2024, 1, 1), 10.0, 0.0, 100.0),
            makeBuy("newer", d(2024, 3, 1), 10.0, 0.0, 110.0),
        };

        const QList<FifoAllocationRow> rows =
            SaleFifoAllocator::allocate(15.0, d(2024, 6, 1), buys, {});

        QCOMPARE(rows.size(), 2);
        QCOMPARE(rows.at(0).buyGuid, QStringLiteral("older"));
        CMP_MONEY(rows.at(0).volume, 10.0);
        QCOMPARE(rows.at(1).buyGuid, QStringLiteral("newer"));
        CMP_MONEY(rows.at(1).volume, 5.0);
    }

    void test_allocate_insufficientVolume_stopsWhenExhausted()
    {
        // Nur 10 verfügbar, 100 nachgefragt — der Rest bleibt offen (wie
        // im bisherigen Verhalten der drei Einzelimplementierungen).
        const QList<BuyObject> buys = { makeBuy("b1", d(2024, 1, 1), 10.0, 0.0, 50.0) };

        const QList<FifoAllocationRow> rows =
            SaleFifoAllocator::allocate(100.0, d(2024, 6, 1), buys, {});

        QCOMPARE(rows.size(), 1);
        CMP_MONEY(rows.first().volume, 10.0);
    }

    void test_allocate_fullyConsumedBuy_isSkipped()
    {
        const QList<BuyObject> buys = {
            makeBuy("used-up", d(2024, 1, 1), 10.0, 10.0, 100.0), // avail = 0
            makeBuy("open",    d(2024, 2, 1), 10.0, 0.0,  105.0),
        };

        const QList<FifoAllocationRow> rows =
            SaleFifoAllocator::allocate(5.0, d(2024, 6, 1), buys, {});

        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows.first().buyGuid, QStringLiteral("open"));
    }

    void test_allocate_zeroSaleVolume_returnsEmpty()
    {
        const QList<BuyObject> buys = { makeBuy("b1", d(2024, 1, 1), 10.0, 0.0, 50.0) };
        QVERIFY(SaleFifoAllocator::allocate(0.0, d(2024, 6, 1), buys, {}).isEmpty());
    }

    void test_allocate_emptyAvailableBuys_returnsEmpty()
    {
        QVERIFY(SaleFifoAllocator::allocate(5.0, d(2024, 6, 1), {}, {}).isEmpty());
    }

    // ── Split zwischen Kauf und Verkauf ───────────────────────────────────
    //
    // Fixture: Kauf vom 01.01.2020 (5 Stück Beleg-Skala, davon 2 bereits
    // verkauft -> 3 Stück verfügbar), 20:1-Split am 01.01.2021, Verkauf von
    // 40 Stück (heutige Skala) am 01.01.2022. availToday = 3 x 20 = 60;
    // saleToday = 40 x 1 = 40 (kein weiterer Split nach dem Verkaufsdatum);
    // takeToday = min(60, 40) = 40; zurück auf die Beleg-Skala des Kaufs:
    // 40 / 20 = 2,0.

    void test_allocate_splitBetweenBuyAndSale_scalesToBuysBelegSkala()
    {
        const QList<BuyObject> buys = { makeBuy("b1", d(2020, 1, 1), 5.0, 2.0, 1000.0) };
        const QList<ShareSplitObject> splits = { makeSplit(d(2021, 1, 1), 20.0, 1.0) };

        const QList<FifoAllocationRow> rows =
            SaleFifoAllocator::allocate(40.0, d(2022, 1, 1), buys, splits);

        QCOMPARE(rows.size(), 1);
        CMP_MONEY(rows.first().volume,   2.0);
        CMP_MONEY(rows.first().buyPrice, 1000.0);
    }

    void test_allocate_splitBetweenBuyAndSale_valueInvariant()
    {
        // Der Euro-Wert des zugeteilten Anteils bleibt exakt gleich,
        // unabhängig davon, in welcher Skala man ihn ausrechnet:
        // takeBeleg x buyPrice(Beleg) == takeToday x buyPrice(heutig).
        const QList<BuyObject> buys = { makeBuy("b1", d(2020, 1, 1), 5.0, 2.0, 1000.0) };
        const QList<ShareSplitObject> splits = { makeSplit(d(2021, 1, 1), 20.0, 1.0) };

        const QList<FifoAllocationRow> rows =
            SaleFifoAllocator::allocate(40.0, d(2022, 1, 1), buys, splits);

        QCOMPARE(rows.size(), 1);
        const double belegValue = rows.first().volume * rows.first().buyPrice;
        CMP_MONEY(belegValue, 2000.0); // 2,0 Stück x 1.000,00 € = 2,0 x 20 Stück x 50,00 €
    }

    void test_allocate_reverseSplitBetweenBuyAndSale()
    {
        // Reverse-Split 1:10: Kauf von 100 Stück (Beleg-Skala) vor dem
        // Split entspricht 10 Stück danach; Verkauf von 9 der 10 heutigen
        // Stücke -> takeToday = 9, zurück auf Beleg-Skala: 9 / 0,1 = 90.
        const QList<BuyObject> buys = { makeBuy("b1", d(2020, 1, 1), 100.0, 0.0, 5.0) };
        const QList<ShareSplitObject> splits = { makeSplit(d(2021, 1, 1), 1.0, 10.0) };

        const QList<FifoAllocationRow> rows =
            SaleFifoAllocator::allocate(9.0, d(2022, 1, 1), buys, splits);

        QCOMPARE(rows.size(), 1);
        CMP_MONEY(rows.first().volume, 90.0);
    }

    void test_allocate_multipleBuysAcrossSplitBoundary()
    {
        // Ein Kauf vor dem Split (5 Stück Beleg-Skala -> 100 heutige
        // Stücke), einer danach (50 Stück, bereits heutige Skala). Verkauf
        // von 120 heutigen Stücken: der ältere Kauf liefert alle 100
        // heutigen Stücke (= 5 Beleg-Stücke), der Rest (20) kommt vom
        // jüngeren Kauf (dessen Beleg-Skala = heutige Skala, also
        // ebenfalls 20).
        const QList<BuyObject> buys = {
            makeBuy("preSplit",  d(2019, 1, 1), 5.0,  0.0, 900.0),
            makeBuy("postSplit", d(2022, 1, 1), 50.0, 0.0, 45.0),
        };
        const QList<ShareSplitObject> splits = { makeSplit(d(2021, 1, 1), 20.0, 1.0) };

        const QList<FifoAllocationRow> rows =
            SaleFifoAllocator::allocate(120.0, d(2023, 1, 1), buys, splits);

        QCOMPARE(rows.size(), 2);
        QCOMPARE(rows.at(0).buyGuid, QStringLiteral("preSplit"));
        CMP_MONEY(rows.at(0).volume, 5.0);   // alle 5 Beleg-Stücke = 100 heutige
        QCOMPARE(rows.at(1).buyGuid, QStringLiteral("postSplit"));
        CMP_MONEY(rows.at(1).volume, 20.0);  // 20 heutige = 20 Beleg (kein Split danach)
    }

    void test_allocate_noSplits_matchesLegacyBehavior()
    {
        // Ohne Splits liefert ShareSplitAdjuster überall Faktor 1,0 — das
        // Ergebnis muss bitgenau wie die ursprüngliche, unskalierte
        // FIFO-Schleife sein (Rückwärtskompatibilität für alle bestehenden
        // Portfolios ohne Splits).
        const QList<BuyObject> buys = {
            makeBuy("b1", d(2024, 1, 1), 6.0, 1.0, 100.0), // avail 5
            makeBuy("b2", d(2024, 2, 1), 8.0, 0.0, 110.0),
        };

        const QList<FifoAllocationRow> rows =
            SaleFifoAllocator::allocate(7.0, d(2024, 6, 1), buys, {});

        QCOMPARE(rows.size(), 2);
        CMP_MONEY(rows.at(0).volume, 5.0);
        CMP_MONEY(rows.at(1).volume, 2.0);
    }
};

QTEST_MAIN(TestSaleFifoAllocator)
#include "tst_salefifoallocator.moc"

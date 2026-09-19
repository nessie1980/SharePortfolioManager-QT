// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// ChartPointSearch — gemeinsame Nächster-Datenpunkt-Suche für die
// Chart-Tooltips (zusammengeführt 19.09.2026, siehe ARCHITECTURE.md,
// "Nächster-Datenpunkt-Suche liegt in beiden Charts doppelt vor" unter
// "Erledigt / Archiv"). Header-only und zustandslos, daher kein
// QCoreApplication und keine .cpp aus app/ im Testziel — gleiche Bauweise
// wie tst_valueformatter/tst_numberparser.
//
// Die Grenzfälle standen bis zur Zusammenführung als
// test_nearestDataPoint_boundaryCases in tst_mainwindow.cpp (gegen
// ViewChart::nearestDataPoint(), das dabei entfallen ist).

#include <QtTest>
#include <QDateTime>

#include "utils/ChartPointSearch.h"

class TestChartPointSearch : public QObject
{
    Q_OBJECT

private:
    const QList<QPointF> m_points { {100.0, 1.0}, {200.0, 2.0}, {300.0, 3.0} };
    const QList<qint64>  m_msecs  { 100, 200, 300 };

private slots:
    // ── Überladung QList<QPointF> (ViewChart) ─────────────────────────────

    void test_points_emptyListReturnsMinusOne()
    {
        QCOMPARE(ChartPointSearch::nearestIndex(QList<QPointF>{}, 150.0), qsizetype(-1));
    }

    void test_points_beforeFirstReturnsFirst()
    {
        QCOMPARE(ChartPointSearch::nearestIndex(m_points, 50.0), qsizetype(0));
    }

    void test_points_afterLastReturnsLast()
    {
        QCOMPARE(ChartPointSearch::nearestIndex(m_points, 350.0), qsizetype(2));
    }

    void test_points_exactHit()
    {
        QCOMPARE(ChartPointSearch::nearestIndex(m_points, 200.0), qsizetype(1));
    }

    void test_points_closerToPrevious()
    {
        QCOMPARE(ChartPointSearch::nearestIndex(m_points, 240.0), qsizetype(1));
    }

    void test_points_closerToNext()
    {
        QCOMPARE(ChartPointSearch::nearestIndex(m_points, 260.0), qsizetype(2));
    }

    void test_points_tieGoesToLaterPoint()
    {
        QCOMPARE(ChartPointSearch::nearestIndex(m_points, 250.0), qsizetype(2));
    }

    void test_points_singlePointAlwaysWins()
    {
        const QList<QPointF> single { {100.0, 1.0} };
        QCOMPARE(ChartPointSearch::nearestIndex(single, 10.0),  qsizetype(0));
        QCOMPARE(ChartPointSearch::nearestIndex(single, 900.0), qsizetype(0));
    }

    // ── Überladung QList<qint64> (ViewPortfolioChart) ─────────────────────
    // Dieselben Fälle — beide Überladungen laufen über dieselbe Vorlage, der
    // Test sichert aber ab, dass die Umrechnung qint64 → double nichts
    // verschiebt.

    void test_msecs_emptyListReturnsMinusOne()
    {
        QCOMPARE(ChartPointSearch::nearestIndex(QList<qint64>{}, 150.0), qsizetype(-1));
    }

    void test_msecs_boundariesAndNeighbours()
    {
        QCOMPARE(ChartPointSearch::nearestIndex(m_msecs, 50.0),  qsizetype(0));
        QCOMPARE(ChartPointSearch::nearestIndex(m_msecs, 350.0), qsizetype(2));
        QCOMPARE(ChartPointSearch::nearestIndex(m_msecs, 200.0), qsizetype(1));
        QCOMPARE(ChartPointSearch::nearestIndex(m_msecs, 240.0), qsizetype(1));
        QCOMPARE(ChartPointSearch::nearestIndex(m_msecs, 260.0), qsizetype(2));
        QCOMPARE(ChartPointSearch::nearestIndex(m_msecs, 250.0), qsizetype(2));
    }

    void test_msecs_realisticDayTimestamps()
    {
        // Drei aufeinanderfolgende Tage um Mitternacht, Maus knapp vor der
        // Tagesmitte zwischen Tag 1 und Tag 2 — muss auf Tag 1 einrasten.
        const qint64 day = 24LL * 3600 * 1000;
        const qint64 d1  = QDateTime(QDate(2026, 8, 26), QTime(0, 0)).toMSecsSinceEpoch();
        const QList<qint64> days { d1, d1 + day, d1 + 2 * day };

        QCOMPARE(ChartPointSearch::nearestIndex(days, static_cast<double>(d1 + day / 2 - 1)),
                 qsizetype(0));
        QCOMPARE(ChartPointSearch::nearestIndex(days, static_cast<double>(d1 + day / 2 + 1)),
                 qsizetype(1));
    }

    // ── Allgemeine Vorlage mit eigener X-Funktion ─────────────────────────

    void test_template_customProjection()
    {
        struct Item { int id; double x; };
        const QList<Item> items { {10, 1.0}, {20, 5.0}, {30, 9.0} };

        const qsizetype index = ChartPointSearch::nearestIndex(
            items, 6.0, [](const Item& item) { return item.x; });

        QCOMPARE(index, qsizetype(1));
        QCOMPARE(items.at(index).id, 20);
    }
};

QTEST_APPLESS_MAIN(TestChartPointSearch)
#include "tst_chartpointsearch.moc"

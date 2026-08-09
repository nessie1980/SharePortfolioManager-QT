// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// tst_sharesplithint.cpp — Unit tests für ShareSplitHint.
//
// Phase 3b der Aktiensplit-Behandlung (09.08.2026), siehe ARCHITECTURE.md,
// "Split-Hinweis in den Editier-Dialogen". Zustandslos und datenbankfrei,
// gleiches Muster wie tst_sharesplitadjuster und tst_salefifoallocator.

#include <QtTest>
#include <QLocale>
#include <QUuid>

#include "../../app/models/ShareSplitObject.h"
#include "../../app/utils/ShareSplitHint.h"

namespace {

QDate d(int y, int m, int day) { return QDate(y, m, day); }

ShareSplitObject makeSplit(const QDate& date, double ratioNew, double ratioOld,
                           bool pricesAdjusted = false)
{
    return ShareSplitObject(QUuid::createUuid().toString(QUuid::WithoutBraces),
                            QStringLiteral("share-1"),
                            date, ratioNew, ratioOld, pricesAdjusted);
}

} // namespace

class TestShareSplitHint : public QObject
{
    Q_OBJECT

private slots:

    // ── hasSplitAfter ─────────────────────────────────────────────────────

    void test_hasSplitAfter_emptyList_false()
    {
        QVERIFY(!ShareSplitHint::hasSplitAfter({}, d(2021, 3, 18)));
    }

    void test_hasSplitAfter_splitLater_true()
    {
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 20.0, 1.0) };
        QVERIFY(ShareSplitHint::hasSplitAfter(splits, d(2021, 3, 18)));
    }

    void test_hasSplitAfter_splitEarlier_false()
    {
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 20.0, 1.0) };
        QVERIFY(!ShareSplitHint::hasSplitAfter(splits, d(2023, 2, 14)));
    }

    void test_hasSplitAfter_splitOnSameDay_false()
    {
        // Derselbe Massstab wie ShareSplitAdjuster::volumeFactor(): ein Split
        // AM Belegdatum wirkt sich auf diesen Beleg nicht mehr aus, er ist
        // bereits in heutiger Skala ausgestellt.
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 20.0, 1.0) };
        QVERIFY(!ShareSplitHint::hasSplitAfter(splits, d(2022, 7, 18)));
    }

    void test_hasSplitAfter_invalidDate_false()
    {
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 20.0, 1.0) };
        QVERIFY(!ShareSplitHint::hasSplitAfter(splits, QDate()));
    }

    // ── footerText: ohne Split ────────────────────────────────────────────

    void test_footerText_noSplits_mentionsCurrentState()
    {
        const QString text = ShareSplitHint::footerText({}, d(2023, 2, 14), 10.0, 52.40);

        // Der Text muss belegt sein — die Zeile ist immer sichtbar, damit das
        // Formular beim Tippen im Datumsfeld nicht springt.
        QVERIFY(!text.isEmpty());
        QVERIFY2(text.contains(QStringLiteral("Kein Split")), qPrintable(text));
    }

    void test_footerText_onlyEarlierSplits_mentionsCurrentState()
    {
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 20.0, 1.0) };
        const QString text = ShareSplitHint::footerText(splits, d(2023, 2, 14), 10.0, 52.40);

        QVERIFY2(text.contains(QStringLiteral("Kein Split")), qPrintable(text));
    }

    // ── footerText: mit Split ─────────────────────────────────────────────

    void test_footerText_singleSplit_showsRatioDateAndConversion()
    {
        // 5 Stück à 1.003,00 € vor einem 20:1-Split → 100 Stück à 50,15 €.
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 20.0, 1.0) };
        const QString text = ShareSplitHint::footerText(splits, d(2021, 3, 18), 5.0, 1003.00);

        QVERIFY2(text.contains(QStringLiteral("20:1")), qPrintable(text));
        QVERIFY2(text.contains(QLocale().toString(d(2022, 7, 18), QLocale::ShortFormat)),
                 qPrintable(text));
        QVERIFY2(text.contains(QLocale().toString(100.0, 'f', 4)), qPrintable(text));
        QVERIFY2(text.contains(QLocale().toString(50.15, 'f', 4)), qPrintable(text));
    }

    void test_footerText_singleSplit_productStaysEqual()
    {
        // Der eigentliche Zweck des Texts: Stückzahl mal Preis bleibt gleich.
        // Ein Split schafft weder Gewinn noch Verlust — sähe der Nutzer nur
        // die veränderte Stückzahl, wirkte das wie ein Fehler.
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 20.0, 1.0) };
        const QString text = ShareSplitHint::footerText(splits, d(2021, 3, 18), 5.0, 1003.00);

        const QLocale loc;
        QVERIFY(text.contains(loc.toString(100.0, 'f', 4)));   // 5 × 20
        QVERIFY(text.contains(loc.toString(50.15, 'f', 4)));   // 1003 / 20
        // 100 × 50,15 = 5.015,00 = 5 × 1.003,00
    }

    void test_footerText_reverseSplit_scalesDown()
    {
        // Reverse-Split 1:10 → aus 100 Stück à 5,00 € werden 10 Stück à 50,00 €.
        const QList<ShareSplitObject> splits = { makeSplit(d(2023, 5, 2), 1.0, 10.0) };
        const QString text = ShareSplitHint::footerText(splits, d(2021, 3, 18), 100.0, 5.00);

        const QLocale loc;
        QVERIFY2(text.contains(QStringLiteral("1:10")), qPrintable(text));
        QVERIFY2(text.contains(loc.toString(10.0, 'f', 4)), qPrintable(text));
        QVERIFY2(text.contains(loc.toString(50.0, 'f', 4)), qPrintable(text));
    }

    void test_footerText_multipleSplits_showsCountAndLatest()
    {
        const QList<ShareSplitObject> splits = {
            makeSplit(d(2018, 3, 1),  4.0,  1.0),
            makeSplit(d(2022, 7, 18), 20.0, 1.0),
        };
        const QString text = ShareSplitHint::footerText(splits, d(2015, 1, 1), 1.0, 800.00);

        QVERIFY2(text.contains(QStringLiteral("2 Splits")), qPrintable(text));
        QVERIFY2(text.contains(QStringLiteral("20:1")), qPrintable(text));
        // Kumuliert 4 × 20 = 80
        QVERIFY2(text.contains(QLocale().toString(80.0, 'f', 4)), qPrintable(text));
        QVERIFY2(text.contains(QLocale().toString(10.0, 'f', 4)), qPrintable(text));
    }

    void test_footerText_multipleSplits_onlyCountsThoseAfterTheDate()
    {
        // Ein Kauf zwischen den beiden Splits sieht nur den späteren.
        const QList<ShareSplitObject> splits = {
            makeSplit(d(2018, 3, 1),  4.0,  1.0),
            makeSplit(d(2022, 7, 18), 20.0, 1.0),
        };
        const QString text = ShareSplitHint::footerText(splits, d(2020, 1, 1), 5.0, 1000.00);

        QVERIFY2(!text.contains(QStringLiteral("2 Splits")), qPrintable(text));
        QVERIFY2(text.contains(QStringLiteral("20:1")), qPrintable(text));
        QVERIFY2(text.contains(QLocale().toString(100.0, 'f', 4)), qPrintable(text));
    }

    void test_footerText_unsortedSplits_stillNamesTheLatest()
    {
        // Die Liste kommt normalerweise sortiert aus dem Repository — der
        // Helfer darf sich darauf aber nicht verlassen, sonst nennt er bei
        // einem Aufrufer mit anderer Reihenfolge den falschen Splittag.
        const QList<ShareSplitObject> splits = {
            makeSplit(d(2022, 7, 18), 20.0, 1.0),
            makeSplit(d(2018, 3, 1),  4.0,  1.0),
        };
        const QString text = ShareSplitHint::footerText(splits, d(2015, 1, 1), 1.0, 800.00);

        QVERIFY2(text.contains(QStringLiteral("2 Splits")), qPrintable(text));
        QVERIFY2(text.contains(QLocale().toString(80.0, 'f', 4)), qPrintable(text));
    }

    void test_footerText_pricesAdjustedFlag_isIrrelevantHere()
    {
        // prices_adjusted betrifft ausschliesslich die Tageswert-Historie
        // (ShareSplitAdjuster::priceFactorForHistory). Belege liegen IMMER in
        // Beleg-Skala vor, das Kennzeichen darf den Hinweis nicht verändern.
        const QList<ShareSplitObject> a = { makeSplit(d(2022, 7, 18), 20.0, 1.0, false) };
        const QList<ShareSplitObject> b = { makeSplit(d(2022, 7, 18), 20.0, 1.0, true) };

        QCOMPARE(ShareSplitHint::footerText(a, d(2021, 3, 18), 5.0, 1003.00),
                 ShareSplitHint::footerText(b, d(2021, 3, 18), 5.0, 1003.00));
    }

    void test_footerText_zeroVolume_doesNotCrash()
    {
        // Neu geöffnetes Formular: Stückzahl und Preis stehen auf 0.
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 20.0, 1.0) };
        const QString text = ShareSplitHint::footerText(splits, d(2021, 3, 18), 0.0, 0.0);

        QVERIFY(!text.isEmpty());
        QVERIFY2(text.contains(QStringLiteral("20:1")), qPrintable(text));
    }

    void test_footerText_fractionalRatio_keepsDecimals()
    {
        // 3:2-Split — ganze Verhältnisse werden ohne Nachkommastellen
        // gezeigt, gebrochene dürfen aber nicht abgeschnitten werden.
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 3.0, 2.0) };
        const QString text = ShareSplitHint::footerText(splits, d(2021, 3, 18), 10.0, 60.00);

        QVERIFY2(text.contains(QStringLiteral("3:2")), qPrintable(text));
        QVERIFY2(text.contains(QLocale().toString(15.0, 'f', 4)), qPrintable(text));
        QVERIFY2(text.contains(QLocale().toString(40.0, 'f', 4)), qPrintable(text));
    }

    // ── tooltipText ───────────────────────────────────────────────────────

    void test_tooltipText_noSplits_isEmpty()
    {
        QVERIFY(ShareSplitHint::tooltipText({}, d(2021, 3, 18)).isEmpty());
    }

    void test_tooltipText_listsAllSplitsAfterTheDate()
    {
        const QList<ShareSplitObject> splits = {
            makeSplit(d(2018, 3, 1),  4.0,  1.0),
            makeSplit(d(2022, 7, 18), 20.0, 1.0),
        };
        const QString tip = ShareSplitHint::tooltipText(splits, d(2015, 1, 1));

        QVERIFY(tip.contains(QStringLiteral("4:1")));
        QVERIFY(tip.contains(QStringLiteral("20:1")));
        QCOMPARE(tip.count(QLatin1Char('\n')), 1); // zwei Zeilen
    }

    void test_tooltipText_skipsSplitsBeforeTheDate()
    {
        const QList<ShareSplitObject> splits = {
            makeSplit(d(2018, 3, 1),  4.0,  1.0),
            makeSplit(d(2022, 7, 18), 20.0, 1.0),
        };
        const QString tip = ShareSplitHint::tooltipText(splits, d(2020, 1, 1));

        QVERIFY2(!tip.contains(QStringLiteral("4:1")), qPrintable(tip));
        QVERIFY2(tip.contains(QStringLiteral("20:1")), qPrintable(tip));
    }

    // ── describeSplit / formatRatioPart ───────────────────────────────────

    void test_describeSplit_wholeRatioHasNoDecimals()
    {
        const QString text = ShareSplitHint::describeSplit(makeSplit(d(2022, 7, 18), 20.0, 1.0));
        QVERIFY2(text.startsWith(QStringLiteral("20:1")), qPrintable(text));
    }

    void test_describeSplit_reverseSplitKeepsOrder()
    {
        // Aus einer Zusammenlegung darf optisch keine Teilung werden.
        const QString text = ShareSplitHint::describeSplit(makeSplit(d(2023, 5, 2), 1.0, 10.0));
        QVERIFY2(text.startsWith(QStringLiteral("1:10")), qPrintable(text));
    }

    void test_formatRatioPart_fractionalKeepsTwoDecimals()
    {
        QCOMPARE(ShareSplitHint::formatRatioPart(1.5), QLocale().toString(1.5, 'f', 2));
    }

    void test_formatRatioPart_wholeNumberHasNoDecimals()
    {
        QCOMPARE(ShareSplitHint::formatRatioPart(20.0), QStringLiteral("20"));
    }
};

int main(int argc, char* argv[])
{
    // Kein QCoreApplication nötig — der Helfer ist zustandslos, greift nicht
    // auf Qt SQL zu und instanziiert keine Widgets (gleiche Bauweise wie
    // tst_sharesplitadjuster). QLocale::setDefault() wirkt auch ohne
    // Applikationsobjekt, wird hier aber gebraucht: der Helfer formatiert
    // über QLocale(), und CI-Runner laufen nicht mit deutschem Locale.
    QLocale::setDefault(QLocale::German);

    TestShareSplitHint t;
    return QTest::qExec(&t, argc, argv);
}

#include "tst_sharesplithint.moc"

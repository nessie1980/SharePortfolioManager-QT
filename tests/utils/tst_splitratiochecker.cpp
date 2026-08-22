// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// tst_splitratiochecker.cpp — Unit tests für SplitRatioChecker.
//
// Punkt 1 der Split-Plausibilitätsprüfung (22.08.2026), siehe ARCHITECTURE.md,
// "Plausibilitätsprüfung des Split-Verhältnisses". Zustandslos und
// datenbankfrei, gleiches Muster wie tst_sharesplitadjuster,
// tst_salefifoallocator und tst_dividendvolumechecker.
//
// Der Bezugsfall ist durchgängig der Alphabet-Feldfall: 10 Stück gekauft,
// Split am 18.07.2022 mit dem Bank-Zuteilungsverhältnis "1:19" fälschlich als
// 19:1 erfasst, Verkaufsbeleg über 200 Stück. Richtig wäre 20:1 gewesen.

#include <QtTest>
#include <QUuid>

#include "../../app/models/BuyObject.h"
#include "../../app/models/ShareSplitObject.h"
#include "../../app/utils/SplitRatioChecker.h"

namespace {

QDate d(int y, int m, int day) { return QDate(y, m, day); }

/// Kauf mit Restbestand; das Datum geht als ISO-8601-Zeitstempel hinein.
BuyObject makeBuy(const QString& guid, const QDate& date,
                  double volume, double volumeSold = 0.0, double price = 100.0)
{
    return BuyObject(guid, QStringLiteral("share-1"), QStringLiteral("depot1"),
                     QString(),
                     QDateTime(date, QTime(10, 0)).toString(Qt::ISODate),
                     volume, volumeSold, price);
}

ShareSplitObject makeSplit(const QDate& date, double ratioNew, double ratioOld)
{
    return ShareSplitObject(QUuid::createUuid().toString(QUuid::WithoutBraces),
                            QStringLiteral("share-1"),
                            date, ratioNew, ratioOld);
}

} // namespace

class TestSplitRatioChecker : public QObject
{
    Q_OBJECT

private slots:

    // ── Kein Verdacht ─────────────────────────────────────────────────────

    void test_diagnose_noSplits_noSuspicion()
    {
        // Ohne Split ist die Unterdeckung schlicht eine zu hohe Menge.
        const QList<BuyObject> buys = { makeBuy("b1", d(2021, 3, 18), 10.0) };

        const auto s = SplitRatioChecker::diagnose(200.0, d(2022, 12, 5), buys, {});

        QVERIFY(!s.hasSuspicion);
        QVERIFY(!s.hasProposal);
        QVERIFY(s.splitsBetween.isEmpty());
    }

    void test_diagnose_noOpenBuys_noSuspicion()
    {
        // Ein vollständig verkaufter Kauf trägt nichts bei — ohne offenen
        // Kauf gibt es keinen Bezugspunkt für eine Rückrechnung.
        const QList<BuyObject>        buys   = { makeBuy("b1", d(2021, 3, 18), 10.0, 10.0) };
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 19.0, 1.0) };

        const auto s = SplitRatioChecker::diagnose(200.0, d(2022, 12, 5), buys, splits);

        QVERIFY(!s.hasSuspicion);
    }

    void test_diagnose_splitBeforeAllBuys_noSuspicion()
    {
        // Der Split liegt VOR dem Kauf: der Kauf ist bereits in der neuen
        // Stückelung ausgestellt, das Verhältnis kann die Unterdeckung
        // nicht erklären.
        const QList<BuyObject>        buys   = { makeBuy("b1", d(2023, 1, 10), 10.0) };
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 19.0, 1.0) };

        const auto s = SplitRatioChecker::diagnose(200.0, d(2023, 6, 1), buys, splits);

        QVERIFY(!s.hasSuspicion);
    }

    void test_diagnose_splitOnBuyDay_noSuspicion()
    {
        // Derselbe Massstab wie ShareSplitAdjuster::volumeFactor(): ein Split
        // AM Kauftag wirkt sich auf diesen Kauf nicht mehr aus.
        const QList<BuyObject>        buys   = { makeBuy("b1", d(2022, 7, 18), 10.0) };
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 19.0, 1.0) };

        const auto s = SplitRatioChecker::diagnose(200.0, d(2022, 12, 5), buys, splits);

        QVERIFY(!s.hasSuspicion);
    }

    void test_diagnose_splitAfterReferenceDate_noSuspicion()
    {
        // Ein Split NACH dem Verkauf skaliert Kauf und Verkaufsmenge
        // gleichermassen auf heutige Skala — er kürzt sich im Vergleich
        // heraus und darf nicht als Ursache genannt werden.
        const QList<BuyObject>        buys   = { makeBuy("b1", d(2021, 3, 18), 10.0) };
        const QList<ShareSplitObject> splits = { makeSplit(d(2023, 7, 18), 19.0, 1.0) };

        const auto s = SplitRatioChecker::diagnose(200.0, d(2022, 12, 5), buys, splits);

        QVERIFY(!s.hasSuspicion);
    }

    void test_diagnose_invalidReferenceDate_noSuspicion()
    {
        const QList<BuyObject>        buys   = { makeBuy("b1", d(2021, 3, 18), 10.0) };
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 19.0, 1.0) };

        const auto s = SplitRatioChecker::diagnose(200.0, QDate(), buys, splits);

        QVERIFY(!s.hasSuspicion);
    }

    // ── Feldfall: Verdacht MIT Vorschlag ──────────────────────────────────

    void test_diagnose_fieldCase_proposesRatioTwentyToOne()
    {
        // 10 Stück gekauft, als 19:1 erfasst -> 190 heute; Verkauf über 200.
        const QList<BuyObject>        buys   = { makeBuy("b1", d(2021, 3, 18), 10.0) };
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 19.0, 1.0) };

        const auto s = SplitRatioChecker::diagnose(200.0, d(2022, 12, 5), buys, splits);

        QVERIFY(s.hasSuspicion);
        QVERIFY(s.hasProposal);
        QCOMPARE(s.proposedRatioNew, 20.0);
        QCOMPARE(s.proposedRatioOld, 1.0);
        QCOMPARE(s.splitsBetween.size(), 1);
    }

    void test_diagnose_fieldCase_proposedVolumeMatchesRequested()
    {
        // Die genannte Stückzahl muss die angeforderte exakt treffen — nur
        // dann trägt die Aussage "die Rechnung ginge damit auf".
        const QList<BuyObject>        buys   = { makeBuy("b1", d(2021, 3, 18), 10.0) };
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 19.0, 1.0) };

        const auto s = SplitRatioChecker::diagnose(200.0, d(2022, 12, 5), buys, splits);

        QVERIFY(s.hasProposal);
        QVERIFY(qAbs(s.proposedAvailableToday - 200.0) < 1e-6);
    }

    void test_diagnose_correctRatio_stillSuspicionButNoProposal()
    {
        // Verhältnis 20:1 ist richtig, verkauft werden sollen 300 statt 200 —
        // also ein echter Mengenfehler. Der Split wird genannt (er liegt
        // dazwischen), aber ein Verhältnis darf NICHT vorgeschlagen werden.
        const QList<BuyObject>        buys   = { makeBuy("b1", d(2021, 3, 18), 10.0) };
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 20.0, 1.0) };

        const auto s = SplitRatioChecker::diagnose(300.0, d(2022, 12, 5), buys, splits);

        QVERIFY(s.hasSuspicion);
        QVERIFY(!s.hasProposal);
    }

    void test_diagnose_typoInVolume_doesNotProposeAbsurdRatio()
    {
        // Der wichtigste Test dieser Datei: 2.000 statt 200 getippt. Die
        // Rückrechnung liefert ein formal sauberes Verhältnis (190:1), das
        // aber vollkommen irreführend wäre. Die enge Regel (genau eins mehr
        // als eingetragen) muss das abfangen.
        const QList<BuyObject>        buys   = { makeBuy("b1", d(2021, 3, 18), 10.0) };
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 19.0, 1.0) };

        const auto s = SplitRatioChecker::diagnose(2000.0, d(2022, 12, 5), buys, splits);

        QVERIFY(s.hasSuspicion);
        QVERIFY2(!s.hasProposal,
                 "Rückrechnung 190:1 darf nicht als Verhältnis vorgeschlagen werden");
    }

    void test_diagnose_partiallySoldBuy_usesRemainingVolume()
    {
        // 12 gekauft, 2 bereits verkauft -> 10 offen, danach derselbe
        // Feldfall. Gerechnet wird mit dem Restbestand, nicht mit volume().
        const QList<BuyObject>        buys   = { makeBuy("b1", d(2021, 3, 18), 12.0, 2.0) };
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 19.0, 1.0) };

        const auto s = SplitRatioChecker::diagnose(200.0, d(2022, 12, 5), buys, splits);

        QVERIFY(s.hasProposal);
        QCOMPARE(s.proposedRatioNew, 20.0);
    }

    void test_diagnose_buyAfterSplit_countedWithoutFactor()
    {
        // Kauf 10 vor dem Split (19:1 -> 190) plus Kauf 50 danach (bleibt 50)
        // ergibt 240 verfügbar. Angefordert 250: das fehlende Stück liegt
        // allein im Faktor, richtig wäre 20:1 (200 + 50 = 250).
        const QList<BuyObject> buys = {
            makeBuy("b1", d(2021, 3, 18), 10.0),
            makeBuy("b2", d(2023, 1, 10), 50.0),
        };
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 19.0, 1.0) };

        const auto s = SplitRatioChecker::diagnose(250.0, d(2023, 6, 1), buys, splits);

        QVERIFY(s.hasProposal);
        QCOMPARE(s.proposedRatioNew, 20.0);
        QVERIFY(qAbs(s.proposedAvailableToday - 250.0) < 1e-6);
    }

    // ── Verdacht OHNE Vorschlag ───────────────────────────────────────────

    void test_diagnose_twoSplitsBetween_namesThemWithoutProposal()
    {
        // Bei mehreren Splits ist nicht zuzuordnen, welcher gemeint wäre.
        const QList<BuyObject> buys = { makeBuy("b1", d(2020, 1, 15), 10.0) };
        const QList<ShareSplitObject> splits = {
            makeSplit(d(2022, 7, 18), 19.0, 1.0),
            makeSplit(d(2021, 1,  4),  2.0, 1.0),
        };

        const auto s = SplitRatioChecker::diagnose(400.0, d(2023, 6, 1), buys, splits);

        QVERIFY(s.hasSuspicion);
        QVERIFY(!s.hasProposal);
        QCOMPARE(s.splitsBetween.size(), 2);
    }

    void test_diagnose_splitsBetween_sortedByDateAscending()
    {
        // Der Meldungstext nennt bei mehreren Splits den jüngsten ("zuletzt
        // …") — die Sortierung ist deshalb Teil der Zusage, nicht Zufall.
        const QList<BuyObject> buys = { makeBuy("b1", d(2020, 1, 15), 10.0) };
        const QList<ShareSplitObject> splits = {
            makeSplit(d(2022, 7, 18), 19.0, 1.0),
            makeSplit(d(2021, 1,  4),  2.0, 1.0),
        };

        const auto s = SplitRatioChecker::diagnose(400.0, d(2023, 6, 1), buys, splits);

        QCOMPARE(s.splitsBetween.first().date(), d(2021, 1, 4));
        QCOMPARE(s.splitsBetween.constLast().date(), d(2022, 7, 18));
    }

    void test_diagnose_reverseSplit_namesItWithoutProposal()
    {
        // Reverse-Split 1:10 — die alte Seite ist hier 10, nicht 1. Die
        // Delta-vs-Gesamt-Verwechslung der Bankmitteilung bezieht sich immer
        // auf EIN gehaltenes Stück und gibt es bei einem Reverse-Split
        // ohnehin nicht; ein Vorschlag darf also nicht entstehen.
        const QList<BuyObject>        buys   = { makeBuy("b1", d(2021, 3, 18), 1000.0) };
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 1.0, 10.0) };

        const auto s = SplitRatioChecker::diagnose(150.0, d(2022, 12, 5), buys, splits);

        QVERIFY(s.hasSuspicion);
        QVERIFY(!s.hasProposal);
    }

    void test_diagnose_ratioOldNotOne_namesItWithoutProposal()
    {
        // Verhältnis 19:2. Die Bank-Notation "1:19" bezieht sich immer auf
        // EIN gehaltenes Stück — bei einer anderen alten Seite greift die
        // Erklärung nicht, es bleibt bei der blossen Nennung.
        const QList<BuyObject>        buys   = { makeBuy("b1", d(2021, 3, 18), 20.0) };
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 19.0, 2.0) };

        const auto s = SplitRatioChecker::diagnose(200.0, d(2022, 12, 5), buys, splits);

        QVERIFY(s.hasSuspicion);
        QVERIFY(!s.hasProposal);
    }

    void test_diagnose_reverseSplitAsFraction_namesItWithoutProposal()
    {
        // Derselbe Reverse-Split, diesmal als 0,1:1 statt 1:10 geschrieben —
        // die alte Seite ist hier 1, es greift also erst die Prüfung auf die
        // neue Seite. Ein Vorschlag darf trotzdem nicht entstehen.
        const QList<BuyObject>        buys   = { makeBuy("b1", d(2021, 3, 18), 1000.0) };
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 0.1, 1.0) };

        const auto s = SplitRatioChecker::diagnose(150.0, d(2022, 12, 5), buys, splits);

        QVERIFY(s.hasSuspicion);
        QVERIFY(!s.hasProposal);
    }

    void test_diagnose_soldOutBuyBeforeSplit_isNotTheReference()
    {
        // Der einzige Kauf VOR dem Split ist vollständig verkauft; der
        // älteste OFFENE Kauf liegt am Splittag selbst. Damit gibt es keinen
        // Bestandsanteil, der den fraglichen Faktor trägt — der Split kommt
        // als Ursache nicht in Frage und darf nicht genannt werden.
        const QList<BuyObject> buys = {
            makeBuy("leer", d(2021, 3, 18), 10.0, 10.0),   // vollständig verkauft
            makeBuy("b2",   d(2022, 7, 18), 50.0),
        };
        const QList<ShareSplitObject> splits = { makeSplit(d(2022, 7, 18), 19.0, 1.0) };

        const auto s = SplitRatioChecker::diagnose(80.0, d(2023, 1, 10), buys, splits);

        QVERIFY(!s.hasSuspicion);
        QVERIFY(!s.hasProposal);
    }
};

QTEST_MAIN(TestSplitRatioChecker)
#include "tst_splitratiochecker.moc"

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// Unit tests for SplitAudit — stateless, database-free comparison
// of a split's stored prices_adjusted() flag against what
// SplitPriceJumpDetector reads out of the current price history. Phase 4 of
// the Aktiensplit-Behandlung (see ARCHITECTURE.md, "Offene Punkte",
// "Aktiensplits werden nicht behandelt").
//
// Erweitert 22.08.2026 (Punkt 4 der Split-Plausibilitaetspruefung, Klasse von
// SplitAdjustmentAudit umbenannt): zusaetzlich zum prices_adjusted-Zustand
// werden jetzt zwei Verhaeltnis-Pruefungen gemeldet — gegen den gemessenen
// Kurssprung und gegen die Verkaufshistorie. Fixture values loosely mirror the
// Alphabet Inc. Cl. A case documented there: 20:1 split, ex-date 18.07.2022.
#include <QtTest>

#include "../../app/utils/SplitAudit.h"
#include <QUuid>

namespace {
QDate d(int y, int m, int day) { return QDate(y, m, day); }

DailyValuesObject dv(const QDate& date, double closing)
{
    return DailyValuesObject(QStringLiteral("share-guid"), date,
                             /*opening=*/closing, closing,
                             /*top=*/closing, /*bottom=*/closing, /*volume=*/0.0);
}

ShareSplitObject split(const QString& guid, const QDate& date,
                      double ratioNew, double ratioOld, bool pricesAdjusted)
{
    return ShareSplitObject(guid, QStringLiteral("share-guid"), date,
                            ratioNew, ratioOld, pricesAdjusted);
}

/// Kauf fuer die Bestandspruefung.
BuyObject buy(const QDate& date, double volume)
{
    return BuyObject(QUuid::createUuid().toString(QUuid::WithoutBraces),
                     QStringLiteral("share-guid"), QStringLiteral("depot1"), QString(),
                     QDateTime(date, QTime(10, 0)).toString(Qt::ISODate),
                     volume, 0.0, 100.0);
}

/// Verkauf fuer die Bestandspruefung.
SaleObject sale(const QDate& date, double volume)
{
    return SaleObject(QUuid::createUuid().toString(QUuid::WithoutBraces),
                      QStringLiteral("share-guid"), QStringLiteral("depot1"), QString(),
                      QDateTime(date, QTime(10, 0)).toString(Qt::ISODate),
                      volume, 50.0, {});
}

/// Kurse des Feldfalls: klarer Sprung 1003,00 -> 50,20 um den 18.07.2022.
QList<DailyValuesObject> fieldCasePrices()
{
    return { dv(d(2022, 7, 18), 1003.0), dv(d(2022, 7, 19), 50.20) };
}
}

class TestSplitAudit : public QObject
{
    Q_OBJECT

private slots:

    // ── Kein Split, keine Kurse ───────────────────────────────────────────────

    void test_check_noSplits_returnsEmpty()
    {
        const QList<DailyValuesObject> values = { dv(d(2022, 7, 18), 1003.0),
                                                   dv(d(2022, 7, 19),   50.2) };
        QVERIFY(SplitAudit::check({}, values).isEmpty());
    }

    // ── Widerspruch: gespeichert unbereinigt, erkannt bereinigt ──────────────

    void test_check_storedNotAdjusted_detectedAdjusted_reportsDiscrepancy()
    {
        // Kein Kurssprung um den Ex-Tag — Historie wirkt bereits bereinigt —
        // aber der Split ist als unbereinigt markiert.
        const QList<ShareSplitObject> splits = {
            split(QStringLiteral("s1"), d(2022, 7, 18), 20.0, 1.0, /*pricesAdjusted=*/false)
        };
        const QList<DailyValuesObject> values = {
            dv(d(2022, 7, 15), 49.80),
            dv(d(2022, 7, 18), 50.20),
            dv(d(2022, 7, 19), 50.60),
        };

        const auto found = SplitAudit::check(splits, values);

        QCOMPARE(found.size(), 1);
        QCOMPARE(found.first().split.guid(), QStringLiteral("s1"));
        QCOMPARE(found.first().outcome.result, SplitPriceJumpDetector::Result::Adjusted);
    }

    // ── Widerspruch: gespeichert bereinigt, erkannt unbereinigt ──────────────

    void test_check_storedAdjusted_detectedNotAdjusted_reportsDiscrepancy()
    {
        // Deutlicher Kurssprung um den Faktor 20 — Historie wirkt UNbereinigt
        // — aber der Split ist als bereits bereinigt markiert.
        const QList<ShareSplitObject> splits = {
            split(QStringLiteral("s1"), d(2022, 7, 18), 20.0, 1.0, /*pricesAdjusted=*/true)
        };
        const QList<DailyValuesObject> values = {
            dv(d(2022, 7, 18), 1003.0),
            dv(d(2022, 7, 19),   50.2),
        };

        const auto found = SplitAudit::check(splits, values);

        QCOMPARE(found.size(), 1);
        QCOMPARE(found.first().outcome.result, SplitPriceJumpDetector::Result::NotAdjusted);
    }

    // ── Kein Widerspruch: gespeicherter Zustand passt zur Historie ───────────

    void test_check_storedNotAdjusted_detectedNotAdjusted_noDiscrepancy()
    {
        const QList<ShareSplitObject> splits = {
            split(QStringLiteral("s1"), d(2022, 7, 18), 20.0, 1.0, /*pricesAdjusted=*/false)
        };
        const QList<DailyValuesObject> values = {
            dv(d(2022, 7, 18), 1003.0),
            dv(d(2022, 7, 19),   50.2),
        };
        QVERIFY(SplitAudit::check(splits, values).isEmpty());
    }

    void test_check_storedAdjusted_detectedAdjusted_noDiscrepancy()
    {
        const QList<ShareSplitObject> splits = {
            split(QStringLiteral("s1"), d(2022, 7, 18), 20.0, 1.0, /*pricesAdjusted=*/true)
        };
        const QList<DailyValuesObject> values = {
            dv(d(2022, 7, 15), 49.80),
            dv(d(2022, 7, 18), 50.20),
            dv(d(2022, 7, 19), 50.60),
        };
        QVERIFY(SplitAudit::check(splits, values).isEmpty());
    }

    // ── Uneindeutige/fehlende Daten: nie ein Widerspruch ──────────────────────
    // Vorsicht vor falschem Alarm ist wichtiger als Vollstaendigkeit — ein
    // Verdachtsfall, den der Nutzer nicht aufloesen koennte, soll nicht als
    // Widerspruch erscheinen.

    void test_check_ambiguousResult_neverReportsAdjustmentFlag()
    {
        // Verhaeltnis 5.0 liegt weder nah bei 1.0 noch nah bei Faktor 20 (siehe
        // SplitPriceJumpDetector-Tests): die Haken-Frage bleibt unbeantwortet,
        // und bei beiden gespeicherten Zustaenden darf daraus kein
        // AdjustmentFlag-Befund werden.
        //
        // Das Ergebnis ist deshalb seit Punkt 4 NICHT mehr leer, und der Test
        // prueft entsprechend die Befundart statt die Anzahl: derselbe Sprung
        // misst sauber 5:1 und widerspricht damit den eingetragenen 20:1 — ein
        // RatioFromPrices-Befund. Das sind zwei verschiedene Fragen, und dass
        // die eine unbeantwortbar ist, macht die andere nicht ungueltig (siehe
        // test_detect_ambiguousResult_canStillReportMismatch in
        // tst_splitpricejumpdetector).
        const QList<DailyValuesObject> values = {
            dv(d(2022, 7, 18), 100.0),
            dv(d(2022, 7, 19),  20.0),
        };
        for (bool stored : { false, true }) {
            const QList<ShareSplitObject> splits = {
                split(QStringLiteral("s1"), d(2022, 7, 18), 20.0, 1.0, stored)
            };

            const auto found = SplitAudit::check(splits, values);

            for (const SplitAudit::Discrepancy& f : found) {
                QVERIFY2(f.kind != SplitAudit::Kind::AdjustmentFlag,
                         "Uneindeutiges Detektor-Ergebnis darf den gespeicherten "
                         "Bereinigungs-Zustand nie als Widerspruch melden");
            }
        }
    }

    void test_check_insufficientData_neverReportsDiscrepancy()
    {
        for (bool stored : { false, true }) {
            const QList<ShareSplitObject> splits = {
                split(QStringLiteral("s1"), d(2022, 7, 18), 20.0, 1.0, stored)
            };
            QVERIFY(SplitAudit::check(splits, {}).isEmpty());
        }
    }

    // ── Mehrere Splits: nur der widersprechende landet im Ergebnis ───────────

    void test_check_multipleSplits_onlyContradictingOneReported()
    {
        const QList<ShareSplitObject> splits = {
            // s1: passt zur Historie (Sprung erkannt, als unbereinigt markiert).
            split(QStringLiteral("s1"), d(2021, 1, 4), 2.0, 1.0, /*pricesAdjusted=*/false),
            // s2: widerspricht (kein Sprung erkannt, aber als unbereinigt markiert).
            split(QStringLiteral("s2"), d(2022, 7, 18), 20.0, 1.0, /*pricesAdjusted=*/false),
        };
        const QList<DailyValuesObject> values = {
            // Sprung um Faktor 2 rund um s1.
            dv(d(2021, 1, 4),  200.0),
            dv(d(2021, 1, 5),  100.0),
            // Kein Sprung rund um s2.
            dv(d(2022, 7, 15), 49.80),
            dv(d(2022, 7, 18), 50.20),
            dv(d(2022, 7, 19), 50.60),
        };

        const auto found = SplitAudit::check(splits, values);

        QCOMPARE(found.size(), 1);
        QCOMPARE(found.first().split.guid(), QStringLiteral("s2"));
    }

    // ── Nachbar-Splits begrenzen das Suchfenster je geprüftem Split ──────────
    // Dieselbe Logik wie PresenterShareSplitEdit::onCheckPriceJump() — der
    // geprüfte Split selbst zählt nicht als eigener Nachbar.

    void test_check_neighborSplit_boundsWindow_perSplit()
    {
        // Zwei Splits: 10.07. und 18.07.2022. Ein Kurs am 05.07. liegt VOR dem
        // Nachbarn (10.07.) und darf beim Pruefen von 18.07. nicht mitzaehlen —
        // ohne die Nachbar-Begrenzung waere er der naechste "davor"-Kurs und
        // taeuschte eine (falsche) Einordnung vor.
        const QList<ShareSplitObject> splits = {
            split(QStringLiteral("s1"), d(2022, 7, 10), 2.0,  1.0, /*pricesAdjusted=*/true),
            split(QStringLiteral("s2"), d(2022, 7, 18), 20.0, 1.0, /*pricesAdjusted=*/false),
        };
        const QList<DailyValuesObject> values = {
            dv(d(2022, 7, 5),  9999.0), // ausserhalb des Fensters von s2, siehe oben
            dv(d(2022, 7, 19),   50.2),
        };

        const auto found = SplitAudit::check(splits, values);

        // s2 hat im begrenzten Fenster keinen Kurs "davor" -> InsufficientData
        // -> kein Widerspruch. s1 findet zwar den Kurs vom 05.07. als "davor",
        // aber keinen Kurs "danach" (das naechste Datum, 19.07., liegt hinter
        // dem Nachbarn s2 und faellt damit aus s1s Fenster) -> ebenfalls
        // InsufficientData -> kein Widerspruch. Ohne die Nachbar-Begrenzung
        // wuerde s2 faelschlich den Kurs vom 05.07. als "davor" verwenden.
        QVERIFY(found.isEmpty());
    }

    // ── Reihenfolge folgt der Eingabe ─────────────────────────────────────────

    // ── Verhaeltnis gegen den gemessenen Kurssprung (Punkt 3/4) ─────────────
    //
    // Kostet nichts: detect() rechnet die Gegenprobe ohnehin mit, das
    // Ergebnis wurde bis dahin nur weggeworfen.

    void test_check_ratioMismatchFromPrices_reportsFinding()
    {
        // 19:1 eingetragen, gemessen ~19,98. Der prices_adjusted-Zustand
        // passt (Sprung erkannt, als unbereinigt markiert) — es darf also
        // GENAU EIN Befund entstehen, und zwar der ueber das Verhaeltnis.
        const QList<ShareSplitObject> splits = {
            split(QStringLiteral("s1"), d(2022, 7, 18), 19.0, 1.0, /*pricesAdjusted=*/false)
        };

        const auto found = SplitAudit::check(splits, fieldCasePrices());

        QCOMPARE(found.size(), 1);
        QCOMPARE(found.first().kind, SplitAudit::Kind::RatioFromPrices);
        QCOMPARE(found.first().outcome.impliedFactor, 20.0);
    }

    void test_check_correctRatio_noRatioFinding()
    {
        const QList<ShareSplitObject> splits = {
            split(QStringLiteral("s1"), d(2022, 7, 18), 20.0, 1.0, /*pricesAdjusted=*/false)
        };
        QVERIFY(SplitAudit::check(splits, fieldCasePrices()).isEmpty());
    }

    void test_check_flagAndRatio_reportedSeparately()
    {
        // Beides trifft zu: 19:1 ist falsch UND der Split ist faelschlich als
        // bereinigt markiert, obwohl ein Sprung messbar ist. Die zwei Befunde
        // sagen Verschiedenes und muessen einzeln erscheinen.
        const QList<ShareSplitObject> splits = {
            split(QStringLiteral("s1"), d(2022, 7, 18), 19.0, 1.0, /*pricesAdjusted=*/true)
        };

        const auto found = SplitAudit::check(splits, fieldCasePrices());

        QCOMPARE(found.size(), 2);
        QCOMPARE(found.at(0).kind, SplitAudit::Kind::AdjustmentFlag);
        QCOMPARE(found.at(1).kind, SplitAudit::Kind::RatioFromPrices);
    }

    void test_check_flagDiscrepancy_hasAdjustmentFlagKind()
    {
        // Rueckwaertskompatibilitaet der Aggregat-Initialisierung: die
        // bisherigen Befunde muessen weiterhin als AdjustmentFlag kommen.
        const QList<ShareSplitObject> splits = {
            split(QStringLiteral("s1"), d(2022, 7, 18), 20.0, 1.0, /*pricesAdjusted=*/false)
        };
        const QList<DailyValuesObject> values = {
            dv(d(2022, 7, 18), 50.20), dv(d(2022, 7, 19), 50.60),
        };

        const auto found = SplitAudit::check(splits, values);

        QCOMPARE(found.size(), 1);
        QCOMPARE(found.first().kind, SplitAudit::Kind::AdjustmentFlag);
    }

    // ── Verhaeltnis gegen die Verkaufshistorie (Punkt 4) ─────────────────────

    void test_check_holdingsConflict_reportsFinding()
    {
        // Feldfall in der Datenbank: 10 gekauft, 19:1 eingetragen (-> 190),
        // Verkaufsbeleg ueber 200. Kurse absichtlich weggelassen, damit nur
        // der Bestandsbefund uebrig bleibt.
        const QList<ShareSplitObject> splits = {
            split(QStringLiteral("s1"), d(2022, 7, 18), 19.0, 1.0, /*pricesAdjusted=*/false)
        };

        const auto found = SplitAudit::check(splits, {},
                                             { buy(d(2021, 3, 18), 10.0) },
                                             { sale(d(2022, 12, 5), 200.0) });

        QCOMPARE(found.size(), 1);
        QCOMPARE(found.first().kind, SplitAudit::Kind::RatioFromHoldings);
        QCOMPARE(found.first().split.guid(), QStringLiteral("s1"));
        QVERIFY(found.first().conflict.hasConflict);
        QCOMPARE(found.first().conflict.suspicion.proposedRatioNew, 20.0);
    }

    void test_check_holdingsConflict_withoutProposal_notReported()
    {
        // Verhaeltnis 20:1 stimmt, verkauft sind 777 — etwa weil die
        // Kaufhistorie unvollstaendig ist. Beim Programmstart erscheint ein
        // modaler Dialog, den niemand abstellen kann; ein Befund ohne
        // konkreten Korrekturvorschlag hat hier nichts zu suchen.
        const QList<ShareSplitObject> splits = {
            split(QStringLiteral("s1"), d(2022, 7, 18), 20.0, 1.0, /*pricesAdjusted=*/false)
        };

        const auto found = SplitAudit::check(splits, {},
                                             { buy(d(2021, 3, 18), 10.0) },
                                             { sale(d(2022, 12, 5), 777.0) });

        QVERIFY(found.isEmpty());
    }

    void test_check_noBuysOrSales_skipsHoldingsCheck()
    {
        const QList<ShareSplitObject> splits = {
            split(QStringLiteral("s1"), d(2022, 7, 18), 19.0, 1.0, /*pricesAdjusted=*/false)
        };

        QVERIFY(SplitAudit::check(splits, {}).isEmpty());
        QVERIFY(SplitAudit::check(splits, {}, { buy(d(2021, 3, 18), 10.0) }, {}).isEmpty());
        QVERIFY(SplitAudit::check(splits, {}, {}, { sale(d(2022, 12, 5), 200.0) }).isEmpty());
    }

    void test_check_holdingsConflict_reportedOncePerShare()
    {
        // Zwei Verkaeufe, die beide nicht gedeckt waeren. Die Pruefung laeuft
        // einmal je Aktie und meldet die frueheste Fundstelle — nicht je
        // Verkauf und nicht je Split.
        const QList<ShareSplitObject> splits = {
            split(QStringLiteral("s1"), d(2022, 7, 18), 19.0, 1.0, /*pricesAdjusted=*/false)
        };

        const auto found = SplitAudit::check(splits, {},
                                             { buy(d(2021, 3, 18), 10.0) },
                                             { sale(d(2022, 12, 5), 200.0),
                                               sale(d(2023,  1, 5), 200.0) });

        QCOMPARE(found.size(), 1);
        QCOMPARE(found.first().conflict.conflictDate, d(2022, 12, 5));
    }

    void test_check_twoSplitsBetween_holdingsConflictNotAttributable()
    {
        // Liegen zwei Splits zwischen Kauf und Verkauf, ist nicht zuzuordnen,
        // welcher gemeint waere — dann kein Befund.
        const QList<ShareSplitObject> splits = {
            split(QStringLiteral("s1"), d(2021, 1,  4),  2.0, 1.0, /*pricesAdjusted=*/false),
            split(QStringLiteral("s2"), d(2022, 7, 18), 19.0, 1.0, /*pricesAdjusted=*/false),
        };

        const auto found = SplitAudit::check(splits, {},
                                             { buy(d(2020, 1, 15), 10.0) },
                                             { sale(d(2023, 6, 1), 400.0) });

        QVERIFY(found.isEmpty());
    }

    void test_check_resultOrder_matchesInputOrder()
    {
        const QList<ShareSplitObject> splits = {
            split(QStringLiteral("s-later"),  d(2022, 7, 18), 20.0, 1.0, false),
            split(QStringLiteral("s-earlier"), d(2021, 1, 4), 2.0,  1.0, false),
        };
        const QList<DailyValuesObject> values = {
            dv(d(2021, 1, 4),   99.9), // kein Sprung -> Widerspruch (unbereinigt markiert)
            dv(d(2021, 1, 5),  100.1),
            dv(d(2022, 7, 18),  49.9), // kein Sprung -> Widerspruch
            dv(d(2022, 7, 19),  50.1),
        };

        const auto found = SplitAudit::check(splits, values);

        QCOMPARE(found.size(), 2);
        QCOMPARE(found.at(0).split.guid(), QStringLiteral("s-later"));
        QCOMPARE(found.at(1).split.guid(), QStringLiteral("s-earlier"));
    }
};

QTEST_MAIN(TestSplitAudit)
#include "tst_splitaudit.moc"

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// Unit tests for SplitAdjustmentAudit — stateless, database-free comparison
// of a split's stored prices_adjusted() flag against what
// SplitPriceJumpDetector reads out of the current price history. Phase 4 of
// the Aktiensplit-Behandlung (see ARCHITECTURE.md, "Offene Punkte",
// "Aktiensplits werden nicht behandelt"). Fixture values loosely mirror the
// Alphabet Inc. Cl. A case documented there: 20:1 split, ex-date 18.07.2022.
#include <QtTest>

#include "../../app/utils/SplitAdjustmentAudit.h"

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
}

class TestSplitAdjustmentAudit : public QObject
{
    Q_OBJECT

private slots:

    // ── Kein Split, keine Kurse ───────────────────────────────────────────────

    void test_check_noSplits_returnsEmpty()
    {
        const QList<DailyValuesObject> values = { dv(d(2022, 7, 18), 1003.0),
                                                   dv(d(2022, 7, 19),   50.2) };
        QVERIFY(SplitAdjustmentAudit::check({}, values).isEmpty());
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

        const auto found = SplitAdjustmentAudit::check(splits, values);

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

        const auto found = SplitAdjustmentAudit::check(splits, values);

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
        QVERIFY(SplitAdjustmentAudit::check(splits, values).isEmpty());
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
        QVERIFY(SplitAdjustmentAudit::check(splits, values).isEmpty());
    }

    // ── Uneindeutige/fehlende Daten: nie ein Widerspruch ──────────────────────
    // Vorsicht vor falschem Alarm ist wichtiger als Vollstaendigkeit — ein
    // Verdachtsfall, den der Nutzer nicht aufloesen koennte, soll nicht als
    // Widerspruch erscheinen.

    void test_check_ambiguousResult_neverReportsDiscrepancy()
    {
        // Verhaeltnis 5.0 liegt weder nah bei 1.0 noch nah bei Faktor 20 (siehe
        // SplitPriceJumpDetector-Tests) — bei beiden gespeicherten Zustaenden
        // darf das kein Widerspruch sein.
        const QList<DailyValuesObject> values = {
            dv(d(2022, 7, 18), 100.0),
            dv(d(2022, 7, 19),  20.0),
        };
        for (bool stored : { false, true }) {
            const QList<ShareSplitObject> splits = {
                split(QStringLiteral("s1"), d(2022, 7, 18), 20.0, 1.0, stored)
            };
            QVERIFY(SplitAdjustmentAudit::check(splits, values).isEmpty());
        }
    }

    void test_check_insufficientData_neverReportsDiscrepancy()
    {
        for (bool stored : { false, true }) {
            const QList<ShareSplitObject> splits = {
                split(QStringLiteral("s1"), d(2022, 7, 18), 20.0, 1.0, stored)
            };
            QVERIFY(SplitAdjustmentAudit::check(splits, {}).isEmpty());
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

        const auto found = SplitAdjustmentAudit::check(splits, values);

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

        const auto found = SplitAdjustmentAudit::check(splits, values);

        // s2 hat im begrenzten Fenster keinen Kurs "davor" -> InsufficientData
        // -> kein Widerspruch. s1 findet zwar den Kurs vom 05.07. als "davor",
        // aber keinen Kurs "danach" (das naechste Datum, 19.07., liegt hinter
        // dem Nachbarn s2 und faellt damit aus s1s Fenster) -> ebenfalls
        // InsufficientData -> kein Widerspruch. Ohne die Nachbar-Begrenzung
        // wuerde s2 faelschlich den Kurs vom 05.07. als "davor" verwenden.
        QVERIFY(found.isEmpty());
    }

    // ── Reihenfolge folgt der Eingabe ─────────────────────────────────────────

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

        const auto found = SplitAdjustmentAudit::check(splits, values);

        QCOMPARE(found.size(), 2);
        QCOMPARE(found.at(0).split.guid(), QStringLiteral("s-later"));
        QCOMPARE(found.at(1).split.guid(), QStringLiteral("s-earlier"));
    }
};

QTEST_MAIN(TestSplitAdjustmentAudit)
#include "tst_splitadjustmentaudit.moc"

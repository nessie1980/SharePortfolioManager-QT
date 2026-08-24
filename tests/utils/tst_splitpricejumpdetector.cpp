// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// Unit tests for SplitPriceJumpDetector — stateless, database-free heuristic
// that suggests whether a share's stored price history around a split's
// ex-date already reflects the split (ShareSplitObject::pricesAdjusted()).
// Fixture values loosely mirror the Alphabet Inc. Cl. A case documented in
// ARCHITECTURE.md ("Split-Verhaeltnis: Notation der Bankmitteilungen"):
// 20:1 split, ex-date 18.07.2022.
//
// Erweitert 22.08.2026 um die Gegenprobe des Verhaeltnisses (Punkt 3 der
// Split-Plausibilitaetspruefung): passt der gemessene Sprung besser zu einem
// anderen Verhaeltnis als zu dem eingetragenen?
#include <QtTest>

#include "../../app/utils/SplitPriceJumpDetector.h"

namespace {
QDate d(int y, int m, int day) { return QDate(y, m, day); }

DailyValuesObject dv(const QDate& date, double closing)
{
    return DailyValuesObject(QStringLiteral("share-guid"), date,
                             /*opening=*/closing, closing,
                             /*top=*/closing, /*bottom=*/closing, /*volume=*/0.0);
}
}

class TestSplitPriceJumpDetector : public QObject
{
    Q_OBJECT

private slots:

    // ── Sprung erkannt (NotAdjusted) ────────────────────────────────────────

    void test_detect_clearJump_returnsNotAdjusted()
    {
        // Vor dem Ex-Tag ~1000, danach ~50 — passt zum Faktor 20.
        const QList<DailyValuesObject> values = {
            dv(d(2022, 7, 15), 1005.0),
            dv(d(2022, 7, 18), 1003.0),
            dv(d(2022, 7, 19),   50.20),
            dv(d(2022, 7, 20),   50.60),
        };
        const auto outcome = SplitPriceJumpDetector::detect(values, d(2022, 7, 18), 20.0);

        QCOMPARE(outcome.result, SplitPriceJumpDetector::Result::NotAdjusted);
        QCOMPARE(outcome.dateBefore, d(2022, 7, 18));
        QCOMPARE(outcome.dateAfter,  d(2022, 7, 19));
        QVERIFY(outcome.priceBefore > outcome.priceAfter);
    }

    void test_detect_reverseSplit_smallFactor_stillDetectsJump()
    {
        // Reverse-Split 1:10 (factor 0.1): Kurs steigt nach dem Ex-Tag um ~10.
        const QList<DailyValuesObject> values = {
            dv(d(2023, 1, 9),  5.00),
            dv(d(2023, 1, 10), 50.00),
        };
        const auto outcome = SplitPriceJumpDetector::detect(values, d(2023, 1, 9), 0.1);
        QCOMPARE(outcome.result, SplitPriceJumpDetector::Result::NotAdjusted);
    }

    // ── Kein Sprung (Adjusted) ───────────────────────────────────────────────

    void test_detect_noJump_returnsAdjusted()
    {
        const QList<DailyValuesObject> values = {
            dv(d(2022, 7, 15), 49.80),
            dv(d(2022, 7, 18), 50.20),
            dv(d(2022, 7, 19), 50.60),
            dv(d(2022, 7, 20), 50.90),
        };
        const auto outcome = SplitPriceJumpDetector::detect(values, d(2022, 7, 18), 20.0);
        QCOMPARE(outcome.result, SplitPriceJumpDetector::Result::Adjusted);
    }

    // ── Uneindeutig ──────────────────────────────────────────────────────────

    void test_detect_ratioBetweenBands_returnsAmbiguous()
    {
        // Verhaeltnis 5.0 liegt weder nah bei 1.0 noch nah bei Faktor 20.
        const QList<DailyValuesObject> values = {
            dv(d(2022, 7, 18), 100.0),
            dv(d(2022, 7, 19),  20.0),
        };
        const auto outcome = SplitPriceJumpDetector::detect(values, d(2022, 7, 18), 20.0);
        QCOMPARE(outcome.result, SplitPriceJumpDetector::Result::Ambiguous);
    }

    void test_detect_smallFactor_overlappingBands_returnsAmbiguous()
    {
        // Faktor 1.25 (Split 5:4): Toleranzbaender um 1.0 (+-15%) und um 1.25
        // (+-20%) ueberlappen sich, ein Verhaeltnis von 1.1 matched beide.
        const QList<DailyValuesObject> values = {
            dv(d(2022, 7, 18), 110.0),
            dv(d(2022, 7, 19), 100.0),
        };
        const auto outcome = SplitPriceJumpDetector::detect(values, d(2022, 7, 18), 1.25);
        QCOMPARE(outcome.result, SplitPriceJumpDetector::Result::Ambiguous);
    }

    // ── Nicht genug Daten ────────────────────────────────────────────────────

    void test_detect_noDataAtAll_returnsInsufficientData()
    {
        const auto outcome = SplitPriceJumpDetector::detect({}, d(2022, 7, 18), 20.0);
        QCOMPARE(outcome.result, SplitPriceJumpDetector::Result::InsufficientData);
    }

    void test_detect_onlyDataBefore_returnsInsufficientData()
    {
        const QList<DailyValuesObject> values = { dv(d(2022, 7, 18), 1003.0) };
        const auto outcome = SplitPriceJumpDetector::detect(values, d(2022, 7, 18), 20.0);
        QCOMPARE(outcome.result, SplitPriceJumpDetector::Result::InsufficientData);
    }

    void test_detect_onlyDataAfter_returnsInsufficientData()
    {
        const QList<DailyValuesObject> values = { dv(d(2022, 7, 19), 50.20) };
        const auto outcome = SplitPriceJumpDetector::detect(values, d(2022, 7, 18), 20.0);
        QCOMPARE(outcome.result, SplitPriceJumpDetector::Result::InsufficientData);
    }

    void test_detect_dataOutsideLookbackWindow_ignored()
    {
        const QList<DailyValuesObject> values = {
            dv(d(2022, 6, 1),  1003.0), // weit ausserhalb des Standardfensters (15 Tage)
            dv(d(2022, 7, 19),   50.20),
        };
        const auto outcome = SplitPriceJumpDetector::detect(values, d(2022, 7, 18), 20.0);
        QCOMPARE(outcome.result, SplitPriceJumpDetector::Result::InsufficientData);
    }

    // ── Ex-Tag selbst zaehlt als "davor" ──────────────────────────────────────
    // Dieselbe Konvention wie ShareSplitAdjuster::volumeFactor(): ein Beleg
    // GENAU am Ex-Tag liegt fachlich noch vor dem Split.

    void test_detect_priceOnExDateItself_countsAsBefore()
    {
        const QList<DailyValuesObject> values = {
            dv(d(2022, 7, 18), 1003.0), // genau am Ex-Tag
            dv(d(2022, 7, 19),   50.20),
        };
        const auto outcome = SplitPriceJumpDetector::detect(values, d(2022, 7, 18), 20.0);
        QCOMPARE(outcome.dateBefore, d(2022, 7, 18));
        QCOMPARE(outcome.result, SplitPriceJumpDetector::Result::NotAdjusted);
    }

    // ── Nachbar-Splits begrenzen das Suchfenster ─────────────────────────────

    void test_detect_previousSplitDate_boundsWindowStart()
    {
        // Ein frueherer Split liegt am 10.07.2022. Der einzige Kurs "davor"
        // (05.07.) liegt VOR diesem Nachbarn und darf nicht mitgezaehlt werden.
        const QList<DailyValuesObject> values = {
            dv(d(2022, 7, 5),  9999.0), // ausserhalb, weil vor previousSplitDate+1
            dv(d(2022, 7, 19),   50.20),
        };
        const auto outcome = SplitPriceJumpDetector::detect(
            values, d(2022, 7, 18), 20.0, /*previousSplitDate=*/d(2022, 7, 10));
        QCOMPARE(outcome.result, SplitPriceJumpDetector::Result::InsufficientData);
    }

    void test_detect_nextSplitDate_boundsWindowEnd_inclusive()
    {
        // Ein spaeterer Split liegt am 19.07.2022 — ein Kurs GENAU an diesem
        // Datum zaehlt laut Klassendoku noch mit ("inklusive").
        const QList<DailyValuesObject> values = {
            dv(d(2022, 7, 18), 1003.0),
            dv(d(2022, 7, 19),   50.20),
        };
        const auto outcome = SplitPriceJumpDetector::detect(
            values, d(2022, 7, 18), 20.0, QDate(), /*nextSplitDate=*/d(2022, 7, 19));
        QCOMPARE(outcome.dateAfter, d(2022, 7, 19));
        QCOMPARE(outcome.result, SplitPriceJumpDetector::Result::NotAdjusted);
    }

    void test_detect_dataAfterNextSplitDate_excluded()
    {
        const QList<DailyValuesObject> values = {
            dv(d(2022, 7, 18), 1003.0),
            dv(d(2022, 7, 20),   50.20), // liegt NACH dem Nachbar-Split vom 19.07.
        };
        const auto outcome = SplitPriceJumpDetector::detect(
            values, d(2022, 7, 18), 20.0, QDate(), /*nextSplitDate=*/d(2022, 7, 19));
        QCOMPARE(outcome.result, SplitPriceJumpDetector::Result::InsufficientData);
    }

    // ── Ungueltige Eingaben ──────────────────────────────────────────────────

    void test_detect_invalidExDate_returnsInsufficientData()
    {
        const auto outcome = SplitPriceJumpDetector::detect(
            { dv(d(2022, 7, 18), 1003.0), dv(d(2022, 7, 19), 50.2) }, QDate(), 20.0);
        QCOMPARE(outcome.result, SplitPriceJumpDetector::Result::InsufficientData);
    }

    void test_detect_zeroFactor_returnsInsufficientData()
    {
        const auto outcome = SplitPriceJumpDetector::detect(
            { dv(d(2022, 7, 18), 1003.0), dv(d(2022, 7, 19), 50.2) }, d(2022, 7, 18), 0.0);
        QCOMPARE(outcome.result, SplitPriceJumpDetector::Result::InsufficientData);
    }

    // ── Naechstgelegener Kurs, nicht irgendeiner ─────────────────────────────

    void test_detect_picksNearestPriceOnEachSide()
    {
        const QList<DailyValuesObject> values = {
            dv(d(2022, 7, 10), 1100.0), // weiter weg, muss ignoriert werden
            dv(d(2022, 7, 18), 1003.0), // naechster Kurs vor dem Ex-Tag
            dv(d(2022, 7, 19),   50.20), // naechster Kurs danach
            dv(d(2022, 7, 25),   52.00), // weiter weg, muss ignoriert werden
        };
        const auto outcome = SplitPriceJumpDetector::detect(values, d(2022, 7, 18), 20.0);
        QCOMPARE(outcome.dateBefore, d(2022, 7, 18));
        QCOMPARE(outcome.priceBefore, 1003.0);
        QCOMPARE(outcome.dateAfter, d(2022, 7, 19));
        QCOMPARE(outcome.priceAfter, 50.20);
    }

    // ── Gegenprobe des Verhaeltnisses (Punkt 3, 22.08.2026) ──────────────────
    //
    // Zweite, feinere Frage: passt der gemessene Sprung womoeglich BESSER zu
    // einem anderen Verhaeltnis als zu dem eingetragenen? Die Toleranzbaender
    // der Ja/Nein-Einordnung taugen dafuer nicht — mit +-20 % geht bei
    // eingetragenen 19 auch ein gemessener Sprung von 19,98 als Treffer durch.
    // Genau das ist der Feldfall Alphabet.

    void test_detect_enteredRatioTooSmall_reportsMismatch()
    {
        // Der Feldfall: Bank schreibt "1:19", eingetragen wurde 19 statt 20.
        const QList<DailyValuesObject> values = {
            dv(d(2022, 7, 18), 1003.0),
            dv(d(2022, 7, 19),   50.20),
        };
        const auto outcome = SplitPriceJumpDetector::detect(values, d(2022, 7, 18), 19.0);

        QCOMPARE(outcome.result, SplitPriceJumpDetector::Result::NotAdjusted);
        QVERIFY(outcome.ratioMismatch);
        QCOMPARE(outcome.impliedFactor, 20.0);
    }

    void test_detect_enteredRatioCorrect_noMismatch()
    {
        const QList<DailyValuesObject> values = {
            dv(d(2022, 7, 18), 1003.0),
            dv(d(2022, 7, 19),   50.20),
        };
        const auto outcome = SplitPriceJumpDetector::detect(values, d(2022, 7, 18), 20.0);

        QCOMPARE(outcome.result, SplitPriceJumpDetector::Result::NotAdjusted);
        QVERIFY(!outcome.ratioMismatch);
    }

    void test_detect_enteredRatioTooLarge_reportsMismatch()
    {
        // Das ist der Fall, den KEINE der Bestandspruefungen bemerken kann:
        // ein zu grosses Verhaeltnis erzeugt nie eine Unterdeckung.
        const QList<DailyValuesObject> values = {
            dv(d(2022, 7, 18), 1003.0),
            dv(d(2022, 7, 19),   50.20),
        };
        const auto outcome = SplitPriceJumpDetector::detect(values, d(2022, 7, 18), 21.0);

        QVERIFY(outcome.ratioMismatch);
        QCOMPARE(outcome.impliedFactor, 20.0);
    }

    void test_detect_adjustedHistory_neverReportsMismatch()
    {
        // Kein Sprung heisst: es gibt gar nichts, woran sich ein Verhaeltnis
        // ablesen liesse — auch bei grob falschem Faktor kein Verdacht.
        const QList<DailyValuesObject> values = {
            dv(d(2022, 7, 18), 50.20),
            dv(d(2022, 7, 19), 50.60),
        };
        const auto outcome = SplitPriceJumpDetector::detect(values, d(2022, 7, 18), 19.0);

        QCOMPARE(outcome.result, SplitPriceJumpDetector::Result::Adjusted);
        QVERIFY(!outcome.ratioMismatch);
    }

    void test_detect_insufficientData_neverReportsMismatch()
    {
        const auto outcome = SplitPriceJumpDetector::detect({}, d(2022, 7, 18), 19.0);

        QCOMPARE(outcome.result, SplitPriceJumpDetector::Result::InsufficientData);
        QVERIFY(!outcome.ratioMismatch);
    }

    void test_detect_ambiguousResult_canStillReportMismatch()
    {
        // Gemessen wird ein 5:1-Sprung, eingetragen sind 20:1. Fuer die
        // Ja/Nein-Einordnung bleibt das uneindeutig — das Verhaeltnis laesst
        // sich trotzdem benennen.
        const QList<DailyValuesObject> values = {
            dv(d(2022, 7, 18), 100.0),
            dv(d(2022, 7, 19),  20.0),
        };
        const auto outcome = SplitPriceJumpDetector::detect(values, d(2022, 7, 18), 20.0);

        QCOMPARE(outcome.result, SplitPriceJumpDetector::Result::Ambiguous);
        QVERIFY(outcome.ratioMismatch);
        QCOMPARE(outcome.impliedFactor, 5.0);
    }

    void test_detect_reverseSplit_wrongRatio_reportsMismatch()
    {
        // Reverse-Split: der Kurs STEIGT, observedRatio liegt unter 1. Der
        // Kandidat ist dann der Kehrwert der naechsten ganzen Zahl.
        const QList<DailyValuesObject> values = {
            dv(d(2023, 1, 9),   5.00),
            dv(d(2023, 1, 10), 50.00),
        };
        const auto outcome = SplitPriceJumpDetector::detect(values, d(2023, 1, 9), 1.0 / 9.0);

        QVERIFY(outcome.ratioMismatch);
        QVERIFY(qAbs(outcome.impliedFactor - 0.1) < 1e-9);
    }

    void test_detect_reverseSplit_correctRatio_noMismatch()
    {
        const QList<DailyValuesObject> values = {
            dv(d(2023, 1, 9),   5.00),
            dv(d(2023, 1, 10), 50.00),
        };
        const auto outcome = SplitPriceJumpDetector::detect(values, d(2023, 1, 9), 0.1);

        QVERIFY(!outcome.ratioMismatch);
    }

    void test_detect_noisyPriceWithinTolerance_staysSilent()
    {
        // Gemessen 20,5 bei eingetragenen 20 — das sind 2,5 % und liegt
        // innerhalb der Toleranz. Tagesschwankungen duerfen keinen Verdacht
        // ausloesen.
        const QList<DailyValuesObject> values = {
            dv(d(2022, 7, 18), 1025.0),
            dv(d(2022, 7, 19),   50.00),
        };
        const auto outcome = SplitPriceJumpDetector::detect(values, d(2022, 7, 18), 20.0);

        QVERIFY(!outcome.ratioMismatch);
    }

    void test_detect_smallFactorSplit_staysSilent()
    {
        // 5:4-Split: die naechste ganze Zahl waere 1, also gar kein Split.
        // Bei so kleinen Verhaeltnissen schweigt die Gegenprobe.
        const QList<DailyValuesObject> values = {
            dv(d(2022, 7, 18), 125.0),
            dv(d(2022, 7, 19), 100.0),
        };
        const auto outcome = SplitPriceJumpDetector::detect(values, d(2022, 7, 18), 1.25);

        QVERIFY(!outcome.ratioMismatch);
        QCOMPARE(outcome.impliedFactor, 0.0);
    }
};

QTEST_MAIN(TestSplitPriceJumpDetector)
#include "tst_splitpricejumpdetector.moc"

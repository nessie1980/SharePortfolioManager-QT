// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "SplitPriceJumpDetector.h"

namespace {
/// Toleranz um 1,0 (kein Sprung) — relativ, siehe Klassendoku für die
/// Ueberlappungs-Problematik bei kleinen Faktoren.
constexpr double kNoJumpTolerance = 0.15;

/// Toleranz um den erwarteten Faktor (Sprung erkannt) — relativ.
constexpr double kJumpTolerance = 0.20;
}

// ── windowStart / windowEnd ──────────────────────────────────────────────────

QDate SplitPriceJumpDetector::windowStart(const QDate& exDate, const QDate& previousSplitDate,
                                          int maxLookbackDays)
{
    QDate start = exDate.addDays(-maxLookbackDays);
    if (previousSplitDate.isValid() && previousSplitDate.addDays(1) > start)
        start = previousSplitDate.addDays(1);
    return start;
}

QDate SplitPriceJumpDetector::windowEnd(const QDate& exDate, const QDate& nextSplitDate,
                                        int maxLookbackDays)
{
    QDate end = exDate.addDays(maxLookbackDays);
    if (nextSplitDate.isValid() && nextSplitDate < end)
        end = nextSplitDate;
    return end;
}

// ── detect ────────────────────────────────────────────────────────────────────

SplitPriceJumpDetector::Outcome SplitPriceJumpDetector::detect(
    const QList<DailyValuesObject>& dailyValues,
    const QDate& exDate,
    double factor,
    const QDate& previousSplitDate,
    const QDate& nextSplitDate,
    int maxLookbackDays)
{
    Outcome outcome;

    if (!exDate.isValid() || factor <= 0.0)
        return outcome;

    const QDate start = windowStart(exDate, previousSplitDate, maxLookbackDays);
    const QDate end    = windowEnd(exDate, nextSplitDate, maxLookbackDays);

    // Naechstgelegenen Kurs VOR (bzw. AN, siehe Klassendoku "inklusive") dem
    // Ex-Tag und NACH dem Ex-Tag suchen — dieselbe Grenzziehung wie
    // ShareSplitAdjuster::volumeFactor() ("split.date() > date" zaehlt als
    // "danach"): ein Kurs GENAU am Ex-Tag liegt fachlich noch vor dem Split.
    for (const DailyValuesObject& dv : dailyValues) {
        if (!dv.isValid())
            continue;
        const QDate d = dv.date();
        if (d < start || d > end)
            continue;

        if (d <= exDate) {
            if (!outcome.dateBefore.isValid() || d > outcome.dateBefore) {
                outcome.dateBefore  = d;
                outcome.priceBefore = dv.closingPrice();
            }
        } else {
            if (!outcome.dateAfter.isValid() || d < outcome.dateAfter) {
                outcome.dateAfter  = d;
                outcome.priceAfter = dv.closingPrice();
            }
        }
    }

    if (!outcome.dateBefore.isValid() || !outcome.dateAfter.isValid()
        || outcome.priceBefore <= 0.0 || outcome.priceAfter <= 0.0) {
        outcome.result = Result::InsufficientData;
        return outcome;
    }

    outcome.observedRatio = outcome.priceBefore / outcome.priceAfter;

    const bool matchesNoJump = outcome.observedRatio >= (1.0 - kNoJumpTolerance)
                             && outcome.observedRatio <= (1.0 + kNoJumpTolerance);
    const bool matchesJump   = outcome.observedRatio >= factor * (1.0 - kJumpTolerance)
                             && outcome.observedRatio <= factor * (1.0 + kJumpTolerance);

    if (matchesNoJump && !matchesJump)
        outcome.result = Result::Adjusted;
    else if (matchesJump && !matchesNoJump)
        outcome.result = Result::NotAdjusted;
    else
        // Beides trifft zu (ueberlappende Baender bei kleinem Faktor) oder
        // keins von beidem (Kursbewegung passt zu nichts Erwartetem) — in
        // beiden Faellen keine sichere Aussage.
        outcome.result = Result::Ambiguous;

    return outcome;
}

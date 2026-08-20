// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "SplitAdjustmentAudit.h"

// ── check ─────────────────────────────────────────────────────────────────────

QList<SplitAdjustmentAudit::Discrepancy> SplitAdjustmentAudit::check(
    const QList<ShareSplitObject>& splits,
    const QList<DailyValuesObject>& dailyValues)
{
    QList<Discrepancy> result;

    for (const ShareSplitObject& split : splits) {
        // Nachbar-Splits derselben Aktie begrenzen das Suchfenster, damit ein
        // benachbarter Split das Ergebnis nicht verfälscht — dieselbe Logik
        // wie PresenterShareSplitEdit::onCheckPriceJump() (der geprüfte Split
        // selbst zählt dabei nicht als eigener Nachbar).
        QDate previousSplitDate;
        QDate nextSplitDate;
        for (const ShareSplitObject& other : splits) {
            if (other.guid() == split.guid())
                continue;
            if (other.date() < split.date()
                && (!previousSplitDate.isValid() || other.date() > previousSplitDate))
                previousSplitDate = other.date();
            if (other.date() > split.date()
                && (!nextSplitDate.isValid() || other.date() < nextSplitDate))
                nextSplitDate = other.date();
        }

        const SplitPriceJumpDetector::Outcome outcome = SplitPriceJumpDetector::detect(
            dailyValues, split.date(), split.factor(), previousSplitDate, nextSplitDate);

        using Result = SplitPriceJumpDetector::Result;
        const bool contradicts =
            (outcome.result == Result::Adjusted    && !split.pricesAdjusted())
         || (outcome.result == Result::NotAdjusted &&  split.pricesAdjusted());

        if (contradicts)
            result.append(Discrepancy{ split, outcome });
    }

    return result;
}

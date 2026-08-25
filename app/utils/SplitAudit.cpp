// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "SplitAudit.h"

// ── check ─────────────────────────────────────────────────────────────────────

QList<SplitAudit::Discrepancy> SplitAudit::check(
    const QList<ShareSplitObject>& splits,
    const QList<DailyValuesObject>& dailyValues,
    const QList<BuyObject>&  buys,
    const QList<SaleObject>& sales)
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
            result.append(Discrepancy{ split, outcome, Kind::AdjustmentFlag, {} });

        // Verhaeltnis-Gegenprobe aus dem Kurssprung (Punkt 3). Kostet nichts
        // — detect() oben hat sie bereits mitgerechnet, das Ergebnis wurde
        // bisher nur weggeworfen.
        if (outcome.ratioMismatch)
            result.append(Discrepancy{ split, outcome, Kind::RatioFromPrices, {} });
    }

    // ── Bestandspruefung (Punkt 4) ────────────────────────────────────────
    // Einmal je Aktie, nicht je Split: checkAgainstHistory() liefert die
    // frueheste Fundstelle ueber alle Depots, ein Aufruf je Split wuerde
    // dieselbe Stelle mehrfach melden. Als Stichtag dient deshalb der
    // frueheste Ex-Tag.
    if (!buys.isEmpty() && !sales.isEmpty()) {
        QDate earliestSplitDate;
        for (const ShareSplitObject& split : splits) {
            if (!split.isValid() || !split.date().isValid())
                continue;
            if (!earliestSplitDate.isValid() || split.date() < earliestSplitDate)
                earliestSplitDate = split.date();
        }

        if (earliestSplitDate.isValid()) {
            const SplitHistoryConflict conflict = SplitRatioChecker::checkAgainstHistory(
                splits, earliestSplitDate, buys, sales);

            // Nur bei eindeutiger Zuordnung melden (Nessies Entscheidung
            // 22.08.2026): hier erscheint ein modaler Dialog bei JEDEM
            // Programmstart, den niemand abstellen kann, solange der Befund
            // besteht. Eine unvollstaendig erfasste Kaufhistorie erzeugt
            // denselben Widerspruch, ohne dass es etwas zu korrigieren gaebe.
            //
            // hasProposal setzt voraus, dass genau ein Split in Frage kommt —
            // deshalb ist splitsBetween.first() hier die richtige Zuordnung
            // und nicht geraten.
            if (conflict.hasConflict && conflict.suspicion.hasProposal) {
                result.append(Discrepancy{ conflict.suspicion.splitsBetween.first(),
                                           SplitPriceJumpDetector::Outcome{},
                                           Kind::RatioFromHoldings,
                                           conflict });
            }
        }
    }

    return result;
}

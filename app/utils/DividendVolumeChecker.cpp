// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "DividendVolumeChecker.h"
#include "ShareSplitAdjuster.h"

#include <QtGlobal>

// ── holdingsAtExDate ──────────────────────────────────────────────────────────

double DividendVolumeChecker::holdingsAtExDate(const QList<BuyObject>&        buys,
                                               const QList<SaleObject>&       sales,
                                               const QList<ShareSplitObject>& splits,
                                               const QString&                 depotNumber,
                                               const QDate&                   exDate,
                                               int* outBuys,
                                               int* outSales)
{
    if (outBuys)  *outBuys  = 0;
    if (outSales) *outSales = 0;

    if (!exDate.isValid())
        return 0.0;

    const QString depot = depotNumber.trimmed();

    // Zwischensumme bewusst auf HEUTIGER Skala: nur dort sind Belege
    // unterschiedlicher Datumsstände überhaupt addierbar (siehe Klassendoku).
    double holdingsToday = 0.0;

    for (const BuyObject& b : buys) {
        if (b.depotNumber().trimmed() != depot) continue;
        const QDate d = b.date();
        // ECHT VOR dem Ex-Tag — ein Kauf am Ex-Tag ist nicht mehr
        // dividendenberechtigt (siehe Klassendoku, "Stichtagsregel").
        if (!d.isValid() || d >= exDate) continue;
        holdingsToday += ShareSplitAdjuster::adjustedVolume(b.volume(), splits, d);
        if (outBuys) ++(*outBuys);
    }

    for (const SaleObject& s : sales) {
        if (s.depotNumber().trimmed() != depot) continue;
        const QDate d = s.date();
        // Ebenfalls ECHT VOR dem Ex-Tag: wer am Ex-Tag verkauft, erhält die
        // Dividende noch, sein Bestand zählt hier also weiterhin mit.
        if (!d.isValid() || d >= exDate) continue;
        holdingsToday -= ShareSplitAdjuster::adjustedVolume(s.volume(), splits, d);
        if (outSales) ++(*outSales);
    }

    // Zurück auf die Beleg-Skala des Ex-Tags — in dieser steht die Stückzahl
    // auf der Dividendenabrechnung.
    return ShareSplitAdjuster::belegVolume(holdingsToday, splits, exDate);
}

// ── check ─────────────────────────────────────────────────────────────────────

DividendVolumeCheckResult DividendVolumeChecker::check(
    double                         enteredVolume,
    const QDate&                   exDate,
    const QString&                 depotNumber,
    const QList<BuyObject>&        buys,
    const QList<SaleObject>&       sales,
    const QList<ShareSplitObject>& splits)
{
    DividendVolumeCheckResult result;
    result.enteredVolume = enteredVolume;

    // Ohne Ex-Tag oder Depotnummer fehlt die Bezugsgrösse. Beide sind seit
    // Phase 2 Pflichtfelder und werden vor dieser Prüfung abgefragt — der
    // Zweig bleibt trotzdem stehen, damit die Klasse für sich genommen
    // vollständig ist und nicht auf eine bestimmte Aufrufreihenfolge baut.
    if (!exDate.isValid() || depotNumber.trimmed().isEmpty())
        return result;

    // Keine einzige Kaufbuchung für diese Aktie: dann gibt es nichts, wogegen
    // sich prüfen liesse. Bewusst NICHT als Abweichung gewertet — sonst wäre
    // eine Dividende bei einer Aktie, deren Kaufhistorie (noch) nicht erfasst
    // ist, überhaupt nicht mehr speicherbar. Die Prüfung greift, sobald der
    // erste Kauf erfasst ist.
    if (buys.isEmpty())
        return result;

    result.checkable      = true;
    result.expectedVolume = holdingsAtExDate(buys, sales, splits,
                                             depotNumber, exDate,
                                             &result.consideredBuys,
                                             &result.consideredSales);
    result.matches = qAbs(result.deviation()) <= kVolumeTolerance;
    return result;
}

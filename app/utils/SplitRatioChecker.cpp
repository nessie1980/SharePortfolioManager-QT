// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "SplitRatioChecker.h"
#include "ShareSplitAdjuster.h"

#include <QMap>
#include <QtGlobal>

#include <algorithm>
#include <utility>

namespace {

/// Depot-Schluessel: getrimmt verglichen wie in DividendVolumeChecker.
QString depotKey(const QString& depotNumber)
{
    return depotNumber.trimmed();
}

/// Ein Verkauf im Bestandsverlauf, auf das Noetigste reduziert.
struct SaleEvent
{
    QDate  date;
    double volume = 0.0;
};

} // namespace

// -- diagnose (Lot-Variante, eigentlicher Rechenkern) ------------------------------

SplitRatioSuspicion SplitRatioChecker::diagnose(
    double                         requiredVolumeToday,
    const QDate&                   referenceDate,
    const QList<SplitVolumeLot>&   lots,
    const QList<ShareSplitObject>& splits)
{
    SplitRatioSuspicion suspicion;

    if (!referenceDate.isValid())
        return suspicion;

    // Aeltester Posten mit Menge. Ein Split VOR diesem Datum wirkt sich auf
    // keinen der Posten aus (ShareSplitAdjuster::volumeFactor() zaehlt nur
    // Splits ECHT NACH dem Belegdatum) und kann die Unterdeckung folglich
    // nicht erklaeren.
    QDate earliestLot;
    for (const SplitVolumeLot& lot : lots) {
        if (lot.volume <= kVolumeEpsilon || !lot.date.isValid())
            continue;
        if (!earliestLot.isValid() || lot.date < earliestLot)
            earliestLot = lot.date;
    }
    if (!earliestLot.isValid())
        return suspicion;   // gar keine Posten — nichts zu deuten

    // Splits NACH dem Stichtag skalieren Posten und angeforderte Menge
    // gleichermassen auf heutige Skala; sie kuerzen sich im Vergleich heraus
    // und bleiben deshalb aussen vor.
    for (const ShareSplitObject& split : splits) {
        if (!split.isValid() || !split.date().isValid())
            continue;
        if (split.date() > earliestLot && split.date() <= referenceDate)
            suspicion.splitsBetween.append(split);
    }
    if (suspicion.splitsBetween.isEmpty())
        return suspicion;

    std::sort(suspicion.splitsBetween.begin(), suspicion.splitsBetween.end(),
              [](const ShareSplitObject& a, const ShareSplitObject& b) {
                  return a.date() < b.date();
              });

    suspicion.hasSuspicion = true;

    // ── Ab hier nur noch der Verhaeltnis-Vorschlag ────────────────────────
    // Bedingung 1: genau ein Split. Bei mehreren waere nicht zuzuordnen,
    // welcher gemeint ist — die blosse Nennung bleibt, die Zahl entfaellt.
    if (suspicion.splitsBetween.size() != 1)
        return suspicion;

    const ShareSplitObject& suspect = suspicion.splitsBetween.first();
    const double factor = suspect.factor();
    if (factor <= 0.0)
        return suspicion;

    // Bedingung 2 und 3: alte Seite 1 und kein Reverse-Split. Nur dann gibt
    // es die Delta-vs-Gesamt-Verwechslung der Bankmitteilung ueberhaupt.
    if (qAbs(suspect.ratioOld() - 1.0) > kRatioTolerance)
        return suspicion;
    if (suspect.ratioNew() < 1.0)
        return suspicion;

    // Verfuegbare Menge (heutige Skala) aufgeteilt nach Posten vor und ab dem
    // Splittag. Nur der erste Teil traegt den fraglichen Faktor; ein Posten
    // AM Splittag liegt bereits in der neuen Stueckelung vor (derselbe
    // Massstab wie ShareSplitAdjuster::volumeFactor()).
    double beforeSplit = 0.0;
    double fromSplitOn = 0.0;
    for (const SplitVolumeLot& lot : lots) {
        if (lot.volume <= kVolumeEpsilon || !lot.date.isValid())
            continue;

        const double today = ShareSplitAdjuster::adjustedVolume(lot.volume, splits, lot.date);
        if (lot.date < suspect.date())
            beforeSplit += today;
        else
            fromSplitOn += today;
    }
    if (beforeSplit <= kVolumeEpsilon)
        return suspicion;   // ohne Posten vor dem Split ist nichts zurueckzurechnen

    const double target = requiredVolumeToday - fromSplitOn;
    if (target <= kVolumeEpsilon)
        return suspicion;

    const double impliedFactor = factor * target / beforeSplit;

    // Bedingung 4: exakt eins mehr als eingetragen — der dokumentierte,
    // systematische Fehler. Jede andere Zahl waere geraten; siehe die
    // Begruendung im Klassenkopf (Tippfehler 2.000 statt 200 ergaebe hier
    // ein ebenso "sauberes", aber voellig irrefuehrendes Verhaeltnis).
    const double proposedNew = suspect.ratioNew() + 1.0;
    if (qAbs(impliedFactor - proposedNew) > kRatioTolerance * qMax(1.0, proposedNew))
        return suspicion;

    suspicion.hasProposal            = true;
    suspicion.proposedRatioNew       = proposedNew;
    suspicion.proposedRatioOld       = 1.0;
    suspicion.proposedAvailableToday = beforeSplit * (proposedNew / factor) + fromSplitOn;

    return suspicion;
}

// -- diagnose (Kauf-Ueberladung) ---------------------------------------------------

SplitRatioSuspicion SplitRatioChecker::diagnose(
    double                         requiredVolumeToday,
    const QDate&                   referenceDate,
    const QList<BuyObject>&        availableBuys,
    const QList<ShareSplitObject>& splits)
{
    QList<SplitVolumeLot> lots;
    lots.reserve(availableBuys.size());
    for (const BuyObject& buy : availableBuys)
        lots.append(SplitVolumeLot{ buy.date(), buy.volume() - buy.volumeSold() });

    return diagnose(requiredVolumeToday, referenceDate, lots, splits);
}

// -- checkAgainstHistory -----------------------------------------------------------

SplitHistoryConflict SplitRatioChecker::checkAgainstHistory(
    const QList<ShareSplitObject>& splits,
    const QDate&                   fromDate,
    const QList<BuyObject>&        buys,
    const QList<SaleObject>&       sales)
{
    SplitHistoryConflict result;

    if (!fromDate.isValid())
        return result;

    // Je Depot ein eigener Verlauf. QMap statt QHash, damit die Reihenfolge
    // der Depots feststeht und das Ergebnis bei gleichem Konfliktdatum
    // reproduzierbar bleibt.
    QMap<QString, QList<SplitVolumeLot>> buysByDepot;
    QMap<QString, QList<SaleEvent>>      salesByDepot;

    for (const BuyObject& buy : buys) {
        if (!buy.date().isValid() || buy.volume() <= kVolumeEpsilon)
            continue;
        // Volle Kaufmenge, NICHT der Restbestand: die Verkaeufe fuehrt der
        // Verlauf unten selbst, ueber volumeSold() waeren sie doppelt weg.
        buysByDepot[depotKey(buy.depotNumber())].append(
            SplitVolumeLot{ buy.date(), buy.volume() });
    }
    for (const SaleObject& sale : sales) {
        if (!sale.date().isValid() || sale.volume() <= kVolumeEpsilon)
            continue;
        salesByDepot[depotKey(sale.depotNumber())].append(
            SaleEvent{ sale.date(), sale.volume() });
    }

    for (auto it = salesByDepot.begin(); it != salesByDepot.end(); ++it) {
        const QString&         depot     = it.key();
        QList<SaleEvent>&      depotSales = it.value();
        const QList<SplitVolumeLot> depotBuys = buysByDepot.value(depot);

        std::sort(depotSales.begin(), depotSales.end(),
                  [](const SaleEvent& a, const SaleEvent& b) { return a.date < b.date; });

        double soldToday = 0.0;
        for (const SaleEvent& sale : std::as_const(depotSales)) {
            soldToday += ShareSplitAdjuster::adjustedVolume(sale.volume, splits, sale.date);

            // Unterdeckungen vor dem Ex-Tag skalieren mit allen anderen
            // Belegen gleich — das Verhaeltnis kann daran nichts aendern.
            if (sale.date < fromDate)
                continue;

            // Ein Kauf AM Verkaufstag zaehlt noch mit, sonst meldete ein
            // Kauf-und-Verkauf am selben Tag faelschlich eine Unterdeckung.
            double boughtToday = 0.0;
            for (const SplitVolumeLot& lot : depotBuys) {
                if (lot.date <= sale.date)
                    boughtToday += ShareSplitAdjuster::adjustedVolume(lot.volume, splits, lot.date);
            }

            if (soldToday <= boughtToday + kVolumeEpsilon)
                continue;

            // Frueheste Fundstelle gewinnt; bei gleichem Datum das
            // alphabetisch erste Depot (QMap-Reihenfolge).
            if (result.hasConflict && result.conflictDate <= sale.date)
                break;

            QList<SplitVolumeLot> lotsUntilSale;
            for (const SplitVolumeLot& lot : depotBuys) {
                if (lot.date <= sale.date)
                    lotsUntilSale.append(lot);
            }

            result.hasConflict    = true;
            result.depotNumber    = depot;
            result.conflictDate   = sale.date;
            result.requiredToday  = soldToday;
            result.availableToday = boughtToday;
            result.suspicion      = diagnose(soldToday, sale.date, lotsUntilSale, splits);
            break;   // je Depot nur die erste Fundstelle
        }
    }

    return result;
}

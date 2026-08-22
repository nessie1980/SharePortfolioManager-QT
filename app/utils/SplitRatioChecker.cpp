// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "SplitRatioChecker.h"
#include "ShareSplitAdjuster.h"

#include <QtGlobal>

#include <algorithm>

namespace {

/// Restbestand eines Kaufs in dessen Beleg-Skala.
double remainingBeleg(const BuyObject& buy)
{
    return buy.volume() - buy.volumeSold();
}

} // namespace

// -- diagnose ---------------------------------------------------------------------

SplitRatioSuspicion SplitRatioChecker::diagnose(
    double                         requiredVolumeToday,
    const QDate&                   referenceDate,
    const QList<BuyObject>&        availableBuys,
    const QList<ShareSplitObject>& splits)
{
    SplitRatioSuspicion suspicion;

    if (!referenceDate.isValid())
        return suspicion;

    // Aeltester Kauf mit Restbestand. Ein Split VOR diesem Datum wirkt sich
    // auf keinen der verfuegbaren Kaeufe aus (ShareSplitAdjuster::
    // volumeFactor() zaehlt nur Splits ECHT NACH dem Belegdatum) und kann
    // die Unterdeckung folglich nicht erklaeren.
    QDate earliestBuy;
    for (const BuyObject& buy : availableBuys) {
        if (remainingBeleg(buy) <= kVolumeEpsilon || !buy.date().isValid())
            continue;
        if (!earliestBuy.isValid() || buy.date() < earliestBuy)
            earliestBuy = buy.date();
    }
    if (!earliestBuy.isValid())
        return suspicion;   // gar keine offenen Kaeufe — nichts zu deuten

    // Splits NACH dem Stichtag skalieren Kaeufe und angeforderte Menge
    // gleichermassen auf heutige Skala; sie kuerzen sich im Vergleich heraus
    // und bleiben deshalb aussen vor.
    for (const ShareSplitObject& split : splits) {
        if (!split.isValid() || !split.date().isValid())
            continue;
        if (split.date() > earliestBuy && split.date() <= referenceDate)
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

    // Verfuegbare Menge (heutige Skala) aufgeteilt nach Kaeufen vor und ab
    // dem Splittag. Nur der erste Teil traegt den fraglichen Faktor; ein Kauf
    // AM Splittag liegt bereits in der neuen Stueckelung vor (derselbe
    // Massstab wie ShareSplitAdjuster::volumeFactor()).
    double beforeSplit = 0.0;
    double fromSplitOn = 0.0;
    for (const BuyObject& buy : availableBuys) {
        const double remaining = remainingBeleg(buy);
        if (remaining <= kVolumeEpsilon || !buy.date().isValid())
            continue;

        const double today = ShareSplitAdjuster::adjustedVolume(remaining, splits, buy.date());
        if (buy.date() < suspect.date())
            beforeSplit += today;
        else
            fromSplitOn += today;
    }
    if (beforeSplit <= kVolumeEpsilon)
        return suspicion;   // ohne Kauf vor dem Split ist nichts zurueckzurechnen

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

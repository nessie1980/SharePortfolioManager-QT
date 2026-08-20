// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../models/ShareSplitObject.h"
#include "../models/DailyValuesObject.h"
#include "SplitPriceJumpDetector.h"

#include <QList>

/**
 * @brief Vergleicht den gespeicherten `prices_adjusted`-Zustand der Splits
 * einer Aktie mit dem, was `SplitPriceJumpDetector` aus der aktuellen
 * Kurshistorie herausliest, und meldet Widersprüche.
 *
 * Phase 4 der Aktiensplit-Behandlung (siehe ARCHITECTURE.md, "Offene
 * Punkte", "Aktiensplits werden nicht behandelt"): "Automatische
 * Nachprüfung des prices_adjusted-Zustands nach jedem Tageswert-Abruf
 * (Kurssprung um den Splittag vergleichen) + Startmeldung bei Widerspruch,
 * analog warnAboutSharesWithoutDailyValues()".
 *
 * `SplitPriceJumpDetector` selbst ist an einen expliziten "Prüfen"-Knopf im
 * Split-Dialog gebunden und setzt `pricesAdjusted()` nie still (Nessies
 * Vorgabe 13.08.2026, siehe SplitPriceJumpDetector.h) — das gilt auch hier:
 * diese Klasse LIEST nur und meldet Widersprüche, sie SCHREIBT nichts in
 * die Datenbank. Die eigentliche Korrektur bleibt dem Nutzer im
 * `ShareSplitsForm` überlassen, genau wie beim manuellen "Prüfen"-Knopf.
 *
 * Zustandslos und datenbankfrei, gleiche Bauweise wie `ShareSplitAdjuster`
 * und `SplitPriceJumpDetector` selbst — Splits und Kurshistorie kommen als
 * Parameter herein.
 */
class SplitAdjustmentAudit
{
public:
    SplitAdjustmentAudit() = delete;

    /// Ein Split, dessen gespeicherter Zustand der erkannten Kurshistorie widerspricht.
    struct Discrepancy
    {
        ShareSplitObject split;                   ///< Der betroffene Split, inkl. gespeichertem pricesAdjusted().
        SplitPriceJumpDetector::Outcome outcome;   ///< Detektor-Ergebnis (Kurse/Daten) für die Meldung.
    };

    /**
     * @brief Prüft alle Splits einer Aktie gegen ihre Kurshistorie.
     *
     * Nachbar-Splits derselben Aktie begrenzen je geprüftem Split das
     * Suchfenster des Detektors, damit ein benachbarter Split das Ergebnis
     * nicht verfälscht — dieselbe Logik wie
     * `PresenterShareSplitEdit::onCheckPriceJump()`.
     *
     * Nur ein eindeutiges Detektor-Ergebnis (Adjusted/NotAdjusted), das dem
     * gespeicherten `pricesAdjusted()` widerspricht, zählt als Widerspruch.
     * Ambiguous- und InsufficientData-Ergebnisse werden übergangen — das
     * hält die Meldung frei von Verdachtsfällen, die der Nutzer ohnehin
     * nicht auflösen könnte.
     *
     * @param splits       Alle Splits der Aktie, beliebige Reihenfolge.
     * @param dailyValues  Komplette Kurshistorie der Aktie, beliebige Reihenfolge.
     * @return Widersprüche in der Reihenfolge von @p splits.
     */
    static QList<Discrepancy> check(const QList<ShareSplitObject>& splits,
                                    const QList<DailyValuesObject>& dailyValues);
};

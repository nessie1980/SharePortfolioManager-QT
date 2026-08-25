// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../models/ShareSplitObject.h"
#include "../models/DailyValuesObject.h"
#include "../models/BuyObject.h"
#include "../models/SaleObject.h"
#include "SplitPriceJumpDetector.h"
#include "SplitRatioChecker.h"

#include <QList>

/**
 * @brief Prüft die gespeicherten Splits einer Aktie gegen das, was
 * Kurshistorie und Belege tatsächlich zeigen, und meldet Widersprüche.
 *
 * Hiess bis zum 22.08.2026 `SplitAdjustmentAudit` und prüfte nur den
 * `prices_adjusted`-Zustand. Seit Punkt 4 der Split-Plausibilitätsprüfung
 * kommen zwei Verhältnis-Prüfungen dazu, weshalb der alte Name nicht mehr
 * zutraf (siehe ARCHITECTURE.md, "Plausibilitätsprüfung des
 * Split-Verhältnisses", Abschnitt "Punkt 4").
 *
 * ### Warum es diese Stufe gibt
 *
 * Die Punkte 1 bis 3 setzen alle eine Nutzeraktion voraus — einen Verkauf,
 * ein Speichern, einen Knopfdruck. Was bereits fehlerhaft in der Datenbank
 * steht und von sich aus nie wieder angefasst wird, fällt damit niemandem
 * auf. Genau das war der Feldfall Alphabet: der Split lag monatelang falsch
 * da, ohne dass irgendetwas ihn noch einmal angesehen hätte.
 *
 * ### Drei Arten von Befund
 *
 * 1. `AdjustmentFlag` — der gespeicherte `prices_adjusted`-Zustand
 *    widerspricht dem, was die Kurshistorie um den Ex-Tag zeigt. Das ist die
 *    ursprüngliche Prüfung aus Phase 4 der Aktiensplit-Behandlung.
 * 2. `RatioFromPrices` — der gemessene Kurssprung passt besser zu einem
 *    anderen Verhältnis als zu dem eingetragenen. Kostet nichts: das
 *    Ergebnis liegt in `SplitPriceJumpDetector::Outcome` bereits vor, weil
 *    der Detektor ohnehin je Split gerufen wird.
 * 3. `RatioFromHoldings` — die Verkaufshistorie geht mit den eingetragenen
 *    Verhältnissen nicht auf. Braucht Käufe und Verkäufe, siehe check().
 *
 * Ein Split kann mehrere Befunde gleichzeitig auslösen; sie sagen
 * Verschiedenes und werden deshalb einzeln gemeldet.
 *
 * `SplitPriceJumpDetector` selbst ist an einen expliziten "Prüfen"-Knopf im
 * Split-Dialog gebunden und setzt `pricesAdjusted()` nie still (Nessies
 * Vorgabe 13.08.2026, siehe SplitPriceJumpDetector.h) — das gilt auch hier:
 * diese Klasse LIEST nur und meldet Widersprüche, sie SCHREIBT nichts in
 * die Datenbank. Die eigentliche Korrektur bleibt dem Nutzer im
 * `ShareSplitsForm` überlassen, genau wie beim manuellen "Prüfen"-Knopf.
 *
 * @note Beide Verhältnis-Prüfungen melden nur bei EINDEUTIGER Zuordnung
 * (Nessies Entscheidung 22.08.2026). Das ist hier wichtiger als in den
 * Dialogen: dort steht eine Rückfrage mit Kontext, hier erscheint ein
 * modaler Dialog bei jedem Programmstart, den niemand abstellen kann, solange
 * der Befund besteht. Eine unvollständig erfasste Kaufhistorie — etwa nach
 * einem Depotübertrag — erzeugt denselben rechnerischen Widerspruch, ohne
 * dass es etwas zu korrigieren gäbe. Solche Fälle bleiben hier still und
 * werden weiterhin beim Speichern eines Splits oder eines Verkaufs sichtbar.
 *
 * Zustandslos und datenbankfrei, gleiche Bauweise wie `ShareSplitAdjuster`
 * und `SplitPriceJumpDetector` selbst — Splits und Kurshistorie kommen als
 * Parameter herein.
 */
class SplitAudit
{
public:
    SplitAudit() = delete;

    /// Art des Befunds, siehe Klassenkopf.
    enum class Kind
    {
        AdjustmentFlag,     ///< Gespeicherter prices_adjusted-Zustand widerspricht der Kurshistorie.
        RatioFromPrices,    ///< Verhältnis passt nicht zum gemessenen Kurssprung.
        RatioFromHoldings,  ///< Verkaufshistorie geht mit diesem Verhältnis nicht auf.
    };

    /// Ein Split, dessen gespeicherter Zustand dem widerspricht, was die Daten zeigen.
    struct Discrepancy
    {
        ShareSplitObject split;                   ///< Der betroffene Split, inkl. gespeichertem pricesAdjusted().
        SplitPriceJumpDetector::Outcome outcome;   ///< Detektor-Ergebnis (Kurse/Daten) für die Meldung.

        /**
         * @brief Art des Befunds.
         *
         * Steht bewusst NACH `split` und `outcome`: die beiden sind seit
         * Phase 4 in Gebrauch, und `Discrepancy{ split, outcome }` soll
         * weiterhin gültig bleiben (ergibt dann `AdjustmentFlag`).
         */
        Kind kind = Kind::AdjustmentFlag;

        /**
         * @brief Nur bei `RatioFromHoldings` gefüllt: die Fundstelle im
         * Bestandsverlauf.
         *
         * Der leere Initialisierer `{}` ist kein Schmuck: ohne ihn meldet
         * `-Wmissing-field-initializers` jede Aggregat-Initialisierung
         * `Discrepancy{ split, outcome }` — also genau die Kurzform, die
         * `kind` eine Zeile darüber ausdrücklich gültig halten soll. Ein
         * Default-Initialisierer nimmt das Feld aus der Warnung heraus,
         * ohne dass die Aufrufstellen alle vier Felder ausschreiben müssen.
         */
        SplitHistoryConflict conflict {};
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
     * Die Bestandsprüfung (`RatioFromHoldings`) läuft EINMAL JE AKTIE, nicht
     * je Split: `SplitRatioChecker::checkAgainstHistory()` liefert die
     * früheste Fundstelle über alle Depots, und ein Aufruf je Split würde
     * für den früheren Split dieselbe Stelle ein zweites Mal melden.
     * Zugeordnet wird der Befund dem Split, den die Rückrechnung benennt —
     * gemeldet wird ohnehin nur, wenn diese Zuordnung eindeutig ist, und
     * das heisst: genau ein Split kommt in Frage.
     *
     * @param splits       Alle Splits der Aktie, beliebige Reihenfolge.
     * @param dailyValues  Komplette Kurshistorie der Aktie, beliebige Reihenfolge.
     * @param buys         Alle Käufe der Aktie. Leer heisst: keine
     *        Bestandsprüfung — dieselbe Zurückhaltung wie bei fehlenden
     *        Kursdaten, ohne Belege gibt es nichts zu vergleichen.
     * @param sales        Alle Verkäufe der Aktie, gleiche Bedingung.
     * @return Befunde in der Reihenfolge von @p splits; der Bestandsbefund,
     *         falls vorhanden, steht am Ende.
     */
    static QList<Discrepancy> check(const QList<ShareSplitObject>& splits,
                                    const QList<DailyValuesObject>& dailyValues,
                                    const QList<BuyObject>&  buys  = {},
                                    const QList<SaleObject>& sales = {});
};

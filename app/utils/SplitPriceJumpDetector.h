// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../models/DailyValuesObject.h"

#include <QDate>
#include <QList>

/**
 * @brief Versucht anhand der gespeicherten Kurshistorie zu erkennen, ob sie
 * um den Ex-Tag eines Splits bereits split-bereinigt vorliegt.
 *
 * Hintergrund: `share_splits.prices_adjusted` (`ShareSplitObject::
 * pricesAdjusted()`) muss der Nutzer bisher von Hand einschätzen — siehe
 * ARCHITECTURE.md, "Split-Verhaeltnis: Notation der Bankmitteilungen". Dieser
 * Helfer bietet dafür einen Vorschlag: er vergleicht den letzten verfügbaren
 * Schlusskurs vor dem Ex-Tag mit dem ersten verfügbaren danach. Springt der
 * Kurs dabei um etwa den erwarteten Split-Faktor, liegt die Historie noch
 * NICHT bereinigt vor (Haken bleibt aus); bleibt der Kurs praktisch gleich,
 * liegt sie bereits bereinigt vor (Haken an).
 *
 * Zustandslos und vollständig datenbankfrei — die Kurshistorie kommt als
 * Parameter herein, im selben Stil wie `ShareSplitAdjuster`.
 *
 * @note Bewusst kein automatisches, stilles Setzen ohne Nutzeraktion
 * (Nessies Vorgabe 13.08.2026): der Aufruf ist an einen expliziten
 * "Prüfen"-Knopf gebunden (siehe `ViewShareSplitEdit`), und bei uneindeutigem
 * Ergebnis bleibt der Haken unverändert — dieselbe Zurückhaltung wie bei
 * `ShareUpdateRules` gegenüber stillen Korrekturen gespeicherter Daten.
 *
 * @note Bei Split-Verhältnissen nah bei 1 (z. B. 5:4) liegen die beiden
 * Toleranzbänder (um 1,0 und um den Faktor) nah beieinander oder überlappen
 * sich — das Ergebnis fällt dann bewusst häufiger auf `Ambiguous`, statt auf
 * Verdacht zu raten.
 *
 * ### Gegenprobe des Verhältnisses (Punkt 3, 22.08.2026)
 *
 * Seit dem 22.08.2026 beantwortet `detect()` eine zweite Frage: passt der
 * gemessene Sprung womöglich BESSER zu einem anderen Verhältnis als zu dem
 * eingetragenen? Die Toleranzbänder oben taugen dafür nicht — sie sind mit
 * ±20 % so weit, dass bei eingetragenen 19 auch ein gemessener Sprung von
 * 19,98 noch als Treffer durchgeht. Genau das ist der Feldfall Alphabet.
 *
 * Verglichen wird deshalb feiner: das nächstgelegene SAUBERE Verhältnis zum
 * gemessenen Sprung (nächste ganze Zahl, bei Reverse-Splits deren Kehrwert)
 * muss innerhalb von `kRatioMatchTolerance` liegen UND der eingetragene
 * Faktor ausserhalb. Passt der eingetragene selbst gut, gibt es nichts zu
 * melden; passt keiner von beiden, wäre eine Zahl geraten.
 *
 * Das ist die einzige Stufe der Split-Plausibilitätsprüfung, die ein ZU
 * GROSSES Verhältnis bemerken kann: geprüft wird gegen eine externe Messung
 * und nicht gegen die eigenen Belege, und ein zu grosses Verhältnis erzeugt
 * dort nie eine Unterdeckung (siehe ARCHITECTURE.md,
 * "Plausibilitätsprüfung des Split-Verhältnisses").
 */
class SplitPriceJumpDetector
{
public:
    SplitPriceJumpDetector() = delete;

    /// Ergebnis der Prüfung.
    enum class Result
    {
        Adjusted,          ///< Kein Sprung erkannt — Historie scheint bereits bereinigt.
        NotAdjusted,       ///< Sprung um ~Faktor erkannt — Historie scheint unbereinigt.
        Ambiguous,         ///< Kurse vorhanden, aber weder eindeutig Sprung noch eindeutig kein Sprung.
        InsufficientData,  ///< Auf einer oder beiden Seiten keine Kursdaten im Suchfenster gefunden.
    };

    /// Vollständiges Ergebnis inkl. der zum Vergleich herangezogenen Kurse.
    struct Outcome
    {
        Result result = Result::InsufficientData;

        QDate  dateBefore;          ///< Invalide, wenn kein Kurs vor dem Ex-Tag gefunden wurde.
        QDate  dateAfter;           ///< Invalide, wenn kein Kurs nach dem Ex-Tag gefunden wurde.
        double priceBefore = 0.0;   ///< Schlusskurs an dateBefore.
        double priceAfter  = 0.0;   ///< Schlusskurs an dateAfter.
        double observedRatio = 0.0; ///< priceBefore / priceAfter; 0.0, wenn nicht bestimmbar.

        /**
         * @brief true, wenn der gemessene Sprung besser zu einem anderen
         * Verhältnis passt als zu dem übergebenen Faktor.
         *
         * Nur bei `NotAdjusted` und `Ambiguous` gesetzt — bei `Adjusted` ist
         * der Kurs praktisch gleich geblieben, es gibt gar keinen Sprung, an
         * dem sich ein Verhältnis messen liesse.
         */
        bool   ratioMismatch = false;

        /// Das besser passende Verhältnis als Faktor (nur bei ratioMismatch).
        double impliedFactor = 0.0;
    };

    /// Standard-Suchfenster in Kalendertagen vor/nach dem Ex-Tag, siehe detect().
    static constexpr int kDefaultMaxLookbackDays = 15;

    /**
     * @brief Relative Toleranz, innerhalb derer ein gemessener Sprung als
     * Treffer für ein Verhältnis gilt (Gegenprobe, siehe Klassenkopf).
     *
     * Deutlich enger als die Toleranzbänder der Ja/Nein-Einordnung: 19 und 20
     * liegen nur gut 5 % auseinander, Schlusskurse zweier aufeinander
     * folgender Tage schwanken für sich schon um ein bis zwei Prozent. 3 %
     * trennt den Feldfall sicher, ohne bei unruhigen Kursen zu raten. Ein
     * 5:4-Split (1,25 gegen 1,5) liegt ausserhalb dessen, was sich so
     * unterscheiden lässt — dort schweigt die Gegenprobe.
     */
    static constexpr double kRatioMatchTolerance = 0.03;

    /**
     * @brief Führt die Prüfung durch.
     *
     * @param dailyValues        Kurshistorie der betroffenen Aktie, beliebiger
     *        Umfang und beliebige Reihenfolge — wird intern gefiltert.
     * @param exDate             Ex-Tag des zu prüfenden Splits.
     * @param factor             Erwarteter Umrechnungsfaktor
     *        (`ShareSplitObject::factor()` des zu prüfenden Splits).
     * @param previousSplitDate  Datum des nächstfrüheren ANDEREN Splits
     *        derselben Aktie; invalide, wenn keiner existiert. Begrenzt das
     *        Suchfenster nach hinten, damit ein Nachbar-Split das Ergebnis
     *        nicht verfälscht.
     * @param nextSplitDate      Datum des nächstspäteren ANDEREN Splits
     *        derselben Aktie; invalide, wenn keiner existiert. Begrenzt das
     *        Suchfenster nach vorn (inklusive: Kurse GENAU an diesem Datum
     *        liegen noch auf der Skala vor dem Nachbar-Split).
     * @param maxLookbackDays    Suchfenster in Kalendertagen vor/nach @p exDate,
     *        zusätzlich zur Begrenzung durch Nachbar-Splits.
     */
    static Outcome detect(const QList<DailyValuesObject>& dailyValues,
                          const QDate& exDate,
                          double factor,
                          const QDate& previousSplitDate = QDate(),
                          const QDate& nextSplitDate = QDate(),
                          int maxLookbackDays = kDefaultMaxLookbackDays);

private:
    /**
     * @brief Nächstgelegenes "sauberes" Verhältnis zu einem gemessenen Sprung.
     *
     * Für @p observedRatio >= 1 die nächste ganze Zahl (19,98 -> 20), darunter
     * — Reverse-Split, der Kurs STEIGT nach dem Ex-Tag — der Kehrwert der
     * nächsten ganzen Zahl (0,098 -> 1/10).
     *
     * @return 0,0, wenn kein sinnvoller Kandidat existiert (insbesondere bei
     *         Verhältnissen nahe 1, wo gar kein Split vorläge).
     */
    static double nearestCleanFactor(double observedRatio);

    static QDate windowStart(const QDate& exDate, const QDate& previousSplitDate,
                             int maxLookbackDays);
    static QDate windowEnd(const QDate& exDate, const QDate& nextSplitDate,
                           int maxLookbackDays);
};

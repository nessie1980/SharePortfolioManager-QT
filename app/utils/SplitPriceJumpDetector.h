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
    };

    /// Standard-Suchfenster in Kalendertagen vor/nach dem Ex-Tag, siehe detect().
    static constexpr int kDefaultMaxLookbackDays = 15;

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
    static QDate windowStart(const QDate& exDate, const QDate& previousSplitDate,
                             int maxLookbackDays);
    static QDate windowEnd(const QDate& exDate, const QDate& nextSplitDate,
                           int maxLookbackDays);
};

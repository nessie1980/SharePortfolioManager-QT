// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../models/BuyObject.h"
#include "../models/ShareSplitObject.h"

#include <QDate>
#include <QList>

/**
 * @brief Ergebnis der Split-Deutung einer Mengen-Unterdeckung.
 *
 * Alle Stueckzahlen liegen auf HEUTIGER Skala vor — nur dort sind Belege
 * unterschiedlicher Datumsstaende ueberhaupt vergleichbar.
 */
struct SplitRatioSuspicion
{
    /**
     * @brief true, wenn ueberhaupt ein Split zwischen den Kaeufen und dem
     * Stichtag liegt und damit als Ursache der Unterdeckung in Frage kommt.
     *
     * false heisst: die Unterdeckung hat mit Splits nichts zu tun (es liegt
     * keiner dazwischen, oder es gibt gar keine offenen Kaeufe). Der Aufrufer
     * darf dann keinen Split-Hinweis zeigen.
     */
    bool hasSuspicion = false;

    /// Splits zwischen dem aeltesten offenen Kauf und dem Stichtag, aufsteigend nach Datum.
    QList<ShareSplitObject> splitsBetween;

    /**
     * @brief true, wenn ein konkretes Ersatz-Verhaeltnis genannt werden darf.
     *
     * Nur gesetzt, wenn GENAU EIN Split dazwischenliegt und die Rueckrechnung
     * exakt auf das um eins groessere Verhaeltnis fuehrt — siehe
     * SplitRatioChecker, Abschnitt "Wann ein Verhaeltnis vorgeschlagen wird".
     * Der Vorschlag bezieht sich immer auf `splitsBetween.first()`.
     */
    bool hasProposal = false;

    /// Vorgeschlagene neue Seite des Verhaeltnisses (nur bei hasProposal).
    double proposedRatioNew = 0.0;

    /// Vorgeschlagene alte Seite des Verhaeltnisses (nur bei hasProposal, immer 1,0).
    double proposedRatioOld = 0.0;

    /**
     * @brief Verfuegbare Menge, die sich mit dem vorgeschlagenen Verhaeltnis
     * ergaebe (heutige Skala, nur bei hasProposal).
     *
     * Entspricht konstruktionsbedingt der angeforderten Menge — genannt wird
     * sie trotzdem, weil erst die Zahl im Meldungstext zeigt, dass die
     * Rechnung damit exakt aufgeht.
     */
    double proposedAvailableToday = 0.0;
};

/**
 * @brief Deutet eine bereits festgestellte Mengen-Unterdeckung als moeglichen
 * Fehler im Split-Verhaeltnis.
 *
 * Zustandslos und vollstaendig datenbankfrei — Kaeufe und Splits kommen als
 * Parameter herein, gleicher Stil wie ShareSplitAdjuster, SaleFifoAllocator,
 * SplitPriceJumpDetector und DividendVolumeChecker. Siehe ARCHITECTURE.md,
 * "Plausibilitaetspruefung des Split-Verhaeltnisses".
 *
 * ### Was diese Klasse NICHT tut
 *
 * Sie stellt die Unterdeckung nicht selbst fest. Das macht weiterhin
 * SaleFifoAllocator::isSaleVolumeCovered(); erst wenn dessen Ergebnis
 * negativ ist, beantwortet diagnose() die Anschlussfrage: liegt ein Split
 * dazwischen, und welches Verhaeltnis wuerde die Rechnung aufgehen lassen?
 *
 * ### Der Anlass
 *
 * Bankmitteilungen nennen das Zuteilungsverhaeltnis als "1:19" — je einem
 * gehaltenen Stueck werden 19 ZUSAETZLICHE eingebucht. Die Anwendung
 * erwartet das Umrechnungsverhaeltnis, hier also 20:1. Im Feldfall Alphabet
 * wurde 19 eingetragen; der Fehler ist systematisch immer genau eins zu
 * klein (siehe ARCHITECTURE.md, "Split-Verhaeltnis: Notation der
 * Bankmitteilungen").
 *
 * Sichtbar wird er erst beim naechsten Verkauf: 10 gekaufte Stueck ergeben
 * mit Faktor 19 heute 190 Stueck, der Verkaufsbeleg lautet aber auf 200.
 * Die Mengenpruefung blockiert das Speichern zu Recht — ohne Deutung liegt
 * aber nahe, die 200 auf 190 zu "korrigieren" und damit den Beleg zu
 * verfaelschen, statt den Split zu berichtigen.
 *
 * ### Rueckrechnung
 *
 * Die verfuegbare Menge auf heutiger Skala ist
 *
 *     availToday = A_vor * f + A_nach
 *
 * mit A_vor als Beitrag der Kaeufe VOR dem Split (bereits ueber die uebrigen
 * Splits skaliert), f als eingetragenem Faktor und A_nach als Beitrag der
 * Kaeufe ab dem Splittag. Gesucht ist das f', fuer das availToday genau der
 * angeforderten Menge entspricht:
 *
 *     f' = f * (requiredVolumeToday - A_nach) / A_vor
 *
 * Feldfall: f' = 19 * 200 / 190 = 20, exakt.
 *
 * ### Wann ein Verhaeltnis vorgeschlagen wird
 *
 * Bewusst eng, und das ist der wichtigste Teil dieser Klasse. Dieselbe Formel
 * liefert naemlich auch dann ein sauberes Ergebnis, wenn gar kein Split-Fehler
 * vorliegt: tippt jemand 2.000 statt 200 Stueck, kommt f' = 190 heraus — ein
 * astreines Verhaeltnis 190:1, das den Benutzer auf eine voellig falsche
 * Faehrte fuehrte. Ein Vorschlag entsteht deshalb nur, wenn ALLE vier
 * Bedingungen zutreffen:
 *
 * 1. Genau ein Split liegt zwischen Kaeufen und Stichtag. Bei mehreren ist
 *    nicht zuzuordnen, welcher gemeint waere.
 * 2. Dessen alte Seite ist 1 — nur so entsteht die Delta-vs-Gesamt-
 *    Verwechslung der Bankmitteilung ueberhaupt.
 * 3. Es ist kein Reverse-Split (neue Seite >= 1). Bei Reverse-Splits gibt es
 *    die Verwechslung nicht.
 * 4. f' ist exakt die eingetragene neue Seite PLUS EINS. Genau das ist der
 *    dokumentierte, systematische Fehler.
 *
 * Trifft eine davon nicht zu, bleibt hasProposal false und der Aufrufer
 * nennt nur die dazwischenliegenden Splits, ohne eine Zahl zu behaupten.
 * Lieber kein Vorschlag als ein irrefuehrender.
 */
class SplitRatioChecker
{
public:
    SplitRatioChecker() = delete;

    /// Mengen unterhalb dieser Schwelle gelten als null (wie in SaleFifoAllocator).
    static constexpr double kVolumeEpsilon = 1e-9;

    /// Relative Toleranz beim Vergleich zurueckgerechneter Verhaeltnisse.
    static constexpr double kRatioTolerance = 1e-6;

    /**
     * @brief Deutet eine Unterdeckung als moeglichen Split-Fehler.
     *
     * @param requiredVolumeToday Angeforderte Menge auf heutiger Skala (im
     *        Verkaufsformular: die Verkaufsmenge ueber
     *        ShareSplitAdjuster::adjustedVolume()).
     * @param referenceDate       Stichtag, gegen den geprueft wird (das
     *        Verkaufsdatum). Splits NACH diesem Tag skalieren beide Seiten
     *        des Vergleichs gleich und bleiben deshalb aussen vor.
     * @param availableBuys       Verfuegbare Kaeufe des gewaehlten Depots,
     *        dieselbe Liste, die auch in SaleFifoAllocator geht.
     * @param splits              Alle Splits der Aktie.
     * @return Deutung; `hasSuspicion == false`, wenn kein Split als Ursache
     *         in Frage kommt.
     */
    static SplitRatioSuspicion diagnose(double                         requiredVolumeToday,
                                        const QDate&                   referenceDate,
                                        const QList<BuyObject>&        availableBuys,
                                        const QList<ShareSplitObject>& splits);
};

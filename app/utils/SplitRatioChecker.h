// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../models/BuyObject.h"
#include "../models/SaleObject.h"
#include "../models/ShareSplitObject.h"

#include <QDate>
#include <QList>
#include <QString>

/**
 * @brief Ein Stueckzahl-Posten in Beleg-Skala seines eigenen Datums.
 *
 * Eingabe des eigentlichen Rechenkerns. Bewusst nur die beiden Groessen, die
 * fuer die Umrechnung gebraucht werden — das Datum bestimmt den anzuwendenden
 * Split-Faktor, die Stueckzahl den Beitrag. Preise, Gebuehren und Steuern
 * spielen keine Rolle: ein Split laesst sie unberuehrt.
 *
 * Was in `volume` steht, haengt vom Aufrufer ab und ist bewusst offen
 * gelassen: das Verkaufsformular reicht Restbestaende herein (`volume() -
 * volumeSold()`), der Split-Dialog volle Kaufmengen, weil er die Verkaeufe
 * selbst als eigene Posten fuehrt. Wuerde die Klasse das festlegen, muesste
 * einer der beiden Aufrufer seine Daten verbiegen.
 */
struct SplitVolumeLot
{
    QDate  date;            ///< Datum des Belegs
    double volume = 0.0;    ///< Stueckzahl in der Beleg-Skala dieses Datums
};

/**
 * @brief Ergebnis der Split-Deutung einer Mengen-Unterdeckung.
 *
 * Alle Stueckzahlen liegen auf HEUTIGER Skala vor — nur dort sind Belege
 * unterschiedlicher Datumsstaende ueberhaupt vergleichbar.
 */
struct SplitRatioSuspicion
{
    /**
     * @brief true, wenn ueberhaupt ein Split zwischen den Posten und dem
     * Stichtag liegt und damit als Ursache der Unterdeckung in Frage kommt.
     *
     * false heisst: die Unterdeckung hat mit Splits nichts zu tun (es liegt
     * keiner dazwischen, oder es gibt gar keine Posten). Der Aufrufer darf
     * dann keinen Split-Hinweis zeigen.
     */
    bool hasSuspicion = false;

    /// Splits zwischen dem aeltesten Posten und dem Stichtag, aufsteigend nach Datum.
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
 * @brief Erster Verkauf, der nach Anwendung einer Split-Liste nicht mehr
 * durch die vorangegangenen Kaeufe gedeckt ist.
 *
 * Ergebnis von SplitRatioChecker::checkAgainstHistory(). Ein negativer
 * Bestand ist kein Verdacht, sondern ein Widerspruch in den Daten: Stuecke,
 * die nie gekauft wurden, koennen nicht verkauft worden sein.
 */
struct SplitHistoryConflict
{
    bool    hasConflict = false;

    /// Depot, in dem die Unterdeckung auftritt (getrimmt; leer ist ein gueltiger Wert).
    QString depotNumber;

    /// Datum des ersten nicht gedeckten Verkaufs.
    QDate   conflictDate;

    /// Bis dahin verkaufte Gesamtmenge, heutige Skala.
    double  requiredToday = 0.0;

    /// Bis dahin gekaufte Gesamtmenge, heutige Skala.
    double  availableToday = 0.0;

    /**
     * @brief Deutung an dieser Stelle, bereits ueber diagnose() ermittelt.
     *
     * Beim Speichern eines Splits traegt sie den Verhaeltnis-Vorschlag. Beim
     * Loeschen ignoriert der Aufrufer sie: dort ist der fragliche Split gar
     * nicht mehr in der Liste, ein Vorschlag zu einem ANDEREN Split waere im
     * Loeschdialog nur verwirrend.
     */
    SplitRatioSuspicion suspicion;
};

/**
 * @brief Prueft Split-Verhaeltnisse gegen die eigenen Kauf- und
 * Verkaufsbelege und deutet Mengen-Unterdeckungen als moeglichen Fehler im
 * Verhaeltnis.
 *
 * Zustandslos und vollstaendig datenbankfrei — alle Daten kommen als
 * Parameter herein, gleicher Stil wie ShareSplitAdjuster, SaleFifoAllocator,
 * SplitPriceJumpDetector und DividendVolumeChecker. Siehe ARCHITECTURE.md,
 * "Plausibilitaetspruefung des Split-Verhaeltnisses".
 *
 * ### Zwei Einstiege, ein Rechenweg
 *
 * `diagnose()` deutet eine BEREITS FESTGESTELLTE Unterdeckung. Das
 * Verkaufsformular ruft sie direkt, denn dort stellt
 * SaleFifoAllocator::isSaleVolumeCovered() die Unterdeckung fest.
 *
 * `checkAgainstHistory()` sucht die Unterdeckung erst — der Split-Dialog
 * kennt keinen einzelnen Verkauf, sondern nur die gesamte Historie. Sie
 * fuehrt je Depot einen Bestandsverlauf und ruft an der ersten Fundstelle
 * `diagnose()`. Ein zweiter Rechenweg entsteht dadurch nicht.
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
 * ### Rueckrechnung
 *
 * Die verfuegbare Menge auf heutiger Skala ist
 *
 *     availToday = A_vor * f + A_nach
 *
 * mit A_vor als Beitrag der Posten VOR dem Split (bereits ueber die uebrigen
 * Splits skaliert), f als eingetragenem Faktor und A_nach als Beitrag der
 * Posten ab dem Splittag. Gesucht ist das f', fuer das availToday genau der
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
 * 1. Genau ein Split liegt zwischen Posten und Stichtag. Bei mehreren ist
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
     * @param requiredVolumeToday Angeforderte Menge auf heutiger Skala.
     * @param referenceDate       Stichtag, gegen den geprueft wird. Splits
     *        NACH diesem Tag skalieren beide Seiten des Vergleichs gleich und
     *        bleiben deshalb aussen vor.
     * @param lots                Verfuegbare Posten in Beleg-Skala.
     * @param splits              Alle Splits der Aktie.
     * @return Deutung; `hasSuspicion == false`, wenn kein Split als Ursache
     *         in Frage kommt.
     */
    static SplitRatioSuspicion diagnose(double                         requiredVolumeToday,
                                        const QDate&                   referenceDate,
                                        const QList<SplitVolumeLot>&   lots,
                                        const QList<ShareSplitObject>& splits);

    /**
     * @brief Bequemlichkeits-Ueberladung fuer verfuegbare Kaeufe.
     *
     * Bildet je Kauf einen Posten aus `volume() - volumeSold()` und ruft die
     * Lot-Variante. Aufrufer ist PresenterSaleEdit, dem genau diese Liste
     * ohnehin vorliegt (`currentAvailableBuys()`).
     */
    static SplitRatioSuspicion diagnose(double                         requiredVolumeToday,
                                        const QDate&                   referenceDate,
                                        const QList<BuyObject>&        availableBuys,
                                        const QList<ShareSplitObject>& splits);

    /**
     * @brief Sucht den ersten Verkauf, der unter @p splits nicht mehr durch
     * die vorangegangenen Kaeufe gedeckt ist.
     *
     * Je Depot ein eigener Bestandsverlauf (Depotnummern getrimmt
     * verglichen, wie in DividendVolumeChecker; Belege ohne Depotnummer
     * bilden eine eigene Gruppe). Kaeufe zaehlen mit voller `volume()`, die
     * Verkaeufe fuehrt der Verlauf selbst — `volumeSold()` bleibt hier
     * ausdruecklich aussen vor, sonst waeren die Verkaeufe doppelt abgezogen.
     *
     * Ein Kauf AM Tag eines Verkaufs zaehlt noch mit: sonst meldete ein
     * Kauf-und-Verkauf am selben Tag faelschlich eine Unterdeckung.
     *
     * @param splits    Die RESULTIERENDE Split-Liste, also beim Speichern
     *        einschliesslich des neuen bzw. geaenderten Splits, beim Loeschen
     *        ohne den zu entfernenden.
     * @param fromDate  Ex-Tag des betroffenen Splits. Unterdeckungen VOR
     *        diesem Tag bleiben aussen vor — dort skalieren alle Belege
     *        gleich, das Verhaeltnis kann daran nichts aendern.
     * @param buys      Alle Kaeufe der Aktie.
     * @param sales     Alle Verkaeufe der Aktie.
     * @return Die frueheste Fundstelle ueber alle Depots; bei gleichem Datum
     *         gewinnt das alphabetisch erste Depot, damit das Ergebnis
     *         reproduzierbar ist.
     */
    static SplitHistoryConflict checkAgainstHistory(const QList<ShareSplitObject>& splits,
                                                    const QDate&                   fromDate,
                                                    const QList<BuyObject>&        buys,
                                                    const QList<SaleObject>&       sales);
};

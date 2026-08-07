// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../models/ShareSplitObject.h"

#include <QDate>
#include <QList>

/**
 * @brief Rechnet Stückzahlen und Kurse zwischen der Beleg-Skala (wie in
 * `buys`/`sales`/`daily_values` gespeichert) und der heutigen Skala (nach
 * allen bekannten Splits) um.
 *
 * Zustandslos und vollständig datenbankfrei — Splits kommen als Parameter
 * herein, damit die Klasse ohne Datenbank testbar ist, im selben Stil wie
 * `PortfolioSeriesCalculator`. Grundlage: ARCHITECTURE.md, "Offene Punkte",
 * "Aktiensplits werden nicht behandelt".
 *
 * ### Grundinvariante
 *
 * Für einen Datensatz mit Datum `d` gilt:
 *
 * | Grösse | Umrechnung |
 * |--------|------------|
 * | Stückzahl | v_heute = v_beleg × volumeFactor(splits, d) |
 * | Preis je Stück (Transaktion) | p_heute = p_beleg / volumeFactor(splits, d) |
 * | Preis je Stück (Tageswert, unbereinigt) | p_heute = p_beleg / priceFactorForHistory(splits, d) |
 *
 * Der Wert v × p bleibt durch einen Split unverändert — ein Split schafft
 * keinen Gewinn und keinen Verlust, er zerlegt oder bündelt nur die
 * Stückelung. Deshalb reicht ein einziger Faktor pro Grössenpaar, und
 * Einzahlung, Kaufwert, Verkaufserlös, Gebühren, Steuern und
 * Dividendensummen bleiben von einem Split gänzlich unberührt.
 *
 * ### Zwei Faktoren, ein Grund
 *
 * Transaktionen (`buys`/`sales`) liegen immer in der Beleg-Skala vor —
 * `volumeFactor()` kumuliert daher über ALLE Splits nach `d`. Tageswerte
 * können dagegen bereits vom Anbieter split-bereinigt geliefert worden
 * sein; `priceFactorForHistory()` kumuliert deshalb nur über Splits, deren
 * `pricesAdjusted() == false` ist — ein bereits bereinigter Split zeigt in
 * der Kurshistorie keinen Sprung mehr und darf daher nicht zusätzlich
 * herausgerechnet werden (sonst der exakte Fehler, der den Alphabet-Fall in
 * ARCHITECTURE.md verursacht hat, nur mit umgekehrtem Vorzeichen).
 */
class ShareSplitAdjuster
{
public:
    ShareSplitAdjuster() = delete;

    /**
     * @brief Kumulierter Stückzahl-Faktor zum Stichtag @p date.
     *
     * Produkt von `factor()` aller Splits mit einem Datum ECHT NACH
     * @p date. Ein Split am Stichtag selbst zählt noch nicht mit — der
     * Beleg des Splittags liegt fachlich vor dem Split.
     *
     * @param splits  Alle Splits der betroffenen Aktie, beliebige Reihenfolge.
     * @param date    Datum des umzurechnenden Datensatzes.
     * @return Faktor > 0; 1.0, wenn kein späterer Split existiert.
     */
    static double volumeFactor(const QList<ShareSplitObject>& splits, const QDate& date);

    /**
     * @brief Kumulierter Kurs-Faktor für die Tageswert-Historie zum Stichtag @p date.
     *
     * Wie volumeFactor(), aber Splits mit `pricesAdjusted() == true` tragen
     * nichts bei — ihre Kurshistorie liegt bereits in heutigen Stücken vor.
     *
     * @param splits  Alle Splits der betroffenen Aktie, beliebige Reihenfolge.
     * @param date    Datum des umzurechnenden Tageswerts.
     * @return Faktor > 0; 1.0, wenn kein unbereinigter späterer Split existiert.
     */
    static double priceFactorForHistory(const QList<ShareSplitObject>& splits, const QDate& date);

    /**
     * @brief Stückzahl eines Beleg-Datensatzes auf heutige Stücke umgerechnet.
     * @param volume  Stückzahl laut Beleg.
     * @param splits  Alle Splits der betroffenen Aktie.
     * @param date    Datum des Datensatzes.
     */
    static double adjustedVolume(double volume, const QList<ShareSplitObject>& splits,
                                 const QDate& date);

    /**
     * @brief Transaktionspreis (Kauf/Verkauf) auf heutige Stücke umgerechnet.
     * @param price   Preis je Stück laut Beleg.
     * @param splits  Alle Splits der betroffenen Aktie.
     * @param date    Datum der Transaktion.
     */
    static double adjustedTransactionPrice(double price, const QList<ShareSplitObject>& splits,
                                           const QDate& date);

    /**
     * @brief Tageswert-Kurs auf heutige Stücke umgerechnet.
     * @param price   Schlusskurs (oder Open/Top/Bottom) wie in `daily_values` gespeichert.
     * @param splits  Alle Splits der betroffenen Aktie.
     * @param date    Datum des Tageswerts.
     */
    static double adjustedHistoryPrice(double price, const QList<ShareSplitObject>& splits,
                                       const QDate& date);
};

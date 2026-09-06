// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QLocale>
#include <QString>

/**
 * @brief Zentrale Formatierung von Kursen fuer die Anzeige.
 *
 * Ein Kurs ist kein Geldbetrag. Er steht in den Uebersichten und
 * Detail-Dialogen fast immer als Faktor in einer Gleichung -- Anteile mal
 * Kurs ergibt Kurswert -- und wird deshalb mit vier Nachkommastellen
 * angezeigt, genau wie die Stueckzahl daneben. Summen, Gebuehren, Steuern
 * und Prozentwerte bleiben davon unberuehrt und behalten ihre zwei Stellen.
 *
 * Anlass war ein sichtbar falsches Ergebnis im Details-Dialog des
 * Verkaufsformulars (siehe ARCHITECTURE.md, "Kurs-Anzeige durchgaengig mit
 * vier Nachkommastellen"): 200,0000 Stk. mal angezeigte 48,59 EUR ergibt
 * 9.718,00 EUR, in der Summenspalte stand aber korrekt 9.719,00 EUR, weil
 * der tatsaechliche Kurs 48,595 EUR betraegt. Dieselbe Gleichung mit
 * demselben Fehler stand an mehreren weiteren Stellen der Anwendung.
 *
 * @note Bewusst header-only, gleiche Bauweise wie ShareUpdateRules.h: zwei
 * zustandslose Einzeiler ohne Abhaengigkeiten. Eine eigene
 * Uebersetzungseinheit muesste in jedes Testziel aufgenommen werden, das
 * irgendeine Form anfasst -- der Aufwand stuende in keinem Verhaeltnis.
 *
 * @note Die Funktionen liefern die nackte Zahl ohne Einheit. Das Anhaengen
 * von " EUR" bleibt bei den Aufrufstellen, ebenso ein etwaiges
 * Vorzeichen-Praefix, da beides je Tabelle und Zeile unterschiedlich
 * gehandhabt wird.
 *
 * @note QLocale() ohne Argument verwendet die per QLocale::setDefault()
 * gesetzte Standard-Locale. Diese wird in main() und in jedem Test-main()
 * auf Deutsch gesetzt -- siehe ARCHITECTURE.md, "System-Locale-abhaengiges
 * Zahlenformat".
 */
class ValueFormatter
{
public:
    ValueFormatter() = delete;

    /**
     * @brief Kurs einer Aktie, vier Nachkommastellen.
     *
     * Gilt auch fuer Groessen, die auf der Kurs-Skala liegen, ohne selbst
     * ein Kurs zu sein: die Differenz zweier Kurse (Vortagsentwicklung je
     * Aktie) und die Dividende je Anteil. Beide stehen in denselben
     * Gleichungen wie ein Kurs und muessen deshalb dieselbe Genauigkeit
     * haben, sonst geht die Zeile nicht auf.
     */
    static QString formatPrice(double value)
    {
        return QLocale().toString(value, 'f', 4);
    }

    /**
     * @brief Kurs fuer ein Eingabefeld, vier Nachkommastellen, OHNE
     * Tausendertrennzeichen.
     *
     * Gegenstueck zu formatPrice() fuer Werte, die in ein QLineEdit
     * geschrieben und von dort wieder eingelesen werden. In einer Tabelle
     * ist das Trennzeichen eine Lesehilfe; in einem Eingabefeld ist es ein
     * Zeichen, das die Gegenrichtung wieder entfernen muesste.
     *
     * Anlass (06.09.2026): die parseDouble()-Implementierungen der Views
     * ersetzen das Komma durch einen Punkt und rufen toDouble(). Ein
     * stehengebliebenes Trennzeichen macht daraus "1.003.0000", also keine
     * Zahl mehr -- toDouble() scheitert und liefert 0,0. Ein per
     * formatPrice() geschriebener vierstelliger Kurs waere beim naechsten
     * Lesen verschwunden.
     *
     * @note Das ist eine Umgehung, keine Loesung. Das eigentliche Problem
     * sitzt in parseDouble() und betrifft jedes Zahlenfeld, das ueber
     * formatMoney()/formatVolume() befuellt wird -- siehe ARCHITECTURE.md,
     * "Zahlenfelder verlieren Werte ab 1.000 beim Zuruecklesen".
     */
    static QString formatPriceForInput(double value)
    {
        QLocale loc;
        loc.setNumberOptions(QLocale::OmitGroupSeparator);
        return loc.toString(value, 'f', 4);
    }

    /**
     * @brief Devisenkurs (Umrechnungsverhaeltnis), vier Nachkommastellen.
     *
     * Bewusst eine eigene Funktion, obwohl sie heute dasselbe liefert wie
     * formatPrice(): ein Devisenkurs ist fachlich etwas anderes als ein
     * Aktienkurs (Nessies Vorgabe, 05.09.2026). Aendert sich spaeter eine
     * der beiden Konventionen, zieht die andere nicht ungewollt mit.
     */
    static QString formatExchangeRate(double value)
    {
        return QLocale().toString(value, 'f', 4);
    }
};

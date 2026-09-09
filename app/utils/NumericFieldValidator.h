// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QDoubleValidator>
#include <QObject>

/**
 * @brief Erzeugt den Validator fuer ein Zahlen-Eingabefeld.
 *
 * Anlass (07.09.2026): `QDoubleValidator` steht ohne Zutun auf
 * `ScientificNotation`. Er haelt damit "2,00E+03" fuer eine gueltige Zahl --
 * und erzeugt sie auch selbst. `QLineEdit` ruft beim Verlassen des Feldes
 * `fixup()` des Validators auf, wenn der Text nicht vollstaendig gueltig ist;
 * Qt liest ihn dann mit der Locale des Validators und schreibt ihn neu.
 *
 * Im Feldfall wurde aus der Eingabe "20.02" auf diesem Weg "2,00E+03": der
 * Punkt galt als Tausendertrennzeichen, die Gruppengroesse wird dabei nicht
 * geprueft, es kam 2002 heraus, und die Ausgabe mit zwei Nachkommastellen in
 * wissenschaftlicher Notation ergab 2,00E+03. Aus 20,02 EUR wurden 2000 EUR,
 * ohne Meldung. Siehe ARCHITECTURE.md, "Wissenschaftliche Notation in
 * Zahlenfeldern".
 *
 * Kurse, Gebuehren und Stueckzahlen sind keine physikalischen Messwerte --
 * wissenschaftliche Notation hat in dieser Anwendung nichts zu suchen. Diese
 * Fabrik setzt deshalb ausnahmslos `StandardNotation`.
 *
 * @note Zentral und nicht je View, obwohl es nur eine Zeile ist: es gibt 31
 * Validator-Stellen in sechs Formularen. Eine davon zu vergessen faellt
 * niemandem auf, bis jemand eine Zahl eintippt, die der Validator umdeutet --
 * dieselbe Erfahrung wie mit den sieben `parseDouble()`-Kopien (siehe
 * NumberParser.h).
 *
 * Seit 08.09.2026 setzt die Fabrik zusaetzlich
 * `QLocale::RejectGroupSeparator`. Ohne das deutete der Validator einen
 * Punkt weiterhin als Tausendertrennzeichen um -- ohne die Gruppengroesse
 * zu pruefen: aus "20.02" wurde "2002,00", stillschweigend um den Faktor
 * 100 daneben. Jetzt ist der Punkt in einem Zahlenfeld gar keine zulaessige
 * Eingabe mehr: er laesst sich nicht tippen, Einfuegen wird abgewiesen, und
 * `fixup()` hat nichts umzuschreiben.
 *
 * Damit gilt in Eingabefeldern genau die Regel, die `NumberParser` beim
 * Lesen anwendet -- vorher setzte der Validator sie ausser Kraft, weil er
 * dem Parser zuvorkam.
 *
 * @note Die Kehrseite: was die Anwendung selbst in ein Eingabefeld
 * schreibt, muss ebenfalls ohne Trennzeichen auskommen, sonst waere ihr
 * eigener Text fuer diesen Validator ungueltig. Alle editierbaren
 * Zahlenfelder laufen deshalb ueber `ValueFormatter::formatForInput()`. In
 * Tabellen und Uebersichten bleibt das Trennzeichen -- dort ist es eine
 * Lesehilfe und wird nie zurueckgelesen.
 *
 * @param bottom    Kleinster zulaessiger Wert.
 * @param top       Groesster zulaessiger Wert.
 * @param decimals  Zugelassene Nachkommastellen.
 * @param parent    Eigentuemer des Validators, ueblicherweise das QLineEdit.
 */
inline QDoubleValidator* makeNumericValidator(double   bottom,
                                              double   top,
                                              int      decimals,
                                              QObject* parent)
{
    auto* validator = new QDoubleValidator(bottom, top, decimals, parent);
    validator->setNotation(QDoubleValidator::StandardNotation);

    QLocale strict;
    strict.setNumberOptions(QLocale::RejectGroupSeparator);
    validator->setLocale(strict);

    return validator;
}

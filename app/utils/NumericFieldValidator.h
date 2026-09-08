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
 * @note NICHT gesetzt ist `QLocale::RejectGroupSeparator`. Der Validator
 * deutet einen Punkt deshalb weiterhin als Tausendertrennzeichen um, aus
 * "20.02" wird jetzt "2002,00" statt "2,00E+03" -- die Anzeige ist nicht mehr
 * abwegig, der Wert aber immer noch stillschweigend falsch. Der Grund ist
 * Nessies bewusste Reihenfolge (07.09.2026): erst die Notation, spaeter das
 * Trennzeichen, weil letzteres verlangt, dass die Eingabefelder auch
 * durchgaengig OHNE Trennzeichen befuellt werden. Siehe ARCHITECTURE.md,
 * "Offene Punkte", "Tausendertrennzeichen in Eingabefeldern".
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
    return validator;
}

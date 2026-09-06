// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QLocale>
#include <QString>

/**
 * @brief Wandelt den Text eines Zahlen-Eingabefeldes in einen double um.
 *
 * Gilt fuer die Eingabefelder der Formulare, also fuer Text, den entweder der
 * Benutzer getippt oder die Anwendung selbst geschrieben hat. NICHT fuer
 * Rohwerte aus Bankbelegen -- die uebersetzt
 * `DocumentFieldValue::forNumericField()` vorher in die deutsche
 * Schreibweise. Die Arbeitsteilung ist wichtig, siehe unten.
 *
 * Anlass (06.09.2026): fuenf der sechs Formulare hatten eine eigene,
 * zeichengleiche `parseDouble()`, die das Komma durch einen Punkt ersetzte
 * und `toDouble()` aufrief. Ein Tausendertrennzeichen blieb dabei stehen --
 * aus "1.003,00" wurde "1.003.00", also keine Zahl mehr, und die Funktion
 * lieferte stillschweigend 0,0. Eine gespeicherte Dividende mit einem Kurs
 * von 1.003,00 EUR wurde korrekt angezeigt und beim naechsten Speichern auf
 * null zurueckgeschrieben, ohne Meldung. Siehe ARCHITECTURE.md,
 * "Zahlenfelder verlieren Werte ab 1.000 beim Zuruecklesen".
 *
 * @note Die sechste Kopie, `ViewShareSplitEdit::parseDouble()`, war als
 * einzige richtig: sie ging ueber `QLocale::toDouble()`. Ihr Kommentar
 * behauptete allerdings, sie folge derselben Konvention wie
 * `ViewBrokerageEdit` -- was nicht stimmte. Diese Klasse verallgemeinert den
 * funktionierenden Weg, sie erfindet nichts Neues.
 *
 * Streng deutsche Auslegung (Nessies Entscheidung 06.09.2026): der Punkt ist
 * immer Tausendertrennzeichen, das Komma immer Dezimaltrenner. "1.003" sind
 * eintausenddrei, nicht eins Komma null null drei. `QLocale::toDouble()`
 * prueft die Gruppierung dabei mit -- "204.71" und "1.5" sind keine
 * gueltigen deutschen Zahlen und werden als unlesbar gemeldet, statt zu
 * 20471 beziehungsweise 15 zu werden. Ein stillschweigender Faktor 1000 bei
 * einem Geldbetrag ist das Schlimmste, was hier passieren kann; ein
 * gemeldeter Fehlschlag ist das kleinere Uebel. Dieselbe Abwaegung wie bei
 * `DocumentFieldValue::toDate()` und zweistelligen Jahreszahlen.
 *
 * Die C-Schreibweise ("39.998" aus einem englisch formatierten Beleg) muss
 * diese Klasse deshalb nicht koennen: sie erreicht die Felder nie
 * unuebersetzt.
 */
class NumberParser
{
public:
    NumberParser() = delete;

    /**
     * @brief Liest eine deutsch formatierte Zahl.
     *
     * @param text  Feldinhalt; fuehrender und folgender Leerraum wird
     *              entfernt.
     * @param ok    Optional. Erhaelt true, wenn der Text gelesen werden
     *              konnte oder leer war; false bei nicht leerem,
     *              unlesbarem Text.
     * @return Der gelesene Wert, sonst 0,0.
     *
     * @note Ein LEERES Feld gilt als Erfolg mit dem Wert 0,0. Bei den
     * optionalen Feldern -- Gebuehren, Steuern, Rabatt -- ist "nichts
     * eingetragen" gleichbedeutend mit null und darf keine Fehlermeldung
     * ausloesen. Fehlende Pflichtfelder faengt weiterhin
     * `hasMissingRequiredFields()` ab, das auf "> 0,0" prueft.
     */
    static double parse(const QString& text, bool* ok = nullptr)
    {
        const QString trimmed = text.trimmed();

        if (trimmed.isEmpty()) {
            if (ok) *ok = true;
            return 0.0;
        }

        bool converted = false;
        const double value = QLocale().toDouble(trimmed, &converted);

        if (ok) *ok = converted;
        return converted ? value : 0.0;
    }
};

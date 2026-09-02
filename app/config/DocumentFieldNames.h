// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QStringList>

/**
 * @brief Welche Feldnamen aus `Documents.xml` welches Formular verarbeitet.
 *
 * Vier Formulare lesen Belege ein — "Aktie hinzufügen", "Käufe", "Verkäufe"
 * und "Dividenden". Jedes führt zwei Listen: die Namen, die es überhaupt
 * verarbeitet, und die Teilmenge davon, ohne die nicht gespeichert werden
 * kann. Bis zum 02.09.2026 lagen sie als `static const QStringList` in den
 * vier `populateFromResult()`-Implementierungen.
 *
 * ### Warum hier und nicht bei den Presentern
 *
 * Diese Listen beschreiben `Documents.xml`, nicht die Masken — sie gehören
 * neben `DocumentsConfig`. Der praktische Anlass war `tst_documentsxml`: das
 * Testziel prüft die ausgelieferte Konfigurationsdatei und braucht die
 * Listen für die Gegenrichtung (jeder Tag in der Datei muss von einem
 * Formular verarbeitet werden). Lägen sie an den Presentern, müsste es vier
 * Presenter-`.cpp` samt IView, IModel, Models, `ShareSplitAdjuster`,
 * `SaleFifoAllocator`, `PdfTextExtractor` und `Parser` mitkompilieren — für
 * zwei Listenvergleiche. Diese Datei hängt an nichts ausser `QStringList`.
 *
 * ### Was NICHT hierher gehört
 *
 * `xmlNameToViewField()` bleibt bei den Presentern. Die linke Seite dieser
 * Übersetzung beschreibt die Datei, die rechte die jeweilige Maske — und die
 * fällt je Formular anders aus: `Price` heisst im Verkaufsdialog
 * `salePrice`, `DividendRate` im Dividendendialog `rate`. Das ist Wissen der
 * Maske, nicht der Konfiguration.
 *
 * ### Vier Listen, keine Ableitung
 *
 * `shareAddKnown()` ist exakt `buyKnown()` plus `Wkn`, `Isin` und `Name` —
 * das Formular liest einen Kaufbeleg und zusätzlich die Stammdaten. Die
 * Liste wird trotzdem ausgeschrieben und nicht abgeleitet: eine Ableitung
 * würde stillschweigend mitziehen, sobald jemand die Kaufliste erweitert,
 * und niemand käme auf den Gedanken, dabei an "Aktie hinzufügen" zu denken
 * (Nessies Entscheidung, 02.09.2026).
 *
 * @note Die Presenter behalten ihre eigenen `knownXmlNames()` /
 * `requiredXmlNames()` als schlanke Weiterleitungen hierher. Formularcode
 * fragt damit weiterhin sein eigenes Formular, und die Tests der vier
 * Form-Ziele bleiben unverändert.
 *
 * @see ARCHITECTURE.md, "Feldschlüssel-Tabellen sind an keiner Stelle
 * geprüft"
 */
namespace DocumentFieldNames {

// ── ShareAddForm — "Aktie hinzufügen" ────────────────────────────────────────

/** Alle Feldnamen, die "Aktie hinzufügen" verarbeitet. */
const QStringList& shareAddKnown();

/** Pflichtfelder von "Aktie hinzufügen". */
const QStringList& shareAddRequired();

// ── BuysForm — "Käufe hinzufügen / editieren" ────────────────────────────────

/** Alle Feldnamen, die der Kaufdialog verarbeitet. */
const QStringList& buyKnown();

/** Pflichtfelder des Kaufdialogs. */
const QStringList& buyRequired();

// ── SalesForm — "Verkäufe hinzufügen / editieren" ────────────────────────────

/** Alle Feldnamen, die der Verkaufsdialog verarbeitet. */
const QStringList& saleKnown();

/** Pflichtfelder des Verkaufsdialogs. */
const QStringList& saleRequired();

// ── DividendForm — "Dividende hinzufügen / editieren" ────────────────────────

/** Alle Feldnamen, die der Dividendendialog verarbeitet. */
const QStringList& dividendKnown();

/** Pflichtfelder des Dividendendialogs. */
const QStringList& dividendRequired();

} // namespace DocumentFieldNames

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../models/ShareSplitObject.h"

#include <QDate>
#include <QList>
#include <QString>

/**
 * @brief Baut die Split-Hinweistexte für die Editier-Dialoge und die
 * Übersichtstabellen.
 *
 * Phase 3b der Aktiensplit-Behandlung (09.08.2026, siehe ARCHITECTURE.md).
 * Editier-Dialoge zeigen durchgehend den BELEG — die dort eingetragene
 * Stückzahl und der dort eingetragene Preis bleiben unangetastet. Damit der
 * Benutzer trotzdem sieht, wie viele Stücke daraus heute geworden sind,
 * steht unter den Feldern ein Hinweis, den diese Klasse formatiert.
 *
 * Ausgelagert, weil `ViewBuyEdit` und `ViewSaleEdit` denselben Text brauchen.
 * Eine zweite Kopie wäre der direkte Weg zurück zu dem Problem, das Phase 2c
 * mit der dreifach duplizierten FIFO-Schleife aufgeräumt hat.
 *
 * Phase 3c (10.08.2026) ergänzt den Marker und die beiden Tooltip-Texte für
 * die Anteile-Spalte der Übersichtstabellen. Sie stehen aus demselben Grund
 * hier: `ViewBuyEdit`, `ViewSaleEdit` und `ViewShareDetails` brauchen exakt
 * dieselben Formulierungen.
 *
 * @note Zustandslos und datenbankfrei — alle Eingangsdaten kommen als
 * Parameter herein, damit die Formatierung ohne Qt-Widgets und ohne SQLite
 * prüfbar bleibt (gleiche Bauweise wie ShareSplitAdjuster und
 * SaleFifoAllocator).
 */
class ShareSplitHint
{
public:
    /**
     * @brief true, wenn nach @p date mindestens ein Split liegt.
     *
     * Massstab ist dasselbe „echt nach dem Datum" wie in
     * ShareSplitAdjuster::volumeFactor(): ein Split AM Belegdatum wirkt sich
     * auf diesen Beleg nicht mehr aus, er ist bereits in heutiger Skala
     * ausgestellt.
     */
    static bool hasSplitAfter(const QList<ShareSplitObject>& splits,
                              const QDate& date);

    /**
     * @brief Fusszeilen-Text für die Editier-Dialoge.
     *
     * Ohne Split nach @p date wird der gedämpfte Hinweis geliefert, dass die
     * Stückzahl bereits dem heutigen Stand entspricht. Die Zeile ist damit
     * IMMER belegt — sonst würden beim Tippen im Datumsfeld alle darunter
     * liegenden Zeilen springen (Nessies Entscheidung 08.08.2026).
     *
     * Preis und Stückzahl werden gegenläufig umgerechnet: aus 5 Stück à
     * 1.003,00 € werden bei einem 20:1-Split 100 Stück à 50,15 €. Das
     * Produkt bleibt gleich — genau das soll der Text zeigen, damit die
     * gegenüber dem Beleg veränderte Stückzahl nicht wie ein Fehler wirkt.
     *
     * @param splits  Alle Splits der Aktie, aufsteigend nach Datum.
     * @param date    Datum des Belegs (Kauf- bzw. Verkaufsdatum).
     * @param volume  Stückzahl laut Beleg.
     * @param price   Preis je Stück laut Beleg.
     */
    static QString footerText(const QList<ShareSplitObject>& splits,
                              const QDate& date,
                              double volume,
                              double price);

    /**
     * @brief Vollständige Aufzählung aller nach @p date liegenden Splits,
     * je Zeile einer — für den Tooltip des Hinweis-Labels.
     *
     * Leer, wenn kein Split nach @p date liegt.
     */
    static QString tooltipText(const QList<ShareSplitObject>& splits,
                               const QDate& date);

    /**
     * @brief Das Markerzeichen der Anteile-Spalte in den Übersichtstabellen.
     *
     * Phase 3c (10.08.2026). Bewusst ein reines Textzeichen statt eines
     * Icons: die Anteile-Spalte ist eine Textspalte, und ein `setCellWidget()`
     * wie in der Dokument-Spalte würde den Zellentext verdrängen und die
     * Zentrierung zerstören (Nessies Entscheidung 10.08.2026).
     */
    static QString marker();

    /**
     * @brief Hängt marker() an @p cellText an, wenn @p affected true ist.
     *
     * Damit steht die Zusammensetzung des Zellentexts an einer Stelle und
     * nicht in jeder der drei Übersichtstabellen erneut.
     */
    static QString withMarker(const QString& cellText, bool affected);

    /**
     * @brief Tooltip einer BELEG-Zeile in einer Übersichtstabelle.
     *
     * Die Zeile selbst zeigt weiterhin die Stückzahl laut Beleg — sie ist
     * eine Abschrift des Dokuments, das nach einem Zeilenklick rechts in der
     * Vorschau steht. Der Tooltip nennt die heutige Entsprechung.
     *
     * Leer, wenn nach @p date kein Split liegt; dann bekommt die Zelle auch
     * keinen Marker.
     *
     * @param splits  Alle Splits der Aktie.
     * @param date    Datum des Belegs.
     * @param volume  Stückzahl laut Beleg.
     * @param price   Preis je Stück laut Beleg.
     */
    static QString overviewRowTooltip(const QList<ShareSplitObject>& splits,
                                      const QDate& date,
                                      double volume,
                                      double price);

    /**
     * @brief Tooltip einer SUMMEN-Zelle (Fusszeile oder Jahreszeile der
     * Übersicht).
     *
     * Aggregate sind keine Beleg-Abschriften, sondern Rechenergebnisse: sie
     * stehen deshalb durchgehend auf heutiger Skala, weil eine Summe über
     * Belege verschiedener Stückelung sonst gar nichts bedeutet. Der Tooltip
     * sagt das, damit die Zahl nicht wie ein Rechenfehler gegenüber den
     * sichtbaren Zeilen wirkt.
     *
     * @param splits        Alle Splits der Aktie.
     * @param earliestDate  Datum des ÄLTESTEN Belegs, der in die Summe
     *        eingeht. Ein Split nach diesem Datum betrifft mindestens einen
     *        der summierten Belege — genau dann ist die Umrechnung wirksam.
     * @return Leer, wenn kein Beleg der Summe umgerechnet werden musste.
     */
    static QString overviewAggregateTooltip(const QList<ShareSplitObject>& splits,
                                            const QDate& earliestDate);

    /** Kurzform eines Splits, z. B. "20:1 am 18.07.2022". */
    static QString describeSplit(const ShareSplitObject& split);

    /** Verhältnis-Seite ohne unnötige Nachkommastellen ("20" statt "20,00"). */
    static QString formatRatioPart(double value);
};

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QList>
#include <QString>
#include <QStringList>

/**
 * @brief Bewertung einer Zeile im Tab "Depot- vs. Marktwert".
 *
 * Die View setzt daraus Farbe und Symbol; welche Bewertung gilt, entscheidet
 * allein der Presenter.
 */
enum class CalculationOverviewTone
{
    Normal,    ///< keine Bewertung
    Ok,        ///< stimmt überein
    Hint,      ///< erwartbare oder erklärte Differenz
    Deviation  ///< echte Abweichung
};

/**
 * @brief Ein Eintrag der Auswahlliste links.
 */
struct CalculationOverviewEntry
{
    /// Gruppe in der Auswahlliste.
    enum class Group
    {
        Total,  ///< "Gesamt / Footer", ganz oben
        Held,   ///< Aktie mit Bestand
        Sold    ///< vollständig verkaufte Aktie
    };

    QString key;                ///< GUID der Aktie oder PresenterCalculationOverview::totalKey()
    QString title;              ///< angezeigter Text
    Group   group = Group::Held;
    CalculationOverviewTone tone = CalculationOverviewTone::Ok;
    QString toolTip;            ///< z. B. "1 Abweichung, 2 Hinweise"
};

/**
 * @brief Eine Zeile der Gegenüberstellung Marktwert / Depotwert, fertig
 * formatiert.
 */
struct CalculationOverviewComparisonRow
{
    QString number;   ///< "1", "2", …
    QString label;    ///< Bezeichnung des Werts
    QString market;   ///< Betrag oder "–"
    QString depot;    ///< Betrag oder "–"
    QString diff;     ///< Depot − Markt oder "–"
    QString way;      ///< "beide: …" oder "Marktwert: …\nDepotwert: …"
    QString usedIn;   ///< "Fließt ein in", eine Stelle je Zeile
    QString toolTip;  ///< Rechenweg mit den eingesetzten Zahlen
};

/**
 * @brief Eine Zeile des Abgleichs gegen die Charts, fertig formatiert.
 */
struct CalculationOverviewCheckRow
{
    QString label;
    QString tableValue;
    QString chartValue;
    QString diff;
    QString result;   ///< "ok", "Hinweis", "ABWEICHUNG", "durch Kurs erklärt", …
    CalculationOverviewTone tone = CalculationOverviewTone::Ok;
};

/**
 * @brief Eine Auffälligkeit (Abweichung oder Hinweis).
 */
struct CalculationOverviewFinding
{
    QString text;
    CalculationOverviewTone tone = CalculationOverviewTone::Hint;
};

/**
 * @brief Eine Tabelle in einer Gruppe, fertig formatiert (ergänzt 18.11.2026).
 *
 * Für die Transaktionen und den Bestand je Aktie. Die View legt daraus eine
 * Qt-Tabelle an, so hoch wie ihr Inhalt; die Summenzeile steht fett darunter.
 */
struct CalculationOverviewTable
{
    QStringList        headers;
    QList<bool>        rightAligned;  ///< je Spalte; fehlt ein Eintrag, linksbündig
    QList<QStringList> rows;
    QStringList        sumRow;        ///< leer = keine Summenzeile
    /// Bewertung je Zelle, gleiche Form wie @c rows; fehlt ein Eintrag, Normal.
    QList<QList<CalculationOverviewTone>> tones;
    QString            note;          ///< Fußnote unter der Tabelle, leer = keine
};

/**
 * @brief Eine Gruppe zum Aufklappen unter dem Abgleich (ergänzt 18.11.2026).
 *
 * Nessies Entscheidungen vom 17.11.2026: je Art eine Gruppe (Käufe,
 * Verkäufe, Dividenden, Kosten, Splits) und "Bestand / FIFO"; die Kopfzeile
 * zeigt Anzahl und Kurzinfo. Ob sie aufgeklappt ist, entscheidet der
 * Presenter — die View übernimmt es bei jeder neuen Auswahl.
 */
struct CalculationOverviewGroup
{
    /// Art der Gruppe, in der Reihenfolge der Anzeige.
    enum class Kind { Buys, Sales, Dividends, Brokerages, Splits, Stock };

    Kind    kind = Kind::Buys;
    QString title;    ///< z. B. "Käufe (3)"
    QString summary;  ///< Kurzinfo rechts in der Kopfzeile, z. B. "290,0000 Stk. · 8.717,30 €"
    CalculationOverviewTone tone = CalculationOverviewTone::Normal;  ///< Farbe der Kurzinfo
    bool    expanded = false;
    QList<CalculationOverviewTable> tables;  ///< leer = keine Einträge
};

/**
 * @brief Alles, was die rechte Seite für eine Auswahl zeigt.
 */
struct CalculationOverviewDetail
{
    QString title;            ///< Aktienname oder "Gesamt / Footer"
    QString subtitle;         ///< WKN, ISIN, Kurs, Bestand bzw. Anzahl Aktien

    QString comparisonTitle;  ///< Überschrift der Gegenüberstellung
    QList<CalculationOverviewComparisonRow> comparison;

    QString checkTitle;       ///< Überschrift des Abgleichs
    QList<CalculationOverviewCheckRow> checks;
    QStringList checkNotes;   ///< Zeilen unter dem Abgleich (Kurseffekt, fehlende Tageswerte)

    QList<CalculationOverviewFinding> findings;  ///< leer = keine Auffälligkeiten

    // ── Gruppen zum Aufklappen (ergänzt 18.11.2026, v1.31.0) ─────────────
    QString checkSummary;     ///< Kurzinfo des Abgleichs, z. B. "alles ok"
    CalculationOverviewTone checkTone = CalculationOverviewTone::Normal;
    bool    checkExpanded = true;      ///< immer offen (Nessie, 17.11.2026)
    QString findingsSummary;  ///< z. B. "keine" oder "1 Abweichung, 2 Hinweise"
    CalculationOverviewTone findingsTone = CalculationOverviewTone::Normal;
    bool    findingsExpanded = false;  ///< offen, sobald es einen Eintrag gibt
    /// Transaktionen und Bestand je Aktie; leer bei "Gesamt / Footer".
    QList<CalculationOverviewGroup> groups;
};

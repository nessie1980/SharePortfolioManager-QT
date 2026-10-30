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
};

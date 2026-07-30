// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QString>
#include <QColor>
#include <QDate>
#include <QList>
#include <QMap>

/**
 * @brief The six toggleable data series in the "Aktien-Chart" tab
 * (C# reference: "Selektion"-Checkboxen in FrmShareDetails).
 *
 * ClosingPrice/OpeningPrice/High/Low share the price (€) axis. HeldVolume
 * ("Anteile", im Portfolio gehaltene Stückzahl) und TradedVolume
 * ("Gehandelte Anteile", Börsen-Handelsvolumen des Tages aus
 * daily_values.volume) teilen sich seit 12.07.2026 eine gemeinsame
 * Stück-Achse (siehe ChartAxis) — ihre zugehörigen Checkboxen sind in
 * ViewChart::setupSelektionBox() gegenseitig exklusiv, es kann also nie
 * beide Serien gleichzeitig geben (siehe ARCHITECTURE.md,
 * "ChartForm-Details").
 */
enum class SeriesKind
{
    ClosingPrice, ///< "Schluss-Kurs" — DailyValuesObject::closingPrice()
    OpeningPrice, ///< "Eröffnungs-Kurs" — DailyValuesObject::openingPrice()
    High,         ///< "Höchstwert" — DailyValuesObject::top()
    Low,          ///< "Tiefstwert" — DailyValuesObject::bottom()
    HeldVolume,   ///< "Anteile" — cumulative held volume as of each date (buys - sales)
    TradedVolume  ///< "Gehandelte Anteile" — DailyValuesObject::volume() (Börsen-Handelsvolumen des Tages)
};

/** All six series, in the same order as the C# reference's checkbox list
 *  (TradedVolume added 12.07.2026 — see ARCHITECTURE.md, "ChartForm-Details"). */
inline QList<SeriesKind> allSeriesKinds()
{
    return { SeriesKind::ClosingPrice, SeriesKind::OpeningPrice,
             SeriesKind::High, SeriesKind::Low,
             SeriesKind::HeldVolume, SeriesKind::TradedVolume };
}

/**
 * @brief Which Y-axis a series plots against. Two independent scales:
 * Price and Volume. HeldVolume and TradedVolume both plot against Volume —
 * they briefly had separate axes (12.07.2026), but since their checkboxes
 * are now mutually exclusive in ViewChart::setupSelektionBox(), only one of
 * them can ever be visible at a time, so a shared axis is unambiguous again
 * (see ARCHITECTURE.md, "ChartForm-Details").
 */
enum class ChartAxis { Price, Volume };

/**
 * @brief Unit for the "Interval"-Auswahl, combined with "Anzahl" to compute
 * the displayed date range backwards from "Start-Datum".
 */
enum class IntervalUnit { Day, Week, Month, Year };

/**
 * @brief One row of the "Legende" box.
 *
 * @p title / @p line1 / @p line2 are already fully formatted (locale-aware
 * numbers, translated labels) by PresenterChart — the View performs no
 * formatting, only layout, same convention as CalculationRow in
 * ShareDetailsForm.
 */
struct LegendEntry
{
    QColor  color;
    QString title;  ///< e.g. "Schluss-Kurs(€)", "Letzter Kauf:" or "Ältere Käufe"
    QString line1;  ///< e.g. "Min: 379,7 / Max: 422,4" or "12.05.2022: 198,36€" — bleibt
                     ///< leer für reine Farbe+Bezeichnung-Einträge wie "Ältere Käufe"/
                     ///< "Ältere Verkäufe" (ergänzt 30.07.2026), ViewChart rendert dann
                     ///< keine zweite Zeile.
    QString line2;  ///< e.g. "" or "422,4€ - 198,36€ = 224,04€ (112,95 %)"
};

using LegendEntries = QList<LegendEntry>;

/**
 * @brief One data series to plot: a date/value pair list plus display info.
 *
 * @p values.size() always equals @p dates.size(); kept as parallel lists
 * (rather than QMap<QDate,double>) since QLineSeries construction needs
 * strictly ordered points and PresenterChart already reads the dates in
 * order from DailyValuesRepository.
 */
struct ChartSeriesData
{
    SeriesKind    kind = SeriesKind::ClosingPrice;
    QList<QDate>  dates;
    QList<double> values;
    QColor        color;
    ChartAxis     axis = ChartAxis::Price; ///< which Y-axis this series plots against
};

/** "Letzter Kauf" / "Letzter Verkauf" reference point for the Legende box,
 *  and (mit allen Feldern) für die einzelnen Kauf-/Verkauf-Markerlinien. */
struct ChartReferenceInfo
{
    bool    valid  = false;
    QDate   date;
    double  price  = 0.0;
    double  volume = 0.0; ///< nur von buysInRange()/salesInRange() befüllt, für den Hover-Tooltip
};

/** Ob eine ChartReferenceLine einen Kauf oder Verkauf markiert — bestimmt
 *  das Tooltip-Label ("Kauf"/"Verkauf"). */
enum class ChartReferenceLineKind { Buy, Sale };

/**
 * @brief One vertical marker line drawn across the price axis for a buy or
 * sale that falls within the currently displayed date range (ergänzt
 * 12.07.2026, portiert vom C#-Referenz-Verhalten — siehe ARCHITECTURE.md,
 * "ChartForm-Details").
 *
 * Color coding matches the Legende's "Letzter Kauf"/"Letzter Verkauf"
 * swatches: the single globally most recent buy/sale gets the "latest"
 * color, any older ones within the range get the "older" color.
 * price/volume are carried along for the Hover-Tooltip (ergänzt 12.07.2026,
 * zweiter Anlauf), nicht für das Zeichnen selbst — die Linie geht immer über
 * die volle Preis-Achsen-Höhe, unabhängig vom konkreten Kaufpreis.
 */
struct ChartReferenceLine
{
    QDate  date;
    QColor color;
    ChartReferenceLineKind kind = ChartReferenceLineKind::Buy;
    double price  = 0.0;
    double volume = 0.0;
};

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IViewChart.h"
#include "IModelChart.h"

#include <QObject>
#include <QString>
#include <QDate>

/**
 * @brief Presenter for the "Aktien-Chart" tab (MVP pattern).
 *
 * Unlike PresenterShareDetails (populate-once, read-only display),
 * PresenterChart is interactive: every control change (Start-Datum,
 * Interval, Anzahl, Selektion-Checkboxen) re-reads the current control
 * values from the view and rebuilds the plotted series + Legende box —
 * same "single refresh slot re-reads getters" convention as
 * PresenterBrokerageEdit::onValuesChanged()/refreshDerivedValues().
 *
 * "Start-Datum" doubles as the *end* of the displayed range (matches the C#
 * reference: Start-Datum=10.7.2026, Interval=Month, Anzahl=1 produces the
 * range 10.06.2026-10.07.2026) — the range's start is computed backwards
 * from it by (Anzahl × Interval-Einheit). See ARCHITECTURE.md,
 * "ChartForm-Details" for the full mapping against the C# reference
 * screenshot.
 */
class PresenterChart : public QObject
{
    Q_OBJECT

public:
    explicit PresenterChart(IViewChart* view, IModelChart* model,
                            QString shareGuid, QObject* parent = nullptr);

    /** Loads the default Start-Datum (latest daily value) and does the first refresh(). */
    void loadAndDisplay();

public slots:
    /** Called whenever any control changes (date, interval, count, any Selektion-checkbox). */
    void onControlsChanged();

private:
    void refresh();

    static QDate computeRangeStart(const QDate& rangeEnd, IntervalUnit unit, int count);

    /**
     * @brief Largest "Anzahl" for which the displayed window still reaches
     * back to, but not past, @p earliestDate — ergänzt 12.07.2026 auf
     * Nessies Vorgabe: weiteres Erhöhen soll gestoppt werden, sobald der
     * älteste vorhandene Wert bereits im Fenster liegt.
     *
     * Kein geschlossener Ausdruck für Monat/Jahr möglich (unterschiedliche
     * Monatslängen — addMonths() clamped den Tag, z.B. 31.01. minus 1 Monat
     * = 28./29.02.), daher eine einfache, robuste Schleife statt einer
     * Formel. @p earliestDate ungültig oder bereits auf/nach @p rangeEnd ->
     * mindestens 1 bleibt immer erlaubt (spiegelt m_countSpin's Minimum in
     * ViewChart).
     *
     * Die Schleife terminiert immer korrekt von selbst (computeRangeStart()
     * bewegt sich bei steigendem count monoton rückwärts, egal welche
     * IntervalUnit), eine feste Obergrenze ist dafür nicht nötig. Trotzdem
     * zwei Sicherheitsbremsen (ergänzt 12.07.2026, zweiter Anlauf — die
     * ursprüngliche feste Konstante kIntervalCountCeiling = 999 erwies sich
     * bei Interval=Tag als echte, spürbare Grenze statt reiner Bremse: 999
     * Tage sind nur ~2,7 Jahre, deutlich weniger als real vorhandene
     * Kurshistorien):
     * 1. earliestDate.daysTo(rangeEnd) — für Interval=Tag die exakt
     *    richtige Grenze, für Woche/Monat/Jahr großzügig genug (jede Stufe
     *    dort ist mindestens so groß wie ein Tag), skaliert automatisch mit
     *    der tatsächlichen Kurshistorie.
     * 2. kAbsoluteSafetyCeiling — feste Notbremse gegen korrupte Datumsdaten
     *    (z.B. ein kaputtes Datum wie Jahr 1), unabhängig von Punkt 1.
     * @param rangeEnd      Aktuelles Start-Datum (= Ende des Zeitraums).
     * @param unit           Aktuelle Interval-Einheit.
     * @param earliestDate   Ältester vorhandener Tageswert für die Aktie
     *                       (ungültig, falls keiner existiert).
     * @return Größte zulässige "Anzahl", mindestens 1.
     */
    static int computeMaxIntervalCount(const QDate& rangeEnd, IntervalUnit unit,
                                       const QDate& earliestDate);

    static QString formatEuro(double value);
    static QString formatPercent(double value);
    static QString formatNumber(double value, int decimals);

    /** Absolute Notbremse gegen die Schleife in computeMaxIntervalCount() —
     *  greift nur bei korrupten Datumsdaten (siehe dortige Doku), nicht im
     *  Normalbetrieb. Die frühere feste kIntervalCountCeiling = 999 wurde
     *  ersetzt: sie war für Interval=Tag eine echte, zu enge Grenze statt
     *  einer reinen Sicherheitsbremse (ergänzt 12.07.2026, zweiter Anlauf). */
    static constexpr int kAbsoluteSafetyCeiling = 1000000;

    IViewChart*  m_view;
    IModelChart* m_model;
    QString      m_shareGuid;

    /** True once loadAndDisplay() found at least one daily value — refresh()
     *  no-ops before that so onControlsChanged() firing during view setup
     *  (e.g. QDateEdit's constructor default) can't run against no data. */
    bool m_hasData = false;
};

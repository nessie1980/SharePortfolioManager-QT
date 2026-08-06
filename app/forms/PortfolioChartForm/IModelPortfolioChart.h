// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "PortfolioChartTypes.h"

#include "../../utils/PortfolioSeriesCalculator.h"

#include <QDate>
#include <QList>

/**
 * @brief Read-only Model-Interface des Depotwert-Charts.
 *
 * Bewusst schmal gehalten, gleiche Konvention wie IModelChart: das Model lädt
 * die Rohdaten, gerechnet wird ausschliesslich im PortfolioSeriesCalculator.
 *
 * @note Anders als IModelChart arbeitet dieses Interface nicht je Aktie,
 * sondern über das gesamte Portfolio — der Chart aggregiert per Definition
 * über alle Aktien hinweg.
 */
class IModelPortfolioChart
{
public:
    virtual ~IModelPortfolioChart() = default;

    /**
     * @brief Lädt alle Aktien des Portfolios mit ihren Transaktionen und
     * Tageswerten, fertig aufbereitet für PortfolioSeriesCalculator.
     *
     * Bewusst ohne Datumsfilter: der Kaufwert und die kumulierten Terme eines
     * Stichtags hängen von der gesamten Vorgeschichte ab, ein gefiltertes
     * Laden würde die Kurve am linken Rand verfälschen. Gefiltert wird erst
     * das Datumsraster im Rechenkern.
     */
    virtual QList<PortfolioShareSeriesInput> loadPortfolioInput() const = 0;

    /**
     * @brief Ältestes Datum, ab dem der Chart überhaupt etwas zeigen kann.
     *
     * Begrenzt "Anzahl" nach oben, damit das Fenster nicht weiter zurück
     * wachsen kann, als es sinnvoll ist.
     *
     * @note Anders als PresenterChart, der hier den ältesten Tageswert
     * verwendet, zählt für den Portfolio-Chart der erste KAUF (korrigiert
     * 06.08.2026, Nessies Rückmeldung). Der Aktien-Chart zeigt den
     * Kursverlauf einer Aktie, der auch vor dem ersten Kauf interessant ist —
     * beim Portfolio dagegen gibt es vor dem ersten Kauf schlicht kein
     * Portfolio, die Kurve läge dort zwangsläufig auf null. Im Feldtest
     * erlaubte die alte Grenze 23 Jahre, obwohl der erste Kauf erst nach
     * gut 11 Jahren erfolgte.
     *
     * @return Datum des ältesten Kaufs; ersatzweise der älteste Tageswert,
     *         falls es noch gar keinen Kauf gibt; sonst ein ungültiges QDate.
     */
    virtual QDate earliestRelevantDate() const = 0;
};

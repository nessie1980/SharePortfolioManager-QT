// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IModelCalculationOverview.h"

#include <QDateTime>
#include <QString>

/**
 * @brief Produktions-Model des Tabs "Depot- vs. Marktwert" (ergänzt
 * 27.10.2026).
 *
 * Header-only und ohne Zustand: reicht nur an CalculationReport::collect()
 * weiter — dieselbe Funktion, mit der auch der Berechnungsbericht seine Daten
 * liest. Der Portfolio-Pfad bleibt leer, er erscheint nur im Kopf der
 * Berichtsdatei.
 */
class ModelCalculationOverview : public IModelCalculationOverview
{
public:
    CalculationReportInput load() const override
    {
        return CalculationReport::collect(QDateTime::currentDateTime(), QString());
    }
};

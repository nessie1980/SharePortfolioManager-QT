// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../../utils/CalculationReport.h"

/**
 * @brief Read-only Model-Interface des Tabs "Depot- vs. Marktwert"
 * (ergänzt 27.10.2026).
 *
 * Bewusst dieselben Eingangsdaten wie der Berechnungsbericht: der Tab zeigt
 * genau das, was der Bericht zum selben Zeitpunkt schreiben würde.
 */
class IModelCalculationOverview
{
public:
    virtual ~IModelCalculationOverview() = default;

    /**
     * @brief Liest alle Aktien samt Tabellen- und Chartwerten, Stand heute
     * (Nessies Entscheidung 27.10.2026: kein Stichtag, sonst passten die
     * Werte nicht mehr zu den Tabellen).
     */
    virtual CalculationReportInput load() const = 0;
};

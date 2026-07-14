// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QString>
#include <QList>

#include "../../models/ShareObject.h"
#include "../../models/SaleObject.h"
#include "../../models/DividendObject.h"
#include "../../models/BrokerageObject.h"
#include "../../utils/ShareCalculator.h"

/**
 * @brief Read-only model interface for the share-details dialog.
 *
 * @note Erweitert 13.07.2026 um loadSales()/loadDividends()/loadBrokerages()
 * für die Tabs "Gewinne/Verluste", "Dividenden" und "Kosten" (siehe
 * ARCHITECTURE.md, "ShareDetailsForm-Details"). Diese drei Methoden liefern
 * dieselben Objekt-Listen, die auch ViewSaleEdit/ViewDividendEdit/
 * ViewBrokerageEdit über ihre jeweiligen IModel*Edit-Interfaces laden — die
 * neuen Tabs zeigen sie nur read-only über OverviewTabWidget an, ohne eigene
 * Berechnungslogik. Kein Widerspruch zur ursprünglichen "bewusst minimal"-
 * Notiz: die dort ausgeschlossenen load*()-Methoden waren für eigene
 * Neuberechnungen gedacht, nicht für reines Durchreichen an dieselben
 * Repository-Aufrufe, die die Editier-Dialoge ohnehin schon nutzen.
 */
class IModelShareDetails
{
public:
    virtual ~IModelShareDetails() = default;

    /** Returns an invalid ShareObject (ShareObject::isValid() == false) if the GUID is unknown. */
    virtual ShareObject loadShare(const QString& shareGuid) const = 0;

    /**
     * @brief Aggregated financial figures for the share (see ShareCalculator).
     *
     * A thin pass-through to ShareCalculator::compute() behind the interface,
     * so PresenterShareDetails stays testable via a FakeModelShareDetails
     * without needing a database.
     */
    virtual ShareValues computeShareValues(const QString& shareGuid,
                                           double curPrice,
                                           double prevDayPrice) const = 0;

    // ── Gewinne/Verluste-, Dividenden-, Kosten-Tabs (nur Depotwert-Modus) ──

    /** Alle Verkäufe der Aktie, für den "Gewinne/Verluste"-Tab. */
    virtual QList<SaleObject> loadSales(const QString& shareGuid) const = 0;

    /** Alle Dividendenzahlungen der Aktie, für den "Dividenden"-Tab. */
    virtual QList<DividendObject> loadDividends(const QString& shareGuid) const = 0;

    /** Alle Kosten-Einträge der Aktie (Kauf/Verkauf/Sonstig), für den "Kosten"-Tab. */
    virtual QList<BrokerageObject> loadBrokerages(const QString& shareGuid) const = 0;
};

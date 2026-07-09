// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QString>

#include "../../models/ShareObject.h"
#include "../../utils/ShareCalculator.h"

/**
 * @brief Read-only model interface for the share-details dialog.
 *
 * Deliberately minimal: ShareDetailsForm currently shows only the share's
 * master data and its "Komplette Depotbewertung" (aggregated Depotwert)
 * figures. The Gewinne/Verluste-, Dividenden- and Kosten-tabs planned for
 * later reuse the existing overview widgets from ViewSaleEdit/
 * ViewDividendEdit/ViewBrokerageEdit instead of loading raw Buy/Sale/
 * Dividend/Brokerage lists here — so this interface does not (and should
 * not) grow load*() methods for those.
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
};

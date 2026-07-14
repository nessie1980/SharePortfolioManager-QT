// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ModelShareDetails.h"

ShareObject ModelShareDetails::loadShare(const QString& shareGuid) const
{
    return m_shareRepo.findByGuid(shareGuid);
}

ShareValues ModelShareDetails::computeShareValues(const QString& shareGuid,
                                                   double curPrice,
                                                   double prevDayPrice) const
{
    return ShareCalculator::compute(shareGuid, curPrice, prevDayPrice);
}

QList<SaleObject> ModelShareDetails::loadSales(const QString& shareGuid) const
{
    return m_saleRepo.findByShare(shareGuid);
}

QList<DividendObject> ModelShareDetails::loadDividends(const QString& shareGuid) const
{
    return m_dividendRepo.findByShare(shareGuid);
}

QList<BrokerageObject> ModelShareDetails::loadBrokerages(const QString& shareGuid) const
{
    return m_brokerageRepo.findByShare(shareGuid);
}

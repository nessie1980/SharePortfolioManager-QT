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

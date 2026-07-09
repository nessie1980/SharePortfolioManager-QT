// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IModelShareDetails.h"
#include "../../repositories/ShareRepository.h"

/**
 * @brief Concrete read-only model for ShareDetailsForm.
 *
 * loadShare() delegates to ShareRepository; computeShareValues() delegates
 * to the stateless ShareCalculator::compute() (which itself reads buys,
 * sales, dividends and brokerage fresh from the repositories). No business
 * logic of its own.
 */
class ModelShareDetails : public IModelShareDetails
{
public:
    ModelShareDetails() = default;

    ShareObject loadShare(const QString& shareGuid) const override;

    ShareValues computeShareValues(const QString& shareGuid,
                                   double curPrice,
                                   double prevDayPrice) const override;

private:
    mutable ShareRepository m_shareRepo;
};

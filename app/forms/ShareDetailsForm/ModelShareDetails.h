// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IModelShareDetails.h"
#include "../../repositories/ShareRepository.h"
#include "../../repositories/SaleRepository.h"
#include "../../repositories/DividendRepository.h"
#include "../../repositories/BrokerageRepository.h"
#include "../../repositories/DailyValuesRepository.h"

/**
 * @brief Concrete read-only model for ShareDetailsForm.
 *
 * loadShare() delegates to ShareRepository; computeShareValues() delegates
 * to the stateless ShareCalculator::compute() (which itself reads buys,
 * sales, dividends and brokerage fresh from the repositories). No business
 * logic of its own.
 *
 * @note Erweitert 13.07.2026 um loadSales()/loadDividends()/loadBrokerages() —
 * dünne Pass-Throughs zu SaleRepository::findByShare()/
 * DividendRepository::findByShare()/BrokerageRepository::findByShare(), analog
 * zu ModelSaleEdit/ModelDividendEdit/ModelBrokerageEdit. Keine eigene Logik.
 */
class ModelShareDetails : public IModelShareDetails
{
public:
    ModelShareDetails() = default;

    ShareObject loadShare(const QString& shareGuid) const override;

    ShareValues computeShareValues(const QString& shareGuid,
                                   double curPrice,
                                   double prevDayPrice) const override;

    QList<SaleObject> loadSales(const QString& shareGuid) const override;
    QList<DividendObject> loadDividends(const QString& shareGuid) const override;
    QList<BrokerageObject> loadBrokerages(const QString& shareGuid) const override;

    QDate latestDailyValueDate(const QString& shareGuid) const override;

private:
    mutable ShareRepository       m_shareRepo;
    mutable SaleRepository        m_saleRepo;
    mutable DividendRepository    m_dividendRepo;
    mutable BrokerageRepository   m_brokerageRepo;
    mutable DailyValuesRepository m_dailyValuesRepo;
};

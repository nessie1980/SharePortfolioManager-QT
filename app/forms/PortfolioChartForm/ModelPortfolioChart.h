// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IModelPortfolioChart.h"

#include "../../repositories/ShareRepository.h"
#include "../../repositories/BuyRepository.h"
#include "../../repositories/SaleRepository.h"
#include "../../repositories/DividendRepository.h"
#include "../../repositories/BrokerageRepository.h"
#include "../../repositories/DailyValuesRepository.h"

/**
 * @brief Produktionsimplementierung von IModelPortfolioChart, gestützt auf die
 * SQLite-Repositories (gleiche Konvention wie ModelChart).
 */
class ModelPortfolioChart : public IModelPortfolioChart
{
public:
    ModelPortfolioChart() = default;

    QList<PortfolioShareSeriesInput> loadPortfolioInput() const override;

    QDate earliestRelevantDate() const override;

private:
    ShareRepository       m_shareRepo;
    BuyRepository         m_buyRepo;
    SaleRepository        m_saleRepo;
    DividendRepository    m_dividendRepo;
    BrokerageRepository   m_brokerageRepo;
    DailyValuesRepository m_dailyValuesRepo;
};

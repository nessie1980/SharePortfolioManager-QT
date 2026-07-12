// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IModelChart.h"

#include "../../repositories/DailyValuesRepository.h"
#include "../../repositories/BuyRepository.h"
#include "../../repositories/SaleRepository.h"

/**
 * @brief Production implementation of IModelChart, backed by the SQLite
 * repositories (same convention as ModelShareDetails).
 */
class ModelChart : public IModelChart
{
public:
    ModelChart() = default;

    QList<DailyValuesObject> loadDailyValues(const QString& shareGuid,
                                              const QDate& from,
                                              const QDate& to) const override;

    QDate latestDailyValueDate(const QString& shareGuid) const override;

    QDate earliestDailyValueDate(const QString& shareGuid) const override;

    QMap<QDate, double> heldVolumeSeries(const QString& shareGuid,
                                         const QList<QDate>& dates) const override;

    ChartReferenceInfo latestBuy(const QString& shareGuid) const override;
    ChartReferenceInfo latestSale(const QString& shareGuid) const override;

    QList<ChartReferenceInfo> buysInRange(const QString& shareGuid,
                                          const QDate& from, const QDate& to) const override;
    QList<ChartReferenceInfo> salesInRange(const QString& shareGuid,
                                           const QDate& from, const QDate& to) const override;

private:
    DailyValuesRepository m_dailyValuesRepo;
    BuyRepository         m_buyRepo;
    SaleRepository        m_saleRepo;
};

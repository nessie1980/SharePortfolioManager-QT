// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IModelDividendEdit.h"
#include "../../repositories/DividendRepository.h"
#include "../../repositories/ShareRepository.h"
#include "../../repositories/DailyValuesRepository.h"
#include "../../repositories/ShareSplitRepository.h"
#include "../../repositories/BuyRepository.h"
#include "../../repositories/SaleRepository.h"

/**
 * @brief Concrete model for the "Dividenden" dialog.
 *
 * Delegates persistence to DividendRepository.
 */
class ModelDividendEdit : public IModelDividendEdit
{
public:
    ModelDividendEdit() = default;

    QList<DividendObject> loadDividends(const QString& shareGuid) const override;
    ShareObject           loadShare(const QString& shareGuid)     const override;

    QList<ShareSplitObject> loadSplits(const QString& shareGuid) const override;

    QList<BuyObject>  loadBuys(const QString& shareGuid)  const override;
    QList<SaleObject> loadSales(const QString& shareGuid) const override;

    bool findClosingPriceForDate(const QString& shareGuid,
                                 const QDate&    date,
                                 double&         outPrice) const override;

    bool addDividend(const DividendObject& dividend)    override;
    bool updateDividend(const DividendObject& dividend) override;
    bool removeDividend(const QString& dividendGuid)    override;

    bool documentExists(const QString& document,
                        const QString& excludeGuid = QString()) const override;

    QString lastError() const override { return m_lastError; }

private:
    DividendRepository   m_dividendRepo;
    ShareRepository      m_shareRepo;
    DailyValuesRepository m_dailyValuesRepo;
    ShareSplitRepository  m_splitRepo;
    BuyRepository         m_buyRepo;
    SaleRepository        m_saleRepo;
    mutable QString      m_lastError;
};

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IModelShareEdit.h"
#include "../../repositories/ShareRepository.h"
#include "../../repositories/BuyRepository.h"
#include "../../repositories/SaleRepository.h"
#include "../../repositories/DividendRepository.h"
#include "../../repositories/BrokerageRepository.h"
#include "../../repositories/ShareSplitRepository.h"

/**
 * @brief Concrete model for the "Aktie editieren" dialog.
 *
 * Delegates all persistence and aggregate queries to the individual
 * repository classes.
 */
class ModelShareEdit : public IModelShareEdit
{
public:
    ModelShareEdit() = default;

    ShareObject loadShare(const QString& guid)  const override;
    bool        saveShare(const ShareObject& share)   override;

    double totalBuyValue(const QString& shareGuid)      const override;
    int    buyCount(const QString& shareGuid)            const override;
    double totalSaleValue(const QString& shareGuid)     const override;
    double totalProfitLoss(const QString& shareGuid)    const override;
    int    saleCount(const QString& shareGuid)          const override;
    double totalDividendValue(const QString& shareGuid) const override;
    int    dividendCount(const QString& shareGuid)      const override;
    double totalBrokerageValue(const QString& shareGuid)const override;
    int    brokerageCount(const QString& shareGuid)     const override;
    double currentVolume(const QString& shareGuid)      const override;
    QString firstBuyDate(const QString& shareGuid)      const override;

    QList<ShareSplitObject> loadSplits(const QString& shareGuid) const override;

    QString lastError() const override { return m_lastError; }

private:
    ShareRepository    m_shareRepo;
    BuyRepository      m_buyRepo;
    SaleRepository     m_saleRepo;
    DividendRepository m_dividendRepo;
    BrokerageRepository m_brokerageRepo;
    ShareSplitRepository m_splitRepo;
    mutable QString    m_lastError;
};

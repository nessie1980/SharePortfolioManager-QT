// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IModelBuyEdit.h"
#include "../../repositories/BuyRepository.h"
#include "../../repositories/BrokerageRepository.h"
#include "../../repositories/ShareRepository.h"
#include "../../repositories/ShareSplitRepository.h"

/**
 * @brief Concrete model for the "Käufe" dialog.
 *
 * Delegates persistence to BuyRepository and BrokerageRepository.
 * Add/update/delete operations are wrapped in a single SQLite transaction.
 */
class ModelBuyEdit : public IModelBuyEdit
{
public:
    ModelBuyEdit() = default;

    QList<BuyObject> loadBuys(const QString& shareGuid)    const override;
    ShareObject      loadShare(const QString& shareGuid)   const override;
    BrokerageObject  loadBrokerage(const QString& buyGuid) const override;

    QList<ShareSplitObject> loadSplits(const QString& shareGuid) const override;

    bool addBuy(const BuyObject& buy,
                double provision = 0.0,
                double brokerFee = 0.0,
                double traderFee = 0.0,
                double reduction = 0.0) override;

    bool updateBuy(const BuyObject& buy,
                   double provision = 0.0,
                   double brokerFee = 0.0,
                   double traderFee = 0.0,
                   double reduction = 0.0) override;

    bool removeBuy(const QString& buyGuid) override;

    bool orderNumberExists(const QString& shareGuid,
                           const QString& orderNumber,
                           const QString& excludeGuid = QString()) const override;

    bool documentExists(const QString& document,
                        const QString& excludeGuid = QString()) const override;

    QString lastError() const override { return m_lastError; }

private:
    BuyRepository       m_buyRepo;
    BrokerageRepository m_brokerageRepo;
    ShareRepository     m_shareRepo;
    ShareSplitRepository m_splitRepo;
    mutable QString     m_lastError;
};

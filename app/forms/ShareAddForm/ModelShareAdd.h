// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IModelShareAdd.h"
#include "../../repositories/ShareRepository.h"
#include "../../repositories/BuyRepository.h"
#include "../../repositories/BrokerageRepository.h"

/**
 * @brief Concrete model for the "Aktie hinzufügen" dialog.
 *
 * Delegates persistence to ShareRepository, BuyRepository and
 * BrokerageRepository. All three inserts are wrapped in a single
 * SQLite transaction: if any step fails, the whole operation rolls back.
 *
 * ### Brokerage rule
 * Every buy transaction automatically gets a linked BrokerageObject entry
 * in the `brokerage` table (buyGuid set, saleGuid empty). This mirrors the
 * original C# application behaviour and keeps all cost data in one place.
 * Even if the buy has zero fees the entry is created so the link exists.
 */
class ModelShareAdd : public IModelShareAdd
{
public:
    ModelShareAdd() = default;

    bool    saveShareWithBuy(const ShareObject& share,
                             const BuyObject&   buy,
                             double provision  = 0.0,
                             double brokerFee  = 0.0,
                             double traderFee  = 0.0,
                             double reduction  = 0.0) override;

    bool    wknExists(const QString& wkn)   const override;
    bool    isinExists(const QString& isin) const override;

    QString lastError() const override { return m_lastError; }

private:
    ShareRepository    m_shareRepo;
    BuyRepository      m_buyRepo;
    BrokerageRepository m_brokerageRepo;
    QString            m_lastError;
};

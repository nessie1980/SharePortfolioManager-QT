// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IModelSaleEdit.h"
#include "../../repositories/SaleRepository.h"
#include "../../repositories/BuyRepository.h"
#include "../../repositories/BrokerageRepository.h"
#include "../../repositories/ShareRepository.h"
#include "../../repositories/ShareSplitRepository.h"

/**
 * @brief Concrete model for the "Verkäufe" dialog.
 *
 * Delegates persistence to SaleRepository, BuyRepository and BrokerageRepository.
 * Add/update/delete operations are wrapped in a single SQLite transaction.
 */
class ModelSaleEdit : public IModelSaleEdit
{
public:
    ModelSaleEdit() = default;

    QList<SaleObject>  loadSales(const QString& shareGuid)         const override;
    ShareObject        loadShare(const QString& shareGuid)         const override;
    QList<BuyObject>   loadAvailableBuys(const QString& shareGuid) const override;
    QList<BuyObject>   loadAllBuys(const QString& shareGuid)       const override;
    BrokerageObject    loadBrokerage(const QString& saleGuid)      const override;
    BrokerageObject    loadBrokerageForBuy(const QString& buyGuid) const override;

    /** Available buys for a specific depot, oldest first (FIFO). */
    QList<BuyObject>   loadAvailableBuysForDepot(const QString& shareGuid,
                                                 const QString& depotNumber) const;

    QList<BuyObject>   loadAvailableBuysForDepotExcludingSale(
        const QString& shareGuid,
        const QString& depotNumber,
        const QString& excludeSaleGuid) const override;

    QList<ShareSplitObject> loadSplits(const QString& shareGuid) const override;

    bool addSale(const SaleObject& sale)       override;
    bool updateSale(const SaleObject& sale)    override;
    bool removeSale(const QString& saleGuid)   override;

    bool orderNumberExists(const QString& shareGuid,
                           const QString& orderNumber,
                           const QString& excludeGuid = QString()) const override;

    bool documentExists(const QString& document,
                        const QString& excludeGuid = QString()) const override;

    QString lastError() const override { return m_lastError; }

private:
    SaleRepository       m_saleRepo;
    BuyRepository        m_buyRepo;
    BrokerageRepository  m_brokerageRepo;
    ShareRepository      m_shareRepo;
    ShareSplitRepository m_splitRepo;
    mutable QString      m_lastError;
};

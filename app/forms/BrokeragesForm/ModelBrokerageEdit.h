// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IModelBrokerageEdit.h"
#include "../../repositories/BrokerageRepository.h"

/**
 * @brief Concrete model for the "Kosten" dialog.
 *
 * Delegates persistence to BrokerageRepository.
 */
class ModelBrokerageEdit : public IModelBrokerageEdit
{
public:
    ModelBrokerageEdit() = default;

    QList<BrokerageObject> loadBrokerages(const QString& shareGuid) const override;

    bool addBrokerage(const BrokerageObject& brokerage)              override;
    bool updateBrokerage(const BrokerageObject& brokerage)           override;
    bool updateDocument(const QString& guid, const QString& document) override;
    bool removeBrokerage(const QString& guid)                        override;

    bool documentExists(const QString& document,
                        const QString& excludeGuid = QString()) const override;

    QString lastError() const override { return m_lastError; }

private:
    BrokerageRepository m_brokerageRepo;
    mutable QString     m_lastError;
};

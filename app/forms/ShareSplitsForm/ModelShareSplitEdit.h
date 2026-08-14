// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IModelShareSplitEdit.h"
#include "../../repositories/ShareSplitRepository.h"
#include "../../repositories/BuyRepository.h"
#include "../../repositories/DailyValuesRepository.h"

/**
 * @brief Konkretes Model für den Dialog "Aktiensplits".
 *
 * Delegiert weitgehend an `ShareSplitRepository`; `BuyRepository` wird
 * ausschliesslich für openLots() gebraucht, also für die Angabe der
 * Bestandsänderung in der Löschabfrage.
 *
 * Einzige Ausnahme ist documentExists(): die Abfrage steht hier direkt statt
 * im Repository — genauso wie in ModelBuyEdit, ModelSaleEdit,
 * ModelDividendEdit und ModelBrokerageEdit. Siehe die Begründung in der
 * Implementierung.
 */
class ModelShareSplitEdit : public IModelShareSplitEdit
{
public:
    ModelShareSplitEdit() = default;

    QList<ShareSplitObject> loadSplits(const QString& shareGuid) const override;
    bool existsForDate(const QString& shareGuid, const QDate& date) const override;
    QList<OpenBuyLot> openLots(const QString& shareGuid) const override;
    bool documentExists(const QString& document,
                        const QString& excludeGuid = QString()) const override;
    QList<DailyValuesObject> dailyValuesInRange(const QString& shareGuid,
                                                const QDate& from,
                                                const QDate& to) const override;

    bool addSplit(const ShareSplitObject& split)    override;
    bool updateSplit(const ShareSplitObject& split) override;
    bool removeSplit(const QString& guid)           override;

    QString lastError() const override { return m_lastError; }

private:
    ShareSplitRepository  m_splitRepo;
    BuyRepository         m_buyRepo;
    DailyValuesRepository m_dailyValuesRepo;
    mutable QString       m_lastError;
};

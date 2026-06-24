// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../../models/ShareObject.h"
#include "../../models/BuyObject.h"

/**
 * @brief Abstract model interface for the "Aktie hinzufügen" dialog.
 *
 * Declares the data operations the Presenter needs without depending
 * on the concrete ModelShareAdd implementation or the repository layer.
 */
class IModelShareAdd
{
public:
    virtual ~IModelShareAdd() = default;

    /**
     * @brief Persist a new share + first buy + linked brokerage atomically.
     *
     * Insert order within the transaction:
     * 1. ShareObject → shares table
     * 2. BuyObject   → buys table (with brokerageGuid pre-filled)
     * 3. BrokerageObject → brokerage table (buy_guid FK requires buy to exist first)
     *
     * @param share      Fully populated ShareObject.
     * @param buy        BuyObject without brokerage fields (brokerageGuid set by model).
     * @param provision  Broker provision fee.
     * @param brokerFee  Broker fee.
     * @param traderFee  Trading venue fee.
     * @param reduction  Reduction on brokerage.
     * @return true on success; false if any insert fails (transaction is rolled back).
     */
    virtual bool saveShareWithBuy(const ShareObject& share,
                                  const BuyObject&   buy,
                                  double provision  = 0.0,
                                  double brokerFee  = 0.0,
                                  double traderFee  = 0.0,
                                  double reduction  = 0.0) = 0;

    /**
     * @brief Check whether a WKN is already registered in the portfolio.
     * @param wkn  WKN to look up.
     * @return true if the WKN exists.
     */
    virtual bool wknExists(const QString& wkn) const = 0;

    /**
     * @brief Check whether an ISIN is already registered in the portfolio.
     * @param isin  ISIN to look up.
     * @return true if the ISIN exists.
     */
    virtual bool isinExists(const QString& isin) const = 0;

    /**
     * @brief Returns the last error message from the most recent failed operation.
     * @return Human-readable error string, or an empty string if no error occurred.
     */
    virtual QString lastError() const = 0;
};

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "BrokerageObject.h"

BrokerageObject::BrokerageObject(const QString& guid,
                                 const QString& shareGuid,
                                 const QString& buyGuid,
                                 const QString& saleGuid,
                                 const QString& dateTime,
                                 double provision,
                                 double brokerFee,
                                 double traderFee,
                                 double reduction,
                                 const QString& document)
    : m_guid(guid)
    , m_shareGuid(shareGuid)
    , m_buyGuid(buyGuid)
    , m_saleGuid(saleGuid)
    , m_dateTime(dateTime)
    , m_provision(provision)
    , m_brokerFee(brokerFee)
    , m_traderFee(traderFee)
    , m_reduction(reduction)
    , m_document(document)
{
    calculateValues();
}

void BrokerageObject::calculateValues()
{
    m_brokerage          = m_provision + m_brokerFee + m_traderFee;
    m_brokerageReduction = m_brokerage - m_reduction;
}

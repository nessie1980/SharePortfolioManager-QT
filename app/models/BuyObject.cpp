// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "BuyObject.h"

BuyObject::BuyObject(const QString& guid,
                     const QString& shareGuid,
                     const QString& depotNumber,
                     const QString& orderNumber,
                     const QString& dateTime,
                     double volume,
                     double volumeSold,
                     double price,
                     const QString& brokerageGuid,
                     const QString& document)
    : m_guid(guid)
    , m_shareGuid(shareGuid)
    , m_depotNumber(depotNumber)
    , m_orderNumber(orderNumber)
    , m_dateTime(dateTime)
    , m_volume(volume)
    , m_volumeSold(volumeSold)
    , m_price(price)
    , m_brokerageGuid(brokerageGuid)
    , m_document(document)
{}

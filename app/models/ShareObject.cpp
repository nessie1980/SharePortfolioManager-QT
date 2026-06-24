// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ShareObject.h"

ShareObject::ShareObject(const QString& guid,
                         const QString& wkn,
                         const QString& isin,
                         const QString& name,
                         ShareType      type,
                         const QString& currency)
    : m_guid(guid)
    , m_wkn(wkn)
    , m_isin(isin)
    , m_name(name)
    , m_shareType(type)
    , m_currency(currency)
    , m_addDateTime(QDateTime::currentDateTime().toString(Qt::ISODate))
{}

ShareObject::ShareObject(const QString& guid,
                         const QString& wkn,
                         const QString& isin,
                         const QString& name,
                         ShareType      shareType,
                         const QString& currency,
                         const QString& addDateTime,
                         double         curPrice,
                         double         prevDayPrice,
                         const QString& lastInternetUpdate,
                         const QString& lastPriceUpdate,
                         ShareUpdateType  updateType,
                         ShareParsingType marketPriceParsingType,
                         const QString& marketPriceUrl,
                         const QString& marketPriceEncoding,
                         ShareParsingType dailyValuesParsingType,
                         const QString& dailyValuesUrl,
                         const QString& dailyValuesEncoding,
                         const QString& detailsWebSiteUrl,
                         const QString& imagePath)
    : m_guid(guid)
    , m_wkn(wkn)
    , m_isin(isin)
    , m_name(name)
    , m_shareType(shareType)
    , m_curPrice(curPrice)
    , m_prevDayPrice(prevDayPrice)
    , m_currency(currency)
    , m_addDateTime(addDateTime)
    , m_lastInternetUpdate(lastInternetUpdate)
    , m_lastPriceUpdate(lastPriceUpdate)
    , m_updateType(updateType)
    , m_marketPriceParsingType(marketPriceParsingType)
    , m_marketPriceUrl(marketPriceUrl)
    , m_marketPriceEncoding(marketPriceEncoding)
    , m_dailyValuesParsingType(dailyValuesParsingType)
    , m_dailyValuesUrl(dailyValuesUrl)
    , m_dailyValuesEncoding(dailyValuesEncoding)
    , m_detailsWebSiteUrl(detailsWebSiteUrl)
    , m_imagePath(imagePath)
{}

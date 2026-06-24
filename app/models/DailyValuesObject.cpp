// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "DailyValuesObject.h"

DailyValuesObject::DailyValuesObject(const QString& shareGuid,
                                     const QDate&   date,
                                     double openingPrice,
                                     double closingPrice,
                                     double top,
                                     double bottom,
                                     double volume)
    : m_shareGuid(shareGuid)
    , m_date(date)
    , m_openingPrice(openingPrice)
    , m_closingPrice(closingPrice)
    , m_top(top)
    , m_bottom(bottom)
    , m_volume(volume)
{}

void DailyValuesObject::setValues(double openingPrice, double closingPrice,
                                   double top, double bottom, double volume)
{
    m_openingPrice = openingPrice;
    m_closingPrice = closingPrice;
    m_top          = top;
    m_bottom       = bottom;
    m_volume       = volume;
}

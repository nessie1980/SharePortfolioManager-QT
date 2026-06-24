// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "DividendObject.h"
#include <QtMath>

DividendObject::DividendObject(const QString& guid,
                               const QString& shareGuid,
                               const QString& dateTime,
                               double rate,
                               double volume,
                               double taxAtSource,
                               double capitalGainsTax,
                               double solidarityTax,
                               double priceAtPayday,
                               bool   enableForeignCurrency,
                               double exchangeRatio,
                               const QString& currency,
                               const QString& document)
    : m_guid(guid)
    , m_shareGuid(shareGuid)
    , m_dateTime(dateTime)
    , m_rate(rate)
    , m_volume(volume)
    , m_priceAtPayday(priceAtPayday)
    , m_enableFc(enableForeignCurrency)
    , m_exchangeRatio(exchangeRatio)
    , m_currency(currency)
    , m_taxAtSource(taxAtSource)
    , m_capitalGainsTax(capitalGainsTax)
    , m_solidarityTax(solidarityTax)
    , m_document(document)
{
    calculateValues();
}

void DividendObject::calculateValues()
{
    // Tax sum
    m_taxSum = m_taxAtSource + m_capitalGainsTax + m_solidarityTax;

    if (m_enableFc && m_exchangeRatio != 0.0) {
        // Foreign currency: payout in FC, then convert to domestic
        m_dividendPayoutFc = qRound(m_rate * m_volume * 100.0) / 100.0;
        m_dividendPayout   = qRound(m_dividendPayoutFc / m_exchangeRatio * 100.0) / 100.0;
    } else {
        // Domestic currency
        m_dividendPayout   = qRound(m_rate * m_volume * 100.0) / 100.0;
        m_dividendPayoutFc = 0.0;
    }

    // Yield = rate / priceAtPayday * 100
    if (m_rate > 0.0 && m_priceAtPayday > 0.0)
        m_yield = m_rate / m_priceAtPayday * 100.0;
    else
        m_yield = 0.0;

    // Net payout after taxes
    m_dividendPayoutWithTaxes = (m_dividendPayout > 0.0)
        ? m_dividendPayout - m_taxSum
        : 0.0;
}

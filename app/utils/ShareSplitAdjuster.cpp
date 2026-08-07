// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ShareSplitAdjuster.h"

// -- volumeFactor ----------------------------------------------------------------

double ShareSplitAdjuster::volumeFactor(const QList<ShareSplitObject>& splits, const QDate& date)
{
    double factor = 1.0;
    for (const ShareSplitObject& split : splits) {
        if (!split.isValid())
            continue;
        if (split.date() > date)
            factor *= split.factor();
    }
    return factor;
}

// -- priceFactorForHistory ---------------------------------------------------------

double ShareSplitAdjuster::priceFactorForHistory(const QList<ShareSplitObject>& splits,
                                                  const QDate& date)
{
    double factor = 1.0;
    for (const ShareSplitObject& split : splits) {
        if (!split.isValid() || split.pricesAdjusted())
            continue;
        if (split.date() > date)
            factor *= split.factor();
    }
    return factor;
}

// -- adjustedVolume ----------------------------------------------------------------

double ShareSplitAdjuster::adjustedVolume(double volume, const QList<ShareSplitObject>& splits,
                                          const QDate& date)
{
    return volume * volumeFactor(splits, date);
}

// -- adjustedTransactionPrice -------------------------------------------------------

double ShareSplitAdjuster::adjustedTransactionPrice(double price,
                                                     const QList<ShareSplitObject>& splits,
                                                     const QDate& date)
{
    const double factor = volumeFactor(splits, date);
    return (factor > 0.0) ? (price / factor) : price;
}

// -- adjustedHistoryPrice ------------------------------------------------------------

double ShareSplitAdjuster::adjustedHistoryPrice(double price,
                                                const QList<ShareSplitObject>& splits,
                                                const QDate& date)
{
    const double factor = priceFactorForHistory(splits, date);
    return (factor > 0.0) ? (price / factor) : price;
}

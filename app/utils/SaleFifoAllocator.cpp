// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "SaleFifoAllocator.h"
#include "ShareSplitAdjuster.h"

// -- allocate --------------------------------------------------------------------

QList<FifoAllocationRow> SaleFifoAllocator::allocate(
    double saleVolume, const QDate& saleDate,
    const QList<BuyObject>& availableBuysOldestFirst,
    const QList<ShareSplitObject>& splits)
{
    QList<FifoAllocationRow> rows;

    // Verkaufsmenge auf heutige Skala umrechnen — das ist die gemeinsame
    // Vergleichsbasis für alle Käufe, unabhängig davon, wie viele Splits
    // zwischen deren jeweiligem Kaufdatum und heute liegen.
    double remainingToday = ShareSplitAdjuster::adjustedVolume(saleVolume, splits, saleDate);

    for (const BuyObject& buy : availableBuysOldestFirst) {
        if (remainingToday <= 1e-9)
            break;

        const double availBeleg = buy.volume() - buy.volumeSold(); // Beleg-Skala des Kaufs
        if (availBeleg <= 1e-9)
            continue;

        const double availToday = ShareSplitAdjuster::adjustedVolume(
            availBeleg, splits, buy.date());
        if (availToday <= 1e-9)
            continue;

        const double takeToday = qMin(availToday, remainingToday);

        // Zurück auf die Beleg-Skala DIESES Kaufs, damit die Zeile zu
        // buy.price() passt und ModelSaleEdit unverändert
        // buy.volumeSold() += detail.volume() rechnen kann.
        const double takeBeleg = ShareSplitAdjuster::belegVolume(takeToday, splits, buy.date());

        FifoAllocationRow row;
        row.buyGuid     = buy.guid();
        row.buyDateTime = buy.dateTime();
        row.volume      = takeBeleg;
        row.buyPrice    = buy.price();
        rows.append(row);

        remainingToday -= takeToday;
    }

    return rows;
}

// -- totalAvailableVolumeToday ----------------------------------------------------

double SaleFifoAllocator::totalAvailableVolumeToday(
    const QList<BuyObject>& availableBuysOldestFirst,
    const QList<ShareSplitObject>& splits)
{
    double sum = 0.0;

    for (const BuyObject& buy : availableBuysOldestFirst) {
        const double availBeleg = buy.volume() - buy.volumeSold(); // Beleg-Skala des Kaufs
        if (availBeleg <= 1e-9)
            continue;

        sum += ShareSplitAdjuster::adjustedVolume(availBeleg, splits, buy.date());
    }

    return sum;
}

// -- isSaleVolumeCovered -----------------------------------------------------------

bool SaleFifoAllocator::isSaleVolumeCovered(
    double saleVolume, const QDate& saleDate,
    const QList<BuyObject>& availableBuysOldestFirst,
    const QList<ShareSplitObject>& splits)
{
    const double saleToday = ShareSplitAdjuster::adjustedVolume(saleVolume, splits, saleDate);
    const double availToday = totalAvailableVolumeToday(availableBuysOldestFirst, splits);

    return saleToday <= availToday + 1e-9;
}

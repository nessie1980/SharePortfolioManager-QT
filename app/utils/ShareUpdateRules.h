// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../models/ShareObject.h"

#include <QList>
#include <QString>

/**
 * @brief Rules governing when a share must fetch daily values.
 *
 * A share whose update type is "Nur Kurs" (ShareUpdateType::MarketPrice) or
 * "Kein Update" (ShareUpdateType::None) never builds a daily-value history.
 * Without that history it cannot be valued on any past date and is therefore
 * excluded from the portfolio value chart entirely — the chart then shows a
 * curve that silently omits those positions.
 *
 * As long as shares are still held, that combination is a data problem rather
 * than a preference. This module holds the single definition of the rule so
 * ViewShareEdit, PresenterShareEdit and MainWindow cannot drift apart.
 *
 * @note Header-only by design (06.08.2026). All functions here are a handful
 * of comparisons with no state and no dependencies beyond ShareObject.h.
 * Three test targets already compile ViewShareEdit.cpp and one compiles
 * MainWindow.cpp; a separate translation unit would have to be added to each
 * of them, and a forgotten entry would surface as a link error rather than as
 * anything informative. The remaining utils modules keep their .h/.cpp split
 * because they carry real implementation weight — this one does not.
 */
namespace ShareUpdateRules {

/**
 * @brief Threshold below which a holding counts as empty.
 *
 * Volumes are accumulated as `volume - volumeSold` over all buys, so an
 * exactly closed position can land a few ULP off zero. Same value and same
 * reasoning as ModelSaleEdit::loadAvailableBuys().
 */
inline constexpr double kVolumeEpsilon = 1e-9;

/**
 * @brief One share reduced to the fields this rule needs.
 *
 * Deliberately not a ShareObject: the rule must be testable without a
 * database, and the caller already holds the computed volume.
 */
struct ShareState
{
    QString         guid;
    QString         wkn;
    QString         name;
    ShareUpdateType updateType    = ShareUpdateType::Both;
    double          currentVolume = 0.0;
};

/**
 * @brief Does this share currently hold any volume?
 * @param currentVolume  Shares still in the depot (bought minus sold).
 */
inline bool requiresDailyValues(double currentVolume)
{
    return currentVolume > kVolumeEpsilon;
}

/**
 * @brief Does this update type fetch a daily-value history?
 */
inline bool updateTypeIncludesDailyValues(ShareUpdateType type)
{
    return type == ShareUpdateType::DailyValues || type == ShareUpdateType::Both;
}

/**
 * @brief Is the given update type acceptable for the given holding?
 *
 * Shares without a holding may use any update type — there is nothing left
 * to value, so a missing history costs nothing.
 */
inline bool isUpdateTypeAllowed(ShareUpdateType type, double currentVolume)
{
    return !requiresDailyValues(currentVolume)
           || updateTypeIncludesDailyValues(type);
}

/**
 * @brief Filter a list of shares down to those violating the rule.
 * @param shares  All shares with their currently held volume.
 * @return Those with a holding but no daily-value update, input order kept.
 */
inline QList<ShareState> sharesNeedingDailyValues(const QList<ShareState>& shares)
{
    QList<ShareState> offenders;
    for (const ShareState& s : shares) {
        if (!isUpdateTypeAllowed(s.updateType, s.currentVolume))
            offenders.append(s);
    }
    return offenders;
}

} // namespace ShareUpdateRules

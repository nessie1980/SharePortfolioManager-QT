// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QCoreApplication>
#include <QString>

#include "IViewShareDetails.h"
#include "IModelShareDetails.h"

/**
 * @brief Presenter for the share-details dialog.
 *
 * Currently covers the "Komplette Depotbewertung" mode only (the C# reference's
 * TabPgCompleteDepotValue) — the Marktwert mode and the Gewinne/Verluste-,
 * Dividenden- and Kosten-tabs are deliberately out of scope for this iteration
 * (see ARCHITECTURE.md, "ShareDetailsForm-Details").
 *
 * All three "Bestandsberechnung" boxes map almost entirely onto existing
 * ShareValues fields (see ShareCalculator.h). The two exceptions —
 * Vortag-Box "Gewinn / Verlust" (volume x prevDayDiff) and Aktuelle-Box
 * "Summe" (curValue + totalDividend + saleProfitLossFinal) — are simple
 * arithmetic over already-computed fields with no repository access
 * involved, so they are computed here rather than added to ShareCalculator.
 *
 * Not a QObject; uses Q_DECLARE_TR_FUNCTIONS for a sensible lupdate context.
 */
class PresenterShareDetails
{
    Q_DECLARE_TR_FUNCTIONS(PresenterShareDetails)

public:
    PresenterShareDetails(IViewShareDetails& view, IModelShareDetails& model, QString shareGuid);

    /**
     * @brief Loads the share and its aggregated ShareValues via the model and
     * pushes formatted content into the view.
     *
     * @return false if the share GUID was not found — in that case
     * view->showError() and view->closeDialog() have already been called and
     * the caller must not exec() the dialog. true otherwise.
     */
    bool loadAndDisplay();

private:
    void buildHeader(const ShareObject& share);

    CalculationRows buildGesamtBox(const ShareValues& v) const;
    CalculationRows buildVortagBox(const ShareValues& v) const;
    CalculationRows buildAktuelleBox(const ShareValues& v) const;

    static QString shareTypeToString(ShareType type);
    /** >= 0 -> "green", < 0 -> "red" (matches the C# reference's Color.Green/Color.Red). */
    static QColor  performanceColor(double value);

    IViewShareDetails&  m_view;
    IModelShareDetails& m_model;
    QString             m_shareGuid;
};

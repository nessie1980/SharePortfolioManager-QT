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
 * Covers the "Komplette Depotbewertung" / "Komplette Marktbewertung" mode
 * (the C# reference's marketValueOverviewTabSelected flag) plus, seit
 * 13.07.2026, die drei zusätzlichen Tabs "Gewinne/Verluste", "Dividenden"
 * und "Kosten" — letztere ausschließlich im Depotwert-Modus (siehe
 * ARCHITECTURE.md, "ShareDetailsForm-Details").
 *
 * Both modes share the same Vortag-Box entirely (no brokerage involved) and
 * the same "Anteile x Aktueller Preis = Einzahlungen" opening rows of the
 * Gesamt-/Aktuelle-Box. They differ in:
 * - Dividenden row: shown normally in Depotwert mode, shown as a disabled
 *   "-" placeholder in Marktwert mode (dividends are a Depotwert-only concept).
 * - Gesamt-Box "Verkäufe"/"Verkaufte Einzahlungen"/"Gewinn / Verlust (gesamt)"/
 *   "Entwicklung": Depotwert mode reads the existing completeCurValue/
 *   completePurchase/completeProfitLoss/completeProfitPct fields directly.
 *   Marktwert mode computes curValue + salePayoutMarket - completePurchaseMarket
 *   fresh in the presenter — deliberately NOT completeCurValueMarket/
 *   completeProfitLossMarket/completeProfitPctMarket, which mix in the
 *   brokerage-inclusive realized P/L for the portfolio grid footer (see
 *   ShareCalculator.h doc comment).
 * - Aktuelle-Box "Gewinn / Verlust (Verkäufe)"/"Summe": Depotwert mode uses
 *   saleProfitLossFinal and computes the sum in the presenter; Marktwert mode
 *   uses the existing saleProfitLoss/marketValue fields directly (marketValue
 *   already equals curValue + saleProfitLoss).
 * - Gewinne/Verluste-, Dividenden-, Kosten-Tabs: nur im Depotwert-Modus
 *   überhaupt angelegt (siehe ViewShareDetails::setupUi()) und daher auch
 *   nur dann von loadAndDisplay() befüllt.
 *
 * Not a QObject; uses Q_DECLARE_TR_FUNCTIONS for a sensible lupdate context.
 */
class PresenterShareDetails
{
    Q_DECLARE_TR_FUNCTIONS(PresenterShareDetails)

public:
    PresenterShareDetails(IViewShareDetails& view, IModelShareDetails& model,
                          QString shareGuid, bool marketValueMode = false);

    /**
     * @brief Loads the share and its aggregated ShareValues via the model and
     * pushes formatted content into the view. Im Depotwert-Modus zusätzlich
     * die Gewinne/Verluste-, Dividenden- und Kosten-Tabs.
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

    /** Dividenden row placeholder for Marktwert mode ("-", greyed out). */
    static CalculationRow disabledRow(const QString& operatorSymbol, const QString& label);

    static QString shareTypeToString(ShareType type);
    /** >= 0 -> "green", < 0 -> "red" (matches the C# reference's Color.Green/Color.Red). */
    static QColor  performanceColor(double value);

    // ── Gewinne/Verluste-, Dividenden-, Kosten-Tabs (nur Depotwert-Modus) ──
    // Reine Pass-Throughs: laden die Objekt-Liste vom Model und reichen sie
    // unverändert an die View weiter. Jahres-Gruppierung/-Summierung
    // geschieht in der View (OverviewTabWidget), identisch zum bisherigen
    // Muster in ViewSaleEdit/ViewDividendEdit/ViewBrokerageEdit.
    void populateGewinneVerluste();
    void populateDividenden();
    void populateKosten();

    IViewShareDetails&  m_view;
    IModelShareDetails& m_model;
    QString             m_shareGuid;
    bool                m_marketValueMode;
};

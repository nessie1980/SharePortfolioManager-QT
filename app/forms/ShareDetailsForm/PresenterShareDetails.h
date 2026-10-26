// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QCoreApplication>
#include <QString>
#include <QDate>

#include "IViewShareDetails.h"
#include "IModelShareDetails.h"
#include "../../models/ShareObject.h"

/**
 * @brief Presenter for the share-details dialog.
 *
 * Covers the "Komplette Depotbewertung" / "Komplette Marktbewertung" mode
 * (the C# reference's marketValueOverviewTabSelected flag) plus, seit
 * 13.07.2026, drei zusätzliche Tabs: "Gewinne/Verluste" (seit 14.07.2026 in
 * beiden Modi, im Marktwert-Modus mit brokeragefreien Werten), "Dividenden"
 * und "Kosten" (weiterhin ausschließlich im Depotwert-Modus, siehe
 * ARCHITECTURE.md, "ShareDetailsForm-Details").
 *
 * Both modes share the same Vortag-Box entirely (no brokerage involved) and
 * the same "Anteile x Aktueller Preis = Einzahlungen" opening rows of the
 * Gesamt-/Aktuelle-Box. They differ in:
 * - Dividenden row (Gesamt-Box): shown normally in Depotwert mode, shown as a disabled
 *   "-" placeholder in Marktwert mode (dividends are a Depotwert-only concept).
 * - Gesamt-Box: Depotwert mode reads completeCurValue/completePurchase/
 *   completeProfitLoss/completeProfitPct, Marktwert mode the "rein Kurs"
 *   counterparts completeCurValueMarket (= curValue + salePayoutMarket,
 *   Verkäufe brutto)/completePurchaseMarket/completeProfitLossMarket/
 *   completeProfitPctMarket — seit dem Bugfix vom 25.10.2026 dieselben Werte
 *   wie Tabelle und Footer (siehe ShareCalculator.h und ARCHITECTURE.md,
 *   "Rechenregeln Depotwert und Marktwert").
 * - Aktuelle-Box (Entscheidung 3, 25.10.2026, beide Modi gleich aufgebaut):
 *   Bestandswert − Einzahlungen (Bestand) = Gewinn / Verlust (aktuell) plus
 *   Entwicklung in %, also exakt die Tabellenspalten "Einzahlung" und
 *   "Aktuelle Entwicklung" — purchaseValue/profitLoss/profitLossPct im
 *   Marktwert-, purchaseValueFinal/profitLossFinal/profitLossPctFinal im
 *   Depotwert-Modus. Keine Dividenden, keine realisierten Gewinne.
 * - Gewinne/Verluste-Tab: seit 14.07.2026 in beiden Modi angelegt und
 *   befüllt (siehe ViewShareDetails::setupUi()) — brokeragefreie Werte im
 *   Marktwert-Modus, siehe ViewShareDetails::populateGewinneVerluste().
 *   Dividenden-/Kosten-Tab: weiterhin nur im Depotwert-Modus überhaupt
 *   angelegt und daher auch nur dann von loadAndDisplay() befüllt.
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
     * pushes formatted content into the view. Gewinne/Verluste-Tab in beiden
     * Modi, Dividenden-/Kosten-Tab zusätzlich im Depotwert-Modus.
     *
     * @return false if the share GUID was not found — in that case
     * view->showError() and view->closeDialog() have already been called and
     * the caller must not exec() the dialog. true otherwise.
     */
    bool loadAndDisplay();

    /**
     * @brief Letzter Werktag (Mo–Fr, keine Feiertagsprüfung) vor @p from.
     *
     * Portiert von der verschachtelten while-Schleife in der C#-Referenz
     * (ShareDetailsForm_Shown()) — dort für jeden möglichen Wochentag
     * durchgerechnet (30.07.2026): das Ergebnis ist in jedem Fall exakt "der
     * letzte Werktag vor from", unabhängig vom Wochentag von @p from. Diese
     * Methode ist die vereinfachte, äquivalente Fassung dieser Schleife.
     *
     * Public + static, damit tst_sharedetailsform.cpp sie direkt mit festen
     * Datums-Kombinationen testen kann, ohne von der echten Systemzeit
     * abzuhängen (dieselbe Konvention wie
     * XmlPortfolioParser::normalizeWebSiteUrl()).
     */
    static QDate previousBusinessDay(const QDate& from);

    /**
     * @brief Ob die "Aktie sollte aktualisiert werden!"-Warnzeile angezeigt
     * werden soll (ergänzt 30.07.2026, portiert von ShareDetailsForm_Shown()
     * in der C#-Referenz, siehe ARCHITECTURE.md "ShareDetailsForm-Details").
     *
     * Kein Warnhinweis, wenn für diese Aktie ohnehin keine Tageswerte
     * abgerufen werden sollen (@p updateType ist MarketPrice oder None —
     * bewusste Einstellung, kein Datenproblem). Andernfalls: Warnhinweis,
     * wenn entweder gar keine Tageswerte vorhanden sind (@p latestDataDate
     * ungültig) oder der neueste vorhandene Tageswert älter als der letzte
     * Werktag vor @p today ist (siehe previousBusinessDay()).
     *
     * Public + static aus demselben Testbarkeits-Grund wie
     * previousBusinessDay() oben.
     */
    static bool needsUpdateWarning(ShareUpdateType updateType,
                                   const QDate& latestDataDate,
                                   const QDate& today);

private:
    void buildHeader(const ShareObject& share);
    void buildUpdateWarning(const ShareObject& share);

    CalculationRows buildGesamtBox(const ShareValues& v) const;
    CalculationRows buildVortagBox(const ShareValues& v) const;
    CalculationRows buildAktuelleBox(const ShareValues& v) const;

    /** Dividenden row placeholder for Marktwert mode ("-", greyed out). */
    static CalculationRow disabledRow(const QString& operatorSymbol, const QString& label);

    static QString shareTypeToString(ShareType type);
    /** >= 0 -> "green", < 0 -> "red" (matches the C# reference's Color.Green/Color.Red). */
    static QColor  performanceColor(double value);

    // ── Gewinne/Verluste- (beide Modi), Dividenden-, Kosten-Tabs (Depotwert-only) ──
    // Reine Pass-Throughs: laden die Objekt-Liste vom Model und reichen sie
    // unverändert an die View weiter. Jahres-Gruppierung/-Summierung sowie
    // die Umschaltung Final-/Market-Felder für Gewinne/Verluste geschieht in
    // der View (OverviewTabWidget/ViewShareDetails), identisch zum bisherigen
    // Muster in ViewSaleEdit/ViewDividendEdit/ViewBrokerageEdit.
    void populateGewinneVerluste();
    void populateDividenden();
    void populateKosten();

    IViewShareDetails&  m_view;
    IModelShareDetails& m_model;
    QString             m_shareGuid;
    bool                m_marketValueMode;
};

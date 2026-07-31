// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QString>
#include <QList>
#include <QColor>

#include "../../models/SaleObject.h"
#include "../../models/DividendObject.h"
#include "../../models/BrokerageObject.h"

/**
 * @brief One line of a "Bestandsberechnung" box (Gesamt/Vortag/Aktuelle).
 *
 * Rendered as three columns: a small leading operator ("×", "=", "+", "−",
 * or empty), a label, and a right-aligned value. All text is pre-formatted
 * by PresenterShareDetails (locale-aware number formatting, sign handling) —
 * the View performs no computation, only layout.
 */
struct CalculationRow
{
    QString operatorSymbol; ///< "", "×", "=", "+", "−"
    QString label;
    QString value;
    QColor  color;          ///< Invalid QColor() -> default text color.
    bool    emphasize = false; ///< Bold label+value (used for "=" result rows).
};

using CalculationRows = QList<CalculationRow>;

/**
 * @brief Passive view interface for the share-details dialog.
 *
 * Implemented by ViewShareDetails (production, QDialog-based) and by a fake
 * in tst_sharedetailsform.cpp for isolated Presenter tests.
 *
 * @note Erweitert 13.07.2026 um populateGewinneVerluste()/populateDividenden()/
 * populateKosten() für die drei neuen, nur im Depotwert-Modus sichtbaren Tabs
 * (siehe ARCHITECTURE.md, "ShareDetailsForm-Details"). Alle drei sind reine
 * Objekt-Listen-Übergaben — die View reicht sie unverändert an je eine
 * OverviewTabWidget-Instanz weiter, die Jahres-Gruppierung und -Summierung
 * geschieht dort (identisches Muster zu ViewSaleEdit::populateOverview() etc.).
 */
class IViewShareDetails
{
public:
    virtual ~IViewShareDetails() = default;

    /** Window title / share name. Plain name for now — see ViewShareDetails
     *  for why the full "Zeitraum: ... / Entwicklung: ..." subtitle from the
     *  C# reference is deferred until the chart tab is implemented. */
    virtual void setHeaderName(const QString& name) = 0;

    /** "Letzte Internet-Aktualisierung: ... / Typ: ..." status line. */
    virtual void setStatusLine(const QString& statusText) = 0;

    /** "Letzte Website-Aktualisierung: ..." bar inside the Depotwert-/Marktwert-tab. */
    virtual void setWebsiteUpdateLine(const QString& statusText) = 0;

    /**
     * @brief Form-weite Warnzeile ("Aktie sollte aktualisiert werden! Daten
     * sind evtl. nicht auf dem aktuellen Stand."), unterhalb des Tab-Widgets —
     * ergänzt 30.07.2026, portiert von toolStripStatusLabelUpdate in der
     * C#-Referenz (FrmShareDetails_Shown()). Leerer String versteckt die
     * Zeile; Text kommt bereits fertig formatiert vom Presenter, die View
     * layoutet nur (dieselbe Konvention wie setStatusLine()/
     * setWebsiteUpdateLine()).
     */
    virtual void setUpdateWarning(const QString& text) = 0;

    /** Tab title: "Komplette Depotbewertung" or "Komplette Marktbewertung", depending on mode. */
    virtual void setBoxesTabTitle(const QString& title) = 0;

    virtual void populateGesamtBox(const CalculationRows& rows) = 0;
    virtual void populateVortagBox(const CalculationRows& rows) = 0;
    virtual void populateAktuelleBox(const CalculationRows& rows) = 0;

    // ── Gewinne/Verluste-, Dividenden-, Kosten-Tabs (nur Depotwert-Modus) ──
    // Werden von PresenterShareDetails nur aufgerufen, wenn der Dialog im
    // Depotwert-Modus geöffnet wurde; die View legt die zugehörigen Tabs auch
    // nur in diesem Fall an (siehe ViewShareDetails::setupUi()).

    /** Befüllt den "Gewinne/Verluste"-Tab mit allen Verkäufen der Aktie. */
    virtual void populateGewinneVerluste(const QList<SaleObject>& sales) = 0;

    /** Befüllt den "Dividenden"-Tab mit allen Dividendenzahlungen der Aktie. */
    virtual void populateDividenden(const QList<DividendObject>& dividends) = 0;

    /** Befüllt den "Kosten"-Tab mit allen Kosten-Einträgen der Aktie. */
    virtual void populateKosten(const QList<BrokerageObject>& brokerages) = 0;

    // ── Fehler / Lifecycle ────────────────────────────────────────────────
    /** Shows a modal error message (QMessageBox::critical convention). */
    virtual void showError(const QString& message) = 0;
    /** Closes the dialog (QDialog::reject()) — used when the share was not found. */
    virtual void closeDialog() = 0;
};

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "SaleBuyDetailRow.h"
#include "../../models/SaleObject.h"
#include "../../models/BuyObject.h"
#include "../../models/BrokerageObject.h"
#include "../../models/ShareSplitObject.h"

#include <QList>
#include <QString>
#include <QStringList>

/**
 * @brief Abstract view interface for the "Verkäufe hinzufügen / editieren" dialog.
 *
 * Mirrors the structure of IViewBuyEdit, adapted for sale-specific fields.
 */
class IViewSaleEdit
{
public:
    virtual ~IViewSaleEdit() = default;

    // ── Form read (user input) ────────────────────────────────────────────
    virtual QString dateTime()     const = 0;
    virtual QString depotNumber()  const = 0;
    virtual QString orderNumber()  const = 0;
    virtual double  volume()       const = 0;
    virtual double  salePrice()    const = 0;
    virtual double  taxAtSource()  const = 0;
    virtual double  capitalGainsTax() const = 0;
    virtual double  solidarityTax() const = 0;
    virtual double  provision()    const = 0;
    virtual double  brokerFee()    const = 0;
    virtual double  traderFee()    const = 0;
    virtual double  reduction()    const = 0;
    virtual QString documentPath() const = 0;

    // ── Form population (Presenter → View) ───────────────────────────────
    virtual void loadSale(const SaleObject& sale) = 0;
    virtual void clearForm()                      = 0;

    // ── Available buys (for Details button) ──────────────────────────────
    virtual void populateAvailableBuys(const QList<BuyObject>& buys) = 0;

    /** Replace the full buy list shown in the buy-allocation widget. */
    virtual void setAllBuys(const QList<BuyObject>& buys) = 0;

    /**
     * @brief Replace the share's splits, used for split-aware FIFO display
     * in onShowDetails() (Aktiensplit-Behandlung, Phase 2c, 07.08.2026).
     * Set once by the Presenter's constructor — splits practically never
     * change during a dialog session, same pattern as setAllBuys().
     */
    virtual void setSplits(const QList<ShareSplitObject>& splits) = 0;

    // ── Derived value display ─────────────────────────────────────────────
    virtual void setSaleValue(double value)        = 0;
    virtual void setKaufwert(double value)         = 0;
    virtual void setGewinnVerlust(double value)    = 0;
    virtual void setGesGebuehren(double value)     = 0;
    virtual void setTaxSum(double value)           = 0;
    virtual void setAuszahlung(double value)       = 0;

    /**
     * @brief Zeigt den Split-Hinweis unter den Verkaufsdaten (Phase 3b, 09.08.2026).
     *
     * Wortgleich zu IViewBuyEdit::setSplitHint(): die Zeile ist immer
     * sichtbar, auch ohne Split, damit das Formular beim Ändern des Datums
     * nicht springt (Nessies Entscheidung 08.08.2026).
     *
     * @param text      Fusszeilen-Text aus ShareSplitHint.
     * @param tooltip   Vollständige Split-Liste, leer wenn keine vorhanden.
     * @param hasSplit  true, wenn nach dem Belegdatum ein Split liegt.
     */
    virtual void setSplitHint(const QString& text,
                              const QString& tooltip,
                              bool hasSplit) = 0;

    // ── Field status ──────────────────────────────────────────────────────
    /**
     * @brief Set a field's value and mark it as Ok (green checkmark).
     * @param field  Field key matching m_inputWidgets / m_statusLabels.
     * @param value  Locale-aware string value from the parser.
     */
    virtual void setFieldOk(const QString& field, const QString& value) = 0;

    /**
     * @brief Mark a field as Error (red cross icon).
     * @param field  Field key matching m_statusLabels.
     */
    virtual void setFieldError(const QString& field) = 0;

    // ── Document ──────────────────────────────────────────────────────────
    virtual void setDocumentPreview(const QString& text) = 0;

    // ── Parse status bar ──────────────────────────────────────────────────
    virtual void setParseProgress(int percent, const QString& status) = 0;
    virtual void setParseStatusIcon(int iconType)                     = 0;
    virtual void setUiBusy(bool busy)                                 = 0;
    virtual void onParseFinished()                                    = 0;

    // ── Details-Dialog (Presenter → View) ────────────────────────────────
    /**
     * @brief Zeigt den Dialog "Details der verwendeten Käufe" an.
     *
     * Die Aufbereitung der Zeilen liegt seit dem Bugfix "anteilige
     * Kauf-Nebenkosten gehen bei der FIFO-Zuteilung verloren" vollständig
     * im Presenter (siehe ARCHITECTURE.md). Grund: die anteilige
     * Kauf-Brokerage kommt über `IModelSaleEdit::loadBrokerageForBuy()`,
     * und die View hat per MVP keinen Modellzugriff. Vorher setzte
     * `ViewSaleEdit::onShowDetails()` die Kosten im Live-FIFO-Zweig
     * hart auf 0,00 €.
     *
     * Die View rendert nur noch — alle Werte in @p summary liegen bereits
     * in heutiger (split-bereinigter) Skala vor.
     */
    virtual void showBuyDetails(const SaleBuyDetailSummary& summary) = 0;

    // ── Overview table (Presenter → View) ────────────────────────────────
    /**
     * @brief Baut die Verkaufs-Übersicht neu auf.
     *
     * @param sales   Alle Verkäufe der Aktie.
     * @param splits  Alle Splits der Aktie (Phase 3c, 11.08.2026).
     *
     * Die Splits kommen bewusst als Parameter herein und nicht über einen
     * eigenen `setSplits()`-Aufruf: sonst entstünde eine unsichtbare
     * Reihenfolge-Abhängigkeit zwischen zwei View-Aufrufen, die erst
     * auffällt, wenn sie einmal falsch herum steht. Gleiche Bauweise wie
     * IViewBuyEdit::populateOverview().
     *
     * Zeilen der Jahres-Tabs bleiben in BELEG-Skala (sie sind Abschriften
     * des Dokuments, das nach einem Zeilenklick rechts erscheint) und tragen
     * bei Bedarf den Split-Marker; alle Summen — Fusszeilen und die
     * Jahreszeilen des Übersicht-Tabs — stehen auf heutiger Skala. Siehe
     * ARCHITECTURE.md, "Split-Marker und Summen in den Übersichtstabellen".
     */
    virtual void populateOverview(const QList<SaleObject>&       sales,
                                  const QList<ShareSplitObject>& splits) = 0;

    /** Switch to the Gesamtübersicht tab (index 0) and clear the form. */
    virtual void showOverviewTab() = 0;

    // ── PDF preview ───────────────────────────────────────────────────────
    virtual void openPdfPreview(const QString& pdfPath) = 0;
    virtual void clearPdfPreview()                       = 0;

    // ── Button state ──────────────────────────────────────────────────────
    /**
     * @brief Update button labels and field editability for the current mode.
     * @param canRemove  true when the selected sale may be deleted.
     * @param isLastSale true when the selected sale is the most recent one.
     * @param isEdit     true when any existing sale is selected (shows "Speichern").
     */
    virtual void setButtonStates(bool canRemove, bool isLastSale, bool isEdit) = 0;

    // ── Feedback ──────────────────────────────────────────────────────────
    virtual void showError(const QString& message) = 0;
    virtual void acceptAndClose()                  = 0;

    // ── Validation ────────────────────────────────────────────────────────
    virtual void markMissingFieldsAsFailed()                                   = 0;
    virtual bool hasMissingRequiredFields(QStringList& missingFields) const    = 0;
};

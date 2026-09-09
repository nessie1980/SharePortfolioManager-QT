// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../../models/BrokerageObject.h"

#include <QList>
#include <QString>
#include <QStringList>

/**
 * @brief Abstract view interface for the "Kosten hinzufügen / editieren" dialog.
 *
 * Simpler than IViewBuyEdit / IViewDividendEdit — no parse pipeline because
 * brokerage records are entered manually. Like DividendForm, every record is
 * fully editable and deletable at any time (no latest-entry restriction).
 */
class IViewBrokerageEdit
{
public:
    virtual ~IViewBrokerageEdit() = default;

    // ── Form read (user input) ────────────────────────────────────────────
    virtual QString dateTime()     const = 0;
    virtual double  provision()    const = 0;
    virtual double  brokerFee()    const = 0;
    virtual double  traderFee()    const = 0;
    virtual double  reduction()    const = 0;
    virtual QString documentPath() const = 0;

    // ── Form population (Presenter → View) ───────────────────────────────
    virtual void loadBrokerage(const BrokerageObject& brokerage) = 0;
    virtual void clearForm()                                       = 0;

    // ── Derived value display ─────────────────────────────────────────────
    /** Total fees = provision + brokerFee + traderFee. */
    virtual void setGesamtGebuehren(double value)    = 0;
    /** Net cost = brokerage - reduction. */
    virtual void setBrokerageReduction(double value) = 0;

    // ── Document / PDF preview ────────────────────────────────────────────
    virtual void setDocumentPreview(const QString& text) = 0;

    /** Open a PDF or image file in the right-hand preview panel. */
    virtual void openPdfPreview(const QString& pdfPath) = 0;

    /** Clear the preview panel (e.g. when the form is reset). */
    virtual void clearPdfPreview() = 0;

    // ── Overview table (Presenter → View) ────────────────────────────────
    virtual void populateOverview(const QList<BrokerageObject>& brokerages) = 0;

    /** Switch to the Gesamtübersicht tab (index 0) and clear the form. */
    virtual void showOverviewTab() = 0;

    // ── Button state ──────────────────────────────────────────────────────
    /**
     * @brief Update button labels and field editability.
     * @param canRemove  true when the selected record may be deleted.
     *                   Standalone records are always removable; records
     *                   linked to a buy/sale are not.
     * @param isEdit     true when any record is loaded (shows "Speichern").
     * @param readOnly   true when the loaded record belongs to a buy/sale
     *                   — only the document path is editable.
     */
    virtual void setButtonStates(bool canRemove, bool isEdit, bool readOnly) = 0;

    // ── Feedback ──────────────────────────────────────────────────────────
    virtual void showError(const QString& message) = 0;
    virtual void acceptAndClose()                  = 0;

    // ── Validation ────────────────────────────────────────────────────────
    virtual void markMissingFieldsAsFailed() = 0;
    virtual bool hasMissingRequiredFields(QStringList& missingFields) const = 0;

    /**
     * @brief Meldet die Zahlenfelder, deren Inhalt sich nicht als Zahl lesen
     *        laesst.
     *
     * Gegenstueck zu hasMissingRequiredFields(), fuer einen anderen Fehler:
     * dort ist ein Feld LEER, hier steht etwas drin, das keine gueltige
     * deutsche Zahl ist. Ueber NumberParser ergibt das 0,0 -- und 0,0 ist
     * hier ein gueltiger Wert, der Fehler bliebe also unsichtbar.
     *
     * Die View beantwortet nur, WAS sie nicht lesen konnte. Was daraus folgt
     * -- Meldung, Speichersperre --, entscheidet der Presenter.
     *
     * @param fieldNames  Wird geleert und mit den ANZEIGENAMEN der Felder
     *                    gefuellt ("Provision", "Verhaeltnis (neu)"), nicht
     *                    mit Feldschluesseln.
     * @return true, wenn mindestens ein Feld unlesbar ist.
     *
     * @note Bewusst anders als in IViewBuyEdit/IViewSaleEdit/
     * IViewDividendEdit/IViewShareAdd, wo dieselbe Methode Feldschluessel
     * liefert. Dort markiert der Presenter die Felder ueber
     * setFieldError() rot und verweist in der Meldung darauf; dieser Dialog
     * hat keine Feldanzeige (Nessies Entscheidung 06.09.2026, keine
     * Statussymbole nachbauen), die Meldung muss die Felder also selbst
     * benennen. Die Namen bleiben damit dort, wo auch die Feldbeschriftungen
     * entstehen -- eine Uebersetzungstabelle im Presenter waere eine zweite
     * Quelle fuer dieselben Bezeichner. Gleiche Bauweise wie
     * hasMissingRequiredFields(), das hier ebenfalls Anzeigenamen liefert.
     *
     * Ergaenzt 09.09.2026, siehe ARCHITECTURE.md, "Unlesbare Zahleneingaben
     * in Kosten und Aktiensplits".
     */
    virtual bool hasUnreadableFields(QStringList& fieldNames) const = 0;

};

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../../models/BuyObject.h"
#include "../../models/BrokerageObject.h"
#include "../../models/ShareSplitObject.h"

#include <QList>
#include <QString>
#include <QStringList>

/**
 * @brief Abstract view interface for the "Käufe hinzufügen / editieren" dialog.
 *
 * Parse-related methods (setFieldOk, setFieldError, setParseProgress,
 * setParseStatusIcon, setUiBusy, onParseFinished) are 1:1 identical to
 * IViewShareAdd so PresenterBuyEdit can reuse the same calling conventions.
 */
class IViewBuyEdit
{
public:
    virtual ~IViewBuyEdit() = default;

    // ── Form read (user input) ────────────────────────────────────────────
    virtual QString dateTime()     const = 0;
    virtual QString depotNumber()  const = 0;
    virtual QString orderNumber()  const = 0;
    virtual double  volume()       const = 0;
    virtual double  price()        const = 0;
    virtual QString documentPath() const = 0;
    virtual double  provision()    const = 0;
    virtual double  brokerFee()    const = 0;
    virtual double  traderFee()    const = 0;
    virtual double  reduction()    const = 0;

    // ── Form population (Presenter → View) ───────────────────────────────
    virtual void loadBuy(const BuyObject& buy,
                         const BrokerageObject& brokerage) = 0;
    virtual void clearForm() = 0;

    // ── Derived value display ─────────────────────────────────────────────
    virtual void setVolumeSold(double value)   = 0;
    virtual void setKurswert(double value)     = 0;
    virtual void setGesGebuehren(double value) = 0;
    virtual void setEndbetrag(double value)    = 0;

    /**
     * @brief Zeigt den Split-Hinweis unter den Kaufdaten (Phase 3b, 09.08.2026).
     *
     * Die Zeile ist immer sichtbar — auch ohne Split, dann mit dem gedämpften
     * "kein Split"-Text. Andernfalls würden beim Tippen im Datumsfeld alle
     * darunter liegenden Zeilen springen (Nessies Entscheidung 08.08.2026).
     *
     * Der fertige Text kommt aus ShareSplitHint; die View entscheidet nur
     * noch über die Darstellung (gedämpft oder hervorgehoben).
     *
     * @param text      Fusszeilen-Text.
     * @param tooltip   Vollständige Split-Liste, leer wenn keine vorhanden.
     * @param hasSplit  true, wenn nach dem Belegdatum ein Split liegt.
     */
    virtual void setSplitHint(const QString& text,
                              const QString& tooltip,
                              bool hasSplit) = 0;

    // ── Field status (same API as IViewShareAdd) ──────────────────────────

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

    // ── Document (same API as IViewShareAdd) ──────────────────────────────

    /** Set document path text (called by presenter after PDF is selected). */
    virtual void setDocumentPreview(const QString& text) = 0;

    // ── Parse status bar (same API as IViewShareAdd) ──────────────────────

    /**
     * @brief Update the progress bar and status label.
     * @param percent  0–100.
     * @param status   Human-readable status text.
     */
    virtual void setParseProgress(int percent, const QString& status) = 0;

    /**
     * @brief Set the status icon next to the progress bar.
     * @param iconType  0 = SearchOk, 1 = SearchFailed, 2 = SearchInfo.
     */
    virtual void setParseStatusIcon(int iconType) = 0;

    /**
     * @brief Disable/enable all input widgets during parsing.
     * @param busy  true while parsing is running.
     */
    virtual void setUiBusy(bool busy) = 0;

    /**
     * @brief Called by the presenter when parsing has finished.
     *
     * Sets SearchInfo icon on required fields that were not touched by the
     * parser — identical to ViewShareAdd::onParseFinished().
     */
    virtual void onParseFinished() = 0;

    // ── Overview table (Presenter → View) ────────────────────────────────

    /**
     * @brief Baut die Kauf-Übersicht neu auf.
     *
     * @param buys        Alle Käufe der Aktie.
     * @param brokerages  Zum jeweiligen Kauf gehörende Kosten, index-parallel
     *        zu @p buys.
     * @param splits      Alle Splits der Aktie (Phase 3c, 10.08.2026).
     *
     * Die Splits kommen bewusst als Parameter herein und nicht über einen
     * eigenen `setSplits()`-Aufruf: sonst entstünde eine unsichtbare
     * Reihenfolge-Abhängigkeit zwischen zwei View-Aufrufen, die erst auffällt,
     * wenn sie einmal falsch herum steht.
     *
     * Zeilen der Jahres-Tabs bleiben in BELEG-Skala (sie sind Abschriften des
     * Dokuments, das nach einem Zeilenklick rechts erscheint); alle Summen —
     * Fusszeilen und die Jahreszeilen des Übersicht-Tabs — stehen auf
     * heutiger Skala. Siehe ARCHITECTURE.md, "Split-Marker und Summen in den
     * Übersichtstabellen".
     */
    virtual void populateOverview(const QList<BuyObject>&        buys,
                                  const QList<BrokerageObject>&  brokerages,
                                  const QList<ShareSplitObject>& splits) = 0;

    /** Switch to the Gesamtübersicht tab (index 0) and clear the form. */
    virtual void showOverviewTab() = 0;

    // ── PDF preview ───────────────────────────────────────────────────────
    virtual void openPdfPreview(const QString& pdfPath) = 0;
    virtual void clearPdfPreview()                       = 0;

    // ── Button state ──────────────────────────────────────────────────────
    /**
     * @brief Update button labels and field editability for the current mode.
     * @param canRemove  true when the selected buy may be deleted.
     * @param isLastBuy  true when the selected buy is the most recent one.
     * @param isEdit     true when any existing buy is selected (shows "Speichern").
     */
    virtual void setButtonStates(bool canRemove, bool isLastBuy, bool isEdit) = 0;

    // ── Feedback ──────────────────────────────────────────────────────────
    virtual void showError(const QString& message) = 0;
    virtual void acceptAndClose()                  = 0;

    // ── Validation (same API as IViewShareAdd) ────────────────────────────
    virtual void markMissingFieldsAsFailed() = 0;
    virtual bool hasMissingRequiredFields(QStringList& missingFields) const = 0;
};

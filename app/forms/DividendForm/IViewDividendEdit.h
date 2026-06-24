// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../../models/DividendObject.h"

#include <QList>
#include <QString>
#include <QStringList>

/**
 * @brief Abstract view interface for the "Dividende hinzufügen / editieren" dialog.
 *
 * Parse-related methods (setFieldOk, setFieldError, setParseProgress,
 * setParseStatusIcon, setUiBusy, onParseFinished) follow the same conventions
 * as IViewBuyEdit so PresenterDividendEdit can reuse the same calling patterns.
 */
class IViewDividendEdit
{
public:
    virtual ~IViewDividendEdit() = default;

    // ── Form read (user input) ────────────────────────────────────────────
    virtual QString dateTime()              const = 0;
    virtual double  rate()                  const = 0;  ///< Dividendensatz je Aktie
    virtual double  volume()                const = 0;  ///< Anteile am Auszahlungstag
    virtual double  taxAtSource()           const = 0;
    virtual double  capitalGainsTax()       const = 0;
    virtual double  solidarityTax()         const = 0;
    virtual double  priceAtPayday()         const = 0;  ///< Kurspreis am Auszahlungstag
    virtual bool    enableForeignCurrency() const = 0;
    virtual double  exchangeRatio()         const = 0;  ///< Devisenkurs FC→EUR
    virtual QString currency()              const = 0;  ///< Währungskürzel (z.B. "USD")
    virtual QString documentPath()          const = 0;

    // ── Form population (Presenter → View) ───────────────────────────────
    virtual void loadDividend(const DividendObject& dividend) = 0;
    virtual void clearForm()                                   = 0;

    // ── Derived value display ─────────────────────────────────────────────
    virtual void setDividendPayout(double value)         = 0;  ///< Brutto-Auszahlung (€)
    virtual void setDividendPayoutFc(double value)       = 0;  ///< Brutto-Auszahlung (FC)
    virtual void setTaxSum(double value)                 = 0;  ///< Gezahlte Steuern
    virtual void setDividendPayoutWithTaxes(double value)= 0;  ///< Auszahlung nach Steuern
    virtual void setYield(double value)                  = 0;  ///< Dividenden-Rendite %

    // ── Foreign currency mode ─────────────────────────────────────────────
    /** Enable/disable the Fremdwährungs-Eingabe fields. */
    virtual void setForeignCurrencyEnabled(bool enabled) = 0;

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

    // ── Overview table (Presenter → View) ────────────────────────────────
    virtual void populateOverview(const QList<DividendObject>& dividends) = 0;

    /** Switch to the Gesamtübersicht tab (index 0) and clear the form. */
    virtual void showOverviewTab() = 0;

    // ── PDF preview ───────────────────────────────────────────────────────
    virtual void openPdfPreview(const QString& pdfPath) = 0;
    virtual void clearPdfPreview()                       = 0;

    // ── Button state ──────────────────────────────────────────────────────
    /**
     * @brief Update button labels and field editability for the current mode.
     * @param canRemove    true when a dividend is selected (always deletable).
     * @param isEdit       true when any existing dividend is selected.
     */
    virtual void setButtonStates(bool canRemove, bool isEdit) = 0;

    // ── Feedback ──────────────────────────────────────────────────────────
    virtual void showError(const QString& message) = 0;
    virtual void acceptAndClose()                  = 0;

    // ── Validation ────────────────────────────────────────────────────────
    virtual void markMissingFieldsAsFailed()                                = 0;
    virtual bool hasMissingRequiredFields(QStringList& missingFields) const = 0;
};

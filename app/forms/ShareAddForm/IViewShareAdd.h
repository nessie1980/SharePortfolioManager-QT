// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../../models/ShareObject.h"
#include "../../models/BuyObject.h"

#include <QString>
#include <QDateTime>

/**
 * @brief Abstract view interface for the "Aktie hinzufügen" dialog.
 *
 * Defines the contract between the Presenter and the View (Qt dialog).
 * All UI read/write operations are accessed through this interface,
 * keeping the Presenter independent of Qt widget internals.
 *
 * ### MVP wiring
 * - The **View** (ViewShareAdd) owns all widgets and implements this interface.
 * - The **Presenter** (PresenterShareAdd) calls these methods to read input
 *   and to populate fields after PDF parsing.
 * - The **Model** (ModelShareAdd) performs the actual database operations.
 */
class IViewShareAdd
{
public:
    virtual ~IViewShareAdd() = default;

    // ── Share master data ─────────────────────────────────────────────────
    virtual QString  wkn()              const = 0;
    virtual QString  isin()             const = 0;
    virtual QString  name()             const = 0;
    virtual QDate    listingDate()      const = 0;  ///< Börsennotierung
    virtual ShareType shareType()       const = 0;
    virtual QString  dividendInterval() const = 0;  ///< Dividendenausschüttungs-Intervall
    virtual QString  countryInfo()      const = 0;  ///< Länder-Info (locale string, e.g. "de-DE")
    virtual QString  detailsWebsite()   const = 0;

    // ── Data sources ──────────────────────────────────────────────────────
    virtual QString          marketPriceUrl()          const = 0;
    virtual ShareParsingType marketPriceParsingType()  const = 0;
    virtual QString          marketPriceApiKey()       const = 0;
    virtual QString          dailyValuesUrl()          const = 0;
    virtual ShareParsingType dailyValuesParsingType()  const = 0;
    virtual QString          dailyValuesApiKey()       const = 0;

    // ── Buy transaction data ──────────────────────────────────────────────
    virtual QDateTime buyDateTime()    const = 0;
    virtual QString   depotNumber()    const = 0;
    virtual QString   orderNumber()    const = 0;
    virtual double    volume()         const = 0;   ///< Anteile
    virtual double    price()          const = 0;   ///< Kurs
    virtual double    provision()      const = 0;
    virtual double    brokerFee()      const = 0;   ///< Courtage
    virtual double    traderFee()      const = 0;   ///< Handelsplatzgebühr
    virtual double    reduction()      const = 0;   ///< Rabatt
    virtual QString   documentPath()   const = 0;

    // ── Presenter → View: populate fields from PDF parse result ──────────

    /**
     * @brief Set a field value and mark it with a green check (parse hit).
     * @param field  One of: "wkn","isin","name","date","time","depotNumber",
     *               "orderNumber","volume","price","provision","brokerFee",
     *               "traderFee","reduction"
     * @param value  The parsed string value.
     */
    virtual void setFieldOk(const QString& field, const QString& value) = 0;

    /**
     * @brief Mark a field with a red X (parse miss — required field not found).
     * @param field  Same field names as setFieldOk().
     */
    virtual void setFieldError(const QString& field) = 0;

    /**
     * @brief Write @p path into the document path field (and, where present,
     * update the type-fallback icon and the PDF preview).
     *
     * Called by the presenter so both entry paths — the manual "…"-Browse
     * click (ViewShareAdd::onBrowseDocument()) and a document dropped onto
     * "Direkte Dokumentenerfassung" (MainWindow::openCaptureDialog() →
     * dlg.presenter()->onDocumentSelected()) — end up showing the same
     * document path in the dialog. Bugfix 21.08.2026, see IViewBuyEdit.h /
     * ARCHITECTURE.md.
     */
    virtual void setDocumentPath(const QString& path) = 0;

    /**
     * @brief Show a PDF preview text in the document preview panel.
     * @param text  Plain text extracted from the PDF.
     */
    virtual void setDocumentPreview(const QString& text) = 0;

    /**
     * @brief Display an error message to the user (e.g. save failed).
     * @param message  Human-readable error string.
     */
    virtual void showError(const QString& message) = 0;

    /**
     * @brief Switch all fields still in Info state to Error (SearchFailed icon).
     *
     * Called by the Presenter when the user tries to save with missing required
     * fields. The Info icon (SearchInfo) becomes the Failed icon (SearchFailed)
     * so the user sees at a glance which fields still need to be filled.
     */
    virtual void markMissingFieldsAsFailed() = 0;

    /**
     * @brief Returns true if any required field still has Info or Error state.
     *
     * Called by the Presenter before saving to block saves with missing values.
     * @param missingFields  Filled with the display names of missing fields.
     * @return true if at least one required field is not in Ok state.
     */
    virtual bool hasMissingRequiredFields(QStringList& missingFields) const = 0;

    /**
     * @brief Show parse progress in the status area.
     * @param percent  0–100
     * @param status   Human-readable status text (e.g. current regex field name)
     */
    virtual void setParseProgress(int percent, const QString& status) = 0;

    /**
     * @brief Set the final icon in the status bar (Ok/Failed/Info).
     * Called by the Presenter after all fields are processed.
     * @param iconType  0 = SearchOk, 1 = SearchFailed, 2 = SearchInfo
     */
    virtual void setParseStatusIcon(int iconType) = 0;

    /**
     * @brief Block or unblock the UI during parsing.
     * @param busy  true = disable form + show wait cursor, false = re-enable
     */
    virtual void setUiBusy(bool busy) = 0;

    /**
     * @brief Called by the Presenter after all parse results have been distributed.
     *
     * The View uses this to mark every registered field that received neither
     * setFieldOk() nor setFieldError() with the SearchInfo icon, indicating
     * the value is still missing and must be entered manually.
     */
    virtual void onParseFinished() = 0;

    /**
     * @brief Close the dialog after a successful save.
     */
    virtual void acceptAndClose() = 0;
};

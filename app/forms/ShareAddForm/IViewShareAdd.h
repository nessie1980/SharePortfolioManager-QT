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
     * @brief Setzt den Feldwert und markiert das Feld als Ok (gruener Haken).
     *
     * @param field   Feldschluessel wie in m_inputWidgets / m_statusLabels.
     * @param value   Rohwert aus dem Beleg (locale-behaftet). Leer bei den
     *                Aufrufen aus der Live-Validierung, die nur das Symbol
     *                setzen und den Feldinhalt nicht anfassen sollen.
     * @param tooltip Ersetzt den Standardtext am gruenen Symbol. Leer laesst
     *                den Standardtext stehen.
     *
     * @return true, wenn der Wert uebernommen wurde; false, wenn ein
     *         nicht-leerer Rohwert nicht ins Zielfeld passte — unbrauchbares
     *         Datum, unbrauchbare Uhrzeit, oder eine Depotnummer, die nicht
     *         in Documents.xml hinterlegt ist. In diesem Fall hat die View
     *         bereits setFieldError() mit dem Rohwert aufgerufen; der
     *         Presenter muss nur noch seine Zaehlung anpassen.
     *
     * Der Rueckgabewert kam am 27.08.2026 dazu. Vorher zaehlte
     * populateFromResult() jeden gefangenen Wert als Treffer, auch wenn die
     * View ihn verworfen hatte — die Statuszeile meldete dann "Analyse OK —
     * 5/5 Pflicht", waehrend am Feld das rote Symbol stand. Siehe
     * ARCHITECTURE.md, "Analyse-Statuszeile und Feldsymbole".
     */
    virtual bool setFieldOk(const QString& field, const QString& value,
                            const QString& tooltip = QString()) = 0;

    /**
     * @brief Markiert ein Feld als fehlerhaft (rotes Symbol).
     *
     * @param field     Feldschluessel wie in m_statusLabels.
     * @param rawValue  Der Rohwert, an dem die Uebernahme gescheitert ist.
     *                  Er wandert in den Tooltip des Symbols, damit beim
     *                  Schreiben von Regeln fuer Documents.xml sichtbar ist,
     *                  WAS gefangen wurde. Leer erzeugt den bisherigen,
     *                  allgemeinen Tooltip — so rufen ihn die Aufrufe aus der
     *                  Live-Validierung auf, wo es keinen Rohwert gibt.
     */
    virtual void setFieldError(const QString& field,
                               const QString& rawValue = QString()) = 0;

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

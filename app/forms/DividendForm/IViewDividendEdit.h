// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../../models/DividendObject.h"
#include "../../models/ShareSplitObject.h"

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

    /**
     * @brief Ex-Tag als ISO-8601-Datumsstring (z.B. "2024-05-13").
     *
     * Pflichtfeld seit 21.08.2026 (siehe DividendObject.h, "Ex-Tag und
     * Depotnummer"). Der Sentinel-Wert "2000-01-01" bedeutet "vom Benutzer
     * noch nicht gesetzt" — genau wie bei date()/dateTime() gibt es bei
     * QDateEdit kein echtes "leer", darum markiert hasMissingRequiredFields()
     * diesen Sentinel als fehlende Pflichtangabe.
     */
    virtual QString exDate()                const = 0;

    /**
     * @brief Depotnummer, in die die Dividende ausgezahlt wurde.
     *
     * Pflichtfeld seit 21.08.2026, identisches Verhalten wie
     * IViewBuyEdit::depotNumber() — leerer String, wenn keine Depotnummer
     * ausgewählt ist.
     */
    virtual QString depotNumber()           const = 0;

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

    /**
     * @brief Setzt den Fremdwährungs-Modus samt Währungsauswahl.
     *
     * Phase 5 der Ex-Tag-Behandlung (21.08.2026). Anders als
     * setForeignCurrencyEnabled(), das nur die Eingabefelder frei- oder
     * sperrt, setzt diese Methode auch den Haken "Fremdwährungseingabe
     * aktivieren" selbst und wählt die passende Währung aus.
     *
     * Nötig, weil ein aus dem Beleg gelesener Devisenkurs sonst wirkungslos
     * bliebe: `PresenterDividendEdit::onSave()` übernimmt `exchangeRatio()`
     * nur, wenn `enableForeignCurrency()` true ist. Vor Phase 5 wurde der
     * Kurs zwar ins Feld geschrieben, aber beim Speichern verworfen.
     *
     * @param enabled  Fremdwährungs-Modus ein- oder ausschalten.
     * @param isoCode  ISO-4217-Kürzel aus dem Beleg ("USD", "GBP", …).
     *        Leer oder unbekannt lässt die Auswahl unverändert.
     */
    virtual void setForeignCurrency(bool enabled, const QString& isoCode) = 0;

    // ── Field status ──────────────────────────────────────────────────────
    /**
     * @brief Set a field's value and mark it as Ok (green checkmark).
     * @param field   Field key matching m_inputWidgets / m_statusLabels.
     * @param value   Locale-aware string value from the parser.
     * @param tooltip Optional tooltip override (e.g. to indicate the value
     *                was auto-filled from stored daily values rather than
     *                entered/parsed). Empty string keeps the default tooltip.
     */
    virtual void setFieldOk(const QString& field, const QString& value,
                            const QString& tooltip = QString()) = 0;

    /**
     * @brief Mark a field as Error (red cross icon).
     * @param field  Field key matching m_statusLabels.
     */
    virtual void setFieldError(const QString& field) = 0;

    /**
     * @brief Hängt einen Hinweis an ein Feld, OHNE dessen Wert zu setzen und
     *        ohne es als ausgefüllt zu markieren.
     *
     * Ergänzt 21.08.2026 für Belege, die eine Pflichtangabe nicht selbst
     * nennen, aber eine Angabe enthalten, aus der sie sich herleiten lässt.
     * Konkreter Anlass: Cortal Consors nennt keinen Ex-Tag, wohl aber den
     * "Schlusstag" (Dividenden-Stichtag), der laut Bank üblicherweise einen
     * Tag davor liegt. Den Ex-Tag daraus zu berechnen wäre geraten — der
     * nächste HANDELStag hängt von Wochenenden und Feiertagen ab, und ein um
     * einen Tag falscher Ex-Tag ginge unmittelbar in die
     * Stückzahl-Plausibilitätsprüfung ein. Der Benutzer bekommt die Zahl
     * deshalb angezeigt und trägt sie selbst ein.
     *
     * Das Feld erhält das Info-Symbol (dasselbe wie in onParseFinished()) und
     * @p tooltip als Erklärung. Es zählt weiterhin als fehlende Pflichtangabe
     * — `hasMissingRequiredFields()` bleibt davon unberührt.
     *
     * @param field    Feldschlüssel wie in setFieldOk()/setFieldError().
     * @param tooltip  Erklärender Text; leer entfernt den Hinweis nicht,
     *                 sondern setzt lediglich das Info-Symbol.
     */
    virtual void setFieldHint(const QString& field, const QString& tooltip) = 0;

    // ── Document ──────────────────────────────────────────────────────────
    /**
     * @brief Write @p path into the document path field.
     *
     * Called by the presenter so both entry paths — the manual "…"-Browse
     * click (ViewDividendEdit::onBrowseDocument()) and a document dropped
     * onto "Direkte Dokumentenerfassung" (MainWindow::handleDroppedDocument()
     * → dlg.presenter()->onDocumentSelected()) — end up showing the same
     * document path in the dialog. Bugfix 21.08.2026, see IViewBuyEdit.h /
     * ARCHITECTURE.md.
     */
    virtual void setDocumentPath(const QString& path) = 0;
    virtual void setDocumentPreview(const QString& text) = 0;

    // ── Parse status bar ──────────────────────────────────────────────────
    virtual void setParseProgress(int percent, const QString& status) = 0;
    virtual void setParseStatusIcon(int iconType)                     = 0;
    virtual void setUiBusy(bool busy)                                 = 0;
    virtual void onParseFinished()                                    = 0;

    // ── Overview table (Presenter → View) ────────────────────────────────
    /**
     * @brief Baut die Dividenden-Übersicht neu auf.
     *
     * @param dividends  Alle Dividenden der Aktie.
     * @param splits     Alle Splits der Aktie (Phase 3c, 11.08.2026).
     *
     * Die Splits kommen als Parameter herein und nicht über einen eigenen
     * Setter — gleiche Bauweise wie IViewBuyEdit/IViewSaleEdit, damit keine
     * unsichtbare Reihenfolge-Abhängigkeit zwischen zwei View-Aufrufen
     * entsteht.
     *
     * Anders als bei Käufen und Verkäufen werden die Anteile hier NICHT auf
     * heutige Skala umgerechnet: "Anteile am Auszahlungstag" bezieht sich auf
     * einen Stichtag, und eine Summe über mehrere Stichtage beschreibt keinen
     * Bestand. Die Belegzeilen tragen den Marker, die Summenzelle zeigt "-".
     * Siehe ARCHITECTURE.md, "Split-Marker und Summen in den
     * Übersichtstabellen".
     */
    virtual void populateOverview(const QList<DividendObject>&   dividends,
                                  const QList<ShareSplitObject>& splits) = 0;

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

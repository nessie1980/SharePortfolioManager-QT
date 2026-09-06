// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IViewDividendEdit.h"
#include "../../config/DocumentsConfig.h"

#include <QDialog>
#include <QCheckBox>
#include <QDateEdit>
#include <QTimeEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QTableWidget>
#include <QGridLayout>
#include <QProgressBar>
#include <QMap>

#include "../../widgets/OverviewTabWidget.h"
#include "../../widgets/DocumentPreviewPanel.h"

class PresenterDividendEdit;

/**
 * @brief Qt dialog implementing IViewDividendEdit.
 *
 * Layout:
 * ┌── Dividende hinzufügen ────────────────┬── Dokumenten-Vorschau ──────────┐
 * │  Datum / Uhrzeit                        │  DocumentPreviewPanel            │
 * │  [✓] Fremdwährungseingabe aktivieren    │                                 │
 * │  Devisenkurs          [combo: en-US/$]  │                                 │
 * │  Dividendensatz           [rate]  $/€   │                                 │
 * │  Anteile am Auszahlungstag  [vol] stk.  │                                 │
 * │  Auszahlung (€)  [ro]   €  [ro FC]  $  │                                 │
 * │  Quellsteuer              [tax]   €     │                                 │
 * │  Kapitalertragssteuer     [tax]   €     │                                 │
 * │  Solidaritätszuschlag     [tax]   €     │                                 │
 * │  Gezahlte Steuern         [ro]    €     │                                 │
 * │  Auszahlung nach Steuern  [ro]    €     │                                 │
 * │  Dividenden-Rendite       [ro]    %     │                                 │
 * │  Preis der Aktie am Zahltag [edit] €    │                                 │
 * ├── Dokument ─────────────────────────── │                                 │
 * │  [path]  [📁]                          │                                 │
 * │  [progress] [icon] [status]            │                                 │
 * ├──────────────────────────────────────  │                                 │
 * │  [Hinzufügen] [Entfernen] [Reset] [Schließen]                           │
 * └────────────────────────────────────────┴─────────────────────────────────┘
 * ┌── Dividenden-Übersicht ────────────────────────────────────────────────────┐
 * │  [Tab: Übersicht (X.XX €)] [Tab: 2024 (X.XX €)] ...                      │
 * └────────────────────────────────────────────────────────────────────────────┘
 */
class ViewDividendEdit : public QDialog, public IViewDividendEdit
{
    Q_OBJECT

public:
    enum class FieldState { Untouched, Ok, Error, Info };

    explicit ViewDividendEdit(const QString& shareGuid,
                              DocumentsConfig* config,
                              QWidget* parent = nullptr);
    ~ViewDividendEdit() override = default;

    PresenterDividendEdit* presenter() const { return m_presenter; }

    // ── IViewDividendEdit read ────────────────────────────────────────────
    QString dateTime()              const override;
    double  rate()                  const override;
    double  volume()                const override;
    double  taxAtSource()           const override;
    double  capitalGainsTax()       const override;
    double  solidarityTax()         const override;
    double  priceAtPayday()         const override;
    bool    enableForeignCurrency() const override;
    double  exchangeRatio()         const override;
    QString currency()              const override;
    QString documentPath()          const override;
    QString exDate()                const override;
    QString depotNumber()           const override;

    // ── IViewDividendEdit write ───────────────────────────────────────────
    void loadDividend(const DividendObject& dividend)    override;
    void clearForm()                                      override;

    void setDividendPayout(double value)          override;
    void setDividendPayoutFc(double value)        override;
    void setTaxSum(double value)                  override;
    void setDividendPayoutWithTaxes(double value) override;
    void setYield(double value)                   override;

    void setForeignCurrencyEnabled(bool enabled) override;
    void setForeignCurrency(bool enabled, const QString& isoCode) override;

    bool setFieldOk(const QString& field, const QString& value,
                    const QString& tooltip = QString()) override;
    void setFieldError(const QString& field,
                       const QString& rawValue = QString()) override;
    void setFieldHint(const QString& field, const QString& tooltip) override;
    void setDocumentPath(const QString& path)                   override;
    void setDocumentPreview(const QString& text)                override;

    void setParseProgress(int percent, const QString& status)   override;
    void setParseStatusIcon(int iconType)                       override;
    void setUiBusy(bool busy)                                   override;
    void onParseFinished()                                      override;

    void populateOverview(const QList<DividendObject>&   dividends,
                          const QList<ShareSplitObject>& splits)      override;
    void openPdfPreview(const QString& pdfPath)                   override;
    void clearPdfPreview()                                         override;
    void showOverviewTab()                                         override;
    void setButtonStates(bool canRemove, bool isEdit)                    override;
    void showError(const QString& message)                        override;
    void acceptAndClose()                                         override;

    void markMissingFieldsAsFailed()                                    override;
    bool hasMissingRequiredFields(QStringList& missingFields) const     override;

private slots:
    void onBrowseDocument();

private:
    void       setupUi();
    QGroupBox* createDividenddatenGroup();
    QGroupBox* createDocumentGroup();
    QWidget*   createPreviewPanel();
    QWidget*   createButtonBar();
    QGroupBox* createOverviewGroup();

    QLabel* addRow(QGridLayout* grid, int& row,
                   const QString& labelText,
                   QWidget* field,
                   const QString& unitText  = QString(),
                   const QString& statusKey = QString());

    static QString formatMoney(double value);
    static QString formatVolume(double value);
    static QString formatPercent(double value);
    /**
     * @brief Liest ein Zahlenfeld dieses Dialogs (delegiert an NumberParser).
     * @param ok  Optional; false bei nicht leerem, unlesbarem Text.
     *            Vorgabe nullptr, damit die bestehenden Aufrufstellen
     *            unveraendert bleiben — ausgewertet wird das Flag erst mit
     *            der Rueckmeldung unlesbarer Eingaben.
     */
    static double  parseDouble(const QString& text, bool* ok = nullptr);

    // ── Dividendendaten GroupBox ──────────────────────────────────────────
    QGroupBox*   m_dividenddatenGroup = nullptr;  ///< Titel wechselt je Edit-Modus
    QDateEdit*   m_date              = nullptr;
    QTimeEdit*   m_time              = nullptr;
    QComboBox*   m_depotNumber       = nullptr;   ///< Depotnummer (Pflichtfeld seit 21.08.2026)
    QDateEdit*   m_exDate            = nullptr;   ///< Ex-Tag (Pflichtfeld seit 21.08.2026)
    QCheckBox*   m_enableFc          = nullptr;   ///< Fremdwährung aktivieren
    QLineEdit*   m_exchangeRatio     = nullptr;   ///< Devisenkurs
    QComboBox*   m_currency          = nullptr;   ///< Währungsauswahl
    QLineEdit*   m_rate              = nullptr;   ///< Dividendensatz je Aktie
    QLabel*      m_rateUnit          = nullptr;   ///< "€" oder Währung
    QLineEdit*   m_volume            = nullptr;   ///< Anteile am Auszahlungstag
    QLineEdit*   m_payout            = nullptr;   ///< Auszahlung € (read-only)
    QLineEdit*   m_payoutFc          = nullptr;   ///< Auszahlung FC (read-only)
    QLabel*      m_payoutFcUnit      = nullptr;   ///< Fremdwährungs-Einheits-Label
    QLineEdit*   m_taxAtSource       = nullptr;
    QLineEdit*   m_capitalGainsTax   = nullptr;
    QLineEdit*   m_solidarityTax     = nullptr;
    QLineEdit*   m_taxSum            = nullptr;   ///< Gezahlte Steuern (read-only)
    QLineEdit*   m_payoutWithTaxes   = nullptr;   ///< Auszahlung nach Steuern (read-only)
    QLineEdit*   m_yield             = nullptr;   ///< Rendite % (read-only)
    QLineEdit*   m_priceAtPayday     = nullptr;   ///< Kurspreis am Zahltag

    // ── Dokument GroupBox ─────────────────────────────────────────────────
    QLineEdit*    m_documentPath    = nullptr;
    QPushButton*  m_btnBrowse       = nullptr;
    QProgressBar* m_parseProgress   = nullptr;
    QLabel*       m_parseStatusIcon = nullptr;
    QLabel*       m_parseStatus     = nullptr;

    // ── inputWidgets / statusLabels ───────────────────────────────────────
    QMap<QString, QWidget*>   m_inputWidgets;
    QMap<QString, QLabel*>    m_statusLabels;
    QMap<QString, FieldState> m_fieldStates;

    // ── Action buttons ────────────────────────────────────────────────────
    QPushButton* m_btnAdd    = nullptr;
    QPushButton* m_btnRemove = nullptr;
    QPushButton* m_btnReset  = nullptr;
    QPushButton* m_btnClose  = nullptr;

    // ── Left panel (for setUiBusy) ────────────────────────────────────────
    QWidget*     m_leftPanel = nullptr;

    // ── PDF-Vorschau ──────────────────────────────────────────────────────
    DocumentPreviewPanel* m_previewPanel = nullptr;

    // ── Dividenden-Übersicht ──────────────────────────────────────────────
    OverviewTabWidget* m_overviewTabs      = nullptr;
    bool               m_suppressTabSignal = false;

    PresenterDividendEdit* m_presenter = nullptr;
    DocumentsConfig*       m_config    = nullptr;
};

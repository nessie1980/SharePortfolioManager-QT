// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IViewShareAdd.h"
#include "../../config/DocumentsConfig.h"
#include "../../widgets/DocumentPreviewPanel.h"

#include <QDialog>
#include <QDateEdit>
#include <QTimeEdit>
#include <QComboBox>
#include <QLineEdit>
#include <QGridLayout>
#include <QLabel>
#include <QScrollArea>
#include <QPushButton>
#include <QProgressBar>
#include <QGroupBox>

class PresenterShareAdd;

/**
 * @brief Qt dialog implementing IViewShareAdd — "Aktie hinzufügen".
 *
 * Left panel  (700 px fixed): two-column label/field form, all fields
 *             visible without scrolling.
 * Right panel (stretching):   PDF preview via `DocumentPreviewPanel`
 *             (nativer `QPdfView`, wenn Qt PDF verfügbar ist, sonst ein
 *             pdftoppm-gerendertes Bild — siehe ARCHITECTURE.md,
 *             "DocumentPreviewPanel"). Auf `DocumentPreviewPanel` umgestellt
 *             (19.07.2026), analog zu ViewBuyEdit/ViewSaleEdit/
 *             ViewDividendEdit/ViewBrokerageEdit — vorher eigenständige,
 *             nicht-delegierte QPdfView-/pdftoppm-Implementierung ohne
 *             Existenzprüfung der Datei (siehe ARCHITECTURE.md, "Offene
 *             Punkte / TODO").
 *
 * Status icons next to parsed fields use IconProvider::SearchOk /
 * IconProvider::SearchFailed (search_ok_24.png / search_failed_24.png).
 */
class ViewShareAdd : public QDialog, public IViewShareAdd
{
    Q_OBJECT

public:
    explicit ViewShareAdd(DocumentsConfig* config,
                          QWidget*         parent = nullptr);
    ~ViewShareAdd() override = default;

    // ── IViewShareAdd read accessors ──────────────────────────────────────
    QString  wkn()              const override;
    QString  isin()             const override;
    QString  name()             const override;
    QDate    listingDate()      const override;
    ShareType shareType()       const override;
    QString  dividendInterval() const override;
    QString  countryInfo()      const override;
    QString  detailsWebsite()   const override;

    QString          marketPriceUrl()         const override;
    ShareParsingType marketPriceParsingType() const override;
    QString          marketPriceApiKey()      const override;
    QString          dailyValuesUrl()         const override;
    ShareParsingType dailyValuesParsingType() const override;
    QString          dailyValuesApiKey()      const override;

    QDateTime buyDateTime()  const override;
    QString   depotNumber()  const override;
    QString   orderNumber()  const override;
    double    volume()       const override;
    double    price()        const override;
    double    provision()    const override;
    double    brokerFee()    const override;
    double    traderFee()    const override;
    double    reduction()    const override;
    QString   documentPath() const override;

    // ── IViewShareAdd write methods ───────────────────────────────────────
    void setFieldOk(const QString& field, const QString& value) override;
    void setFieldError(const QString& field)                    override;
    void setDocumentPreview(const QString& text)                override;
    void showError(const QString& message)                      override;
    void setParseProgress(int percent, const QString& status)   override;
    void setParseStatusIcon(int iconType)                       override;
    void setUiBusy(bool busy)                                   override;
    void markMissingFieldsAsFailed()                            override;
    bool hasMissingRequiredFields(QStringList& missingFields)   const override;
    void onParseFinished()                                      override;
    void acceptAndClose()                                       override;

private slots:
    void onBrowseDocument();
    void onMarketParsingTypeChanged(int index);
    void onDailyParsingTypeChanged(int index);
    void recalcDerivedValues();

private:
    void       setupUi();
    QWidget*   createFormPanel();
    QGroupBox* createGeneralGroup();
    QGroupBox* createDataSourcesGroup();
    QGroupBox* createBuyDataGroup();
    QGroupBox* createDocumentGroup();
    QWidget*   createButtonBar();
    QWidget*   createPreviewPanel();

    QLabel* addRow(QGridLayout* grid, int& row,
                   const QString& labelText,
                   QWidget* field,
                   const QString& unitText = QString());

    DocumentPreviewPanel* m_previewPanel = nullptr;

    // ── Form widgets ──────────────────────────────────────────────────────
    QLineEdit*      m_wkn            = nullptr;
    QLineEdit*      m_isin           = nullptr;
    QLineEdit*      m_name           = nullptr;
    QDateEdit*      m_listingDate    = nullptr;
    QComboBox*      m_shareType      = nullptr;
    QComboBox*      m_divInterval    = nullptr;
    QComboBox*      m_countryInfo    = nullptr;
    QLineEdit*      m_detailsWebsite = nullptr;

    QLineEdit*      m_marketUrl      = nullptr;
    QComboBox*      m_marketParsing  = nullptr;
    QLineEdit*      m_marketApiKey   = nullptr;
    QLineEdit*      m_dailyUrl       = nullptr;
    QComboBox*      m_dailyParsing   = nullptr;
    QLineEdit*      m_dailyApiKey    = nullptr;

    QDateEdit*      m_buyDate        = nullptr;
    QTimeEdit*      m_buyTime        = nullptr;
    QComboBox*      m_depotNumber    = nullptr;
    QLineEdit*      m_orderNumber    = nullptr;
    QLineEdit*      m_volume         = nullptr;   ///< editable, numeric (4 decimals)
    QLineEdit*      m_price          = nullptr;   ///< editable, numeric (4 decimals)
    QLineEdit*      m_kurswert       = nullptr;   ///< read-only
    QLineEdit*      m_provision      = nullptr;   ///< editable, numeric (2 decimals)
    QLineEdit*      m_brokerFee      = nullptr;   ///< editable, numeric (2 decimals)
    QLineEdit*      m_traderFee      = nullptr;   ///< editable, numeric (2 decimals)
    QLineEdit*      m_gesGebuehren   = nullptr;   ///< read-only
    QLineEdit*      m_reduction      = nullptr;   ///< editable, numeric (2 decimals)
    QLineEdit*      m_endbetrag      = nullptr;   ///< read-only

    QLineEdit*      m_documentPath   = nullptr;
    QPushButton*    m_btnBrowse      = nullptr;
    QPushButton*    m_btnSave        = nullptr;
    QPushButton*    m_btnCancel      = nullptr;

    // ── Form panel reference for bulk enable/disable during parsing ───────
    QWidget*        m_formPanel      = nullptr;

    // ── Parse progress (status bar at bottom of dialog) ───────────────────
    QProgressBar*   m_parseProgress  = nullptr;
    QLabel*         m_parseStatusIcon = nullptr;  ///< Icon left of status text
    QLabel*         m_parseStatus    = nullptr;

    // ── Parse status tracking ─────────────────────────────────────────────
    /// State of a single parsed field — drives the status icon and save validation.
    enum class FieldState {
        Untouched,  ///< Field has not been touched by the parser yet
        Ok,         ///< Parser found and filled the value  → SearchOk icon
        Error,      ///< Parser tried but failed (required) → SearchFailed icon
        Info        ///< Parser finished, value still empty  → SearchInfo icon
    };

    QMap<QString, QLabel*>    m_statusLabels;
    QMap<QString, QWidget*>   m_inputWidgets;
    QMap<QString, FieldState> m_fieldStates;  ///< Tracks parse result per field key

    PresenterShareAdd* m_presenter    = nullptr;
    DocumentsConfig*   m_config       = nullptr;
};

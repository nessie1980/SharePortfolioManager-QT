// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IViewSaleEdit.h"
#include "../../config/DocumentsConfig.h"
#include "../../widgets/OverviewTabWidget.h"
#include "../../widgets/DocumentPreviewPanel.h"

#include <QDialog>
#include <QDateEdit>
#include <QTimeEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QTableWidget>
#include <QGridLayout>
#include <QScrollArea>
#include <QProgressBar>
#include <QMap>

// weiterhin benoetigt fuer onShowDetails() (lokaler FIFO-Details-Dialog mit
// eigener, unabhaengiger PDF-Vorschau - nicht Teil der OverviewTabWidget/
// DocumentPreviewPanel-Migration, siehe ARCHITECTURE.md).
#ifdef SPM_HAVE_QTPDF
#  include <QPdfView>
#  include <QPdfDocument>
#endif

class PresenterSaleEdit;

/**
 * @brief Qt dialog implementing IViewSaleEdit.
 *
 * Layout mirrors ViewBuyEdit, with sale-specific fields:
 *
 * ┌── Verkaufsdaten ──────────────────────┬── Dokumenten-Vorschau ──────────┐
 * │  Datum / Uhrzeit                      │  QPdfView / pdftoppm            │
 * │  Depotnummer                          │                                 │
 * │  Ordernummer                          │                                 │
 * │  Verkaufte Anteile          stk.      │                                 │
 * │  Verkaufs-Preis einer Aktie   €       │                                 │
 * │  Verkaufter Kaufwert [ro]     €       │                                 │
 * │  Gewinn / Verlust   [ro]      €       │                                 │
 * │  Quellsteuer                  €       │                                 │
 * │  Kapitalertragssteuer         €       │                                 │
 * │  Solidaritätszuschlag         €       │                                 │
 * │  Provision                    €       │                                 │
 * │  Courtage                     €       │                                 │
 * │  Handelsplatzgebühr           €       │                                 │
 * │  Rabatt                       €       │                                 │
 * │  Ges. Gebühren     [ro]       €       │                                 │
 * │  Auszahlung        [ro, grün] €       │                                 │
 * ├── Dokument ──────────────────────────  │                                 │
 * │  [path]  [📁]  [Details]             │                                 │
 * │  [progress] [icon] [status]           │                                 │
 * ├─────────────────────────────────────  │                                 │
 * │  [Hinzufügen] [Entfernen] [Reset] [Schließen]                          │
 * └──────────────────────────────────────┴─────────────────────────────────┘
 * ┌── Verkaufs-Übersicht ──────────────────────────────────────────────────┐
 * │  [Tab: Übersicht] [Tab: 2024] ...                                      │
 * └────────────────────────────────────────────────────────────────────────┘
 */
class ViewSaleEdit : public QDialog, public IViewSaleEdit
{
    Q_OBJECT

public:
    enum class FieldState { Untouched, Ok, Error, Info };

    explicit ViewSaleEdit(const QString& shareGuid,
                          DocumentsConfig* config,
                          QWidget* parent = nullptr);
    ~ViewSaleEdit() override = default;

    PresenterSaleEdit* presenter() const { return m_presenter; }

    // ── IViewSaleEdit read ────────────────────────────────────────────────
    QString dateTime()        const override;
    QString depotNumber()     const override;
    QString orderNumber()     const override;
    double  volume()          const override;
    double  salePrice()       const override;
    double  taxAtSource()     const override;
    double  capitalGainsTax() const override;
    double  solidarityTax()   const override;
    double  provision()       const override;
    double  brokerFee()       const override;
    double  traderFee()       const override;
    double  reduction()       const override;
    QString documentPath()    const override;

    // ── IViewSaleEdit write ───────────────────────────────────────────────
    void loadSale(const SaleObject& sale)                               override;
    void clearForm()                                                     override;
    void populateAvailableBuys(const QList<BuyObject>& buys)            override;
    void setAllBuys(const QList<BuyObject>& buys)                       override;

    void setSaleValue(double value)     override;
    void setKaufwert(double value)      override;
    void setGewinnVerlust(double value) override;
    void setGesGebuehren(double value)  override;
    void setTaxSum(double value)        override;
    void setAuszahlung(double value)    override;

    void setFieldOk(const QString& field, const QString& value) override;
    void setFieldError(const QString& field)                    override;
    void setDocumentPreview(const QString& text)                override;

    void setParseProgress(int percent, const QString& status)   override;
    void setParseStatusIcon(int iconType)                       override;
    void setUiBusy(bool busy)                                   override;
    void onParseFinished()                                      override;

    void populateOverview(const QList<SaleObject>& sales)       override;
    void openPdfPreview(const QString& pdfPath)                 override;
    void clearPdfPreview()                                       override;
    void showOverviewTab()                                       override;
    void setButtonStates(bool canRemove, bool isLastSale, bool isEdit) override;
    void showError(const QString& message)                       override;
    void acceptAndClose()                                        override;

    void markMissingFieldsAsFailed()                                    override;
    bool hasMissingRequiredFields(QStringList& missingFields) const     override;

private slots:
    void onBrowseDocument();
    void onShowDetails();

private:
    void       setupUi();
    QGroupBox* createVerkaufsdatenGroup();
    QGroupBox* createDocumentGroup();
    QWidget*   createPreviewPanel();   // instanziiert jetzt DocumentPreviewPanel
    QWidget*   createButtonBar();
    QGroupBox* createOverviewGroup();  // nutzt jetzt OverviewTabWidget

    QLabel* addRow(QGridLayout* grid, int& row,
                   const QString& labelText,
                   QWidget* field,
                   const QString& unitText  = QString(),
                   const QString& statusKey = QString());

    static QString formatMoney(double value);
    static QString formatVolume(double value);
    static double  parseDouble(const QString& text);

    // ── Verkaufsdaten form ────────────────────────────────────────────────
    QDateEdit*   m_date             = nullptr;
    QTimeEdit*   m_time             = nullptr;
    QComboBox*   m_depotNumber      = nullptr;
    QLineEdit*   m_orderNumber      = nullptr;
    QLineEdit*   m_volume           = nullptr;
    QLineEdit*   m_salePrice        = nullptr;
    QLineEdit*   m_saleValue        = nullptr;   ///< read-only: vol × salePrice
    QLineEdit*   m_kaufwert         = nullptr;   ///< read-only: buy-side value
    QLineEdit*   m_gewinnVerlust    = nullptr;   ///< read-only: profit/loss
    QLineEdit*   m_taxAtSource      = nullptr;
    QLineEdit*   m_capitalGainsTax  = nullptr;
    QLineEdit*   m_solidarityTax    = nullptr;
    QLineEdit*   m_provision        = nullptr;
    QLineEdit*   m_brokerFee        = nullptr;
    QLineEdit*   m_traderFee        = nullptr;
    QLineEdit*   m_reduction        = nullptr;
    QLineEdit*   m_gesGebuehren     = nullptr;   ///< read-only
    QLineEdit*   m_auszahlung       = nullptr;   ///< read-only, green

    // ── Dokument GroupBox ─────────────────────────────────────────────────
    QLineEdit*    m_documentPath    = nullptr;
    QPushButton*  m_btnBrowse       = nullptr;
    QPushButton*  m_btnDetails      = nullptr;
    QProgressBar* m_parseProgress   = nullptr;
    QLabel*       m_parseStatusIcon = nullptr;
    QLabel*       m_parseStatus     = nullptr;

    // ── inputWidgets / statusLabels ───────────────────────────────────────
    QMap<QString, QWidget*>     m_inputWidgets;
    QMap<QString, QLabel*>      m_statusLabels;
    QMap<QString, FieldState>   m_fieldStates;

    // ── Available buys + loaded sale (for Details dialog) ────────────────
    QList<BuyObject>  m_availableBuys;  ///< Käufe mit verbleibendem Volumen (für FIFO-Vorschau)
    QList<BuyObject>  m_allBuys;        ///< Alle Käufe inkl. vollständig verkaufter (für Dok.-Lookup)
    SaleObject        m_loadedSale;   ///< Cached when loadSale() is called; invalid in new-mode.

    // ── Action buttons ────────────────────────────────────────────────────
    QPushButton* m_btnAdd    = nullptr;
    QPushButton* m_btnRemove = nullptr;
    QPushButton* m_btnReset  = nullptr;
    QPushButton* m_btnClose  = nullptr;

    // ── Left panel (for setUiBusy) ────────────────────────────────────────
    QWidget*     m_leftPanel = nullptr;

    // ── Dokumenten-Vorschau (rechtes Panel) ───────────────────────────────
    DocumentPreviewPanel* m_previewPanel = nullptr;

    // ── Verkaufs-Übersicht ────────────────────────────────────────────────
    OverviewTabWidget* m_overviewTabs      = nullptr;
    bool                m_suppressTabSignal = false;

    PresenterSaleEdit* m_presenter = nullptr;
    DocumentsConfig*   m_config    = nullptr;
};

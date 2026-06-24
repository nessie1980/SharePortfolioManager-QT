// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IViewBuyEdit.h"
#include "../../config/DocumentsConfig.h"

#include <QDialog>
#include <QDateEdit>
#include <QTimeEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QGridLayout>
#include <QScrollArea>
#include <QProgressBar>
#include <QMap>

#ifdef SPM_HAVE_QTPDF
#  include <QPdfView>
#  include <QPdfDocument>
#endif

class PresenterBuyEdit;

/**
 * @brief Qt dialog implementing IViewBuyEdit.
 *
 * Layout:
 * ┌── Kaufdaten ──────────────────────┬── Dokumenten-Vorschau ─────────────┐
 * │  (form fields)                    │  QPdfView / pdftoppm               │
 * ├── Dokument ────────────────────── │                                    │
 * │  Dokument: [path]  [...]          │                                    │
 * │  [progress] [icon] [status text]  │                                    │
 * ├───────────────────────────────────┤                                    │
 * │  [Hinzufügen][Entfernen][Reset][Schließen]                             │
 * └───────────────────────────────────┴────────────────────────────────────┘
 * ┌── Kauf-Übersicht ──────────────────────────────────────────────────────┐
 * │  [Tab: Übersicht] [Tab: 2024] ...                                      │
 * └────────────────────────────────────────────────────────────────────────┘
 */
class ViewBuyEdit : public QDialog, public IViewBuyEdit
{
    Q_OBJECT

public:
    enum class FieldState { Untouched, Ok, Error, Info };

    explicit ViewBuyEdit(const QString& shareGuid,
                         DocumentsConfig* config,
                         QWidget* parent = nullptr);
    ~ViewBuyEdit() override = default;

    PresenterBuyEdit* presenter() const { return m_presenter; }

    // ── IViewBuyEdit read ─────────────────────────────────────────────────
    QString dateTime()     const override;
    QString depotNumber()  const override;
    QString orderNumber()  const override;
    double  volume()       const override;
    double  price()        const override;
    QString documentPath() const override;
    double  provision()    const override;
    double  brokerFee()    const override;
    double  traderFee()    const override;
    double  reduction()    const override;

    // ── IViewBuyEdit write ────────────────────────────────────────────────
    void loadBuy(const BuyObject& buy,
                 const BrokerageObject& brokerage) override;
    void clearForm()                              override;

    void setVolumeSold(double value)              override;
    void setKurswert(double value)                override;
    void setGesGebuehren(double value)            override;
    void setEndbetrag(double value)               override;

    // ── Field status (same as ViewShareAdd) ───────────────────────────────
    void setFieldOk(const QString& field, const QString& value) override;
    void setFieldError(const QString& field)                    override;
    void setDocumentPreview(const QString& text)                override;

    // ── Parse status bar (same as ViewShareAdd) ───────────────────────────
    void setParseProgress(int percent, const QString& status)   override;
    void setParseStatusIcon(int iconType)                       override;
    void setUiBusy(bool busy)                                   override;
    void onParseFinished()                                      override;

    // ── Overview / PDF / buttons ──────────────────────────────────────────
    void populateOverview(const QList<BuyObject>&       buys,
                          const QList<BrokerageObject>& brokerages) override;
    void openPdfPreview(const QString& pdfPath)   override;
    void clearPdfPreview()                         override;
    void showOverviewTab()                         override;
    void setButtonStates(bool canRemove, bool isLastBuy, bool isEdit) override;
    void showError(const QString& message)         override;
    void acceptAndClose()                          override;

    // ── Validation ────────────────────────────────────────────────────────
    void markMissingFieldsAsFailed() override;
    bool hasMissingRequiredFields(QStringList& missingFields) const override;

private slots:
    void onBrowseDocument();
    void onOverviewRowActivated(int row, int column);
    void onUebersichtRowActivated(int row, int column);

private:
    void       setupUi();
    QGroupBox* createKaufdatenGroup();
    QGroupBox* createDocumentGroup();    ///< Dokument + Statusbar (like ViewShareAdd)
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
    static double  parseDouble(const QString& text);

    // ── Kaufdaten form ────────────────────────────────────────────────────
    QDateEdit*   m_date          = nullptr;
    QTimeEdit*   m_time          = nullptr;
    QComboBox*   m_depotNumber   = nullptr;
    QLineEdit*   m_orderNumber   = nullptr;
    QLineEdit*   m_volume        = nullptr;
    QLineEdit*   m_volumeSold    = nullptr;   ///< read-only
    QLineEdit*   m_price         = nullptr;
    QLineEdit*   m_kurswert      = nullptr;   ///< read-only
    QLineEdit*   m_provision     = nullptr;
    QLineEdit*   m_brokerFee     = nullptr;
    QLineEdit*   m_traderFee     = nullptr;
    QLineEdit*   m_gesGebuehren  = nullptr;   ///< read-only
    QLineEdit*   m_reduction     = nullptr;
    QLineEdit*   m_endbetrag     = nullptr;   ///< read-only

    // ── Dokument GroupBox (same as ViewShareAdd) ──────────────────────────
    QLineEdit*    m_documentPath    = nullptr;
    QPushButton*  m_btnBrowse       = nullptr;
    QProgressBar* m_parseProgress   = nullptr;
    QLabel*       m_parseStatusIcon = nullptr;
    QLabel*       m_parseStatus     = nullptr;

    // ── inputWidgets / statusLabels (same pattern as ViewShareAdd) ────────
    QMap<QString, QWidget*>     m_inputWidgets;
    QMap<QString, QLabel*>      m_statusLabels;
    QMap<QString, FieldState>   m_fieldStates;

    // ── Action buttons ────────────────────────────────────────────────────
    QPushButton* m_btnAdd    = nullptr;
    QPushButton* m_btnRemove = nullptr;
    QPushButton* m_btnReset  = nullptr;
    QPushButton* m_btnClose  = nullptr;

    // ── Left panel (for setUiBusy) ────────────────────────────────────────
    QWidget*     m_leftPanel = nullptr;

    // ── PDF preview ───────────────────────────────────────────────────────
#ifdef SPM_HAVE_QTPDF
    QPdfDocument* m_pdfDocument   = nullptr;
    QPdfView*     m_pdfView       = nullptr;
    QLabel*       m_zoomLabel     = nullptr;
#else
    QLabel*       m_pdfLabel      = nullptr;
    QScrollArea*  m_pdfScroll     = nullptr;
    QProcess*     m_pdfRenderProc = nullptr;
    QString       m_pdfImagePath;
#endif

    // ── Kauf-Übersicht ────────────────────────────────────────────────────
    QTabWidget* m_overviewTabs    = nullptr;
    bool        m_suppressTabSignal = false;  ///< guard against currentChanged re-entrancy

    PresenterBuyEdit* m_presenter = nullptr;

    /// DocumentsConfig for populating the depot number ComboBox.
    DocumentsConfig*  m_config    = nullptr;
};

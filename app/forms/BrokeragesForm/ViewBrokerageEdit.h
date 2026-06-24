// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IViewBrokerageEdit.h"

#include <QDialog>
#include <QDateEdit>
#include <QTimeEdit>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QGridLayout>
#include <QScrollArea>
#include <QMap>

#ifdef SPM_HAVE_QTPDF
#  include <QPdfDocument>
#  include <QPdfView>
#endif

class PresenterBrokerageEdit;

/**
 * @brief Qt dialog implementing IViewBrokerageEdit.
 *
 * Layout:
 * ┌── Kosten hinzufügen ──────────────────┬── Dokumenten-Vorschau ──────┐
 * │  Datum / Uhrzeit    [date]  [time]    │  QPdfView / pdftoppm        │
 * │  Provision          [edit]  €         │                             │
 * │  Courtage           [edit]  €         │                             │
 * │  Handelsplatzgeb.   [edit]  €         │                             │
 * │  Ges. Gebühren      [readonly] €      │                             │
 * │  Rabatt             [edit]  €         │                             │
 * │  Netto-Kosten       [readonly] €      │                             │
 * ├── Dokument ──────────────────────────  │                             │
 * │  [path]  [📁]                         │                             │
 * ├───────────────────────────────────────│                             │
 * │  [Hinzufügen] [Entfernen] [Reset] [Schließen]                      │
 * └───────────────────────────────────────┴─────────────────────────────┘
 * ┌── Kosten-Übersicht ────────────────────────────────────────────────┐
 * │  [Tab: Übersicht (X.XX €)] [Tab: 2024 (X.XX €)] ...              │
 * └────────────────────────────────────────────────────────────────────┘
 */
class ViewBrokerageEdit : public QDialog, public IViewBrokerageEdit
{
    Q_OBJECT

public:
    enum class FieldState { Untouched, Ok, Error };

    explicit ViewBrokerageEdit(const QString& shareGuid,
                               QWidget* parent = nullptr);
    ~ViewBrokerageEdit() override = default;

    PresenterBrokerageEdit* presenter() const { return m_presenter; }

    // ── IViewBrokerageEdit read ───────────────────────────────────────────
    QString dateTime()     const override;
    double  provision()    const override;
    double  brokerFee()    const override;
    double  traderFee()    const override;
    double  reduction()    const override;
    QString documentPath() const override;

    // ── IViewBrokerageEdit write ──────────────────────────────────────────
    void loadBrokerage(const BrokerageObject& brokerage)        override;
    void clearForm()                                             override;

    void setGesamtGebuehren(double value)    override;
    void setBrokerageReduction(double value) override;

    void setDocumentPreview(const QString& text) override;

    void openPdfPreview(const QString& pdfPath)  override;
    void clearPdfPreview()                        override;

    void populateOverview(const QList<BrokerageObject>& brokerages) override;
    void showOverviewTab()                                           override;

    void setButtonStates(bool canRemove, bool isEdit, bool readOnly) override;
    void showError(const QString& message)                           override;
    void acceptAndClose()                                            override;

    void markMissingFieldsAsFailed()                                    override;
    bool hasMissingRequiredFields(QStringList& missingFields) const     override;

private slots:
    void onBrowseDocument();
    void onOverviewRowActivated(int row, int column);
    void onUebersichtRowActivated(int row, int column);

private:
    void       setupUi();
    QWidget*   createLeftPanel();
    QGroupBox* createKostendatenGroup();
    QGroupBox* createDocumentGroup();
    QWidget*   createButtonBar();
    QGroupBox* createOverviewGroup();
    QWidget*   createPreviewPanel();

    QLabel* addRow(QGridLayout* grid, int& row,
                   const QString& labelText,
                   QWidget* field,
                   const QString& unitText = QString());

    static QString formatMoney(double value);
    static double  parseDouble(const QString& text);

    // Doc icon helper (same logic as BuysForm / SalesForm)
    static QWidget* makeDocIconWidget(const QString& path);

    // ── Kostendaten GroupBox ──────────────────────────────────────────────
    QGroupBox*  m_kostendatenGroup = nullptr; ///< Title changes per edit mode
    QDateEdit*  m_date             = nullptr;
    QTimeEdit*  m_time             = nullptr;
    QLineEdit*  m_provision        = nullptr;
    QLineEdit*  m_brokerFee        = nullptr;
    QLineEdit*  m_traderFee        = nullptr;
    QLineEdit*  m_gesGebuehren     = nullptr; ///< read-only
    QLineEdit*  m_reduction        = nullptr;
    QLineEdit*  m_nettoKosten      = nullptr; ///< read-only, coloured

    // ── Document ─────────────────────────────────────────────────────────
    QLineEdit*   m_documentPath = nullptr;
    QPushButton* m_btnBrowse    = nullptr;

    // ── Buttons ───────────────────────────────────────────────────────────
    QPushButton* m_btnAdd    = nullptr;
    QPushButton* m_btnRemove = nullptr;
    QPushButton* m_btnReset  = nullptr;
    QPushButton* m_btnClose  = nullptr;

    // ── Overview ──────────────────────────────────────────────────────────
    QTabWidget* m_tabs = nullptr;

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

    // ── Field validity tracking ───────────────────────────────────────────
    QMap<QString, FieldState> m_fieldStates;

    // ── Presenter ─────────────────────────────────────────────────────────
    PresenterBrokerageEdit* m_presenter = nullptr;

    // ── Tab navigation guard ──────────────────────────────────────────────
    bool m_suppressTabSignal = false;
};

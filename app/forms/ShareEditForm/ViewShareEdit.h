// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IViewShareEdit.h"
#include "../../config/DocumentsConfig.h"
#include "../../models/ShareSplitObject.h"
#include <QDialog>
#include <QLineEdit>
#include <QDateEdit>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QGridLayout>
#include <QButtonGroup>

class PresenterShareEdit;

/**
 * @brief Qt dialog implementing IViewShareEdit — "Aktie editieren".
 *
 * Layout (matches the C# predecessor screenshot):
 *
 * ┌── Allgemein ──────────────────────────────────────────────────────┐
 * │  WKN (ISIN):        [edit]                                        │
 * │  Datum:             [date]                                        │
 * │  Name:              [edit]                                        │
 * │  Börsennotierung:   [date]                                        │
 * │  Einzahlung:        [read-only]  €                                │
 * │  Anteile:           [read-only]  Stk.                             │
 * │  Splits:            [read-only]  [✏]                              │
 * │  Update via Internet: ○Beide  ○Markt-Preis  ○Tages-Werte  ○Keine │
 * │  Details-Webseite:  [edit]                                        │
 * │  Markt-Wert-Webseite: [edit]  [combo ApiYahoo/OnVista/Regex]      │
 * │  API Schlüssel:     [edit]                                        │
 * │  Tages-Werte-Webseite: [edit]  [combo]                            │
 * │  API Schlüssel:     [edit]                                        │
 * │  Länder-Info:       [combo]                                       │
 * │  Dividendeninterval:[combo]                                       │
 * │  Typ:               [combo]                                       │
 * │                            [Speichern]  [Schließen]               │
 * └───────────────────────────────────────────────────────────────────┘
 * ┌── Einnahmen / Ausgabe ─────────────────────────────────────────────┐
 * │  Käufe:         [✏]  [value read-only]   €                        │
 * │  Verkäufe:      [✏]  [value read-only]   €                        │
 * │  Verlust aus V.:[✏]  [value read-only]   €                        │
 * │  Dividenden:    [✏]  [value read-only]   €                        │
 * │  Kosten:        [✏]  [value read-only]   €                        │
 * └───────────────────────────────────────────────────────────────────┘
 */
class ViewShareEdit : public QDialog, public IViewShareEdit
{
    Q_OBJECT

public:
    /**
     * @brief Construct the edit dialog for a specific share.
     * @param shareGuid  GUID of the share to edit.
     * @param config     Documents configuration (passed through to ViewBuyEdit).
     * @param parent     Parent widget.
     */
    explicit ViewShareEdit(const QString& shareGuid,
                           DocumentsConfig* config,
                           QWidget* parent = nullptr);
    ~ViewShareEdit() override = default;

    // ── IViewShareEdit read accessors ─────────────────────────────────────
    QString   wkn()              const override;
    QString   isin()             const override;
    QString   name()             const override;
    QDate     listingDate()      const override;
    ShareType shareType()        const override;
    QString   dividendInterval() const override;
    QString   countryInfo()      const override;
    QString   detailsWebsite()   const override;

    QString          marketPriceUrl()         const override;
    ShareParsingType marketPriceParsingType() const override;
    QString          marketPriceApiKey()      const override;
    QString          dailyValuesUrl()          const override;
    ShareParsingType dailyValuesParsingType() const override;
    QString          dailyValuesApiKey()      const override;

    ShareUpdateType  updateType()             const override;

    // ── IViewShareEdit write methods ──────────────────────────────────────
    void loadShare(const ShareObject& share)              override;
    void setFirstBuyDate(const QString& dateStr)          override;
    void setCurrentVolume(double volume)                  override;
    void setSplitInfo(const QList<ShareSplitObject>& splits) override;
    void setDailyValuesRequired(bool required)            override;
    void setTotalBuys(double value, int count)            override;
    void setTotalSales(double value, int count)           override;
    void setTotalProfitLoss(double value, int count)      override;
    void setTotalDividends(double value, int count)       override;
    void setTotalBrokerages(double value, int count)      override;
    void showError(const QString& message)                override;
    void acceptAndClose()                                 override;

public slots:
    /**
     * @brief Refresh all aggregate values in the "Einnahmen / Ausgabe" group.
     *
     * Called after ViewBuyEdit (or any other sub-dialog) emits dataChanged()
     * so the summary stays up to date without reopening this dialog.
     */
    void refreshSummary();

private slots:
    void onMarketParsingTypeChanged(int index);
    void onDailyParsingTypeChanged(int index);
    void onEditBuys();
    void onEditSales();
    void onEditDividends();
    void onEditBrokerages();
    void onEditSplits();

private:
    void       setupUi();
    QGroupBox* createGeneralGroup();
    QGroupBox* createSummaryGroup();

    /** Helper: add a label/field row to a QGridLayout, returns the label. */
    static QLabel* addRow(QGridLayout* grid, int& row,
                          const QString& labelText,
                          QWidget* field,
                          const QString& unitText = QString());

    /** Helper: format a monetary value for display in a read-only field. */
    static QString formatMoney(double value);

    /**
     * @brief Helper: Kurzform eines Splits, z. B. "20:1 am 18.07.2022".
     *
     * Wird sowohl für den Feldtext als auch für den Tooltip verwendet, damit
     * beide garantiert dieselbe Schreibweise haben.
     */
    static QString formatSplit(const ShareSplitObject& split);

    /** Helper: Verhältnis-Seite ohne unnötige Nachkommastellen ("20" statt "20,00"). */
    static QString formatRatioPart(double value);

    // ── Allgemein ─────────────────────────────────────────────────────────
    QLineEdit*    m_wkn             = nullptr;
    QLineEdit*    m_isin            = nullptr;
    QLineEdit*    m_name            = nullptr;
    QLineEdit*    m_datumField      = nullptr; ///< read-only, date of first buy
    QDateEdit*    m_listingDate     = nullptr; ///< editable Börsennotierung
    QLineEdit*    m_einzahlung      = nullptr; ///< read-only, filled from totalBuyValue
    QLineEdit*    m_anteile         = nullptr; ///< read-only, filled from totalVolume
    QLineEdit*    m_splitsField     = nullptr; ///< read-only, siehe setSplitInfo() (08.08.2026)
    QPushButton*  m_btnEditSplits   = nullptr; ///< fünfter Stift-Button, öffnet ViewShareSplitEdit
    QButtonGroup* m_updateGroup     = nullptr; ///< Beide / Markt-Preis / Tages-Werte / Keine
    QLabel*       m_updateHint      = nullptr; ///< Hinweis unter den Radios, siehe setDailyValuesRequired()
    QLineEdit*    m_detailsWebsite  = nullptr;
    QLineEdit*    m_marketUrl       = nullptr;
    QComboBox*    m_marketParsing   = nullptr;
    QLineEdit*    m_marketApiKey    = nullptr;
    QLineEdit*    m_dailyUrl        = nullptr;
    QComboBox*    m_dailyParsing    = nullptr;
    QLineEdit*    m_dailyApiKey     = nullptr;
    QComboBox*    m_countryInfo     = nullptr;
    QComboBox*    m_divInterval     = nullptr;
    QComboBox*    m_shareType       = nullptr;

    QPushButton*  m_btnSave         = nullptr;
    QPushButton*  m_btnClose        = nullptr;

    // ── Einnahmen / Ausgabe ───────────────────────────────────────────────
    QLineEdit*    m_totalBuys       = nullptr;
    QLineEdit*    m_totalSales      = nullptr;
    QLineEdit*    m_totalProfitLoss = nullptr;
    QLineEdit*    m_totalDividends  = nullptr;
    QLineEdit*    m_totalBrokerages = nullptr;

    QPushButton*  m_btnEditBuys       = nullptr;
    QPushButton*  m_btnEditSales      = nullptr;
    QPushButton*  m_btnEditDividends  = nullptr;
    QPushButton*  m_btnEditBrokerages = nullptr;

    PresenterShareEdit* m_presenter = nullptr;

    /// Stored share GUID so refreshSummary() can reload aggregates via the presenter.
    QString m_shareGuid;

    /// DocumentsConfig passed through to ViewBuyEdit for PDF parsing.
    DocumentsConfig* m_config = nullptr;
};

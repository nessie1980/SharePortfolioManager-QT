// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QDialog>
#include <QTabWidget>
#include <QTableWidget>
#include <QLabel>
#include <QGroupBox>
#include <QPushButton>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include "../../models/ShareObject.h"
#include "../../models/BuyObject.h"
#include "../../models/SaleObject.h"
#include "../../models/DividendObject.h"
#include "../../models/BrokerageObject.h"

/**
 * @brief Dialog showing all details of a single share.
 *
 * Opened by double-clicking a row in the main portfolio tables.
 *
 * Layout:
 * - Header row: share logo placeholder | name + WKN/ISIN | current price + prev-day performance
 * - QTabWidget:
 *     - "Stammdaten"   — master data fields (read-only grid)
 *     - "Käufe"        — buy transactions table
 *     - "Verkäufe"     — sale transactions table
 *     - "Dividenden"   — dividend payments table
 *     - "Brokerages"   — brokerage records table
 * - Close button
 */
class ShareDetailsForm : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Construct and populate the dialog for the given share GUID.
     * @param shareGuid  GUID of the share to display.
     * @param parent     Parent widget.
     */
    explicit ShareDetailsForm(const QString& shareGuid, QWidget* parent = nullptr);
    ~ShareDetailsForm() override = default;

private:
    // ── Setup ──────────────────────────────────────────────────────────────
    void setupUi();
    void setupMasterDataTab();
    void setupBuysTab();
    void setupSalesTab();
    void setupDividendsTab();
    void setupBrokeragesTab();

    // ── Population ─────────────────────────────────────────────────────────
    void populate();
    void populateMasterData();
    void populateBuys();
    void populateSales();
    void populateDividends();
    void populateBrokerages();

    // ── Helpers ────────────────────────────────────────────────────────────
    /** Add a read-only label/value row to a QGridLayout. */
    static void addFieldRow(QGridLayout* grid, int row,
                            const QString& label, const QString& value);

    /** Update a tab label to include the record count, e.g. "Käufe (3)". */
    void updateTabLabel(int tabIndex, const QString& baseLabel, int count);

    // ── Data ───────────────────────────────────────────────────────────────
    QString          m_shareGuid;
    ShareObject      m_share;
    QList<BuyObject>        m_buys;
    QList<SaleObject>       m_sales;
    QList<DividendObject>   m_dividends;
    QList<BrokerageObject>  m_brokerages;

    // ── Widgets ────────────────────────────────────────────────────────────
    QLabel*      m_headerName       = nullptr;
    QLabel*      m_headerIds        = nullptr;
    QLabel*      m_headerPrice      = nullptr;
    QLabel*      m_headerPerf       = nullptr;

    QTabWidget*  m_tabs             = nullptr;

    // Master data tab
    QWidget*     m_masterTab        = nullptr;
    QGridLayout* m_masterGrid       = nullptr;

    // Transactions tabs
    QTableWidget* m_buysTable       = nullptr;
    QTableWidget* m_salesTable      = nullptr;
    QTableWidget* m_dividendsTable  = nullptr;
    QTableWidget* m_brokeragesTable = nullptr;

    // Tab indices (used for updateTabLabel)
    int m_tabMaster     = 0;
    int m_tabBuys       = 1;
    int m_tabSales      = 2;
    int m_tabDividends  = 3;
    int m_tabBrokerages = 4;
};

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ShareDetailsForm.h"

#include "../../repositories/ShareRepository.h"
#include "../../repositories/BuyRepository.h"
#include "../../repositories/SaleRepository.h"
#include "../../repositories/DividendRepository.h"
#include "../../repositories/BrokerageRepository.h"

#include <QHeaderView>
#include <QLocale>
#include <QFont>
#include <QFrame>
#include <QScrollArea>
#include <QSizePolicy>
#include <QDialogButtonBox>

// ── Constructor ───────────────────────────────────────────────────────────────

ShareDetailsForm::ShareDetailsForm(const QString& shareGuid, QWidget* parent)
    : QDialog(parent)
    , m_shareGuid(shareGuid)
{
    setWindowTitle(tr("Aktiendetails"));
    setMinimumSize(900, 600);
    resize(1100, 700);

    // Load all data first
    ShareRepository shareRepo;
    m_share = shareRepo.findByGuid(shareGuid);

    BuyRepository buyRepo;
    m_buys = buyRepo.findByShare(shareGuid);

    SaleRepository saleRepo;
    m_sales = saleRepo.findByShare(shareGuid);

    DividendRepository divRepo;
    m_dividends = divRepo.findByShare(shareGuid);

    BrokerageRepository brokerageRepo;
    m_brokerages = brokerageRepo.findByShare(shareGuid);

    setupUi();
    populate();
}

// ── setupUi ───────────────────────────────────────────────────────────────────

void ShareDetailsForm::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // ── Header strip ──────────────────────────────────────────────────────
    auto* headerFrame = new QFrame();
    headerFrame->setFrameShape(QFrame::StyledPanel);
    headerFrame->setFrameShadow(QFrame::Sunken);
    headerFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* headerLayout = new QHBoxLayout(headerFrame);
    headerLayout->setContentsMargins(12, 8, 12, 8);
    headerLayout->setSpacing(16);

    // Name + type (large)
    m_headerName = new QLabel();
    QFont nameFont = m_headerName->font();
    nameFont.setPointSize(nameFont.pointSize() + 4);
    nameFont.setBold(true);
    m_headerName->setFont(nameFont);

    // WKN / ISIN / type row
    m_headerIds = new QLabel();
    m_headerIds->setForegroundRole(QPalette::Mid);

    auto* nameBlock = new QVBoxLayout();
    nameBlock->setSpacing(2);
    nameBlock->addWidget(m_headerName);
    nameBlock->addWidget(m_headerIds);

    // Current price (right-aligned)
    m_headerPrice = new QLabel();
    QFont priceFont = m_headerPrice->font();
    priceFont.setPointSize(priceFont.pointSize() + 3);
    priceFont.setBold(true);
    m_headerPrice->setFont(priceFont);
    m_headerPrice->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // Prev-day performance (right-aligned, colored later)
    m_headerPerf = new QLabel();
    QFont perfFont = m_headerPerf->font();
    perfFont.setPointSize(perfFont.pointSize() + 1);
    m_headerPerf->setFont(perfFont);
    m_headerPerf->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto* priceBlock = new QVBoxLayout();
    priceBlock->setSpacing(2);
    priceBlock->addWidget(m_headerPrice);
    priceBlock->addWidget(m_headerPerf);

    headerLayout->addLayout(nameBlock, 1);
    headerLayout->addLayout(priceBlock, 0);

    mainLayout->addWidget(headerFrame);

    // ── Tab widget ────────────────────────────────────────────────────────
    m_tabs = new QTabWidget();
    mainLayout->addWidget(m_tabs, 1);

    setupMasterDataTab();
    setupBuysTab();
    setupSalesTab();
    setupDividendsTab();
    setupBrokeragesTab();

    // ── Close button ──────────────────────────────────────────────────────
    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

// ── setupMasterDataTab ────────────────────────────────────────────────────────

void ShareDetailsForm::setupMasterDataTab()
{
    m_masterTab = new QWidget();
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);

    auto* container = new QWidget();
    m_masterGrid = new QGridLayout(container);
    m_masterGrid->setColumnStretch(1, 1);
    m_masterGrid->setColumnStretch(3, 1);
    m_masterGrid->setHorizontalSpacing(16);
    m_masterGrid->setVerticalSpacing(6);
    m_masterGrid->setContentsMargins(12, 12, 12, 12);

    scroll->setWidget(container);

    auto* tabLayout = new QVBoxLayout(m_masterTab);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->addWidget(scroll);

    m_tabMaster = m_tabs->addTab(m_masterTab, tr("Stammdaten"));
}

// ── setupBuysTab ──────────────────────────────────────────────────────────────

void ShareDetailsForm::setupBuysTab()
{
    m_buysTable = new QTableWidget(0, 10);
    m_buysTable->setHorizontalHeaderLabels({
        tr("Datum"), tr("Depot"), tr("Order-Nr."),
        tr("Stück"), tr("Kurs"), tr("Provision"),
        tr("Broker-G."), tr("Händler-G."), tr("Rabatt"),
        tr("Kaufwert (Netto)")
    });
    m_buysTable->horizontalHeader()->setStretchLastSection(true);
    m_buysTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_buysTable->verticalHeader()->setVisible(false);
    m_buysTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_buysTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_buysTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_buysTable->setAlternatingRowColors(true);
    m_buysTable->setSortingEnabled(true);

    m_tabBuys = m_tabs->addTab(m_buysTable, tr("Käufe"));
}

// ── setupSalesTab ─────────────────────────────────────────────────────────────

void ShareDetailsForm::setupSalesTab()
{
    m_salesTable = new QTableWidget(0, 9);
    m_salesTable->setHorizontalHeaderLabels({
        tr("Datum"), tr("Depot"), tr("Order-Nr."),
        tr("Stück"), tr("Kurs"), tr("Steuern"),
        tr("Brokerage"), tr("Verkaufswert"), tr("Gewinn / Verlust")
    });
    m_salesTable->horizontalHeader()->setStretchLastSection(true);
    m_salesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_salesTable->verticalHeader()->setVisible(false);
    m_salesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_salesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_salesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_salesTable->setAlternatingRowColors(true);
    m_salesTable->setSortingEnabled(true);

    m_tabSales = m_tabs->addTab(m_salesTable, tr("Verkäufe"));
}

// ── setupDividendsTab ─────────────────────────────────────────────────────────

void ShareDetailsForm::setupDividendsTab()
{
    m_dividendsTable = new QTableWidget(0, 8);
    m_dividendsTable->setHorizontalHeaderLabels({
        tr("Datum"), tr("Stück"), tr("Rate"),
        tr("Brutto"), tr("Steuern"), tr("Netto"),
        tr("Rendite %"), tr("Kurs am Zahltag")
    });
    m_dividendsTable->horizontalHeader()->setStretchLastSection(true);
    m_dividendsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_dividendsTable->verticalHeader()->setVisible(false);
    m_dividendsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_dividendsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_dividendsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_dividendsTable->setAlternatingRowColors(true);
    m_dividendsTable->setSortingEnabled(true);

    m_tabDividends = m_tabs->addTab(m_dividendsTable, tr("Dividenden"));
}

// ── setupBrokeragesTab ────────────────────────────────────────────────────────

void ShareDetailsForm::setupBrokeragesTab()
{
    m_brokeragesTable = new QTableWidget(0, 6);
    m_brokeragesTable->setHorizontalHeaderLabels({
        tr("Datum"), tr("Provision"), tr("Broker-G."),
        tr("Händler-G."), tr("Rabatt"), tr("Gesamt")
    });
    m_brokeragesTable->horizontalHeader()->setStretchLastSection(true);
    m_brokeragesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_brokeragesTable->verticalHeader()->setVisible(false);
    m_brokeragesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_brokeragesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_brokeragesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_brokeragesTable->setAlternatingRowColors(true);
    m_brokeragesTable->setSortingEnabled(true);

    m_tabBrokerages = m_tabs->addTab(m_brokeragesTable, tr("Brokerages"));
}

// ── populate ─────────────────────────────────────────────────────────────────

void ShareDetailsForm::populate()
{
    if (!m_share.isValid())
        return;

    populateMasterData();
    populateBuys();
    populateSales();
    populateDividends();
    populateBrokerages();

    // Header strip
    m_headerName->setText(m_share.name());

    const QString shareTypeStr = [this]() -> QString {
        switch (m_share.shareType()) {
        case ShareType::Share: return tr("Aktie");
        case ShareType::Fond:  return tr("Fonds");
        case ShareType::Etf:   return tr("ETF");
        default:               return tr("Unbekannt");
        }
    }();

    m_headerIds->setText(
        tr("WKN: %1  |  ISIN: %2  |  %3  |  %4")
            .arg(m_share.wkn())
            .arg(m_share.isin().isEmpty() ? tr("-") : m_share.isin())
            .arg(shareTypeStr)
            .arg(m_share.currency()));

    const QLocale locale;
    m_headerPrice->setText(
        locale.toString(m_share.curPrice(), 'f', 2)
        + QStringLiteral(" ") + m_share.currency());

    const double perf = m_share.pricePerformance();
    const double diff = m_share.priceDifference();
    const QString sign = (diff >= 0) ? QStringLiteral("+") : QString();
    const QString perfText =
        sign + locale.toString(diff, 'f', 2)
        + QStringLiteral(" (")
        + sign + locale.toString(perf, 'f', 2)
        + QStringLiteral(" %)");

    m_headerPerf->setText(perfText);
    if (diff > 0)
        m_headerPerf->setStyleSheet(QStringLiteral("color: green;"));
    else if (diff < 0)
        m_headerPerf->setStyleSheet(QStringLiteral("color: red;"));
    else
        m_headerPerf->setStyleSheet(QString());
}

// ── populateMasterData ────────────────────────────────────────────────────────

void ShareDetailsForm::populateMasterData()
{
    const QLocale locale;
    int row = 0;

    // ── Identifikation ────────────────────────────────────────────────────
    auto* idLabel = new QLabel(tr("<b>Identifikation</b>"));
    m_masterGrid->addWidget(idLabel, row++, 0, 1, 4);

    addFieldRow(m_masterGrid, row++, tr("Name:"),            m_share.name());
    addFieldRow(m_masterGrid, row++, tr("WKN:"),             m_share.wkn());
    addFieldRow(m_masterGrid, row++, tr("ISIN:"),            m_share.isin().isEmpty() ? tr("-") : m_share.isin());
    addFieldRow(m_masterGrid, row++, tr("GUID:"),            m_share.guid());

    const QString shareTypeStr = [this]() -> QString {
        switch (m_share.shareType()) {
        case ShareType::Share: return tr("Aktie");
        case ShareType::Fond:  return tr("Fonds");
        case ShareType::Etf:   return tr("ETF");
        default:               return tr("Unbekannt");
        }
    }();
    addFieldRow(m_masterGrid, row++, tr("Typ:"),             shareTypeStr);

    // Spacer
    m_masterGrid->setRowMinimumHeight(row++, 8);

    // ── Kursdaten ─────────────────────────────────────────────────────────
    auto* priceLabel = new QLabel(tr("<b>Kursdaten</b>"));
    m_masterGrid->addWidget(priceLabel, row++, 0, 1, 4);

    addFieldRow(m_masterGrid, row++, tr("Aktueller Kurs:"),
                locale.toString(m_share.curPrice(), 'f', 4) + QStringLiteral(" ") + m_share.currency());
    addFieldRow(m_masterGrid, row++, tr("Vortag:"),
                locale.toString(m_share.prevDayPrice(), 'f', 4) + QStringLiteral(" ") + m_share.currency());
    addFieldRow(m_masterGrid, row++, tr("Änderung:"),
                locale.toString(m_share.priceDifference(), 'f', 4)
                + QStringLiteral(" / ")
                + locale.toString(m_share.pricePerformance(), 'f', 2)
                + QStringLiteral(" %"));
    addFieldRow(m_masterGrid, row++, tr("Letztes Kurs-Update:"),
                m_share.lastPriceUpdate().isEmpty() ? tr("-") : m_share.lastPriceUpdate());
    addFieldRow(m_masterGrid, row++, tr("Letztes Internet-Update:"),
                m_share.lastInternetUpdate().isEmpty() ? tr("-") : m_share.lastInternetUpdate());

    m_masterGrid->setRowMinimumHeight(row++, 8);

    // ── Internet-Aktualisierung ───────────────────────────────────────────
    auto* updateLabel = new QLabel(tr("<b>Internet-Aktualisierung</b>"));
    m_masterGrid->addWidget(updateLabel, row++, 0, 1, 4);

    const QString updateTypeStr = [this]() -> QString {
        switch (m_share.updateType()) {
        case ShareUpdateType::None:        return tr("Keine");
        case ShareUpdateType::MarketPrice: return tr("Marktwert");
        case ShareUpdateType::DailyValues: return tr("Tageswerte");
        case ShareUpdateType::Both:        return tr("Marktwert + Tageswerte");
        default:                           return tr("Unbekannt");
        }
    }();
    addFieldRow(m_masterGrid, row++, tr("Update-Typ:"), updateTypeStr);

    auto parsingTypeStr = [](ShareParsingType t) -> QString {
        switch (t) {
        case ShareParsingType::Regex:      return QStringLiteral("Regex");
        case ShareParsingType::ApiOnVista: return QStringLiteral("OnVista API");
        case ShareParsingType::ApiYahoo:   return QStringLiteral("Yahoo API");
        default:                           return QStringLiteral("Unbekannt");
        }
    };

    addFieldRow(m_masterGrid, row++, tr("Marktpreis-Parsing:"),   parsingTypeStr(m_share.marketPriceParsingType()));
    addFieldRow(m_masterGrid, row++, tr("Marktpreis-URL:"),
                m_share.marketPriceUrl().isEmpty() ? tr("-") : m_share.marketPriceUrl());
    addFieldRow(m_masterGrid, row++, tr("Marktpreis-Encoding:"),  m_share.marketPriceEncoding());
    addFieldRow(m_masterGrid, row++, tr("Tageswerte-Parsing:"),   parsingTypeStr(m_share.dailyValuesParsingType()));
    addFieldRow(m_masterGrid, row++, tr("Tageswerte-URL:"),
                m_share.dailyValuesUrl().isEmpty() ? tr("-") : m_share.dailyValuesUrl());
    addFieldRow(m_masterGrid, row++, tr("Tageswerte-Encoding:"),  m_share.dailyValuesEncoding());

    m_masterGrid->setRowMinimumHeight(row++, 8);

    // ── Anzeige ───────────────────────────────────────────────────────────
    auto* displayLabel = new QLabel(tr("<b>Anzeige</b>"));
    m_masterGrid->addWidget(displayLabel, row++, 0, 1, 4);

    addFieldRow(m_masterGrid, row++, tr("Details-Website:"),
                m_share.detailsWebSiteUrl().isEmpty() ? tr("-") : m_share.detailsWebSiteUrl());
    addFieldRow(m_masterGrid, row++, tr("Logo-Pfad:"),
                m_share.imagePath().isEmpty() ? tr("-") : m_share.imagePath());
    addFieldRow(m_masterGrid, row++, tr("Hinzugefügt am:"),
                m_share.addDateTime().isEmpty() ? tr("-") : m_share.addDateTime());

    // Push everything to the top
    m_masterGrid->setRowStretch(row, 1);
}

// ── populateBuys ──────────────────────────────────────────────────────────────

void ShareDetailsForm::populateBuys()
{
    const QLocale locale;
    m_buysTable->setSortingEnabled(false);

    for (const BuyObject& buy : m_buys) {
        const int r = m_buysTable->rowCount();
        m_buysTable->insertRow(r);

        auto makeItem = [](const QString& text) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            return item;
        };
        auto makeTextItem = [](const QString& text) {
            return new QTableWidgetItem(text);
        };

        m_buysTable->setItem(r, 0, makeTextItem(buy.dateAsStr()));
        m_buysTable->setItem(r, 1, makeTextItem(buy.depotNumber()));
        m_buysTable->setItem(r, 2, makeTextItem(buy.orderNumber()));
        m_buysTable->setItem(r, 3, makeItem(locale.toString(buy.volume(), 'f', 4)));
        m_buysTable->setItem(r, 4, makeItem(locale.toString(buy.price(), 'f', 4)));

        // Brokerage values are stored in the linked BrokerageObject
        BrokerageRepository brokerageRepo;
        const BrokerageObject brokerage = brokerageRepo.findByBuyGuid(buy.guid());
        const double provision  = brokerage.isValid() ? brokerage.provision()  : 0.0;
        const double brokerFee  = brokerage.isValid() ? brokerage.brokerFee()  : 0.0;
        const double traderFee  = brokerage.isValid() ? brokerage.traderFee()  : 0.0;
        const double reduction  = brokerage.isValid() ? brokerage.reduction()  : 0.0;
        const double buyValueBrokerageReduction =
            buy.buyValue() + provision + brokerFee + traderFee - reduction;

        m_buysTable->setItem(r, 5, makeItem(locale.toString(provision,  'f', 2)));
        m_buysTable->setItem(r, 6, makeItem(locale.toString(brokerFee,  'f', 2)));
        m_buysTable->setItem(r, 7, makeItem(locale.toString(traderFee,  'f', 2)));
        m_buysTable->setItem(r, 8, makeItem(locale.toString(reduction,  'f', 2)));
        m_buysTable->setItem(r, 9, makeItem(locale.toString(buyValueBrokerageReduction, 'f', 2)));
    }

    m_buysTable->setSortingEnabled(true);
    m_buysTable->resizeColumnsToContents();
    updateTabLabel(m_tabBuys, tr("Käufe"), m_buys.size());
}

// ── populateSales ─────────────────────────────────────────────────────────────

void ShareDetailsForm::populateSales()
{
    const QLocale locale;
    m_salesTable->setSortingEnabled(false);

    for (const SaleObject& sale : m_sales) {
        const int r = m_salesTable->rowCount();
        m_salesTable->insertRow(r);

        auto makeItem = [](const QString& text) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            return item;
        };
        auto makeTextItem = [](const QString& text) {
            return new QTableWidgetItem(text);
        };

        const double plBR = sale.profitLossBrokerageReduction();
        auto* plItem = makeItem(locale.toString(plBR, 'f', 2));
        if (plBR > 0)
            plItem->setForeground(QColor(Qt::darkGreen));
        else if (plBR < 0)
            plItem->setForeground(QColor(Qt::red));

        m_salesTable->setItem(r, 0, makeTextItem(sale.dateAsStr()));
        m_salesTable->setItem(r, 1, makeTextItem(sale.depotNumber()));
        m_salesTable->setItem(r, 2, makeTextItem(sale.orderNumber()));
        m_salesTable->setItem(r, 3, makeItem(locale.toString(sale.volume(), 'f', 4)));
        m_salesTable->setItem(r, 4, makeItem(locale.toString(sale.salePrice(), 'f', 4)));
        m_salesTable->setItem(r, 5, makeItem(locale.toString(sale.taxSum(), 'f', 2)));
        m_salesTable->setItem(r, 6, makeItem(locale.toString(sale.brokerage(), 'f', 2)));
        m_salesTable->setItem(r, 7, makeItem(locale.toString(sale.payoutBrokerageReduction(), 'f', 2)));
        m_salesTable->setItem(r, 8, plItem);
    }

    m_salesTable->setSortingEnabled(true);
    m_salesTable->resizeColumnsToContents();
    updateTabLabel(m_tabSales, tr("Verkäufe"), m_sales.size());
}

// ── populateDividends ─────────────────────────────────────────────────────────

void ShareDetailsForm::populateDividends()
{
    const QLocale locale;
    m_dividendsTable->setSortingEnabled(false);

    for (const DividendObject& div : m_dividends) {
        const int r = m_dividendsTable->rowCount();
        m_dividendsTable->insertRow(r);

        auto makeItem = [](const QString& text) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            return item;
        };
        auto makeTextItem = [](const QString& text) {
            return new QTableWidgetItem(text);
        };

        m_dividendsTable->setItem(r, 0, makeTextItem(div.dateAsStr()));
        m_dividendsTable->setItem(r, 1, makeItem(locale.toString(div.volume(), 'f', 4)));
        m_dividendsTable->setItem(r, 2, makeItem(locale.toString(div.rate(), 'f', 4)));
        m_dividendsTable->setItem(r, 3, makeItem(locale.toString(div.dividendPayout(), 'f', 2)));
        m_dividendsTable->setItem(r, 4, makeItem(locale.toString(div.taxSum(), 'f', 2)));
        m_dividendsTable->setItem(r, 5, makeItem(locale.toString(div.dividendPayoutWithTaxes(), 'f', 2)));
        m_dividendsTable->setItem(r, 6, makeItem(locale.toString(div.yield(), 'f', 2) + QStringLiteral(" %")));
        m_dividendsTable->setItem(r, 7, makeItem(locale.toString(div.priceAtPayday(), 'f', 4)));
    }

    m_dividendsTable->setSortingEnabled(true);
    m_dividendsTable->resizeColumnsToContents();
    updateTabLabel(m_tabDividends, tr("Dividenden"), m_dividends.size());
}

// ── populateBrokerages ────────────────────────────────────────────────────────

void ShareDetailsForm::populateBrokerages()
{
    const QLocale locale;
    m_brokeragesTable->setSortingEnabled(false);

    for (const BrokerageObject& b : m_brokerages) {
        const int r = m_brokeragesTable->rowCount();
        m_brokeragesTable->insertRow(r);

        auto makeItem = [](const QString& text) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            return item;
        };

        m_brokeragesTable->setItem(r, 0, new QTableWidgetItem(b.dateAsStr()));
        m_brokeragesTable->setItem(r, 1, makeItem(locale.toString(b.provision(), 'f', 2)));
        m_brokeragesTable->setItem(r, 2, makeItem(locale.toString(b.brokerFee(), 'f', 2)));
        m_brokeragesTable->setItem(r, 3, makeItem(locale.toString(b.traderFee(), 'f', 2)));
        m_brokeragesTable->setItem(r, 4, makeItem(locale.toString(b.reduction(), 'f', 2)));
        m_brokeragesTable->setItem(r, 5, makeItem(locale.toString(b.brokerageReduction(), 'f', 2)));
    }

    m_brokeragesTable->setSortingEnabled(true);
    m_brokeragesTable->resizeColumnsToContents();
    updateTabLabel(m_tabBrokerages, tr("Brokerages"), m_brokerages.size());
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void ShareDetailsForm::addFieldRow(QGridLayout* grid, int row,
                                   const QString& label, const QString& value)
{
    auto* lbl = new QLabel(label);
    lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QFont f = lbl->font();
    f.setBold(true);
    lbl->setFont(f);

    auto* val = new QLabel(value);
    val->setTextInteractionFlags(Qt::TextSelectableByMouse);
    val->setWordWrap(true);

    // Two columns per row: label (col 0) | value (col 1)
    // (extra columns reserved for a potential future second column pair)
    grid->addWidget(lbl, row, 0);
    grid->addWidget(val, row, 1, 1, 3);
}

void ShareDetailsForm::updateTabLabel(int tabIndex,
                                      const QString& baseLabel,
                                      int count)
{
    m_tabs->setTabText(tabIndex,
        count > 0
            ? QStringLiteral("%1 (%2)").arg(baseLabel).arg(count)
            : baseLabel);
}

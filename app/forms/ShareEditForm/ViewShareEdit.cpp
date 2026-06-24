// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ViewShareEdit.h"
#include "PresenterShareEdit.h"
#include "ModelShareEdit.h"
#include "../../IconProvider.h"
#include "../../config/AppSettings.h"
#include "../UiConstants.h"

// ── BuysForm ──────────────────────────────────────────────────────────────────
#include "../BuysForm/ViewBuyEdit.h"
#include "../BuysForm/PresenterBuyEdit.h"

// ── SalesForm ─────────────────────────────────────────────────────────────────
#include "../SalesForm/ViewSaleEdit.h"
#include "../SalesForm/PresenterSaleEdit.h"

// ── DividendForm ──────────────────────────────────────────────────────────────
#include "../DividendForm/ViewDividendEdit.h"
#include "../DividendForm/PresenterDividendEdit.h"

// ── BrokeragesForm ────────────────────────────────────────────────────────────
#include "../BrokeragesForm/ViewBrokerageEdit.h"
#include "../BrokeragesForm/PresenterBrokerageEdit.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRadioButton>
#include "../OwnMessageBoxForm/OwnMessageBox.h"
#include <QLocale>
#include <QSizePolicy>

// ── Constructor ───────────────────────────────────────────────────────────────

ViewShareEdit::ViewShareEdit(const QString& shareGuid,
                             DocumentsConfig* config,
                             QWidget* parent)
    : QDialog(parent)
    , m_shareGuid(shareGuid)
    , m_config(config)
{
    setWindowTitle(tr("Aktie editieren"));
    setMinimumWidth(640);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);

    // Build all widgets first — presenter calls loadAndPopulate() in its
    // constructor and writes into the widgets via IViewShareEdit, so they
    // must already exist before the presenter is created.
    setupUi();

    auto* model = new ModelShareEdit();
    m_presenter = new PresenterShareEdit(this, model, shareGuid, this);

    connect(m_btnSave,  &QPushButton::clicked, m_presenter, &PresenterShareEdit::onSave);
    connect(m_btnClose, &QPushButton::clicked, m_presenter, &PresenterShareEdit::onCancel);
    connect(m_btnClose, &QPushButton::clicked, this,         &QDialog::reject);

    // The pencil buttons for Sales, Dividends and Brokerages still route
    // through the presenter (which will emit the corresponding signals once
    // those sub-dialogs are implemented).
    connect(m_btnEditSales,      &QPushButton::clicked,
            this, &ViewShareEdit::onEditSales);
    connect(m_btnEditDividends,  &QPushButton::clicked,
            this, &ViewShareEdit::onEditDividends);
    connect(m_btnEditBrokerages, &QPushButton::clicked,
            this, &ViewShareEdit::onEditBrokerages);

    // Käufe pencil — handled locally so we can open ViewBuyEdit and refresh
    // the summary after the sub-dialog closes.
    connect(m_btnEditBuys, &QPushButton::clicked,
            this, &ViewShareEdit::onEditBuys);
}

// ── setupUi ───────────────────────────────────────────────────────────────────

void ViewShareEdit::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    mainLayout->addWidget(createGeneralGroup());
    mainLayout->addWidget(createSummaryGroup());
    mainLayout->addStretch(1);
}

// ── createGeneralGroup ────────────────────────────────────────────────────────

QGroupBox* ViewShareEdit::createGeneralGroup()
{
    auto* gb   = new QGroupBox(tr("  Allgemein"));
    gb->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    auto* grid = new QGridLayout(gb);
    grid->setColumnStretch(1, 1);
    grid->setVerticalSpacing(4);
    grid->setHorizontalSpacing(8);
    grid->setContentsMargins(8, 8, 8, 10);
    int row = 0;

    // WKN (ISIN)
    m_wkn = new QLineEdit;
    m_wkn->setMaxLength(6);
    m_wkn->setReadOnly(true);
    m_wkn->setEnabled(false);
    m_isin = new QLineEdit;
    m_isin->setMaxLength(12);
    m_isin->setReadOnly(true);
    m_isin->setEnabled(false);
    auto* wknIsinWidget = new QWidget;
    auto* wknIsinLayout = new QHBoxLayout(wknIsinWidget);
    wknIsinLayout->setContentsMargins(0, 0, 0, 0);
    wknIsinLayout->setSpacing(4);
    wknIsinLayout->addWidget(m_wkn, 1);
    wknIsinLayout->addWidget(new QLabel(QStringLiteral("(")));
    wknIsinLayout->addWidget(m_isin, 2);
    wknIsinLayout->addWidget(new QLabel(QStringLiteral(")")));
    addRow(grid, row, tr("WKN (ISIN):"), wknIsinWidget);

    // Datum (read-only — date of first buy, set later via setFirstBuyDate)
    m_datumField = new QLineEdit;
    m_datumField->setReadOnly(true);
    m_datumField->setEnabled(false);
    m_datumField->setAlignment(Qt::AlignRight);
    addRow(grid, row, tr("Datum:"), m_datumField);

    // Name
    m_name = new QLineEdit;
    addRow(grid, row, tr("Name:"), m_name);

    // Börsennotierung (editable)
    m_listingDate = new QDateEdit(QDate::currentDate());
    m_listingDate->setCalendarPopup(true);
    m_listingDate->setDisplayFormat(tr("dd.MM.yyyy"));
    m_listingDate->setMinimumDate(QDate(1, 1, 1));
    m_listingDate->setMaximumDate(QDate(9999, 12, 31));
    addRow(grid, row, tr("Börsennotierung:"), m_listingDate);

    // Einzahlung (read-only — total buy value)
    m_einzahlung = new QLineEdit;
    m_einzahlung->setReadOnly(true);
    m_einzahlung->setEnabled(false);
    m_einzahlung->setAlignment(Qt::AlignRight);
    addRow(grid, row, tr("Einzahlung:"), m_einzahlung, tr("€"));

    // Anteile (read-only — total volume)
    m_anteile = new QLineEdit;
    m_anteile->setReadOnly(true);
    m_anteile->setEnabled(false);
    m_anteile->setAlignment(Qt::AlignRight);
    addRow(grid, row, tr("Anteile:"), m_anteile, tr("Stk."));

    // Update via Internet — radio buttons
    auto* updateWidget = new QWidget;
    auto* updateLayout = new QHBoxLayout(updateWidget);
    updateLayout->setContentsMargins(0, 0, 0, 0);
    updateLayout->setSpacing(12);

    m_updateGroup = new QButtonGroup(this);
    const struct { const char* label; int id; IconProvider::IconName icon; } radios[] = {
        { QT_TR_NOOP("Beide"),       static_cast<int>(ShareUpdateType::Both),        IconProvider::StateUpdateBoth   },
        { QT_TR_NOOP("Markt-Preis"), static_cast<int>(ShareUpdateType::MarketPrice), IconProvider::StateUpdateMarket },
        { QT_TR_NOOP("Tages-Werte"), static_cast<int>(ShareUpdateType::DailyValues), IconProvider::StateUpdateDaily  },
        { QT_TR_NOOP("Keine"),       static_cast<int>(ShareUpdateType::None),        IconProvider::StateNoUpdate     },
    };
    for (const auto& r : radios) {
        auto* rb = new QRadioButton(tr(r.label));
        rb->setIcon(IconProvider::icon(r.icon));
        m_updateGroup->addButton(rb, r.id);
        updateLayout->addWidget(rb);
    }
    updateLayout->addStretch(1);
    addRow(grid, row, tr("Update via Internet durchführen:"), updateWidget);

    // Details-Webseite
    m_detailsWebsite = new QLineEdit;
    m_detailsWebsite->setPlaceholderText(tr("https://…"));
    addRow(grid, row, tr("Details-Webseite:"), m_detailsWebsite);

    // Markt-Wert-Webseite + parsing combo
    m_marketUrl     = new QLineEdit; m_marketUrl->setPlaceholderText(tr("https://…"));
    m_marketParsing = new QComboBox;
    m_marketParsing->addItem(QStringLiteral("Regex"),      static_cast<int>(ShareParsingType::Regex));
    m_marketParsing->addItem(QStringLiteral("ApiYahoo"),   static_cast<int>(ShareParsingType::ApiYahoo));
    m_marketParsing->addItem(QStringLiteral("ApiOnVista"), static_cast<int>(ShareParsingType::ApiOnVista));
    auto* marketWidget = new QWidget;
    auto* marketLayout = new QHBoxLayout(marketWidget);
    marketLayout->setContentsMargins(0, 0, 0, 0);
    marketLayout->setSpacing(4);
    marketLayout->addWidget(m_marketUrl, 1);
    marketLayout->addWidget(m_marketParsing, 0);
    addRow(grid, row, tr("Markt-Wert-Webseite:"), marketWidget);

    m_marketApiKey = new QLineEdit;
    m_marketApiKey->setReadOnly(true);
    m_marketApiKey->setEnabled(false);
    addRow(grid, row, tr("API Schlüssel:"), m_marketApiKey);

    // Tages-Werte-Webseite + parsing combo
    m_dailyUrl     = new QLineEdit; m_dailyUrl->setPlaceholderText(tr("https://…"));
    m_dailyParsing = new QComboBox;
    m_dailyParsing->addItem(QStringLiteral("Regex"),      static_cast<int>(ShareParsingType::Regex));
    m_dailyParsing->addItem(QStringLiteral("ApiYahoo"),   static_cast<int>(ShareParsingType::ApiYahoo));
    m_dailyParsing->addItem(QStringLiteral("ApiOnVista"), static_cast<int>(ShareParsingType::ApiOnVista));
    auto* dailyWidget = new QWidget;
    auto* dailyLayout = new QHBoxLayout(dailyWidget);
    dailyLayout->setContentsMargins(0, 0, 0, 0);
    dailyLayout->setSpacing(4);
    dailyLayout->addWidget(m_dailyUrl, 1);
    dailyLayout->addWidget(m_dailyParsing, 0);
    addRow(grid, row, tr("Tages-Werte-Webseite:"), dailyWidget);

    m_dailyApiKey = new QLineEdit;
    m_dailyApiKey->setReadOnly(true);
    m_dailyApiKey->setEnabled(false);
    addRow(grid, row, tr("API Schlüssel:"), m_dailyApiKey);

    // Länder-Info
    m_countryInfo = new QComboBox;
    m_countryInfo->addItems({ "de-DE","en-US","en-GB","fr-FR","ja-JP",
                               "de-AT","de-CH","nl-NL","lu-LU","es-ES","it-IT" });
    addRow(grid, row, tr("Länder-Info:"), m_countryInfo);

    // Dividendenausschüttungs-Intervall
    m_divInterval = new QComboBox;
    m_divInterval->addItems({ tr("keine"), tr("jährlich"), tr("halbjährlich"),
                               tr("vierteljährlich"), tr("monatlich") });
    addRow(grid, row, tr("Dividendenauszahlungs-Intervall:"), m_divInterval);

    // Typ
    m_shareType = new QComboBox;
    m_shareType->addItem(tr("Aktie"), static_cast<int>(ShareType::Share));
    m_shareType->addItem(tr("Fond"),  static_cast<int>(ShareType::Fond));
    m_shareType->addItem(tr("ETF"),   static_cast<int>(ShareType::Etf));
    addRow(grid, row, tr("Typ:"), m_shareType);

    // Button row
    m_btnSave  = new QPushButton(IconProvider::icon(IconProvider::ButtonSave),
                                 tr("Speichern"));
    m_btnSave->setFixedHeight(UiConstants::kButtonHeight);
    m_btnClose = new QPushButton(IconProvider::icon(IconProvider::ButtonCancel),
                                 tr("Schließen"));
    m_btnClose->setFixedHeight(UiConstants::kButtonHeight);

    auto* btnRow = new QWidget;
    auto* btnLayout = new QHBoxLayout(btnRow);
    btnLayout->setContentsMargins(0, 4, 0, 0);
    btnLayout->addStretch(1);
    btnLayout->addWidget(m_btnSave);
    btnLayout->addWidget(m_btnClose);
    grid->addWidget(btnRow, row++, 0, 1, 3);

    connect(m_marketParsing, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ViewShareEdit::onMarketParsingTypeChanged);
    connect(m_dailyParsing,  QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ViewShareEdit::onDailyParsingTypeChanged);

    return gb;
}

// ── createSummaryGroup ────────────────────────────────────────────────────────

QGroupBox* ViewShareEdit::createSummaryGroup()
{
    auto* gb   = new QGroupBox(tr("  Einnahmen / Ausgabe"));
    gb->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    auto* grid = new QGridLayout(gb);
    grid->setColumnStretch(2, 1);
    grid->setVerticalSpacing(4);
    grid->setHorizontalSpacing(8);
    grid->setContentsMargins(8, 8, 8, 10);

    const QIcon pencilIcon = IconProvider::icon(IconProvider::ButtonEdit);

    auto makeRow = [&](int gridRow,
                       const QString& labelText,
                       QLineEdit*& valueField,
                       QPushButton*& btn) {
        auto* lbl = new QLabel(labelText);
        lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        btn = new QPushButton(pencilIcon, QString());
        btn->setFixedSize(28, UiConstants::kButtonHeight);
        btn->setToolTip(tr("Einträge erfassen / bearbeiten"));

        valueField = new QLineEdit;
        valueField->setReadOnly(true);
        valueField->setEnabled(false);
        valueField->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        valueField->setFixedHeight(UiConstants::kFieldHeight);

        auto* euroLabel = new QLabel(QStringLiteral("€"));

        grid->addWidget(lbl,        gridRow, 0);
        grid->addWidget(btn,        gridRow, 1);
        grid->addWidget(valueField, gridRow, 2);
        grid->addWidget(euroLabel,  gridRow, 3);
    };

    int r = 0;
    makeRow(r++, tr("Käufe:"),               m_totalBuys,       m_btnEditBuys);
    makeRow(r++, tr("Verkäufe:"),             m_totalSales,      m_btnEditSales);

    // Verlust aus Verkäufen — no pencil button (shares the Sales row)
    {
        auto* lbl = new QLabel(tr("Verlust aus Verkäufen:"));
        lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto* spacer = new QWidget; spacer->setFixedSize(28, 24);
        auto* plField = new QLineEdit;
        plField->setReadOnly(true);
        plField->setEnabled(false);
        plField->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_totalProfitLoss = plField;
        grid->addWidget(lbl,     r,   0);
        grid->addWidget(spacer,  r,   1);
        grid->addWidget(plField, r,   2);
        grid->addWidget(new QLabel(QStringLiteral("€")), r, 3);
        ++r;
    }

    makeRow(r++, tr("Dividenden:"), m_totalDividends,   m_btnEditDividends);
    makeRow(r++, tr("Kosten:"),     m_totalBrokerages,  m_btnEditBrokerages);

    return gb;
}

// ── onEditBuys ────────────────────────────────────────────────────────────────

void ViewShareEdit::onEditBuys()
{
    ViewBuyEdit dlg(m_shareGuid, m_config, this);

    // dataChanged is a signal of PresenterBuyEdit, not ViewBuyEdit.
    // We access it via the presenter() getter on the dialog.
    connect(dlg.presenter(), &PresenterBuyEdit::dataChanged,
            this,            &ViewShareEdit::refreshSummary);

    dlg.exec();

    // Refresh once on close in case the user only browsed without mutating.
    refreshSummary();
}

// ── onEditSales ───────────────────────────────────────────────────────────────

void ViewShareEdit::onEditSales()
{
    ViewSaleEdit dlg(m_shareGuid, m_config, this);

    connect(dlg.presenter(), &PresenterSaleEdit::dataChanged,
            this,            &ViewShareEdit::refreshSummary);

    dlg.exec();

    refreshSummary();
}

// ── onEditDividends ───────────────────────────────────────────────────────────

void ViewShareEdit::onEditDividends()
{
    ViewDividendEdit dlg(m_shareGuid, m_config, this);

    connect(dlg.presenter(), &PresenterDividendEdit::dataChanged,
            this,            &ViewShareEdit::refreshSummary);

    dlg.exec();

    refreshSummary();
}
// ── onEditBrokerages ──────────────────────────────────────────────────────────

void ViewShareEdit::onEditBrokerages()
{
    ViewBrokerageEdit dlg(m_shareGuid, this);

    connect(dlg.presenter(), &PresenterBrokerageEdit::dataChanged,
            this,            &ViewShareEdit::refreshSummary);

    dlg.exec();

    refreshSummary();
}

// ── refreshSummary ────────────────────────────────────────────────────────────

void ViewShareEdit::refreshSummary()
{
    // Delegate to the presenter which already knows how to compute the aggregates.
    // We call populateSummary() indirectly via the public onSave guard — but
    // populateSummary is private. The cleanest solution that avoids touching
    // PresenterShareEdit is to expose a dedicated public refresh slot there.
    // For now we call the presenter's loadAndPopulate path via a light wrapper
    // that only reloads aggregate numbers (not the share fields).
    //
    // NOTE: PresenterShareEdit must add a public slot "refreshSummary()" that
    //       calls populateSummary(). See the accompanying PresenterShareEdit changes.
    if (m_presenter)
        m_presenter->refreshSummary();
}

// ── IViewShareEdit: loadShare ─────────────────────────────────────────────────

void ViewShareEdit::loadShare(const ShareObject& share)
{
    m_wkn->setText(share.wkn());
    m_isin->setText(share.isin());
    m_name->setText(share.name());

    const QDate d = QDate::fromString(share.addDateTime().left(10), Qt::ISODate);
    if (d.isValid())
        m_listingDate->setDate(d);

    if (auto* rb = m_updateGroup->button(static_cast<int>(share.updateType())))
        rb->setChecked(true);

    m_detailsWebsite->setText(share.detailsWebSiteUrl());
    m_marketUrl->setText(share.marketPriceUrl());
    m_dailyUrl->setText(share.dailyValuesUrl());

    onMarketParsingTypeChanged(m_marketParsing->currentIndex());
    onDailyParsingTypeChanged(m_dailyParsing->currentIndex());

    const int mIdx = m_marketParsing->findData(static_cast<int>(share.marketPriceParsingType()));
    if (mIdx >= 0) m_marketParsing->setCurrentIndex(mIdx);
    const int dIdx = m_dailyParsing->findData(static_cast<int>(share.dailyValuesParsingType()));
    if (dIdx >= 0) m_dailyParsing->setCurrentIndex(dIdx);

    const int ciIdx = m_countryInfo->findText(share.marketPriceEncoding().isEmpty()
                                              ? QStringLiteral("de-DE")
                                              : share.marketPriceEncoding());
    if (ciIdx >= 0) m_countryInfo->setCurrentIndex(ciIdx);

    const int stIdx = m_shareType->findData(static_cast<int>(share.shareType()));
    if (stIdx >= 0) m_shareType->setCurrentIndex(stIdx);
}

// ── IViewShareEdit: aggregate setters ────────────────────────────────────────

void ViewShareEdit::setFirstBuyDate(const QString& dateStr)
{
    m_datumField->setText(dateStr.isEmpty() ? tr("-") : dateStr);
}

void ViewShareEdit::setCurrentVolume(double volume)
{
    m_anteile->setText(QLocale().toString(volume, 'f', 4));
}

void ViewShareEdit::setTotalBuys(double value, int /*count*/)
{
    m_totalBuys->setText(formatMoney(value));
    m_einzahlung->setText(formatMoney(value));
}

void ViewShareEdit::setTotalSales(double value, int /*count*/)
{
    m_totalSales->setText(formatMoney(value));
}

void ViewShareEdit::setTotalProfitLoss(double value, int /*count*/)
{
    m_totalProfitLoss->setText(formatMoney(value));
    if (value < 0.0)
        m_totalProfitLoss->setStyleSheet(QStringLiteral("color: red;"));
    else if (value > 0.0)
        m_totalProfitLoss->setStyleSheet(QStringLiteral("color: green;"));
    else
        m_totalProfitLoss->setStyleSheet(QString());
}

void ViewShareEdit::setTotalDividends(double value, int /*count*/)
{
    m_totalDividends->setText(formatMoney(value));
}

void ViewShareEdit::setTotalBrokerages(double value, int /*count*/)
{
    m_totalBrokerages->setText(formatMoney(value));
}

// ── IViewShareEdit: feedback ──────────────────────────────────────────────────

void ViewShareEdit::showError(const QString& message)
{
    OwnMessageBox::critical(this, tr("Fehler"), message);
}

void ViewShareEdit::acceptAndClose()
{
    accept();
}

// ── IViewShareEdit: read accessors ────────────────────────────────────────────

QString ViewShareEdit::wkn()  const { return m_wkn->text(); }
QString ViewShareEdit::isin() const { return m_isin->text(); }
QString ViewShareEdit::name() const { return m_name->text(); }

QDate ViewShareEdit::listingDate() const
{
    return m_listingDate->date();
}

ShareType ViewShareEdit::shareType() const
{
    return static_cast<ShareType>(m_shareType->currentData().toInt());
}

QString ViewShareEdit::dividendInterval() const
{
    return m_divInterval->currentText();
}

QString ViewShareEdit::countryInfo() const
{
    return m_countryInfo->currentText();
}

QString ViewShareEdit::detailsWebsite()    const { return m_detailsWebsite->text(); }
QString ViewShareEdit::marketPriceUrl()    const { return m_marketUrl->text(); }
QString ViewShareEdit::marketPriceApiKey() const { return m_marketApiKey->text(); }
QString ViewShareEdit::dailyValuesUrl()    const { return m_dailyUrl->text(); }
QString ViewShareEdit::dailyValuesApiKey() const { return m_dailyApiKey->text(); }

ShareParsingType ViewShareEdit::marketPriceParsingType() const
{
    return static_cast<ShareParsingType>(m_marketParsing->currentData().toInt());
}

ShareParsingType ViewShareEdit::dailyValuesParsingType() const
{
    return static_cast<ShareParsingType>(m_dailyParsing->currentData().toInt());
}

ShareUpdateType ViewShareEdit::updateType() const
{
    return static_cast<ShareUpdateType>(m_updateGroup->checkedId());
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void ViewShareEdit::onMarketParsingTypeChanged(int /*index*/)
{
    switch (marketPriceParsingType()) {
    case ShareParsingType::Regex:
        m_marketApiKey->clear();
        break;
    case ShareParsingType::ApiYahoo:
        m_marketApiKey->setText(AppSettings::instance().apiKeyYahoo());
        break;
    case ShareParsingType::ApiOnVista:
        m_marketApiKey->setText(AppSettings::instance().apiKeyOnVista());
        break;
    }
}

void ViewShareEdit::onDailyParsingTypeChanged(int /*index*/)
{
    switch (dailyValuesParsingType()) {
    case ShareParsingType::Regex:
        m_dailyApiKey->clear();
        break;
    case ShareParsingType::ApiYahoo:
        m_dailyApiKey->setText(AppSettings::instance().apiKeyYahoo());
        break;
    case ShareParsingType::ApiOnVista:
        m_dailyApiKey->setText(AppSettings::instance().apiKeyOnVista());
        break;
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────────

QLabel* ViewShareEdit::addRow(QGridLayout* grid, int& row,
                              const QString& labelText,
                              QWidget* field,
                              const QString& unitText)
{
    field->setFixedHeight(UiConstants::kFieldHeight);

    auto* lbl = new QLabel(labelText);
    lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(lbl,   row, 0);
    grid->addWidget(field, row, 1);
    if (!unitText.isEmpty()) {
        auto* unit = new QLabel(unitText);
        grid->addWidget(unit, row, 2);
    }
    ++row;
    return lbl;
}

QString ViewShareEdit::formatMoney(double value)
{
    return QLocale().toString(value, 'f', 2);
}

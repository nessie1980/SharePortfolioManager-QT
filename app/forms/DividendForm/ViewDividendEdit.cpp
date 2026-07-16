// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ViewDividendEdit.h"
#include "PresenterDividendEdit.h"
#include "ModelDividendEdit.h"
#include "../../IconProvider.h"
#include "../UiConstants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include "../OwnMessageBoxForm/OwnMessageBox.h"
#include "../../widgets/OverviewTabWidget.h"
#include "../../widgets/DocumentPreviewPanel.h"
#include <QLocale>
#include <QSizePolicy>
#include <QFileInfo>
#include <QDoubleValidator>
#include <QApplication>
#include <functional>

// ── Constructor ───────────────────────────────────────────────────────────────

ViewDividendEdit::ViewDividendEdit(const QString& shareGuid,
                                   DocumentsConfig* config,
                                   QWidget* parent)
    : QDialog(parent)
    , m_config(config)
{
    setWindowTitle(tr("Hinzufügen / editieren der Dividende(n) dieser Aktie"));
    setFixedSize(1200, 760);

    setupUi();

    auto* model = new ModelDividendEdit();
    m_presenter = new PresenterDividendEdit(this, model, shareGuid, m_config, this);

    connect(m_btnAdd,    &QPushButton::clicked, m_presenter, &PresenterDividendEdit::onSave);
    connect(m_btnRemove, &QPushButton::clicked, m_presenter, &PresenterDividendEdit::onRemove);
    connect(m_btnReset,  &QPushButton::clicked, m_presenter, &PresenterDividendEdit::onReset);
    connect(m_btnClose,  &QPushButton::clicked, m_presenter, &PresenterDividendEdit::onClose);

    // Forward numeric text changes so derived fields stay current.
    auto fwd = [this](const QString&) { m_presenter->onValuesChanged(); };
    connect(m_rate,          &QLineEdit::textChanged, this, fwd);
    connect(m_volume,        &QLineEdit::textChanged, this, fwd);
    connect(m_taxAtSource,   &QLineEdit::textChanged, this, fwd);
    connect(m_capitalGainsTax,&QLineEdit::textChanged, this, fwd);
    connect(m_solidarityTax, &QLineEdit::textChanged, this, fwd);
    connect(m_exchangeRatio, &QLineEdit::textChanged, this, fwd);
    connect(m_priceAtPayday, &QLineEdit::textChanged, this, fwd);

    // Foreign currency toggle — direkt in der View verdrahten damit
    // setEnabled/setStyleSheet sofort wirken, unabhängig vom Presenter
    connect(m_enableFc, &QCheckBox::toggled,
            this, &ViewDividendEdit::setForeignCurrencyEnabled);
    // Presenter zusätzlich informieren für refreshDerivedValues
    connect(m_enableFc, &QCheckBox::toggled,
            m_presenter, &PresenterDividendEdit::onForeignCurrencyToggled);
    // Währungswechsel → Symbol in Dividendensatz-Label und FC-Auszahlung aktualisieren
    connect(m_currency, QOverload<int>::of(&QComboBox::activated),
            this, [this](int) {
                if (m_enableFc->isChecked())
                    setForeignCurrencyEnabled(true);
            });

    // ── Live field validation ─────────────────────────────────────────────
    connect(m_date, &QDateEdit::editingFinished,
            m_presenter, &PresenterDividendEdit::onDateEdited);
    connect(m_rate, &QLineEdit::editingFinished,
            m_presenter, &PresenterDividendEdit::onRateEdited);
    connect(m_volume, &QLineEdit::editingFinished,
            m_presenter, &PresenterDividendEdit::onVolumeEdited);
    connect(m_priceAtPayday, &QLineEdit::editingFinished,
            m_presenter, &PresenterDividendEdit::onPriceAtPaydayEdited);
    connect(m_exchangeRatio, &QLineEdit::editingFinished,
            m_presenter, &PresenterDividendEdit::onExchangeRatioEdited);

    auto connectTax = [this](QLineEdit* le, const QString& key) {
        connect(le, &QLineEdit::editingFinished, m_presenter,
                [this, le, key]() {
                    m_presenter->onTaxEdited(key, parseDouble(le->text()));
                });
    };
    connectTax(m_taxAtSource,    QStringLiteral("taxAtSource"));
    connectTax(m_capitalGainsTax,QStringLiteral("capitalGainsTax"));
    connectTax(m_solidarityTax,  QStringLiteral("solidarityTax"));
}

// ── setupUi ───────────────────────────────────────────────────────────────────

void ViewDividendEdit::setupUi()
{
    auto* main = new QHBoxLayout(this);
    main->setContentsMargins(6, 6, 6, 6);
    main->setSpacing(8);

    // Dokumenten-Vorschau zuerst erzeugen (aber erst unten ins Layout
    // einfügen) — createOverviewGroup() verbindet OverviewTabWidget::
    // documentActivated mit m_previewPanel und braucht dafür ein bereits
    // existierendes Objekt (analog ViewBuyEdit/ViewSaleEdit, 16.07.2026).
    auto* previewPanel = createPreviewPanel();

    m_leftPanel  = new QWidget;
    auto* leftLayout = new QVBoxLayout(m_leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(6);
    leftLayout->addWidget(createDividenddatenGroup(), 0);
    leftLayout->addWidget(createDocumentGroup(),      0);
    leftLayout->addWidget(createButtonBar(),          0);
    leftLayout->addWidget(createOverviewGroup(),      1);
    m_leftPanel->setMinimumWidth(480);

    main->addWidget(m_leftPanel, 3);
    main->addWidget(previewPanel, 2);
}

// ── createDividenddatenGroup ──────────────────────────────────────────────────

QGroupBox* ViewDividendEdit::createDividenddatenGroup()
{
    m_dividenddatenGroup = new QGroupBox(tr("  Dividende hinzufügen"));
    m_dividenddatenGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    auto* grid = new QGridLayout(m_dividenddatenGroup);
    grid->setColumnStretch(1, 1);
    grid->setVerticalSpacing(4);
    grid->setHorizontalSpacing(8);
    grid->setContentsMargins(8, 8, 8, 10);
    int row = 0;

    // ── Datum + Uhrzeit ───────────────────────────────────────────────────
    m_date = new QDateEdit(QDate::currentDate());
    m_date->setCalendarPopup(true);
    m_date->setDisplayFormat(QStringLiteral("d . M . yyyy"));
    m_date->setFixedHeight(UiConstants::kFieldHeight);

    m_time = new QTimeEdit(QTime(0, 0, 0));
    m_time->setDisplayFormat(QStringLiteral("HH:mm:ss"));
    m_time->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_time->setFixedHeight(UiConstants::kFieldHeight);

    auto* dtWidget = new QWidget;
    auto* dtLayout = new QHBoxLayout(dtWidget);
    dtLayout->setContentsMargins(0, 0, 0, 0);
    dtLayout->setSpacing(4);
    dtLayout->addWidget(m_date, 1);
    dtLayout->addWidget(m_time, 1);
    m_statusLabels[QStringLiteral("date")] =
        addRow(grid, row, tr("Datum der Auszahlung:"), dtWidget, QString(), QStringLiteral("date"));
    m_inputWidgets[QStringLiteral("date")] = m_date;
    m_inputWidgets[QStringLiteral("time")] = m_time;

    // ── Fremdwährung aktivieren ───────────────────────────────────────────
    m_enableFc = new QCheckBox;
    m_enableFc->setFixedHeight(UiConstants::kFieldHeight);
    addRow(grid, row, tr("Fremdwährungseingabe aktivieren:"), m_enableFc);

    // ── Devisenkurs + Währungsauswahl in einer Zeile ──────────────────────
    m_exchangeRatio = new QLineEdit(QStringLiteral("1,0000"));
    m_exchangeRatio->setAlignment(Qt::AlignRight);
    m_exchangeRatio->setValidator(new QDoubleValidator(0.0001, 999999.0, 4, m_exchangeRatio));
    m_exchangeRatio->setFixedHeight(UiConstants::kFieldHeight);

    m_currency = new QComboBox;
    m_currency->setFixedHeight(UiConstants::kFieldHeight);
    const QList<QPair<QString,QString>> currencies = {
        { QStringLiteral("en-US"), QStringLiteral("$")  },
        { QStringLiteral("en-GB"), QStringLiteral("£")  },
        { QStringLiteral("ja-JP"), QStringLiteral("¥")  },
        { QStringLiteral("de-CH"), QStringLiteral("CHF")},
        { QStringLiteral("en-CA"), QStringLiteral("CAD")},
        { QStringLiteral("en-AU"), QStringLiteral("AUD")},
        { QStringLiteral("sv-SE"), QStringLiteral("SEK")},
        { QStringLiteral("nb-NO"), QStringLiteral("NOK")},
        { QStringLiteral("da-DK"), QStringLiteral("DKK")},
    };
    for (const auto& p : currencies)
        m_currency->addItem(
            QStringLiteral("%1 / %2").arg(p.first, p.second),
            p.first);

    {
        auto* fcRowWidget = new QWidget;
        auto* fcRowLayout = new QHBoxLayout(fcRowWidget);
        fcRowLayout->setContentsMargins(0, 0, 0, 0);
        fcRowLayout->setSpacing(4);
        fcRowLayout->addWidget(m_exchangeRatio, 2);
        auto* ratioSep = new QLabel;
        ratioSep->setFixedWidth(28);
        fcRowLayout->addWidget(ratioSep);
        fcRowLayout->addWidget(m_currency,      2);
        auto* currencyUnit = new QLabel;
        currencyUnit->setFixedWidth(28);
        fcRowLayout->addWidget(currencyUnit);
        fcRowWidget->setFixedHeight(UiConstants::kFieldHeight);

        auto* fcLbl = new QLabel(tr("Devisenkurs:"));
        fcLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(fcLbl,       row, 0);
        grid->addWidget(fcRowWidget, row, 1, 1, 2);
        auto* fcSpacer = new QLabel;
        fcSpacer->setFixedSize(20, 20);
        grid->addWidget(fcSpacer,    row, 3);
        ++row;

        // setEnabled NACH dem Einbetten — damit Qt die Parent-Child-Hierarchie kennt
        m_exchangeRatio->setEnabled(false);
        m_currency->setEnabled(false);
    }

    // ── Dividendensatz je Aktie ───────────────────────────────────────────
    m_rate = new QLineEdit(QStringLiteral("0,0000"));
    m_rate->setAlignment(Qt::AlignRight);
    m_rate->setValidator(new QDoubleValidator(0.0, 999999.0, 4, m_rate));
    m_rateUnit = new QLabel(QStringLiteral("€"));
    m_rateUnit->setFixedWidth(28);
    m_rateUnit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    {
        auto* rateLabel = new QLabel(tr("Dividendensatz:"));
        rateLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_rate->setFixedHeight(UiConstants::kFieldHeight);
        grid->addWidget(rateLabel,   row, 0);
        grid->addWidget(m_rate,      row, 1);
        grid->addWidget(m_rateUnit,  row, 2);
        auto* sl = new QLabel;
        sl->setFixedSize(20, 20);
        sl->setAlignment(Qt::AlignCenter);
        grid->addWidget(sl, row, 3);
        m_statusLabels[QStringLiteral("rate")] = sl;
        m_inputWidgets[QStringLiteral("rate")]  = m_rate;
        ++row;
    }

    // ── Anteile am Auszahlungstag ─────────────────────────────────────────
    m_volume = new QLineEdit(QStringLiteral("0,0000"));
    m_volume->setAlignment(Qt::AlignRight);
    m_volume->setValidator(new QDoubleValidator(0.0, 9'999'999.0, 4, m_volume));
    m_statusLabels[QStringLiteral("volume")] =
        addRow(grid, row, tr("Anteile am Auszahlungstag:"), m_volume,
               tr("stk."), QStringLiteral("volume"));
    m_inputWidgets[QStringLiteral("volume")] = m_volume;

    // ── Auszahlung € + FC (zwei Spalten in einer Zeile) ───────────────────
    m_payout = new QLineEdit(QStringLiteral("0,00"));
    m_payout->setReadOnly(true);
    m_payout->setStyleSheet(QStringLiteral("background: palette(midlight);"));
    m_payout->setAlignment(Qt::AlignRight);
    m_payout->setFixedHeight(UiConstants::kFieldHeight);

    m_payoutFc = new QLineEdit(QStringLiteral("0,00"));
    m_payoutFc->setReadOnly(true);
    m_payoutFc->setStyleSheet(QStringLiteral("background: palette(midlight);"));
    m_payoutFc->setAlignment(Qt::AlignRight);
    m_payoutFc->setFixedHeight(UiConstants::kFieldHeight);

    m_payoutFcUnit = new QLabel(QStringLiteral("$"));
    m_payoutFcUnit->setFixedWidth(28);
    m_payoutFcUnit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // ── Auszahlung € + FC in einer Zeile ─────────────────────────────────
    {
        auto* payoutRow    = new QWidget;
        auto* payoutLayout = new QHBoxLayout(payoutRow);
        payoutLayout->setContentsMargins(0, 0, 0, 0);
        payoutLayout->setSpacing(4);

        payoutLayout->addWidget(m_payout, 2);
        auto* euroLbl = new QLabel(QStringLiteral("€"));
        euroLbl->setFixedWidth(28);
        payoutLayout->addWidget(euroLbl);

        payoutLayout->addWidget(m_payoutFc, 2);
        payoutLayout->addWidget(m_payoutFcUnit);

        payoutRow->setFixedHeight(UiConstants::kFieldHeight);

        auto* lbl = new QLabel(tr("Auszahlung:"));
        lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(lbl,       row, 0);
        grid->addWidget(payoutRow, row, 1, 1, 2);
        auto* payoutSpacer = new QLabel;
        payoutSpacer->setFixedSize(20, 20);
        grid->addWidget(payoutSpacer, row, 3);
        ++row;
    }

    // ── Steuerfelder ──────────────────────────────────────────────────────
    auto addTaxRow = [&](QLineEdit*& field, const QString& label,
                          const QString& key) {
        field = new QLineEdit(QStringLiteral("0,00"));
        field->setAlignment(Qt::AlignRight);
        field->setValidator(new QDoubleValidator(0.0, 99'999.0, 2, field));
        m_statusLabels[key] =
            addRow(grid, row, label, field, tr("€"), key);
        m_inputWidgets[key] = field;
    };

    addTaxRow(m_taxAtSource,    tr("Quellsteuer:"),          QStringLiteral("taxAtSource"));
    addTaxRow(m_capitalGainsTax,tr("Kapitalertragssteuer:"), QStringLiteral("capitalGainsTax"));
    addTaxRow(m_solidarityTax,  tr("Solidaritätszuschlag:"), QStringLiteral("solidarityTax"));

    // ── Gezahlte Steuern (read-only) ──────────────────────────────────────
    m_taxSum = new QLineEdit(QStringLiteral("0,00"));
    m_taxSum->setReadOnly(true);
    m_taxSum->setStyleSheet(QStringLiteral("background: palette(midlight);"));
    m_taxSum->setAlignment(Qt::AlignRight);
    addRow(grid, row, tr("Gezahlte Steuern:"), m_taxSum, tr("€"));

    // ── Auszahlung nach Steuern (read-only, green) ────────────────────────
    m_payoutWithTaxes = new QLineEdit(QStringLiteral("0,00"));
    m_payoutWithTaxes->setReadOnly(true);
    m_payoutWithTaxes->setStyleSheet(
        QStringLiteral("background: #d4edda; color: #155724; font-weight: bold;"));
    m_payoutWithTaxes->setAlignment(Qt::AlignRight);
    addRow(grid, row, tr("Auszahlung nach Steuern:"), m_payoutWithTaxes, tr("€"));

    // ── Dividenden-Rendite nach Steuern (read-only) ───────────────────────
    m_yield = new QLineEdit(QStringLiteral("0,00"));
    m_yield->setReadOnly(true);
    m_yield->setStyleSheet(QStringLiteral("background: palette(midlight);"));
    m_yield->setAlignment(Qt::AlignRight);
    addRow(grid, row, tr("Dividenden-Rendite nach Steuern:"), m_yield, tr("%"));

    // ── Kurspreis am Auszahlungstag ───────────────────────────────────────
    m_priceAtPayday = new QLineEdit(QStringLiteral("0,00"));
    m_priceAtPayday->setAlignment(Qt::AlignRight);
    m_priceAtPayday->setValidator(new QDoubleValidator(0.0, 9'999'999.0, 2, m_priceAtPayday));
    m_statusLabels[QStringLiteral("priceAtPayday")] =
        addRow(grid, row, tr("Preis der Aktien am Auszahlungstag:"), m_priceAtPayday,
               tr("€"), QStringLiteral("priceAtPayday"));
    m_inputWidgets[QStringLiteral("priceAtPayday")] = m_priceAtPayday;

    return m_dividenddatenGroup;
}

// ── createDocumentGroup ───────────────────────────────────────────────────────

QGroupBox* ViewDividendEdit::createDocumentGroup()
{
    auto* gb   = new QGroupBox(tr("  Dokument"));
    gb->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    auto* grid = new QGridLayout(gb);
    grid->setColumnStretch(1, 1);
    grid->setVerticalSpacing(4);
    grid->setHorizontalSpacing(8);
    grid->setContentsMargins(8, 8, 8, 10);
    int row = 0;

    m_documentPath = new QLineEdit;
    m_documentPath->setReadOnly(true);
    m_documentPath->setPlaceholderText(tr("Kein Dokument ausgewählt …"));
    m_btnBrowse = new QPushButton(IconProvider::icon(IconProvider::MenuFolderOpen16), QString());
    m_btnBrowse->setFixedWidth(36);
    m_btnBrowse->setFixedHeight(UiConstants::kFieldHeight);
    m_btnBrowse->setToolTip(tr("PDF-Dokument auswählen"));
    connect(m_btnBrowse, &QPushButton::clicked, this, &ViewDividendEdit::onBrowseDocument);

    auto* docRow = new QWidget;
    auto* docL   = new QHBoxLayout(docRow);
    docL->setContentsMargins(0, 0, 0, 0);
    docL->setSpacing(4);
    docL->addWidget(m_documentPath, 1);
    docL->addWidget(m_btnBrowse);

    auto* label = new QLabel(tr("Dokument:"));
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(label,  row, 0);
    grid->addWidget(docRow, row, 1);

    auto* docStatus = new QLabel;
    docStatus->setFixedSize(20, 20);
    docStatus->setAlignment(Qt::AlignCenter);
    grid->addWidget(docStatus, row, 2);
    m_statusLabels[QStringLiteral("document")] = docStatus;
    ++row;

    // ── Parse status bar ──────────────────────────────────────────────────
    m_parseProgress = new QProgressBar;
    m_parseProgress->setRange(0, 100);
    m_parseProgress->setValue(0);
    m_parseProgress->setFixedHeight(14);
    m_parseProgress->setFixedWidth(200);
    m_parseProgress->setTextVisible(false);
    m_parseProgress->setStyleSheet(
        QStringLiteral("QProgressBar { background: transparent; border: none; }"));

    m_parseStatusIcon = new QLabel;
    m_parseStatusIcon->setFixedSize(18, 18);
    m_parseStatusIcon->setAlignment(Qt::AlignCenter);

    m_parseStatus = new QLabel;
    m_parseStatus->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* statusRow    = new QWidget;
    statusRow->setFixedHeight(22);
    auto* statusLayout = new QHBoxLayout(statusRow);
    statusLayout->setContentsMargins(0, 2, 0, 0);
    statusLayout->setSpacing(6);
    statusLayout->addWidget(m_parseProgress);
    statusLayout->addWidget(m_parseStatusIcon);
    statusLayout->addWidget(m_parseStatus, 1);

    grid->addWidget(statusRow, row, 0, 1, 4);

    return gb;
}

// ── createPreviewPanel ────────────────────────────────────────────────────────

QWidget* ViewDividendEdit::createPreviewPanel()
{
    m_previewPanel = new DocumentPreviewPanel(this);
    return m_previewPanel;
}

// ── createButtonBar ───────────────────────────────────────────────────────────

QWidget* ViewDividendEdit::createButtonBar()
{
    auto* bar    = new QWidget;
    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(0, 4, 0, 0);
    layout->setSpacing(6);

    m_btnAdd    = new QPushButton(IconProvider::icon(IconProvider::ButtonAdd),
                                  tr("Hinzufügen"));
    m_btnRemove = new QPushButton(IconProvider::icon(IconProvider::ButtonDelete),
                                  tr("Entfernen"));
    m_btnReset  = new QPushButton(IconProvider::icon(IconProvider::ButtonReset),
                                  tr("Reset"));
    m_btnClose  = new QPushButton(IconProvider::icon(IconProvider::ButtonBack),
                                  tr("Schließen"));

    for (auto* btn : { m_btnAdd, m_btnRemove, m_btnReset, m_btnClose })
        btn->setFixedHeight(UiConstants::kButtonHeight);

    layout->addStretch(1);
    layout->addWidget(m_btnAdd);
    layout->addWidget(m_btnRemove);
    layout->addWidget(m_btnReset);
    layout->addWidget(m_btnClose);

    return bar;
}

// ── createOverviewGroup ───────────────────────────────────────────────────────

QGroupBox* ViewDividendEdit::createOverviewGroup()
{
    auto* gb     = new QGroupBox(tr("  Dividenden-Übersicht"));
    auto* layout = new QVBoxLayout(gb);
    layout->setContentsMargins(6, 6, 6, 6);

    m_overviewTabs = new OverviewTabWidget();
    m_overviewTabs->setMinimumHeight(140);
    layout->addWidget(m_overviewTabs);

    // Zeilenklick in einem Jahres-Tab → Dividende laden. GUID kommt direkt
    // aus OverviewTabWidget::rowActivated(), kein eigener Slot mehr nötig.
    connect(m_overviewTabs, &OverviewTabWidget::rowActivated,
            this, [this](const QVariant& userData) {
        const QString guid = userData.toString();
        if (!guid.isEmpty() && m_presenter)
            m_presenter->onRowSelected(guid);
    });

    // Tab-Wechsel: Übersicht → Formular zurücksetzen, Jahres-Tab → erste
    // Zeile automatisch laden (ersetzt das bisherige QTabWidget::currentChanged).
    connect(m_overviewTabs, &OverviewTabWidget::currentTabChanged,
            this, [this](int newIndex) {
        if (m_suppressTabSignal) return;

        for (int i = 0; i < m_overviewTabs->count(); ++i) {
            auto* container = m_overviewTabs->widget(i);
            if (!container) continue;
            const auto tables = container->findChildren<QTableWidget*>();
            for (auto* tbl : tables)
                tbl->clearSelection();
        }
        if (newIndex == 0) {
            if (m_presenter) m_presenter->onReset();
            return;
        }
        auto* container = m_overviewTabs->widget(newIndex);
        if (!container) return;
        auto* tbl = qobject_cast<QTableWidget*>(
            container->property("dataTable").value<QObject*>());
        if (!tbl || tbl->rowCount() == 0) return;
        tbl->selectRow(0);
        auto* item = tbl->item(0, 0);
        if (!item) return;
        const QString guid = item->data(Qt::UserRole).toString();
        if (!guid.isEmpty() && m_presenter)
            m_presenter->onRowSelected(guid);
    });

    // Doppelklick auf Dokument-Spalte im Jahres-Tab → Vorschau aktualisieren.
    connect(m_overviewTabs, &OverviewTabWidget::documentActivated,
            this, [this](const QString& path) {
        m_previewPanel->showDocument(path);
    });

    return gb;
}

// ── IViewDividendEdit — read accessors ────────────────────────────────────────

QString ViewDividendEdit::dateTime() const
{
    return QDateTime(m_date->date(), m_time->time()).toString(Qt::ISODate);
}

double ViewDividendEdit::rate()            const { return parseDouble(m_rate->text());           }
double ViewDividendEdit::volume()          const { return parseDouble(m_volume->text());         }
double ViewDividendEdit::taxAtSource()     const { return parseDouble(m_taxAtSource->text());    }
double ViewDividendEdit::capitalGainsTax() const { return parseDouble(m_capitalGainsTax->text());}
double ViewDividendEdit::solidarityTax()   const { return parseDouble(m_solidarityTax->text());  }
double ViewDividendEdit::priceAtPayday()   const { return parseDouble(m_priceAtPayday->text());  }
bool   ViewDividendEdit::enableForeignCurrency() const { return m_enableFc->isChecked();         }
QString ViewDividendEdit::documentPath()   const { return m_documentPath->text();                }

double ViewDividendEdit::exchangeRatio() const
{
    const double v = parseDouble(m_exchangeRatio->text());
    return v > 0.0 ? v : 1.0;
}

QString ViewDividendEdit::currency() const
{
    // Return the IETF locale string stored as item data
    const QVariant data = m_currency->currentData();
    return data.isValid() ? data.toString() : QStringLiteral("en-US");
}

// ── IViewDividendEdit — write methods ─────────────────────────────────────────

void ViewDividendEdit::loadDividend(const DividendObject& d)
{
    const QDateTime dt = QDateTime::fromString(d.dateTime(), Qt::ISODate);
    m_date->setDate(dt.isValid() ? dt.date() : QDate::currentDate());
    m_time->setTime(dt.isValid() ? dt.time() : QTime(0, 0, 0));

    {
        QSignalBlocker block(m_enableFc);
        m_enableFc->setChecked(d.enableForeignCurrency());
    }
    setForeignCurrencyEnabled(d.enableForeignCurrency());

    m_exchangeRatio->setText(formatVolume(d.exchangeRatio() > 0.0 ? d.exchangeRatio() : 1.0));

    // Select matching currency
    {
        QSignalBlocker block(m_currency);
        for (int i = 0; i < m_currency->count(); ++i) {
            if (m_currency->itemData(i).toString() == d.currency()) {
                m_currency->setCurrentIndex(i);
                break;
            }
        }
    }

    m_rate->setText(formatVolume(d.rate()));
    m_volume->setText(formatVolume(d.volume()));
    m_taxAtSource->setText(formatMoney(d.taxAtSource()));
    m_capitalGainsTax->setText(formatMoney(d.capitalGainsTax()));
    m_solidarityTax->setText(formatMoney(d.solidarityTax()));
    m_priceAtPayday->setText(formatMoney(d.priceAtPayday()));
    m_documentPath->setText(d.document());
}

void ViewDividendEdit::clearForm()
{
    m_date->setDate(QDate::currentDate());
    m_time->setTime(QTime(0, 0, 0));
    {
        QSignalBlocker block(m_enableFc);
        m_enableFc->setChecked(false);
    }
    setForeignCurrencyEnabled(false);
    m_exchangeRatio->setText(QStringLiteral("1,0000"));
    {
        QSignalBlocker block(m_currency);
        m_currency->setCurrentIndex(0);
    }
    m_rate->setText(QStringLiteral("0,0000"));
    m_volume->setText(QStringLiteral("0,0000"));
    m_taxAtSource->setText(QStringLiteral("0,00"));
    m_capitalGainsTax->setText(QStringLiteral("0,00"));
    m_solidarityTax->setText(QStringLiteral("0,00"));
    m_priceAtPayday->setText(QStringLiteral("0,00"));
    m_documentPath->clear();

    // Restore all input fields to enabled state
    m_date->setEnabled(true);           m_date->setStyleSheet(QString());
    m_time->setEnabled(true);           m_time->setStyleSheet(QString());
    m_enableFc->setEnabled(true);
    // FC-Felder deaktiviert bis Checkbox angehakt wird.
    // StyleSheet wird von setForeignCurrencyEnabled gesetzt.
    m_exchangeRatio->setEnabled(false);
    m_exchangeRatio->setStyleSheet(QStringLiteral("background: palette(midlight);"));
    m_currency->setEnabled(false);
    m_currency->setStyleSheet(QStringLiteral("background: palette(midlight);"));
    m_rate->setEnabled(true);           m_rate->setStyleSheet(QString());
    m_volume->setEnabled(true);         m_volume->setStyleSheet(QString());
    m_taxAtSource->setEnabled(true);    m_taxAtSource->setStyleSheet(QString());
    m_capitalGainsTax->setEnabled(true);m_capitalGainsTax->setStyleSheet(QString());
    m_solidarityTax->setEnabled(true);  m_solidarityTax->setStyleSheet(QString());
    m_priceAtPayday->setEnabled(true);  m_priceAtPayday->setStyleSheet(QString());

    // Reset all field status icons
    for (auto it = m_statusLabels.begin(); it != m_statusLabels.end(); ++it) {
        if (it.value()) {
            it.value()->setPixmap(QPixmap());
            it.value()->setToolTip(QString());
        }
    }
    m_fieldStates.clear();

    // Reset parse status bar
    m_parseProgress->setValue(0);
    m_parseProgress->setStyleSheet(
        QStringLiteral("QProgressBar { background: transparent; border: none; }"));
    m_parseStatusIcon->setPixmap(QPixmap());
    m_parseStatus->clear();

    // Abgeleitete read-only-Felder auf Null zurücksetzen
    m_payout->setText(formatMoney(0.0));
    m_payoutFc->setText(formatMoney(0.0));
    m_taxSum->setText(formatMoney(0.0));
    m_payoutWithTaxes->setText(formatMoney(0.0));
    m_yield->setText(formatPercent(0.0));

    clearPdfPreview();
}

void ViewDividendEdit::setDividendPayout(double value)
{
    m_payout->setText(formatMoney(value));
}

void ViewDividendEdit::setDividendPayoutFc(double value)
{
    m_payoutFc->setText(formatMoney(value));
}

void ViewDividendEdit::setTaxSum(double value)
{
    m_taxSum->setText(formatMoney(value));
}

void ViewDividendEdit::setDividendPayoutWithTaxes(double value)
{
    m_payoutWithTaxes->setText(formatMoney(value));
    // Color: green if positive, red if negative
    if (value >= 0.0)
        m_payoutWithTaxes->setStyleSheet(
            QStringLiteral("background: #d4edda; color: #155724; font-weight: bold;"));
    else
        m_payoutWithTaxes->setStyleSheet(
            QStringLiteral("background: #f8d7da; color: #721c24; font-weight: bold;"));
}

void ViewDividendEdit::setYield(double value)
{
    m_yield->setText(formatPercent(value));
}

void ViewDividendEdit::setForeignCurrencyEnabled(bool enabled)
{
    // Felder aktivieren/deaktivieren
    m_exchangeRatio->setEnabled(enabled);
    m_currency->setEnabled(enabled);

    // Visuelles Feedback für beide Felder explizit setzen
    const QString disabledStyle = QStringLiteral("background: palette(midlight);");
    m_exchangeRatio->setStyleSheet(enabled ? QString() : disabledStyle);
    m_currency->setStyleSheet(enabled ? QString() : disabledStyle);

    // Währungssymbol im Dividendensatz-Label und FC-Auszahlungs-Label aktualisieren
    if (enabled) {
        const QString comboText = m_currency->currentText();
        const int slashPos = comboText.lastIndexOf(QLatin1Char('/'));
        const QString sym = (slashPos >= 0)
            ? comboText.mid(slashPos + 1).trimmed()
            : QStringLiteral("FW");
        m_rateUnit->setText(sym);
        m_payoutFcUnit->setText(sym);
    } else {
        m_rateUnit->setText(QStringLiteral("€"));
        m_payoutFcUnit->setText(QStringLiteral("FW"));
    }
}

// ── Field status ──────────────────────────────────────────────────────────────

void ViewDividendEdit::setFieldOk(const QString& field, const QString& value,
                                  const QString& tooltip)
{
    auto* lbl = m_statusLabels.value(field);
    if (lbl) {
        m_fieldStates[field] = FieldState::Ok;
        lbl->setPixmap(IconProvider::icon(IconProvider::SearchOk).pixmap(16, 16));
        lbl->setToolTip(tooltip.isEmpty() ? tr("Eingabe gültig") : tooltip);
        lbl->setVisible(true);
    }

    auto* widget = m_inputWidgets.value(field);
    if (!widget) return;

    if (auto* le = qobject_cast<QLineEdit*>(widget)) {
        if (!value.isEmpty()) {
            QString norm = value;
            norm.replace(QLatin1Char('.'), QLatin1Char(','));
            le->setText(norm.trimmed());
        }
    } else if (auto* de = qobject_cast<QDateEdit*>(widget)) {
        QDate d = QDate::fromString(value, QStringLiteral("d.M.yyyy"));
        if (!d.isValid()) d = QDate::fromString(value, Qt::ISODate);
        if (d.isValid()) de->setDate(d);
    } else if (auto* te = qobject_cast<QTimeEdit*>(widget)) {
        QTime t = QTime::fromString(value, QStringLiteral("h:m:s"));
        if (!t.isValid()) t = QTime::fromString(value, QStringLiteral("h:m"));
        if (t.isValid()) te->setTime(t);
    }
}

void ViewDividendEdit::setFieldError(const QString& field)
{
    auto* lbl = m_statusLabels.value(field);
    if (!lbl) return;
    m_fieldStates[field] = FieldState::Error;
    lbl->setPixmap(IconProvider::icon(IconProvider::SearchFailed).pixmap(16, 16));
    lbl->setToolTip(tr("Ungültige oder fehlende Eingabe"));
    lbl->setVisible(true);
}

void ViewDividendEdit::setDocumentPreview(const QString& /*text*/) {}

// ── Parse status bar ──────────────────────────────────────────────────────────

void ViewDividendEdit::setParseProgress(int percent, const QString& status)
{
    m_parseProgress->setStyleSheet(QString());
    m_parseProgress->setValue(percent);
    m_parseStatus->setText(status);
    if (percent < 100)
        m_parseStatusIcon->setPixmap(
            IconProvider::icon(IconProvider::SearchInfo).pixmap(16, 16));
}

void ViewDividendEdit::setParseStatusIcon(int iconType)
{
    if (!m_parseStatusIcon) return;
    IconProvider::IconName name;
    switch (iconType) {
    case 0:  name = IconProvider::SearchOk;     break;
    case 1:  name = IconProvider::SearchFailed; break;
    default: name = IconProvider::SearchInfo;   break;
    }
    m_parseStatusIcon->setPixmap(IconProvider::icon(name).pixmap(16, 16));
}

void ViewDividendEdit::setUiBusy(bool busy)
{
    if (m_leftPanel) m_leftPanel->setDisabled(busy);
    m_btnAdd->setDisabled(busy);
    m_btnClose->setDisabled(busy);
    if (busy) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        m_parseProgress->setStyleSheet(QString());
        m_parseStatusIcon->setPixmap(
            IconProvider::icon(IconProvider::SearchInfo).pixmap(16, 16));
    } else {
        QApplication::restoreOverrideCursor();
        m_parseProgress->setValue(100);
        if (m_leftPanel) m_leftPanel->update();
        update();
    }
}

void ViewDividendEdit::onParseFinished()
{
    static const QStringList requiredFieldKeys = {
        QStringLiteral("date"),
        QStringLiteral("rate"),
        QStringLiteral("volume"),
    };
    for (const QString& field : requiredFieldKeys) {
        if (m_fieldStates.value(field, FieldState::Untouched) == FieldState::Untouched) {
            m_fieldStates[field] = FieldState::Info;
            if (auto* lbl = m_statusLabels.value(field)) {
                lbl->setPixmap(
                    IconProvider::icon(IconProvider::SearchInfo).pixmap(16, 16));
                lbl->setToolTip(tr("Wert fehlt noch — bitte manuell eingeben"));
                lbl->setVisible(true);
            }
        }
    }
}

// ── populateOverview ──────────────────────────────────────────────────────────

void ViewDividendEdit::populateOverview(const QList<DividendObject>& dividends)
{
    if (dividends.isEmpty()) {
        m_overviewTabs->clear();
        return;
    }

    // Collect distinct years, sorted descending
    QList<int> years;
    for (const DividendObject& d : dividends) {
        const int y = d.year();
        if (!years.contains(y)) years.append(y);
    }
    std::sort(years.begin(), years.end(), std::greater<int>());

    // ── Übersicht-Tab (Jahr | Dividende) ──────────────────────────────────
    QMap<int, double> yearVal;
    for (const DividendObject& d : dividends)
        yearVal[d.year()] += d.dividendPayoutWithTaxes();

    double totalVal = 0.0;
    for (int y : std::as_const(years)) totalVal += yearVal.value(y);

    const QString uebersichtTitle =
        tr("Übersicht (%1 €)").arg(formatMoney(totalVal));

    auto populateUebersichtData = [years, yearVal](QTableWidget* tbl) {
        tbl->setRowCount(years.size());
        for (int i = 0; i < years.size(); ++i) {
            const int y = years.at(i);
            auto* iYear = new QTableWidgetItem(QString::number(y));
            auto* iVal  = new QTableWidgetItem(
                formatMoney(yearVal.value(y)) + QStringLiteral(" €"));
            iYear->setTextAlignment(Qt::AlignCenter);
            iVal->setTextAlignment(Qt::AlignCenter);
            iYear->setData(Qt::UserRole, y);
            tbl->setItem(i, 0, iYear);
            tbl->setItem(i, 1, iVal);
        }
    };

    auto populateUebersichtFooter = [totalVal](QTableWidget* f) {
        auto* iLabel = new QTableWidgetItem(tr("Gesamt:"));
        auto* iVal   = new QTableWidgetItem(
            formatMoney(totalVal) + QStringLiteral(" €"));
        iLabel->setTextAlignment(Qt::AlignCenter);
        iVal->setTextAlignment(Qt::AlignCenter);
        f->setItem(0, 0, iLabel);
        f->setItem(0, 1, iVal);
    };

    // ── Jahres-Tabs (Datum | Dividendensatz | Anteile | Dividende | Dok.) ──
    constexpr int kColDate     = 0;
    constexpr int kColRate     = 1;
    constexpr int kColVolume   = 2;
    constexpr int kColDividend = 3;
    constexpr int kColDoc      = 4;

    const QStringList jahresHeaders = {
        tr("Datum"), tr("Dividendensatz"), tr("Anteile"),
        tr("Dividende"), tr("Dokument")
    };

    auto jahresTitleForYear = [this, dividends](int year) {
        double yearTotal = 0.0;
        for (const DividendObject& d : dividends)
            if (d.year() == year)
                yearTotal += d.dividendPayoutWithTaxes();
        return tr("%1 (%2 €)").arg(year).arg(formatMoney(yearTotal));
    };

    auto populateJahresData = [this, dividends, kColDate, kColRate, kColVolume,
                                kColDividend, kColDoc](int year, QTableWidget* tbl) {
        QList<DividendObject> yearDivs;
        for (const DividendObject& d : dividends)
            if (d.year() == year)
                yearDivs.append(d);

        tbl->setRowCount(yearDivs.size());
        for (int i = 0; i < yearDivs.size(); ++i) {
            const DividendObject& d = yearDivs.at(i);

            auto* iDate = new QTableWidgetItem(d.dateAsStr());
            iDate->setData(Qt::UserRole, d.guid());
            iDate->setTextAlignment(Qt::AlignCenter);

            auto* iRate = new QTableWidgetItem(
                formatVolume(d.rate()) + QStringLiteral(" €"));
            iRate->setTextAlignment(Qt::AlignCenter);

            auto* iVol = new QTableWidgetItem(
                formatVolume(d.volume()) + QStringLiteral(" stk."));
            iVol->setTextAlignment(Qt::AlignCenter);

            auto* iDiv = new QTableWidgetItem(
                formatMoney(d.dividendPayoutWithTaxes()) + QStringLiteral(" €"));
            iDiv->setTextAlignment(Qt::AlignCenter);

            auto* iDoc = new QTableWidgetItem;
            iDoc->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            if (!d.document().isEmpty()) {
                iDoc->setData(Qt::UserRole, d.document());
                const QString ext = QFileInfo(d.document()).suffix().toLower();
                IconProvider::IconName iconName;
                if (ext == QStringLiteral("pdf"))
                    iconName = IconProvider::DocPdfImage16;
                else if (ext == QStringLiteral("doc") || ext == QStringLiteral("docx"))
                    iconName = IconProvider::DocWordImage16;
                else if (ext == QStringLiteral("xls") || ext == QStringLiteral("xlsx"))
                    iconName = IconProvider::DocExcelImage16;
                else
                    iconName = IconProvider::SearchFailed2;
                auto* iconLabel = new QLabel;
                iconLabel->setPixmap(IconProvider::icon(iconName).pixmap(16, 16));
                iconLabel->setAlignment(Qt::AlignCenter);
                iconLabel->setToolTip(d.document());
                tbl->setItem(i, kColDoc, iDoc);
                tbl->setCellWidget(i, kColDoc, iconLabel);
            } else {
                iDoc->setText(QStringLiteral("-"));
                iDoc->setTextAlignment(Qt::AlignCenter);
                tbl->setItem(i, kColDoc, iDoc);
            }

            tbl->setItem(i, kColDate,     iDate);
            tbl->setItem(i, kColRate,     iRate);
            tbl->setItem(i, kColVolume,   iVol);
            tbl->setItem(i, kColDividend, iDiv);
        }
    };

    auto populateJahresFooter = [this, dividends, kColDate, kColRate, kColVolume,
                                  kColDividend, kColDoc](int year, QTableWidget* f) {
        double totVol = 0.0, totDiv = 0.0;
        for (const DividendObject& d : dividends) {
            if (d.year() != year) continue;
            totVol += d.volume();
            totDiv += d.dividendPayoutWithTaxes();
        }
        auto* iLabel = new QTableWidgetItem(tr("Gesamt:"));
        iLabel->setTextAlignment(Qt::AlignCenter);
        auto* iRate  = new QTableWidgetItem(QStringLiteral("-"));
        iRate->setTextAlignment(Qt::AlignCenter);
        auto* iVol   = new QTableWidgetItem(
            formatVolume(totVol) + QStringLiteral(" stk."));
        iVol->setTextAlignment(Qt::AlignCenter);
        auto* iDiv   = new QTableWidgetItem(
            formatMoney(totDiv) + QStringLiteral(" €"));
        iDiv->setTextAlignment(Qt::AlignCenter);
        auto* iDoc   = new QTableWidgetItem(QStringLiteral("-"));
        iDoc->setTextAlignment(Qt::AlignCenter);
        f->setItem(0, kColDate,     iLabel);
        f->setItem(0, kColRate,     iRate);
        f->setItem(0, kColVolume,   iVol);
        f->setItem(0, kColDividend, iDiv);
        f->setItem(0, kColDoc,      iDoc);
    };

    // Dokument-Spalte als Stretch (konsistent zu ViewBuyEdit) — TODO: künftig
    // auf feste 36px ohne Spaltenüberschrift umstellen, siehe ARCHITECTURE.md.
    m_overviewTabs->populateOverview(
        years,
        uebersichtTitle,
        { tr("Jahr"), tr("Dividende") },
        { 100, -1 },
        populateUebersichtData,
        populateUebersichtFooter,
        jahresHeaders,
        { 100, -1, -1, -1, -1 },
        jahresTitleForYear,
        populateJahresData,
        populateJahresFooter,
        kColDoc);
}

// ── showOverviewTab ───────────────────────────────────────────────────────────

void ViewDividendEdit::showOverviewTab()
{
    if (m_overviewTabs && m_overviewTabs->currentIndex() != 0) {
        m_suppressTabSignal = true;
        m_overviewTabs->setCurrentIndex(0);
        m_suppressTabSignal = false;
    }
    clearForm();
}

// ── PDF preview ───────────────────────────────────────────────────────────────

void ViewDividendEdit::clearPdfPreview()
{
    m_previewPanel->clearDocument();
}

void ViewDividendEdit::openPdfPreview(const QString& pdfPath)
{
    m_previewPanel->showDocument(pdfPath);
}

// ── setButtonStates ───────────────────────────────────────────────────────────

void ViewDividendEdit::setButtonStates(bool canRemove, bool isEdit)
{
    m_btnRemove->setEnabled(canRemove);

    if (isEdit) {
        m_btnAdd->setText(tr("Speichern"));
        m_btnAdd->setIcon(IconProvider::icon(IconProvider::ButtonSave));
        if (m_dividenddatenGroup)
            m_dividenddatenGroup->setTitle(tr("  Dividende editieren"));
    } else {
        m_btnAdd->setText(tr("Hinzufügen"));
        m_btnAdd->setIcon(IconProvider::icon(IconProvider::ButtonAdd));
        if (m_dividenddatenGroup)
            m_dividenddatenGroup->setTitle(tr("  Dividende hinzufügen"));
    }

    // All dividend fields are always editable — no read-only mode.
    // Exception: FC fields depend on the checkbox state.
    m_date->setEnabled(true);
    m_time->setEnabled(true);
    m_enableFc->setEnabled(true);
    const bool fcOn = m_enableFc->isChecked();
    const QString disabledFcStyle = QStringLiteral("background: palette(midlight);");
    m_exchangeRatio->setEnabled(fcOn);
    m_exchangeRatio->setStyleSheet(fcOn ? QString() : disabledFcStyle);
    m_currency->setEnabled(fcOn);
    m_currency->setStyleSheet(fcOn ? QString() : disabledFcStyle);
    m_rate->setEnabled(true);
    m_volume->setEnabled(true);
    m_taxAtSource->setEnabled(true);
    m_capitalGainsTax->setEnabled(true);
    m_solidarityTax->setEnabled(true);
    m_priceAtPayday->setEnabled(true);
    m_btnBrowse->setEnabled(true);
}

// ── showError / acceptAndClose ────────────────────────────────────────────────

void ViewDividendEdit::showError(const QString& message)
{
    OwnMessageBox::critical(this, tr("Fehler"), message);
}

void ViewDividendEdit::acceptAndClose()
{
    accept();
}

// ── Private slots ─────────────────────────────────────────────────────────────

void ViewDividendEdit::onBrowseDocument()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("PDF-Dokument auswählen"),
        m_documentPath->text(),
        tr("PDF-Dokumente (*.pdf);;Alle Dateien (*)"));

    if (!path.isEmpty()) {
        m_documentPath->setText(path);
        m_presenter->onDocumentPathEdited();
        m_presenter->onDocumentSelected(path);
    }
}

// ── Static helpers ────────────────────────────────────────────────────────────

QLabel* ViewDividendEdit::addRow(QGridLayout* grid, int& row,
                                  const QString& labelText,
                                  QWidget* field,
                                  const QString& unitText,
                                  const QString& statusKey)
{
    field->setFixedHeight(UiConstants::kFieldHeight);

    auto* lbl = new QLabel(labelText);
    lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(lbl, row, 0);

    if (unitText.isEmpty()) {
        grid->addWidget(field, row, 1, 1, 2);
    } else {
        auto* unit = new QLabel(unitText);
        unit->setFixedWidth(28);
        unit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        grid->addWidget(field, row, 1);
        grid->addWidget(unit,  row, 2);
    }

    QLabel* statusLabel = nullptr;
    if (!statusKey.isEmpty()) {
        statusLabel = new QLabel;
        statusLabel->setFixedSize(20, 20);
        statusLabel->setAlignment(Qt::AlignCenter);
        grid->addWidget(statusLabel, row, 3);
    }

    ++row;
    return statusLabel;
}

QString ViewDividendEdit::formatMoney(double value)
{
    return QLocale().toString(value, 'f', 2);
}

QString ViewDividendEdit::formatVolume(double value)
{
    return QLocale().toString(value, 'f', 4);
}

QString ViewDividendEdit::formatPercent(double value)
{
    return QLocale().toString(value, 'f', 2);
}

double ViewDividendEdit::parseDouble(const QString& text)
{
    QString s = text.trimmed();
    s.replace(QLatin1Char(','), QLatin1Char('.'));
    bool ok = false;
    const double v = s.toDouble(&ok);
    return ok ? v : 0.0;
}

// ── markMissingFieldsAsFailed ─────────────────────────────────────────────────

void ViewDividendEdit::markMissingFieldsAsFailed()
{
    struct Check { QString key; bool ok; };
    const QList<Check> checks = {
        { QStringLiteral("rate"),          parseDouble(m_rate->text())          > 0.0 },
        { QStringLiteral("volume"),        parseDouble(m_volume->text())        > 0.0 },
        { QStringLiteral("priceAtPayday"), parseDouble(m_priceAtPayday->text()) > 0.0 },
    };
    for (const auto& c : checks) {
        auto* lbl = m_statusLabels.value(c.key);
        if (!lbl) continue;
        if (c.ok) {
            if (m_fieldStates.value(c.key, FieldState::Untouched) != FieldState::Ok) {
                m_fieldStates[c.key] = FieldState::Ok;
                lbl->setPixmap(IconProvider::icon(IconProvider::SearchOk).pixmap(16, 16));
                lbl->setToolTip(tr("Feld ausgefüllt"));
            }
        } else {
            m_fieldStates[c.key] = FieldState::Error;
            lbl->setPixmap(IconProvider::icon(IconProvider::SearchFailed).pixmap(16, 16));
            lbl->setToolTip(tr("Pflichtfeld — bitte ausfüllen"));
        }
    }
}

// ── hasMissingRequiredFields ──────────────────────────────────────────────────

bool ViewDividendEdit::hasMissingRequiredFields(QStringList& missingFields) const
{
    missingFields.clear();
    if (parseDouble(m_rate->text())          <= 0.0) missingFields.append(tr("Dividendensatz"));
    if (parseDouble(m_volume->text())        <= 0.0) missingFields.append(tr("Anteile am Auszahlungstag"));
    if (parseDouble(m_priceAtPayday->text()) <= 0.0) missingFields.append(tr("Preis der Aktie am Auszahlungstag"));
    return !missingFields.isEmpty();
}

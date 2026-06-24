// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ViewBrokerageEdit.h"
#include "PresenterBrokerageEdit.h"
#include "ModelBrokerageEdit.h"
#include "../../IconProvider.h"
#include "../UiConstants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFrame>
#include <QFileDialog>
#include "../OwnMessageBoxForm/OwnMessageBox.h"
#include <QLocale>
#include <QSizePolicy>
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QDoubleValidator>

#ifndef SPM_HAVE_QTPDF
#  include <QProcess>
#endif

// ── Constructor ───────────────────────────────────────────────────────────────

ViewBrokerageEdit::ViewBrokerageEdit(const QString& shareGuid,
                                     QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Hinzufügen / editieren der Kosten dieser Aktie"));
    setFixedSize(1100, 680);

    setupUi();

    auto* model = new ModelBrokerageEdit();
    m_presenter = new PresenterBrokerageEdit(this, model, shareGuid, this);

    connect(m_btnAdd,    &QPushButton::clicked, m_presenter, &PresenterBrokerageEdit::onSave);
    connect(m_btnRemove, &QPushButton::clicked, m_presenter, &PresenterBrokerageEdit::onRemove);
    connect(m_btnReset,  &QPushButton::clicked, m_presenter, &PresenterBrokerageEdit::onReset);
    connect(m_btnClose,  &QPushButton::clicked, m_presenter, &PresenterBrokerageEdit::onClose);

    // Forward fee text changes so derived fields stay current.
    auto fwd = [this](const QString&) { m_presenter->onValuesChanged(); };
    connect(m_provision,  &QLineEdit::textChanged, this, fwd);
    connect(m_brokerFee,  &QLineEdit::textChanged, this, fwd);
    connect(m_traderFee,  &QLineEdit::textChanged, this, fwd);
    connect(m_reduction,  &QLineEdit::textChanged, this, fwd);

    // Live field validation
    connect(m_date, &QDateEdit::editingFinished,
            m_presenter, &PresenterBrokerageEdit::onDateEdited);

    auto connectFee = [this](QLineEdit* le, const QString& key) {
        connect(le, &QLineEdit::editingFinished, m_presenter,
                [this, le, key]() {
                    m_presenter->onFeeEdited(key, parseDouble(le->text()));
                });
    };
    connectFee(m_provision,  QStringLiteral("provision"));
    connectFee(m_brokerFee,  QStringLiteral("brokerFee"));
    connectFee(m_traderFee,  QStringLiteral("traderFee"));
    connectFee(m_reduction,  QStringLiteral("reduction"));
}

// ── setupUi ───────────────────────────────────────────────────────────────────

void ViewBrokerageEdit::setupUi()
{
    // Identical structure to BuysForm/SalesForm/DividendForm:
    // Left panel (form + overview) | Right panel (PDF preview) — top-level QHBoxLayout
    auto* main = new QHBoxLayout(this);
    main->setContentsMargins(6, 6, 6, 6);
    main->setSpacing(8);

    // ── Left panel ────────────────────────────────────────────────────────
    auto* leftPanel  = new QWidget;
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(6);
    leftLayout->addWidget(createKostendatenGroup(), 0);
    leftLayout->addWidget(createDocumentGroup(),    0);
    leftLayout->addWidget(createButtonBar(),        0);
    leftLayout->addWidget(createOverviewGroup(),    1);

    main->addWidget(leftPanel,            3);
    main->addWidget(createPreviewPanel(), 2);
}

// ── createLeftPanel ───────────────────────────────────────────────────────────
// (Left panel is built inline in setupUi — this stub kept for header compatibility)
QWidget* ViewBrokerageEdit::createLeftPanel()
{
    // Not called — left panel is constructed directly in setupUi().
    return new QWidget;
}

// ── createKostendatenGroup ────────────────────────────────────────────────────

QGroupBox* ViewBrokerageEdit::createKostendatenGroup()
{
    m_kostendatenGroup = new QGroupBox(tr("  Kosten hinzufügen"));
    m_kostendatenGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    auto* grid = new QGridLayout(m_kostendatenGroup);
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
    addRow(grid, row, tr("Datum / Uhrzeit:"), dtWidget);

    // ── Provision ─────────────────────────────────────────────────────────
    m_provision = new QLineEdit(QStringLiteral("0,00"));
    m_provision->setAlignment(Qt::AlignRight);
    m_provision->setValidator(new QDoubleValidator(0.0, 9999999.0, 2, m_provision));
    m_provision->setFixedHeight(UiConstants::kFieldHeight);
    addRow(grid, row, tr("Provision:"), m_provision, tr("€"));

    // ── Courtage ──────────────────────────────────────────────────────────
    m_brokerFee = new QLineEdit(QStringLiteral("0,00"));
    m_brokerFee->setAlignment(Qt::AlignRight);
    m_brokerFee->setValidator(new QDoubleValidator(0.0, 9999999.0, 2, m_brokerFee));
    m_brokerFee->setFixedHeight(UiConstants::kFieldHeight);
    addRow(grid, row, tr("Courtage:"), m_brokerFee, tr("€"));

    // ── Handelsplatzgebühr ────────────────────────────────────────────────
    m_traderFee = new QLineEdit(QStringLiteral("0,00"));
    m_traderFee->setAlignment(Qt::AlignRight);
    m_traderFee->setValidator(new QDoubleValidator(0.0, 9999999.0, 2, m_traderFee));
    m_traderFee->setFixedHeight(UiConstants::kFieldHeight);
    addRow(grid, row, tr("Handelsplatzgebühr:"), m_traderFee, tr("€"));

    // ── Ges. Gebühren (read-only) ─────────────────────────────────────────
    m_gesGebuehren = new QLineEdit(QStringLiteral("0,00"));
    m_gesGebuehren->setReadOnly(true);
    m_gesGebuehren->setAlignment(Qt::AlignRight);
    m_gesGebuehren->setFixedHeight(UiConstants::kFieldHeight);
    m_gesGebuehren->setStyleSheet(
        QStringLiteral("QLineEdit { background: palette(midlight); }"));
    addRow(grid, row, tr("Ges. Gebühren:"), m_gesGebuehren, tr("€"));

    // ── Rabatt ────────────────────────────────────────────────────────────
    m_reduction = new QLineEdit(QStringLiteral("0,00"));
    m_reduction->setAlignment(Qt::AlignRight);
    m_reduction->setValidator(new QDoubleValidator(0.0, 9999999.0, 2, m_reduction));
    m_reduction->setFixedHeight(UiConstants::kFieldHeight);
    addRow(grid, row, tr("Rabatt:"), m_reduction, tr("€"));

    // ── Netto-Kosten (read-only, coloured) ────────────────────────────────
    m_nettoKosten = new QLineEdit(QStringLiteral("0,00"));
    m_nettoKosten->setReadOnly(true);
    m_nettoKosten->setAlignment(Qt::AlignRight);
    m_nettoKosten->setFixedHeight(UiConstants::kFieldHeight);
    m_nettoKosten->setStyleSheet(
        QStringLiteral("QLineEdit { background: #d4edda; color: #155724; font-weight: bold; }"));
    addRow(grid, row, tr("Netto-Kosten:"), m_nettoKosten, tr("€"));

    return m_kostendatenGroup;
}

// ── createDocumentGroup ───────────────────────────────────────────────────────

QGroupBox* ViewBrokerageEdit::createDocumentGroup()
{
    auto* gb     = new QGroupBox(tr("  Dokument"));
    auto* layout = new QHBoxLayout(gb);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);

    m_documentPath = new QLineEdit;
    m_documentPath->setReadOnly(true);
    m_documentPath->setPlaceholderText(tr("Kein Dokument ausgewählt"));
    m_documentPath->setFixedHeight(UiConstants::kFieldHeight);

    m_btnBrowse = new QPushButton(
        IconProvider::icon(IconProvider::MenuFolderOpen16), QString());
    m_btnBrowse->setFixedSize(UiConstants::kFieldHeight, UiConstants::kFieldHeight);
    m_btnBrowse->setToolTip(tr("Dokument auswählen"));
    connect(m_btnBrowse, &QPushButton::clicked, this, &ViewBrokerageEdit::onBrowseDocument);

    layout->addWidget(m_documentPath, 1);
    layout->addWidget(m_btnBrowse);
    return gb;
}

// ── createButtonBar ───────────────────────────────────────────────────────────

QWidget* ViewBrokerageEdit::createButtonBar()
{
    auto* bar    = new QWidget;
    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

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

    m_btnRemove->setEnabled(false);

    layout->addStretch(1);
    layout->addWidget(m_btnAdd);
    layout->addWidget(m_btnRemove);
    layout->addWidget(m_btnReset);
    layout->addWidget(m_btnClose);
    return bar;
}

// ── createPreviewPanel ────────────────────────────────────────────────────────

QWidget* ViewBrokerageEdit::createPreviewPanel()
{
    auto* gb     = new QGroupBox(tr("  Dokumenten-Vorschau"));
    auto* layout = new QVBoxLayout(gb);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);

#ifdef SPM_HAVE_QTPDF
    m_pdfDocument = new QPdfDocument(this);
    m_pdfView     = new QPdfView(this);
    m_pdfView->setDocument(m_pdfDocument);
    m_pdfView->setPageMode(QPdfView::PageMode::MultiPage);
    m_pdfView->setZoomMode(QPdfView::ZoomMode::FitInView);
    m_pdfView->setFrameShape(QFrame::StyledPanel);
    m_pdfView->setStyleSheet(
        QStringLiteral("QPdfView { background-color: #ffffff; }"
                        "QPdfView > QWidget { background-color: #ffffff; }"));

    auto* zoomBar    = new QWidget;
    auto* zoomLayout = new QHBoxLayout(zoomBar);
    zoomLayout->setContentsMargins(0, 0, 0, 2);
    zoomLayout->setSpacing(4);

    auto* btnZoomOut = new QPushButton(QStringLiteral("−"));
    auto* btnZoomIn  = new QPushButton(QStringLiteral("+"));
    auto* btnFit     = new QPushButton(tr("Anpassen"));
    btnZoomOut->setFixedWidth(28);
    btnZoomIn->setFixedWidth(28);
    btnFit->setFixedWidth(80);

    m_zoomLabel = new QLabel(QStringLiteral("100%"));
    m_zoomLabel->setFixedWidth(48);
    m_zoomLabel->setAlignment(Qt::AlignCenter);

    zoomLayout->addWidget(btnZoomOut);
    zoomLayout->addWidget(btnZoomIn);
    zoomLayout->addWidget(btnFit);
    zoomLayout->addWidget(m_zoomLabel);
    zoomLayout->addStretch(1);

    connect(btnZoomIn, &QPushButton::clicked, this, [this] {
        const qreal z = qMin(m_pdfView->zoomFactor() * 1.25, 4.0);
        m_pdfView->setZoomMode(QPdfView::ZoomMode::Custom);
        m_pdfView->setZoomFactor(z);
        m_zoomLabel->setText(QString::number(qRound(z * 100)) + QStringLiteral("%"));
    });
    connect(btnZoomOut, &QPushButton::clicked, this, [this] {
        const qreal z = qMax(m_pdfView->zoomFactor() * 0.8, 0.25);
        m_pdfView->setZoomMode(QPdfView::ZoomMode::Custom);
        m_pdfView->setZoomFactor(z);
        m_zoomLabel->setText(QString::number(qRound(z * 100)) + QStringLiteral("%"));
    });
    connect(btnFit, &QPushButton::clicked, this, [this] {
        m_pdfView->setZoomMode(QPdfView::ZoomMode::FitInView);
        m_zoomLabel->setText(tr("Anp."));
    });

    layout->addWidget(zoomBar);
    layout->addWidget(m_pdfView, 1);
#else
    m_pdfLabel = new QLabel;
    m_pdfLabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    m_pdfLabel->setText(tr("Wählen Sie ein Dokument aus.\n"
                            "Die erste Seite wird hier angezeigt."));
    m_pdfLabel->setWordWrap(true);
    m_pdfScroll = new QScrollArea;
    m_pdfScroll->setWidget(m_pdfLabel);
    m_pdfScroll->setWidgetResizable(true);
    m_pdfScroll->setFrameShape(QFrame::StyledPanel);
    layout->addWidget(m_pdfScroll, 1);
#endif

    return gb;
}

// ── createOverviewGroup ───────────────────────────────────────────────────────

QGroupBox* ViewBrokerageEdit::createOverviewGroup()
{
    auto* gb     = new QGroupBox(tr("  Kosten-Übersicht"));
    gb->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    auto* layout = new QVBoxLayout(gb);
    layout->setContentsMargins(4, 4, 4, 4);

    m_tabs = new QTabWidget;
    m_tabs->setTabPosition(QTabWidget::North);
    m_tabs->setMinimumHeight(140);
    layout->addWidget(m_tabs);

    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (m_suppressTabSignal) return;
        if (index <= 0) {
            m_presenter->onReset();
            return;
        }
        auto* container = m_tabs->widget(index);
        if (!container) return;
        auto* tbl = container->property("dataTable").value<QTableWidget*>();
        if (!tbl || tbl->rowCount() == 0) return;
        tbl->selectRow(0);
        const QString guid = tbl->item(0, 0)
                             ? tbl->item(0, 0)->data(Qt::UserRole).toString()
                             : QString();
        if (!guid.isEmpty())
            m_presenter->onRowSelected(guid);
    });

    return gb;
}

// ── addRow ────────────────────────────────────────────────────────────────────

QLabel* ViewBrokerageEdit::addRow(QGridLayout* grid, int& row,
                                   const QString& labelText,
                                   QWidget* field,
                                   const QString& unitText)
{
    auto* lbl = new QLabel(labelText);
    lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(lbl, row, 0);

    if (unitText.isEmpty()) {
        grid->addWidget(field, row, 1, 1, 2);
    } else {
        grid->addWidget(field, row, 1);
        auto* unit = new QLabel(unitText);
        unit->setFixedWidth(28);
        grid->addWidget(unit, row, 2);
    }
    ++row;
    return lbl;
}

// ── IViewBrokerageEdit: read accessors ───────────────────────────────────────

QString ViewBrokerageEdit::dateTime() const
{
    const QDate d = m_date->date();
    const QTime t = m_time->time();
    return QDateTime(d, t).toString(Qt::ISODate);
}

double  ViewBrokerageEdit::provision()    const { return parseDouble(m_provision->text()); }
double  ViewBrokerageEdit::brokerFee()    const { return parseDouble(m_brokerFee->text()); }
double  ViewBrokerageEdit::traderFee()    const { return parseDouble(m_traderFee->text()); }
double  ViewBrokerageEdit::reduction()    const { return parseDouble(m_reduction->text()); }
QString ViewBrokerageEdit::documentPath() const { return m_documentPath->text(); }

// ── IViewBrokerageEdit: write methods ────────────────────────────────────────

void ViewBrokerageEdit::loadBrokerage(const BrokerageObject& brokerage)
{
    const QDateTime dt = QDateTime::fromString(brokerage.dateTime(), Qt::ISODate);
    if (dt.isValid()) {
        m_date->blockSignals(true);
        m_date->setDate(dt.date());
        m_date->blockSignals(false);
        m_time->setTime(dt.time());
    }

    const auto setMoney = [](QLineEdit* le, double v) {
        le->setText(QLocale().toString(v, 'f', 2));
    };
    setMoney(m_provision, brokerage.provision());
    setMoney(m_brokerFee, brokerage.brokerFee());
    setMoney(m_traderFee, brokerage.traderFee());
    setMoney(m_reduction, brokerage.reduction());

    m_documentPath->setText(brokerage.document());
}

void ViewBrokerageEdit::clearForm()
{
    m_date->blockSignals(true);
    m_date->setDate(QDate::currentDate());
    m_date->blockSignals(false);
    m_time->setTime(QTime(0, 0, 0));

    const QString zero = QStringLiteral("0,00");
    m_provision->setText(zero);
    m_brokerFee->setText(zero);
    m_traderFee->setText(zero);
    m_reduction->setText(zero);
    m_gesGebuehren->setText(zero);
    m_nettoKosten->setText(zero);
    m_nettoKosten->setStyleSheet(
        QStringLiteral("QLineEdit { background: #d4edda; color: #155724; font-weight: bold; }"));

    m_documentPath->clear();
    m_fieldStates.clear();

    // Restore editable state in case readOnly was active
    for (QLineEdit* le : { m_provision, m_brokerFee, m_traderFee, m_reduction })
        le->setEnabled(true);
    m_date->setEnabled(true);
    m_time->setEnabled(true);
    m_btnBrowse->setEnabled(true);
    m_btnAdd->setEnabled(true);

    m_kostendatenGroup->setTitle(tr("  Kosten hinzufügen"));
}

void ViewBrokerageEdit::setGesamtGebuehren(double value)
{
    m_gesGebuehren->setText(formatMoney(value));
}

void ViewBrokerageEdit::setBrokerageReduction(double value)
{
    m_nettoKosten->setText(formatMoney(value));
    const QString style = value > 0.0
        ? QStringLiteral("QLineEdit { background: #d4edda; color: #155724; font-weight: bold; }")
        : QStringLiteral("QLineEdit { background: #f8d7da; color: #721c24; font-weight: bold; }");
    m_nettoKosten->setStyleSheet(style);
}

void ViewBrokerageEdit::setDocumentPreview(const QString& text)
{
    m_documentPath->setText(text);
}

// ── PDF preview ───────────────────────────────────────────────────────────────

void ViewBrokerageEdit::openPdfPreview(const QString& pdfPath)
{
#ifdef SPM_HAVE_QTPDF
    m_pdfDocument->close();
    m_pdfDocument->load(pdfPath);
    m_pdfView->setZoomMode(QPdfView::ZoomMode::FitInView);
    QTimer::singleShot(100, this, [this]() {
        m_pdfView->setZoomMode(QPdfView::ZoomMode::FitInView);
    });
#else
    m_pdfImagePath = QDir::tempPath() + QStringLiteral("/spm_brok_preview");
    if (m_pdfRenderProc) { m_pdfRenderProc->kill(); m_pdfRenderProc->deleteLater(); }
    m_pdfRenderProc = new QProcess(this);
    connect(m_pdfRenderProc,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
        if (m_pdfRenderProc) { m_pdfRenderProc->deleteLater(); m_pdfRenderProc = nullptr; }
        if (exitCode != 0) {
            m_pdfLabel->setText(tr("PDF-Vorschau konnte nicht gerendert werden."));
            return;
        }
        const QString imgPath = m_pdfImagePath + QStringLiteral("-1.png");
        QPixmap px(imgPath);
        if (px.isNull()) { m_pdfLabel->setText(tr("Vorschaubild nicht gefunden.")); return; }
        const int availW = m_pdfScroll->viewport()->width() - 4;
        if (availW > 0 && px.width() > availW)
            px = px.scaledToWidth(availW, Qt::SmoothTransformation);
        m_pdfLabel->setPixmap(px);
        m_pdfLabel->resize(px.size());
    });
    m_pdfRenderProc->start(QStringLiteral("pdftoppm"),
                           { QStringLiteral("-r"),   QStringLiteral("150"),
                             QStringLiteral("-png"),
                             QStringLiteral("-f"),   QStringLiteral("1"),
                             QStringLiteral("-l"),   QStringLiteral("1"),
                             pdfPath, m_pdfImagePath });
#endif
}

void ViewBrokerageEdit::clearPdfPreview()
{
#ifdef SPM_HAVE_QTPDF
    m_pdfDocument->close();
#else
    if (m_pdfRenderProc) {
        m_pdfRenderProc->kill();
        m_pdfRenderProc->deleteLater();
        m_pdfRenderProc = nullptr;
    }
    m_pdfLabel->clear();
    m_pdfLabel->setText(tr("Kein Dokument ausgewählt."));
#endif
}

// ── populateOverview ──────────────────────────────────────────────────────────

void ViewBrokerageEdit::populateOverview(const QList<BrokerageObject>& brokerages)
{
    m_suppressTabSignal = true;

    while (m_tabs->count() > 0)
        m_tabs->removeTab(0);

    if (brokerages.isEmpty()) {
        m_suppressTabSignal = false;
        return;
    }

    // ── Collect years (descending) ────────────────────────────────────────
    QList<int> years;
    for (const auto& b : brokerages) {
        if (!years.contains(b.year()))
            years.append(b.year());
    }
    std::sort(years.begin(), years.end(), std::greater<int>());

    // ── Übersicht-Tab (index 0) ───────────────────────────────────────────
    {
        auto* container = new QWidget;
        auto* cl        = new QVBoxLayout(container);
        cl->setContentsMargins(0, 0, 0, 0);
        cl->setSpacing(2);

        auto* dataTable = new QTableWidget;
        dataTable->setColumnCount(2);
        dataTable->setHorizontalHeaderLabels({ tr("Jahr"), tr("Netto-Kosten") });
        dataTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        dataTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        dataTable->setSelectionMode(QAbstractItemView::SingleSelection);
        dataTable->verticalHeader()->setVisible(false);
        dataTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
        dataTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        dataTable->horizontalHeader()->setFixedHeight(22);
        dataTable->setColumnWidth(0, 80);
        dataTable->setRowCount(years.size());
        dataTable->setFrameShape(QFrame::NoFrame);
        dataTable->setAlternatingRowColors(true);

        double totalNetto = 0.0;
        for (int i = 0; i < years.size(); ++i) {
            const int yr = years[i];
            double netto = 0.0;
            for (const auto& b : brokerages)
                if (b.year() == yr) netto += b.brokerageReduction();
            totalNetto += netto;

            auto* yearItem  = new QTableWidgetItem(QString::number(yr));
            yearItem->setData(Qt::UserRole, yr);
            yearItem->setTextAlignment(Qt::AlignCenter);
            auto* nettoItem = new QTableWidgetItem(
                QLocale().toString(netto, 'f', 2) + QStringLiteral(" €"));
            nettoItem->setTextAlignment(Qt::AlignCenter);
            dataTable->setItem(i, 0, yearItem);
            dataTable->setItem(i, 1, nettoItem);
        }
        container->setProperty("dataTable",
                               QVariant::fromValue(static_cast<QTableWidget*>(dataTable)));

        auto* footerTable = new QTableWidget(1, 2);
        footerTable->horizontalHeader()->setVisible(false);
        footerTable->verticalHeader()->setVisible(false);
        footerTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        footerTable->setSelectionMode(QAbstractItemView::NoSelection);
        footerTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
        footerTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        footerTable->setColumnWidth(0, 80);
        footerTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        footerTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        footerTable->setFrameShape(QFrame::NoFrame);
        {
            const int rowH = dataTable->rowHeight(0) > 0 ? dataTable->rowHeight(0) : 22;
            footerTable->setFixedHeight(rowH + 2);
        }

        auto makeBold = [](QTableWidgetItem* it) {
            QFont f = it->font(); f.setBold(true); it->setFont(f);
        };
        auto* totLbl = new QTableWidgetItem(tr("Gesamt"));
        totLbl->setTextAlignment(Qt::AlignCenter); makeBold(totLbl);
        auto* totVal = new QTableWidgetItem(
            QLocale().toString(totalNetto, 'f', 2) + QStringLiteral(" €"));
        totVal->setTextAlignment(Qt::AlignCenter); makeBold(totVal);
        footerTable->setItem(0, 0, totLbl);
        footerTable->setItem(0, 1, totVal);

        connect(dataTable->horizontalHeader(), &QHeaderView::sectionResized,
                footerTable->horizontalHeader(),
                [footerTable](int idx, int, int newSize) {
                    footerTable->setColumnWidth(idx, newSize);
                });

        auto* sep = new QFrame;
        sep->setFrameShape(QFrame::HLine);
        sep->setFrameShadow(QFrame::Sunken);

        cl->addWidget(dataTable);
        cl->addWidget(sep);
        cl->addWidget(footerTable);

        const QString tabTitle = tr("Übersicht (%1 €)")
            .arg(QLocale().toString(totalNetto, 'f', 2));
        m_tabs->addTab(container, tabTitle);

        connect(dataTable, &QTableWidget::cellClicked,
                this, &ViewBrokerageEdit::onUebersichtRowActivated);
    }

    // ── Jahres-Tabs (newest first) ────────────────────────────────────────
    for (const int yr : years) {
        QList<BrokerageObject> yearBrokerages;
        for (const auto& b : brokerages)
            if (b.year() == yr) yearBrokerages.append(b);

        auto* container = new QWidget;
        auto* cl        = new QVBoxLayout(container);
        cl->setContentsMargins(0, 0, 0, 0);
        cl->setSpacing(2);

        const int kColCount = 6;
        auto* dataTable = new QTableWidget;
        dataTable->setColumnCount(kColCount);
        dataTable->setHorizontalHeaderLabels({
            tr("Datum"), tr("Typ"),
            tr("Ges. Gebühren"), tr("Rabatt"), tr("Netto-Kosten"), tr("Dok.")
        });
        dataTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        dataTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        dataTable->setSelectionMode(QAbstractItemView::SingleSelection);
        dataTable->verticalHeader()->setVisible(false);
        dataTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
        dataTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        dataTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        dataTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
        dataTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
        dataTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
        dataTable->horizontalHeader()->setFixedHeight(22);
        dataTable->setColumnWidth(0, 100);
        dataTable->setColumnWidth(5, 36);
        dataTable->setRowCount(yearBrokerages.size());
        dataTable->setFrameShape(QFrame::NoFrame);
        dataTable->setAlternatingRowColors(true);

        container->setProperty("dataTable",
                               QVariant::fromValue(static_cast<QTableWidget*>(dataTable)));

        double sumGesGeb = 0.0, sumRabatt = 0.0, sumNetto = 0.0;

        for (int i = 0; i < yearBrokerages.size(); ++i) {
            const BrokerageObject& b = yearBrokerages[i];

            const QString typ = !b.buyGuid().isEmpty()   ? tr("Kauf")
                              : !b.saleGuid().isEmpty()  ? tr("Verkauf")
                              :                             tr("Sonstig");

            auto setCell = [&](int col, const QString& txt) {
                auto* it = new QTableWidgetItem(txt);
                it->setTextAlignment(Qt::AlignCenter);
                it->setFlags(it->flags() & ~Qt::ItemIsEditable);
                dataTable->setItem(i, col, it);
            };

            auto* dateItem = new QTableWidgetItem(b.dateAsStr());
            dateItem->setData(Qt::UserRole, b.guid());
            dateItem->setTextAlignment(Qt::AlignCenter);
            dateItem->setFlags(dateItem->flags() & ~Qt::ItemIsEditable);
            dataTable->setItem(i, 0, dateItem);

            setCell(1, typ);
            setCell(2, QLocale().toString(b.brokerage(), 'f', 2));
            setCell(3, QLocale().toString(b.reduction(), 'f', 2));
            setCell(4, QLocale().toString(b.brokerageReduction(), 'f', 2));

            const QString doc = b.document();
            if (!doc.isEmpty())
                dataTable->setCellWidget(i, 5, makeDocIconWidget(doc));
            else
                setCell(5, QStringLiteral("-"));

            sumGesGeb += b.brokerage();
            sumRabatt += b.reduction();
            sumNetto  += b.brokerageReduction();
        }

        auto* footerTable = new QTableWidget(1, kColCount);
        footerTable->horizontalHeader()->setVisible(false);
        footerTable->verticalHeader()->setVisible(false);
        footerTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        footerTable->setSelectionMode(QAbstractItemView::NoSelection);
        footerTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
        footerTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        footerTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        footerTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
        footerTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
        footerTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
        footerTable->setColumnWidth(0, 100);
        footerTable->setColumnWidth(5, 36);
        {
            const int rowH = dataTable->rowHeight(0) > 0 ? dataTable->rowHeight(0) : 22;
            footerTable->setFixedHeight(rowH + 2);
        }
        footerTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        footerTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        footerTable->setFrameShape(QFrame::NoFrame);

        auto makeBold = [](QTableWidgetItem* it) {
            QFont f = it->font(); f.setBold(true); it->setFont(f);
        };
        auto setFooter = [&](int col, const QString& txt) {
            auto* it = new QTableWidgetItem(txt);
            it->setTextAlignment(Qt::AlignCenter);
            makeBold(it);
            footerTable->setItem(0, col, it);
        };
        setFooter(0, tr("Gesamt"));
        setFooter(1, QStringLiteral("-"));
        setFooter(2, QLocale().toString(sumGesGeb, 'f', 2));
        setFooter(3, QLocale().toString(sumRabatt, 'f', 2));
        setFooter(4, QLocale().toString(sumNetto,  'f', 2));
        setFooter(5, QStringLiteral(""));

        connect(dataTable->horizontalHeader(), &QHeaderView::sectionResized,
                footerTable->horizontalHeader(),
                [footerTable](int idx, int, int newSize) {
                    footerTable->setColumnWidth(idx, newSize);
                });
        QTimer::singleShot(0, footerTable, [dataTable, footerTable]() {
            for (int c = 0; c < dataTable->columnCount(); ++c)
                footerTable->setColumnWidth(c, dataTable->columnWidth(c));
        });

        auto* sep = new QFrame;
        sep->setFrameShape(QFrame::HLine);
        sep->setFrameShadow(QFrame::Sunken);

        cl->addWidget(dataTable);
        cl->addWidget(sep);
        cl->addWidget(footerTable);

        const QString tabTitle = QStringLiteral("%1 (%2 €)")
            .arg(yr).arg(QLocale().toString(sumNetto, 'f', 2));
        m_tabs->addTab(container, tabTitle);

        connect(dataTable, &QTableWidget::cellClicked,
                this, &ViewBrokerageEdit::onOverviewRowActivated);
    }

    m_tabs->setCurrentIndex(0);
    m_suppressTabSignal = false;
}

// ── showOverviewTab ───────────────────────────────────────────────────────────

void ViewBrokerageEdit::showOverviewTab()
{
    m_suppressTabSignal = true;
    if (m_tabs->count() > 0)
        m_tabs->setCurrentIndex(0);
    m_suppressTabSignal = false;
    clearForm();
}

// ── setButtonStates ───────────────────────────────────────────────────────────

void ViewBrokerageEdit::setButtonStates(bool canRemove, bool isEdit, bool readOnly)
{
    m_btnRemove->setEnabled(canRemove);
    m_btnAdd->setEnabled(!readOnly);

    if (isEdit) {
        m_btnAdd->setText(tr("Speichern"));
        m_btnAdd->setIcon(IconProvider::icon(IconProvider::ButtonSave));
    } else {
        m_btnAdd->setText(tr("Hinzufügen"));
        m_btnAdd->setIcon(IconProvider::icon(IconProvider::ButtonAdd));
    }

    m_kostendatenGroup->setTitle(isEdit ? tr("  Kosten editieren")
                                        : tr("  Kosten hinzufügen"));

    for (QLineEdit* le : { m_provision, m_brokerFee, m_traderFee, m_reduction })
        le->setEnabled(!readOnly);
    m_date->setEnabled(!readOnly);
    m_time->setEnabled(!readOnly);
    m_btnBrowse->setEnabled(!readOnly);
}

// ── showError / acceptAndClose ────────────────────────────────────────────────

void ViewBrokerageEdit::showError(const QString& message)
{
    OwnMessageBox::critical(this, tr("Fehler"), message);
}

void ViewBrokerageEdit::acceptAndClose()
{
    accept();
}

// ── markMissingFieldsAsFailed / hasMissingRequiredFields ─────────────────────

void ViewBrokerageEdit::markMissingFieldsAsFailed()
{
    // Only the date can be missing (fee fields default to 0 which is valid).
}

bool ViewBrokerageEdit::hasMissingRequiredFields(QStringList& missingFields) const
{
    missingFields.clear();
    if (m_date->date() <= QDate(2000, 1, 1))
        missingFields << QStringLiteral("date");
    return !missingFields.isEmpty();
}

// ── Private slots ─────────────────────────────────────────────────────────────

void ViewBrokerageEdit::onBrowseDocument()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Dokument auswählen"),
        QDir::homePath(),
        tr("Alle Dateien (*);;PDF-Dateien (*.pdf);;Word-Dokumente (*.doc *.docx)"));
    if (!path.isEmpty())
        m_presenter->onDocumentSelected(path);
}

void ViewBrokerageEdit::onOverviewRowActivated(int row, int /*column*/)
{
    auto* tbl = qobject_cast<QTableWidget*>(sender());
    if (!tbl) return;
    const QString guid = tbl->item(row, 0)
                         ? tbl->item(row, 0)->data(Qt::UserRole).toString()
                         : QString();
    if (!guid.isEmpty())
        m_presenter->onRowSelected(guid);
}

void ViewBrokerageEdit::onUebersichtRowActivated(int row, int /*column*/)
{
    auto* tbl = qobject_cast<QTableWidget*>(sender());
    if (!tbl || !tbl->item(row, 0)) return;
    const int yr = tbl->item(row, 0)->data(Qt::UserRole).toInt();

    for (int t = 1; t < m_tabs->count(); ++t) {
        if (m_tabs->tabText(t).startsWith(QString::number(yr))) {
            m_suppressTabSignal = true;
            m_tabs->setCurrentIndex(t);
            m_suppressTabSignal = false;

            auto* container = m_tabs->widget(t);
            auto* yearTbl   = container
                              ? container->property("dataTable").value<QTableWidget*>()
                              : nullptr;
            if (yearTbl && yearTbl->rowCount() > 0) {
                yearTbl->selectRow(0);
                const QString guid = yearTbl->item(0, 0)
                                     ? yearTbl->item(0, 0)->data(Qt::UserRole).toString()
                                     : QString();
                if (!guid.isEmpty())
                    m_presenter->onRowSelected(guid);
            }
            break;
        }
    }
}

// ── Static helpers ────────────────────────────────────────────────────────────

QString ViewBrokerageEdit::formatMoney(double value)
{
    return QLocale().toString(value, 'f', 2);
}

double ViewBrokerageEdit::parseDouble(const QString& text)
{
    QString s = text.trimmed();
    s.replace(QLatin1Char(','), QLatin1Char('.'));
    bool ok = false;
    const double v = s.toDouble(&ok);
    return ok ? v : 0.0;
}

QWidget* ViewBrokerageEdit::makeDocIconWidget(const QString& path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    IconProvider::IconName iconName;
    if (ext == QStringLiteral("pdf")) {
        iconName = IconProvider::DocPdfImage16;
    } else if (ext == QStringLiteral("doc") || ext == QStringLiteral("docx")) {
        iconName = IconProvider::DocWordImage16;
    } else if (ext == QStringLiteral("xls") || ext == QStringLiteral("xlsx")) {
        iconName = IconProvider::DocExcelImage16;
    } else {
        iconName = IconProvider::SearchFailed2;
    }

    auto* lbl = new QLabel;
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setPixmap(IconProvider::icon(iconName).pixmap(16, 16));
    lbl->setToolTip(path);
    return lbl;
}

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ViewShareAdd.h"
#include "PresenterShareAdd.h"
#include "ModelShareAdd.h"
#include "../../IconProvider.h"
#include "../../config/AppSettings.h"
#include "../../core/DocumentRootMigrator.h"
#include "../UiConstants.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include "../OwnMessageBoxForm/OwnMessageBox.h"
#include <QSizePolicy>
#include <QLocale>
#include <QApplication>
#include <QFileInfo>

// ─────────────────────────────────────────────────────────────────────────────
ViewShareAdd::ViewShareAdd(DocumentsConfig* config, QWidget* parent)
    : QDialog(parent)
    , m_config(config)
{
    setWindowTitle(tr("Aktie hinzufügen"));
    setMinimumWidth(1100);
    resize(1400, 946);

    auto* model = new ModelShareAdd();
    m_presenter = new PresenterShareAdd(this, model, config, this);

    setupUi();

    connect(m_btnSave,   &QPushButton::clicked, m_presenter, &PresenterShareAdd::onSave);
    connect(m_btnCancel, &QPushButton::clicked, m_presenter, &PresenterShareAdd::onCancel);
    connect(m_btnCancel, &QPushButton::clicked, this,         &QDialog::reject);
}

// ─────────────────────────────────────────────────────────────────────────────
void ViewShareAdd::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(4);

    auto* contentRow = new QHBoxLayout;
    contentRow->setSpacing(8);
    m_formPanel = createFormPanel();
    contentRow->addWidget(m_formPanel,          0);
    contentRow->addWidget(createPreviewPanel(), 1);

    mainLayout->addLayout(contentRow, 1);
    mainLayout->addWidget(createButtonBar());
}

// ─────────────────────────────────────────────────────────────────────────────
QWidget* ViewShareAdd::createFormPanel()
{
    auto* panel  = new QWidget;
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    layout->addWidget(createGeneralGroup());
    layout->addWidget(createDataSourcesGroup());
    layout->addWidget(createBuyDataGroup());
    layout->addWidget(createDocumentGroup());

    panel->setFixedWidth(700);

    // Wrap in a scroll area so all GroupBoxes are fully visible regardless
    // of dialog height — nothing gets clipped anymore.
    auto* scroll = new QScrollArea;
    scroll->setWidget(panel);
    scroll->setWidgetResizable(true);
    scroll->setFixedWidth(700);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    return scroll;
}

// ─────────────────────────────────────────────────────────────────────────────
QGroupBox* ViewShareAdd::createGeneralGroup()
{
    auto* gb   = new QGroupBox(tr("  Allgemein"));
    gb->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    auto* grid = new QGridLayout(gb);
    grid->setColumnStretch(1, 1);
    grid->setVerticalSpacing(4);
    grid->setHorizontalSpacing(8);
    grid->setContentsMargins(8, 8, 8, 10);
    int row = 0;

    m_wkn = new QLineEdit; m_wkn->setMaxLength(6);
    m_wkn->setPlaceholderText(tr("z.B. 840400"));
    m_statusLabels["wkn"] = addRow(grid, row, tr("WKN:"), m_wkn);
    m_inputWidgets["wkn"] = m_wkn;

    m_isin = new QLineEdit; m_isin->setMaxLength(12);
    m_isin->setPlaceholderText(tr("z.B. DE0008404005"));
    m_statusLabels["isin"] = addRow(grid, row, tr("ISIN:"), m_isin);
    m_inputWidgets["isin"] = m_isin;

    m_name = new QLineEdit;
    m_name->setPlaceholderText(tr("z.B. Allianz SE"));
    m_statusLabels["name"] = addRow(grid, row, tr("Name:"), m_name);
    m_inputWidgets["name"] = m_name;

    // Default = tomorrow — a future date is impossible as a listing date and
    // serves as a clear sentinel that the user has not entered a value yet.
    m_listingDate = new QDateEdit(QDate::currentDate().addDays(1));
    m_listingDate->setCalendarPopup(true);
    m_listingDate->setDisplayFormat(tr("d . M . yyyy"));
    m_listingDate->setMinimumDate(QDate(1, 1, 1));
    m_listingDate->setMaximumDate(QDate(9999, 12, 31));
    m_listingDate->setFixedHeight(UiConstants::kFieldHeight);
    m_statusLabels["listingDate"] = addRow(grid, row, tr("Börsennotierung:"), m_listingDate);
    m_inputWidgets["listingDate"] = m_listingDate;

    m_shareType = new QComboBox;
    m_shareType->addItem(tr("Aktie"),  static_cast<int>(ShareType::Share));
    m_shareType->addItem(tr("Fond"),   static_cast<int>(ShareType::Fond));
    m_shareType->addItem(tr("ETF"),    static_cast<int>(ShareType::Etf));
    m_statusLabels["shareType"] = addRow(grid, row, tr("Typ:"), m_shareType);

    m_divInterval = new QComboBox;
    m_divInterval->addItems({ tr("keine"), tr("jährlich"), tr("halbjährlich"),
                               tr("vierteljährlich"), tr("monatlich") });
    m_statusLabels["divInterval"] = addRow(grid, row, tr("Dividendenausschüttungs-Intervall:"), m_divInterval);

    m_countryInfo = new QComboBox;
    m_countryInfo->addItems({ "de-DE","en-US","en-GB","fr-FR","ja-JP",
                               "de-AT","de-CH","nl-NL","lu-LU","es-ES","it-IT" });
    m_statusLabels["countryInfo"] = addRow(grid, row, tr("Länder-Info:"), m_countryInfo);

    m_detailsWebsite = new QLineEdit;
    m_detailsWebsite->setPlaceholderText(tr("https://…"));
    m_statusLabels["detailsWebsite"] = addRow(grid, row, tr("Details-Webseite:"), m_detailsWebsite);
    m_inputWidgets["detailsWebsite"] = m_detailsWebsite;

    return gb;
}

// ─────────────────────────────────────────────────────────────────────────────
QGroupBox* ViewShareAdd::createDataSourcesGroup()
{
    auto* gb   = new QGroupBox(tr("  Datenquellen"));
    gb->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    auto* grid = new QGridLayout(gb);
    grid->setColumnStretch(1, 1);
    grid->setVerticalSpacing(4);
    grid->setHorizontalSpacing(8);
    grid->setContentsMargins(8, 8, 8, 10);
    int row = 0;

    m_marketUrl = new QLineEdit; m_marketUrl->setPlaceholderText(tr("https://…"));
    m_marketParsing = new QComboBox;
    m_marketParsing->addItems({ tr("Regex"), tr("ApiYahoo"), tr("ApiOnVista") });
    auto* mRow = new QWidget;
    auto* mRowL = new QHBoxLayout(mRow);
    mRowL->setContentsMargins(0,0,0,0); mRowL->setSpacing(4);
    mRowL->addWidget(m_marketUrl, 1); mRowL->addWidget(m_marketParsing);
    m_statusLabels["marketUrl"] = addRow(grid, row, tr("Markt-Werte-Webseite:"), mRow);
    m_inputWidgets["marketUrl"]  = m_marketUrl;

    m_marketApiKey = new QLineEdit; m_marketApiKey->setPlaceholderText(tr("API-Schlüssel"));
    m_marketApiKey->setEnabled(false);
    auto* mkLabel = new QLabel(tr("API-Schlüssel:"));
    mkLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    mkLabel->setStyleSheet("color: gray;");
    grid->addWidget(mkLabel,        row, 0);
    grid->addWidget(m_marketApiKey, row, 1, 1, 2);
    ++row;
    connect(m_marketParsing, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ViewShareAdd::onMarketParsingTypeChanged);

    m_dailyUrl = new QLineEdit; m_dailyUrl->setPlaceholderText(tr("https://…"));
    m_dailyParsing = new QComboBox;
    m_dailyParsing->addItems({ tr("Regex"), tr("ApiYahoo"), tr("ApiOnVista") });
    auto* dRow = new QWidget;
    auto* dRowL = new QHBoxLayout(dRow);
    dRowL->setContentsMargins(0,0,0,0); dRowL->setSpacing(4);
    dRowL->addWidget(m_dailyUrl, 1); dRowL->addWidget(m_dailyParsing);
    m_statusLabels["dailyUrl"] = addRow(grid, row, tr("Tages-Werte-Webseite:"), dRow);
    m_inputWidgets["dailyUrl"]  = m_dailyUrl;

    m_dailyApiKey = new QLineEdit; m_dailyApiKey->setPlaceholderText(tr("API-Schlüssel"));
    m_dailyApiKey->setEnabled(false);
    auto* dkLabel = new QLabel(tr("API-Schlüssel:"));
    dkLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    dkLabel->setStyleSheet("color: gray;");
    grid->addWidget(dkLabel,       row, 0);
    grid->addWidget(m_dailyApiKey, row, 1, 1, 2);
    ++row;
    connect(m_dailyParsing, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ViewShareAdd::onDailyParsingTypeChanged);

    return gb;
}

// ─────────────────────────────────────────────────────────────────────────────
QGroupBox* ViewShareAdd::createBuyDataGroup()
{
    auto* gb   = new QGroupBox(tr("  Kaufdaten"));
    gb->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    auto* grid = new QGridLayout(gb);
    grid->setColumnStretch(1, 1);
    grid->setVerticalSpacing(4);
    grid->setHorizontalSpacing(8);
    grid->setContentsMargins(8, 8, 8, 10);
    int row = 0;

    m_buyDate = new QDateEdit(QDate::currentDate());
    m_buyDate->setCalendarPopup(true);
    m_buyDate->setDisplayFormat(tr("d . M . yyyy"));
    m_buyDate->setFixedHeight(UiConstants::kFieldHeight);
    m_buyTime = new QTimeEdit(QTime::currentTime());
    m_buyTime->setDisplayFormat(tr("HH:mm:ss"));
    m_buyTime->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_buyTime->setFixedHeight(UiConstants::kFieldHeight);
    auto* dtRow = new QWidget;
    auto* dtL   = new QHBoxLayout(dtRow);
    dtL->setContentsMargins(0,0,0,0); dtL->setSpacing(4);
    dtL->addWidget(m_buyDate); dtL->addWidget(m_buyTime);
    m_statusLabels["date"] = addRow(grid, row, tr("Datum:"), dtRow);
    m_inputWidgets["date"] = m_buyDate;
    m_inputWidgets["time"] = m_buyTime;

    m_depotNumber = new QComboBox;
    m_depotNumber->setEditable(false);  // Only known depot numbers from Documents.xml allowed
    m_depotNumber->addItem(tr("— bitte wählen —"));
    // Populate from Documents.xml BankIdentifierValues
    if (m_config) {
        for (const auto& bank : m_config->entries()) {
            if (!bank.identifier.isEmpty())
                // Show "BankName (DepotNr)" so the user knows which bank it belongs to
                m_depotNumber->addItem(
                    QStringLiteral("%1 (%2)").arg(bank.name, bank.identifier),
                    bank.identifier);  // store raw identifier as item data
        }
    }
    m_statusLabels["depotNumber"] = addRow(grid, row, tr("Depot number:"), m_depotNumber);

    m_orderNumber = new QLineEdit;
    m_statusLabels["orderNumber"] = addRow(grid, row, tr("Ordernummer:"), m_orderNumber);
    m_inputWidgets["orderNumber"]  = m_orderNumber;

    m_volume = new QLineEdit(QStringLiteral("0,0000"));
    m_volume->setAlignment(Qt::AlignRight);
    m_volume->setValidator(new QDoubleValidator(0.0, 1e9, 4, m_volume));
    m_statusLabels["volume"] = addRow(grid, row, tr("Anteile:"), m_volume, tr("Stk."));
    m_inputWidgets["volume"]  = m_volume;
    connect(m_volume, &QLineEdit::textChanged,
            this, &ViewShareAdd::recalcDerivedValues);

    m_price = new QLineEdit(QStringLiteral("0,0000"));
    m_price->setAlignment(Qt::AlignRight);
    m_price->setValidator(new QDoubleValidator(0.0, 1e9, 4, m_price));
    m_statusLabels["price"] = addRow(grid, row, tr("Kurs:"), m_price, tr("€"));
    m_inputWidgets["price"]  = m_price;
    connect(m_price, &QLineEdit::textChanged,
            this, &ViewShareAdd::recalcDerivedValues);

    m_kurswert = new QLineEdit(QStringLiteral("0,00"));
    m_kurswert->setReadOnly(true);
    m_kurswert->setAlignment(Qt::AlignRight);
    m_kurswert->setStyleSheet("background: palette(midlight);");
    addRow(grid, row, tr("Kurswert:"), m_kurswert, tr("€"));

    m_provision = new QLineEdit(QStringLiteral("0,00"));
    m_provision->setAlignment(Qt::AlignRight);
    m_provision->setValidator(new QDoubleValidator(0.0, 1e6, 2, m_provision));
    m_statusLabels["provision"] = addRow(grid, row, tr("Provision:"), m_provision, tr("€"));
    m_inputWidgets["provision"]  = m_provision;
    connect(m_provision, &QLineEdit::textChanged,
            this, &ViewShareAdd::recalcDerivedValues);

    m_brokerFee = new QLineEdit(QStringLiteral("0,00"));
    m_brokerFee->setAlignment(Qt::AlignRight);
    m_brokerFee->setValidator(new QDoubleValidator(0.0, 1e6, 2, m_brokerFee));
    m_statusLabels["brokerFee"] = addRow(grid, row, tr("Courtage:"), m_brokerFee, tr("€"));
    m_inputWidgets["brokerFee"]  = m_brokerFee;
    connect(m_brokerFee, &QLineEdit::textChanged,
            this, &ViewShareAdd::recalcDerivedValues);

    m_traderFee = new QLineEdit(QStringLiteral("0,00"));
    m_traderFee->setAlignment(Qt::AlignRight);
    m_traderFee->setValidator(new QDoubleValidator(0.0, 1e6, 2, m_traderFee));
    m_statusLabels["traderFee"] = addRow(grid, row, tr("Handelsplatzgebühr:"), m_traderFee, tr("€"));
    m_inputWidgets["traderFee"]  = m_traderFee;
    connect(m_traderFee, &QLineEdit::textChanged,
            this, &ViewShareAdd::recalcDerivedValues);

    m_gesGebuehren = new QLineEdit(QStringLiteral("0,00"));
    m_gesGebuehren->setReadOnly(true);
    m_gesGebuehren->setAlignment(Qt::AlignRight);
    m_gesGebuehren->setStyleSheet("background: palette(midlight);");
    addRow(grid, row, tr("Ges. Gebühren:"), m_gesGebuehren, tr("€"));

    m_reduction = new QLineEdit(QStringLiteral("0,00"));
    m_reduction->setAlignment(Qt::AlignRight);
    m_reduction->setValidator(new QDoubleValidator(0.0, 1e6, 2, m_reduction));
    m_statusLabels["reduction"] = addRow(grid, row, tr("Rabatt:"), m_reduction, tr("€"));
    m_inputWidgets["reduction"]  = m_reduction;
    connect(m_reduction, &QLineEdit::textChanged,
            this, &ViewShareAdd::recalcDerivedValues);

    m_endbetrag = new QLineEdit(QStringLiteral("0,00"));
    m_endbetrag->setReadOnly(true);
    m_endbetrag->setAlignment(Qt::AlignRight);
    m_endbetrag->setStyleSheet("background: #d4edda; color: #155724; font-weight: bold;");
    addRow(grid, row, tr("Endbetrag:"), m_endbetrag, tr("€"));

    return gb;
}

// ─────────────────────────────────────────────────────────────────────────────
QGroupBox* ViewShareAdd::createDocumentGroup()
{
    auto* gb   = new QGroupBox(tr("  Dokument"));
    gb->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    auto* grid = new QGridLayout(gb);
    grid->setColumnStretch(1, 1);
    grid->setContentsMargins(8, 8, 8, 10);
    int row = 0;

    m_documentPath = new QLineEdit;
    m_documentPath->setReadOnly(true);
    m_documentPath->setPlaceholderText(tr("Kein Dokument ausgewählt …"));

    // Reine Fallback-Anzeige, kein aktiver Auswahlweg — Icon je nach
    // Dateiendung, analog ViewBrokerageEdit::makeDocIconWidget() (20.07.2026,
    // siehe ARCHITECTURE.md "Offene Punkte / TODO"). Aktuell kann hier nur
    // PDF ausgewählt werden; die Logik bleibt für eine mögliche spätere
    // Erweiterung bewusst erhalten.
    m_docTypeIcon = new QLabel;
    m_docTypeIcon->setObjectName(QStringLiteral("docTypeIcon"));
    m_docTypeIcon->setFixedSize(16, 16);
    m_docTypeIcon->setAlignment(Qt::AlignCenter);

    m_btnBrowse = new QPushButton(IconProvider::icon(IconProvider::MenuFolderOpen16), QString());
    m_btnBrowse->setFixedWidth(36);
    m_btnBrowse->setToolTip(tr("PDF-Dokument auswählen"));
    connect(m_btnBrowse, &QPushButton::clicked, this, &ViewShareAdd::onBrowseDocument);

    auto* docRow = new QWidget;
    auto* docL   = new QHBoxLayout(docRow);
    docL->setContentsMargins(0,0,0,0); docL->setSpacing(4);
    docL->addWidget(m_documentPath, 1); docL->addWidget(m_docTypeIcon); docL->addWidget(m_btnBrowse);

    auto* label = new QLabel(tr("Dokument:"));
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(label,  row, 0);
    grid->addWidget(docRow, row, 1);
    ++row;

    // ── Parse status bar (hidden until a document is parsed) ──────────────
    m_parseProgress = new QProgressBar;
    m_parseProgress->setRange(0, 100);
    m_parseProgress->setValue(0);
    m_parseProgress->setFixedHeight(14);
    m_parseProgress->setFixedWidth(200);
    m_parseProgress->setTextVisible(false);
    m_parseProgress->setVisible(false);

    m_parseStatusIcon = new QLabel;
    m_parseStatusIcon->setFixedSize(18, 18);
    m_parseStatusIcon->setAlignment(Qt::AlignCenter);
    m_parseStatusIcon->setVisible(false);

    m_parseStatus = new QLabel;
    m_parseStatus->setVisible(false);
    m_parseStatus->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* statusRow = new QWidget;
    auto* statusLayout = new QHBoxLayout(statusRow);
    statusLayout->setContentsMargins(0, 2, 0, 0);
    statusLayout->setSpacing(6);
    statusLayout->addWidget(m_parseProgress);
    statusLayout->addWidget(m_parseStatusIcon);
    statusLayout->addWidget(m_parseStatus, 1);

    grid->addWidget(statusRow, row, 0, 1, 4);

    return gb;
}

// ─────────────────────────────────────────────────────────────────────────────
QWidget* ViewShareAdd::createPreviewPanel()
{
    // Auf DocumentPreviewPanel umgestellt (19.07.2026) — vorher eine
    // eigenständige Kopie desselben QPdfView-/pdftoppm-Codes ohne
    // Existenzprüfung der Datei (siehe ARCHITECTURE.md, "Offene Punkte /
    // TODO"). DocumentPreviewPanel bringt bereits ein eigenes GroupBox
    // ("Dokumenten-Vorschau") mit, kein zusätzlicher Wrapper nötig — analog
    // zu ViewBuyEdit/ViewSaleEdit/ViewDividendEdit/ViewBrokerageEdit.
    m_previewPanel = new DocumentPreviewPanel(this);
    return m_previewPanel;
}

// ─────────────────────────────────────────────────────────────────────────────
QWidget* ViewShareAdd::createButtonBar()
{
    auto* bar    = new QWidget;
    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->addStretch(1);

    m_btnSave = new QPushButton(tr("Speichern"));
    m_btnSave->setDefault(true);
    m_btnSave->setIcon(IconProvider::icon(IconProvider::ButtonSave));
    m_btnSave->setMinimumWidth(110);
    m_btnSave->setFixedHeight(UiConstants::kButtonHeight);

    m_btnCancel = new QPushButton(tr("Abbrechen"));
    m_btnCancel->setIcon(IconProvider::icon(IconProvider::ButtonCancel));
    m_btnCancel->setMinimumWidth(110);
    m_btnCancel->setFixedHeight(UiConstants::kButtonHeight);

    layout->addWidget(m_btnSave);
    layout->addWidget(m_btnCancel);
    return bar;
}

// ─────────────────────────────────────────────────────────────────────────────
QLabel* ViewShareAdd::addRow(QGridLayout* grid, int& row,
                              const QString& labelText, QWidget* field,
                              const QString& unitText)
{
    field->setFixedHeight(UiConstants::kFieldHeight);

    auto* label = new QLabel(labelText);
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(label, row, 0);

    if (unitText.isEmpty()) {
        grid->addWidget(field, row, 1, 1, 2);
    } else {
        auto* unit = new QLabel(unitText);
        unit->setFixedWidth(26);
        unit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        grid->addWidget(field, row, 1);
        grid->addWidget(unit,  row, 2);
    }

    auto* status = new QLabel;
    status->setFixedSize(20, 20);
    status->setAlignment(Qt::AlignCenter);
    grid->addWidget(status, row, 3);
    ++row;
    return status;
}

// ─────────────────────────────────────────────────────────────────────────────
// ── Slots ─────────────────────────────────────────────────────────────────────

void ViewShareAdd::onBrowseDocument()
{
    const QString root = AppSettings::instance().documentsRootPath();
    const QString startDir = !root.isEmpty() ? root : QString();

    const QString path = QFileDialog::getOpenFileName(
        this, tr("PDF-Dokument auswählen"), startDir,
        tr("PDF-Dokumente (*.pdf)"));

    if (path.isEmpty())
        return;

    if (!DocumentRootMigrator::isPathWithinRoot(path, root)) {
        OwnMessageBox::critical(this, tr("Fehler"),
            tr("Die gewählte Datei muss innerhalb des Dokument-Root-Verzeichnisses "
               "liegen:\n%1").arg(root));
        return;
    }

    m_documentPath->setText(path);

    // Fallback-Icon je nach Dateiendung — siehe createDocumentGroup().
    const QString ext = QFileInfo(path).suffix().toLower();
    IconProvider::IconName iconName;
    if (ext == QStringLiteral("pdf"))
        iconName = IconProvider::DocPdfImage16;
    else if (ext == QStringLiteral("doc") || ext == QStringLiteral("docx"))
        iconName = IconProvider::DocWordImage16;
    else if (ext == QStringLiteral("xls") || ext == QStringLiteral("xlsx"))
        iconName = IconProvider::DocExcelImage16;
    else
        iconName = IconProvider::SearchFailed2;
    m_docTypeIcon->setPixmap(IconProvider::icon(iconName).pixmap(16, 16));
    m_docTypeIcon->setToolTip(path);

    m_previewPanel->showDocument(path);
    m_presenter->onDocumentSelected(path);
}

void ViewShareAdd::onMarketParsingTypeChanged(int index)
{
    m_marketApiKey->setEnabled(index != 0);
    if (index == 0) m_marketApiKey->clear();
}

void ViewShareAdd::onDailyParsingTypeChanged(int index)
{
    m_dailyApiKey->setEnabled(index != 0);
    if (index == 0) m_dailyApiKey->clear();
}

void ViewShareAdd::recalcDerivedValues()
{
    auto parse = [](const QString& t) -> double {
        QString s = t.trimmed(); s.replace(QLatin1Char(','), QLatin1Char('.'));
        bool ok = false; double v = s.toDouble(&ok); return ok ? v : 0.0;
    };
    const double kw  = parse(m_volume->text()) * parse(m_price->text());
    const double ges = parse(m_provision->text()) + parse(m_brokerFee->text())
                       + parse(m_traderFee->text());
    const double end = kw + ges - parse(m_reduction->text());
    const QLocale loc;
    m_kurswert->setText(loc.toString(kw,  'f', 2));
    m_gesGebuehren->setText(loc.toString(ges, 'f', 2));
    m_endbetrag->setText(loc.toString(end, 'f', 2));
}

// ─────────────────────────────────────────────────────────────────────────────
// ── IViewShareAdd read accessors ──────────────────────────────────────────────

QString  ViewShareAdd::wkn()              const { return m_wkn->text(); }
QString  ViewShareAdd::isin()             const { return m_isin->text(); }
QString  ViewShareAdd::name()             const { return m_name->text(); }
QDate    ViewShareAdd::listingDate()      const { return m_listingDate->date(); }
QString  ViewShareAdd::detailsWebsite()   const { return m_detailsWebsite->text(); }
QString  ViewShareAdd::marketPriceUrl()   const { return m_marketUrl->text(); }
QString  ViewShareAdd::marketPriceApiKey()const { return m_marketApiKey->text(); }
QString  ViewShareAdd::dailyValuesUrl()   const { return m_dailyUrl->text(); }
QString  ViewShareAdd::dailyValuesApiKey()const { return m_dailyApiKey->text(); }
QString  ViewShareAdd::orderNumber()      const { return m_orderNumber->text(); }
QString  ViewShareAdd::documentPath()     const { return m_documentPath->text(); }
QString  ViewShareAdd::dividendInterval() const { return m_divInterval->currentText(); }
QString  ViewShareAdd::countryInfo()      const { return m_countryInfo->currentText(); }

static double saParseDouble(const QString& text)
{
    QString s = text.trimmed();
    s.replace(QLatin1Char(','), QLatin1Char('.'));
    bool ok = false;
    const double v = s.toDouble(&ok);
    return ok ? v : 0.0;
}

double   ViewShareAdd::volume()           const { return saParseDouble(m_volume->text());    }
double   ViewShareAdd::price()            const { return saParseDouble(m_price->text());     }
double   ViewShareAdd::provision()        const { return saParseDouble(m_provision->text()); }
double   ViewShareAdd::brokerFee()        const { return saParseDouble(m_brokerFee->text()); }
double   ViewShareAdd::traderFee()        const { return saParseDouble(m_traderFee->text()); }
double   ViewShareAdd::reduction()        const { return saParseDouble(m_reduction->text()); }
QString ViewShareAdd::depotNumber() const
{
    // Return the raw BankIdentifierValue stored as item data, not the display text
    const QVariant data = m_depotNumber->currentData();
    return data.isValid() ? data.toString() : QString();
}

ShareType ViewShareAdd::shareType() const
{
    return static_cast<ShareType>(m_shareType->currentData().toInt());
}

ShareParsingType ViewShareAdd::marketPriceParsingType() const
{
    switch (m_marketParsing->currentIndex()) {
    case 1:  return ShareParsingType::ApiYahoo;
    case 2:  return ShareParsingType::ApiOnVista;
    default: return ShareParsingType::Regex;
    }
}

ShareParsingType ViewShareAdd::dailyValuesParsingType() const
{
    switch (m_dailyParsing->currentIndex()) {
    case 1:  return ShareParsingType::ApiYahoo;
    case 2:  return ShareParsingType::ApiOnVista;
    default: return ShareParsingType::Regex;
    }
}

QDateTime ViewShareAdd::buyDateTime() const
{
    return QDateTime(m_buyDate->date(), m_buyTime->time());
}

// ─────────────────────────────────────────────────────────────────────────────
// ── IViewShareAdd write methods ───────────────────────────────────────────────

void ViewShareAdd::setFieldOk(const QString& field, const QString& value)
{
    if (auto* le = qobject_cast<QLineEdit*>(m_inputWidgets.value(field))) {
        // Remove any newlines/carriage-returns that pdftotext may introduce,
        // then collapse multiple spaces into one.
        QString clean = value;
        clean.replace(QLatin1Char('\n'), QLatin1Char(' '));
        clean.replace(QLatin1Char('\r'), QLatin1Char(' '));
        clean = clean.simplified();   // collapses whitespace + trims
        le->setText(clean);
    } else if (auto* de = qobject_cast<QDateEdit*>(m_inputWidgets.value(field))) {
        QDate d = QDate::fromString(value, QStringLiteral("d.M.yyyy"));
        if (!d.isValid()) d = QDate::fromString(value, Qt::ISODate);
        if (d.isValid()) de->setDate(d);
    } else if (auto* te = qobject_cast<QTimeEdit*>(m_inputWidgets.value(field))) {
        QTime t = QTime::fromString(value, QStringLiteral("h:m:s"));
        if (!t.isValid()) t = QTime::fromString(value, QStringLiteral("h:m"));
        if (t.isValid()) te->setTime(t);
    } else if (auto* sp = qobject_cast<QLineEdit*>(m_inputWidgets.value(field))) {
        // Numeric QLineEdit (volume, price, provision, brokerFee, traderFee, reduction)
        QString norm = value; norm.replace(QLatin1Char('.'), QLatin1Char(','));
        sp->setText(norm.trimmed());
        recalcDerivedValues();
    } else if (field == QStringLiteral("depotNumber")) {
        // Select the matching entry by its stored BankIdentifierValue (item data)
        for (int i = 0; i < m_depotNumber->count(); ++i) {
            if (m_depotNumber->itemData(i).toString() == value.trimmed()) {
                m_depotNumber->setCurrentIndex(i);
                break;
            }
        }
        // If not found: leave on placeholder — user must select manually
    }

    m_fieldStates[field] = FieldState::Ok;
    if (auto* lbl = m_statusLabels.value(field)) {
        lbl->setPixmap(IconProvider::icon(IconProvider::SearchOk).pixmap(16, 16));
        lbl->setToolTip(tr("Aus PDF übernommen"));
    }
}

void ViewShareAdd::setFieldError(const QString& field)
{
    m_fieldStates[field] = FieldState::Error;
    if (auto* lbl = m_statusLabels.value(field)) {
        lbl->setPixmap(IconProvider::icon(IconProvider::SearchFailed).pixmap(16, 16));
        lbl->setToolTip(tr("Nicht im PDF gefunden — bitte manuell eingeben"));
    }
}

void ViewShareAdd::setParseProgress(int percent, const QString& status)
{
    m_parseProgress->setVisible(true);
    m_parseStatusIcon->setVisible(true);
    m_parseStatus->setVisible(true);
    m_parseProgress->setValue(percent);
    m_parseStatus->setText(status);

    // During progress: SearchInfo; set to Ok/Failed/Warning by presenter at end
    if (percent < 100)
        m_parseStatusIcon->setPixmap(
            IconProvider::icon(IconProvider::SearchInfo).pixmap(16, 16));
}

void ViewShareAdd::setParseStatusIcon(int iconType)
{
    if (!m_parseStatusIcon) return;
    IconProvider::IconName name;
    switch (iconType) {
    case 0:  name = IconProvider::SearchOk;     break;
    case 1:  name = IconProvider::SearchFailed; break;
    default: name = IconProvider::SearchInfo;   break;
    }
    m_parseStatusIcon->setPixmap(IconProvider::icon(name).pixmap(16, 16));
    m_parseStatusIcon->setVisible(true);
}

void ViewShareAdd::setUiBusy(bool busy)
{
    if (m_formPanel)
        m_formPanel->setDisabled(busy);

    m_btnSave->setDisabled(busy);
    m_btnCancel->setDisabled(busy);

    if (busy) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        m_parseProgress->setVisible(true);
        m_parseStatusIcon->setVisible(true);
        m_parseStatus->setVisible(true);
        m_parseStatusIcon->setPixmap(
            IconProvider::icon(IconProvider::SearchInfo).pixmap(16, 16));
    } else {
        QApplication::restoreOverrideCursor();
        m_parseProgress->setValue(100);
        // Re-apply API key state (may have been wrongly re-enabled by parent enable)
        m_marketApiKey->setDisabled(m_marketParsing->currentIndex() == 0);
        m_dailyApiKey->setDisabled(m_dailyParsing->currentIndex() == 0);
        if (m_formPanel) m_formPanel->update();
        update();
    }
}

void ViewShareAdd::setDocumentPreview(const QString& /*text*/)
{
    // Plain text is only used internally by the parser.
    // Visual preview is handled by m_previewPanel->showDocument() via onBrowseDocument().
}

void ViewShareAdd::showError(const QString& message)
{
    OwnMessageBox::critical(this, tr("Fehler"), message);
}

void ViewShareAdd::markMissingFieldsAsFailed()
{
    // Evaluate every required field based on current widget content — not on
    // the stored FieldState. This ensures that fields the user filled manually
    // after parsing show Ok, and fields cleared again show Failed.

    // Helper: is a QLineEdit non-empty?
    auto lineOk = [](QLineEdit* le) {
        return le && !le->text().trimmed().isEmpty();
    };

    // Evaluate each required field against its widget value
    struct FieldCheck {
        QString   key;
        bool      ok;
    };

    const QList<FieldCheck> checks = {
        { QStringLiteral("wkn"),          lineOk(m_wkn)                                      },
        { QStringLiteral("isin"),         lineOk(m_isin)                                     },
        { QStringLiteral("name"),         lineOk(m_name)                                     },
        { QStringLiteral("listingDate"),  m_listingDate->date() < QDate::currentDate()       },
        { QStringLiteral("detailsWebsite"), lineOk(m_detailsWebsite)                         },
        { QStringLiteral("marketUrl"),    lineOk(m_marketUrl)                                },
        { QStringLiteral("dailyUrl"),     lineOk(m_dailyUrl)                                 },
        { QStringLiteral("date"),         m_buyDate->date().isValid()                        },
        { QStringLiteral("depotNumber"),  !m_depotNumber->currentData().toString().isEmpty() },
        { QStringLiteral("orderNumber"),  lineOk(m_orderNumber)                              },
        { QStringLiteral("volume"),       saParseDouble(m_volume->text()) > 0.0              },
        { QStringLiteral("price"),        saParseDouble(m_price->text())  > 0.0              },
    };

    for (const auto& check : checks) {
        auto* lbl = m_statusLabels.value(check.key);
        if (!lbl) continue;

        if (check.ok) {
            // Field is filled — show Ok only if it was previously marked
            // (i.e. has a status label). Don't override Ok from the parser.
            if (m_fieldStates.value(check.key, FieldState::Untouched) != FieldState::Ok) {
                m_fieldStates[check.key] = FieldState::Ok;
                lbl->setPixmap(
                    IconProvider::icon(IconProvider::SearchOk).pixmap(16, 16));
                lbl->setToolTip(tr("Feld ausgefüllt"));
            }
        } else {
            // Field is empty/invalid — mark as failed
            m_fieldStates[check.key] = FieldState::Error;
            lbl->setPixmap(
                IconProvider::icon(IconProvider::SearchFailed).pixmap(16, 16));
            lbl->setToolTip(tr("Pflichtfeld — bitte ausfüllen"));
        }
    }
}

bool ViewShareAdd::hasMissingRequiredFields(QStringList& missingFields) const
{
    // Use the same widget-content checks as markMissingFieldsAsFailed()
    // so both methods are always consistent with each other.
    missingFields.clear();

    auto lineEmpty = [](QLineEdit* le) {
        return !le || le->text().trimmed().isEmpty();
    };

    if (lineEmpty(m_wkn))            missingFields.append(tr("WKN"));
    if (lineEmpty(m_isin))           missingFields.append(tr("ISIN"));
    if (lineEmpty(m_name))           missingFields.append(tr("Name"));
    if (m_listingDate->date() >= QDate::currentDate())
                                     missingFields.append(tr("Börsennotierung"));
    if (lineEmpty(m_detailsWebsite)) missingFields.append(tr("Details-Webseite"));
    if (lineEmpty(m_marketUrl))      missingFields.append(tr("Markt-Werte-Webseite"));
    if (lineEmpty(m_dailyUrl))       missingFields.append(tr("Tages-Werte-Webseite"));
    if (!m_buyDate->date().isValid())missingFields.append(tr("Datum"));
    if (m_depotNumber->currentData().toString().isEmpty())
                                     missingFields.append(tr("Depotnummer"));
    if (lineEmpty(m_orderNumber))    missingFields.append(tr("Ordernummer"));
    if (saParseDouble(m_volume->text()) <= 0.0) missingFields.append(tr("Anteile"));
    if (saParseDouble(m_price->text())  <= 0.0) missingFields.append(tr("Kurs"));

    return !missingFields.isEmpty();
}

void ViewShareAdd::onParseFinished()
{
    // Only required fields that were not touched by the parser get SearchInfo.
    // Optional fields (provision, brokerFee, traderFee, reduction, document)
    // never get a status icon — they stay empty by default.
    static const QStringList requiredFieldKeys = {
        QStringLiteral("wkn"),
        QStringLiteral("isin"),
        QStringLiteral("name"),
        QStringLiteral("listingDate"),
        QStringLiteral("detailsWebsite"),
        QStringLiteral("marketUrl"),
        QStringLiteral("dailyUrl"),
        QStringLiteral("date"),
        QStringLiteral("depotNumber"),
        QStringLiteral("orderNumber"),
        QStringLiteral("volume"),
        QStringLiteral("price"),
    };

    for (const QString& field : requiredFieldKeys) {
        bool shouldShowInfo = false;

        if (field == QStringLiteral("listingDate")) {
            // Show info icon if still on the future-date sentinel
            shouldShowInfo = (m_listingDate->date() >= QDate::currentDate());
        } else {
            shouldShowInfo = (m_fieldStates.value(field, FieldState::Untouched)
                              == FieldState::Untouched);
        }

        if (shouldShowInfo) {
            m_fieldStates[field] = FieldState::Info;
            if (auto* lbl = m_statusLabels.value(field)) {
                lbl->setPixmap(
                    IconProvider::icon(IconProvider::SearchInfo).pixmap(16, 16));
                lbl->setToolTip(tr("Wert fehlt noch — bitte manuell eingeben"));
            }
        }
    }
}

void ViewShareAdd::acceptAndClose()
{
    QDialog::accept();
}

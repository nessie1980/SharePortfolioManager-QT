// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ViewShareDetails.h"
#include "../../widgets/OverviewTabFiller.h"
#include "../../utils/ShareSplitAdjuster.h"
#include "../../utils/ShareSplitHint.h"
#include "../../utils/ValueFormatter.h"

#include "../OwnMessageBoxForm/OwnMessageBox.h"
#include "../../IconProvider.h"

#include <QHBoxLayout>
#include <QFrame>
#include <QDialogButtonBox>
#include <QFont>
#include <QPalette>
#include <QLocale>
#include <QFileInfo>
#include <algorithm>

namespace {

// kDocPreviewPanelWidth = 480 ist kein Schätzwert, sondern 1:1 aus den
// Editier-Dialogen übernommen: dort ist der Dialog fest 1200px breit
// (`setFixedSize(1200, 760)`) und im Verhältnis 3:2 zwischen Formular und
// Vorschau-Panel aufgeteilt (`main->addWidget(m_leftPanel, 3); main->
// addWidget(createPreviewPanel(), 2);`) — 1200 × 2/5 = 480.
constexpr int kDocPreviewPanelWidth = 480;

/**
 * @brief Baut eine QGroupBox um ein OverviewTabWidget, identisch zu
 * ViewDividendEdit::createOverviewGroup() (Titel mit zwei führenden
 * Leerzeichen — Projektkonvention, siehe z.B. "  Dokumenten-Vorschau").
 */
QGroupBox* wrapInOverviewGroup(const QString& title, OverviewTabWidget* tabs)
{
    auto* gb = new QGroupBox(title);
    auto* layout = new QVBoxLayout(gb);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->addWidget(tabs);
    return gb;
}

} // namespace

// ── Constructor ───────────────────────────────────────────────────────────────

ViewShareDetails::ViewShareDetails(const QString& shareGuid, bool marketValueMode, QWidget* parent)
    : QDialog(parent)
    , m_presenter(*this, m_model, shareGuid, marketValueMode)
    , m_marketValueMode(marketValueMode)
    , m_shareGuid(shareGuid)
{
    setObjectName(QStringLiteral("ViewShareDetails"));
    // Nochmals verbreitert (12.07.2026, zweite Anpassung) — Ziel: die langen
    // Legende-Zeilen ("422,40€ - 198,36€ = 224,04€ (112,95 %)") sollen nach
    // Möglichkeit immer einzeilig bleiben, nicht nur meistens.
    setMinimumSize(1150, 600);
    resize(1550, 780);

    setupUi();

    // Loads the share via the model and pushes formatted content into this
    // view through the IViewShareDetails methods below. If the GUID does not
    // resolve to a valid share, showError() + closeDialog() are triggered
    // from inside the presenter and m_validShare stays false.
    m_validShare = m_presenter.loadAndDisplay();
}

// ── setupUi ───────────────────────────────────────────────────────────────────

void ViewShareDetails::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // ── Status line ("Letzte Internet-Aktualisierung: ... / Typ: ...") ─────
    m_statusLine = new QLabel();
    m_statusLine->setObjectName(QStringLiteral("statusLine"));
    mainLayout->addWidget(m_statusLine);

    // ── Tab widget ────────────────────────────────────────────────────────
    m_tabs = new QTabWidget();
    m_tabs->setObjectName(QStringLiteral("tabs"));
    mainLayout->addWidget(m_tabs, 1);

    setupChartTab();
    setupDepotwertTab();

    // Gewinne/Verluste-Tab existiert seit 14.07.2026 in beiden Modi (Nessies
    // Vorgabe) — im Marktwert-Modus zeigt er die brokeragefreien Werte
    // (SaleObject::payout()/profitLoss() statt .../BrokerageReduction(), siehe
    // populateGewinneVerluste() unten). Dividenden- und Kosten-Tab bleiben
    // Depotwert-only: beides sind laut C#-Referenz reine Depotwert-Konzepte
    // (siehe ARCHITECTURE.md, "Marktwert- vs. Depotwert-Modus" — dieselbe
    // Begründung, aus der auch die Dividenden-Zeile in der Marktwert-Box
    // deaktiviert ist, bzw. "OverviewTabWidget-Details").
    setupGewinneVerlusteTab();
    if (!m_marketValueMode) {
        setupDividendenTab();
        setupKostenTab();
    }

    // Beim Wechsel des äußeren Tabs immer auf die Jahresübersicht
    // zurücksetzen (14.07.2026, Nessies Vorgabe) — siehe onMainTabChanged().
    connect(m_tabs, &QTabWidget::currentChanged,
            this, &ViewShareDetails::onMainTabChanged);

    // ── "Aktie sollte aktualisiert werden!"-Warnzeile (ergänzt 30.07.2026) ──
    // Form-weite Statusleiste — bewusst NICHT Teil von ViewChart, siehe
    // ARCHITECTURE.md, "ChartForm-Details": portiert von
    // toolStripStatusLabelUpdate in der C#-Referenz (FrmShareDetails_Shown()),
    // dort ebenfalls eine form-weite Statusleiste unterhalb aller Tabs, nicht
    // an den Chart-Tab gebunden. Gleicher grauer Balken-Look wie
    // m_websiteUpdateLine, aber roter Text; standardmäßig versteckt.
    m_updateWarningLine = new QLabel();
    m_updateWarningLine->setObjectName(QStringLiteral("updateWarningLine"));
    m_updateWarningLine->setContentsMargins(8, 4, 8, 4);
    m_updateWarningLine->setAutoFillBackground(true);
    QPalette warningPalette = m_updateWarningLine->palette();
    warningPalette.setColor(QPalette::Window, palette().color(QPalette::Mid));
    warningPalette.setColor(QPalette::WindowText, Qt::red);
    m_updateWarningLine->setPalette(warningPalette);
    m_updateWarningLine->setVisible(false);
    mainLayout->addWidget(m_updateWarningLine);

    // ── Close button ──────────────────────────────────────────────────────
    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    buttonBox->setObjectName(QStringLiteral("buttonBox"));
    // QDialogButtonBox::Close only auto-translates to "Schließen" if Qt's own
    // qtbase_de.qm is loaded via QTranslator — this project only loads its
    // own spm_de.ts/spm_en.ts, so the standard button stays "Close" in the
    // German UI. Set the text explicitly instead of depending on that.
    buttonBox->button(QDialogButtonBox::Close)->setText(tr("Schließen"));
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

// ── setupChartTab ─────────────────────────────────────────────────────────────

void ViewShareDetails::setupChartTab()
{
    auto* chartView = new ViewChart(m_shareGuid, m_tabs);
    connect(chartView, &ViewChart::titleInfoChanged,
            this, &ViewShareDetails::onChartTitleInfoChanged);

    m_tabs->addTab(chartView, tr("Aktien-Chart"));
}

// ── setupDepotwertTab ─────────────────────────────────────────────────────────

void ViewShareDetails::setupDepotwertTab()
{
    auto* tab = new QWidget();
    tab->setObjectName(QStringLiteral("depotwertTab"));

    auto* tabLayout = new QVBoxLayout(tab);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->setSpacing(0);

    // "Letzte Website- Aktualisierung: ..." bar — grey bar spanning the tab
    // width, matches the C# reference screenshot. Positioned inside this tab
    // (not the outer dialog), since it's specific to the Depotwert-/
    // Marktwert-Ansicht (present in both modes).
    m_websiteUpdateLine = new QLabel();
    m_websiteUpdateLine->setObjectName(QStringLiteral("websiteUpdateLine"));
    m_websiteUpdateLine->setContentsMargins(8, 4, 8, 4);
    m_websiteUpdateLine->setAutoFillBackground(true);
    QPalette barPalette = m_websiteUpdateLine->palette();
    barPalette.setColor(QPalette::Window, palette().color(QPalette::Mid));
    m_websiteUpdateLine->setPalette(barPalette);
    tabLayout->addWidget(m_websiteUpdateLine);

    auto* grid = new QGridLayout();
    grid->setContentsMargins(12, 12, 12, 12);
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(16);
    tabLayout->addLayout(grid, 1);

    QGroupBox* gesamtBox   = createCalculationBox(tr("Gesamt-Bestandsberechnung"), m_gesamtGrid);
    QGroupBox* vortagBox   = createCalculationBox(tr("Vortag-Bestandsberechnung"), m_vortagGrid);
    QGroupBox* aktuelleBox = createCalculationBox(tr("Aktuelle Bestandsberechnung"), m_aktuelleGrid);

    gesamtBox->setObjectName(QStringLiteral("gesamtBox"));
    vortagBox->setObjectName(QStringLiteral("vortagBox"));
    aktuelleBox->setObjectName(QStringLiteral("aktuelleBox"));

    // Layout: Gesamt-Box spans both rows on the left; Vortag top-right,
    // Aktuelle bottom-right — matches the C# reference's screen layout.
    grid->addWidget(gesamtBox,   0, 0, 2, 1);
    grid->addWidget(vortagBox,   0, 1);
    grid->addWidget(aktuelleBox, 1, 1);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setRowStretch(2, 1);

    // Placeholder text — the presenter sets the real title via
    // setBoxesTabTitle() ("Komplette Depotbewertung"/"Komplette Marktbewertung").
    m_boxesTabIndex = m_tabs->addTab(tab, QString());
}

// ── setupGewinneVerlusteTab / setupDividendenTab / setupKostenTab ────────────
//
// Layout je Tab: OverviewTabWidget links (nimmt den verbleibenden Platz ein)
// + DocumentPreviewPanel rechts, fest in der Breite (kDocPreviewPanelWidth).
// Kein Popup (13.07.2026, auf Wunsch durch eingebettetes Panel ersetzt).
// Seit 19.07.2026 aktualisiert wireOverviewTab() das Panel direkt bei
// Zeilenklick UND bei automatischer Erst-Zeilen-Auswahl beim Tab-Wechsel
// (kein Doppelklick mehr nötig, siehe wireOverviewTab()).

void ViewShareDetails::setupGewinneVerlusteTab()
{
    auto* wrapper = new QWidget();
    auto* wrapperLayout = new QHBoxLayout(wrapper);
    wrapperLayout->setContentsMargins(0, 0, 0, 0);

    m_gewinneVerlusteTab = new OverviewTabWidget();
    m_gewinneVerlustePreview = new DocumentPreviewPanel(wrapper);
    m_gewinneVerlustePreview->setFixedWidth(kDocPreviewPanelWidth);
    // jahresDocColumn = 4, siehe populateGewinneVerluste().
    wireOverviewTab(m_gewinneVerlusteTab, m_gewinneVerlustePreview, /*docColumn=*/4);

    wrapperLayout->addWidget(
        wrapInOverviewGroup(tr("  Gewinne / Verluste-Übersicht"), m_gewinneVerlusteTab), 1);
    wrapperLayout->addWidget(m_gewinneVerlustePreview);

    m_tabs->addTab(wrapper, tr("Gewinne / Verluste"));
}

void ViewShareDetails::setupDividendenTab()
{
    auto* wrapper = new QWidget();
    auto* wrapperLayout = new QHBoxLayout(wrapper);
    wrapperLayout->setContentsMargins(0, 0, 0, 0);

    m_dividendenTab = new OverviewTabWidget();
    m_dividendenPreview = new DocumentPreviewPanel(wrapper);
    m_dividendenPreview->setFixedWidth(kDocPreviewPanelWidth);
    // jahresDocColumn = 4, siehe populateDividenden().
    wireOverviewTab(m_dividendenTab, m_dividendenPreview, /*docColumn=*/4);

    wrapperLayout->addWidget(
        wrapInOverviewGroup(tr("  Dividenden-Übersicht"), m_dividendenTab), 1);
    wrapperLayout->addWidget(m_dividendenPreview);

    m_tabs->addTab(wrapper, tr("Dividenden"));
}

void ViewShareDetails::setupKostenTab()
{
    auto* wrapper = new QWidget();
    auto* wrapperLayout = new QHBoxLayout(wrapper);
    wrapperLayout->setContentsMargins(0, 0, 0, 0);

    m_kostenTab = new OverviewTabWidget();
    m_kostenPreview = new DocumentPreviewPanel(wrapper);
    m_kostenPreview->setFixedWidth(kDocPreviewPanelWidth);
    // jahresDocColumn = 5, siehe populateKosten().
    wireOverviewTab(m_kostenTab, m_kostenPreview, /*docColumn=*/5);

    wrapperLayout->addWidget(
        wrapInOverviewGroup(tr("  Kosten-Übersicht"), m_kostenTab), 1);
    wrapperLayout->addWidget(m_kostenPreview);

    m_tabs->addTab(wrapper, tr("Kosten"));
}

// ── wireOverviewTab ───────────────────────────────────────────────────────────
//
// Ersetzt seit 19.07.2026 (Nessies Vorgabe) den vorherigen Doppelklick auf
// die Dokument-Spalte (OverviewTabWidget::documentActivated(), entfallen) —
// siehe ARCHITECTURE.md, "ShareDetailsForm: Dokument-Vorschau per
// Zeilenauswahl statt Doppelklick".

void ViewShareDetails::wireOverviewTab(OverviewTabWidget* tabs, DocumentPreviewPanel* preview,
                                        int docColumn)
{
    // 1) Klick auf eine beliebige Stelle einer Jahres-Tab-Zeile lädt sofort
    //    deren Dokument (oder leert die Vorschau, falls die Zeile keins hat).
    connect(tabs, &OverviewTabWidget::rowActivatedWithDocument,
            this, [preview](const QVariant& /*userData*/, const QString& path) {
                preview->showDocument(path);
            });

    // 2) Tab-Wechsel: Übersicht → Jahres-Tab selektiert automatisch die
    //    erste Zeile und lädt deren Dokument — identisches Verhalten zum
    //    automatischen Erst-Zeilen-Laden beim Tab-Wechsel in ViewBuyEdit/
    //    ViewSaleEdit/ViewDividendEdit/ViewBrokerageEdit (dort über den
    //    Presenter, hier als reine Anzeige direkt über die Tabelle, da es
    //    keinen Presenter-Vorgang "Zeile laden" gibt). Wechsel zurück zur
    //    Übersicht (Index 0) leert die Vorschau.
    connect(tabs, &OverviewTabWidget::currentTabChanged,
            this, [tabs, preview, docColumn](int index) {
                if (index == 0) {
                    preview->clearDocument();
                    return;
                }
                auto* container = tabs->widget(index);
                if (!container)
                    return;
                auto* tbl = qobject_cast<QTableWidget*>(
                    container->property("dataTable").value<QObject*>());
                if (!tbl || tbl->rowCount() == 0)
                    return;
                tbl->selectRow(0);

                QString path;
                if (docColumn >= 0) {
                    if (const auto* docItem = tbl->item(0, docColumn))
                        path = docItem->data(Qt::UserRole).toString();
                }
                preview->showDocument(path);
            });
}

// ── onMainTabChanged ──────────────────────────────────────────────────────────

void ViewShareDetails::onMainTabChanged(int index)
{
    Q_UNUSED(index);
    // Immer alle drei zurücksetzen (nicht nur die neu aktive) — einfacher als
    // Index-Tracking und funktional gleichwertig: Der Reset passiert entweder
    // beim Verlassen (während der Tab schon unsichtbar ist) oder beim
    // Betreten, je nachdem welche Instanz gerade betroffen ist.
    if (m_gewinneVerlusteTab)
        m_gewinneVerlusteTab->setCurrentIndex(0);
    if (m_dividendenTab)
        m_dividendenTab->setCurrentIndex(0);
    if (m_kostenTab)
        m_kostenTab->setCurrentIndex(0);
}

// ── createCalculationBox ──────────────────────────────────────────────────────

QGroupBox* ViewShareDetails::createCalculationBox(const QString& title, QGridLayout*& outGrid)
{
    auto* box = new QGroupBox(title);
    outGrid = new QGridLayout(box);
    outGrid->setHorizontalSpacing(8);
    outGrid->setVerticalSpacing(4);
    outGrid->setColumnStretch(1, 1);
    return box;
}

// ── IViewShareDetails: Header ─────────────────────────────────────────────────

void ViewShareDetails::setHeaderName(const QString& name)
{
    m_headerName = name;
    setWindowTitle(name);
}

void ViewShareDetails::setStatusLine(const QString& statusText)
{
    m_statusLine->setText(statusText);
}

void ViewShareDetails::setWebsiteUpdateLine(const QString& statusText)
{
    m_websiteUpdateLine->setText(statusText);
}

void ViewShareDetails::setUpdateWarning(const QString& text)
{
    m_updateWarningLine->setText(text);
    m_updateWarningLine->setVisible(!text.isEmpty());
}

void ViewShareDetails::setBoxesTabTitle(const QString& title)
{
    m_tabs->setTabText(m_boxesTabIndex, title);
}

// ── IViewShareDetails: Depotwert-Boxen ────────────────────────────────────────

void ViewShareDetails::populateGesamtBox(const CalculationRows& rows)
{
    populateBox(m_gesamtGrid, rows);
}

void ViewShareDetails::populateVortagBox(const CalculationRows& rows)
{
    populateBox(m_vortagGrid, rows);
}

void ViewShareDetails::populateAktuelleBox(const CalculationRows& rows)
{
    populateBox(m_aktuelleGrid, rows);
}

// ── IViewShareDetails: Gewinne/Verluste-, Dividenden-, Kosten-Tabs ────────────

void ViewShareDetails::populateGewinneVerluste(const QList<SaleObject>&       sales,
                                               const QList<ShareSplitObject>& splits)
{
    if (!m_gewinneVerlusteTab)
        return; // defensiv — Tab wird seit 14.07.2026 immer in setupUi() angelegt

    // Seit v1.31.2 gemeinsam mit ViewSaleEdit über OverviewTabFiller
    // (Refactor 19.11.2026). Marktwert-Modus "rein Kurs" (Nessies Rechenregel
    // 25.10.2026): Auszahlung = Verkaufswert brutto, Gewinn/Verlust =
    // Verkaufswert − Kaufwert, ohne Kosten und Steuern; Depotwert-Modus wie
    // der Editier-Dialog.
    OverviewTabFiller::fillSales(m_gewinneVerlusteTab, sales, splits,
                                 m_marketValueMode ? OverviewTabFiller::SaleAmounts::Market
                                                   : OverviewTabFiller::SaleAmounts::Depot);
}

void ViewShareDetails::populateDividenden(const QList<DividendObject>&   dividends,
                                          const QList<ShareSplitObject>& splits)
{
    if (!m_dividendenTab)
        return; // Marktwert-Modus — Tab existiert nicht, siehe setupUi()

    // Seit v1.31.2 gemeinsam mit ViewDividendEdit über OverviewTabFiller
    // (Refactor 19.11.2026).
    OverviewTabFiller::fillDividends(m_dividendenTab, dividends, splits);
}

void ViewShareDetails::populateKosten(const QList<BrokerageObject>& brokerages)
{
    if (!m_kostenTab)
        return; // Marktwert-Modus — Tab existiert nicht, siehe setupUi()

    // Seit v1.31.2 gemeinsam mit ViewBrokerageEdit über OverviewTabFiller
    // (Refactor 19.11.2026); Spalte "Jahr" der Übersicht dabei von 80 auf
    // 100 px angeglichen.
    OverviewTabFiller::fillBrokerages(m_kostenTab, brokerages);
}

// ── IViewShareDetails: Fehler / Lifecycle ─────────────────────────────────────

void ViewShareDetails::showError(const QString& message)
{
    OwnMessageBox::critical(this, tr("Fehler"), message);
}

void ViewShareDetails::closeDialog()
{
    QDialog::reject();
}

// ── onChartTitleInfoChanged ───────────────────────────────────────────────────

void ViewShareDetails::onChartTitleInfoChanged(const QString& infoText)
{
    // Empty infoText (no daily values at all for this share) -> fall back to
    // just the share name, same title as before ChartForm existed. This also
    // keeps test_shareDetailsDialog_validShare_constructsAndShowsCloseButtonText
    // passing unchanged for shares without seeded daily values.
    setWindowTitle(infoText.isEmpty() ? m_headerName
                                       : QStringLiteral("%1 - %2").arg(m_headerName, infoText));
}

// ── Helpers ────────────────────────────────────────────────────────────────────

void ViewShareDetails::populateBox(QGridLayout* grid, const CalculationRows& rows)
{
    // Idempotent, in case a box is ever repopulated.
    QLayoutItem* child = nullptr;
    while ((child = grid->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    int row = 0;
    for (const CalculationRow& r : rows) {
        auto* opLabel = new QLabel(r.operatorSymbol);
        opLabel->setFixedWidth(14);
        opLabel->setAlignment(Qt::AlignCenter);

        auto* nameLabel = new QLabel(r.label);

        auto* valueLabel = new QLabel(r.value);
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        if (r.color.isValid())
            valueLabel->setStyleSheet(QStringLiteral("color: %1;").arg(r.color.name()));

        if (r.emphasize) {
            QFont nameFont = nameLabel->font();
            nameFont.setBold(true);
            nameLabel->setFont(nameFont);

            QFont valueFont = valueLabel->font();
            valueFont.setBold(true);
            valueLabel->setFont(valueFont);
        }

        grid->addWidget(opLabel,    row, 0);
        grid->addWidget(nameLabel,  row, 1);
        grid->addWidget(valueLabel, row, 2);
        ++row;
    }
}


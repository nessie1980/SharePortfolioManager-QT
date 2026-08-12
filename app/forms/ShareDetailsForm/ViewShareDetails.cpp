// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ViewShareDetails.h"
#include "../../utils/ShareSplitAdjuster.h"
#include "../../utils/ShareSplitHint.h"

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

    // Marktwert-Modus (seit 14.07.2026, Nessies Vorgabe): brokeragefreie
    // Felder, konsistent zum Rest der Marktwert-Box (siehe ShareCalculator.h,
    // "Marktwert-Tab (no brokerage, no reduction)"). Depotwert-Modus bleibt
    // unverändert bei den *BrokerageReduction()-Feldern (verifiziert gegen
    // ViewSaleEdit::populateOverview(), 13.07.2026).
    const bool market = m_marketValueMode;
    auto salePayout = [market](const SaleObject& s) {
        return market ? s.payout() : s.payoutBrokerageReduction();
    };
    auto saleProfitLoss = [market](const SaleObject& s) {
        return market ? s.profitLoss() : s.profitLossBrokerageReduction();
    };

    QList<int> years;
    for (const SaleObject& s : sales)
        if (!years.contains(s.year()))
            years.append(s.year());
    std::sort(years.begin(), years.end(), std::greater<int>());

    const QLocale loc;
    auto fmtMoney  = [&](double v) { return loc.toString(v, 'f', 2) + QStringLiteral(" €"); };
    auto fmtVolume = [&](double v) { return loc.toString(v, 'f', 4) + QStringLiteral(" stk."); };

    double totalPayout = 0.0;
    for (const SaleObject& s : sales)
        totalPayout += salePayout(s);

    // ── Split-Behandlung (Phase 3c, 11.08.2026) ───────────────────────────
    //
    // Aggregate rechnen je Beleg über ShareSplitAdjuster::adjustedVolume()
    // auf heutige Skala und summieren erst danach — niemals die Summe
    // skalieren. Liegt vor dem ältesten Beleg einer Summe ein Split, war
    // mindestens ein summierter Beleg umzurechnen; genau dann trägt die
    // Zelle den Marker.
    //
    // @note Gilt für STÜCKZAHLEN, nicht für Geldbeträge. Auszahlung und
    // Gewinn/Verlust werden unverändert summiert — ein Split schafft weder
    // Gewinn noch Verlust. Siehe ARCHITECTURE.md, "Anteilige
    // Kauf-Nebenkosten der FIFO-Zuteilung".
    auto todayVolume = [&splits](const SaleObject& s) {
        return ShareSplitAdjuster::adjustedVolume(s.volume(), splits, s.date());
    };

    QMap<int, QDate> yearEarliest;
    QDate            earliestOverall;
    for (const SaleObject& s : sales) {
        const QDate d = s.date();
        if (!d.isValid()) continue;
        const int y = s.year();
        if (!yearEarliest.contains(y) || d < yearEarliest.value(y))
            yearEarliest[y] = d;
        if (!earliestOverall.isValid() || d < earliestOverall)
            earliestOverall = d;
    }

    auto yearVolAffected = [&splits, yearEarliest](int year) {
        return ShareSplitHint::hasSplitAfter(splits, yearEarliest.value(year));
    };
    auto yearVolTooltip = [&splits, yearEarliest](int year) {
        return ShareSplitHint::overviewAggregateTooltip(splits, yearEarliest.value(year));
    };
    const bool    totalVolAffected = ShareSplitHint::hasSplitAfter(splits, earliestOverall);
    const QString totalVolTooltip  =
        ShareSplitHint::overviewAggregateTooltip(splits, earliestOverall);

    m_gewinneVerlusteTab->populateOverview(
        years,
        tr("Übersicht (%1)").arg(fmtMoney(totalPayout)),
        { tr("Jahr"), tr("Anteile"), tr("Auszahlung"), tr("Gewinn / Verlust") },
        { 100, -1, -1, -1 },
        [&](QTableWidget* data) {
            data->setRowCount(years.size());
            for (int i = 0; i < years.size(); ++i) {
                const int yr = years.at(i);
                double volToday = 0.0, payout = 0.0, gv = 0.0;
                for (const SaleObject& s : sales) {
                    if (s.year() != yr) continue;
                    volToday += todayVolume(s);
                    payout   += salePayout(s);
                    gv       += saleProfitLoss(s);
                }
                auto* iYear = centeredItem(QString::number(yr));
                iYear->setData(Qt::UserRole, yr);
                data->setItem(i, 0, iYear);
                auto* iVol = centeredItem(ShareSplitHint::withMarker(
                    fmtVolume(volToday), yearVolAffected(yr)));
                if (!yearVolTooltip(yr).isEmpty())
                    iVol->setToolTip(yearVolTooltip(yr));
                data->setItem(i, 1, iVol);
                data->setItem(i, 2, centeredItem(fmtMoney(payout)));
                data->setItem(i, 3, centeredItem(fmtMoney(gv)));
            }
        },
        [&](QTableWidget* footer) {
            double totVolToday = 0.0, totGV = 0.0;
            for (const SaleObject& s : sales) {
                totVolToday += todayVolume(s);
                totGV       += saleProfitLoss(s);
            }
            footer->setItem(0, 0, centeredItem(tr("Gesamt:")));
            auto* fVol = centeredItem(ShareSplitHint::withMarker(
                fmtVolume(totVolToday), totalVolAffected));
            if (!totalVolTooltip.isEmpty())
                fVol->setToolTip(totalVolTooltip);
            footer->setItem(0, 1, fVol);
            footer->setItem(0, 2, centeredItem(fmtMoney(totalPayout)));
            footer->setItem(0, 3, centeredItem(fmtMoney(totGV)));
        },
        { tr("Datum"), tr("Anteile"), tr("Auszahlung"), tr("Gewinn / Verlust"), QString() },
        { 100, -1, -1, -1, 36 },
        [&](int year) {
            double yearPayout = 0.0;
            for (const SaleObject& s : sales) if (s.year() == year) yearPayout += salePayout(s);
            return QStringLiteral("%1 (%2)").arg(year).arg(fmtMoney(yearPayout));
        },
        [&](int year, QTableWidget* data) {
            QList<SaleObject> yearSales;
            for (const SaleObject& s : sales) if (s.year() == year) yearSales.append(s);

            data->setRowCount(yearSales.size());
            for (int i = 0; i < yearSales.size(); ++i) {
                const SaleObject& s = yearSales.at(i);

                auto* iDate = centeredItem(s.dateAsStr());
                iDate->setData(Qt::UserRole, s.guid());
                data->setItem(i, 0, iDate);

                // Belegzeile: bleibt in BELEG-Skala. Die Zeile ist eine
                // Abschrift des Dokuments, das nach einem Zeilenklick rechts
                // in der Vorschau erscheint — die Zahlen müssen sich decken.
                const bool    volAffected = ShareSplitHint::hasSplitAfter(splits, s.date());
                auto* iVol = centeredItem(ShareSplitHint::withMarker(
                    fmtVolume(s.volume()), volAffected));
                const QString volTooltip = ShareSplitHint::overviewRowTooltip(
                    splits, s.date(), s.volume(), s.salePrice());
                if (!volTooltip.isEmpty())
                    iVol->setToolTip(volTooltip);
                data->setItem(i, 1, iVol);

                data->setItem(i, 2, centeredItem(fmtMoney(salePayout(s))));
                data->setItem(i, 3, centeredItem(fmtMoney(saleProfitLoss(s))));

                if (!s.document().isEmpty()) {
                    auto* docItem = new QTableWidgetItem;
                    docItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                    docItem->setData(Qt::UserRole, s.document());
                    data->setItem(i, 4, docItem);
                    data->setCellWidget(i, 4, documentIconWidget(s.document()));
                } else {
                    data->setItem(i, 4, centeredItem(QStringLiteral("-")));
                }
            }
        },
        [&](int year, QTableWidget* footer) {
            // Summe über die Belege eines Jahres: je Beleg umrechnen, dann
            // summieren. Fällt ein Split mitten ins Jahr, mischte die frühere
            // rohe Summe zwei Stückelungen und war damit bedeutungslos.
            double volToday = 0.0, payout = 0.0, gv = 0.0;
            for (const SaleObject& s : sales) {
                if (s.year() != year) continue;
                volToday += todayVolume(s);
                payout   += salePayout(s);
                gv       += saleProfitLoss(s);
            }
            footer->setItem(0, 0, centeredItem(tr("Gesamt:")));
            auto* fVol = centeredItem(ShareSplitHint::withMarker(
                fmtVolume(volToday), yearVolAffected(year)));
            if (!yearVolTooltip(year).isEmpty())
                fVol->setToolTip(yearVolTooltip(year));
            footer->setItem(0, 1, fVol);
            footer->setItem(0, 2, centeredItem(fmtMoney(payout)));
            footer->setItem(0, 3, centeredItem(fmtMoney(gv)));
            footer->setItem(0, 4, centeredItem(QStringLiteral("-")));
        },
        /*jahresDocColumn=*/4);
}

void ViewShareDetails::populateDividenden(const QList<DividendObject>&   dividends,
                                          const QList<ShareSplitObject>& splits)
{
    if (!m_dividendenTab)
        return; // Marktwert-Modus — Tab existiert nicht, siehe setupUi()

    QList<int> years;
    for (const DividendObject& d : dividends)
        if (!years.contains(d.year()))
            years.append(d.year());
    std::sort(years.begin(), years.end(), std::greater<int>());

    const QLocale loc;
    auto fmtMoney  = [&](double v) { return loc.toString(v, 'f', 2) + QStringLiteral(" €"); };
    auto fmtVolume = [&](double v) { return loc.toString(v, 'f', 4) + QStringLiteral(" stk."); };
    auto fmtRate   = [&](double v) { return loc.toString(v, 'f', 4) + QStringLiteral(" €"); };

    double totalVal = 0.0;
    for (const DividendObject& d : dividends)
        totalVal += d.dividendPayoutWithTaxes();

    m_dividendenTab->populateOverview(
        years,
        tr("Übersicht (%1)").arg(fmtMoney(totalVal)),
        { tr("Jahr"), tr("Dividende") },
        { 100, -1 },
        [&](QTableWidget* data) {
            data->setRowCount(years.size());
            for (int i = 0; i < years.size(); ++i) {
                const int yr = years.at(i);
                double yearVal = 0.0;
                for (const DividendObject& d : dividends)
                    if (d.year() == yr) yearVal += d.dividendPayoutWithTaxes();

                auto* iYear = centeredItem(QString::number(yr));
                iYear->setData(Qt::UserRole, yr);
                data->setItem(i, 0, iYear);
                data->setItem(i, 1, centeredItem(fmtMoney(yearVal)));
            }
        },
        [&](QTableWidget* footer) {
            footer->setItem(0, 0, centeredItem(tr("Gesamt:")));
            footer->setItem(0, 1, centeredItem(fmtMoney(totalVal)));
        },
        { tr("Datum"), tr("Dividendensatz"), tr("Anteile"), tr("Dividende"), QString() },
        { 100, -1, -1, -1, 36 },
        [&](int year) {
            double yearTotal = 0.0;
            for (const DividendObject& d : dividends) if (d.year() == year) yearTotal += d.dividendPayoutWithTaxes();
            return QStringLiteral("%1 (%2)").arg(year).arg(fmtMoney(yearTotal));
        },
        [&](int year, QTableWidget* data) {
            QList<DividendObject> yearDivs;
            for (const DividendObject& d : dividends) if (d.year() == year) yearDivs.append(d);

            data->setRowCount(yearDivs.size());
            for (int i = 0; i < yearDivs.size(); ++i) {
                const DividendObject& d = yearDivs.at(i);

                auto* iDate = centeredItem(d.dateAsStr());
                iDate->setData(Qt::UserRole, d.guid());
                data->setItem(i, 0, iDate);
                data->setItem(i, 1, centeredItem(fmtRate(d.rate())));

                // Belegzeile: bleibt in BELEG-Skala. "Anteile am
                // Auszahlungstag" ist die Stückzahl, auf die die Bank
                // tatsächlich ausgeschüttet hat (Phase 3c, 11.08.2026).
                const bool volAffected = ShareSplitHint::hasSplitAfter(splits, d.date());
                auto* iVol = centeredItem(ShareSplitHint::withMarker(
                    fmtVolume(d.volume()), volAffected));
                const QString volTooltip = ShareSplitHint::overviewRowTooltip(
                    splits, d.date(), d.volume(), d.rate());
                if (!volTooltip.isEmpty())
                    iVol->setToolTip(volTooltip);
                data->setItem(i, 2, iVol);

                data->setItem(i, 3, centeredItem(fmtMoney(d.dividendPayoutWithTaxes())));

                if (!d.document().isEmpty()) {
                    auto* docItem = new QTableWidgetItem;
                    docItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                    docItem->setData(Qt::UserRole, d.document());
                    data->setItem(i, 4, docItem);
                    data->setCellWidget(i, 4, documentIconWidget(d.document()));
                } else {
                    data->setItem(i, 4, centeredItem(QStringLiteral("-")));
                }
            }
        },
        [&](int year, QTableWidget* footer) {
            double totDiv = 0.0;
            for (const DividendObject& d : dividends) {
                if (d.year() != year) continue;
                totDiv += d.dividendPayoutWithTaxes();
            }
            footer->setItem(0, 0, centeredItem(tr("Gesamt:")));
            footer->setItem(0, 1, centeredItem(QStringLiteral("-")));

            // Anteile-Summe bewusst "-" statt einer Zahl (Nessies
            // Entscheidung 11.08.2026, Phase 3c). "Anteile am Auszahlungstag"
            // bezieht sich auf je einen Stichtag; die Summe über mehrere
            // Ausschüttungen beschreibt keinen Bestand, den es je gab. Anders
            // als bei Verkäufen hilft hier auch eine Umrechnung auf heutige
            // Skala nicht weiter — sie würde aus einer bedeutungslosen Zahl
            // nur eine andere machen. Der Dividendensatz daneben steht aus
            // demselben Grund schon immer auf "-". Identisch zu
            // ViewDividendEdit::populateOverview().
            auto* fVol = centeredItem(QStringLiteral("-"));
            fVol->setToolTip(tr("Anteile beziehen sich auf verschiedene "
                                "Auszahlungstage und lassen sich nicht summieren."));
            footer->setItem(0, 2, fVol);

            footer->setItem(0, 3, centeredItem(fmtMoney(totDiv)));
            footer->setItem(0, 4, centeredItem(QStringLiteral("-")));
        },
        /*jahresDocColumn=*/4);
}

void ViewShareDetails::populateKosten(const QList<BrokerageObject>& brokerages)
{
    if (!m_kostenTab)
        return; // Marktwert-Modus — Tab existiert nicht, siehe setupUi()

    QList<int> years;
    for (const BrokerageObject& b : brokerages)
        if (!years.contains(b.year()))
            years.append(b.year());
    std::sort(years.begin(), years.end(), std::greater<int>());

    const QLocale loc;
    auto fmtMoney = [&](double v) { return loc.toString(v, 'f', 2) + QStringLiteral(" €"); };

    double totalNetto = 0.0;
    for (const BrokerageObject& b : brokerages)
        totalNetto += b.brokerageReduction();

    m_kostenTab->populateOverview(
        years,
        tr("Übersicht (%1)").arg(fmtMoney(totalNetto)),
        { tr("Jahr"), tr("Netto-Kosten") },
        { 80, -1 },
        [&](QTableWidget* data) {
            data->setRowCount(years.size());
            for (int i = 0; i < years.size(); ++i) {
                const int yr = years.at(i);
                double netto = 0.0;
                for (const BrokerageObject& b : brokerages)
                    if (b.year() == yr) netto += b.brokerageReduction();

                auto* iYear = centeredItem(QString::number(yr));
                iYear->setData(Qt::UserRole, yr);
                data->setItem(i, 0, iYear);
                data->setItem(i, 1, centeredItem(fmtMoney(netto)));
            }
        },
        [&](QTableWidget* footer) {
            footer->setItem(0, 0, centeredItem(tr("Gesamt:")));
            footer->setItem(0, 1, centeredItem(fmtMoney(totalNetto)));
        },
        { tr("Datum"), tr("Typ"), tr("Ges. Gebühren"), tr("Rabatt"), tr("Netto-Kosten"), QString() },
        { 100, -1, -1, -1, -1, 36 },
        [&](int year) {
            double yearNetto = 0.0;
            for (const BrokerageObject& b : brokerages) if (b.year() == year) yearNetto += b.brokerageReduction();
            return QStringLiteral("%1 (%2)").arg(year).arg(fmtMoney(yearNetto));
        },
        [&](int year, QTableWidget* data) {
            QList<BrokerageObject> yearBrokerages;
            for (const BrokerageObject& b : brokerages) if (b.year() == year) yearBrokerages.append(b);

            data->setRowCount(yearBrokerages.size());
            for (int i = 0; i < yearBrokerages.size(); ++i) {
                const BrokerageObject& b = yearBrokerages.at(i);

                // "Typ": Kauf (buyGuid gesetzt) / Verkauf (saleGuid gesetzt) /
                // Sonstig (Standalone-Eintrag) — identisch zu ViewBrokerageEdit.
                const QString typ = !b.buyGuid().isEmpty()  ? tr("Kauf")
                                   : !b.saleGuid().isEmpty() ? tr("Verkauf")
                                                              : tr("Sonstig");

                auto* iDate = centeredItem(b.dateAsStr());
                iDate->setData(Qt::UserRole, b.guid());
                data->setItem(i, 0, iDate);
                data->setItem(i, 1, centeredItem(typ));
                data->setItem(i, 2, centeredItem(fmtMoney(b.brokerage())));
                data->setItem(i, 3, centeredItem(fmtMoney(b.reduction())));
                data->setItem(i, 4, centeredItem(fmtMoney(b.brokerageReduction())));

                if (!b.document().isEmpty()) {
                    auto* docItem = new QTableWidgetItem;
                    docItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                    docItem->setData(Qt::UserRole, b.document());
                    data->setItem(i, 5, docItem);
                    data->setCellWidget(i, 5, documentIconWidget(b.document()));
                } else {
                    data->setItem(i, 5, centeredItem(QStringLiteral("-")));
                }
            }
        },
        [&](int year, QTableWidget* footer) {
            double sumGeb = 0.0, sumRabatt = 0.0, sumNetto = 0.0;
            for (const BrokerageObject& b : brokerages) {
                if (b.year() != year) continue;
                sumGeb    += b.brokerage();
                sumRabatt += b.reduction();
                sumNetto  += b.brokerageReduction();
            }
            footer->setItem(0, 0, centeredItem(tr("Gesamt:")));
            footer->setItem(0, 1, centeredItem(QStringLiteral("-")));
            footer->setItem(0, 2, centeredItem(fmtMoney(sumGeb)));
            footer->setItem(0, 3, centeredItem(fmtMoney(sumRabatt)));
            footer->setItem(0, 4, centeredItem(fmtMoney(sumNetto)));
            footer->setItem(0, 5, centeredItem(QStringLiteral("-")));
        },
        /*jahresDocColumn=*/5);
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

QTableWidgetItem* ViewShareDetails::centeredItem(const QString& text)
{
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(Qt::AlignCenter);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

QWidget* ViewShareDetails::documentIconWidget(const QString& documentPath)
{
    // Identisch zur Icon-Auswahl-Logik in ViewDividendEdit::populateOverview()
    // und ViewBrokerageEdit::makeDocIconWidget().
    const QString ext = QFileInfo(documentPath).suffix().toLower();
    IconProvider::IconName iconName;
    if (ext == QStringLiteral("pdf"))
        iconName = IconProvider::DocPdfImage16;
    else if (ext == QStringLiteral("doc") || ext == QStringLiteral("docx"))
        iconName = IconProvider::DocWordImage16;
    else if (ext == QStringLiteral("xls") || ext == QStringLiteral("xlsx"))
        iconName = IconProvider::DocExcelImage16;
    else
        iconName = IconProvider::SearchFailed2;

    auto* label = new QLabel;
    label->setAlignment(Qt::AlignCenter);
    label->setPixmap(IconProvider::icon(iconName).pixmap(16, 16));
    label->setToolTip(documentPath);
    return label;
}

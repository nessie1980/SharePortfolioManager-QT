// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ViewShareDetails.h"

#include "../OwnMessageBoxForm/OwnMessageBox.h"

#include <QHBoxLayout>
#include <QFrame>
#include <QDialogButtonBox>
#include <QFont>
#include <QPalette>

// ── Constructor ───────────────────────────────────────────────────────────────

ViewShareDetails::ViewShareDetails(const QString& shareGuid, bool marketValueMode, QWidget* parent)
    : QDialog(parent)
    , m_presenter(*this, m_model, shareGuid, marketValueMode)
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

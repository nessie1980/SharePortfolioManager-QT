// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ViewShareDetails.h"

#include "../OwnMessageBoxForm/OwnMessageBox.h"

#include <QHBoxLayout>
#include <QFrame>
#include <QDialogButtonBox>
#include <QFont>

// ── Constructor ───────────────────────────────────────────────────────────────

ViewShareDetails::ViewShareDetails(const QString& shareGuid, QWidget* parent)
    : QDialog(parent)
    , m_presenter(*this, m_model, shareGuid)
{
    setObjectName(QStringLiteral("ViewShareDetails"));
    // TODO: once the chart tab computes a display window, restore the C#
    // reference's full title ("{Name} - Zeitraum: ... / Entwicklung: ...").
    // For now the window title is just the share name (setHeaderName()).
    setMinimumSize(900, 600);
    resize(1100, 700);

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
    // Placeholder — the chart itself is tracked as its own ChartForm work
    // item (see ARCHITECTURE.md, "Offene Punkte / TODO") and intentionally
    // not embedded here yet, even though the C# reference has it as tab 1
    // of this same dialog.
    auto* placeholder = new QWidget();
    placeholder->setObjectName(QStringLiteral("chartPlaceholder"));

    auto* layout = new QVBoxLayout(placeholder);
    auto* label  = new QLabel(tr("Der Aktien-Chart ist noch nicht implementiert."));
    label->setAlignment(Qt::AlignCenter);
    QFont f = label->font();
    f.setItalic(true);
    label->setFont(f);
    layout->addWidget(label);

    m_tabs->addTab(placeholder, tr("Aktien-Chart"));
}

// ── setupDepotwertTab ─────────────────────────────────────────────────────────

void ViewShareDetails::setupDepotwertTab()
{
    auto* tab = new QWidget();
    tab->setObjectName(QStringLiteral("depotwertTab"));

    auto* grid = new QGridLayout(tab);
    grid->setContentsMargins(12, 12, 12, 12);
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(16);

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

    m_tabs->addTab(tab, tr("Komplette Depotbewertung"));
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
    setWindowTitle(name);
}

void ViewShareDetails::setStatusLine(const QString& statusText)
{
    m_statusLine->setText(statusText);
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

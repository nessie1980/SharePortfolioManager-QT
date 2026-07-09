// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QDialog>
#include <QTabWidget>
#include <QLabel>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QGroupBox>

#include "IViewShareDetails.h"
#include "ModelShareDetails.h"
#include "PresenterShareDetails.h"

/**
 * @brief Read-only share-details dialog — ported from the C# reference's
 * FrmShareDetails.
 *
 * Opened by double-clicking a row in either of the main portfolio tables
 * (Depotwert-/Marktwert-Tab) — see MainWindow::onPortfolioRowDoubleClicked().
 *
 * Current scope (see ARCHITECTURE.md, "ShareDetailsForm-Details"):
 * - "Aktien-Chart" tab: placeholder only — the actual chart is tracked as
 *   its own ChartForm work item and not embedded here (yet).
 * - "Komplette Depotbewertung" tab: three "Bestandsberechnung" boxes
 *   (Gesamt/Vortag/Aktuelle), rendered as vertical calculation rows rather
 *   than the C# reference's multi-column WinForms grid — an intentional,
 *   revisitable simplification (see chat history, 09.07.2026).
 *
 * Deliberately NOT yet implemented (deferred): Marktwert mode (no
 * MainWindow.cpp mode-switch has been wired up — the dialog always shows
 * the Depotwert box regardless of which portfolio tab triggered the
 * double-click) and the Gewinne/Verluste-, Dividenden- and Kosten-tabs
 * (planned to reuse ViewSaleEdit's/ViewDividendEdit's/ViewBrokerageEdit's
 * existing overview widgets rather than duplicating them here).
 *
 * Pure MVP View: contains no repository access and no formatting/business
 * logic. All data arrives already formatted via IViewShareDetails, computed
 * by PresenterShareDetails from ModelShareDetails.
 */
class ViewShareDetails : public QDialog, public IViewShareDetails
{
    Q_OBJECT

public:
    /**
     * @brief Construct and populate the dialog for the given share GUID.
     * @param shareGuid  GUID of the share to display.
     * @param parent     Parent widget.
     */
    explicit ViewShareDetails(const QString& shareGuid, QWidget* parent = nullptr);
    ~ViewShareDetails() override = default;

    /**
     * @brief Whether the share GUID resolved to a valid share.
     *
     * If false, showError() has already displayed a message and the dialog
     * must not be exec()'d — see MainWindow::onPortfolioRowDoubleClicked().
     */
    bool hasValidShare() const { return m_validShare; }

    // ── IViewShareDetails ────────────────────────────────────────────────────
    void setHeaderName(const QString& name) override;
    void setStatusLine(const QString& statusText) override;

    void populateGesamtBox(const CalculationRows& rows) override;
    void populateVortagBox(const CalculationRows& rows) override;
    void populateAktuelleBox(const CalculationRows& rows) override;

    void showError(const QString& message) override;
    void closeDialog() override;

private:
    // ── Setup ──────────────────────────────────────────────────────────────
    void setupUi();
    void setupChartTab();
    void setupDepotwertTab();

    /** Creates one "Gesamt-/Vortag-/Aktuelle Bestandsberechnung" QGroupBox with an empty grid. */
    QGroupBox* createCalculationBox(const QString& title, QGridLayout*& outGrid);

    /** Generic row rendering shared by all three calculation boxes. */
    static void populateBox(QGridLayout* grid, const CalculationRows& rows);

    // ── MVP wiring ─────────────────────────────────────────────────────────
    ModelShareDetails     m_model;
    PresenterShareDetails m_presenter;
    bool                  m_validShare = false;

    // ── Widgets ────────────────────────────────────────────────────────────
    QLabel*     m_statusLine = nullptr;
    QTabWidget* m_tabs       = nullptr;

    QGridLayout* m_gesamtGrid   = nullptr;
    QGridLayout* m_vortagGrid   = nullptr;
    QGridLayout* m_aktuelleGrid = nullptr;
};

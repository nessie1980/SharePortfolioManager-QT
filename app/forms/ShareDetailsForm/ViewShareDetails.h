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
#include "../ChartForm/ViewChart.h"
#include "../../widgets/OverviewTabWidget.h"
#include "../../widgets/DocumentPreviewPanel.h"

/**
 * @brief Read-only share-details dialog — ported from the C# reference's
 * FrmShareDetails.
 *
 * Opened by double-clicking a row in either of the main portfolio tables
 * (Depotwert-/Marktwert-Tab) — see MainWindow::onPortfolioRowDoubleClicked(),
 * which passes marketValueMode based on which table the double-click came from.
 *
 * Current scope (see ARCHITECTURE.md, "ShareDetailsForm-Details"):
 * - "Aktien-Chart" tab: embeds ViewChart (own MVP triad, see ChartForm/) —
 *   implemented 12.07.2026.
 * - "Komplette Depotbewertung" / "Komplette Marktbewertung" tab: three
 *   "Bestandsberechnung" boxes (Gesamt/Vortag/Aktuelle), rendered as vertical
 *   calculation rows rather than the C# reference's multi-column WinForms
 *   grid — an intentional, revisitable simplification (see chat history,
 *   09.07.2026).
 * - "Gewinne/Verluste" / "Dividenden" / "Kosten" tabs: je eine reine
 *   Anzeige-Instanz von OverviewTabWidget (siehe app/widgets/
 *   OverviewTabWidget.h) — dasselbe "Übersicht + Jahres-Tabs mit
 *   Frozen-Footer"-Muster, das ViewSaleEdit/ViewDividendEdit/
 *   ViewBrokerageEdit für ihre Editier-Übersichten nutzen, hier aber ohne
 *   Verbindung zu einem Editier-Formular (rein lesend). "Gewinne/Verluste"
 *   existiert in beiden Modi (implementiert 13.07.2026, auf beide Modi
 *   erweitert 14.07.2026 — im Marktwert-Modus mit brokeragefreien Werten,
 *   siehe populateGewinneVerluste()); "Dividenden" und "Kosten" bleiben
 *   Depotwert-only, da beides laut C#-Referenz reine Depotwert-Konzepte sind
 *   (siehe ARCHITECTURE.md, "Marktwert- vs. Depotwert-Modus").
 *
 * Pure MVP View: contains no repository access and no formatting/business
 * logic beyond die reine Tabellen-Darstellung in den neuen Tabs (Jahres-
 * Gruppierung/-Summierung liegt dort bewusst in der View, identisch zum
 * bestehenden Muster in den Editier-Dialogen — keine Abweichung von der
 * Architektur-Konvention, die schon dort etabliert ist).
 */
class ViewShareDetails : public QDialog, public IViewShareDetails
{
    Q_OBJECT

public:
    /**
     * @brief Construct and populate the dialog for the given share GUID.
     * @param shareGuid        GUID of the share to display.
     * @param marketValueMode  true if opened from the Marktwert portfolio
     *                         tab (shows "Komplette Marktbewertung", disabled
     *                         Dividenden rows, brokerage-free figures,
     *                         brokeragefreier Gewinne/Verluste-Tab, KEINE
     *                         Dividenden-/Kosten-Tabs);
     *                         false for the Depotwert tab (default).
     * @param parent           Parent widget.
     */
    explicit ViewShareDetails(const QString& shareGuid, bool marketValueMode = false,
                              QWidget* parent = nullptr);
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
    void setWebsiteUpdateLine(const QString& statusText) override;
    void setBoxesTabTitle(const QString& title) override;

    void populateGesamtBox(const CalculationRows& rows) override;
    void populateVortagBox(const CalculationRows& rows) override;
    void populateAktuelleBox(const CalculationRows& rows) override;

    void populateGewinneVerluste(const QList<SaleObject>& sales) override;
    void populateDividenden(const QList<DividendObject>& dividends) override;
    void populateKosten(const QList<BrokerageObject>& brokerages) override;

    void showError(const QString& message) override;
    void closeDialog() override;

private slots:
    /**
     * @brief Combines the share name with ViewChart's "Zeitraum: ... /
     * Entwicklung: ..." info into the full window title — restores the C#
     * reference's title format, deferred until the chart tab existed
     * (see ViewShareDetails.cpp, setHeaderName()).
     * @param infoText  Empty string if ViewChart has no range info yet
     *                  (e.g. no daily values at all for this share) — in
     *                  that case the title falls back to just the share name.
     */
    void onChartTitleInfoChanged(const QString& infoText);

    /**
     * @brief Setzt beim Wechsel des äußeren Tabs (Aktien-Chart/Depotwert/
     * Gewinne-Verluste/Dividenden/Kosten) alle vorhandenen Gewinne/Verluste-,
     * Dividenden- und Kosten-Tabs auf ihren Übersicht-Tab (Index 0) zurück —
     * verhindert, dass beim erneuten Betreten eines dieser Tabs weiterhin ein
     * zuvor gewählter Jahres-Tab angezeigt wird (14.07.2026, Nessies Vorgabe).
     * @param index  Ungenutzt — es werden immer alle drei Instanzen
     *               zurückgesetzt, unabhängig davon, welcher Tab neu aktiv
     *               wurde (einfacher und robuster als Index-Tracking pro Tab).
     */
    void onMainTabChanged(int index);

private:
    // ── Setup ──────────────────────────────────────────────────────────────
    void setupUi();
    void setupChartTab();
    void setupDepotwertTab();
    void setupGewinneVerlusteTab();
    void setupDividendenTab();
    void setupKostenTab();

    /** Creates one "Gesamt-/Vortag-/Aktuelle Bestandsberechnung" QGroupBox with an empty grid. */
    QGroupBox* createCalculationBox(const QString& title, QGridLayout*& outGrid);

    /** Generic row rendering shared by all three calculation boxes. */
    static void populateBox(QGridLayout* grid, const CalculationRows& rows);

    /** Erzeugt für eine Zeile einen zentrierten, nicht editierbaren QTableWidgetItem. */
    static QTableWidgetItem* centeredItem(const QString& text);

    /** Dokument-Icon (PDF/Word/Excel) bzw. "-" für die Dokument-Spalte, identisch
     *  zur Icon-Auswahl-Logik in ViewDividendEdit/ViewBrokerageEdit. */
    static QWidget* documentIconWidget(const QString& documentPath);

    // ── MVP wiring ─────────────────────────────────────────────────────────
    ModelShareDetails     m_model;
    PresenterShareDetails m_presenter;
    bool                  m_validShare = false;
    bool                  m_marketValueMode; ///< Steuert Boxen-Titel/-Inhalt sowie, ob Dividenden-/Kosten-Tab zusätzlich angelegt werden (Gewinne/Verluste immer).
    QString               m_shareGuid;  ///< Stored for setupChartTab() (ViewChart construction).
    QString               m_headerName; ///< Share name, combined with ViewChart's range info for the window title.

    // ── Widgets ────────────────────────────────────────────────────────────
    QLabel*     m_statusLine        = nullptr;
    QTabWidget* m_tabs              = nullptr;
    QLabel*     m_websiteUpdateLine = nullptr; ///< Inside the Depotwert-/Marktwert-tab.
    int         m_boxesTabIndex     = -1;      ///< Index of the Depotwert-/Marktwert-tab, for setBoxesTabTitle().

    QGridLayout* m_gesamtGrid   = nullptr;
    QGridLayout* m_vortagGrid   = nullptr;
    QGridLayout* m_aktuelleGrid = nullptr;

    OverviewTabWidget* m_gewinneVerlusteTab = nullptr; ///< Nur im Depotwert-Modus angelegt.
    OverviewTabWidget* m_dividendenTab      = nullptr; ///< Nur im Depotwert-Modus angelegt.
    OverviewTabWidget* m_kostenTab          = nullptr; ///< Nur im Depotwert-Modus angelegt.

    // Je ein eingebettetes Vorschau-Panel rechts neben der jeweiligen
    // OverviewTabWidget-Instanz, aktualisiert über deren documentActivated()-
    // Signal (Doppelklick auf die Dokument-Spalte einer Jahres-Tab-Zeile).
    DocumentPreviewPanel* m_gewinneVerlustePreview = nullptr;
    DocumentPreviewPanel* m_dividendenPreview      = nullptr;
    DocumentPreviewPanel* m_kostenPreview          = nullptr;
};

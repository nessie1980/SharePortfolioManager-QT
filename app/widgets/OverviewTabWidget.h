// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QStringList>
#include <QVariant>

#include <functional>

/**
 * @brief Wiederverwendbares "Übersicht + Jahres-Tabs"-Anzeige-Widget mit
 * Frozen-Footer-Tabellen (Gesamt-Zeile bleibt immer sichtbar, kein Scrollen).
 *
 * Extrahiert 13.07.2026 aus der bis dahin dreifach identisch existierenden
 * `buildFrozenTable()`-Lambda in `ViewBuyEdit`/`ViewSaleEdit`/`ViewDividendEdit`
 * (im Code selbst als "identical to ViewBuyEdit/ViewSaleEdit" dokumentiert) —
 * siehe ARCHITECTURE.md, Abschnitt "OverviewTabWidget". `ViewBrokerageEdit`
 * hatte eine vierte, leicht abweichende Variante; alle vier werden auf dieses
 * gemeinsame Widget umgestellt.
 *
 * Rein passiv (kein Presenter-Wissen, keine Berechnungslogik): der Aufrufer
 * liefert Spaltendefinitionen und Populate-Callbacks, das Widget kümmert sich
 * ausschließlich um Tab-Aufbau, Frozen-Footer-Layout, Spaltenbreiten-Sync
 * zwischen Daten- und Footer-Tabelle sowie die Übersicht→Jahr-Klick-Navigation.
 *
 * Verwendet von:
 * - `ViewBuyEdit`, `ViewSaleEdit`, `ViewDividendEdit`, `ViewBrokerageEdit`
 *   (Editier-Dialoge — dort zusätzlich mit `rowActivated()` verbunden, um bei
 *   Klick auf eine Jahres-Tab-Zeile den entsprechenden Datensatz im linken
 *   Formular zu laden).
 * - `ViewShareDetails` (reine Anzeige — `rowActivated()` bleibt unverbunden,
 *   da es dort kein Editier-Formular gibt).
 *
 * Jede Tabelle besteht aus zwei `QTableWidget`s: einer scrollbaren `dataTable`
 * und einer einzeiligen, nicht scrollbaren `footerTable` mit der fett
 * hervorgehobenen Gesamt-Zeile. Beide sind über `container->property("dataTable")`
 * bzw. `"footerTable")` auffindbar — identisch zum bisherigen Test-Zugriffsmuster
 * (siehe z.B. `tst_buysform.cpp`, `dataTableFromContainer()`), damit bestehende
 * und neue Tests unverändert funktionieren.
 */
class OverviewTabWidget : public QWidget
{
    Q_OBJECT

public:
    explicit OverviewTabWidget(QWidget* parent = nullptr);

    /**
     * @brief Baut Übersicht-Tab (Index 0) und Jahres-Tabs (Index 1..n, in der
     * von @p years vorgegebenen Reihenfolge) komplett neu auf. Vorherige Tabs
     * werden entfernt. Eine leere @p years-Liste räumt lediglich auf (keine Tabs).
     *
     * @param years                     Jahre in der gewünschten Tab-Reihenfolge
     *                                  (Konvention im Projekt: absteigend).
     * @param uebersichtTitle           Tab-Titel für Index 0, z.B. "Übersicht (1.234,56 €)".
     * @param uebersichtHeaders         Spaltenüberschriften des Übersicht-Tabs.
     * @param uebersichtColWidths       Spaltenbreiten in px; -1 = stretch. Gleiche
     *                                  Länge wie uebersichtHeaders.
     * @param populateUebersichtData    Füllt die Datenzeilen des Übersicht-Tabs
     *                                  (eine Zeile pro Jahr; Spalte 0 sollte das
     *                                  Jahr per Qt::UserRole tragen, siehe
     *                                  onUebersichtRowActivated()).
     * @param populateUebersichtFooter  Füllt die Gesamt-Zeile des Übersicht-Tabs.
     * @param jahresHeaders             Spaltenüberschriften je Jahres-Tab (für
     *                                  alle Jahre identisch).
     * @param jahresColWidths           Spaltenbreiten je Jahres-Tab; -1 = stretch.
     * @param jahresTitleForYear        Liefert den Tab-Titel für ein gegebenes
     *                                  Jahr, z.B. "2024 (456,78 €)".
     * @param populateJahresData        Füllt die Datenzeilen eines Jahres-Tabs
     *                                  (Jahr als Parameter; Spalte 0 sollte die
     *                                  GUID der Zeile per Qt::UserRole tragen).
     * @param populateJahresFooter      Füllt die Gesamt-Zeile eines Jahres-Tabs
     *                                  (Jahr als Parameter).
     * @param jahresDocColumn           Spaltenindex der Dokument-Spalte in den
     *                                  Jahres-Tabs (-1 = keine Dokument-Spalte,
     *                                  Standard). Bei >= 0 emittiert ein
     *                                  Doppelklick auf diese Spalte
     *                                  documentActivated(path) — der
     *                                  Dokumentpfad muss dazu im Qt::UserRole
     *                                  des jeweiligen Zell-Items stehen.
     */
    void populateOverview(
        const QList<int>& years,
        const QString& uebersichtTitle,
        const QStringList& uebersichtHeaders,
        const QList<int>& uebersichtColWidths,
        const std::function<void(QTableWidget* data)>& populateUebersichtData,
        const std::function<void(QTableWidget* footer)>& populateUebersichtFooter,
        const QStringList& jahresHeaders,
        const QList<int>& jahresColWidths,
        const std::function<QString(int year)>& jahresTitleForYear,
        const std::function<void(int year, QTableWidget* data)>& populateJahresData,
        const std::function<void(int year, QTableWidget* footer)>& populateJahresFooter,
        int jahresDocColumn = -1);

    /** Entfernt alle Tabs (z.B. für eine leere Datenliste). */
    void clear();

    /** Für Tests und Spezialfälle direkter Zugriff auf das interne QTabWidget. */
    QTabWidget* tabWidget() const { return m_tabs; }

signals:
    /**
     * @brief Gefeuert bei Klick auf eine Zeile in einem Jahres-Tab; @p userData
     * ist der Wert aus Qt::UserRole von Spalte 0 der geklickten Zeile
     * (üblicherweise eine GUID). In rein anzeigenden Kontexten (ViewShareDetails)
     * bleibt dieses Signal unverbunden.
     */
    void rowActivated(const QVariant& userData);

    /**
     * @brief Gefeuert bei Doppelklick auf die Dokument-Spalte einer Jahres-
     * Tab-Zeile (siehe jahresDocColumn in populateOverview()); @p path ist
     * der Dokumentpfad aus Qt::UserRole. Der Aufrufer entscheidet, was damit
     * passiert (z.B. eine eingebettete Vorschau aktualisieren) — OverviewTabWidget
     * öffnet selbst keinen Dialog und kennt keine PDF-Anzeige-Logik.
     */
    void documentActivated(const QString& path);

private:
    /**
     * @brief Generische Frozen-Footer-Tabellen-Fabrik — identisch zum bisherigen
     * `buildFrozenTable()`-Muster in ViewBuyEdit/ViewSaleEdit/ViewDividendEdit.
     * @return Container-QWidget mit dataTable (Property "dataTable") + Separator
     *         + footerTable (Property "footerTable").
     */
    QWidget* buildFrozenTable(
        int colCount,
        const QStringList& headers,
        const QList<int>& colWidths,
        const std::function<void(QTableWidget*)>& populateData,
        const std::function<void(QTableWidget*)>& populateFooter,
        int docColumn = -1);

    /** Klick auf eine Übersicht-Zeile → springt zum Jahres-Tab (Jahr aus Qt::UserRole Sp.0). */
    void onUebersichtRowActivated(QTableWidgetItem* item);

    /** Klick auf eine Jahres-Tab-Zeile → emittiert rowActivated() (GUID aus Qt::UserRole Sp.0). */
    void onJahresRowActivated(QTableWidgetItem* item);

    QTabWidget* m_tabs = nullptr;
    /** Jahr je Tab-Index (Index 0 = Übersicht, ungültig/-1); für die Klick-Navigation. */
    QList<int>  m_tabYears;
    bool        m_suppressTabSignal = false;
};

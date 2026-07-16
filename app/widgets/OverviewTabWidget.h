// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QWidget>
#include <QTabBar>
#include <QStackedWidget>
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
 * (im Code selbst als "identical to ViewBuyEdit/ViewSaleEdit" dokumentiert).
 * `ViewBrokerageEdit` hatte eine vierte, leicht abweichende Variante; alle
 * vier sollen künftig auf dieses gemeinsame Widget umgestellt werden — Stand
 * 14.07.2026 nutzt aber tatsächlich nur `ViewShareDetails` diese Klasse; die
 * Editier-Dialoge haben weiterhin je eine eigene, lokale Kopie des Musters
 * (eigenes `m_overviewTabs`/`buildFrozenTable`).
 *
 * Rein passiv (kein Presenter-Wissen, keine Berechnungslogik): der Aufrufer
 * liefert Spaltendefinitionen und Populate-Callbacks, das Widget kümmert sich
 * ausschließlich um Tab-Aufbau, Frozen-Footer-Layout, Spaltenbreiten-Sync
 * zwischen Daten- und Footer-Tabelle sowie die Übersicht→Jahr-Klick-Navigation.
 *
 * @note **Fixierter Übersicht-Tab (14.07.2026, auf Nessies Vorgabe):** Intern
 * kein einzelnes `QTabWidget` mehr, sondern zwei nebeneinanderliegende
 * `QTabBar`s über einem gemeinsamen `QStackedWidget` (`m_stack`):
 * - `m_pinnedBar` — genau ein Eintrag ("Übersicht"), nie scrollbar.
 * - `m_yearsBar`  — die Jahres-Tabs, scrollt bei Bedarf (QTabBar-Standard).
 *
 * Grund: Bei vielen Jahren an Historie zeigt ein einzelnes `QTabWidget`
 * Scroll-Pfeile an, sobald die Tab-Leiste zu breit wird — dabei kann der
 * Übersicht-Tab (bisher Index 0) mit aus dem Sichtbereich scrollen.
 * `QTabWidget`/`QTabBar` unterstützen "angepinnte" Tabs nicht nativ, daher
 * die Aufteilung in zwei Bars. Ein schmaler `QFrame::VLine`-Separator trennt
 * beide optisch. `count()`/`widget()`/`tabText()`/`currentIndex()`/
 * `setCurrentIndex()` ersetzen die bisherige `tabWidget()`-Methode und bilden
 * weiterhin einen einzigen, durchgehenden Index ab (0 = Übersicht,
 * 1..n = Jahre in Aufbau-Reihenfolge), damit `m_tabYears` und die
 * Klick-Navigation (`onUebersichtRowActivated()`) unverändert bleiben.
 *
 * @note **Bugfixes nach erstem Build (14.07.2026, Nessies Feedback):**
 * - **Übersicht-Tab nicht mehr anwählbar:** `m_pinnedBar` hat nur genau
 *   einen Tab (Index 0) — dessen `currentIndex` ändert sich also nie,
 *   wodurch `QTabBar::currentChanged` bei einem Klick auf einen bereits
 *   (intern) als "aktuell" geltenden Tab nicht feuert. Nach einem Sprung in
 *   einen Jahres-Tab (z.B. per Klick auf eine Übersicht-Zeile) ließ sich der
 *   Übersicht-Tab dadurch nicht mehr zurück anwählen, derselbe Effekt drohte
 *   spiegelbildlich in `m_yearsBar`. Fix: beide `QTabBar`s auf
 *   `QTabBar::tabBarClicked(int)` statt `currentChanged(int)` umgestellt —
 *   feuert bei jedem tatsächlichen Klick, unabhängig vom internen Indexstand
 *   der jeweiligen Bar. Die Klick-Handler (`onPinnedBarClicked()`/
 *   `onYearsBarClicked()`) rufen direkt `setCurrentIndex()` auf, die Stack
 *   und beide Bars synchron hält; `setCurrentIndex()` selbst konnte dadurch
 *   vereinfacht werden (der ursprüngliche `m_suppressTabSignal`-Tanz um die
 *   Bar-`setCurrentIndex()`-Aufrufe war nur zur Rekursionsvermeidung bei
 *   `currentChanged` nötig, `tabBarClicked` feuert nicht bei programmatischen
 *   Änderungen).
 * - **Spaltenköpfe erst bei Selektion fett:** `buildFrozenTable()` setzte
 *   Fettschrift bisher nur auf die Footer-Zeile — die als "erst bei
 *   Selektion fett" wahrgenommene Kopfzeile kam von Qt's Style-
 *   Standardverhalten (`QHeaderView::highlightSections`, hebt die zur
 *   Selektion gehörige Kopfspalte hervor). Fix: `data->horizontalHeader()->
 *   setFont(...)` (fett) direkt beim Tabellenaufbau gesetzt sowie
 *   `setHighlightSections(false)`, sodass die Spaltenköpfe unabhängig von
 *   jeder Selektion immer fett erscheinen.
 *
 * Bewusst (weiterhin) vereinfacht: Wenn der Übersicht-Tab aktiv ist, zeigt
 * `m_yearsBar` weiterhin seinen zuletzt gewählten Jahres-Tab optisch als
 * "selektiert" an (native `QTabBar`/`QStackedWidget` kennen keinen "keiner
 * ausgewählt"-Zustand) — der `VLine`-Separator macht die beiden Gruppen aber
 * klar erkennbar. Bei Bedarf (weiteres visuelles Feedback) kann das per
 * Stylesheet/Property noch verfeinert werden.
 *
 * Verwendet von:
 * - `ViewShareDetails` (Gewinne/Verluste-, Dividenden-, Kosten-Tab; reine
 *   Anzeige — `rowActivated()` bleibt dort unverbunden, da es kein
 *   Editier-Formular gibt).
 *
 * Jede Tabelle besteht aus zwei `QTableWidget`s: einer scrollbaren `dataTable`
 * und einer einzeiligen, nicht scrollbaren `footerTable` mit der fett
 * hervorgehobenen Gesamt-Zeile. Beide sind über `container->property("dataTable")`
 * bzw. `"footerTable")` auffindbar.
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

    // ── Durchgehender Tab-Index (0 = Übersicht, 1..n = Jahre) ───────────────
    // Ersetzt die bisherige tabWidget()-Methode: intern existiert kein
    // einzelnes QTabWidget mehr (siehe Klassenkommentar), daher bildet
    // OverviewTabWidget selbst die für Aufrufer/Tests relevante Teilmenge der
    // bisherigen QTabWidget-Schnittstelle nach.

    /** Gesamtzahl der Tabs (Übersicht + Jahre). */
    int count() const;

    /** Container-Widget am gegebenen Index, oder nullptr außerhalb des Bereichs. */
    QWidget* widget(int index) const;

    /** Tab-Titel am gegebenen Index. */
    QString tabText(int index) const;

    /** Aktuell angezeigter Index. */
    int currentIndex() const;

    /** Wechselt zum Tab am gegebenen Index (0 = Übersicht, 1..n = Jahre). */
    void setCurrentIndex(int index);

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

    /**
     * @brief Gefeuert, wenn der sichtbare Tab wechselt (Klick auf den
     * Übersicht- oder einen Jahres-Tab-Reiter selbst, oder programmatischer
     * Sprung z.B. durch einen Klick auf eine Übersicht-Zeile) — nicht bei
     * einem Zeilenklick innerhalb eines Jahres-Tabs (dafür: rowActivated()).
     * @p index folgt der durchgehenden Zählung (0 = Übersicht, 1..n = Jahre).
     * Wird während populateOverview()/clear() unterdrückt. In
     * ViewShareDetails bleibt dieses Signal unverbunden (rein anzeigender
     * Kontext).
     */
    void currentTabChanged(int index);

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

    /** tabBarClicked-Handler von m_pinnedBar — feuert bei jedem Klick, auch
     *  wenn m_pinnedBar (nur 1 Tab, Index immer 0) intern "unverändert" bleibt. */
    void onPinnedBarClicked(int index);

    /** tabBarClicked-Handler von m_yearsBar — s.o., auch wenn der angeklickte
     *  Jahres-Tab schon als "aktuell" in m_yearsBar gemerkt war. */
    void onYearsBarClicked(int index);

    /** Leert die Selektion in allen Tabellen — Konsistenz beim Tab-Wechsel. */
    void clearAllTableSelections();

    QTabBar*        m_pinnedBar = nullptr; ///< Fixiert: einziger "Übersicht"-Tab, nie scrollbar.
    QTabBar*        m_yearsBar  = nullptr; ///< Jahres-Tabs, scrollt bei Bedarf (QTabBar-Standard).
    QStackedWidget* m_stack     = nullptr; ///< Seiten: Index 0 = Übersicht, 1..n = Jahre.

    /** Jahr je Tab-Index (Index 0 = Übersicht, ungültig/-1); für die Klick-Navigation. */
    QList<int>  m_tabYears;
    bool        m_suppressTabSignal = false;
};

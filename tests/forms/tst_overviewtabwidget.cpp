// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// tst_overviewtabwidget.cpp — Unit tests for OverviewTabWidget.
//
// Bewusste Ausnahme vom Fake-View/Fake-Model-Muster der übrigen Form-Tests
// (siehe tst_sharedetailsform.cpp, tst_chartform.cpp): OverviewTabWidget hat
// keinen eigenen Presenter — es ist ein wiederverwendbares, in sich
// geschlossenes Anzeige-Widget (Spaltendefinitionen + Populate-Callbacks als
// reine Funktionsargumente, siehe ARCHITECTURE.md, "OverviewTabWidget-
// Details"). Getestet wird daher die echte QWidget-Instanz direkt, analog zu
// tst_backupsettingsform.cpp (dort ebenfalls ein echter QDialog statt einer
// Fake-Schicht). Keine Datenbank nötig.

#include <QtTest>
#include <QApplication>
#include <QTableWidget>
#include <QTabBar>
#include <QHeaderView>
#include <QStringList>

#include "../../app/widgets/OverviewTabWidget.h"

// ─────────────────────────────────────────────────────────────────────────────
// TestOverviewTabWidget
// ─────────────────────────────────────────────────────────────────────────────

class TestOverviewTabWidget : public QObject
{
    Q_OBJECT

private:
    /**
     * @brief Baut eine typische Instanz mit den Jahren 2025, 2024 (absteigend,
     * wie im Projekt üblich) und je einer Datenzeile pro Tab:
     * - Übersicht-Tab: zwei Zeilen (eine pro Jahr), Spalte 0 trägt das Jahr
     *   als Qt::UserRole (siehe OverviewTabWidget::onUebersichtRowActivated()).
     * - Jahres-Tabs: je eine Zeile, Spalte 0 trägt eine synthetische GUID als
     *   Qt::UserRole (siehe OverviewTabWidget::onJahresRowActivated()).
     */
    static void populateSample(OverviewTabWidget& w)
    {
        w.populateOverview(
            {2025, 2024},
            QStringLiteral("Übersicht"),
            {QStringLiteral("Jahr"), QStringLiteral("Summe")},
            {-1, 100},
            [](QTableWidget* data) {
                data->setRowCount(2);
                const int years[2] = {2025, 2024};
                for (int r = 0; r < 2; ++r) {
                    auto* item = new QTableWidgetItem(QString::number(years[r]));
                    item->setData(Qt::UserRole, years[r]);
                    data->setItem(r, 0, item);
                    data->setItem(r, 1, new QTableWidgetItem(QStringLiteral("100,00")));
                }
            },
            [](QTableWidget* footer) {
                footer->setItem(0, 0, new QTableWidgetItem(QStringLiteral("Gesamt")));
                footer->setItem(0, 1, new QTableWidgetItem(QStringLiteral("200,00")));
            },
            {QStringLiteral("Datum"), QStringLiteral("Wert")},
            {-1, 100},
            [](int year) { return QString::number(year); },
            [](int year, QTableWidget* data) {
                data->setRowCount(1);
                auto* item = new QTableWidgetItem(QStringLiteral("01.01."));
                item->setData(Qt::UserRole, QStringLiteral("guid-%1").arg(year));
                data->setItem(0, 0, item);
                data->setItem(0, 1, new QTableWidgetItem(QStringLiteral("50,00")));
            },
            [](int /*year*/, QTableWidget* footer) {
                footer->setItem(0, 0, new QTableWidgetItem(QStringLiteral("Gesamt")));
                footer->setItem(0, 1, new QTableWidgetItem(QStringLiteral("50,00")));
            });
    }

    /** Holt die dataTable-Property eines Tab-Containers (siehe buildFrozenTable()). */
    static QTableWidget* dataTableOf(QWidget* container)
    {
        if (!container)
            return nullptr;
        return qobject_cast<QTableWidget*>(
            container->property("dataTable").value<QObject*>());
    }

private slots:

    // ── count() / widget() / tabText() ──────────────────────────────────────

    void test_populateOverview_countWidgetTabText()
    {
        OverviewTabWidget w;
        populateSample(w);

        QCOMPARE(w.count(), 3); // Übersicht + 2 Jahre
        QVERIFY(w.widget(0) != nullptr);
        QVERIFY(w.widget(1) != nullptr);
        QVERIFY(w.widget(2) != nullptr);
        QVERIFY(w.widget(3) == nullptr); // außerhalb des Bereichs

        QCOMPARE(w.tabText(0), QStringLiteral("Übersicht"));
        QCOMPARE(w.tabText(1), QStringLiteral("2025")); // erstes Jahr in der übergebenen Reihenfolge
        QCOMPARE(w.tabText(2), QStringLiteral("2024"));
    }

    void test_populateOverview_emptyYears_leavesNoTabs()
    {
        OverviewTabWidget w;
        w.populateOverview(
            {}, QStringLiteral("Übersicht"), {}, {},
            [](QTableWidget*) {}, [](QTableWidget*) {},
            {}, {}, [](int y) { return QString::number(y); },
            [](int, QTableWidget*) {}, [](int, QTableWidget*) {});

        QCOMPARE(w.count(), 0);
    }

    // ── setCurrentIndex() / currentIndex() ──────────────────────────────────

    void test_setCurrentIndex_switchesStackAndBothBars()
    {
        OverviewTabWidget w;
        populateSample(w);

        auto* pinnedBar = w.findChild<QTabBar*>(QStringLiteral("pinnedBar"));
        auto* yearsBar  = w.findChild<QTabBar*>(QStringLiteral("yearsBar"));
        if (!pinnedBar || !yearsBar) QFAIL("Tab-Bars nicht gefunden");

        w.setCurrentIndex(2); // zweiter Jahres-Tab (2024)
        QCOMPARE(w.currentIndex(), 2);
        QCOMPARE(yearsBar->currentIndex(), 1);
        QCOMPARE(pinnedBar->currentIndex(), 0); // unverändert, einziger Tab

        w.setCurrentIndex(0);
        QCOMPARE(w.currentIndex(), 0);
        QCOMPARE(pinnedBar->currentIndex(), 0);
    }

    void test_setCurrentIndex_outOfRange_isIgnored()
    {
        OverviewTabWidget w;
        populateSample(w);

        w.setCurrentIndex(1);
        w.setCurrentIndex(99); // außerhalb — muss ignoriert werden
        QCOMPARE(w.currentIndex(), 1);

        w.setCurrentIndex(-1);
        QCOMPARE(w.currentIndex(), 1);
    }

    // ── Regressionstests für die Bugfixes vom 14.07.2026 (Nessies Feedback
    // nach dem ersten Build, siehe ARCHITECTURE.md, "OverviewTabWidget-
    // Details") ─────────────────────────────────────────────────────────────

    /**
     * Vor dem Fix: m_pinnedBar hat nur genau einen Tab (Index immer 0), daher
     * feuerte QTabBar::currentChanged bei einem erneuten Klick darauf nicht —
     * der Übersicht-Tab ließ sich nach einem Sprung in einen Jahres-Tab nicht
     * mehr zurück anwählen. Fix: tabBarClicked statt currentChanged.
     */
    void test_pinnedTabClick_afterYearTabClick_returnsToOverview()
    {
        OverviewTabWidget w;
        populateSample(w);

        auto* pinnedBar = w.findChild<QTabBar*>(QStringLiteral("pinnedBar"));
        auto* yearsBar  = w.findChild<QTabBar*>(QStringLiteral("yearsBar"));
        if (!pinnedBar || !yearsBar) QFAIL("Tab-Bars nicht gefunden");

        // Klick auf den zweiten Jahres-Tab (echtes Nutzer-Klick-Signal, kein
        // programmatischer setCurrentIndex()-Aufruf).
        yearsBar->tabBarClicked(1);
        QCOMPARE(w.currentIndex(), 2);

        // Klick auf den Übersicht-Tab — vor dem Bugfix passierte hier nichts.
        pinnedBar->tabBarClicked(0);
        QCOMPARE(w.currentIndex(), 0);
    }

    /** Spiegelbildlicher Fall: ein Jahres-Tab, der schon vorher als "aktuell"
     *  in m_yearsBar galt, muss trotzdem per Klick wieder anwählbar sein. */
    void test_yearsBarClick_sameYearAsBefore_stillSwitchesBack()
    {
        OverviewTabWidget w;
        populateSample(w);

        auto* yearsBar = w.findChild<QTabBar*>(QStringLiteral("yearsBar"));
        if (!yearsBar) QFAIL("yearsBar nicht gefunden");

        yearsBar->tabBarClicked(0); // 2025
        QCOMPARE(w.currentIndex(), 1);

        w.setCurrentIndex(0); // zurück zur Übersicht, z.B. wie
                              // onUebersichtRowActivated() es tun würde

        // yearsBar's eigener currentIndex ist immer noch 0 (2025) — ein Klick
        // darauf muss trotzdem wieder umschalten.
        yearsBar->tabBarClicked(0);
        QCOMPARE(w.currentIndex(), 1);
    }

    /** Spaltenköpfe müssen von Anfang an fett sein, unabhängig von jeder
     *  Selektion — vorher erschienen sie erst fett, sobald Qt's Style
     *  (highlightSections) die zur Selektion gehörige Kopfspalte hervorhob. */
    void test_headerColumns_alwaysBold_regardlessOfSelection()
    {
        OverviewTabWidget w;
        populateSample(w);

        auto* dataTable = dataTableOf(w.widget(0));
        if (!dataTable) QFAIL("dataTable nicht gefunden");

        QVERIFY(dataTable->horizontalHeader()->font().bold());
        QVERIFY(!dataTable->horizontalHeader()->highlightSections());

        dataTable->selectRow(0);
        QVERIFY(dataTable->horizontalHeader()->font().bold());
    }

    // ── Klick-Navigation Übersicht → Jahr ────────────────────────────────────

    void test_uebersichtRowClick_jumpsToMatchingYearTab()
    {
        OverviewTabWidget w;
        populateSample(w);
        w.setCurrentIndex(0);

        auto* dataTable = dataTableOf(w.widget(0));
        if (!dataTable) QFAIL("dataTable nicht gefunden");

        // Zeile 1 in der Übersicht-Tabelle trägt Jahr 2024 (siehe populateSample()).
        dataTable->cellClicked(1, 0);

        QCOMPARE(w.currentIndex(), 2); // Jahres-Tab für 2024
    }

    void test_uebersichtRowClick_unknownYear_doesNothing()
    {
        OverviewTabWidget w;
        populateSample(w);
        w.setCurrentIndex(0);

        auto* dataTable = dataTableOf(w.widget(0));
        if (!dataTable) QFAIL("dataTable nicht gefunden");

        // Künstliche Zeile mit einem Jahr, für das es keinen Tab gibt.
        dataTable->setRowCount(3);
        auto* item = new QTableWidgetItem(QStringLiteral("1999"));
        item->setData(Qt::UserRole, 1999);
        dataTable->setItem(2, 0, item);

        dataTable->cellClicked(2, 0);

        QCOMPARE(w.currentIndex(), 0); // unverändert
    }

    // ── clear() ──────────────────────────────────────────────────────────────

    void test_clear_removesAllTabsAndResetsCount()
    {
        OverviewTabWidget w;
        populateSample(w);
        QCOMPARE(w.count(), 3);

        w.clear();

        QCOMPARE(w.count(), 0);
        auto* pinnedBar = w.findChild<QTabBar*>(QStringLiteral("pinnedBar"));
        auto* yearsBar  = w.findChild<QTabBar*>(QStringLiteral("yearsBar"));
        if (!pinnedBar || !yearsBar) QFAIL("Tab-Bars nicht gefunden");
        QCOMPARE(pinnedBar->count(), 0);
        QCOMPARE(yearsBar->count(), 0);
    }

    /** Ein erneutes populateOverview() muss die alten Tabs vollständig
     *  ersetzen, nicht an sie anhängen (Regressionstest für den kompletten
     *  Rebuild in populateOverview()/clear()). */
    void test_populateOverview_calledTwice_replacesOldTabs()
    {
        OverviewTabWidget w;
        populateSample(w);
        QCOMPARE(w.count(), 3);

        w.populateOverview(
            {2020}, QStringLiteral("Übersicht"),
            {QStringLiteral("Jahr")}, {-1},
            [](QTableWidget* data) { data->setRowCount(0); },
            [](QTableWidget*) {},
            {QStringLiteral("Datum")}, {-1},
            [](int year) { return QString::number(year); },
            [](int, QTableWidget* data) { data->setRowCount(0); },
            [](int, QTableWidget*) {});

        QCOMPARE(w.count(), 2); // Übersicht + genau 1 Jahr
        QCOMPARE(w.tabText(1), QStringLiteral("2020"));
    }
};

QTEST_MAIN(TestOverviewTabWidget)
#include "tst_overviewtabwidget.moc"

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
#include "../../app/widgets/GridStyle.h"

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
     *
     * Keine Dokument-Spalte konfiguriert (jahresDocColumn bleibt beim
     * Default -1) — für Tests, die eine Dokument-Spalte brauchen, siehe
     * populateSampleWithDoc() weiter unten.
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
                auto* item = new QTableWidgetItem(QStringLiteral("01.01.%1").arg(year));
                item->setData(Qt::UserRole, QStringLiteral("guid-%1").arg(year));
                data->setItem(0, 0, item);
                data->setItem(0, 1, new QTableWidgetItem(QStringLiteral("50,00")));
            },
            [](int /*year*/, QTableWidget* footer) {
                footer->setItem(0, 0, new QTableWidgetItem(QStringLiteral("Gesamt")));
                footer->setItem(0, 1, new QTableWidgetItem(QStringLiteral("50,00")));
            });
    }

    /**
     * @brief Wie populateSample(), aber mit genau einem Jahr (2025), einer
     * dritten Spalte als konfigurierter Dokument-Spalte (jahresDocColumn = 2)
     * und @p docPath als Dokumentpfad (Qt::UserRole) auf der einzigen
     * Jahres-Tab-Zeile — für die rowActivatedWithDocument()-/
     * documentActivated()-Tests unten. Ein leerer @p docPath simuliert eine
     * Zeile ohne Dokument.
     */
    static void populateSampleWithDoc(OverviewTabWidget& w, const QString& docPath)
    {
        w.populateOverview(
            {2025},
            QStringLiteral("Übersicht"),
            {QStringLiteral("Jahr"), QStringLiteral("Summe")},
            {-1, 100},
            [](QTableWidget* data) {
                data->setRowCount(1);
                auto* item = new QTableWidgetItem(QStringLiteral("2025"));
                item->setData(Qt::UserRole, 2025);
                data->setItem(0, 0, item);
                data->setItem(0, 1, new QTableWidgetItem(QStringLiteral("100,00")));
            },
            [](QTableWidget* footer) {
                footer->setItem(0, 0, new QTableWidgetItem(QStringLiteral("Gesamt")));
                footer->setItem(0, 1, new QTableWidgetItem(QStringLiteral("100,00")));
            },
            {QStringLiteral("Datum"), QStringLiteral("Wert"), QString()},
            {-1, 100, 36},
            [](int year) { return QString::number(year); },
            [docPath](int /*year*/, QTableWidget* data) {
                data->setRowCount(1);
                auto* item = new QTableWidgetItem(QStringLiteral("01.01.2025"));
                item->setData(Qt::UserRole, QStringLiteral("guid-2025"));
                data->setItem(0, 0, item);
                data->setItem(0, 1, new QTableWidgetItem(QStringLiteral("50,00")));
                auto* docItem = new QTableWidgetItem;
                docItem->setData(Qt::UserRole, docPath);
                data->setItem(0, 2, docItem);
            },
            [](int /*year*/, QTableWidget* footer) {
                footer->setItem(0, 0, new QTableWidgetItem(QStringLiteral("Gesamt")));
                footer->setItem(0, 1, new QTableWidgetItem(QStringLiteral("50,00")));
            },
            /*jahresDocColumn=*/2);
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

    // ── Grid-Selektionsfarbe (Feature 29.07.2026, Nessies Vorgabe: wie im
    // Haupt-Grid, konsistent in allen Grids) ────────────────────────────────

    /** Sowohl der Übersicht- als auch jeder Jahres-Tab müssen die einheitliche
     *  Blau/Gelb-Selektionsfarbe (GridStyle) auf ihrer dataTable tragen — die
     *  footerTable bleibt unangetastet, da nicht selektierbar. */
    void test_populateOverview_dataTablesHaveGridSelectionStyle()
    {
        OverviewTabWidget w;
        populateSample(w);

        for (int i = 0; i < w.count(); ++i) {
            auto* dataTable = dataTableOf(w.widget(i));
            if (!dataTable) QFAIL("dataTable nicht gefunden");
            QVERIFY(dataTable->styleSheet().contains(GridStyle::kSelectionBackground));
            QVERIFY(dataTable->styleSheet().contains(GridStyle::kSelectionForeground));
        }
    }

    /** footerTable ist nicht selektierbar (NoSelection) und bekommt daher
     *  bewusst kein Selektions-Stylesheet. */
    void test_populateOverview_footerTableHasNoGridSelectionStyle()
    {
        OverviewTabWidget w;
        populateSample(w);

        auto* container = w.widget(1);
        if (!container) QFAIL("Container nicht gefunden");
        auto* footerTable = qobject_cast<QTableWidget*>(
            container->property("footerTable").value<QObject*>());
        if (!footerTable) QFAIL("footerTable nicht gefunden");

        QVERIFY(!footerTable->styleSheet().contains(GridStyle::kSelectionBackground));
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

    // ── rowActivatedWithDocument() (neu, 19.07.2026, siehe ARCHITECTURE.md,
    // "ShareDetailsForm: Dokument-Vorschau per Zeilenauswahl statt
    // Doppelklick") / documentActivated() (Doppelklick, unverändert seit
    // 13.07.2026 — wird weiterhin von ViewBuyEdit/ViewSaleEdit/
    // ViewDividendEdit/ViewBrokerageEdit verbunden) ─────────────────────────

    /** Klick auf eine beliebige Spalte einer Jahres-Tab-Zeile (nicht die
     *  Dokument-Spalte) muss trotzdem rowActivatedWithDocument() mit dem
     *  Dokumentpfad AUS der Dokument-Spalte derselben Zeile feuern. */
    void test_jahresRowClick_withDocColumn_emitsRowActivatedWithDocumentAndPath()
    {
        OverviewTabWidget w;
        populateSampleWithDoc(w, QStringLiteral("/tmp/beleg.pdf"));

        auto* dataTable = dataTableOf(w.widget(1)); // Jahres-Tab 2025
        if (!dataTable) QFAIL("dataTable nicht gefunden");

        QSignalSpy spy(&w, &OverviewTabWidget::rowActivatedWithDocument);
        dataTable->cellClicked(0, 1); // Spalte 1 ("Wert"), nicht die Dokument-Spalte (2)

        QCOMPARE(spy.count(), 1);
        const QList<QVariant> args = spy.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("guid-2025"));
        QCOMPARE(args.at(1).toString(), QStringLiteral("/tmp/beleg.pdf"));
    }

    /** Zeile ohne Dokument (leerer Pfad in der Dokument-Spalte) → leerer
     *  Pfad im Signal, aber das Signal feuert trotzdem. */
    void test_jahresRowClick_rowWithoutDocument_emitsEmptyPath()
    {
        OverviewTabWidget w;
        populateSampleWithDoc(w, QString());

        auto* dataTable = dataTableOf(w.widget(1));
        if (!dataTable) QFAIL("dataTable nicht gefunden");

        QSignalSpy spy(&w, &OverviewTabWidget::rowActivatedWithDocument);
        dataTable->cellClicked(0, 0);

        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.takeFirst().at(1).toString().isEmpty());
    }

    /** populateSample() (ohne jahresDocColumn-Argument) lässt die Dokument-
     *  Spalte auf ihrem Default (-1, keine Dokument-Spalte konfiguriert) —
     *  rowActivatedWithDocument() muss trotzdem feuern, nur mit leerem Pfad. */
    void test_jahresRowClick_noDocColumnConfigured_emitsEmptyPath()
    {
        OverviewTabWidget w;
        populateSample(w);

        auto* dataTable = dataTableOf(w.widget(1));
        if (!dataTable) QFAIL("dataTable nicht gefunden");

        QSignalSpy spy(&w, &OverviewTabWidget::rowActivatedWithDocument);
        dataTable->cellClicked(0, 0);

        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.takeFirst().at(1).toString().isEmpty());
    }

    /** Klick auf eine Zeile muss weiterhin ganz normal auch rowActivated()
     *  auslösen (unverändert) — rowActivatedWithDocument() ist rein additiv. */
    void test_jahresRowClick_stillEmitsPlainRowActivated()
    {
        OverviewTabWidget w;
        populateSampleWithDoc(w, QStringLiteral("/tmp/beleg.pdf"));

        auto* dataTable = dataTableOf(w.widget(1));
        if (!dataTable) QFAIL("dataTable nicht gefunden");

        QSignalSpy spy(&w, &OverviewTabWidget::rowActivated);
        dataTable->cellClicked(0, 0);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toString(), QStringLiteral("guid-2025"));
    }

    /** Regression: documentActivated() (Doppelklick auf die Dokument-Spalte)
     *  muss unverändert weiter funktionieren — beim Nachziehen von
     *  rowActivatedWithDocument() wurde dieser Mechanismus versehentlich kurz
     *  entfernt (brach den Build von ViewBuyEdit & Co.), seither bewusst als
     *  Ergänzung statt als Ersatz umgesetzt, siehe OverviewTabWidget.h. */
    void test_documentColumnDoubleClick_stillEmitsDocumentActivated()
    {
        OverviewTabWidget w;
        populateSampleWithDoc(w, QStringLiteral("/tmp/beleg.pdf"));

        auto* dataTable = dataTableOf(w.widget(1));
        if (!dataTable) QFAIL("dataTable nicht gefunden");

        QSignalSpy spy(&w, &OverviewTabWidget::documentActivated);
        dataTable->cellDoubleClicked(0, 2); // Dokument-Spalte (jahresDocColumn = 2)

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toString(), QStringLiteral("/tmp/beleg.pdf"));
    }

    /** Doppelklick auf die Dokument-Spalte einer Zeile ohne Dokument darf
     *  nicht feuern (unverändert seit 13.07.2026). */
    void test_documentColumnDoubleClick_emptyPath_doesNotEmitDocumentActivated()
    {
        OverviewTabWidget w;
        populateSampleWithDoc(w, QString());

        auto* dataTable = dataTableOf(w.widget(1));
        if (!dataTable) QFAIL("dataTable nicht gefunden");

        QSignalSpy spy(&w, &OverviewTabWidget::documentActivated);
        dataTable->cellDoubleClicked(0, 2);

        QCOMPARE(spy.count(), 0);
    }

    /** Doppelklick außerhalb der Dokument-Spalte darf documentActivated()
     *  nicht auslösen (unverändert seit 13.07.2026). */
    void test_documentColumnDoubleClick_wrongColumn_doesNotEmitDocumentActivated()
    {
        OverviewTabWidget w;
        populateSampleWithDoc(w, QStringLiteral("/tmp/beleg.pdf"));

        auto* dataTable = dataTableOf(w.widget(1));
        if (!dataTable) QFAIL("dataTable nicht gefunden");

        QSignalSpy spy(&w, &OverviewTabWidget::documentActivated);
        dataTable->cellDoubleClicked(0, 0); // Datum-Spalte, nicht die Dokument-Spalte

        QCOMPARE(spy.count(), 0);
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

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// Tests für PresenterCalculationOverview — den Tab "Depot- vs. Marktwert"
// (ergänzt 27.10.2026, siehe ARCHITECTURE.md, "Tab Depot- vs. Marktwert").
//
// Fake-View/Fake-Model-Paar wie tst_portfoliochartform: keine Datenbank,
// kein QWidget. Geprüft wird, was der Presenter in Liste und Detail schreibt
// und wie er die Auswahl über ein Neuladen hinweg hält. Die Zahlen selbst
// prüft tst_calculationbreakdown. Seit v1.31.0 (18.11.2026) auch die Gruppen
// zum Aufklappen: Reihenfolge, Kurzinfo und welche aufgeklappt sind.

#include <QtTest>
#include <QCoreApplication>

#include "../../app/forms/CalculationOverviewForm/IViewCalculationOverview.h"
#include "../../app/forms/CalculationOverviewForm/IModelCalculationOverview.h"
#include "../../app/forms/CalculationOverviewForm/PresenterCalculationOverview.h"

// ── Fakes ─────────────────────────────────────────────────────────────────────

class FakeViewCalculationOverview : public IViewCalculationOverview
{
public:
    QString selectedKey() const override { return selected; }
    void setEntries(const QList<CalculationOverviewEntry>& e) override { entries = e; }
    void setSelectedKey(const QString& key) override { selected = key; }
    void setSummary(const QString& text) override { summary = text; }
    void showDetail(const CalculationOverviewDetail& d) override { detail = d; ++detailCount; }

    QString                         selected;
    QList<CalculationOverviewEntry> entries;
    QString                         summary;
    CalculationOverviewDetail       detail;
    int                             detailCount = 0;
};

class FakeModelCalculationOverview : public IModelCalculationOverview
{
public:
    CalculationReportInput load() const override { ++loadCount; return input; }

    CalculationReportInput input;
    mutable int            loadCount = 0;
};

// ── Tests ─────────────────────────────────────────────────────────────────────

class TestCalculationOverviewForm : public QObject
{
    Q_OBJECT

private:
    static QDate d(int year, int month, int day) { return QDate(year, month, day); }

    /// 12 Stück à 50,00, Kurs 62,00 — Tabelle und Chart passen.
    static CalculationReportShare share(const QString& guid, const QString& name, double volume = 12.0)
    {
        CalculationReportShare s;
        s.share = ShareObject(guid, QStringLiteral("WKN-") + guid, QString(), name);
        s.share.setCurPrice(62.0);

        ShareValues& v = s.values;
        v.volume                   = volume;
        v.curPrice                 = 62.0;
        v.purchaseValue            = volume * 50.0;
        v.curValue                 = volume * 62.0;
        v.profitLoss               = volume * 12.0;
        v.completePurchaseMarket   = 600.0;
        v.completeCurValueMarket   = 744.0;
        v.completeProfitLossMarket = 144.0;
        v.purchaseValueFinal       = volume * 50.0;
        v.profitLossFinal          = volume * 12.0;
        v.completePurchase         = 600.0;
        v.completeCurValue         = 744.0;
        v.completeProfitLoss       = 144.0;

        PortfolioSharePoint& c = s.chart;
        c.date              = d(2026, 10, 22);
        c.name              = name;
        c.volume            = volume;
        c.price             = 62.0;
        c.holdingsValue     = volume * 62.0;
        c.purchaseValueHeld = volume * 50.0;
        c.realizedGain      = 144.0 - volume * 12.0;
        c.marketValue       = volume * 62.0;
        c.purchaseValueHeldFinal = volume * 50.0; // zweite Linien (01.11.2026)
        c.marketValueComplete    = 744.0;
        return s;
    }

    static CalculationReportInput inputFor(const QList<CalculationReportShare>& shares)
    {
        CalculationReportInput input;
        input.created = QDateTime(d(2026, 10, 27), QTime(21, 30));
        input.shares  = shares;
        PortfolioSeriesPoint& t = input.chartTotal;
        t.date = d(2026, 10, 22);
        for (const CalculationReportShare& s : shares) {
            t.holdingsValue += s.chart.holdingsValue;
            t.marketValue   += s.chart.marketValue;
            t.development   += CalculationBreakdown::chartDevelopment(s.chart);
            t.developmentHeld     += CalculationBreakdown::chartDevelopmentHeld(s.chart);
            t.marketValueComplete += s.chart.marketValueComplete;
        }
        return input;
    }

    // ── Bausteine für die Gruppen (ergänzt 18.11.2026) ────────────────────

    static QString iso(const QDate& date)
    {
        return QDateTime(date, QTime(10, 0)).toString(Qt::ISODate);
    }

    static BuyObject buy(const QString& guid, const QDate& date, double volume, double price,
                         double volumeSold = 0.0)
    {
        return BuyObject(guid, QStringLiteral("g1"), QStringLiteral("D1"),
                         QStringLiteral("ord-") + guid, iso(date), volume, volumeSold, price);
    }

    static SaleObject sale(const QString& guid, const QDate& date, double volume, double price)
    {
        return SaleObject(guid, QStringLiteral("g1"), QStringLiteral("D1"),
                          QStringLiteral("ord-") + guid, iso(date), volume, price, {});
    }

    /// Stimmige Aktie mit dem passenden Kauf: 12 Stück à 50,00.
    static CalculationReportShare shareWithBuy()
    {
        CalculationReportShare s = share(QStringLiteral("g1"), QStringLiteral("Aktie A"));
        s.buys = { buy(QStringLiteral("b1"), d(2026, 1, 10), 12.0, 50.0) };
        return s;
    }

    /**
     * Bestand 3 Stück, Tabelle und Chart passen. Zwei Käufe (10 und 5), ein
     * Verkauf über 12: nach FIFO 10 und 2, gespeichert absichtlich 8 und 4.
     */
    static CalculationReportShare fifoShare()
    {
        CalculationReportShare s = share(QStringLiteral("g1"), QStringLiteral("Aktie A"), 3.0);
        s.buys  = { buy(QStringLiteral("b1"), d(2026, 1, 10), 10.0, 50.0, 8.0),
                    buy(QStringLiteral("b2"), d(2026, 2, 10),  5.0, 60.0, 4.0) };
        s.sales = { sale(QStringLiteral("s1"), d(2026, 3, 1), 12.0, 70.0) };
        return s;
    }

    static const CalculationOverviewGroup& group(const CalculationOverviewDetail& detail,
                                                 CalculationOverviewGroup::Kind kind)
    {
        for (const CalculationOverviewGroup& g : detail.groups) {
            if (g.kind == kind)
                return g;
        }
        static const CalculationOverviewGroup none;
        return none;
    }

private slots:

    void test_loadAndDisplay_selectsTotalByDefault()
    {
        FakeViewCalculationOverview  view;
        FakeModelCalculationOverview model;
        model.input = inputFor({ share(QStringLiteral("g1"), QStringLiteral("Aktie A")) });

        PresenterCalculationOverview presenter(&view, &model);
        presenter.loadAndDisplay();

        QCOMPARE(view.selected, PresenterCalculationOverview::totalKey());
        QCOMPARE(view.detail.title, QStringLiteral("Gesamt / Footer"));
        QCOMPARE(view.detail.comparison.size(), 10);
        QCOMPARE(view.detail.checks.size(), 5); // seit 01.11.2026 mit den zweiten Linien
        QVERIFY(view.detail.findings.isEmpty());
    }

    void test_entries_totalFirstThenHeldAndSold()
    {
        FakeViewCalculationOverview  view;
        FakeModelCalculationOverview model;
        model.input = inputFor({ share(QStringLiteral("g1"), QStringLiteral("Aktie A")),
                                 share(QStringLiteral("g2"), QStringLiteral("Aktie B"), 0.0) });

        PresenterCalculationOverview presenter(&view, &model);
        presenter.loadAndDisplay();

        using G = CalculationOverviewEntry::Group;
        QCOMPARE(view.entries.size(), 3);
        QVERIFY(view.entries.at(0).group == G::Total);
        QVERIFY(view.entries.at(1).group == G::Held);
        QVERIFY(view.entries.at(2).group == G::Sold);
        QCOMPARE(view.entries.at(1).key, QStringLiteral("g1"));
        QCOMPARE(view.entries.at(1).title, QStringLiteral("Aktie A (WKN-g1)"));
        QVERIFY(view.summary.contains(QStringLiteral("2 Aktien (1 im Bestand, 1 verkauft)")));
        QVERIFY(view.summary.contains(QStringLiteral("27.10.2026 21:30")));
    }

    void test_entries_deviationIsMarked()
    {
        CalculationReportShare s = share(QStringLiteral("g1"), QStringLiteral("Aktie A"));
        s.chart.volume = 10.0; // Bestand weicht ab

        const QList<CalculationOverviewEntry> entries =
            PresenterCalculationOverview::buildEntries(inputFor({ s }));
        QVERIFY(entries.at(1).tone == CalculationOverviewTone::Deviation);
        QVERIFY(entries.at(0).tone == CalculationOverviewTone::Deviation); // Gesamt sieht sie auch
        QVERIFY2(entries.at(1).toolTip.contains(QStringLiteral("1 Abweichung")),
                 qPrintable(entries.at(1).toolTip));
    }

    void test_selectShare_showsComparisonAndChecks()
    {
        FakeViewCalculationOverview  view;
        FakeModelCalculationOverview model;
        model.input = inputFor({ share(QStringLiteral("g1"), QStringLiteral("Aktie A")) });

        PresenterCalculationOverview presenter(&view, &model);
        presenter.loadAndDisplay();
        view.selected = QStringLiteral("g1");
        presenter.onSelectionChanged();

        QCOMPARE(view.detail.title, QStringLiteral("Aktie A"));
        QCOMPARE(view.detail.comparison.size(), 14);
        QCOMPARE(view.detail.checks.size(), 7); // seit 01.11.2026 mit den zweiten Linien
        for (const CalculationOverviewCheckRow& c : view.detail.checks)
            QVERIFY2(c.tone == CalculationOverviewTone::Ok, qPrintable(c.label));

        // "Fließt ein in" der Einzahlung nennt beide Tabs und den Footer,
        // eine Stelle je Zeile.
        QCOMPARE(view.detail.comparison.at(0).usedIn,
                 QStringLiteral("beide Tabs: Spalte \"Einzahlung / Marktwert\" oben\n"
                                "→ Footer \"Einzahlung (gesamt)\" (Summe)"));
    }

    void test_selectGroupHeader_keepsDetail()
    {
        FakeViewCalculationOverview  view;
        FakeModelCalculationOverview model;
        model.input = inputFor({ share(QStringLiteral("g1"), QStringLiteral("Aktie A")) });

        PresenterCalculationOverview presenter(&view, &model);
        presenter.loadAndDisplay();
        const int before = view.detailCount;
        view.selected.clear(); // Gruppenüberschrift hat keinen Schlüssel
        presenter.onSelectionChanged();
        QCOMPARE(view.detailCount, before);
    }

    void test_reload_keepsSelectionAndReadsModelAgain()
    {
        FakeViewCalculationOverview  view;
        FakeModelCalculationOverview model;
        model.input = inputFor({ share(QStringLiteral("g1"), QStringLiteral("Aktie A")) });

        PresenterCalculationOverview presenter(&view, &model);
        presenter.loadAndDisplay();
        view.selected = QStringLiteral("g1");
        presenter.onSelectionChanged();

        presenter.reload();
        QCOMPARE(model.loadCount, 2);
        QCOMPARE(view.selected, QStringLiteral("g1"));
        QCOMPARE(view.detail.title, QStringLiteral("Aktie A"));
    }

    void test_reload_fallsBackToTotalWhenShareIsGone()
    {
        FakeViewCalculationOverview  view;
        FakeModelCalculationOverview model;
        model.input = inputFor({ share(QStringLiteral("g1"), QStringLiteral("Aktie A")) });

        PresenterCalculationOverview presenter(&view, &model);
        presenter.loadAndDisplay();
        view.selected = QStringLiteral("g1");

        model.input = inputFor({ share(QStringLiteral("g2"), QStringLiteral("Aktie B")) });
        presenter.reload();
        QCOMPARE(view.selected, PresenterCalculationOverview::totalKey());
        QCOMPARE(view.detail.title, QStringLiteral("Gesamt / Footer"));
    }

    void test_shareWithoutDailyValues_noChecksButNote()
    {
        CalculationReportShare s = share(QStringLiteral("g1"), QStringLiteral("Aktie A"));
        s.chartExcluded = true;
        s.chart = PortfolioSharePoint{};

        const CalculationOverviewDetail detail = PresenterCalculationOverview::buildShareDetail(s);
        QVERIFY(detail.checks.isEmpty());
        QCOMPARE(detail.checkNotes,
                 QStringList({ QStringLiteral("Keine Tageswerte — die Aktie fehlt in beiden Charts.") }));
        QCOMPARE(detail.findings.size(), 1);
        QVERIFY(detail.findings.at(0).tone == CalculationOverviewTone::Deviation);
    }

    void test_priceDifference_isHintInChecksAndFindings()
    {
        CalculationReportShare s = share(QStringLiteral("g1"), QStringLiteral("Aktie A"));
        s.chart.price = 61.5;

        const CalculationOverviewDetail detail = PresenterCalculationOverview::buildShareDetail(s);
        QCOMPARE(detail.checks.at(1).result, QStringLiteral("Hinweis"));
        QVERIFY(detail.checks.at(1).tone == CalculationOverviewTone::Hint);
        QCOMPARE(detail.findings.size(), 1);
        QVERIFY(detail.findings.at(0).tone == CalculationOverviewTone::Hint);
    }

    void test_comparisonRow_waysAndDash()
    {
        CalculationBreakdownRow same;
        same.label = QStringLiteral("Kurswert");
        same.hasMarket = same.hasDepot = true;
        same.market = same.depot = 744.0;
        same.marketWay = same.depotWay = QStringLiteral("12 × 62");
        same.usedIn = { QStringLiteral("A"), QStringLiteral("B") };

        const CalculationOverviewComparisonRow a = PresenterCalculationOverview::toComparisonRow(2, same);
        QCOMPARE(a.number, QStringLiteral("2"));
        QCOMPARE(a.way, QStringLiteral("beide: 12 × 62"));
        QCOMPARE(a.usedIn, QStringLiteral("A\nB"));
        QCOMPARE(a.diff, QLocale().toString(0.0, 'f', 2) + QStringLiteral(" €"));

        CalculationBreakdownRow depotOnly;
        depotOnly.label = QStringLiteral("Kosten");
        depotOnly.hasDepot = true;
        depotOnly.depot = 10.0;
        depotOnly.marketWay = QStringLiteral("nicht enthalten");
        depotOnly.depotWay = QStringLiteral("alle Kosten");

        const CalculationOverviewComparisonRow b = PresenterCalculationOverview::toComparisonRow(5, depotOnly);
        QCOMPARE(b.market, QStringLiteral("–"));
        QCOMPARE(b.diff,   QStringLiteral("–"));
        QCOMPARE(b.way, QStringLiteral("Marktwert: nicht enthalten\nDepotwert: alle Kosten"));
        // Der Tooltip nennt nur die Sicht, in der der Wert vorkommt.
        QCOMPARE(b.toolTip, QStringLiteral("Depotwert: alle Kosten"));
    }

    void test_totalDetail_withoutChartPoint_showsNote()
    {
        CalculationReportInput input = inputFor({ share(QStringLiteral("g1"), QStringLiteral("Aktie A")) });
        input.chartTotal = PortfolioSeriesPoint{};

        const CalculationOverviewDetail detail = PresenterCalculationOverview::buildTotalDetail(input);
        QVERIFY(detail.checks.isEmpty());
        QCOMPARE(detail.checkNotes,
                 QStringList({ QStringLiteral("Kein Chartpunkt — keine Aktie mit Tageswerten.") }));
    }

    void test_emptyPortfolio_onlyTotalEntry()
    {
        FakeViewCalculationOverview  view;
        FakeModelCalculationOverview model;
        model.input = inputFor({});
        model.input.chartTotal = PortfolioSeriesPoint{};

        PresenterCalculationOverview presenter(&view, &model);
        presenter.loadAndDisplay();

        QCOMPARE(view.entries.size(), 1);
        QCOMPARE(view.detail.title, QStringLiteral("Gesamt / Footer"));
        QVERIFY(view.summary.contains(QStringLiteral("0 Aktien")));
    }

    // ── Gruppen zum Aufklappen (ergänzt 18.11.2026, v1.31.0) ─────────────

    void test_shareDetail_sixGroupsInOrder()
    {
        using K = CalculationOverviewGroup::Kind;
        const CalculationOverviewDetail detail = PresenterCalculationOverview::buildShareDetail(
            share(QStringLiteral("g1"), QStringLiteral("Aktie A")));

        QCOMPARE(detail.groups.size(), 6);
        const QList<K> kinds = { K::Buys, K::Sales, K::Dividends, K::Brokerages, K::Splits, K::Stock };
        for (int i = 0; i < kinds.size(); ++i)
            QVERIFY2(detail.groups.at(i).kind == kinds.at(i), qPrintable(QString::number(i)));

        QCOMPARE(detail.groups.at(0).title, QStringLiteral("Käufe (0)"));
        QCOMPARE(detail.groups.at(2).title, QStringLiteral("Dividenden (0)"));
        QCOMPARE(detail.groups.at(5).title, QStringLiteral("Bestand / FIFO"));

        // Leere Arten bleiben sichtbar, mit "keine" und ohne Tabelle.
        for (int i = 0; i < 5; ++i) {
            QCOMPARE(detail.groups.at(i).summary, QStringLiteral("keine"));
            QVERIFY(detail.groups.at(i).tables.isEmpty());
        }
    }

    void test_totalDetail_noTransactionGroups()
    {
        const CalculationOverviewDetail detail = PresenterCalculationOverview::buildTotalDetail(
            inputFor({ shareWithBuy() }));
        QVERIFY(detail.groups.isEmpty());
        QVERIFY(detail.checkExpanded);
        QCOMPARE(detail.checkSummary, QStringLiteral("alles ok"));
    }

    void test_defaultExpansion_onlyCheckOpen()
    {
        // Nessie, 17.11.2026, Punkt 7: der Abgleich ist offen, alles andere
        // zu, solange nichts abweicht.
        const CalculationOverviewDetail detail =
            PresenterCalculationOverview::buildShareDetail(shareWithBuy());

        QVERIFY(detail.checkExpanded);
        QCOMPARE(detail.checkSummary, QStringLiteral("alles ok"));
        QVERIFY(detail.checkTone == CalculationOverviewTone::Ok);
        QVERIFY(!detail.findingsExpanded);
        QCOMPARE(detail.findingsSummary, QStringLiteral("keine"));
        for (const CalculationOverviewGroup& g : detail.groups)
            QVERIFY2(!g.expanded, qPrintable(g.title));
        QCOMPARE(group(detail, CalculationOverviewGroup::Kind::Stock).summary,
                 QStringLiteral("Bestand ok"));
    }

    void test_findings_hintOnly_expandsFindings()
    {
        // Punkt 8a: offen, sobald es einen Eintrag gibt — auch nur ein Hinweis.
        CalculationReportShare s = shareWithBuy();
        s.chart.price = 61.5;

        const CalculationOverviewDetail detail = PresenterCalculationOverview::buildShareDetail(s);
        QVERIFY(detail.findingsExpanded);
        QCOMPARE(detail.findingsSummary, QStringLiteral("1 Hinweis"));
        QVERIFY(detail.findingsTone == CalculationOverviewTone::Hint);
        QCOMPARE(detail.checkSummary, QStringLiteral("1 Hinweis"));
    }

    void test_checkDeviation_keepsCheckOpen_summaryNamesIt()
    {
        CalculationReportShare s = shareWithBuy();
        s.chart.volume = 10.0; // Bestand weicht ab

        const CalculationOverviewDetail detail = PresenterCalculationOverview::buildShareDetail(s);
        QVERIFY(detail.checkExpanded);
        QVERIFY(detail.checkTone == CalculationOverviewTone::Deviation);
        QVERIFY2(detail.checkSummary.contains(QStringLiteral("Abweichung")),
                 qPrintable(detail.checkSummary));
        QVERIFY(detail.findingsExpanded);
    }

    void test_fifoDeviation_expandsStockGroup_notInFindings()
    {
        // Punkt 1b: die Gruppe geht bei einer Abweichung auf. Punkt 4b: die
        // FIFO-Abweichung wird keine Auffälligkeit, das Symbol links bleibt.
        const CalculationReportShare s = fifoShare();
        const CalculationOverviewDetail detail = PresenterCalculationOverview::buildShareDetail(s);

        const CalculationOverviewGroup& stock = group(detail, CalculationOverviewGroup::Kind::Stock);
        QVERIFY(stock.expanded);
        QVERIFY(stock.tone == CalculationOverviewTone::Deviation);
        QCOMPARE(stock.summary, QStringLiteral("2 Käufe abweichend · Bestand ok"));

        QCOMPARE(stock.tables.size(), 2);
        const CalculationOverviewTable& perBuy = stock.tables.at(0);
        QCOMPARE(perBuy.rows.size(), 2);
        QCOMPARE(perBuy.rows.at(0).at(5), CalculationBreakdown::volume(-2.0)); // 8 − 10
        QVERIFY(perBuy.tones.at(0).at(5) == CalculationOverviewTone::Deviation);

        QVERIFY(detail.findings.isEmpty());
        QVERIFY(!detail.findingsExpanded);
        const QList<CalculationOverviewEntry> entries =
            PresenterCalculationOverview::buildEntries(inputFor({ s }));
        QVERIFY(entries.at(1).tone == CalculationOverviewTone::Ok);
    }

    void test_buysGroup_soldColumnsAndSumRow()
    {
        // Punkt 3b: "verk. gespeich." und "verk. FIFO" auch in den Käufen.
        const CalculationOverviewDetail detail =
            PresenterCalculationOverview::buildShareDetail(fifoShare());
        const CalculationOverviewGroup& buys = group(detail, CalculationOverviewGroup::Kind::Buys);

        QCOMPARE(buys.title, QStringLiteral("Käufe (2)"));
        QCOMPARE(buys.tables.size(), 1);
        const CalculationOverviewTable& t = buys.tables.at(0);
        QVERIFY(t.headers.contains(QStringLiteral("verk. gespeich.")));
        QVERIFY(t.headers.contains(QStringLiteral("verk. FIFO")));
        QVERIFY(!t.headers.contains(QStringLiteral("Stück heute")));
        QCOMPARE(t.rows.size(), 2);
        QCOMPARE(t.sumRow.value(0), QStringLiteral("Summe"));
        QCOMPARE(t.sumRow.value(6), CalculationBreakdown::euro(800.0)); // 500 + 300, ohne Kosten

        QCOMPARE(buys.summary, CalculationBreakdown::volume(15.0) + QStringLiteral(" Stk. · ")
                                   + CalculationBreakdown::euro(800.0));
        QVERIFY(!buys.expanded);
    }

    void test_splits_addTodayColumns()
    {
        CalculationReportShare s = shareWithBuy();
        s.splits = { ShareSplitObject(QStringLiteral("x1"), QStringLiteral("g1"),
                                      d(2026, 3, 1), 2.0, 1.0) };

        const CalculationOverviewDetail detail = PresenterCalculationOverview::buildShareDetail(s);
        const CalculationOverviewTable& t =
            group(detail, CalculationOverviewGroup::Kind::Buys).tables.at(0);
        QCOMPARE(t.headers.size(), 11);
        QCOMPARE(t.headers.at(9), QStringLiteral("Stück heute"));
        QCOMPARE(t.rows.at(0).at(9), CalculationBreakdown::volume(24.0) + QStringLiteral(" Stk."));

        const CalculationOverviewGroup& splits = group(detail, CalculationOverviewGroup::Kind::Splits);
        QCOMPARE(splits.title, QStringLiteral("Splits (1)"));
        QVERIFY2(splits.summary.contains(QStringLiteral("Faktor")), qPrintable(splits.summary));
    }
};

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    // Deutsches Zahlenformat wie in der Anwendung (siehe main.cpp).
    QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));

    TestCalculationOverviewForm t;
    return QTest::qExec(&t, argc, argv);
}

#include "tst_calculationoverviewform.moc"

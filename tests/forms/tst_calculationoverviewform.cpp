// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// Tests für PresenterCalculationOverview — den Tab "Depot- vs. Marktwert"
// (ergänzt 27.10.2026, siehe ARCHITECTURE.md, "Tab Depot- vs. Marktwert").
//
// Fake-View/Fake-Model-Paar wie tst_portfoliochartform: keine Datenbank,
// kein QWidget. Geprüft wird, was der Presenter in Liste und Detail schreibt
// und wie er die Auswahl über ein Neuladen hinweg hält. Die Zahlen selbst
// prüft tst_calculationbreakdown.

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
        }
        return input;
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
        QCOMPARE(view.detail.checks.size(), 3);
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
        QCOMPARE(view.detail.checks.size(), 5);
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

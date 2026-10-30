// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// Tests für CalculationBreakdown — das gemeinsame Zeilenmodell von
// Berechnungsbericht und Tab "Depot- vs. Marktwert" (ergänzt 27.10.2026,
// siehe ARCHITECTURE.md, "Tab Depot- vs. Marktwert").
//
// Datenbankfrei mit von Hand befüllten Eingangsdaten, gleiche Muster-Aktie
// wie tst_calculationreport. Dass der Berichtstext durch den Umbau gleich
// geblieben ist, sichern die unveränderten Tests in tst_calculationreport;
// hier geht es um das Zeilenmodell selbst und darum, dass Bericht und Tab
// dieselben Auffälligkeiten sehen.

#include <QtTest>
#include <QCoreApplication>

#include "../../app/utils/CalculationBreakdown.h"
#include "../../app/utils/CalculationReport.h"

class TestCalculationBreakdown : public QObject
{
    Q_OBJECT

private:
    static QDate d(int year, int month, int day) { return QDate(year, month, day); }

    /// 12 Stück à 50,00 gekauft, Kurs 62,00 — Tabelle und Chart passen.
    static CalculationReportShare consistentShare(const QString& guid = QStringLiteral("share-1"),
                                                  const QString& name = QStringLiteral("Aktie A"))
    {
        CalculationReportShare s;
        s.share = ShareObject(guid, QStringLiteral("A0ABCD"), QStringLiteral("DE000A0ABCD0"), name);
        s.share.setCurPrice(62.0);
        s.share.setPrevDayPrice(61.0);

        ShareValues& v = s.values;
        v.volume                   = 12.0;
        v.curPrice                 = 62.0;
        v.purchaseValue            = 600.0;
        v.curValue                 = 744.0;
        v.profitLoss               = 144.0;
        v.profitLossPct            = 24.0;
        v.completePurchaseMarket   = 600.0;
        v.completeProfitLossMarket = 144.0;
        v.completeCurValueMarket   = 744.0;
        v.completeProfitPctMarket  = 24.0;
        v.purchaseValueFinal       = 610.0;
        v.profitLossFinal          = 134.0;
        v.profitLossPctFinal       = 134.0 / 610.0 * 100.0;
        v.totalBrokerage           = 10.0;
        v.completePurchase         = 610.0;
        v.completeCurValue         = 744.0;
        v.completeProfitLoss       = 134.0;
        v.completeProfitPct        = 134.0 / 610.0 * 100.0;

        PortfolioSharePoint& c = s.chart;
        c.date              = d(2026, 10, 22);
        c.name              = name;
        c.volume            = 12.0;
        c.price             = 62.0;
        c.holdingsValue     = 744.0;
        c.purchaseValueHeld = 600.0;
        c.costs             = 10.0;
        c.marketValue       = 744.0;
        return s;
    }

    static CalculationReportInput inputFor(const QList<CalculationReportShare>& shares)
    {
        CalculationReportInput input;
        input.created = QDateTime(d(2026, 10, 27), QTime(12, 0));
        input.shares  = shares;

        PortfolioSeriesPoint& t = input.chartTotal;
        t.date = d(2026, 10, 22);
        for (const CalculationReportShare& s : shares) {
            t.holdingsValue     += s.chart.holdingsValue;
            t.purchaseValueHeld += s.chart.purchaseValueHeld;
            t.costs             += s.chart.costs;
            t.marketValue       += s.chart.marketValue;
            t.development       += CalculationBreakdown::chartDevelopment(s.chart);
        }
        return input;
    }

    static QString money(double v) { return QLocale().toString(v, 'f', 2); }

private slots:

    // ── shareRows() ──────────────────────────────────────────────────────

    void test_shareRows_fourteenRowsInReportOrder()
    {
        const QList<CalculationBreakdownRow> rows =
            CalculationBreakdown::shareRows(consistentShare().values);

        QStringList labels;
        for (const CalculationBreakdownRow& r : rows)
            labels << r.label;
        QCOMPARE(labels, QStringList({
            QStringLiteral("Einzahlung (gehaltene Stücke)"),
            QStringLiteral("Kurswert gehaltener Stücke"),
            QStringLiteral("Aktuelle Entwicklung"),
            QStringLiteral("Aktuelle Entwicklung %"),
            QStringLiteral("Kosten gesamt"),
            QStringLiteral("Dividenden netto"),
            QStringLiteral("Verkäufe (Markt brutto / Depot netto)"),
            QStringLiteral("Realisiert rein Kurs"),
            QStringLiteral("Realisiert mit Kosten und Steuern"),
            QStringLiteral("Kpl. Einzahlung"),
            QStringLiteral("Kpl. Marktwert / Kpl. Wert"),
            QStringLiteral("Komplette Entwicklung"),
            QStringLiteral("Komplette Entwicklung %"),
            QStringLiteral("Beitrag Footer \"Aktueller Depotstand\"") }));
    }

    void test_shareRows_valuesComeFromTheMatchingFields()
    {
        const ShareValues v = consistentShare().values;
        const QList<CalculationBreakdownRow> rows = CalculationBreakdown::shareRows(v);

        // Einzahlung: Marktwert ohne, Depotwert mit Kosten.
        QCOMPARE(rows.at(0).market, v.purchaseValue);
        QCOMPARE(rows.at(0).depot,  v.purchaseValueFinal);
        // Kosten gibt es nur in der Depotwert-Sicht.
        QVERIFY(!rows.at(4).hasMarket);
        QVERIFY(rows.at(4).hasDepot);
        QCOMPARE(rows.at(4).depot, v.totalBrokerage);
        // Prozentzeilen sind als solche markiert.
        QVERIFY(rows.at(3).percent);
        QVERIFY(rows.at(12).percent);
        QVERIFY(!rows.at(11).percent);
    }

    void test_shareRows_everyRowNamesWhereItIsUsed()
    {
        for (const CalculationBreakdownRow& r :
             CalculationBreakdown::shareRows(consistentShare().values))
            QVERIFY2(!r.usedIn.isEmpty(), qPrintable(r.label));
    }

    void test_shareRows_usedInNamesTabColumnAndFooter()
    {
        const QList<CalculationBreakdownRow> rows =
            CalculationBreakdown::shareRows(consistentShare().values);

        // Zeile 1: obere Hälfte der Spalte "Einzahlung / Marktwert" in beiden
        // Tabs, als Summe im Footer "Einzahlung (gesamt)" derselben Spalte.
        QCOMPARE(rows.at(0).usedIn, QStringList({
            QStringLiteral("beide Tabs: Spalte \"Einzahlung / Marktwert\" oben"),
            QStringLiteral("→ Footer \"Einzahlung (gesamt)\" (Summe)") }));
        // Kosten nur im Depotwert-Tab.
        QCOMPARE(rows.at(4).usedIn.first(),
                 QStringLiteral("Kompletter Depotwert: Spalte \"Kosten / Dividenden\" oben"));

        // Zeile 2 (Kurswert) endet auch im Marktwert-Chart, Zeile 12
        // (Komplette Entwicklung) im Depotwert-Chart.
        QVERIFY(rows.at(1).usedIn.join(QLatin1Char('\n')).contains(QStringLiteral("Marktwert-Chart")));
        QVERIFY(rows.at(11).usedIn.join(QLatin1Char('\n')).contains(QStringLiteral("Depotwert-Chart")));
    }

    void test_shareRows_derivedRowsCarryFigures()
    {
        const QList<CalculationBreakdownRow> rows =
            CalculationBreakdown::shareRows(consistentShare().values);

        QCOMPARE(rows.at(2).marketFigures,
                 QStringLiteral("%1 − %2 = %3").arg(money(744.0), money(600.0), money(144.0)));
        QCOMPARE(rows.at(2).depotFigures,
                 QStringLiteral("%1 − %2 = %3").arg(money(744.0), money(610.0), money(134.0)));
        // Summen über die Transaktionen haben keine Zahlenrechnung.
        QVERIFY(rows.at(0).marketFigures.isEmpty());
    }

    // ── Footer ───────────────────────────────────────────────────────────

    void test_footerTotals_matchSumsOfBothShares()
    {
        const CalculationReportShare a = consistentShare(QStringLiteral("g1"), QStringLiteral("A"));
        CalculationReportShare b = consistentShare(QStringLiteral("g2"), QStringLiteral("B"));
        b.values.totalDividend = 5.0;

        const CalculationFooterTotals t = CalculationBreakdown::footerTotals({ a, b });
        QCOMPARE(t.marketPurchase,        1200.0);
        QCOMPARE(t.marketCurValue,        1488.0);
        QCOMPARE(t.depotPurchase,         1220.0);
        QCOMPARE(t.depotCurValue,         1488.0);
        QCOMPARE(t.depotProfit,           268.0);   // Aktueller Depotstand − Einzahlung
        QCOMPARE(t.depotBrokerage,        20.0);
        QCOMPARE(t.depotDividend,         5.0);
        QCOMPARE(t.marketCompleteProfit,  288.0);
        QCOMPARE(t.depotCompleteProfit,   268.0);
    }

    void test_footerRows_tenRowsCostsOnlyInDepot()
    {
        const QList<CalculationBreakdownRow> rows = CalculationBreakdown::footerRows(
            CalculationBreakdown::footerTotals({ consistentShare() }));

        QCOMPARE(rows.size(), 10);
        QCOMPARE(rows.at(1).label, QStringLiteral("Kosten (ges.)"));
        QVERIFY(!rows.at(1).hasMarket);
        QCOMPARE(rows.at(5).label, QStringLiteral("Aktueller Depotstand"));
        QCOMPARE(rows.at(5).market, 744.0);
        QCOMPARE(rows.at(5).depot,  744.0);
        for (const CalculationBreakdownRow& r : rows)
            QVERIFY2(!r.usedIn.isEmpty(), qPrintable(r.label));
    }

    // ── check() ──────────────────────────────────────────────────────────

    void test_check_results()
    {
        using R = CalculationCheckResult;
        const QString s = QStringLiteral("Aktie A");

        QVERIFY(CalculationBreakdown::check(s, QStringLiteral("x"), 10.0, 10.0, false).result == R::Ok);
        QVERIFY(CalculationBreakdown::check(s, QStringLiteral("x"), 10.0, 9.0, false, true).result == R::Hint);
        QVERIFY(CalculationBreakdown::check(s, QStringLiteral("x"), 10.0, 9.0, false).result == R::Deviation);
        QVERIFY(CalculationBreakdown::check(s, QStringLiteral("x"), 10.0, 9.0, false, false, 1.0).result
                == R::Explained);

        const CalculationCheck rest =
            CalculationBreakdown::check(s, QStringLiteral("Marktwert"), 10.0, 7.0, false, false, 1.0);
        QVERIFY(rest.result == R::Deviation);
        QCOMPARE(rest.resultText, QStringLiteral("ABWEICHUNG"));
        QVERIFY2(rest.finding.contains(QStringLiteral("Rest ") + money(2.0)), qPrintable(rest.finding));
    }

    void test_shareChecks_fiveChecksAllOkForConsistentShare()
    {
        const QList<CalculationCheck> checks = CalculationBreakdown::shareChecks(consistentShare());
        QCOMPARE(checks.size(), 5);
        for (const CalculationCheck& c : checks)
            QVERIFY2(c.result == CalculationCheckResult::Ok, qPrintable(c.label));
    }

    void test_shareChecks_emptyWithoutDailyValues()
    {
        CalculationReportShare s = consistentShare();
        s.chartExcluded = true;
        s.chart = PortfolioSharePoint{};

        QVERIFY(CalculationBreakdown::shareChecks(s).isEmpty());
        const CalculationFindings f = CalculationBreakdown::shareFindings(s);
        QCOMPARE(f.deviations, QStringList({ QStringLiteral("Aktie A: keine Tageswerte, fehlt in beiden Charts") }));
    }

    // ── findings() ───────────────────────────────────────────────────────

    void test_findings_sharesFirstThenTotal()
    {
        CalculationReportShare s = consistentShare();
        s.chart.volume = 10.0;  // Bestandsabweichung je Aktie
        CalculationReportInput input = inputFor({ s });
        input.chartTotal.marketValue -= 50.0;  // zusätzlich eine Abweichung im Gesamt

        const CalculationFindings f = CalculationBreakdown::findings(input);
        QVERIFY(f.deviations.size() >= 2);
        QVERIFY2(f.deviations.first().startsWith(QStringLiteral("Aktie A: Bestand")),
                 qPrintable(f.deviations.first()));
        QVERIFY2(f.deviations.last().startsWith(QStringLiteral("GESAMT:")),
                 qPrintable(f.deviations.last()));
    }

    void test_findings_sameAsReportPartThree()
    {
        // Bericht und Tab dürfen nicht auseinanderlaufen: jede Zeile aus
        // findings() steht genau so in Teil 3 des Berichts.
        CalculationReportShare s = consistentShare();
        s.chart.price = 61.5; // Hinweis Kurs
        s.chart.volume = 11.0; // Abweichung Bestand
        const CalculationReportInput input = inputFor({ s });

        const QString report = CalculationReport::build(input);
        const QString three  = report.mid(report.indexOf(QStringLiteral("TEIL 3")));
        const CalculationFindings f = CalculationBreakdown::findings(input);
        QVERIFY(!f.deviations.isEmpty());
        QVERIFY(!f.hints.isEmpty());
        for (const QString& line : f.deviations + f.hints)
            QVERIFY2(three.contains(QStringLiteral("  • ") + line + QLatin1Char('\n')), qPrintable(line));
    }

    void test_totalChecks_noChartPoint_noChecks()
    {
        CalculationReportInput input = inputFor({ consistentShare() });
        input.chartTotal = PortfolioSeriesPoint{};

        const CalculationTotalChecks t = CalculationBreakdown::totalChecks(
            input, CalculationBreakdown::footerTotals(input.shares));
        QVERIFY(t.checks.isEmpty());
    }

    void test_priceEffect_heldVolumeTimesPriceDifference()
    {
        CalculationReportShare s = consistentShare();
        s.chart.price         = 61.0;
        s.chart.holdingsValue = 732.0;
        QCOMPARE(CalculationBreakdown::priceEffect(s), 12.0); // 12 × (62,00 − 61,00)
    }
};

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    // Deutsches Zahlenformat wie in der Anwendung (siehe main.cpp).
    QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));

    TestCalculationBreakdown t;
    return QTest::qExec(&t, argc, argv);
}

#include "tst_calculationbreakdown.moc"

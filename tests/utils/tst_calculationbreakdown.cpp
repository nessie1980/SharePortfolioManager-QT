// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// Tests für CalculationBreakdown — das gemeinsame Zeilenmodell von
// Berechnungsbericht und Tab "Depot- vs. Marktwert" (ergänzt 27.10.2026,
// siehe ARCHITECTURE.md, "Tab Depot- vs. Marktwert").
//
// Datenbankfrei mit von Hand befüllten Eingangsdaten, gleiche Muster-Aktie
// wie tst_calculationreport. Seit v1.30.1 (17.11.2026) auch die
// Transaktionstabellen je Aktie (buyTable() bis stockSummary()), die der
// Bericht bis dahin selbst gerechnet hat. Dass der Berichtstext durch den Umbau gleich
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
        c.purchaseValueHeldFinal = 610.0; // zweite Linien (01.11.2026)
        c.marketValueComplete    = 744.0;
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
            t.purchaseValueHeldFinal += s.chart.purchaseValueHeldFinal;
            t.developmentHeld        += CalculationBreakdown::chartDevelopmentHeld(s.chart);
            t.marketValueComplete    += s.chart.marketValueComplete;
        }
        return input;
    }

    static QString money(double v) { return QLocale().toString(v, 'f', 2); }

    // ── Bausteine für die Transaktionstabellen (ergänzt 17.11.2026) ───────

    static QString iso(const QDate& date)
    {
        return QDateTime(date, QTime(10, 0)).toString(Qt::ISODate);
    }

    static BuyObject buy(const QString& guid, const QDate& date, double volume, double price,
                         double volumeSold = 0.0, const QString& brokerageGuid = QString())
    {
        return BuyObject(guid, QStringLiteral("share-1"), QStringLiteral("D1"),
                         QStringLiteral("ord-") + guid, iso(date), volume, volumeSold, price,
                         brokerageGuid);
    }

    static SaleObject sale(const QString& guid, const QDate& date, double volume, double price)
    {
        return SaleObject(guid, QStringLiteral("share-1"), QStringLiteral("D1"),
                          QStringLiteral("ord-") + guid, iso(date), volume, price, {});
    }

    /// Aktie ohne Werte, nur mit Transaktionen — für die Tabellenfunktionen.
    static CalculationReportShare plainShare()
    {
        CalculationReportShare s;
        s.share = ShareObject(QStringLiteral("share-1"), QStringLiteral("A0ABCD"),
                              QStringLiteral("DE000A0ABCD0"), QStringLiteral("Aktie A"));
        return s;
    }

    /**
     * Zwei Käufe (10 und 5 Stück), ein Verkauf über 12 Stück. Nach FIFO
     * gehen 10 Stück aus dem ersten und 2 aus dem zweiten Kauf; gespeichert
     * sind absichtlich 8 und 4, damit gespeichert und FIFO auseinanderliegen.
     */
    static CalculationReportShare fifoShare()
    {
        CalculationReportShare s = plainShare();
        s.buys  = { buy(QStringLiteral("b1"), d(2026, 1, 10), 10.0, 50.0, 8.0),
                    buy(QStringLiteral("b2"), d(2026, 2, 10),  5.0, 60.0, 4.0) };
        s.sales = { sale(QStringLiteral("s1"), d(2026, 3, 1), 12.0, 70.0) };
        s.values.volume = 3.0;
        return s;
    }

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

    void test_shareChecks_sevenChecksAllOkForConsistentShare()
    {
        // Seit 01.11.2026 sieben Zeilen: dazu die zweiten Linien beider Charts.
        const QList<CalculationCheck> checks = CalculationBreakdown::shareChecks(consistentShare());
        QCOMPARE(checks.size(), 7);
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

    // ── Punkt A und Ersatzkurse (ergänzt 31.10.2026) ─────────────────────

    void test_shareChecks_soldShare_priceDifferenceIsNotReported()
    {
        // Vollständig verkauft, gespeicherter Kurs veraltet (Fall ADIDAS aus
        // Nessies Bericht: 27,52 gegen Schlusskurs 167,70). Ohne Bestand
        // wirkt der Kurs auf keinen Wert — die Zeile bleibt, ohne Meldung.
        CalculationReportShare s = consistentShare();
        s.values.volume   = 0.0;
        s.values.curValue = 0.0;
        s.values.curPrice = 27.518;
        s.chart.volume        = 0.0;
        s.chart.price         = 167.70;
        s.chart.holdingsValue = 0.0;
        s.chart.marketValue   = 0.0;
        s.chart.purchaseValueHeld = 0.0;
        s.values.purchaseValue = 0.0;
        s.values.purchaseValueFinal = 0.0;
        s.values.profitLossFinal    = 0.0;
        s.chart.purchaseValueHeldFinal = 0.0;
        s.chart.realizedGain  = 134.0 + 10.0; // Entwicklung wie in der Tabelle
        s.chart.costs         = 10.0;

        const QList<CalculationCheck> checks = CalculationBreakdown::shareChecks(s);
        QCOMPARE(checks.at(1).label, QStringLiteral("Kurs (Tabelle / Chart)"));
        QVERIFY(checks.at(1).result == CalculationCheckResult::Ok);
        QCOMPARE(checks.at(1).resultText, QStringLiteral("ohne Bestand"));

        const CalculationFindings f = CalculationBreakdown::shareFindings(s);
        QVERIFY2(f.deviations.isEmpty() && f.hints.isEmpty(),
                 qPrintable((f.deviations + f.hints).join(QLatin1Char('\n'))));
    }

    void test_shareChecks_heldShare_priceDifferenceStaysHint()
    {
        CalculationReportShare s = consistentShare();
        s.chart.price = 61.5;
        QVERIFY(CalculationBreakdown::shareChecks(s).at(1).result == CalculationCheckResult::Hint);
    }

    void test_shareFindings_substitutePricesIsHintNotDeviation()
    {
        CalculationReportShare s = consistentShare();
        s.substitutePrices = true;

        const CalculationFindings f = CalculationBreakdown::shareFindings(s);
        QVERIFY(f.deviations.isEmpty());
        QCOMPARE(f.hints, QStringList({ QStringLiteral("Aktie A: keine Tageswerte, in den Charts mit "
                                                       "Kauf- und Verkaufskursen bewertet (Ersatzkurse)") }));
    }

    void test_priceEffect_heldVolumeTimesPriceDifference()
    {
        CalculationReportShare s = consistentShare();
        s.chart.price         = 61.0;
        s.chart.holdingsValue = 732.0;
        QCOMPARE(CalculationBreakdown::priceEffect(s), 12.0); // 12 × (62,00 − 61,00)
    }

    // ── Transaktionstabellen (Refactor 17.11.2026, v1.30.1) ──────────────

    void test_buyTable_valuesCostsAndSums()
    {
        // b1: Kosten über BrokerageObject::buyGuid() (10 + 2 − 2 = 10,00),
        // b2: Kosten über BuyObject::brokerageGuid() (4 + 1 − 0 = 5,00).
        // Ein freistehender Eintrag zählt bei keinem Kauf.
        CalculationReportShare s = plainShare();
        s.buys = { buy(QStringLiteral("b1"), d(2026, 1, 10), 12.0, 50.0),
                   buy(QStringLiteral("b2"), d(2026, 2, 10),  4.0, 25.5, 0.0, QStringLiteral("k2")) };
        s.brokerages = {
            BrokerageObject(QStringLiteral("k1"), QStringLiteral("share-1"), QStringLiteral("b1"),
                            QString(), iso(d(2026, 1, 10)), 10.0, 2.0, 0.0, 2.0),
            BrokerageObject(QStringLiteral("k2"), QStringLiteral("share-1"), QString(),
                            QString(), iso(d(2026, 2, 10)), 4.0, 1.0, 0.0, 0.0),
            BrokerageObject(QStringLiteral("k3"), QStringLiteral("share-1"), QString(),
                            QString(), iso(d(2026, 3, 1)), 7.0) };

        const CalculationBuyTable t = CalculationBreakdown::buyTable(s);
        QCOMPARE(t.rows.size(), 2);
        QVERIFY(!t.withSplits);

        const CalculationBuyRow& r1 = t.rows.at(0);
        QCOMPARE(r1.date, d(2026, 1, 10));
        QCOMPARE(r1.depotNumber, QStringLiteral("D1"));
        QCOMPARE(r1.value, 600.0);
        QCOMPARE(r1.costs, 10.0);
        QCOMPARE(r1.valueWithCosts, 610.0);

        const CalculationBuyRow& r2 = t.rows.at(1);
        QCOMPARE(r2.value, 102.0);   // 4 × 25,50
        QCOMPARE(r2.costs, 5.0);
        QCOMPARE(r2.valueWithCosts, 107.0);

        QCOMPARE(t.sumVolume, 16.0);
        QCOMPARE(t.sumVolumeToday, 16.0);
        QCOMPARE(t.sumValue, 702.0);
        QCOMPARE(t.sumCosts, 15.0);
        QCOMPARE(t.sumValueWithCosts, 717.0);
    }

    void test_buyTable_fifoSoldMatchesReplay()
    {
        const CalculationReportShare s = fifoShare();
        const CalculationBuyTable t = CalculationBreakdown::buyTable(s);
        const QList<double> replay = CalculationReport::replayFifoSold(s.buys, s.sales, s.splits);

        QCOMPARE(t.rows.size(), 2);
        QCOMPARE(t.rows.at(0).soldFifo, replay.at(0));
        QCOMPARE(t.rows.at(1).soldFifo, replay.at(1));
        QCOMPARE(t.rows.at(0).soldFifo, 10.0);
        QCOMPARE(t.rows.at(1).soldFifo, 2.0);

        // Gespeichert bleibt, was in der Datenbank steht.
        QCOMPARE(t.rows.at(0).soldStored, 8.0);
        QCOMPARE(t.rows.at(1).soldStored, 4.0);

        QCOMPARE(t.rows.at(0).remainingFifo, 0.0);
        QCOMPARE(t.rows.at(1).remainingFifo, 3.0);
        QCOMPARE(t.sumSoldStoredToday, 12.0);
        QCOMPARE(t.sumSoldFifoToday, 12.0);
    }

    void test_buyTable_withSplits_todayColumnsAndSums()
    {
        // Kauf vor einem 2:1-Split: 10 Stück à 50,00 sind heute 20 Stück à
        // 25,00. Der Kauf danach bleibt unverändert.
        CalculationReportShare s = plainShare();
        s.buys   = { buy(QStringLiteral("b1"), d(2026, 1, 10), 10.0, 50.0, 4.0),
                     buy(QStringLiteral("b2"), d(2026, 6, 10),  6.0, 30.0) };
        s.splits = { ShareSplitObject(QStringLiteral("x1"), QStringLiteral("share-1"),
                                      d(2026, 3, 1), 2.0, 1.0) };

        const CalculationBuyTable t = CalculationBreakdown::buyTable(s);
        QVERIFY(t.withSplits);
        QCOMPARE(t.rows.at(0).volume, 10.0);       // Beleg-Skala bleibt
        QCOMPARE(t.rows.at(0).volumeToday, 20.0);
        QCOMPARE(t.rows.at(0).priceToday, 25.0);
        QCOMPARE(t.rows.at(1).volumeToday, 6.0);
        QCOMPARE(t.rows.at(1).priceToday, 30.0);

        QCOMPARE(t.sumVolumeToday, 26.0);
        QCOMPARE(t.sumSoldStoredToday, 8.0);       // 4 Stück vor dem Split
    }

    void test_saleTable_payoutAndSums()
    {
        // Verkauf 10 × 70,00 = 700,00, Gebühren 9 + 1 = 10,00, Rabatt 2,00,
        // Steuern 20 + 1,10 = 21,10 → Auszahlung 700 − 10 + 2 − 21,10 = 670,90.
        CalculationReportShare s = plainShare();
        s.sales = {
            SaleObject(QStringLiteral("s1"), QStringLiteral("share-1"), QStringLiteral("D1"),
                       QStringLiteral("ord-s1"), iso(d(2026, 4, 1)), 10.0, 70.0, {},
                       0.0, 20.0, 1.10, QStringLiteral("k1"), 9.0, 1.0, 0.0, 2.0),
            sale(QStringLiteral("s2"), d(2026, 5, 1), 2.0, 80.0) };

        const CalculationSaleTable t = CalculationBreakdown::saleTable(s);
        QCOMPARE(t.rows.size(), 2);

        const CalculationSaleRow& r = t.rows.at(0);
        QCOMPARE(r.value, 700.0);
        QCOMPARE(r.fees, 10.0);
        QCOMPARE(r.reduction, 2.0);
        QCOMPARE(r.taxes, 21.10);
        QCOMPARE(r.payout, 700.0 - 10.0 + 2.0 - 21.10);

        QCOMPARE(t.sumVolumeToday, 12.0);
        QCOMPARE(t.sumValue, 860.0);
        QCOMPARE(t.sumPayout, r.payout + 160.0);
    }

    void test_dividendTable_foreignCurrencyAndSums()
    {
        CalculationReportShare s = plainShare();
        s.dividends = {
            DividendObject(QStringLiteral("d1"), QStringLiteral("share-1"), iso(d(2026, 3, 10)),
                           1.0, 12.0, 1.0, 2.0, 0.11),
            DividendObject(QStringLiteral("d2"), QStringLiteral("share-1"), iso(d(2024, 6, 17)),
                           0.2, 200.0, 5.57, 3.71, 0.20, 0.0, true, 1.0778,
                           QStringLiteral("USD")) };

        const CalculationDividendTable t = CalculationBreakdown::dividendTable(s);
        QCOMPARE(t.rows.size(), 2);

        // Euro-Beleg: Satz in €, Steuern 3,11, Netto 8,89.
        QCOMPARE(t.rows.at(0).currency, QStringLiteral("€"));
        QCOMPARE(t.rows.at(0).gross, 12.0);
        QCOMPARE(t.rows.at(0).taxes, s.dividends.at(0).taxSum());
        QCOMPARE(t.rows.at(0).net, s.dividends.at(0).dividendPayoutWithTaxes());

        // Fremdwährung: Satz in der Belegwährung, Beträge wie DividendObject.
        QCOMPARE(t.rows.at(1).currency, QStringLiteral("USD"));
        QCOMPARE(t.rows.at(1).rate, 0.2);
        QCOMPARE(t.rows.at(1).gross, s.dividends.at(1).dividendPayout());

        QCOMPARE(t.sumGross, s.dividends.at(0).dividendPayout() + s.dividends.at(1).dividendPayout());
        QCOMPARE(t.sumTaxes, s.dividends.at(0).taxSum() + s.dividends.at(1).taxSum());
        QCOMPARE(t.sumNet, s.dividends.at(0).dividendPayoutWithTaxes()
                               + s.dividends.at(1).dividendPayoutWithTaxes());
    }

    void test_brokerageTable_assignmentAndSums()
    {
        CalculationReportShare s = plainShare();
        s.brokerages = {
            BrokerageObject(QStringLiteral("k1"), QStringLiteral("share-1"), QStringLiteral("b1"),
                            QString(), iso(d(2026, 1, 10)), 10.0, 2.0, 1.0, 2.0),
            BrokerageObject(QStringLiteral("k2"), QStringLiteral("share-1"), QString(),
                            QStringLiteral("s1"), iso(d(2026, 4, 1)), 9.0, 1.0),
            BrokerageObject(QStringLiteral("k3"), QStringLiteral("share-1"), QString(),
                            QString(), iso(d(2026, 5, 1)), 5.0) };

        const CalculationBrokerageTable t = CalculationBreakdown::brokerageTable(s);
        QCOMPARE(t.rows.size(), 3);
        QVERIFY(t.rows.at(0).assignment == CalculationBrokerageAssignment::Buy);
        QVERIFY(t.rows.at(1).assignment == CalculationBrokerageAssignment::Sale);
        QVERIFY(t.rows.at(2).assignment == CalculationBrokerageAssignment::Standalone);

        QCOMPARE(t.rows.at(0).gross, 13.0);  // 10 + 2 + 1
        QCOMPARE(t.rows.at(0).net, 11.0);    // − 2 Rabatt

        QCOMPARE(t.sumProvision, 24.0);
        QCOMPARE(t.sumBrokerFee, 3.0);
        QCOMPARE(t.sumTraderFee, 1.0);
        QCOMPARE(t.sumGross, 28.0);
        QCOMPARE(t.sumReduction, 2.0);
        QCOMPARE(t.sumNet, 26.0);
    }

    void test_stockSummary_fiveValues()
    {
        const CalculationStockSummary st = CalculationBreakdown::stockSummary(fifoShare());
        QCOMPARE(st.bought, 15.0);
        QCOMPARE(st.sold, 12.0);
        QCOMPARE(st.computed, 3.0);
        QCOMPARE(st.fifo, 3.0);
        QCOMPARE(st.table, 3.0);
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

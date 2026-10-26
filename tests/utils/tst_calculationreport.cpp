// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// Tests für CalculationReport — den Berechnungsbericht (ergänzt 23.10.2026,
// siehe ARCHITECTURE.md, "Berechnungsbericht").
//
// Drei Gruppen:
// - build(): datenbankfrei mit von Hand befüllten Eingangsdaten. Geprüft wird,
//   dass die Abschnitte vorhanden sind und Abweichungen zwischen Tabelle,
//   Footer und Chart erkannt und in Teil 3 gelistet werden.
// - replayFifoSold(): die FIFO-Nachrechnung je Kauf.
// - collect()/writeToFile(): gegen eine In-Memory-SQLite-Datenbank bzw. eine
//   temporäre Datei.

#include <QtTest>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>

#include "../../app/utils/CalculationReport.h"
#include "../../app/core/Database.h"
#include "../../app/repositories/ShareRepository.h"
#include "../../app/repositories/BuyRepository.h"
#include "../../app/repositories/SaleRepository.h"
#include "../../app/repositories/DailyValuesRepository.h"
#include "../../app/models/DailyValuesObject.h"

class TestCalculationReport : public QObject
{
    Q_OBJECT

private:
    static QDate d(int year, int month, int day) { return QDate(year, month, day); }

    static QString iso(const QDate& date)
    {
        return QDateTime(date, QTime(10, 0)).toString(Qt::ISODate);
    }

    static BuyObject buy(const QString& guid, const QDate& date, double volume,
                         double price, const QString& depot = QString(), double volumeSold = 0.0)
    {
        return BuyObject(guid, QStringLiteral("share-1"), depot, QStringLiteral("ord-") + guid,
                         iso(date), volume, volumeSold, price);
    }

    static SaleObject sale(const QString& guid, const QDate& date, double volume,
                           double price, const QString& depot = QString())
    {
        return SaleObject(guid, QStringLiteral("share-1"), depot, QStringLiteral("ord-") + guid,
                          iso(date), volume, price, {});
    }

    /// Eine Aktie, deren Tabellen- und Chartwerte zueinander passen:
    /// 12 Stück à 62,00 = 744,00, keine Verkäufe.
    static CalculationReportShare consistentShare()
    {
        CalculationReportShare s;
        s.share = ShareObject(QStringLiteral("share-1"), QStringLiteral("A0ABCD"),
                              QStringLiteral("DE000A0ABCD0"), QStringLiteral("Aktie A"));
        s.share.setCurPrice(62.0);
        s.share.setPrevDayPrice(61.0);
        s.buys = { buy(QStringLiteral("b1"), d(2026, 1, 10), 12.0, 50.0) };

        ShareValues& v = s.values;
        v.volume                   = 12.0;
        v.curPrice                 = 62.0;
        v.purchaseValue            = 600.0;
        v.curValue                 = 744.0;
        v.profitLoss               = 144.0;
        v.completePurchaseMarket   = 600.0;
        v.completeProfitLossMarket = 144.0;
        v.completeCurValueMarket   = 744.0;
        v.purchaseValueFinal       = 600.0;
        v.profitLossFinal          = 144.0;
        v.completePurchase         = 600.0;
        v.completeCurValue         = 744.0;
        v.completeProfitLoss       = 144.0;

        PortfolioSharePoint& c = s.chart;
        c.date              = d(2026, 10, 22);
        c.name              = QStringLiteral("Aktie A");
        c.volume            = 12.0;
        c.price             = 62.0;
        c.holdingsValue     = 744.0;
        c.purchaseValueHeld = 600.0;
        c.marketValue       = 744.0;
        return s;
    }

    static CalculationReportInput inputFor(const QList<CalculationReportShare>& shares)
    {
        CalculationReportInput input;
        input.created       = QDateTime(d(2026, 10, 23), QTime(12, 0));
        input.portfolioPath = QStringLiteral("/tmp/test.db");
        input.shares        = shares;

        PortfolioSeriesPoint& t = input.chartTotal;
        t.date = d(2026, 10, 22);
        for (const CalculationReportShare& s : shares) {
            t.holdingsValue     += s.chart.holdingsValue;
            t.purchaseValueHeld += s.chart.purchaseValueHeld;
            t.realizedGain      += s.chart.realizedGain;
            t.marketValue       += s.chart.marketValue;
            t.development       += s.chart.holdingsValue + s.chart.realizedGain
                                   - s.chart.purchaseValueHeld;
        }
        return input;
    }

    static QString partThree(const QString& report)
    {
        const qsizetype pos = report.indexOf(QStringLiteral("TEIL 3"));
        return pos < 0 ? QString() : report.mid(pos);
    }

private slots:

    // ── build() ──────────────────────────────────────────────────────────

    void test_build_containsAllThreeParts()
    {
        const QString report = CalculationReport::build(inputFor({ consistentShare() }));

        QVERIFY(report.contains(QStringLiteral("Berechnungsbericht")));
        QVERIFY(report.contains(QStringLiteral("TEIL 1 — JE AKTIE")));
        QVERIFY(report.contains(QStringLiteral("AKTIE: Aktie A")));
        QVERIFY(report.contains(QStringLiteral("TEIL 2 — GESAMTÜBERSICHT")));
        QVERIFY(report.contains(QStringLiteral("Footer \"Kompletter Marktwert\"")));
        QVERIFY(report.contains(QStringLiteral("Footer \"Kompletter Depotwert\"")));
        QVERIFY(report.contains(QStringLiteral("TEIL 3 — AUFFÄLLIGKEITEN")));
    }

    void test_build_consistentValues_noFindings()
    {
        const QString report = CalculationReport::build(inputFor({ consistentShare() }));

        QVERIFY(!report.contains(QStringLiteral("ABWEICHUNG")));
        QVERIFY(partThree(report).contains(QStringLiteral("Keine —")));
    }

    void test_build_realizedGain_doesNotMoveFooterDepotstand()
    {
        // Bis 25.10.2026 der Fall vom 19.10.2026: der Footer "Aktueller
        // Depotstand" addierte den realisierten Gewinn, der Chart nicht.
        // Seit dem Bugfix vom 25.10.2026 ist er Σ Kurswert — ein
        // realisierter Gewinn erscheint nur in den Gesamt-Spalten.
        CalculationReportShare s = consistentShare();
        s.values.saleProfitLoss         = 65.0;
        s.values.salePayoutMarket       = 465.0;
        s.values.completePurchaseMarket = 1000.0;
        s.values.completeCurValueMarket = 1209.0;

        const QString report   = CalculationReport::build(inputFor({ s }));
        const QString findings = partThree(report);

        QVERIFY2(!report.contains(QStringLiteral("ABWEICHUNG")), qPrintable(findings));
        const qsizetype footer = report.indexOf(QStringLiteral("Footer \"Kompletter Marktwert\""));
        QVERIFY(footer >= 0);
        const qsizetype line = report.indexOf(QStringLiteral("Aktueller Depotstand"), footer);
        const QString depotstand = report.mid(line, report.indexOf(QLatin1Char('\n'), line) - line);
        QVERIFY2(depotstand.contains(QLocale().toString(744.0, 'f', 2)), qPrintable(depotstand));
        QVERIFY2(!depotstand.contains(QLocale().toString(809.0, 'f', 2)), qPrintable(depotstand));
    }

    void test_build_volumeDifference_isReportedPerShare()
    {
        CalculationReportShare s = consistentShare();
        s.chart.volume = 10.0; // Chart sieht nach FIFO nur noch 10 Stück

        const QString findings = partThree(CalculationReport::build(inputFor({ s })));
        QVERIFY2(findings.contains(QStringLiteral("Aktie A: Bestand (Stück)")), qPrintable(findings));
    }

    void test_build_priceDifference_isOnlyAHint()
    {
        CalculationReportShare s = consistentShare();
        s.chart.price = 61.5; // letzter Tageswert weicht vom aktuellen Kurs ab

        const QString report   = CalculationReport::build(inputFor({ s }));
        const QString findings = partThree(report);
        QVERIFY(report.contains(QStringLiteral("Hinweis")));
        QVERIFY2(findings.contains(QStringLiteral("(Hinweis) Aktie A: Kurs")), qPrintable(findings));
    }

    void test_build_priceDifferenceExplainsMarketValueDiff()
    {
        // Chart bewertet 12 Stück mit 61,00 (732,00), die Tabelle mit 62,00
        // (744,00). Die Differenz 12,00 geht vollständig im Kurs auf.
        CalculationReportShare s = consistentShare();
        s.chart.price         = 61.0;
        s.chart.holdingsValue = 732.0;
        s.chart.marketValue   = 732.0;

        const QString report   = CalculationReport::build(inputFor({ s }));
        const QString findings = partThree(report);
        QVERIFY(report.contains(QStringLiteral("durch Kurs erklärt")));
        QVERIFY2(!findings.contains(QStringLiteral("ABWEICHUNG"))
                     && !findings.contains(QStringLiteral("Rest")),
                 qPrintable(findings));
    }

    void test_build_marketDiffBeyondPriceEffect_showsRest()
    {
        // Kursunterschied 12,00 (12 × (62,00 − 61,00)) erklärt nur einen
        // Teil: der Marktwert-Chart liegt 44,00 unter der Tabelle, der Rest
        // 32,00 bleibt eine Abweichung und wird genannt.
        CalculationReportShare s = consistentShare();
        s.chart.price         = 61.0;
        s.chart.holdingsValue = 732.0;
        s.chart.marketValue   = 700.0;

        const QString findings = partThree(CalculationReport::build(inputFor({ s })));
        QVERIFY2(findings.contains(QStringLiteral("Marktwert / Marktwert-Chart"))
                     && findings.contains(QStringLiteral("Rest ") + QLocale().toString(32.0, 'f', 2)),
                 qPrintable(findings));
    }

    void test_build_buysTableShowsCostsAndSumRow()
    {
        // Kauf 12 × 50,00 = 600,00 mit zugeordneten Kosten 12,00 Gebühren
        // − 2,00 Rabatt = 10,00 → Kaufwert mit Kosten 610,00 (23.10.2026).
        CalculationReportShare s = consistentShare();
        s.brokerages = { BrokerageObject(QStringLiteral("k1"), QStringLiteral("share-1"),
                                         QStringLiteral("b1"), QString(), iso(d(2026, 1, 10)),
                                         10.0, 2.0, 0.0, 2.0) };

        const QString report = CalculationReport::build(inputFor({ s }));
        const qsizetype from = report.indexOf(QStringLiteral("Käufe"));
        const QString buys   = report.mid(from, report.indexOf(QStringLiteral("Verkäufe")) - from);

        const QString eur = QStringLiteral(" €");
        QVERIFY2(buys.contains(QStringLiteral("Kaufwert m. Kosten")), qPrintable(buys));
        QVERIFY2(buys.contains(QLocale().toString(10.0, 'f', 2) + eur), qPrintable(buys));
        QVERIFY2(buys.contains(QLocale().toString(610.0, 'f', 2) + eur), qPrintable(buys));
        QVERIFY2(buys.contains(QLocale().toString(50.0, 'f', 4) + eur), qPrintable(buys));
        QVERIFY2(buys.contains(QStringLiteral("Summe")), qPrintable(buys));
        QVERIFY2(buys.contains(QLocale().toString(12.0, 'f', 4) + QStringLiteral(" Stk.")),
                 qPrintable(buys));
        // Ohne Splits keine Spalten in heutiger Skala.
        QVERIFY(!buys.contains(QStringLiteral("Stück heute")));
    }

    void test_build_longDepotNumber_keepsColumnsAligned()
    {
        // Nessies Rückmeldung 23.10.2026: eine neunstellige Depotnummer in
        // der achtstelligen Spalte verschob die Datenzeilen gegenüber Kopf-
        // und Summenzeile um ein Zeichen. Alle Zeilen der Käufe-Tabelle
        // müssen danach gleich lang sein (letzte Spalte rechtsbündig).
        CalculationReportShare s = consistentShare();
        s.buys = { buy(QStringLiteral("b1"), d(2026, 1, 10), 12.0, 50.0, QStringLiteral("501403950")) };

        const QString report = CalculationReport::build(inputFor({ s }));
        const qsizetype datum = report.indexOf(QStringLiteral("Datum"), report.indexOf(QStringLiteral("Käufe")));
        const qsizetype from  = report.lastIndexOf(QLatin1Char('\n'), datum) + 1; // Zeilenanfang
        const QStringList lines = report.mid(from).split(QLatin1Char('\n'));

        // Kopf, Linie, Datenzeile, Linie, Summe
        QVERIFY(lines.size() > 5);
        const qsizetype headerLen = lines.at(0).size();
        QCOMPARE(lines.at(2).size(), headerLen);
        QCOMPARE(lines.at(4).size(), headerLen);
        QVERIFY(lines.at(4).trimmed().startsWith(QStringLiteral("Summe")));
    }

    void test_build_salesTableHasSumRow()
    {
        CalculationReportShare s = consistentShare();
        s.sales = { sale(QStringLiteral("s1"), d(2026, 4, 1), 4.0, 60.0),
                    sale(QStringLiteral("s2"), d(2026, 5, 1), 2.0, 70.0) };

        const QString report = CalculationReport::build(inputFor({ s }));
        const qsizetype from = report.indexOf(QStringLiteral("Verkäufe"));
        const QString sales  = report.mid(from, report.indexOf(QStringLiteral("Dividenden"), from) - from);

        QVERIFY2(sales.contains(QStringLiteral("Summe")), qPrintable(sales));
        QVERIFY2(sales.contains(QLocale().toString(6.0, 'f', 4) + QStringLiteral(" Stk.")), qPrintable(sales));
        // 4 × 60 + 2 × 70 = 380
        QVERIFY2(sales.contains(QLocale().toString(380.0, 'f', 2) + QStringLiteral(" €")), qPrintable(sales));
    }

    void test_build_dividendsTableShowsTaxesAndSumRow()
    {
        // 23.10.2026: Steuern aufgeschlüsselt, Einheiten, Summen für Brutto,
        // Steuern und Netto.
        CalculationReportShare s = consistentShare();
        s.dividends = { DividendObject(QStringLiteral("d1"), QStringLiteral("share-1"),
                                       iso(d(2026, 3, 10)), 1.0, 12.0, 1.0, 2.0, 0.11) };

        const QString report = CalculationReport::build(inputFor({ s }));
        const qsizetype from = report.indexOf(QStringLiteral("Dividenden"));
        const QString divs   = report.mid(from, report.indexOf(QStringLiteral("\nKosten"), from) - from);

        QVERIFY2(divs.contains(QStringLiteral("Quellensteuer")), qPrintable(divs));
        QVERIFY2(divs.contains(QStringLiteral("Kapitalertragst.")), qPrintable(divs));
        QVERIFY2(divs.contains(QStringLiteral("Soli")), qPrintable(divs));
        QVERIFY2(divs.contains(QStringLiteral("Summe")), qPrintable(divs));
        QVERIFY2(divs.contains(QLocale().toString(12.0, 'f', 4) + QStringLiteral(" Stk.")), qPrintable(divs));
        // Brutto 12,00, Steuern 1,00 + 2,00 + 0,11 = 3,11, Netto 8,89
        QVERIFY2(divs.contains(QLocale().toString(3.11, 'f', 2) + QStringLiteral(" €")), qPrintable(divs));
        QVERIFY2(divs.contains(QLocale().toString(8.89, 'f', 2) + QStringLiteral(" €")), qPrintable(divs));
    }

    void test_build_longTable_repeatsHeaderEvery40Rows()
    {
        // 85 Käufe: Titelleiste am Anfang, nach Zeile 40 und nach Zeile 80 —
        // also dreimal; vor der Summenzeile nicht noch einmal.
        CalculationReportShare s = consistentShare();
        s.buys.clear();
        for (int i = 0; i < 85; ++i)
            s.buys.append(buy(QStringLiteral("b%1").arg(i), d(2026, 1, 1).addDays(i), 1.0, 50.0));

        const QString report = CalculationReport::build(inputFor({ s }));
        const qsizetype from = report.indexOf(QStringLiteral("Käufe"));
        const QString buys   = report.mid(from, report.indexOf(QStringLiteral("Verkäufe"), from) - from);

        QCOMPARE(buys.count(QStringLiteral("Kaufwert m. Kosten")), 3);

        // Die zweite Titelleiste steht direkt nach der 40. Datenzeile.
        const QStringList lines = buys.split(QLatin1Char('\n'));
        int headerLine = -1, seen = 0;
        for (int i = 0; i < lines.size(); ++i) {
            if (lines.at(i).contains(QStringLiteral("Kaufwert m. Kosten")) && ++seen == 2) {
                headerLine = i;
                break;
            }
        }
        QVERIFY(headerLine > 0);
        QVERIFY(lines.at(headerLine - 1).contains(d(2026, 1, 1).addDays(39).toString(QStringLiteral("dd.MM.yyyy"))));
        QVERIFY(lines.at(headerLine + 1).trimmed().startsWith(QStringLiteral("---")));
    }

    void test_build_shortTable_headerOnlyOnce()
    {
        const QString report = CalculationReport::build(inputFor({ consistentShare() }));
        const qsizetype from = report.indexOf(QStringLiteral("Käufe"));
        const QString buys   = report.mid(from, report.indexOf(QStringLiteral("Verkäufe"), from) - from);
        QCOMPARE(buys.count(QStringLiteral("Kaufwert m. Kosten")), 1);
    }

    void test_build_calculationWaysStandNextToTheirRow()
    {
        // 23.10.2026: Rechenweg rechts neben der Zeile, nicht als Liste
        // darunter. Unterschiedliche Rechenwege: Depotwert in der
        // Fortsetzungszeile direkt unter der Marktwert-Zeile.
        const QString report = CalculationReport::build(inputFor({ consistentShare() }));
        QVERIFY(!report.contains(QStringLiteral("Rechenwege:")));

        const QStringList lines = report.split(QLatin1Char('\n'));
        int row = -1;
        for (int i = 0; i < lines.size(); ++i) {
            if (lines.at(i).contains(QStringLiteral("Einzahlung (gehaltene Stücke)"))) {
                row = i;
                break;
            }
        }
        QVERIFY(row >= 0);
        QVERIFY2(lines.at(row).contains(QStringLiteral("Marktwert: Σ Reststück × Kaufkurs je Kauf")),
                 qPrintable(lines.at(row)));
        QVERIFY2(lines.at(row + 1).trimmed().startsWith(QStringLiteral("Depotwert: ")),
                 qPrintable(lines.at(row + 1)));
        // Marktwert-Spalte und Rechenweg-Spalte beginnen in beiden Zeilen an
        // derselben Stelle.
        QCOMPARE(lines.at(row).indexOf(QStringLiteral("Marktwert: ")),
                 lines.at(row + 1).indexOf(QStringLiteral("Depotwert: ")));

        // Gleicher Rechenweg: eine Zeile mit "beide:".
        for (const QString& line : lines) {
            if (line.contains(QStringLiteral("Kurswert gehaltener Stücke"))
                && line.contains(QStringLiteral("beide:"))) {
                return;
            }
        }
        QFAIL("Zeile \"Kurswert gehaltener Stücke\" mit \"beide:\" fehlt");
    }

    void test_build_sideBySideTable_ruleAfterEveryItem()
    {
        // 23.10.2026: nach jedem der 14 Punkte eine Trennlinie, dazu die
        // Linie unter der Überschrift — 15 Linien in der Tabelle.
        const QString report = CalculationReport::build(inputFor({ consistentShare() }));
        const qsizetype from = report.indexOf(QStringLiteral("Tabellen \"Kompletter Marktwert\""));
        const qsizetype to   = report.indexOf(QStringLiteral("Charts (letzter Stichtag"), from);
        const QStringList lines = report.mid(from, to - from).split(QLatin1Char('\n'));

        int rules = 0;
        for (int i = 2; i < lines.size(); ++i) // die ersten zwei Zeilen: Abschnittstitel und Unterstrich
            if (lines.at(i).startsWith(QStringLiteral("  ---")))
                ++rules;
        QCOMPARE(rules, 15);
    }

    void test_build_foreignCurrencyDividend_rateShowsCurrency()
    {
        // Bugfix 24.10.2026 (Nessies Bericht, Dividende in USD): der Satz je
        // Stück trug "€", obwohl er in der Belegwährung vorliegt.
        CalculationReportShare s = consistentShare();
        s.dividends = { DividendObject(QStringLiteral("d1"), QStringLiteral("share-1"),
                                       iso(d(2024, 6, 17)), 0.2, 200.0, 5.57, 3.71, 0.20,
                                       0.0, /*enableForeignCurrency*/ true, 1.0778,
                                       QStringLiteral("USD")) };

        const QString report = CalculationReport::build(inputFor({ s }));
        QVERIFY2(report.contains(QLocale().toString(0.2, 'f', 4) + QStringLiteral(" USD")),
                 qPrintable(report));
    }

    void test_build_tinyDifferenceIsNotShownAsNegativeZero()
    {
        // Bugfix 24.10.2026: eine Rechendifferenz nahe null erschien als
        // "-0,00".
        CalculationReportShare s = consistentShare();
        s.values.completeProfitLoss = 144.0 - 1e-9;

        const QString report = CalculationReport::build(inputFor({ s }));
        QVERIFY(!report.contains(QStringLiteral("-0,00")));
    }

    // ── anonymize() (ergänzt 24.10.2026) ─────────────────────────────────

    void test_anonymize_replacesIdentifiersKeepsValues()
    {
        CalculationReportShare a = consistentShare();
        a.buys  = { buy(QStringLiteral("b1"), d(2026, 1, 10), 12.0, 50.0, QStringLiteral("501403950")) };
        a.sales = { sale(QStringLiteral("s1"), d(2026, 4, 1), 2.0, 60.0, QStringLiteral("501403950")) };

        CalculationReportShare b = consistentShare();
        b.share = ShareObject(QStringLiteral("share-2"), QStringLiteral("B0BBBB"),
                              QStringLiteral("DE000B0BBBB0"), QStringLiteral("Aktie Beta AG"));
        b.buys  = { buy(QStringLiteral("b2"), d(2026, 2, 1), 5.0, 80.0, QStringLiteral("8006189848")),
                    buy(QStringLiteral("b3"), d(2026, 3, 1), 5.0, 82.0, QStringLiteral("501403950")) };

        const CalculationReportInput anon = CalculationReport::anonymize(inputFor({ a, b }));

        QVERIFY(anon.anonymized);
        QCOMPARE(anon.portfolioPath, QStringLiteral("(anonymisiert)"));
        QCOMPARE(anon.shares.at(0).share.name(), QStringLiteral("Aktie 1"));
        QCOMPARE(anon.shares.at(0).share.wkn(),  QStringLiteral("WKN-1"));
        QCOMPARE(anon.shares.at(1).share.isin(), QStringLiteral("ISIN-2"));

        // Gleiche Depotnummer = gleiche Bezeichnung, auch über Aktien hinweg.
        QCOMPARE(anon.shares.at(0).buys.at(0).depotNumber(),  QStringLiteral("Depot 1"));
        QCOMPARE(anon.shares.at(0).sales.at(0).depotNumber(), QStringLiteral("Depot 1"));
        QCOMPARE(anon.shares.at(1).buys.at(0).depotNumber(),  QStringLiteral("Depot 2"));
        QCOMPARE(anon.shares.at(1).buys.at(1).depotNumber(),  QStringLiteral("Depot 1"));

        // Werte unverändert.
        QCOMPARE(anon.shares.at(1).buys.at(1).price(), 82.0);
        QCOMPARE(anon.shares.at(0).sales.at(0).volume(), 2.0);
        QCOMPARE(anon.shares.at(0).values.curValue, 744.0);
    }

    void test_anonymize_reportContainsNoRealIdentifiers()
    {
        CalculationReportShare s = consistentShare();
        s.buys = { buy(QStringLiteral("b1"), d(2026, 1, 10), 12.0, 50.0, QStringLiteral("501403950")) };

        const QString real = CalculationReport::build(inputFor({ s }));
        const QString anon = CalculationReport::build(CalculationReport::anonymize(inputFor({ s })));

        for (const QString& secret : { QStringLiteral("Aktie A"), QStringLiteral("A0ABCD"),
                                       QStringLiteral("DE000A0ABCD0"), QStringLiteral("501403950"),
                                       QStringLiteral("/tmp/test.db") }) {
            QVERIFY2(real.contains(secret), qPrintable(secret));
            QVERIFY2(!anon.contains(secret), qPrintable(secret));
        }
        QVERIFY(anon.contains(QStringLiteral("ANONYMISIERT")));
        QVERIFY(!real.contains(QStringLiteral("ANONYMISIERT")));
        // Beträge bleiben gleich.
        QVERIFY(anon.contains(QLocale().toString(744.0, 'f', 2) + QStringLiteral(" €")));
    }

    void test_anonymizedFilePath()
    {
        QCOMPARE(CalculationReport::anonymizedFilePath(QStringLiteral("/home/x/bericht.txt")),
                 QStringLiteral("/home/x/bericht-anonymisiert.txt"));
        QCOMPARE(CalculationReport::anonymizedFilePath(QStringLiteral("/home/x/bericht")),
                 QStringLiteral("/home/x/bericht-anonymisiert.txt"));
        QCOMPARE(CalculationReport::anonymizedFilePath(QStringLiteral("/home/x/a.b.txt")),
                 QStringLiteral("/home/x/a.b-anonymisiert.txt"));
    }

    void test_build_soldShareIsListedAndMarked()
    {
        CalculationReportShare s = consistentShare();
        s.values.volume = 0.0;
        s.sales = { sale(QStringLiteral("s1"), d(2026, 5, 1), 12.0, 70.0) };

        const QString report = CalculationReport::build(inputFor({ s }));
        QVERIFY(report.contains(QStringLiteral("vollständig verkauft")));
        QVERIFY(report.contains(QStringLiteral("Verkäufe")));
    }

    void test_build_shareWithoutDailyValues_isNamedInFindings()
    {
        CalculationReportShare s = consistentShare();
        s.chartExcluded = true;
        s.chart = PortfolioSharePoint{};

        const QString findings = partThree(CalculationReport::build(inputFor({ s })));
        QVERIFY2(findings.contains(QStringLiteral("Aktie A: keine Tageswerte")), qPrintable(findings));
    }

    // Nebenbefund aus Nessies Testdepot (26.10.2026): eine verkaufte Aktie
    // ohne Tageswerte zählt mit ihrer Kompletten Entwicklung im Footer, fehlt
    // aber im Chart. Die Differenz ist erwartbar und gehört zu den Hinweisen,
    // nicht zu den Abweichungen.
    static CalculationReportShare soldShareWithoutDailyValues(double completeProfitLoss)
    {
        CalculationReportShare s;
        s.share = ShareObject(QStringLiteral("share-4"), QStringLiteral("B0BCDE"),
                              QStringLiteral("DE000B0BCDE0"), QStringLiteral("Aktie B"));
        s.values.completePurchase   = 6380.24;
        s.values.completeCurValue   = 6380.24 + completeProfitLoss;
        s.values.completeProfitLoss = completeProfitLoss;
        s.chartExcluded = true;
        return s;
    }

    static QString deviationsSection(const QString& report)
    {
        const QString three = partThree(report);
        const qsizetype from = three.indexOf(QStringLiteral("Abweichungen"));
        const qsizetype to   = three.indexOf(QStringLiteral("Hinweise"));
        return three.mid(from, to < 0 ? -1 : to - from);
    }

    void test_build_excludedShare_explainsFooterDifference()
    {
        const CalculationReportInput input =
            inputFor({ consistentShare(), soldShareWithoutDailyValues(1279.86) });
        const QString report = CalculationReport::build(input);

        const QString deviations = deviationsSection(report);
        QVERIFY2(!deviations.contains(QStringLiteral("GESAMT")), qPrintable(deviations));
        QVERIFY2(report.contains(QStringLiteral("(Hinweis, durch Aktien o. Tageswerte erklärt) GESAMT: "
                                                "Depotwert: Footer \"Komplette Entwicklung\"")),
                 qPrintable(partThree(report)));
        // Die fehlenden Tageswerte selbst bleiben als Befund stehen.
        QVERIFY2(deviations.contains(QStringLiteral("Aktie B: keine Tageswerte")), qPrintable(deviations));
    }

    void test_build_excludedShare_restBeyondItStaysDeviation()
    {
        CalculationReportInput input =
            inputFor({ consistentShare(), soldShareWithoutDailyValues(1279.86) });
        input.chartTotal.development -= 10.0; // Chart liegt zusätzlich 10,00 darunter

        const QString deviations = deviationsSection(CalculationReport::build(input));
        const QLocale loc;
        QVERIFY2(deviations.contains(QStringLiteral("durch Aktien ohne Tageswerte ")
                                     + loc.toString(1279.86, 'f', 2))
                     && deviations.contains(QStringLiteral("Rest ") + loc.toString(10.0, 'f', 2)),
                 qPrintable(deviations));
    }

    void test_build_emptyPortfolio()
    {
        const QString report = CalculationReport::build(inputFor({}));
        QVERIFY(report.contains(QStringLiteral("Keine Aktien im Portfolio.")));
        QVERIFY(report.contains(QStringLiteral("TEIL 3")));
    }

    // ── replayFifoSold() ─────────────────────────────────────────────────

    void test_replayFifoSold_oldestBuyFirst()
    {
        const QList<double> sold = CalculationReport::replayFifoSold(
            { buy(QStringLiteral("b1"), d(2026, 1, 1), 10.0, 100.0),
              buy(QStringLiteral("b2"), d(2026, 2, 1), 10.0, 110.0) },
            { sale(QStringLiteral("s1"), d(2026, 3, 1), 15.0, 120.0) },
            {});

        QCOMPARE(sold.size(), 2);
        QCOMPARE(sold.at(0), 10.0);
        QCOMPARE(sold.at(1), 5.0);
    }

    void test_replayFifoSold_ignoresStoredVolumeSold()
    {
        // Gespeichert steht ein falscher Wert — die Nachrechnung beginnt bei 0.
        const QList<double> sold = CalculationReport::replayFifoSold(
            { buy(QStringLiteral("b1"), d(2026, 1, 1), 10.0, 100.0, QString(), 7.0) },
            { sale(QStringLiteral("s1"), d(2026, 3, 1), 4.0, 120.0) },
            {});

        QCOMPARE(sold.at(0), 4.0);
    }

    void test_replayFifoSold_onlyBuysOfTheSameDepot()
    {
        const QList<double> sold = CalculationReport::replayFifoSold(
            { buy(QStringLiteral("b1"), d(2026, 1, 1), 10.0, 100.0, QStringLiteral("111")),
              buy(QStringLiteral("b2"), d(2026, 2, 1), 10.0, 110.0, QStringLiteral("222")) },
            { sale(QStringLiteral("s1"), d(2026, 3, 1), 5.0, 120.0, QStringLiteral("222")) },
            {});

        QCOMPARE(sold.at(0), 0.0);
        QCOMPARE(sold.at(1), 5.0);
    }

    void test_replayFifoSold_buyAfterSaleIsNotUsed()
    {
        const QList<double> sold = CalculationReport::replayFifoSold(
            { buy(QStringLiteral("b1"), d(2026, 1, 1), 10.0, 100.0),
              buy(QStringLiteral("b2"), d(2026, 5, 1), 10.0, 110.0) },
            { sale(QStringLiteral("s1"), d(2026, 3, 1), 12.0, 120.0) },
            {});

        // Nur 10 Stück waren am Verkaufstag vorhanden; der Rest bleibt offen.
        QCOMPARE(sold.at(0), 10.0);
        QCOMPARE(sold.at(1), 0.0);
    }

    // ── collect() gegen die Datenbank ────────────────────────────────────

    void initTestCase()
    {
        QVERIFY(Database::instance().open(QStringLiteral(":memory:")));
    }

    void cleanupTestCase()
    {
        Database::instance().close();
    }

    void test_collect_tableAndChartValuesForOneShare()
    {
        const QString guid = QStringLiteral("share-db-1");
        ShareObject share(guid, QStringLiteral("DB0001"), QString(), QStringLiteral("DB Aktie"));
        share.setCurPrice(120.0);
        QVERIFY(ShareRepository().insert(share));
        // insert() übernimmt den Kurs nicht zwingend — für die Tabelle zählt,
        // was findAll() liefert; deshalb wird der Erwartungswert daraus gelesen.

        QVERIFY(BuyRepository().insert(BuyObject(QStringLiteral("db-b1"), guid, QString(),
                                                 QStringLiteral("o1"), iso(d(2026, 1, 1)),
                                                 10.0, 0.0, 100.0)));

        DailyValuesRepository dv;
        dv.upsert(DailyValuesObject(guid, d(2026, 1, 1), 100.0, 100.0, 100.0, 100.0, 0.0));
        dv.upsert(DailyValuesObject(guid, d(2026, 2, 1), 110.0, 110.0, 110.0, 110.0, 0.0));

        const CalculationReportInput input =
            CalculationReport::collect(QDateTime::currentDateTime(), QStringLiteral(":memory:"));

        QCOMPARE(input.shares.size(), 1);
        const CalculationReportShare& s = input.shares.constFirst();
        QCOMPARE(s.buys.size(), 1);
        QVERIFY(qAbs(s.values.volume - 10.0) < 1e-9);
        QVERIFY(!s.chartExcluded);
        QCOMPARE(s.chart.date, d(2026, 2, 1));
        QVERIFY(qAbs(s.chart.marketValue - 1100.0) < 0.005);
        QVERIFY(qAbs(input.chartTotal.marketValue - 1100.0) < 0.005);

        const QString report = CalculationReport::build(input);
        QVERIFY(report.contains(QStringLiteral("AKTIE: DB Aktie")));
    }

    // ── writeToFile() ────────────────────────────────────────────────────

    void test_writeToFile_utf8WithBom()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("bericht.txt"));

        QVERIFY(CalculationReport::writeToFile(path, QStringLiteral("Prüfung äöü\n")));

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray bytes = file.readAll();
        QVERIFY(bytes.startsWith(QByteArray("\xEF\xBB\xBF")));
        QVERIFY(QString::fromUtf8(bytes.mid(3)).contains(QStringLiteral("Prüfung äöü")));
    }

    void test_writeToFile_reportsErrorForInvalidPath()
    {
        QString error;
        QVERIFY(!CalculationReport::writeToFile(QStringLiteral("/nonexistent-dir/x/bericht.txt"),
                                                QStringLiteral("x"), &error));
        QVERIFY(!error.isEmpty());
    }
};

int main(int argc, char* argv[])
{
    // QCoreApplication ist für Qt SQL zwingend (siehe tst_portfoliochartform).
    QCoreApplication app(argc, argv);
    // Deutsches Zahlenformat wie in der Anwendung (siehe main.cpp).
    QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));

    TestCalculationReport t;
    return QTest::qExec(&t, argc, argv);
}

#include "tst_calculationreport.moc"

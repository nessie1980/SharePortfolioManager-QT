// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include <QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDir>
#include <QUuid>

#include "XmlPortfolioParser.h"
#include "PortfolioImporter.h"
#include "ImportLogger.h"
#include "../../app/core/Database.h"

class TestPortfolioImporter : public QObject
{
    Q_OBJECT

private:
    QString newGuid() const { return QUuid::createUuid().toString(QUuid::WithoutBraces); }

    QSqlQuery query(const QString& sql) const
    {
        QSqlQuery q(QSqlDatabase::database(Database::connectionName()));
        q.exec(sql);
        return q;
    }

    ImportLogger makeLogger() const
    {
        return ImportLogger(QDir::temp().filePath("tst_portfolioimporter.log"));
    }

    // ── Rohling-Fabriken mit sinnvollen Defaults ────────────────────────────

    RawShare makeShare(const QString& wkn) const
    {
        RawShare s;
        s.wkn = wkn;
        s.isin = QStringLiteral("ISIN-%1").arg(wkn);
        s.name = QStringLiteral("Test %1").arg(wkn);
        s.updateStr = QStringLiteral("Both");
        s.stockMarketLaunchDate = QStringLiteral("01.01.2000");
        s.lastUpdateInternet = QStringLiteral("01.01.2024 10:00");
        s.lastUpdateShareDate = QStringLiteral("01.01.2024 10:00");
        s.sharePrice = QStringLiteral("10,00");
        s.sharePriceBefore = QStringLiteral("9,50");
        s.shareTypeStr = QStringLiteral("0");
        return s;
    }

    RawBuy makeBuy(const QString& guid, const QString& orderNumber,
                   const QString& brokerageGuid = QString()) const
    {
        RawBuy b;
        b.guid = guid;
        b.depotNumber = QStringLiteral("D1");
        b.orderNumber = orderNumber;
        b.date = QStringLiteral("01.01.2024");
        b.volume = QStringLiteral("10,00");
        b.volumeSold = QStringLiteral("0,00");
        b.price = QStringLiteral("10,00");
        b.brokerageGuid = brokerageGuid;
        return b;
    }

    RawSale makeSale(const QString& guid, const QString& orderNumber, const QString& buyGuid,
                     const QString& brokerageGuid = QString()) const
    {
        RawSale s;
        s.guid = guid;
        s.depotNumber = QStringLiteral("D1");
        s.orderNumber = orderNumber;
        s.date = QStringLiteral("02.01.2024");
        s.volume = QStringLiteral("10,00");
        s.salePrice = QStringLiteral("11,00");
        s.taxAtSource = QStringLiteral("0,00");
        s.capitalGainsTax = QStringLiteral("2,00");
        s.solidarityTax = QStringLiteral("0,11");
        s.brokerageGuid = brokerageGuid;

        RawUsedBuy ub;
        ub.buyGuid = buyGuid;
        ub.buyDate = QStringLiteral("01.01.2024");
        ub.buyVolume = QStringLiteral("10,00");
        ub.buyPrice = QStringLiteral("10,00");
        ub.reduction = QStringLiteral("0,00");
        ub.brokerage = QStringLiteral("0,00");
        s.usedBuys.append(ub);
        return s;
    }

    void resetDb()
    {
        Database::instance().execute(QStringLiteral("DELETE FROM daily_values"));
        Database::instance().execute(QStringLiteral("DELETE FROM dividends"));
        Database::instance().execute(QStringLiteral("DELETE FROM sale_buy_details"));
        Database::instance().execute(QStringLiteral("DELETE FROM brokerage"));
        Database::instance().execute(QStringLiteral("DELETE FROM sales"));
        Database::instance().execute(QStringLiteral("DELETE FROM buys"));
        Database::instance().execute(QStringLiteral("DELETE FROM shares"));
    }

private slots:

    void initTestCase()
    {
        QVERIFY(Database::instance().open(":memory:"));
    }

    void cleanupTestCase()
    {
        Database::instance().close();
    }

    void init()
    {
        resetDb();
    }

    // ── Share: Neuanlage + Wiederverwendung per WKN ─────────────────────────

    void test_importShare_insertsNewShare()
    {
        RawPortfolio portfolio;
        portfolio.shares.append(makeShare(QStringLiteral("TEST01")));

        ImportLogger logger = makeLogger();
        PortfolioImporter(logger, false).importPortfolio(portfolio);

        QSqlQuery q = query(QStringLiteral("SELECT wkn, isin, name FROM shares"));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("TEST01"));
        QCOMPARE(q.value(1).toString(), QStringLiteral("ISIN-TEST01"));
        QVERIFY(!q.next()); // genau eine Zeile
    }

    void test_importShare_reusesExistingShareByWkn_masterDataUntouched()
    {
        RawPortfolio portfolio1;
        portfolio1.shares.append(makeShare(QStringLiteral("TEST02")));
        ImportLogger logger1 = makeLogger();
        PortfolioImporter(logger1, false).importPortfolio(portfolio1);

        QSqlQuery q1 = query(QStringLiteral("SELECT guid FROM shares WHERE wkn='TEST02'"));
        QVERIFY(q1.next());
        const QString firstGuid = q1.value(0).toString();

        // Zweiter Import derselben WKN mit geändertem Namen — darf NICHT übernommen
        // werden (Stammdaten bleiben bei Wiederverwendung unangetastet).
        RawPortfolio portfolio2;
        RawShare share2 = makeShare(QStringLiteral("TEST02"));
        share2.name = QStringLiteral("Sollte nicht uebernommen werden");
        portfolio2.shares.append(share2);
        ImportLogger logger2 = makeLogger();
        PortfolioImporter(logger2, false).importPortfolio(portfolio2);

        QSqlQuery q2 = query(QStringLiteral(
            "SELECT COUNT(*), MIN(guid), MIN(name) FROM shares WHERE wkn='TEST02'"));
        QVERIFY(q2.next());
        QCOMPARE(q2.value(0).toInt(), 1);            // keine Dublette
        QCOMPARE(q2.value(1).toString(), firstGuid);  // GUID wiederverwendet
        QCOMPARE(q2.value(2).toString(), QStringLiteral("Test TEST02")); // Stammdaten unverändert
    }

    // ── Idempotenz: GUID-basiertes Deduplizieren bei erneutem Import ───────

    void test_importBuy_isIdempotentOnRerun()
    {
        RawShare share = makeShare(QStringLiteral("TEST03"));
        share.buys.append(makeBuy(newGuid(), QStringLiteral("ORD-100")));

        RawPortfolio portfolio;
        portfolio.shares.append(share);

        ImportLogger logger1 = makeLogger();
        PortfolioImporter(logger1, false).importPortfolio(portfolio);
        ImportLogger logger2 = makeLogger();
        PortfolioImporter(logger2, false).importPortfolio(portfolio); // identischer zweiter Lauf

        QSqlQuery q = query(QStringLiteral("SELECT COUNT(*) FROM buys"));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 1); // keine Dublette trotz zweitem Lauf
    }

    // ── OrderNumber-Kollision: Fehler wird geloggt, Import läuft weiter ─────
    // (Regressionstest für den AGIF/Facebook-Fall vom 01.07.2026)

    void test_importBuy_orderNumberCollision_skipsSecondButContinues()
    {
        RawShare share = makeShare(QStringLiteral("TEST04"));
        share.buys.append(makeBuy(newGuid(), QStringLiteral("DUP-ORDER"))); // erster Buy
        share.buys.append(makeBuy(newGuid(), QStringLiteral("DUP-ORDER"))); // gleiche OrderNumber, andere GUID

        RawPortfolio portfolio;
        portfolio.shares.append(share);

        ImportLogger logger = makeLogger();
        PortfolioImporter(logger, false).importPortfolio(portfolio); // darf nicht abbrechen

        QSqlQuery q = query(QStringLiteral("SELECT COUNT(*) FROM buys WHERE order_number='DUP-ORDER'"));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 1); // nur der erste Buy wurde gespeichert

        // Der Import muss trotz des Fehlers weiterlaufen — die Aktie selbst existiert.
        QSqlQuery q2 = query(QStringLiteral("SELECT COUNT(*) FROM shares WHERE wkn='TEST04'"));
        QVERIFY(q2.next());
        QCOMPARE(q2.value(0).toInt(), 1);
    }

    // ── Regressionstests: Brokerage-Zuordnung vom 02.07.2026 ────────────────
    // BuyPart/SalePart in der Quelle können falsch gesetzt sein (siehe
    // Mensch u. Maschine / Procter & Gamble, Import vom 01.07.2026). Die
    // Zuordnung wird gegen buys/sales verifiziert statt den Flags zu vertrauen.

    void test_importBrokerage_correctsWrongBuyPartFlag()
    {
        RawShare share = makeShare(QStringLiteral("TEST05"));
        const QString buyGuid  = newGuid();
        const QString saleGuid = newGuid();
        share.buys.append(makeBuy(buyGuid, QStringLiteral("ORD-B1")));
        share.sales.append(makeSale(saleGuid, QStringLiteral("ORD-S1"), buyGuid));

        // Fehlerhafte Quelle: BuyPart="True", obwohl GuidBuySale auf die Sale zeigt.
        RawBrokerage b;
        b.guid = newGuid();
        b.buyPart = true;
        b.salePart = false;
        b.guidBuySale = saleGuid;
        b.date = QStringLiteral("02.01.2024");
        b.provision = QStringLiteral("12,91");
        share.brokerages.append(b);

        RawPortfolio portfolio;
        portfolio.shares.append(share);
        ImportLogger logger = makeLogger();
        PortfolioImporter(logger, false).importPortfolio(portfolio);

        QSqlQuery q = query(QStringLiteral(
            "SELECT buy_guid, sale_guid FROM brokerage WHERE guid='%1'").arg(b.guid));
        QVERIFY(q.next());
        QVERIFY(q.value(0).toString().isEmpty());  // buy_guid NICHT gesetzt
        QCOMPARE(q.value(1).toString(), saleGuid);   // sale_guid korrekt gesetzt
    }

    void test_importBrokerage_correctsWrongSalePartFlag()
    {
        RawShare share = makeShare(QStringLiteral("TEST06"));
        const QString buyGuid  = newGuid();
        const QString saleGuid = newGuid();
        share.buys.append(makeBuy(buyGuid, QStringLiteral("ORD-B2")));
        share.sales.append(makeSale(saleGuid, QStringLiteral("ORD-S2"), buyGuid));

        // Spiegelfall: SalePart="True", obwohl GuidBuySale auf den Buy zeigt.
        RawBrokerage b;
        b.guid = newGuid();
        b.buyPart = false;
        b.salePart = true;
        b.guidBuySale = buyGuid;
        b.date = QStringLiteral("01.01.2024");
        b.provision = QStringLiteral("1,00");
        share.brokerages.append(b);

        RawPortfolio portfolio;
        portfolio.shares.append(share);
        ImportLogger logger = makeLogger();
        PortfolioImporter(logger, false).importPortfolio(portfolio);

        QSqlQuery q = query(QStringLiteral(
            "SELECT buy_guid, sale_guid FROM brokerage WHERE guid='%1'").arg(b.guid));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(), buyGuid);   // buy_guid korrekt gesetzt
        QVERIFY(q.value(1).toString().isEmpty());    // sale_guid NICHT gesetzt
    }

    void test_importBrokerage_correctFlags_areAcceptedUnchanged()
    {
        RawShare share = makeShare(QStringLiteral("TEST07"));
        const QString buyGuid = newGuid();
        share.buys.append(makeBuy(buyGuid, QStringLiteral("ORD-B3")));

        RawBrokerage b;
        b.guid = newGuid();
        b.buyPart = true;
        b.salePart = false;
        b.guidBuySale = buyGuid;
        b.date = QStringLiteral("01.01.2024");
        share.brokerages.append(b);

        RawPortfolio portfolio;
        portfolio.shares.append(share);
        ImportLogger logger = makeLogger();
        PortfolioImporter(logger, false).importPortfolio(portfolio); // Kontrollfall: darf nicht scheitern

        QSqlQuery q = query(QStringLiteral(
            "SELECT buy_guid FROM brokerage WHERE guid='%1'").arg(b.guid));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(), buyGuid);
    }

    void test_importBrokerage_guidBuySaleNotFoundInEitherTable_isSkipped()
    {
        RawShare share = makeShare(QStringLiteral("TEST08"));
        RawBrokerage b;
        b.guid = newGuid();
        b.buyPart = true;
        b.guidBuySale = newGuid(); // existiert weder als Buy noch als Sale
        b.date = QStringLiteral("01.01.2024");
        share.brokerages.append(b);

        RawPortfolio portfolio;
        portfolio.shares.append(share);
        ImportLogger logger = makeLogger();
        PortfolioImporter(logger, false).importPortfolio(portfolio); // darf nicht abstürzen

        QSqlQuery q = query(QStringLiteral("SELECT COUNT(*) FROM brokerage"));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 0); // nichts eingefügt
    }

    // ── Dividende mit Fremdwährung ───────────────────────────────────────────

    void test_importDividend_foreignCurrencyFieldsAreStored()
    {
        RawShare share = makeShare(QStringLiteral("TEST09"));
        RawDividend d;
        d.guid = newGuid();
        d.date = QStringLiteral("01.07.2024");
        d.rate = QStringLiteral("0,485");
        d.volume = QStringLiteral("125");
        d.taxAtSource = QStringLiteral("0,00");
        d.capitalGainsTax = QStringLiteral("12,00");
        d.solidarityTax = QStringLiteral("0,66");
        d.price = QStringLiteral("65,00");
        d.hasForeignCurrency = true;
        d.fc.enabled = true;
        d.fc.exchangeRatio = QStringLiteral("1,08");
        d.fc.currency = QStringLiteral("en-US");
        share.dividends.append(d);

        RawPortfolio portfolio;
        portfolio.shares.append(share);
        ImportLogger logger = makeLogger();
        PortfolioImporter(logger, false).importPortfolio(portfolio);

        QSqlQuery q = query(QStringLiteral(
            "SELECT enable_fc, exchange_ratio, currency FROM dividends WHERE guid='%1'").arg(d.guid));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toBool(), true);
        QCOMPARE(q.value(1).toDouble(), 1.08);
        QCOMPARE(q.value(2).toString(), QStringLiteral("en-US"));
    }

    // ── Dry-Run: keine Schreibzugriffe ───────────────────────────────────────

    void test_dryRun_writesNothing()
    {
        RawShare share = makeShare(QStringLiteral("TEST10"));
        share.buys.append(makeBuy(newGuid(), QStringLiteral("ORD-DRY")));

        RawPortfolio portfolio;
        portfolio.shares.append(share);
        ImportLogger logger = makeLogger();
        PortfolioImporter(logger, /*dryRun=*/true).importPortfolio(portfolio);

        QSqlQuery q1 = query(QStringLiteral("SELECT COUNT(*) FROM shares"));
        QVERIFY(q1.next());
        QCOMPARE(q1.value(0).toInt(), 0);

        QSqlQuery q2 = query(QStringLiteral("SELECT COUNT(*) FROM buys"));
        QVERIFY(q2.next());
        QCOMPARE(q2.value(0).toInt(), 0);
    }

    // ── Tageswerte: INSERT OR REPLACE bei erneutem Import ───────────────────

    void test_dailyValues_upsertReplacesExistingValueOnRerun()
    {
        RawShare share = makeShare(QStringLiteral("TEST11"));
        RawDailyValue e;
        e.date = QStringLiteral("18.08.2015");
        e.open = QStringLiteral("10,00");
        e.close = QStringLiteral("10,50");
        e.top = QStringLiteral("10,80");
        e.bottom = QStringLiteral("9,90");
        e.volume = QStringLiteral("1000");
        share.dailyValues.append(e);

        RawPortfolio portfolio1;
        portfolio1.shares.append(share);
        ImportLogger logger1 = makeLogger();
        PortfolioImporter(logger1, false).importPortfolio(portfolio1);

        // Zweiter Import derselben WKN mit geändertem Schlusskurs am selben Tag.
        RawShare share2 = makeShare(QStringLiteral("TEST11"));
        RawDailyValue e2 = e;
        e2.close = QStringLiteral("11,00");
        share2.dailyValues.append(e2);

        RawPortfolio portfolio2;
        portfolio2.shares.append(share2);
        ImportLogger logger2 = makeLogger();
        PortfolioImporter(logger2, false).importPortfolio(portfolio2);

        QSqlQuery q = query(QStringLiteral(
            "SELECT COUNT(*), MIN(closing) FROM daily_values WHERE date='2015-08-18'"));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 1);        // keine Dublette
        QCOMPARE(q.value(1).toDouble(), 11.00); // aktualisierter Wert
    }

    void test_importDailyValues_logsInsertedUpdatedUnchangedBreakdown()
    {
        const QString logPath = QDir::temp().filePath("tst_portfolioimporter_dailyvalues.log");
        QFile::remove(logPath); // saubere Ausgangslage, Logger öffnet im Append-Modus

        RawShare share = makeShare(QStringLiteral("TEST12"));
        RawDailyValue e1;
        e1.date = QStringLiteral("13.06.2024");
        e1.open = e1.close = e1.top = e1.bottom = QStringLiteral("100,00");
        e1.volume = QStringLiteral("1000");
        RawDailyValue e2;
        e2.date = QStringLiteral("14.06.2024");
        e2.open = e2.close = e2.top = e2.bottom = QStringLiteral("100,00");
        e2.volume = QStringLiteral("1000");
        share.dailyValues.append(e1);
        share.dailyValues.append(e2);

        {
            ImportLogger logger(logPath);
            RawPortfolio portfolio;
            portfolio.shares.append(share);
            PortfolioImporter(logger, false).importPortfolio(portfolio);
        }

        // Zweiter Import: e1 unverändert, e2 mit geändertem Schlusskurs, e3 neu.
        RawShare share2 = makeShare(QStringLiteral("TEST12"));
        RawDailyValue e2changed = e2;
        e2changed.close = QStringLiteral("111,00");
        RawDailyValue e3;
        e3.date = QStringLiteral("15.06.2024");
        e3.open = e3.close = e3.top = e3.bottom = QStringLiteral("100,00");
        e3.volume = QStringLiteral("1000");
        share2.dailyValues.append(e1);
        share2.dailyValues.append(e2changed);
        share2.dailyValues.append(e3);

        {
            ImportLogger logger(logPath);
            RawPortfolio portfolio2;
            portfolio2.shares.append(share2);
            PortfolioImporter(logger, false).importPortfolio(portfolio2);
        }

        QFile logFile(logPath);
        QVERIFY(logFile.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString logContent = QString::fromUtf8(logFile.readAll());
        logFile.close();

        QVERIFY(logContent.contains(QStringLiteral(
            "3 Tageswert(e) geholt (Eingefügt: 1 / Aktualisiert: 1 / Unverändert: 1)")));

        QFile::remove(logPath);
    }
};

QTEST_MAIN(TestPortfolioImporter)
#include "tst_portfolioimporter.moc"

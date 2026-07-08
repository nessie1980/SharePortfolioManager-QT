// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include <QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>
#include <algorithm>

#include "XmlPortfolioParser.h"
#include "PortfolioValidator.h"
#include "../../app/core/Database.h"

class TestPortfolioValidator : public QObject
{
    Q_OBJECT

private:
    QString newGuid() const { return QUuid::createUuid().toString(QUuid::WithoutBraces); }

    // ── Rohling-Fabrik mit garantiert gültigen Defaults ─────────────────────
    // (jedes Feld einzeln überschreibbar, um genau einen Fehlerfall zu isolieren)
    RawShare makeValidShare(const QString& wkn) const
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
        s.marketValueParsing = QStringLiteral("ApiYahoo");
        s.dailyValuesParsing = QStringLiteral("ApiOnvista");
        return s;
    }

    RawBuy makeValidBuy(const QString& guid, const QString& orderNumber) const
    {
        RawBuy b;
        b.guid = guid;
        b.depotNumber = QStringLiteral("D1");
        b.orderNumber = orderNumber;
        b.date = QStringLiteral("01.01.2024");
        b.volume = QStringLiteral("10,00");
        b.volumeSold = QStringLiteral("0,00");
        b.price = QStringLiteral("10,00");
        return b;
    }

    RawSale makeValidSale(const QString& guid, const QString& orderNumber,
                         const QString& usedBuyGuid) const
    {
        RawSale s;
        s.guid = guid;
        s.depotNumber = QStringLiteral("D1");
        s.orderNumber = orderNumber;
        s.date = QStringLiteral("02.01.2024");
        s.volume = QStringLiteral("10,00");
        s.salePrice = QStringLiteral("11,00");
        RawUsedBuy ub;
        ub.buyGuid = usedBuyGuid;
        ub.buyDate = QStringLiteral("01.01.2024");
        ub.buyVolume = QStringLiteral("10,00");
        ub.buyPrice = QStringLiteral("10,00");
        s.usedBuys.append(ub);
        return s;
    }

    RawBrokerage makeValidBrokerage(const QString& guid, const QString& guidBuySale) const
    {
        RawBrokerage b;
        b.guid = guid;
        b.guidBuySale = guidBuySale;
        b.date = QStringLiteral("01.01.2024");
        b.provision = QStringLiteral("9,90");
        return b;
    }

    RawDividend makeValidDividend(const QString& guid) const
    {
        RawDividend d;
        d.guid = guid;
        d.date = QStringLiteral("01.07.2024");
        d.rate = QStringLiteral("0,50");
        d.volume = QStringLiteral("100");
        return d;
    }

    RawDailyValue makeValidDailyValue(const QString& date) const
    {
        RawDailyValue e;
        e.date = date;
        e.open = e.close = e.top = e.bottom = QStringLiteral("100,00");
        e.volume = QStringLiteral("1000");
        return e;
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
        Database::instance().execute(QStringLiteral("DELETE FROM daily_values"));
        Database::instance().execute(QStringLiteral("DELETE FROM dividends"));
        Database::instance().execute(QStringLiteral("DELETE FROM sale_buy_details"));
        Database::instance().execute(QStringLiteral("DELETE FROM brokerage"));
        Database::instance().execute(QStringLiteral("DELETE FROM sales"));
        Database::instance().execute(QStringLiteral("DELETE FROM buys"));
        Database::instance().execute(QStringLiteral("DELETE FROM shares"));
    }

    // ── Gültiger Fall (Kontrolle) ────────────────────────────────────────────

    void test_validate_completelyValidPortfolio_noIssues()
    {
        RawShare share = makeValidShare(QStringLiteral("VAL01"));
        const QString buyGuid = newGuid();
        share.buys.append(makeValidBuy(buyGuid, QStringLiteral("ORD-1")));
        share.brokerages.append(makeValidBrokerage(newGuid(), buyGuid));
        share.dividends.append(makeValidDividend(newGuid()));
        share.dailyValues.append(makeValidDailyValue(QStringLiteral("18.08.2015")));

        RawPortfolio portfolio;
        portfolio.shares.append(share);

        QList<ValidationIssue> issues;
        QVERIFY(PortfolioValidator::validate(portfolio, issues));
        QVERIFY(issues.isEmpty());
    }

    // ── Share-Ebene ───────────────────────────────────────────────────────

    void test_validate_missingWkn_isReported()
    {
        RawShare share = makeValidShare(QString());
        RawPortfolio portfolio;
        portfolio.shares.append(share);

        QList<ValidationIssue> issues;
        QVERIFY(!PortfolioValidator::validate(portfolio, issues));
        QVERIFY(!issues.isEmpty());
        QVERIFY(issues.first().message.contains(QStringLiteral("WKN")));
    }

    void test_validate_unknownUpdateValue_isReported()
    {
        RawShare share = makeValidShare(QStringLiteral("VAL02"));
        share.updateStr = QStringLiteral("Sometimes"); // Tippfehler-Simulation
        RawPortfolio portfolio;
        portfolio.shares.append(share);

        QList<ValidationIssue> issues;
        QVERIFY(!PortfolioValidator::validate(portfolio, issues));
        QVERIFY(std::any_of(issues.cbegin(), issues.cend(), [](const ValidationIssue& i) {
            return i.recordId == QStringLiteral("Update");
        }));
    }

    void test_validate_unknownShareType_isReported()
    {
        RawShare share = makeValidShare(QStringLiteral("VAL03"));
        share.shareTypeStr = QStringLiteral("9");
        RawPortfolio portfolio;
        portfolio.shares.append(share);

        QList<ValidationIssue> issues;
        QVERIFY(!PortfolioValidator::validate(portfolio, issues));
        QVERIFY(std::any_of(issues.cbegin(), issues.cend(), [](const ValidationIssue& i) {
            return i.recordId == QStringLiteral("ShareType");
        }));
    }

    void test_validate_unknownParsingValue_isReported()
    {
        RawShare share = makeValidShare(QStringLiteral("VAL04"));
        share.marketValueParsing = QStringLiteral("ApiYaho"); // Tippfehler-Simulation
        RawPortfolio portfolio;
        portfolio.shares.append(share);

        QList<ValidationIssue> issues;
        QVERIFY(!PortfolioValidator::validate(portfolio, issues));
        QVERIFY(std::any_of(issues.cbegin(), issues.cend(), [](const ValidationIssue& i) {
            return i.recordId == QStringLiteral("MarketValue.Parsing");
        }));
    }

    void test_validate_emptyParsingValue_isAccepted()
    {
        // Leerer Parsing-Wert ist ein legitimer Zustand ("nicht konfiguriert"),
        // kein Datenfehler — darf NICHT gemeldet werden.
        RawShare share = makeValidShare(QStringLiteral("VAL05"));
        share.marketValueParsing = QString();
        share.dailyValuesParsing = QString();
        RawPortfolio portfolio;
        portfolio.shares.append(share);

        QList<ValidationIssue> issues;
        QVERIFY(PortfolioValidator::validate(portfolio, issues));
        QVERIFY(issues.isEmpty());
    }

    void test_validate_regexParsingValue_isAccepted()
    {
        // "Regex" ist laut ARCHITECTURE.md ein regulärer dritter Parsing-Typ,
        // kein Tippfehler — darf NICHT gemeldet werden.
        RawShare share = makeValidShare(QStringLiteral("VAL06"));
        share.marketValueParsing = QStringLiteral("Regex");
        RawPortfolio portfolio;
        portfolio.shares.append(share);

        QList<ValidationIssue> issues;
        QVERIFY(PortfolioValidator::validate(portfolio, issues));
        QVERIFY(issues.isEmpty());
    }

    void test_validate_unparsableShareDate_isReported()
    {
        RawShare share = makeValidShare(QStringLiteral("VAL07"));
        share.stockMarketLaunchDate = QStringLiteral("32.13.2024"); // ungültiges Datum
        RawPortfolio portfolio;
        portfolio.shares.append(share);

        QList<ValidationIssue> issues;
        QVERIFY(!PortfolioValidator::validate(portfolio, issues));
        QVERIFY(std::any_of(issues.cbegin(), issues.cend(), [](const ValidationIssue& i) {
            return i.recordId == QStringLiteral("StockMarketLaunchDate");
        }));
    }

    void test_validate_parseErrorsFromXmlParser_areIncluded()
    {
        // Simuliert einen von XmlPortfolioParser bereits erkannten strukturellen
        // Fehler (z.B. <MarketValues> statt <MarketValue>, siehe
        // XmlPortfolioParser::parseShare()) — muss 1:1 in die Validierung einfließen.
        RawShare share = makeValidShare(QStringLiteral("VAL08"));
        share.parseErrors.append(QStringLiteral("Unerwartetes Element <MarketValues> ..."));
        RawPortfolio portfolio;
        portfolio.shares.append(share);

        QList<ValidationIssue> issues;
        QVERIFY(!PortfolioValidator::validate(portfolio, issues));
        QVERIFY(std::any_of(issues.cbegin(), issues.cend(), [](const ValidationIssue& i) {
            return i.message.contains(QStringLiteral("MarketValues"));
        }));
    }

    // ── Buy/Sale/Brokerage/Dividend/DailyValue ──────────────────────────────

    void test_validate_buyMissingGuid_isReported()
    {
        RawShare share = makeValidShare(QStringLiteral("VAL09"));
        RawBuy b = makeValidBuy(QString(), QStringLiteral("ORD-1"));
        share.buys.append(b);
        RawPortfolio portfolio;
        portfolio.shares.append(share);

        QList<ValidationIssue> issues;
        QVERIFY(!PortfolioValidator::validate(portfolio, issues));
        QVERIFY(std::any_of(issues.cbegin(), issues.cend(), [](const ValidationIssue& i) {
            return i.category == QStringLiteral("Buy") && i.message.contains(QStringLiteral("GUID"));
        }));
    }

    void test_validate_buyUnparsableDate_isReported()
    {
        RawShare share = makeValidShare(QStringLiteral("VAL10"));
        RawBuy b = makeValidBuy(newGuid(), QStringLiteral("ORD-1"));
        b.date = QStringLiteral("31.02.2024"); // 31. Februar existiert nicht
        share.buys.append(b);
        RawPortfolio portfolio;
        portfolio.shares.append(share);

        QList<ValidationIssue> issues;
        QVERIFY(!PortfolioValidator::validate(portfolio, issues));
        QVERIFY(std::any_of(issues.cbegin(), issues.cend(), [](const ValidationIssue& i) {
            return i.category == QStringLiteral("Buy") && i.message.contains(QStringLiteral("Datum"));
        }));
    }

    void test_validate_duplicateOrderNumberAmongBuysInSameFile_isReported()
    {
        RawShare share = makeValidShare(QStringLiteral("VAL11"));
        share.buys.append(makeValidBuy(newGuid(), QStringLiteral("ORD-DUP")));
        share.buys.append(makeValidBuy(newGuid(), QStringLiteral("ORD-DUP")));
        RawPortfolio portfolio;
        portfolio.shares.append(share);

        QList<ValidationIssue> issues;
        QVERIFY(!PortfolioValidator::validate(portfolio, issues));
        QVERIFY(std::any_of(issues.cbegin(), issues.cend(), [](const ValidationIssue& i) {
            return i.category == QStringLiteral("Buy") && i.recordId == QStringLiteral("ORD-DUP");
        }));
    }

    void test_validate_duplicateGuidAcrossCategoriesInSameShare_isReported()
    {
        RawShare share = makeValidShare(QStringLiteral("VAL12"));
        const QString sharedGuid = newGuid();
        share.buys.append(makeValidBuy(sharedGuid, QStringLiteral("ORD-1")));
        share.dividends.append(makeValidDividend(sharedGuid)); // dieselbe GUID wie der Buy!
        RawPortfolio portfolio;
        portfolio.shares.append(share);

        QList<ValidationIssue> issues;
        QVERIFY(!PortfolioValidator::validate(portfolio, issues));
        QVERIFY(std::any_of(issues.cbegin(), issues.cend(), [&](const ValidationIssue& i) {
            return i.recordId == sharedGuid && i.message.contains(QStringLiteral("mehrfach"));
        }));
    }

    void test_validate_brokerageGuidBuySaleNotFound_isReported()
    {
        RawShare share = makeValidShare(QStringLiteral("VAL13"));
        share.brokerages.append(makeValidBrokerage(newGuid(), newGuid())); // Ziel-GUID existiert nirgends
        RawPortfolio portfolio;
        portfolio.shares.append(share);

        QList<ValidationIssue> issues;
        QVERIFY(!PortfolioValidator::validate(portfolio, issues));
        QVERIFY(std::any_of(issues.cbegin(), issues.cend(), [](const ValidationIssue& i) {
            return i.category == QStringLiteral("Brokerage") &&
                   i.message.contains(QStringLiteral("weder als Buy noch als Sale"));
        }));
    }

    void test_validate_brokerageGuidBuySaleAmbiguous_isReported()
    {
        RawShare share = makeValidShare(QStringLiteral("VAL14"));
        const QString collidingGuid = newGuid();
        share.buys.append(makeValidBuy(collidingGuid, QStringLiteral("ORD-1")));
        share.sales.append(makeValidSale(collidingGuid, QStringLiteral("ORD-2"), collidingGuid));
        // collidingGuid ist absichtlich sowohl Buy- als auch Sale-GUID (Datenfehler-Simulation)
        share.brokerages.append(makeValidBrokerage(newGuid(), collidingGuid));
        RawPortfolio portfolio;
        portfolio.shares.append(share);

        QList<ValidationIssue> issues;
        QVERIFY(!PortfolioValidator::validate(portfolio, issues));
        // Hier schlagen zwangsläufig auch die Duplicate-GUID-Prüfung (collidingGuid
        // taucht als Buy UND Sale auf) sowie die Brokerage-Mehrdeutigkeitsprüfung an —
        // uns interessiert hier nur, dass Letztere zuverlässig dabei ist.
        QVERIFY(std::any_of(issues.cbegin(), issues.cend(), [](const ValidationIssue& i) {
            return i.category == QStringLiteral("Brokerage") &&
                   i.message.contains(QStringLiteral("sowohl als Buy als auch als Sale"));
        }));
    }

    void test_validate_dailyValueUnparsableDate_isReported()
    {
        RawShare share = makeValidShare(QStringLiteral("VAL15"));
        share.dailyValues.append(makeValidDailyValue(QStringLiteral("nicht-ein-datum")));
        RawPortfolio portfolio;
        portfolio.shares.append(share);

        QList<ValidationIssue> issues;
        QVERIFY(!PortfolioValidator::validate(portfolio, issues));
        QVERIFY(std::any_of(issues.cbegin(), issues.cend(), [](const ValidationIssue& i) {
            return i.category == QStringLiteral("DailyValue");
        }));
    }

    // ── Numerische Felder (ergänzt 08.07.2026) ──────────────────────────────

    void test_validate_shareUnparsableSharePrice_isReported()
    {
        RawShare share = makeValidShare(QStringLiteral("VAL19"));
        share.sharePrice = QStringLiteral("nicht-eine-zahl");
        RawPortfolio portfolio;
        portfolio.shares.append(share);

        QList<ValidationIssue> issues;
        QVERIFY(!PortfolioValidator::validate(portfolio, issues));
        QVERIFY(std::any_of(issues.cbegin(), issues.cend(), [](const ValidationIssue& i) {
            return i.category == QStringLiteral("Share") && i.recordId == QStringLiteral("SharePrice");
        }));
    }

    void test_validate_buyUnparsableVolume_isReported()
    {
        RawShare share = makeValidShare(QStringLiteral("VAL20"));
        RawBuy b = makeValidBuy(newGuid(), QStringLiteral("ORD-1"));
        b.volume = QStringLiteral("zehn");
        share.buys.append(b);
        RawPortfolio portfolio;
        portfolio.shares.append(share);

        QList<ValidationIssue> issues;
        QVERIFY(!PortfolioValidator::validate(portfolio, issues));
        QVERIFY(std::any_of(issues.cbegin(), issues.cend(), [](const ValidationIssue& i) {
            return i.category == QStringLiteral("Buy") && i.message.contains(QStringLiteral("Volume"));
        }));
    }

    void test_validate_saleUnparsableSalePrice_isReported()
    {
        RawShare share = makeValidShare(QStringLiteral("VAL21"));
        RawSale s = makeValidSale(newGuid(), QStringLiteral("ORD-1"), newGuid());
        s.salePrice = QStringLiteral("11,00,00"); // doppeltes Komma — kein gültiges Format
        share.sales.append(s);
        RawPortfolio portfolio;
        portfolio.shares.append(share);

        QList<ValidationIssue> issues;
        QVERIFY(!PortfolioValidator::validate(portfolio, issues));
        QVERIFY(std::any_of(issues.cbegin(), issues.cend(), [](const ValidationIssue& i) {
            return i.category == QStringLiteral("Sale") && i.message.contains(QStringLiteral("SalePrice"));
        }));
    }

    void test_validate_usedBuyUnparsableBuyPrice_isReported()
    {
        RawShare share = makeValidShare(QStringLiteral("VAL22"));
        RawSale s = makeValidSale(newGuid(), QStringLiteral("ORD-1"), newGuid());
        s.usedBuys[0].buyPrice = QStringLiteral("??");
        share.sales.append(s);
        RawPortfolio portfolio;
        portfolio.shares.append(share);

        QList<ValidationIssue> issues;
        QVERIFY(!PortfolioValidator::validate(portfolio, issues));
        QVERIFY(std::any_of(issues.cbegin(), issues.cend(), [](const ValidationIssue& i) {
            return i.category == QStringLiteral("Sale") &&
                   i.message.contains(QStringLiteral("UsedBuy")) &&
                   i.message.contains(QStringLiteral("BuyPrice"));
        }));
    }

    void test_validate_brokerageUnparsableProvision_isReported()
    {
        RawShare share = makeValidShare(QStringLiteral("VAL23"));
        const QString buyGuid = newGuid();
        share.buys.append(makeValidBuy(buyGuid, QStringLiteral("ORD-1")));
        RawBrokerage b = makeValidBrokerage(newGuid(), buyGuid);
        b.provision = QStringLiteral("neun-neunzig");
        share.brokerages.append(b);
        RawPortfolio portfolio;
        portfolio.shares.append(share);

        QList<ValidationIssue> issues;
        QVERIFY(!PortfolioValidator::validate(portfolio, issues));
        QVERIFY(std::any_of(issues.cbegin(), issues.cend(), [](const ValidationIssue& i) {
            return i.category == QStringLiteral("Brokerage") &&
                   i.message.contains(QStringLiteral("Provision"));
        }));
    }

    void test_validate_dividendUnparsableRate_isReported()
    {
        RawShare share = makeValidShare(QStringLiteral("VAL24"));
        RawDividend d = makeValidDividend(newGuid());
        d.rate = QStringLiteral("fünfzig-cent");
        share.dividends.append(d);
        RawPortfolio portfolio;
        portfolio.shares.append(share);

        QList<ValidationIssue> issues;
        QVERIFY(!PortfolioValidator::validate(portfolio, issues));
        QVERIFY(std::any_of(issues.cbegin(), issues.cend(), [](const ValidationIssue& i) {
            return i.category == QStringLiteral("Dividend") && i.message.contains(QStringLiteral("Rate"));
        }));
    }

    void test_validate_dividendForeignCurrencyUnparsableExchangeRatio_isReported()
    {
        RawShare share = makeValidShare(QStringLiteral("VAL25"));
        RawDividend d = makeValidDividend(newGuid());
        d.hasForeignCurrency = true;
        d.fc.enabled = true;
        d.fc.exchangeRatio = QStringLiteral("ein Euro achtzig");
        d.fc.currency = QStringLiteral("en-US");
        share.dividends.append(d);
        RawPortfolio portfolio;
        portfolio.shares.append(share);

        QList<ValidationIssue> issues;
        QVERIFY(!PortfolioValidator::validate(portfolio, issues));
        QVERIFY(std::any_of(issues.cbegin(), issues.cend(), [](const ValidationIssue& i) {
            return i.category == QStringLiteral("Dividend") &&
                   i.message.contains(QStringLiteral("ExchangeRatio"));
        }));
    }

    void test_validate_dailyValueUnparsableClose_isReported()
    {
        RawShare share = makeValidShare(QStringLiteral("VAL26"));
        RawDailyValue e = makeValidDailyValue(QStringLiteral("18.08.2015"));
        e.close = QStringLiteral("hundert");
        share.dailyValues.append(e);
        RawPortfolio portfolio;
        portfolio.shares.append(share);

        QList<ValidationIssue> issues;
        QVERIFY(!PortfolioValidator::validate(portfolio, issues));
        QVERIFY(std::any_of(issues.cbegin(), issues.cend(), [](const ValidationIssue& i) {
            return i.category == QStringLiteral("DailyValue") && i.message.contains(QStringLiteral("C"));
        }));
    }

    void test_validate_emptyNumericFields_areAccepted()
    {
        // Leere numerische Felder sind laut Documents.xml (ResultEmpty="true")
        // ein legitimer "nicht konfiguriert"-Zustand — PortfolioImporter::
        // toDouble() faellt fuer sie auf 0.0 zurueck, das ist kein Datenfehler.
        // Nur ein NICHT-leerer, aber unparsbarer Wert darf gemeldet werden.
        RawShare share = makeValidShare(QStringLiteral("VAL27"));
        share.sharePriceBefore.clear();

        RawBuy buy = makeValidBuy(newGuid(), QStringLiteral("ORD-1"));
        buy.volumeSold.clear();
        share.buys.append(buy);

        RawBrokerage brokerage = makeValidBrokerage(newGuid(), buy.guid);
        brokerage.brokerFee.clear();
        brokerage.traderFee.clear();
        brokerage.reduction.clear();
        share.brokerages.append(brokerage);

        RawDividend dividend = makeValidDividend(newGuid());
        dividend.taxAtSource.clear();
        dividend.capitalGainsTax.clear();
        dividend.solidarityTax.clear();
        dividend.price.clear();
        share.dividends.append(dividend);

        RawPortfolio portfolio;
        portfolio.shares.append(share);

        QList<ValidationIssue> issues;
        QVERIFY(PortfolioValidator::validate(portfolio, issues));
        QVERIFY(issues.isEmpty());
    }

    // ── Abgleich gegen bereits vorhandene DB-Daten ──────────────────────────

    void test_validate_orderNumberAlreadyExistsInDb_isReported()
    {
        const QString shareGuid = newGuid();
        Database::instance().execute(QStringLiteral(
            "INSERT INTO shares (guid, wkn, name) VALUES ('%1', 'VAL16', 'Bereits vorhanden')")
                .arg(shareGuid));
        Database::instance().execute(QStringLiteral(
            "INSERT INTO buys (guid, share_guid, depot_number, order_number, datetime, "
            "volume, volume_sold, price, brokerage_guid, document) "
            "VALUES ('%1', '%2', 'D1', 'ORD-EXISTING', '2024-01-01T00:00:00', 10, 0, 10, '', '')")
                .arg(newGuid(), shareGuid));

        RawShare share = makeValidShare(QStringLiteral("VAL16")); // gleiche WKN wie oben
        share.buys.append(makeValidBuy(newGuid(), QStringLiteral("ORD-EXISTING")));
        RawPortfolio portfolio;
        portfolio.shares.append(share);

        QList<ValidationIssue> issues;
        QVERIFY(!PortfolioValidator::validate(portfolio, issues));
        QVERIFY(std::any_of(issues.cbegin(), issues.cend(), [](const ValidationIssue& i) {
            return i.category == QStringLiteral("Buy") && i.message.contains(QStringLiteral("Datenbank"));
        }));
    }

    void test_validate_brokerageResolvesAgainstExistingDbBuy_noIssue()
    {
        // Ein Brokerage in der neuen Datei darf sich auf einen Buy beziehen, der
        // bereits aus einem früheren Import in der DB steht (nicht nur auf Buys
        // in der aktuellen Datei).
        const QString shareGuid = newGuid();
        const QString existingBuyGuid = newGuid();
        Database::instance().execute(QStringLiteral(
            "INSERT INTO shares (guid, wkn, name) VALUES ('%1', 'VAL17', 'Bereits vorhanden')")
                .arg(shareGuid));
        Database::instance().execute(QStringLiteral(
            "INSERT INTO buys (guid, share_guid, depot_number, order_number, datetime, "
            "volume, volume_sold, price, brokerage_guid, document) "
            "VALUES ('%1', '%2', 'D1', 'ORD-OLD', '2024-01-01T00:00:00', 10, 0, 10, '', '')")
                .arg(existingBuyGuid, shareGuid));

        RawShare share = makeValidShare(QStringLiteral("VAL17"));
        share.brokerages.append(makeValidBrokerage(newGuid(), existingBuyGuid));
        RawPortfolio portfolio;
        portfolio.shares.append(share);

        QList<ValidationIssue> issues;
        QVERIFY(PortfolioValidator::validate(portfolio, issues));
        QVERIFY(issues.isEmpty());
    }

    // ── Regressionstest: idempotenter Re-Import darf NICHT als OrderNumber- ─
    // Kollision gewertet werden (Bug gefunden 05.07.2026 durch
    // test_importBuy_isIdempotentOnRerun in tst_portfolioimporter.cpp — der
    // zweite, identische Import-Lauf schlug fälschlich fehl, weil die
    // DB-Prüfung nicht zwischen "andere GUID, gleiche OrderNumber" (echter
    // Fehler) und "dieselbe GUID nochmal" (normaler Re-Import) unterschied).

    void test_validate_sameGuidReimportedWithSameOrderNumber_noIssue()
    {
        const QString shareGuid = newGuid();
        const QString buyGuid = newGuid();
        Database::instance().execute(QStringLiteral(
            "INSERT INTO shares (guid, wkn, name) VALUES ('%1', 'VAL18', 'Bereits vorhanden')")
                .arg(shareGuid));
        Database::instance().execute(QStringLiteral(
            "INSERT INTO buys (guid, share_guid, depot_number, order_number, datetime, "
            "volume, volume_sold, price, brokerage_guid, document) "
            "VALUES ('%1', '%2', 'D1', 'ORD-100', '2024-01-01T00:00:00', 10, 0, 10, '', '')")
                .arg(buyGuid, shareGuid));

        // Derselbe Buy (gleiche GUID, gleiche OrderNumber) wird erneut importiert —
        // das ist der normale Idempotenz-Fall, kein Datenfehler.
        RawShare share = makeValidShare(QStringLiteral("VAL18"));
        share.buys.append(makeValidBuy(buyGuid, QStringLiteral("ORD-100")));
        RawPortfolio portfolio;
        portfolio.shares.append(share);

        QList<ValidationIssue> issues;
        QVERIFY(PortfolioValidator::validate(portfolio, issues));
        QVERIFY(issues.isEmpty());
    }
};

QTEST_MAIN(TestPortfolioValidator)
#include "tst_portfoliovalidator.moc"

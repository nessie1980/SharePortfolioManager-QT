// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "XmlPortfolioParser.h"

#include <QFile>

// ── Data quality helpers ─────────────────────────────────────────────────────

QString XmlPortfolioParser::normalizeWebSiteUrl(const QString& raw,
                                                const QString& fieldLabel,
                                                QStringList&   warnings)
{
    static const QString kDoubleEscaped = QStringLiteral("&amp;");

    if (!raw.contains(kDoubleEscaped))
        return raw;

    QString corrected = raw;
    corrected.replace(kDoubleEscaped, QStringLiteral("&"));

    warnings.append(QStringLiteral(
        "%1 enthält doppelt-XML-escapte Ampersands (\"&amp;\" im geparsten Wert) "
        "— automatisch zu \"&\" korrigiert. Original: \"%2\" -> Korrigiert: \"%3\"")
            .arg(fieldLabel, raw, corrected));

    return corrected;
}

// ── Attribute-level helpers ─────────────────────────────────────────────────

RawDailyValue XmlPortfolioParser::parseDailyValueEntry(const QXmlStreamAttributes& a)
{
    RawDailyValue v;
    v.date   = a.value(QStringLiteral("D")).toString();
    v.close  = a.value(QStringLiteral("C")).toString();
    v.open   = a.value(QStringLiteral("O")).toString();
    v.top    = a.value(QStringLiteral("T")).toString();
    v.bottom = a.value(QStringLiteral("B")).toString();
    v.volume = a.value(QStringLiteral("V")).toString();
    return v;
}

RawBrokerage XmlPortfolioParser::parseBrokerage(const QXmlStreamAttributes& a)
{
    RawBrokerage b;
    b.guid        = a.value(QStringLiteral("Guid")).toString();
    b.buyPart     = a.value(QStringLiteral("BuyPart")).toString()
                        .compare(QStringLiteral("True"), Qt::CaseInsensitive) == 0;
    b.salePart    = a.value(QStringLiteral("SalePart")).toString()
                        .compare(QStringLiteral("True"), Qt::CaseInsensitive) == 0;
    b.guidBuySale = a.value(QStringLiteral("GuidBuySale")).toString();
    b.date        = a.value(QStringLiteral("Date")).toString();
    b.provision   = a.value(QStringLiteral("Provision")).toString();
    b.brokerFee   = a.value(QStringLiteral("BrokerFee")).toString();
    b.traderFee   = a.value(QStringLiteral("TraderPlaceFee")).toString();
    b.reduction   = a.value(QStringLiteral("Reduction")).toString();
    b.doc         = a.value(QStringLiteral("Doc")).toString();
    return b;
}

RawBuy XmlPortfolioParser::parseBuy(const QXmlStreamAttributes& a)
{
    RawBuy b;
    b.guid          = a.value(QStringLiteral("Guid")).toString();
    b.depotNumber   = a.value(QStringLiteral("DepotNumber")).toString();
    b.orderNumber   = a.value(QStringLiteral("OrderNumber")).toString();
    b.date          = a.value(QStringLiteral("Date")).toString();
    b.volume        = a.value(QStringLiteral("Volume")).toString();
    b.volumeSold    = a.value(QStringLiteral("VolumeSold")).toString();
    b.price         = a.value(QStringLiteral("Price")).toString();
    b.brokerageGuid = a.value(QStringLiteral("BrokerageGuid")).toString();
    b.doc           = a.value(QStringLiteral("Doc")).toString();
    return b;
}

RawUsedBuy XmlPortfolioParser::parseUsedBuy(const QXmlStreamAttributes& a)
{
    RawUsedBuy u;
    u.buyDate   = a.value(QStringLiteral("BuyDate")).toString();
    u.buyGuid   = a.value(QStringLiteral("BuyGuid")).toString();
    u.buyVolume = a.value(QStringLiteral("BuyVolume")).toString();
    u.buyPrice  = a.value(QStringLiteral("BuyPrice")).toString();
    u.reduction = a.value(QStringLiteral("Reduction")).toString();
    u.brokerage = a.value(QStringLiteral("Brokerage")).toString();
    return u;
}

// ── Element-level helpers ───────────────────────────────────────────────────

RawSale XmlPortfolioParser::parseSale(QXmlStreamReader& xml)
{
    RawSale sale;
    const auto attrs   = xml.attributes();
    sale.guid           = attrs.value(QStringLiteral("Guid")).toString();
    sale.date            = attrs.value(QStringLiteral("Date")).toString();
    sale.depotNumber     = attrs.value(QStringLiteral("DepotNumber")).toString();
    sale.orderNumber     = attrs.value(QStringLiteral("OrderNumber")).toString();
    sale.volume          = attrs.value(QStringLiteral("Volume")).toString();
    sale.salePrice       = attrs.value(QStringLiteral("SalePrice")).toString();
    sale.taxAtSource      = attrs.value(QStringLiteral("TaxAtSource")).toString();
    sale.capitalGainsTax  = attrs.value(QStringLiteral("CapitalGainsTax")).toString();
    sale.solidarityTax    = attrs.value(QStringLiteral("SolidarityTax")).toString();
    sale.reduction        = attrs.value(QStringLiteral("Reduction")).toString();
    sale.brokerageGuid    = attrs.value(QStringLiteral("BrokerageGuid")).toString();
    sale.doc               = attrs.value(QStringLiteral("Doc")).toString();

    while (xml.readNextStartElement()) {
        if (xml.name() == QStringLiteral("UsedBuys")) {
            while (xml.readNextStartElement()) {
                if (xml.name() == QStringLiteral("UsedBuy")) {
                    sale.usedBuys.append(parseUsedBuy(xml.attributes()));
                    xml.skipCurrentElement();
                } else {
                    xml.skipCurrentElement();
                }
            }
        } else {
            xml.skipCurrentElement();
        }
    }
    return sale;
}

RawDividend XmlPortfolioParser::parseDividend(QXmlStreamReader& xml)
{
    RawDividend div;
    const auto attrs  = xml.attributes();
    div.guid           = attrs.value(QStringLiteral("Guid")).toString();
    div.date            = attrs.value(QStringLiteral("Date")).toString();
    div.rate            = attrs.value(QStringLiteral("Rate")).toString();
    div.volume          = attrs.value(QStringLiteral("Volume")).toString();
    div.taxAtSource      = attrs.value(QStringLiteral("TaxAtSource")).toString();
    div.capitalGainsTax  = attrs.value(QStringLiteral("CapitalGainsTax")).toString();
    div.solidarityTax    = attrs.value(QStringLiteral("SolidarityTax")).toString();
    div.price             = attrs.value(QStringLiteral("Price")).toString();
    div.doc                = attrs.value(QStringLiteral("Doc")).toString();

    while (xml.readNextStartElement()) {
        if (xml.name() == QStringLiteral("ForeignCurrency")) {
            const auto a = xml.attributes();
            div.hasForeignCurrency = true;
            div.fc.enabled = a.value(QStringLiteral("Flag")).toString()
                                 .compare(QStringLiteral("Checked"), Qt::CaseInsensitive) == 0;
            div.fc.exchangeRatio = a.value(QStringLiteral("ExchangeRatio")).toString();
            div.fc.currency      = a.value(QStringLiteral("FCName")).toString();
            xml.skipCurrentElement();
        } else {
            xml.skipCurrentElement();
        }
    }
    return div;
}

RawShare XmlPortfolioParser::parseShare(QXmlStreamReader& xml)
{
    RawShare share;
    const auto attrs = xml.attributes();
    share.wkn       = attrs.value(QStringLiteral("WKN")).toString();
    share.isin      = attrs.value(QStringLiteral("ISIN")).toString();
    share.name      = attrs.value(QStringLiteral("Name")).toString();
    share.updateStr = attrs.value(QStringLiteral("Update")).toString();

    while (xml.readNextStartElement()) {
        const QString tag = xml.name().toString();

        if (tag == QStringLiteral("DetailsWebSite")) {
            share.detailsWebSite = normalizeWebSiteUrl(xml.readElementText(),
                                                       QStringLiteral("DetailsWebSite"),
                                                       share.parseWarnings);
        } else if (tag == QStringLiteral("StockMarketLaunchDate")) {
            share.stockMarketLaunchDate = xml.readElementText();
        } else if (tag == QStringLiteral("LastUpdateInternet")) {
            share.lastUpdateInternet = xml.readElementText();
        } else if (tag == QStringLiteral("LastUpdateShareDate")) {
            share.lastUpdateShareDate = xml.readElementText();
        } else if (tag == QStringLiteral("SharePrice")) {
            share.sharePrice = xml.readElementText();
        } else if (tag == QStringLiteral("SharePriceBefore")) {
            share.sharePriceBefore = xml.readElementText();
        } else if (tag == QStringLiteral("Culture")) {
            share.culture = xml.readElementText();
        } else if (tag == QStringLiteral("ShareType")) {
            share.shareTypeStr = xml.readElementText();
        } else if (tag == QStringLiteral("MarketValue")) {
            const auto a = xml.attributes();
            share.marketValueWebSite = normalizeWebSiteUrl(
                a.value(QStringLiteral("WebSite")).toString(),
                QStringLiteral("MarketValue.WebSite"),
                share.parseWarnings);
            share.marketValueParsing = a.value(QStringLiteral("Parsing")).toString();
            xml.skipCurrentElement();
        } else if (tag == QStringLiteral("MarketValues")) {
            // Datenfehler in der Quell-XML (gemeldet 05.07.2026, Nvidia/Wacker
            // Chemie): das Element heißt hier "<MarketValues>" (Plural) statt
            // "<MarketValue>" (Singular) wie beim Rest des Bestands (32 von 34
            // Vorkommen laut grep). Das wird NICHT als gültige Schreibvariante
            // akzeptiert und automatisch übernommen — ein falscher Elementname
            // ist ein struktureller Fehler in der Quelle, kein reines
            // Formatierungsdetail wie der Ampersand-Fall. Stattdessen wird der
            // Fehler protokolliert und das Element unverarbeitet übersprungen;
            // MarketValue.WebSite/Parsing bleiben für diese Aktie leer, bis die
            // Quelle korrigiert und neu importiert wurde (oder die Daten manuell
            // über ShareEditForm nachgetragen werden).
            share.parseErrors.append(QStringLiteral(
                "Unerwartetes Element <MarketValues> (Plural) statt <MarketValue> "
                "(Singular) gefunden — wird als Datenfehler in der Quell-XML "
                "gewertet und NICHT übernommen. MarketValue.WebSite/Parsing "
                "bleiben für diese Aktie leer. Quell-XML korrigieren und erneut "
                "importieren, oder Werte manuell über ShareEditForm nachtragen."));
            xml.skipCurrentElement();
        } else if (tag == QStringLiteral("DailyValues")) {
            const auto a = xml.attributes();
            share.dailyValuesWebSite = normalizeWebSiteUrl(
                a.value(QStringLiteral("WebSite")).toString(),
                QStringLiteral("DailyValues.WebSite"),
                share.parseWarnings);
            share.dailyValuesParsing = a.value(QStringLiteral("Parsing")).toString();
            while (xml.readNextStartElement()) {
                if (xml.name() == QStringLiteral("Entry")) {
                    share.dailyValues.append(parseDailyValueEntry(xml.attributes()));
                    xml.skipCurrentElement();
                } else {
                    xml.skipCurrentElement();
                }
            }
        } else if (tag == QStringLiteral("Brokerages")) {
            while (xml.readNextStartElement()) {
                if (xml.name() == QStringLiteral("Brokerage")) {
                    share.brokerages.append(parseBrokerage(xml.attributes()));
                    xml.skipCurrentElement();
                } else {
                    xml.skipCurrentElement();
                }
            }
        } else if (tag == QStringLiteral("Buys")) {
            while (xml.readNextStartElement()) {
                if (xml.name() == QStringLiteral("Buy")) {
                    share.buys.append(parseBuy(xml.attributes()));
                    xml.skipCurrentElement();
                } else {
                    xml.skipCurrentElement();
                }
            }
        } else if (tag == QStringLiteral("Sales")) {
            while (xml.readNextStartElement()) {
                if (xml.name() == QStringLiteral("Sale")) {
                    share.sales.append(parseSale(xml));
                } else {
                    xml.skipCurrentElement();
                }
            }
        } else if (tag == QStringLiteral("Dividends")) {
            // PayoutInterval attribute exists in the source XML but has no
            // corresponding column in the current schema — ignored here.
            while (xml.readNextStartElement()) {
                if (xml.name() == QStringLiteral("Dividend")) {
                    share.dividends.append(parseDividend(xml));
                } else {
                    xml.skipCurrentElement();
                }
            }
        } else {
            xml.skipCurrentElement();
        }
    }

    return share;
}

// ── Entry point ──────────────────────────────────────────────────────────────

bool XmlPortfolioParser::parse(const QString& filePath,
                               RawPortfolio&  outPortfolio,
                               QString&       errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMessage = QStringLiteral("Datei konnte nicht geöffnet werden: %1").arg(filePath);
        return false;
    }

    QXmlStreamReader xml(&file);

    if (!xml.readNextStartElement() || xml.name() != QStringLiteral("Portfolio")) {
        errorMessage = QStringLiteral("Wurzelelement <Portfolio> nicht gefunden.");
        return false;
    }

    while (xml.readNextStartElement()) {
        if (xml.name() == QStringLiteral("Share"))
            outPortfolio.shares.append(parseShare(xml));
        else
            xml.skipCurrentElement();
    }

    if (xml.hasError()) {
        errorMessage = QStringLiteral("XML-Fehler: %1 (Zeile %2)")
                           .arg(xml.errorString())
                           .arg(xml.lineNumber());
        return false;
    }
    return true;
}

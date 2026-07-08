// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "PortfolioValidator.h"

#include "../../app/repositories/ShareRepository.h"
#include "../../app/repositories/BuyRepository.h"
#include "../../app/repositories/SaleRepository.h"
#include "../../app/repositories/BrokerageRepository.h"
#include "../../app/repositories/DividendRepository.h"

#include <QDate>
#include <QMap>
#include <QSet>

// ── Helpers ──────────────────────────────────────────────────────────────────

bool PortfolioValidator::isParsableGermanDate(const QString& raw)
{
    const QString datePart = raw.trimmed().section(QLatin1Char(' '), 0, 0);
    return QDate::fromString(datePart, QStringLiteral("dd.MM.yyyy")).isValid();
}

bool PortfolioValidator::isParsableGermanNumber(const QString& raw)
{
    QString s = raw.trimmed();
    if (s.isEmpty())
        return true; // legitimately optional in the source — not a data error

    // Mirrors PortfolioImporter::toDouble() exactly: "1.234,56" (thousands
    // '.' + decimal ',') as well as plain "66,52".
    if (s.contains(QLatin1Char(',')) && s.contains(QLatin1Char('.'))) {
        s.remove(QLatin1Char('.'));
        s.replace(QLatin1Char(','), QLatin1Char('.'));
    } else if (s.contains(QLatin1Char(','))) {
        s.replace(QLatin1Char(','), QLatin1Char('.'));
    }

    bool ok = false;
    s.toDouble(&ok);
    return ok;
}

// ── Share-level checks ───────────────────────────────────────────────────────

void PortfolioValidator::validateShare(const RawShare& share, QList<ValidationIssue>& issues)
{
    const auto addIssue = [&](const QString& category, const QString& recordId,
                              const QString& message) {
        issues.append(ValidationIssue{ share.wkn, share.name, category, recordId, message });
    };

    if (share.wkn.trimmed().isEmpty()) {
        addIssue(QStringLiteral("Share"), QString(),
                 QStringLiteral("Keine WKN vorhanden."));
        // Ohne WKN lässt sich die Aktie später nicht per findByWkn() zuordnen —
        // die übrigen Prüfungen für diese Aktie liefen zwar noch, aber jeder
        // DB-Abgleich wäre gegen eine leere shareGuid und damit witzlos.
        // Trotzdem: weitere Probleme derselben Aktie mitmelden, damit der
        // Bericht möglichst vollständig ist.
    }

    // ── Update ──────────────────────────────────────────────────────────────
    const QString updateStr = share.updateStr.trimmed();
    static const QStringList kValidUpdate = {
        QStringLiteral("None"), QStringLiteral("MarketPrice"),
        QStringLiteral("DailyValues"), QStringLiteral("Both")
    };
    bool updateOk = false;
    for (const QString& v : kValidUpdate) {
        if (updateStr.compare(v, Qt::CaseInsensitive) == 0) { updateOk = true; break; }
    }
    if (!updateOk) {
        addIssue(QStringLiteral("Share"), QStringLiteral("Update"),
                 QStringLiteral("Unbekannter Update-Wert \"%1\" (erwartet: None/MarketPrice/"
                               "DailyValues/Both).").arg(share.updateStr));
    }

    // ── ShareType ───────────────────────────────────────────────────────────
    const QString typeStr = share.shareTypeStr.trimmed();
    if (typeStr != QStringLiteral("0") && typeStr != QStringLiteral("1") &&
        typeStr != QStringLiteral("2")) {
        addIssue(QStringLiteral("Share"), QStringLiteral("ShareType"),
                 QStringLiteral("Unbekannter ShareType-Wert \"%1\" (erwartet: 0/1/2).")
                     .arg(share.shareTypeStr));
    }

    // ── Parsing (MarketValue / DailyValues) ──────────────────────────────────
    static const QStringList kValidParsing = {
        QStringLiteral(""), QStringLiteral("Regex"), QStringLiteral("ApiYahoo"),
        QStringLiteral("ApiOnVista"), QStringLiteral("ApiOnvista")
    };
    const auto parsingOk = [&](const QString& s) {
        for (const QString& v : kValidParsing) {
            if (s.compare(v, Qt::CaseInsensitive) == 0) return true;
        }
        return false;
    };
    if (!parsingOk(share.marketValueParsing.trimmed())) {
        addIssue(QStringLiteral("Share"), QStringLiteral("MarketValue.Parsing"),
                 QStringLiteral("Unbekannter Parsing-Wert \"%1\" (erwartet: leer/Regex/ApiYahoo/"
                               "ApiOnVista).").arg(share.marketValueParsing));
    }
    if (!parsingOk(share.dailyValuesParsing.trimmed())) {
        addIssue(QStringLiteral("Share"), QStringLiteral("DailyValues.Parsing"),
                 QStringLiteral("Unbekannter Parsing-Wert \"%1\" (erwartet: leer/Regex/ApiYahoo/"
                               "ApiOnVista).").arg(share.dailyValuesParsing));
    }

    // ── Datumsfelder ──────────────────────────────────────────────────────────
    if (!isParsableGermanDate(share.stockMarketLaunchDate)) {
        addIssue(QStringLiteral("Share"), QStringLiteral("StockMarketLaunchDate"),
                 QStringLiteral("Datum \"%1\" nicht parsbar (erwartet: dd.MM.yyyy).")
                     .arg(share.stockMarketLaunchDate));
    }
    if (!isParsableGermanDate(share.lastUpdateInternet)) {
        addIssue(QStringLiteral("Share"), QStringLiteral("LastUpdateInternet"),
                 QStringLiteral("Datum \"%1\" nicht parsbar (erwartet: dd.MM.yyyy[ HH:mm]).")
                     .arg(share.lastUpdateInternet));
    }
    if (!isParsableGermanDate(share.lastUpdateShareDate)) {
        addIssue(QStringLiteral("Share"), QStringLiteral("LastUpdateShareDate"),
                 QStringLiteral("Datum \"%1\" nicht parsbar (erwartet: dd.MM.yyyy[ HH:mm]).")
                     .arg(share.lastUpdateShareDate));
    }

    // ── Numerische Felder ─────────────────────────────────────────────────────
    const auto checkNumber = [&](const QString& fieldName, const QString& raw) {
        if (!isParsableGermanNumber(raw)) {
            addIssue(QStringLiteral("Share"), fieldName,
                     QStringLiteral("Feld \"%1\" mit Wert \"%2\" ist keine gültige Zahl "
                                   "(erwartet: deutsches Dezimalformat, z. B. \"1234,56\").")
                         .arg(fieldName, raw));
        }
    };
    checkNumber(QStringLiteral("SharePrice"), share.sharePrice);
    checkNumber(QStringLiteral("SharePriceBefore"), share.sharePriceBefore);

    // ── Vom Parser bereits erkannte, strukturelle Fehler (z.B. <MarketValues>
    //    statt <MarketValue>, siehe XmlPortfolioParser::parseShare()) ─────────
    for (const QString& parseError : share.parseErrors) {
        addIssue(QStringLiteral("Share"), QString(), parseError);
    }
}

// ── Duplicate GUIDs within the same share ───────────────────────────────────

void PortfolioValidator::validateDuplicateGuids(const RawShare& share, QList<ValidationIssue>& issues)
{
    // GUIDs sind als global eindeutige Identifikatoren gedacht — eine Kollision
    // *innerhalb derselben Aktie*, über Buy/Sale/Brokerage/Dividend hinweg, ist
    // in jedem Fall ein Datenfehler in der Quelle, unabhängig von der Kategorie.
    QMap<QString, QStringList> guidToCategories; // guid -> Liste von "Kategorie #n"

    int i = 0;
    for (const RawBuy& b : share.buys) {
        ++i;
        if (!b.guid.trimmed().isEmpty())
            guidToCategories[b.guid].append(QStringLiteral("Buy #%1").arg(i));
    }
    i = 0;
    for (const RawSale& s : share.sales) {
        ++i;
        if (!s.guid.trimmed().isEmpty())
            guidToCategories[s.guid].append(QStringLiteral("Sale #%1").arg(i));
    }
    i = 0;
    for (const RawBrokerage& b : share.brokerages) {
        ++i;
        if (!b.guid.trimmed().isEmpty())
            guidToCategories[b.guid].append(QStringLiteral("Brokerage #%1").arg(i));
    }
    i = 0;
    for (const RawDividend& d : share.dividends) {
        ++i;
        if (!d.guid.trimmed().isEmpty())
            guidToCategories[d.guid].append(QStringLiteral("Dividend #%1").arg(i));
    }

    for (auto it = guidToCategories.constBegin(); it != guidToCategories.constEnd(); ++it) {
        if (it.value().size() > 1) {
            issues.append(ValidationIssue{
                share.wkn, share.name, QStringLiteral("Share"), it.key(),
                QStringLiteral("GUID \"%1\" kommt mehrfach in derselben Aktie vor (%2) — "
                              "GUIDs müssen eindeutig sein.")
                    .arg(it.key(), it.value().join(QStringLiteral(", ")))
            });
        }
    }
}

// ── Buys ─────────────────────────────────────────────────────────────────────

void PortfolioValidator::validateBuys(const RawShare& share, const QString& existingShareGuid,
                                      QList<ValidationIssue>& issues)
{
    BuyRepository buyRepo;
    QSet<QString> seenOrderNumbers;

    // OrderNumber -> GUID der bereits in der DB vorhandenen Buys dieser Aktie.
    // Wichtig für Idempotenz: ein erneuter Import DESSELBEN Datensatzes (gleiche
    // GUID, gleiche OrderNumber) ist kein Fehler, sondern der normale
    // Re-Import-Fall (siehe importBuys() — Dedupe per GUID). Nur eine ANDERE
    // GUID mit derselben OrderNumber ist ein echter Datenfehler.
    QMap<QString, QString> dbOrderNumberToGuid;
    if (!existingShareGuid.isEmpty()) {
        for (const BuyObject& b : buyRepo.findByShare(existingShareGuid))
            dbOrderNumberToGuid.insert(b.orderNumber(), b.guid());
    }

    for (const RawBuy& b : share.buys) {
        const QString recordId = b.guid.isEmpty() ? b.orderNumber : b.guid;

        if (b.guid.trimmed().isEmpty()) {
            issues.append(ValidationIssue{
                share.wkn, share.name, QStringLiteral("Buy"), b.orderNumber,
                QStringLiteral("Keine GUID im Quell-XML vorhanden.") });
        }
        if (!isParsableGermanDate(b.date)) {
            issues.append(ValidationIssue{
                share.wkn, share.name, QStringLiteral("Buy"), recordId,
                QStringLiteral("Datum \"%1\" nicht parsbar (erwartet: dd.MM.yyyy).").arg(b.date) });
        }

        const auto checkNumber = [&](const QString& fieldName, const QString& raw) {
            if (!isParsableGermanNumber(raw)) {
                issues.append(ValidationIssue{
                    share.wkn, share.name, QStringLiteral("Buy"), recordId,
                    QStringLiteral("Feld \"%1\" mit Wert \"%2\" ist keine gültige Zahl "
                                  "(erwartet: deutsches Dezimalformat, z. B. \"1234,56\").")
                        .arg(fieldName, raw) });
            }
        };
        checkNumber(QStringLiteral("Volume"), b.volume);
        checkNumber(QStringLiteral("VolumeSold"), b.volumeSold);
        checkNumber(QStringLiteral("Price"), b.price);

        const QString orderNumber = b.orderNumber.trimmed();
        if (!orderNumber.isEmpty()) {
            if (seenOrderNumbers.contains(orderNumber)) {
                issues.append(ValidationIssue{
                    share.wkn, share.name, QStringLiteral("Buy"), orderNumber,
                    QStringLiteral("OrderNumber \"%1\" kommt mehrfach unter den Buys dieser Aktie "
                                  "im Quell-XML vor.").arg(orderNumber) });
            }
            seenOrderNumbers.insert(orderNumber);

            const auto dbMatch = dbOrderNumberToGuid.constFind(orderNumber);
            if (dbMatch != dbOrderNumberToGuid.constEnd() && dbMatch.value() != b.guid) {
                issues.append(ValidationIssue{
                    share.wkn, share.name, QStringLiteral("Buy"), orderNumber,
                    QStringLiteral("OrderNumber \"%1\" existiert bereits in der Datenbank für "
                                  "diese Aktie unter einer anderen GUID (UNIQUE-Konflikt bei "
                                  "einem erneuten Import).").arg(orderNumber) });
            }
        }
    }
}

// ── Sales ────────────────────────────────────────────────────────────────────

void PortfolioValidator::validateSales(const RawShare& share, const QString& existingShareGuid,
                                       QList<ValidationIssue>& issues)
{
    SaleRepository saleRepo;
    QSet<QString> seenOrderNumbers;

    // OrderNumber -> GUID der bereits in der DB vorhandenen Sales dieser Aktie
    // (analog zu ModelSaleEdit::orderNumberExists() — SaleRepository hat kein
    // eigenes orderNumberExists(), im Gegensatz zu BuyRepository). Wichtig für
    // Idempotenz: ein erneuter Import DESSELBEN Datensatzes (gleiche GUID) ist
    // kein Fehler — nur eine ANDERE GUID mit derselben OrderNumber ist einer.
    QMap<QString, QString> dbOrderNumberToGuid;
    if (!existingShareGuid.isEmpty()) {
        for (const SaleObject& s : saleRepo.findByShare(existingShareGuid))
            dbOrderNumberToGuid.insert(s.orderNumber(), s.guid());
    }

    for (const RawSale& s : share.sales) {
        const QString recordId = s.guid.isEmpty() ? s.orderNumber : s.guid;

        if (s.guid.trimmed().isEmpty()) {
            issues.append(ValidationIssue{
                share.wkn, share.name, QStringLiteral("Sale"), s.orderNumber,
                QStringLiteral("Keine GUID im Quell-XML vorhanden.") });
        }
        if (!isParsableGermanDate(s.date)) {
            issues.append(ValidationIssue{
                share.wkn, share.name, QStringLiteral("Sale"), recordId,
                QStringLiteral("Datum \"%1\" nicht parsbar (erwartet: dd.MM.yyyy).").arg(s.date) });
        }

        const auto checkNumber = [&](const QString& fieldName, const QString& raw) {
            if (!isParsableGermanNumber(raw)) {
                issues.append(ValidationIssue{
                    share.wkn, share.name, QStringLiteral("Sale"), recordId,
                    QStringLiteral("Feld \"%1\" mit Wert \"%2\" ist keine gültige Zahl "
                                  "(erwartet: deutsches Dezimalformat, z. B. \"1234,56\").")
                        .arg(fieldName, raw) });
            }
        };
        checkNumber(QStringLiteral("Volume"), s.volume);
        checkNumber(QStringLiteral("SalePrice"), s.salePrice);
        checkNumber(QStringLiteral("TaxAtSource"), s.taxAtSource);
        checkNumber(QStringLiteral("CapitalGainsTax"), s.capitalGainsTax);
        checkNumber(QStringLiteral("SolidarityTax"), s.solidarityTax);
        checkNumber(QStringLiteral("Reduction"), s.reduction);

        const QString orderNumber = s.orderNumber.trimmed();
        if (!orderNumber.isEmpty()) {
            if (seenOrderNumbers.contains(orderNumber)) {
                issues.append(ValidationIssue{
                    share.wkn, share.name, QStringLiteral("Sale"), orderNumber,
                    QStringLiteral("OrderNumber \"%1\" kommt mehrfach unter den Sales dieser Aktie "
                                  "im Quell-XML vor.").arg(orderNumber) });
            }
            seenOrderNumbers.insert(orderNumber);

            const auto dbMatch = dbOrderNumberToGuid.constFind(orderNumber);
            if (dbMatch != dbOrderNumberToGuid.constEnd() && dbMatch.value() != s.guid) {
                issues.append(ValidationIssue{
                    share.wkn, share.name, QStringLiteral("Sale"), orderNumber,
                    QStringLiteral("OrderNumber \"%1\" existiert bereits in der Datenbank für "
                                  "diese Aktie unter einer anderen GUID.").arg(orderNumber) });
            }
        }

        for (const RawUsedBuy& u : s.usedBuys) {
            if (u.buyGuid.trimmed().isEmpty()) {
                issues.append(ValidationIssue{
                    share.wkn, share.name, QStringLiteral("Sale"), s.guid,
                    QStringLiteral("<UsedBuy> ohne BuyGuid.") });
            }
            if (!isParsableGermanDate(u.buyDate)) {
                issues.append(ValidationIssue{
                    share.wkn, share.name, QStringLiteral("Sale"), s.guid,
                    QStringLiteral("<UsedBuy BuyGuid=\"%1\"> Datum \"%2\" nicht parsbar.")
                        .arg(u.buyGuid, u.buyDate) });
            }

            const auto checkUsedBuyNumber = [&](const QString& fieldName, const QString& raw) {
                if (!isParsableGermanNumber(raw)) {
                    issues.append(ValidationIssue{
                        share.wkn, share.name, QStringLiteral("Sale"), s.guid,
                        QStringLiteral("<UsedBuy BuyGuid=\"%1\"> Feld \"%2\" mit Wert \"%3\" ist "
                                      "keine gültige Zahl (erwartet: deutsches Dezimalformat, "
                                      "z. B. \"1234,56\").").arg(u.buyGuid, fieldName, raw) });
                }
            };
            checkUsedBuyNumber(QStringLiteral("BuyVolume"), u.buyVolume);
            checkUsedBuyNumber(QStringLiteral("BuyPrice"), u.buyPrice);
            checkUsedBuyNumber(QStringLiteral("Reduction"), u.reduction);
            checkUsedBuyNumber(QStringLiteral("Brokerage"), u.brokerage);
        }
    }
}

// ── Brokerages ───────────────────────────────────────────────────────────────

void PortfolioValidator::validateBrokerages(const RawShare& share, const QString& existingShareGuid,
                                            QList<ValidationIssue>& issues)
{
    BuyRepository  buyRepo;
    SaleRepository saleRepo;

    // GUIDs der Buys/Sales dieser Aktie — sowohl aus der aktuellen Datei als
    // auch bereits in der DB vorhanden (aus einem früheren Import-Lauf).
    QSet<QString> buyGuids;
    QSet<QString> saleGuids;
    for (const RawBuy& b : share.buys)
        if (!b.guid.trimmed().isEmpty()) buyGuids.insert(b.guid);
    for (const RawSale& s : share.sales)
        if (!s.guid.trimmed().isEmpty()) saleGuids.insert(s.guid);
    if (!existingShareGuid.isEmpty()) {
        for (const BuyObject& b : buyRepo.findByShare(existingShareGuid))
            buyGuids.insert(b.guid());
        for (const SaleObject& s : saleRepo.findByShare(existingShareGuid))
            saleGuids.insert(s.guid());
    }

    for (const RawBrokerage& b : share.brokerages) {
        const QString recordId = b.guid.isEmpty() ? b.guidBuySale : b.guid;

        if (b.guid.trimmed().isEmpty()) {
            issues.append(ValidationIssue{
                share.wkn, share.name, QStringLiteral("Brokerage"), b.guidBuySale,
                QStringLiteral("Keine GUID im Quell-XML vorhanden.") });
        }
        if (!isParsableGermanDate(b.date)) {
            issues.append(ValidationIssue{
                share.wkn, share.name, QStringLiteral("Brokerage"), recordId,
                QStringLiteral("Datum \"%1\" nicht parsbar (erwartet: dd.MM.yyyy).").arg(b.date) });
        }

        const auto checkNumber = [&](const QString& fieldName, const QString& raw) {
            if (!isParsableGermanNumber(raw)) {
                issues.append(ValidationIssue{
                    share.wkn, share.name, QStringLiteral("Brokerage"), recordId,
                    QStringLiteral("Feld \"%1\" mit Wert \"%2\" ist keine gültige Zahl "
                                  "(erwartet: deutsches Dezimalformat, z. B. \"1234,56\").")
                        .arg(fieldName, raw) });
            }
        };
        checkNumber(QStringLiteral("Provision"), b.provision);
        checkNumber(QStringLiteral("BrokerFee"), b.brokerFee);
        checkNumber(QStringLiteral("TraderFee"), b.traderFee);
        checkNumber(QStringLiteral("Reduction"), b.reduction);

        const QString guidBuySale = b.guidBuySale.trimmed();
        if (guidBuySale.isEmpty()) {
            issues.append(ValidationIssue{
                share.wkn, share.name, QStringLiteral("Brokerage"), b.guid,
                QStringLiteral("Kein GuidBuySale im Quell-XML vorhanden.") });
            continue;
        }

        const bool existsAsBuy  = buyGuids.contains(guidBuySale);
        const bool existsAsSale = saleGuids.contains(guidBuySale);
        if (existsAsBuy && existsAsSale) {
            issues.append(ValidationIssue{
                share.wkn, share.name, QStringLiteral("Brokerage"), b.guid,
                QStringLiteral("GuidBuySale \"%1\" existiert sowohl als Buy als auch als Sale "
                              "dieser Aktie — Zuordnung nicht eindeutig.").arg(guidBuySale) });
        } else if (!existsAsBuy && !existsAsSale) {
            issues.append(ValidationIssue{
                share.wkn, share.name, QStringLiteral("Brokerage"), b.guid,
                QStringLiteral("GuidBuySale \"%1\" existiert weder als Buy noch als Sale dieser "
                              "Aktie (weder in der aktuellen Datei noch in der Datenbank).")
                    .arg(guidBuySale) });
        }
    }
}

// ── Dividends ────────────────────────────────────────────────────────────────

void PortfolioValidator::validateDividends(const RawShare& share, QList<ValidationIssue>& issues)
{
    for (const RawDividend& d : share.dividends) {
        const QString recordId = d.guid.isEmpty() ? d.date : d.guid;

        if (d.guid.trimmed().isEmpty()) {
            issues.append(ValidationIssue{
                share.wkn, share.name, QStringLiteral("Dividend"), d.date,
                QStringLiteral("Keine GUID im Quell-XML vorhanden.") });
        }
        if (!isParsableGermanDate(d.date)) {
            issues.append(ValidationIssue{
                share.wkn, share.name, QStringLiteral("Dividend"), recordId,
                QStringLiteral("Datum \"%1\" nicht parsbar (erwartet: dd.MM.yyyy).").arg(d.date) });
        }

        const auto checkNumber = [&](const QString& fieldName, const QString& raw) {
            if (!isParsableGermanNumber(raw)) {
                issues.append(ValidationIssue{
                    share.wkn, share.name, QStringLiteral("Dividend"), recordId,
                    QStringLiteral("Feld \"%1\" mit Wert \"%2\" ist keine gültige Zahl "
                                  "(erwartet: deutsches Dezimalformat, z. B. \"1234,56\").")
                        .arg(fieldName, raw) });
            }
        };
        checkNumber(QStringLiteral("Rate"), d.rate);
        checkNumber(QStringLiteral("Volume"), d.volume);
        checkNumber(QStringLiteral("TaxAtSource"), d.taxAtSource);
        checkNumber(QStringLiteral("CapitalGainTax"), d.capitalGainsTax);
        checkNumber(QStringLiteral("SolidarityTax"), d.solidarityTax);
        checkNumber(QStringLiteral("PriceAtPayday"), d.price);
        // ExchangeRatio nur relevant, wenn ein <ForeignCurrency>-Element im
        // Quell-XML überhaupt vorhanden war (unabhängig vom Flag="Checked"-
        // Status — ein kaputter Wechselkurs ist auch dann ein Datenfehler,
        // wenn die Fremdwährung gerade nicht aktiv genutzt wird).
        if (d.hasForeignCurrency)
            checkNumber(QStringLiteral("ExchangeRatio"), d.fc.exchangeRatio);
    }
}

// ── Daily values ─────────────────────────────────────────────────────────────

void PortfolioValidator::validateDailyValues(const RawShare& share, QList<ValidationIssue>& issues)
{
    for (const RawDailyValue& e : share.dailyValues) {
        if (!isParsableGermanDate(e.date)) {
            issues.append(ValidationIssue{
                share.wkn, share.name, QStringLiteral("DailyValue"), e.date,
                QStringLiteral("Datum \"%1\" nicht parsbar (erwartet: dd.MM.yyyy).").arg(e.date) });
        }

        const auto checkNumber = [&](const QString& fieldName, const QString& raw) {
            if (!isParsableGermanNumber(raw)) {
                issues.append(ValidationIssue{
                    share.wkn, share.name, QStringLiteral("DailyValue"), e.date,
                    QStringLiteral("Feld \"%1\" mit Wert \"%2\" ist keine gültige Zahl "
                                  "(erwartet: deutsches Dezimalformat, z. B. \"1234,56\").")
                        .arg(fieldName, raw) });
            }
        };
        checkNumber(QStringLiteral("C"), e.close);
        checkNumber(QStringLiteral("O"), e.open);
        checkNumber(QStringLiteral("T"), e.top);
        checkNumber(QStringLiteral("B"), e.bottom);
        checkNumber(QStringLiteral("V"), e.volume);
    }
}

// ── Entry point ──────────────────────────────────────────────────────────────

bool PortfolioValidator::validate(const RawPortfolio& portfolio, QList<ValidationIssue>& issues)
{
    ShareRepository shareRepo;

    for (const RawShare& share : portfolio.shares) {
        validateShare(share, issues);
        validateDuplicateGuids(share, issues);

        // Für DB-Abgleiche (OrderNumber-/GUID-Kollisionen mit bereits
        // importierten Daten) wird die GUID der Aktie benötigt, FALLS sie schon
        // existiert (per WKN) — bei einer neuen Aktie gibt es naturgemäß noch
        // keine DB-Einträge, gegen die zu prüfen wäre.
        const QString existingShareGuid = share.wkn.trimmed().isEmpty()
            ? QString()
            : shareRepo.findByWkn(share.wkn).guid();

        validateBuys(share, existingShareGuid, issues);
        validateSales(share, existingShareGuid, issues);
        validateBrokerages(share, existingShareGuid, issues);
        validateDividends(share, issues);
        validateDailyValues(share, issues);
    }

    return issues.isEmpty();
}

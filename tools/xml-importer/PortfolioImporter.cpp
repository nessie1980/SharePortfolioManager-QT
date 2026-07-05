// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "PortfolioImporter.h"

#include "../../app/models/ShareObject.h"
#include "../../app/models/BuyObject.h"
#include "../../app/models/SaleObject.h"
#include "../../app/models/BrokerageObject.h"
#include "../../app/models/DividendObject.h"
#include "../../app/models/DailyValuesObject.h"

#include "../../app/repositories/ShareRepository.h"
#include "../../app/repositories/BuyRepository.h"
#include "../../app/repositories/SaleRepository.h"
#include "../../app/repositories/BrokerageRepository.h"
#include "../../app/repositories/DividendRepository.h"
#include "../../app/repositories/DailyValuesRepository.h"

#include <QUuid>
#include <QDateTime>

// ── Conversion helpers ──────────────────────────────────────────────────────

double PortfolioImporter::toDouble(const QString& germanNumber)
{
    QString s = germanNumber.trimmed();
    if (s.isEmpty())
        return 0.0;

    // Handle "1.234,56" (thousands '.' + decimal ',') as well as plain "66,52".
    if (s.contains(QLatin1Char(',')) && s.contains(QLatin1Char('.'))) {
        s.remove(QLatin1Char('.'));
        s.replace(QLatin1Char(','), QLatin1Char('.'));
    } else if (s.contains(QLatin1Char(','))) {
        s.replace(QLatin1Char(','), QLatin1Char('.'));
    }

    bool ok = false;
    const double value = s.toDouble(&ok);
    return ok ? value : 0.0;
}

QDate PortfolioImporter::toDate(const QString& germanDate)
{
    // The source XML mixes pure dates ("25.05.2016") and date+time
    // ("06.03.2026 14:01") in different elements — only the date part
    // is needed here, so split off anything after the first space.
    const QString datePart = germanDate.trimmed().section(QLatin1Char(' '), 0, 0);
    return QDate::fromString(datePart, QStringLiteral("dd.MM.yyyy"));
}

QString PortfolioImporter::toIsoDate(const QString& germanDate)
{
    const QDate d = toDate(germanDate);
    return d.isValid() ? d.toString(Qt::ISODate) : QString();
}

QString PortfolioImporter::toIsoDateTime(const QString& germanDateOrDateTime)
{
    const QString s = germanDateOrDateTime.trimmed();
    if (s.isEmpty())
        return QString();

    QDateTime dt = QDateTime::fromString(s, QStringLiteral("dd.MM.yyyy HH:mm"));
    if (!dt.isValid())
        dt = QDateTime::fromString(s, QStringLiteral("dd.MM.yyyy"));
    if (!dt.isValid())
        return QString();
    return dt.toString(Qt::ISODate);
}

// ── Construction ─────────────────────────────────────────────────────────────

PortfolioImporter::PortfolioImporter(ImportLogger& logger, bool dryRun)
    : m_logger(logger)
    , m_dryRun(dryRun)
{
}

// ── Top level ────────────────────────────────────────────────────────────────

void PortfolioImporter::importPortfolio(const RawPortfolio& portfolio)
{
    m_logger.info(QStringLiteral("Import gestartet — %1 Aktie(n) im Quell-XML%2")
                      .arg(portfolio.shares.size())
                      .arg(m_dryRun ? QStringLiteral(" [DRY-RUN — keine Schreibzugriffe]") : QString()));

    for (const RawShare& share : portfolio.shares)
        importShare(share);

    m_logger.info(QStringLiteral("Import abgeschlossen."));
}

// ── Share ────────────────────────────────────────────────────────────────────

void PortfolioImporter::importShare(const RawShare& share)
{
    if (share.wkn.trimmed().isEmpty()) {
        m_logger.log(QStringLiteral("Share"), share.name, ImportLogger::Action::Error,
                     QStringLiteral("Keine WKN vorhanden — Aktie übersprungen."));
        return;
    }

    // Datenqualitäts-Hinweise aus dem Parser protokollieren: sicher
    // auto-korrigierte Formatdetails (z.B. doppelt-XML-escapte WebSite-URLs,
    // siehe XmlPortfolioParser::normalizeWebSiteUrl()) als INFO, und
    // strukturelle Datenfehler in der Quelle, die bewusst NICHT automatisch
    // übernommen wurden (z.B. falscher Elementname "<MarketValues>" statt
    // "<MarketValue>"), als ERROR — unabhängig davon, ob die Aktie neu
    // angelegt oder wiederverwendet wird, damit der Datenfehler in der
    // Quelle sichtbar bleibt und nicht stillschweigend durchrutscht.
    for (const QString& warning : share.parseWarnings) {
        m_logger.log(QStringLiteral("Share"), share.wkn, ImportLogger::Action::Info, warning);
    }
    for (const QString& error : share.parseErrors) {
        m_logger.log(QStringLiteral("Share"), share.wkn, ImportLogger::Action::Error, error);
    }

    ShareRepository shareRepo;
    QString shareGuid;

    const ShareObject existing = shareRepo.findByWkn(share.wkn);
    if (existing.isValid()) {
        shareGuid = existing.guid();
        m_logger.log(QStringLiteral("Share"), share.wkn, ImportLogger::Action::Reused,
                     QStringLiteral("Aktie bereits vorhanden (GUID %1) — Stammdaten nicht "
                                   "überschrieben, importiere nur fehlende Transaktionen.")
                         .arg(shareGuid));
    } else {
        shareGuid = QUuid::createUuid().toString(QUuid::WithoutBraces);

        const QString updateStr = share.updateStr.trimmed();
        const ShareUpdateType updateType =
            (updateStr.compare(QStringLiteral("MarketPrice"), Qt::CaseInsensitive) == 0) ? ShareUpdateType::MarketPrice :
            (updateStr.compare(QStringLiteral("DailyValues"), Qt::CaseInsensitive) == 0) ? ShareUpdateType::DailyValues :
            (updateStr.compare(QStringLiteral("None"),        Qt::CaseInsensitive) == 0) ? ShareUpdateType::None :
            ShareUpdateType::Both; // default / "Both"

        const auto parsingTypeOf = [](const QString& s) {
            if (s.compare(QStringLiteral("ApiYahoo"), Qt::CaseInsensitive) == 0)
                return ShareParsingType::ApiYahoo;
            if (s.compare(QStringLiteral("ApiOnVista"), Qt::CaseInsensitive) == 0 ||
                s.compare(QStringLiteral("ApiOnvista"), Qt::CaseInsensitive) == 0)
                return ShareParsingType::ApiOnVista;
            return ShareParsingType::Regex;
        };

        const QString typeStr = share.shareTypeStr.trimmed();
        const ShareType shareType =
            (typeStr == QStringLiteral("1")) ? ShareType::Fond :
            (typeStr == QStringLiteral("2")) ? ShareType::Etf  :
            ShareType::Share;

        const ShareObject newShare(
            shareGuid,
            share.wkn,
            share.isin,
            share.name,
            shareType,
            QStringLiteral("EUR"),
            toIsoDate(share.stockMarketLaunchDate),  // addDateTime — entspricht der
                                                      // "Börsennotierung" (listingDate),
                                                      // siehe C#-Kompatibilitätshinweis in PresenterShareEdit
            toDouble(share.sharePrice),
            toDouble(share.sharePriceBefore),
            toIsoDateTime(share.lastUpdateInternet),
            toIsoDateTime(share.lastUpdateShareDate),
            updateType,
            parsingTypeOf(share.marketValueParsing),
            share.marketValueWebSite,
            QStringLiteral("UTF-8"),
            parsingTypeOf(share.dailyValuesParsing),
            share.dailyValuesWebSite,
            QStringLiteral("UTF-8"),
            share.detailsWebSite,
            QString()); // imagePath — nicht im Quell-XML vorhanden

        if (m_dryRun) {
            m_logger.log(QStringLiteral("Share"), share.wkn, ImportLogger::Action::Inserted,
                         QStringLiteral("[DRY-RUN] würde neu angelegt werden (GUID %1)").arg(shareGuid));
        } else if (shareRepo.insert(newShare)) {
            m_logger.log(QStringLiteral("Share"), share.wkn, ImportLogger::Action::Inserted,
                         QStringLiteral("GUID %1").arg(shareGuid));
        } else {
            m_logger.log(QStringLiteral("Share"), share.wkn, ImportLogger::Action::Error,
                         shareRepo.lastError().text().isEmpty()
                             ? QStringLiteral("unbekannter Datenbankfehler")
                             : shareRepo.lastError().text());
            return; // ohne Share kein Ziel für die Kindobjekte
        }

        if (!share.culture.trimmed().isEmpty()) {
            m_logger.info(QStringLiteral("Share %1: <Culture>=\"%2\" hat keine Entsprechung "
                                         "im aktuellen Schema — ignoriert.")
                              .arg(share.wkn, share.culture));
        }
    }

    importBuys(share, shareGuid);
    importSales(share, shareGuid);
    importBrokerages(share, shareGuid);
    importDividends(share, shareGuid);
    importDailyValues(share, shareGuid);
}

// ── Buys ─────────────────────────────────────────────────────────────────────

void PortfolioImporter::importBuys(const RawShare& share, const QString& shareGuid)
{
    BuyRepository buyRepo;
    for (const RawBuy& b : share.buys) {
        if (b.guid.trimmed().isEmpty()) {
            m_logger.log(QStringLiteral("Buy"), b.orderNumber, ImportLogger::Action::Error,
                         QStringLiteral("Keine GUID im Quell-XML — übersprungen."));
            continue;
        }
        if (buyRepo.findByGuid(b.guid).isValid()) {
            m_logger.log(QStringLiteral("Buy"), b.guid, ImportLogger::Action::Skipped,
                         QStringLiteral("Bereits importiert (GUID existiert bereits)."));
            continue;
        }

        const BuyObject buy(
            b.guid,
            shareGuid,
            b.depotNumber,
            b.orderNumber,
            toIsoDateTime(b.date),
            toDouble(b.volume),
            toDouble(b.volumeSold),
            toDouble(b.price),
            b.brokerageGuid,
            b.doc);

        if (m_dryRun) {
            m_logger.log(QStringLiteral("Buy"), b.guid, ImportLogger::Action::Inserted,
                         QStringLiteral("[DRY-RUN] Order %1").arg(b.orderNumber));
        } else if (buyRepo.insert(buy)) {
            m_logger.log(QStringLiteral("Buy"), b.guid, ImportLogger::Action::Inserted,
                         QStringLiteral("Order %1").arg(b.orderNumber));
        } else {
            m_logger.log(QStringLiteral("Buy"), b.guid, ImportLogger::Action::Error,
                         buyRepo.lastError().text().isEmpty()
                             ? QStringLiteral("unbekannter Datenbankfehler")
                             : buyRepo.lastError().text());
        }
    }
}

// ── Sales ────────────────────────────────────────────────────────────────────

void PortfolioImporter::importSales(const RawShare& share, const QString& shareGuid)
{
    SaleRepository saleRepo;
    for (const RawSale& s : share.sales) {
        if (s.guid.trimmed().isEmpty()) {
            m_logger.log(QStringLiteral("Sale"), s.orderNumber, ImportLogger::Action::Error,
                         QStringLiteral("Keine GUID im Quell-XML — übersprungen."));
            continue;
        }
        if (saleRepo.findByGuid(s.guid).isValid()) {
            m_logger.log(QStringLiteral("Sale"), s.guid, ImportLogger::Action::Skipped,
                         QStringLiteral("Bereits importiert (GUID existiert bereits)."));
            continue;
        }

        QList<SaleBuyDetail> details;
        for (const RawUsedBuy& u : s.usedBuys) {
            details.append(SaleBuyDetail(
                u.buyGuid,
                toIsoDateTime(u.buyDate),
                toDouble(u.buyVolume),
                toDouble(u.buyPrice),
                toDouble(u.reduction),
                toDouble(u.brokerage)));
        }

        const SaleObject sale(
            s.guid,
            shareGuid,
            s.depotNumber,
            s.orderNumber,
            toIsoDateTime(s.date),
            toDouble(s.volume),
            toDouble(s.salePrice),
            details,
            toDouble(s.taxAtSource),
            toDouble(s.capitalGainsTax),
            toDouble(s.solidarityTax),
            s.brokerageGuid,
            0.0, 0.0, 0.0, 0.0, // provision/brokerFee/traderFee/reduction: nicht Teil der
                                // sales-Tabelle, kommen über den verknüpften Brokerage-Datensatz
            s.doc);

        if (m_dryRun) {
            m_logger.log(QStringLiteral("Sale"), s.guid, ImportLogger::Action::Inserted,
                         QStringLiteral("[DRY-RUN] Order %1, %2 Kauf-Zuteilung(en)")
                             .arg(s.orderNumber).arg(details.size()));
        } else if (saleRepo.insert(sale)) {
            m_logger.log(QStringLiteral("Sale"), s.guid, ImportLogger::Action::Inserted,
                         QStringLiteral("Order %1, %2 Kauf-Zuteilung(en)")
                             .arg(s.orderNumber).arg(details.size()));
        } else {
            m_logger.log(QStringLiteral("Sale"), s.guid, ImportLogger::Action::Error,
                         saleRepo.lastError().text().isEmpty()
                             ? QStringLiteral("unbekannter Datenbankfehler")
                             : saleRepo.lastError().text());
        }
    }
}

// ── Brokerages ───────────────────────────────────────────────────────────────

void PortfolioImporter::importBrokerages(const RawShare& share, const QString& shareGuid)
{
    BrokerageRepository brokRepo;
    BuyRepository       buyRepo;
    SaleRepository      saleRepo;

    for (const RawBrokerage& b : share.brokerages) {
        if (b.guid.trimmed().isEmpty()) {
            m_logger.log(QStringLiteral("Brokerage"), b.guidBuySale, ImportLogger::Action::Error,
                         QStringLiteral("Keine GUID im Quell-XML — übersprungen."));
            continue;
        }
        if (brokRepo.findByGuid(b.guid).isValid()) {
            m_logger.log(QStringLiteral("Brokerage"), b.guid, ImportLogger::Action::Skipped,
                         QStringLiteral("Bereits importiert (GUID existiert bereits)."));
            continue;
        }
        if (b.guidBuySale.trimmed().isEmpty()) {
            m_logger.log(QStringLiteral("Brokerage"), b.guid, ImportLogger::Action::Error,
                         QStringLiteral("Kein GuidBuySale im Quell-XML — übersprungen."));
            continue;
        }

        // Die Zuordnung wird NICHT aus BuyPart/SalePart übernommen, sondern
        // anhand der tatsächlich vorhandenen Buy-/Sale-Datensätze ermittelt.
        // BuyPart/SalePart in der Quell-XML können falsch gesetzt sein — siehe
        // Import vom 01.07.2026: zwei Verkaufs-Brokerages trugen BuyPart="True",
        // obwohl GuidBuySale auf eine Sale zeigte. Buys/Sales sind zu diesem
        // Zeitpunkt bereits importiert (Insert-Reihenfolge: buys -> sales -> brokerage).
        const bool existsAsBuy  = buyRepo.findByGuid(b.guidBuySale).isValid();
        const bool existsAsSale = saleRepo.findByGuid(b.guidBuySale).isValid();

        QString buyGuid;
        QString saleGuid;

        if (existsAsBuy && !existsAsSale) {
            buyGuid = b.guidBuySale;
        } else if (existsAsSale && !existsAsBuy) {
            saleGuid = b.guidBuySale;
        } else if (existsAsBuy && existsAsSale) {
            // GUID-Kollision zwischen Buy und Sale — bei echten UUIDs praktisch
            // ausgeschlossen. Nicht selbst auflösen, sondern laut melden.
            m_logger.log(QStringLiteral("Brokerage"), b.guid, ImportLogger::Action::Error,
                         QStringLiteral("GuidBuySale %1 existiert sowohl als Buy als auch als Sale "
                                       "— Zuordnung nicht eindeutig, übersprungen.")
                             .arg(b.guidBuySale));
            continue;
        } else {
            // Weder als Buy noch als Sale gefunden, z.B. weil der referenzierte
            // Datensatz selbst beim Import fehlgeschlagen ist (Kaskaden-Fehler,
            // etwa durch eine OrderNumber-Kollision beim zugehörigen Buy).
            m_logger.log(QStringLiteral("Brokerage"), b.guid, ImportLogger::Action::Error,
                         QStringLiteral("GuidBuySale %1 existiert weder als Buy noch als Sale "
                                       "— übersprungen.")
                             .arg(b.guidBuySale));
            continue;
        }

        // BuyPart/SalePart aus der Quelle gegen den tatsächlichen Befund prüfen
        // und bei Widerspruch loggen — deckt Datenfehler in der Quelle auf,
        // statt sie stillschweigend zu übernehmen oder hart abzubrechen.
        const bool flagsSayBuy  = b.buyPart  && !b.salePart;
        const bool flagsSaySale = b.salePart && !b.buyPart;
        const bool flagsMatch   = (!buyGuid.isEmpty() && flagsSayBuy) ||
                                  (!saleGuid.isEmpty() && flagsSaySale);
        if (!flagsMatch) {
            m_logger.info(QStringLiteral("Brokerage %1: BuyPart/SalePart in der Quelle "
                                         "(BuyPart=%2, SalePart=%3) widerspricht dem tatsächlichen "
                                         "Befund (%4) — Zuordnung anhand der Datenbank korrigiert.")
                              .arg(b.guid,
                                   b.buyPart  ? QStringLiteral("True") : QStringLiteral("False"),
                                   b.salePart ? QStringLiteral("True") : QStringLiteral("False"),
                                   buyGuid.isEmpty() ? QStringLiteral("Sale") : QStringLiteral("Buy")));
        }

        const BrokerageObject brokerage(
            b.guid,
            shareGuid,
            buyGuid,
            saleGuid,
            toIsoDateTime(b.date),
            toDouble(b.provision),
            toDouble(b.brokerFee),
            toDouble(b.traderFee),
            toDouble(b.reduction),
            b.doc);

        const QString linkInfo = !buyGuid.isEmpty()
            ? QStringLiteral("verknüpft mit Buy %1").arg(buyGuid)
            : QStringLiteral("verknüpft mit Sale %1").arg(saleGuid);

        if (m_dryRun) {
            m_logger.log(QStringLiteral("Brokerage"), b.guid, ImportLogger::Action::Inserted,
                         QStringLiteral("[DRY-RUN] %1").arg(linkInfo));
        } else if (brokRepo.insert(brokerage)) {
            m_logger.log(QStringLiteral("Brokerage"), b.guid, ImportLogger::Action::Inserted, linkInfo);
        } else {
            m_logger.log(QStringLiteral("Brokerage"), b.guid, ImportLogger::Action::Error,
                         brokRepo.lastError().text().isEmpty()
                             ? QStringLiteral("unbekannter Datenbankfehler")
                             : brokRepo.lastError().text());
        }
    }
}

// ── Dividends ────────────────────────────────────────────────────────────────

void PortfolioImporter::importDividends(const RawShare& share, const QString& shareGuid)
{
    DividendRepository divRepo;
    for (const RawDividend& d : share.dividends) {
        if (d.guid.trimmed().isEmpty()) {
            m_logger.log(QStringLiteral("Dividend"), d.date, ImportLogger::Action::Error,
                         QStringLiteral("Keine GUID im Quell-XML — übersprungen."));
            continue;
        }
        if (divRepo.findByGuid(d.guid).isValid()) {
            m_logger.log(QStringLiteral("Dividend"), d.guid, ImportLogger::Action::Skipped,
                         QStringLiteral("Bereits importiert (GUID existiert bereits)."));
            continue;
        }

        const bool    enableFc      = d.hasForeignCurrency && d.fc.enabled;
        const double  exchangeRatio = enableFc ? toDouble(d.fc.exchangeRatio) : 1.0;
        const QString currency      = (enableFc && !d.fc.currency.trimmed().isEmpty())
                                           ? d.fc.currency
                                           : QStringLiteral("EUR");

        const DividendObject dividend(
            d.guid,
            shareGuid,
            toIsoDateTime(d.date),
            toDouble(d.rate),
            toDouble(d.volume),
            toDouble(d.taxAtSource),
            toDouble(d.capitalGainsTax),
            toDouble(d.solidarityTax),
            toDouble(d.price),
            enableFc,
            exchangeRatio,
            currency,
            d.doc);

        if (m_dryRun) {
            m_logger.log(QStringLiteral("Dividend"), d.guid, ImportLogger::Action::Inserted,
                         QStringLiteral("[DRY-RUN] %1").arg(d.date));
        } else if (divRepo.insert(dividend)) {
            m_logger.log(QStringLiteral("Dividend"), d.guid, ImportLogger::Action::Inserted, d.date);
        } else {
            m_logger.log(QStringLiteral("Dividend"), d.guid, ImportLogger::Action::Error,
                         divRepo.lastError().text().isEmpty()
                             ? QStringLiteral("unbekannter Datenbankfehler")
                             : divRepo.lastError().text());
        }
    }
}

// ── Daily values ─────────────────────────────────────────────────────────────

void PortfolioImporter::importDailyValues(const RawShare& share, const QString& shareGuid)
{
    if (share.dailyValues.isEmpty())
        return;

    QList<DailyValuesObject> values;
    int parseErrors = 0;

    for (const RawDailyValue& e : share.dailyValues) {
        const QDate date = toDate(e.date);
        if (!date.isValid()) {
            ++parseErrors;
            continue;
        }
        values.append(DailyValuesObject(
            shareGuid,
            date,
            toDouble(e.open),
            toDouble(e.close),
            toDouble(e.top),
            toDouble(e.bottom),
            toDouble(e.volume)));
    }

    if (parseErrors > 0) {
        m_logger.log(QStringLiteral("DailyValue"), share.wkn, ImportLogger::Action::Error,
                     QStringLiteral("%1 Eintrag/Einträge mit ungültigem Datum übersprungen.")
                         .arg(parseErrors));
    }

    if (values.isEmpty())
        return;

    if (m_dryRun) {
        m_logger.log(QStringLiteral("DailyValue"), share.wkn, ImportLogger::Action::Inserted,
                     QStringLiteral("[DRY-RUN] %1 Tageswert(e) würden importiert/aktualisiert "
                                   "(INSERT OR REPLACE)").arg(values.size()));
        return;
    }

    // upsertList() nutzt INSERT OR REPLACE über den Composite-Key (share_guid, date) —
    // dadurch ist ein erneuter Import bei aktualisierten Kursdaten immer sicher.
    DailyValuesRepository dvRepo;
    if (dvRepo.upsertList(values)) {
        m_logger.log(QStringLiteral("DailyValue"), share.wkn, ImportLogger::Action::Inserted,
                     QStringLiteral("%1 Tageswert(e) importiert/aktualisiert").arg(values.size()));
    } else {
        m_logger.log(QStringLiteral("DailyValue"), share.wkn, ImportLogger::Action::Error,
                     dvRepo.lastError().text().isEmpty()
                         ? QStringLiteral("unbekannter Datenbankfehler")
                         : dvRepo.lastError().text());
    }
}

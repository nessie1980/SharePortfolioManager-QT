// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "PresenterShareAdd.h"
#include "../../utils/DocumentClassifier.h"

#include <QTimer>
#include <QUuid>
#include <QDateTime>
#include <QDir>
#include <QFile>

// ─────────────────────────────────────────────────────────────────────────────
PresenterShareAdd::PresenterShareAdd(IViewShareAdd*   view,
                                     IModelShareAdd*  model,
                                     DocumentsConfig* config,
                                     QObject*         parent)
    : QObject(parent)
    , m_view(view)
    , m_model(model)
    , m_config(config)
{
    connect(&m_parser, &ParserLib::Parser::parserUpdated,
            this,      &PresenterShareAdd::onParserUpdated);
    connect(&m_pdfExtractor, &PdfTextExtractor::finished,
            this,            &PresenterShareAdd::onPdfTextExtracted);
}

// ─────────────────────────────────────────────────────────────────────────────
void PresenterShareAdd::onDocumentSelected(const QString& filePath)
{
    if (filePath.isEmpty())
        return;

    m_pendingPdfPath = filePath;
    m_pdfExtractor.extract(filePath);
}

// ─────────────────────────────────────────────────────────────────────────────
// Replaces the former onPdfConversionFinished(int, int) QProcess slot —
// PdfTextExtractor now owns the pdftotext invocation (see ARCHITECTURE.md).
void PresenterShareAdd::onPdfTextExtracted(bool success, const QString& text)
{
    if (!success) {
        m_view->showError(QObject::tr(
            "PDF-Konvertierung fehlgeschlagen oder kein Text extrahierbar."));
        return;
    }

    m_pdfText = text;
    startParserForText(m_pdfText);
}

// ─────────────────────────────────────────────────────────────────────────────
// Bank-/document-type detection now delegates to DocumentClassifier
// (see ARCHITECTURE.md) — behaviour unchanged, including the fallback to
// DocumentType::Buy when the bank matched but no identifier did.
void PresenterShareAdd::startParserForText(const QString& pdfText)
{
    if (!m_config || !m_config->isValid())
        return;

    int bankIndex = -1;
    if (!DocumentClassifier::matchBankIndex(pdfText, *m_config, bankIndex)) {
        const QStringList required = {
            "wkn","isin","name","date","depotNumber",
            "orderNumber","volume","price"
        };
        for (const auto& f : required)
            m_view->setFieldError(f);
        m_view->onParseFinished();
        return;
    }

    const BankEntry matchedBank = m_config->entries().at(bankIndex);
    const DocumentType docType = DocumentClassifier::detectDocumentType(
        pdfText, matchedBank, DocumentType::Buy);

    const DocumentEntry* docEntry =
        DocumentsConfig::findDocument(matchedBank, docType);
    if (!docEntry) {
        m_view->onParseFinished();
        return;
    }

    // ── Step 4: start ParserLib with text + regexList ─────────────────────
    m_view->setUiBusy(true);
    m_view->setParseProgress(0, QObject::tr("Dokumenten-Analyse läuft..."));

    ParserLib::ParsingValues pv(pdfText,
                                docEntry->encoding.isEmpty()
                                    ? QStringLiteral("UTF-8")
                                    : docEntry->encoding,
                                docEntry->regexList);
    m_parser.setParsingValues(pv);
    m_parser.startParsing();
}

// ─────────────────────────────────────────────────────────────────────────────
void PresenterShareAdd::onParserUpdated(const ParserLib::ParserInfoState& state)
{
    // ── Progress updates ──────────────────────────────────────────────────
    if (state.lastErrorCode == ParserLib::ParserErrorCode::SearchRunning ||
        state.lastErrorCode == ParserLib::ParserErrorCode::SearchStarted)
    {
        const QString statusText = state.lastRegexListKey.isEmpty()
            ? QObject::tr("Dokumenten-Analyse läuft...")
            : QObject::tr("Analysiere: %1").arg(state.lastRegexListKey);
        m_view->setParseProgress(state.percentage, statusText);
        return;
    }

    // ── Error states ──────────────────────────────────────────────────────
    if (state.lastErrorCode < ParserLib::ParserErrorCode::NoError &&
        state.lastErrorCode != ParserLib::ParserErrorCode::ParsingFailed)
    {
        m_view->setParseStatusIcon(1); // SearchFailed
        m_view->setParseProgress(0, QObject::tr("Analyse fehlgeschlagen: %1")
                                    .arg(state.exceptionMessage));
        m_view->setUiBusy(false);
        m_view->showError(QObject::tr("Parser-Fehler: %1")
                          .arg(state.exceptionMessage));
        m_view->onParseFinished();
        return;
    }

    // ── Finished (success or partial via ParsingFailed on optional field) ─
    if (state.lastErrorCode == ParserLib::ParserErrorCode::Finished ||
        state.lastErrorCode == ParserLib::ParserErrorCode::ParsingFailed)
    {
        populateFromResult(state.searchResult);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void PresenterShareAdd::populateFromResult(
    const QMap<QString, QList<QString>>& result)
{
    // All known XML field names — ensures every field is processed even if
    // ParserLib stopped early due to a required-field miss (ParsingFailed).
    static const QStringList knownXmlNames = {
        "Wkn","Isin","Name","Date","Time","DepotNumber","OrderNumber",
        "Volume","Price","Provision","BrokerFee","TraderPlaceFee","Reduction"
    };

    // Required fields — must all be found to allow saving
    static const QStringList requiredXmlNames = {
        "Wkn","Isin","Name","Date","DepotNumber","OrderNumber","Volume","Price"
    };

    int found         = 0;
    int requiredFound = 0;
    for (const QString& xmlName : knownXmlNames) {
        const QString viewField = xmlNameToViewField(xmlName);
        if (viewField.isEmpty())
            continue;

        if (result.contains(xmlName)) {
            const QList<QString>& values = result[xmlName];
            if (!values.isEmpty() && !values.first().trimmed().isEmpty()) {
                m_view->setFieldOk(viewField, values.first().trimmed());
                ++found;
                if (requiredXmlNames.contains(xmlName))
                    ++requiredFound;
                continue;
            }
        }
        // Field missing or empty — onParseFinished marks it with Info icon
    }

    m_view->onParseFinished();

    // Defer UI unblock so Qt can repaint widgets (sync parser holds event loop)
    const int reqTotal     = requiredXmlNames.size();
    const int optionalFound = found - requiredFound;  // optional fields that were found
    const int optionalTotal = knownXmlNames.size() - reqTotal;

    QTimer::singleShot(0, this, [this, requiredFound, reqTotal,
                                  optionalFound, optionalTotal]() {
        m_view->setUiBusy(false);

        if (requiredFound == reqTotal) {
            m_view->setParseStatusIcon(0); // SearchOk
            if (optionalFound > 0)
                m_view->setParseProgress(100,
                    QObject::tr("Analyse OK — %1/%1 Pflicht, %2/%3 Optional")
                    .arg(reqTotal).arg(optionalFound).arg(optionalTotal));
            else
                m_view->setParseProgress(100,
                    QObject::tr("Analyse OK — %1/%1 Pflicht")
                    .arg(reqTotal));
        } else {
            m_view->setParseStatusIcon(1); // SearchFailed
            if (optionalFound > 0)
                m_view->setParseProgress(100,
                    QObject::tr("Analyse fehlgeschlagen — %1/%2 Pflicht, %3/%4 Optional")
                    .arg(requiredFound).arg(reqTotal)
                    .arg(optionalFound).arg(optionalTotal));
            else
                m_view->setParseProgress(100,
                    QObject::tr("Analyse fehlgeschlagen — %1/%2 Pflicht")
                    .arg(requiredFound).arg(reqTotal));
        }
    });
}

// ─────────────────────────────────────────────────────────────────────────────
void PresenterShareAdd::onSave()
{
    const QString error = validateInput();
    if (!error.isEmpty()) {
        m_view->showError(error);
        return;
    }

    // ── Build ShareObject ─────────────────────────────────────────────────
    const QString shareGuid = QUuid::createUuid().toString(QUuid::WithoutBraces);

    ShareObject share(shareGuid,
                      m_view->wkn().trimmed(),
                      m_view->isin().trimmed(),
                      m_view->name().trimmed(),
                      m_view->shareType());
    share.setAddDateTime(QDateTime::currentDateTime().toString(Qt::ISODate));
    share.setDetailsWebSiteUrl(m_view->detailsWebsite().trimmed());
    share.setMarketPriceUrl(m_view->marketPriceUrl().trimmed());
    share.setMarketPriceParsingType(m_view->marketPriceParsingType());
    share.setMarketPriceEncoding(QStringLiteral("UTF-8"));
    share.setDailyValuesUrl(m_view->dailyValuesUrl().trimmed());
    share.setDailyValuesParsingType(m_view->dailyValuesParsingType());
    share.setDailyValuesEncoding(QStringLiteral("UTF-8"));

    // ── Build BuyObject ───────────────────────────────────────────────────
    const QString buyGuid = QUuid::createUuid().toString(QUuid::WithoutBraces);

    BuyObject buy(buyGuid,
                  shareGuid,
                  m_view->depotNumber().trimmed(),
                  m_view->orderNumber().trimmed(),
                  m_view->buyDateTime().toString(Qt::ISODate),
                  m_view->volume(),
                  0.0,
                  m_view->price(),
                  QString(),  // brokerageGuid set by model after brokerage insert
                  m_view->documentPath().trimmed());

    // ── Persist ───────────────────────────────────────────────────────────
    if (!m_model->saveShareWithBuy(share, buy,
                                   m_view->provision(),
                                   m_view->brokerFee(),
                                   m_view->traderFee(),
                                   m_view->reduction())) {
        m_view->showError(m_model->lastError());
        return;
    }

    m_view->acceptAndClose();
}

// ─────────────────────────────────────────────────────────────────────────────
void PresenterShareAdd::onCancel()
{
    if (m_parser.isBusy())
        m_parser.cancelParsing();
}

// ─────────────────────────────────────────────────────────────────────────────
// static
QString PresenterShareAdd::xmlNameToViewField(const QString& xmlName)
{
    static const QMap<QString, QString> map = {
        { QStringLiteral("Wkn"),            QStringLiteral("wkn")         },
        { QStringLiteral("Isin"),           QStringLiteral("isin")        },
        { QStringLiteral("Name"),           QStringLiteral("name")        },
        { QStringLiteral("Date"),           QStringLiteral("date")        },
        { QStringLiteral("Time"),           QStringLiteral("time")        },
        { QStringLiteral("DepotNumber"),    QStringLiteral("depotNumber") },
        { QStringLiteral("OrderNumber"),    QStringLiteral("orderNumber") },
        { QStringLiteral("Volume"),         QStringLiteral("volume")      },
        { QStringLiteral("Price"),          QStringLiteral("price")       },
        { QStringLiteral("Provision"),      QStringLiteral("provision")   },
        { QStringLiteral("BrokerFee"),      QStringLiteral("brokerFee")   },
        { QStringLiteral("TraderPlaceFee"), QStringLiteral("traderFee")   },
        { QStringLiteral("Reduction"),      QStringLiteral("reduction")   },
    };
    return map.value(xmlName, QString());
}

// ─────────────────────────────────────────────────────────────────────────────
QString PresenterShareAdd::validateInput() const
{
    // ── Check for fields still missing after PDF parse ────────────────────
    QStringList missingFields;
    if (m_view->hasMissingRequiredFields(missingFields)) {
        m_view->markMissingFieldsAsFailed();
        return QObject::tr(
            "Es fehlen noch Pflichtangaben.\n"
            "Die fehlenden Felder sind in der Maske rot markiert.");
    }

    // ── Plausibility checks ───────────────────────────────────────────────
    if (m_view->wkn().trimmed().isEmpty())
        return QObject::tr("WKN darf nicht leer sein.");

    if (m_view->name().trimmed().isEmpty())
        return QObject::tr("Name darf nicht leer sein.");

    if (m_view->listingDate() >= QDate::currentDate())
        return QObject::tr("Bitte ein gültiges Börsennotierungsdatum eingeben "
                           "(muss in der Vergangenheit liegen).");

    if (m_view->depotNumber().trimmed().isEmpty())
        return QObject::tr("Bitte eine Depotnummer aus der Liste wählen.");

    if (m_view->marketPriceUrl().trimmed().isEmpty())
        return QObject::tr("Bitte die Markt-Werte-Webseite eingeben.");

    if (m_view->dailyValuesUrl().trimmed().isEmpty())
        return QObject::tr("Bitte die Tages-Werte-Webseite eingeben.");

    if (m_view->volume() <= 0.0)
        return QObject::tr("Anteile müssen größer als 0 sein.");

    if (m_view->price() <= 0.0)
        return QObject::tr("Kurs muss größer als 0 sein.");

    if (!m_view->buyDateTime().isValid())
        return QObject::tr("Bitte ein gültiges Kaufdatum eingeben.");

    if (m_model->wknExists(m_view->wkn().trimmed()))
        return QObject::tr("Die WKN \"%1\" ist bereits im Portfolio vorhanden.")
               .arg(m_view->wkn().trimmed());

    if (!m_view->isin().trimmed().isEmpty()
        && m_model->isinExists(m_view->isin().trimmed()))
    {
        return QObject::tr("Die ISIN \"%1\" ist bereits im Portfolio vorhanden.")
               .arg(m_view->isin().trimmed());
    }

    return {};
}

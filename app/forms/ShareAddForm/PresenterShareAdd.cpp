// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "PresenterShareAdd.h"
#include "../../utils/PdfTextExtractor.h"   // converterInfo()/converterMissingMessage()
#include "../../config/DocumentFieldNames.h"
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
    // Bugfix 21.08.2026: write the path (+ type icon + preview) into the view
    // here, same as ViewShareAdd::onBrowseDocument() already did — this call
    // is the ONLY path taken when a document is dropped onto "Direkte
    // Dokumentenerfassung" (MainWindow::openCaptureDialog() calls
    // dlg.presenter()->onDocumentSelected() directly, bypassing
    // onBrowseDocument() entirely). Without it the field stayed on "Kein
    // Dokument ausgewählt …" even though parsing succeeded. See
    // ARCHITECTURE.md.
    m_view->setDocumentPath(filePath);
    // Ohne PDF-Wandler wird NICHT ausgewertet — angehaengt aber schon
    // (korrigiert 04.09.2026). Diese Methode macht zwei Dinge: das Dokument
    // anhaengen (Pfad, Vorschau, Dublettenpruefung) und es auswerten. Nur das
    // zweite braucht pdftotext. Der Riegel stand zuerst ganz oben und nahm
    // damit auch das Anhaengen weg — ohne Wandler liess sich kein Beleg mehr
    // zuordnen, obwohl der Pfad ohnehin unabhaengig davon in der Datenbank
    // landet und die Felder von Hand gefuellt werden koennen.
    //
    // Die Meldung ersetzt die frueher hier erscheinende "PDF-Konvertierung
    // fehlgeschlagen oder kein Text extrahierbar" — dieselbe wie bei einem
    // Beleg ohne Textebene, weshalb der Benutzer den Fehler beim Dokument
    // suchte statt bei der fehlenden Installation. Siehe ARCHITECTURE.md,
    // "Fehlendes pdftotext wird nicht als solches benannt".
    if (!PdfTextExtractor::converterInfo().available) {
        m_view->showError(PdfTextExtractor::converterMissingMessage());
        return;
    }

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
// Depot-/document-type detection now delegates to DocumentClassifier
// (see ARCHITECTURE.md) — behaviour unchanged, including the fallback to
// DocumentType::Buy when the depot matched but no identifier did.
void PresenterShareAdd::startParserForText(const QString& pdfText)
{
    if (!m_config || !m_config->isValid())
        return;

    int depotIndex = -1;
    if (!DocumentClassifier::matchDepotIndex(pdfText, *m_config, depotIndex)) {
        // Bis zum 02.09.2026 stand hier eine dritte, von Hand gepflegte
        // Liste derselben acht Feldschluessel. Sie war die einzige Stelle,
        // die niemand gegen die uebrigen Tabellen abgeglichen hat — jetzt
        // entsteht sie aus requiredXmlNames() ueber dieselbe Uebersetzung,
        // die auch populateFromResult() benutzt. Damit kann sie nicht mehr
        // auseinanderlaufen, und die Tests decken sie mit ab.
        for (const QString& xmlName : requiredXmlNames()) {
            const QString viewField = xmlNameToViewField(xmlName);
            if (!viewField.isEmpty())
                m_view->setFieldError(viewField);
        }
        m_view->onParseFinished();
        return;
    }

    const DepotEntry matchedDepot = m_config->entries().at(depotIndex);
    const DocumentType docType = DocumentClassifier::detectDocumentType(
        pdfText, matchedDepot, DocumentType::Buy);

    const DocumentEntry* docEntry =
        DocumentsConfig::findDocument(matchedDepot, docType);
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
    // Die beiden Listen liegen seit dem 02.09.2026 in knownXmlNames() /
    // requiredXmlNames() statt hier lokal — nur so kommen die Tests an sie
    // heran. Inhalt unveraendert.
    const QStringList& known    = knownXmlNames();
    const QStringList& required = requiredXmlNames();

    int found         = 0;
    int requiredFound = 0;
    for (const QString& xmlName : known) {
        const QString viewField = xmlNameToViewField(xmlName);
        if (viewField.isEmpty())
            continue;

        if (result.contains(xmlName)) {
            const QList<QString>& values = result[xmlName];
            if (!values.isEmpty() && !values.first().trimmed().isEmpty()) {
                // Seit 27.08.2026 zaehlt nur, was die View auch UEBERNOMMEN
                // hat — nicht mehr, was der Parser gefangen hat. Vorher
                // meldete die Statuszeile "Analyse OK — 5/5 Pflicht", waehrend
                // am Feld das rote Symbol stand, weil die View den Rohwert
                // verworfen hatte (unbrauchbares Datum, unbekannte
                // Depotnummer). Siehe ARCHITECTURE.md, "Analyse-Statuszeile
                // und Feldsymbole".
                if (m_view->setFieldOk(viewField, values.first().trimmed())) {
                    ++found;
                    if (required.contains(xmlName))
                        ++requiredFound;
                }
                continue;
            }
        }
        // Field missing or empty — onParseFinished marks it with Info icon
    }

    m_view->onParseFinished();

    // Defer UI unblock so Qt can repaint widgets (sync parser holds event loop)
    const int reqTotal     = required.size();
    const int optionalFound = found - requiredFound;  // optional fields that were found
    const int optionalTotal = known.size() - reqTotal;

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

    // 06.08.2026: Der Anlage-Dialog bietet keine Update-Typ-Auswahl an, und
    // validateInput() erzwingt volume() > 0 — eine neu angelegte Aktie hat
    // also immer Anteile im Bestand und braucht damit zwingend Tageswerte
    // (siehe ShareUpdateRules). Bisher ergab sich das nur beiläufig aus dem
    // Vorgabewert von ShareObject::m_updateType; hier bewusst explizit
    // gesetzt, damit eine spätere Änderung dieses Vorgabewerts nicht
    // stillschweigend Aktien ohne Kurshistorie anlegt, die dann aus dem
    // Depotwert-Chart herausfallen.
    share.setUpdateType(ShareUpdateType::Both);

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
const QStringList& PresenterShareAdd::knownXmlNames()
{
    // Weiterleitung — die Liste selbst liegt seit dem 02.09.2026 in
    // app/config/DocumentFieldNames.cpp. Sie beschreibt Documents.xml,
    // nicht diese Maske, und tst_documentsxml kommt dort ohne den
    // halben MVP-Stack an sie heran. Formularcode fragt weiterhin sein
    // eigenes Formular.
    return DocumentFieldNames::shareAddKnown();
}

// ─────────────────────────────────────────────────────────────────────────────
// static
const QStringList& PresenterShareAdd::requiredXmlNames()
{
    // Weiterleitung — die Liste selbst liegt seit dem 02.09.2026 in
    // app/config/DocumentFieldNames.cpp. Sie beschreibt Documents.xml,
    // nicht diese Maske, und tst_documentsxml kommt dort ohne den
    // halben MVP-Stack an sie heran. Formularcode fragt weiterhin sein
    // eigenes Formular.
    return DocumentFieldNames::shareAddRequired();
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

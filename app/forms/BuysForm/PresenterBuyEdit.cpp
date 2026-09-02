// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "PresenterBuyEdit.h"
#include "../../config/DocumentFieldNames.h"
#include "../../utils/DocumentClassifier.h"
#include "../../utils/ShareSplitHint.h"

#include <QTimer>
#include <QUuid>
#include <QDateTime>

// ── Constructor ───────────────────────────────────────────────────────────────

PresenterBuyEdit::PresenterBuyEdit(IViewBuyEdit*    view,
                                   IModelBuyEdit*   model,
                                   const QString&   shareGuid,
                                   DocumentsConfig* config,
                                   QObject*         parent)
    : QObject(parent)
    , m_view(view)
    , m_model(model)
    , m_config(config)
    , m_shareGuid(shareGuid)
{
    connect(&m_parser, &ParserLib::Parser::parserUpdated,
            this,      &PresenterBuyEdit::onParserUpdated);
    connect(&m_pdfExtractor, &PdfTextExtractor::finished,
            this,            &PresenterBuyEdit::onPdfTextExtracted);

    m_splits = m_model->loadSplits(m_shareGuid);

    reloadOverview();
    m_view->clearForm();
    m_view->setButtonStates(/*canRemove=*/false, /*isLastBuy=*/false, /*isEdit=*/false);
    refreshSplitHint();
}

// ── onSave ────────────────────────────────────────────────────────────────────

void PresenterBuyEdit::onSave()
{
    const QString error = validateInput();
    if (!error.isEmpty()) {
        m_view->showError(error);
        return;
    }

    const bool isEdit = !m_currentBuyGuid.isEmpty();

    // ── Non-latest buy: only the document path may change ─────────────────
    if (isEdit && !m_isLastBuy) {
        for (BuyObject& b : m_buys) {
            if (b.guid() == m_currentBuyGuid) {
                b.setDocument(m_view->documentPath().trimmed());
                const BrokerageObject brokerage =
                    m_model->loadBrokerage(m_currentBuyGuid);
                if (!m_model->updateBuy(b,
                        brokerage.isValid() ? brokerage.provision()  : 0.0,
                        brokerage.isValid() ? brokerage.brokerFee()  : 0.0,
                        brokerage.isValid() ? brokerage.traderFee()  : 0.0,
                        brokerage.isValid() ? brokerage.reduction()  : 0.0)) {
                    m_view->showError(m_model->lastError());
                    return;
                }
                emit dataChanged();
                reloadOverview();
                m_currentBuyGuid.clear();
                m_isLastBuy = false;
                m_view->setButtonStates(/*canRemove=*/false, /*isLastBuy=*/false, /*isEdit=*/false);
                m_view->showOverviewTab();
                return;
            }
        }
        return;
    }

    // ── New buy or latest-buy full edit ───────────────────────────────────
    const QString guid = isEdit
        ? m_currentBuyGuid
        : QUuid::createUuid().toString(QUuid::WithoutBraces);

    double volumeSold = 0.0;
    if (isEdit) {
        for (const BuyObject& b : std::as_const(m_buys)) {
            if (b.guid() == m_currentBuyGuid) {
                volumeSold = b.volumeSold();
                break;
            }
        }
    }

    const BuyObject buy(guid, m_shareGuid,
                        m_view->depotNumber().trimmed(),
                        m_view->orderNumber().trimmed(),
                        m_view->dateTime(),
                        m_view->volume(), volumeSold, m_view->price(),
                        QString(), m_view->documentPath().trimmed());

    const bool ok = isEdit
        ? m_model->updateBuy(buy, m_view->provision(), m_view->brokerFee(),
                             m_view->traderFee(), m_view->reduction())
        : m_model->addBuy(buy,   m_view->provision(), m_view->brokerFee(),
                          m_view->traderFee(), m_view->reduction());

    if (!ok) { m_view->showError(m_model->lastError()); return; }

    emit dataChanged();
    reloadOverview();

    // After saving (new or edit): reset state and jump to Übersicht.
    m_currentBuyGuid.clear();
    m_isLastBuy = false;
    m_view->setButtonStates(/*canRemove=*/false, /*isLastBuy=*/false, /*isEdit=*/false);
    m_view->showOverviewTab();
}

// ── onRemove ──────────────────────────────────────────────────────────────────

void PresenterBuyEdit::onRemove()
{
    if (m_currentBuyGuid.isEmpty()) return;

    // Guard: only the latest buy with no sold shares may be deleted.
    for (const BuyObject& b : std::as_const(m_buys)) {
        if (b.guid() == m_currentBuyGuid) {
            if (!m_isLastBuy || !qFuzzyIsNull(b.volumeSold())) {
                m_view->showError(tr("Dieser Kauf kann nicht entfernt werden.\n"
                                     "Es darf nur der jüngste Kauf gelöscht werden, "
                                     "sofern noch keine Anteile daraus verkauft wurden."));
                return;
            }
            break;
        }
    }

    if (!m_model->removeBuy(m_currentBuyGuid)) {
        m_view->showError(m_model->lastError());
        return;
    }

    emit dataChanged();
    m_currentBuyGuid.clear();
    m_isLastBuy = false;
    reloadOverview();
    m_view->setButtonStates(false, false, false);
    m_view->showOverviewTab();
}

// ── onReset ───────────────────────────────────────────────────────────────────

void PresenterBuyEdit::onReset()
{
    m_currentBuyGuid.clear();
    m_isLastBuy     = false;
    m_view->setButtonStates(false, false, false);
    refreshDerivedValues();
    m_view->showOverviewTab();  // calls clearForm() + switches to tab 0
}

// ── onClose ───────────────────────────────────────────────────────────────────

void PresenterBuyEdit::onClose()
{
    m_view->acceptAndClose();
}

// ── onRowSelected ─────────────────────────────────────────────────────────────

void PresenterBuyEdit::onRowSelected(const QString& buyGuid)
{
    if (buyGuid.isEmpty()) { onReset(); return; }

    for (const BuyObject& b : std::as_const(m_buys)) {
        if (b.guid() == buyGuid) {
            m_currentBuyGuid = buyGuid;
            m_isLastBuy = isLatestBuy(buyGuid);
            const bool canRemove = m_isLastBuy && qFuzzyIsNull(b.volumeSold());
            const BrokerageObject brokerage = m_model->loadBrokerage(buyGuid);
            m_view->loadBuy(b, brokerage);
            m_view->setVolumeSold(b.volumeSold());
            if (!b.document().isEmpty())
                m_view->openPdfPreview(b.document());
            else
                m_view->clearPdfPreview();
            m_view->setButtonStates(/*canRemove=*/canRemove, /*isLastBuy=*/m_isLastBuy, /*isEdit=*/true);
            refreshDerivedValues();

            // Validate all fields so icons reflect the loaded state.
            onDateEdited();
            onDepotNumberEdited();
            onOrderNumberEdited();
            onVolumeOrPriceEdited();
            onFeeEdited(QStringLiteral("provision"), m_view->provision());
            onFeeEdited(QStringLiteral("brokerFee"), m_view->brokerFee());
            onFeeEdited(QStringLiteral("traderFee"), m_view->traderFee());
            onFeeEdited(QStringLiteral("reduction"),  m_view->reduction());
            onDocumentPathEdited();
            return;
        }
    }
}

// ── onValuesChanged ───────────────────────────────────────────────────────────

void PresenterBuyEdit::onValuesChanged()
{
    refreshDerivedValues();
}

// ── Live field validation ─────────────────────────────────────────────────────

void PresenterBuyEdit::onDateEdited()
{
    // Date is a QDateEdit — always valid as long as it's not the null sentinel.
    // We use the sentinel date 2000-01-01 as "not set" (QDateEdit minimum).
    const QDate d = QDate::fromString(
        m_view->dateTime().left(10), Qt::ISODate);
    if (d.isValid() && d > QDate(2000, 1, 1))
        m_view->setFieldOk(QStringLiteral("date"), QString());
    else
        m_view->setFieldError(QStringLiteral("date"));

    // Der Hinweis hängt am Datum und soll live mitlaufen (Nessies Entscheidung
    // 08.08.2026) — refreshDerivedValues() allein genügt nicht, es wird beim
    // Ändern des Datums nicht aufgerufen.
    refreshSplitHint();
}

void PresenterBuyEdit::onDepotNumberEdited()
{
    if (!m_view->depotNumber().trimmed().isEmpty())
        m_view->setFieldOk(QStringLiteral("depotNumber"), QString());
    else
        m_view->setFieldError(QStringLiteral("depotNumber"));
}

void PresenterBuyEdit::onOrderNumberEdited()
{
    const QString nr = m_view->orderNumber().trimmed();
    if (nr.isEmpty()) {
        m_view->setFieldError(QStringLiteral("orderNumber"));
        return;
    }
    if (m_model->orderNumberExists(m_shareGuid, nr, m_currentBuyGuid)) {
        m_view->setFieldError(QStringLiteral("orderNumber"));
    } else {
        m_view->setFieldOk(QStringLiteral("orderNumber"), QString());
    }
}

void PresenterBuyEdit::onVolumeOrPriceEdited()
{
    if (m_view->volume() > 0.0)
        m_view->setFieldOk(QStringLiteral("volume"), QString());
    else
        m_view->setFieldError(QStringLiteral("volume"));

    if (m_view->price() > 0.0)
        m_view->setFieldOk(QStringLiteral("price"), QString());
    else
        m_view->setFieldError(QStringLiteral("price"));
}

void PresenterBuyEdit::onFeeEdited(const QString& fieldKey, double value)
{
    // Optional fields: only validate if non-zero (user entered something).
    // Negative values are invalid.
    if (value < 0.0)
        m_view->setFieldError(fieldKey);
    else
        m_view->setFieldOk(fieldKey, QString());
}

void PresenterBuyEdit::onDocumentPathEdited()
{
    const QString path = m_view->documentPath().trimmed();
    if (path.isEmpty()) {
        // Document is optional — clear any previous error icon.
        m_view->setFieldOk(QStringLiteral("document"), QString());
        return;
    }
    if (m_model->documentExists(path, m_currentBuyGuid))
        m_view->setFieldError(QStringLiteral("document"));
    else
        m_view->setFieldOk(QStringLiteral("document"), QString());
}

// ── onDocumentSelected ────────────────────────────────────────────────────────
// Identical to PresenterShareAdd::onDocumentSelected()

void PresenterBuyEdit::onDocumentSelected(const QString& path)
{
    if (path.isEmpty()) return;

    m_pendingPdfPath = path;
    // Bugfix 21.08.2026: write the path into the view's document field here,
    // same as ViewBuyEdit::onBrowseDocument() already did — this call is the
    // ONLY path taken when a document is dropped onto "Direkte
    // Dokumentenerfassung" (MainWindow::openCaptureDialog() calls
    // dlg.presenter()->onDocumentSelected() directly, bypassing
    // onBrowseDocument() entirely). Without it the field stayed on "Kein
    // Dokument ausgewählt …" even though parsing succeeded. See
    // ARCHITECTURE.md.
    m_view->setDocumentPath(path);
    m_view->openPdfPreview(path);
    onDocumentPathEdited();   // validate duplicate before parsing

    // For non-latest buys only the document path is updated — no re-parse.
    // In new-buy mode (no selection) parsing is always allowed.
    const bool isNonLatestEdit = !m_currentBuyGuid.isEmpty() && !m_isLastBuy;
    if (isNonLatestEdit) return;

    m_pdfExtractor.extract(path);
}

// ── onPdfTextExtracted ────────────────────────────────────────────────────────
// Replaces the former onPdfConversionFinished(int, int) QProcess slot —
// PdfTextExtractor now owns the pdftotext invocation (see ARCHITECTURE.md).

void PresenterBuyEdit::onPdfTextExtracted(bool success, const QString& text)
{
    if (!success) {
        m_view->showError(QObject::tr(
            "PDF-Konvertierung fehlgeschlagen oder kein Text extrahierbar."));
        return;
    }

    m_pdfText = text;
    startParserForText(m_pdfText);
}

// ── startParserForText ────────────────────────────────────────────────────────
// Depot-/document-type detection now delegates to DocumentClassifier
// (see ARCHITECTURE.md) — behaviour unchanged, including the fallback to
// DocumentType::Buy when the depot matched but no identifier did.

void PresenterBuyEdit::startParserForText(const QString& pdfText)
{
    if (!m_config || !m_config->isValid()) return;

    int depotIndex = -1;
    if (!DocumentClassifier::matchDepotIndex(pdfText, *m_config, depotIndex)) {
        // Bis zum 02.09.2026 stand hier eine dritte, von Hand gepflegte Liste
        // derselben fuenf Feldschluessel — die einzige, die niemand gegen die
        // uebrigen Tabellen abgeglichen hat. Sie entsteht jetzt aus
        // requiredXmlNames() ueber dieselbe Uebersetzung wie in
        // populateFromResult() und kann damit nicht mehr auseinanderlaufen.
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

// ── onParserUpdated ───────────────────────────────────────────────────────────
// Identical to PresenterShareAdd::onParserUpdated()

void PresenterBuyEdit::onParserUpdated(const ParserLib::ParserInfoState& state)
{
    if (state.lastErrorCode == ParserLib::ParserErrorCode::SearchRunning ||
        state.lastErrorCode == ParserLib::ParserErrorCode::SearchStarted)
    {
        const QString statusText = state.lastRegexListKey.isEmpty()
            ? QObject::tr("Dokumenten-Analyse läuft...")
            : QObject::tr("Analysiere: %1").arg(state.lastRegexListKey);
        m_view->setParseProgress(state.percentage, statusText);
        return;
    }

    if (state.lastErrorCode < ParserLib::ParserErrorCode::NoError &&
        state.lastErrorCode != ParserLib::ParserErrorCode::ParsingFailed)
    {
        m_view->setParseStatusIcon(1);
        m_view->setParseProgress(0, QObject::tr("Analyse fehlgeschlagen: %1")
                                    .arg(state.exceptionMessage));
        m_view->setUiBusy(false);
        m_view->showError(QObject::tr("Parser-Fehler: %1")
                          .arg(state.exceptionMessage));
        m_view->onParseFinished();
        return;
    }

    if (state.lastErrorCode == ParserLib::ParserErrorCode::Finished ||
        state.lastErrorCode == ParserLib::ParserErrorCode::ParsingFailed)
    {
        populateFromResult(state.searchResult);
    }
}

// ── populateFromResult ────────────────────────────────────────────────────────
// Identical to PresenterShareAdd::populateFromResult()
// Only Buy-relevant fields (no Wkn/Isin/Name/marketUrl etc.)

void PresenterBuyEdit::populateFromResult(
    const QMap<QString, QList<QString>>& result)
{
    // ── WKN / ISIN-Prüfung ────────────────────────────────────────────────
    // Verify that the parsed document belongs to the share currently open.
    // At least one identifier must match; warn and abort if both mismatch.
    const ShareObject share = m_model->loadShare(m_shareGuid);
    if (share.isValid()) {
        const QString parsedWkn  = result.contains(QStringLiteral("Wkn"))
            ? result[QStringLiteral("Wkn")].value(0).trimmed().toUpper()
            : QString();
        const QString parsedIsin = result.contains(QStringLiteral("Isin"))
            ? result[QStringLiteral("Isin")].value(0).trimmed().toUpper()
            : QString();

        const bool wknMatch  = !parsedWkn.isEmpty()
            && parsedWkn.compare(share.wkn().trimmed(), Qt::CaseInsensitive) == 0;
        const bool isinMatch = !parsedIsin.isEmpty()
            && parsedIsin.compare(share.isin().trimmed(), Qt::CaseInsensitive) == 0;
        const bool anyParsed = !parsedWkn.isEmpty() || !parsedIsin.isEmpty();

        if (anyParsed && !wknMatch && !isinMatch) {
            QTimer::singleShot(0, this, [this, parsedWkn, parsedIsin,
                                          shareWkn = share.wkn(),
                                          shareName = share.wkn()]() {
                m_view->setUiBusy(false);
                m_view->setParseStatusIcon(1);
                m_view->setParseProgress(100,
                    QObject::tr("Dokument gehört nicht zu dieser Aktie"));
                m_view->showError(
                    QObject::tr(
                        "Das gewählte Dokument gehört nicht zur aktuell geöffneten Aktie.\n\n"
                        "Dokument:  WKN %1 / ISIN %2\n"
                        "Aktie:     WKN %3")
                    .arg(parsedWkn.isEmpty() ? QObject::tr("(nicht gefunden)") : parsedWkn)
                    .arg(parsedIsin.isEmpty() ? QObject::tr("(nicht gefunden)") : parsedIsin)
                    .arg(shareWkn));
            });
            m_view->onParseFinished();
            return;
        }
    }

    // Die beiden Listen liegen seit dem 02.09.2026 in knownXmlNames() /
    // requiredXmlNames() statt hier lokal — nur so kommen die Tests an sie
    // heran. Inhalt unveraendert.
    const QStringList& known    = knownXmlNames();
    const QStringList& required = requiredXmlNames();

    int found = 0, requiredFound = 0;
    for (const QString& xmlName : known) {
        const QString viewField = xmlNameToViewField(xmlName);
        if (viewField.isEmpty()) continue;

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
                    if (required.contains(xmlName)) ++requiredFound;
                }
                continue;
            }
        }
    }

    m_view->onParseFinished();

    const int reqTotal      = required.size();
    const int optionalFound = found - requiredFound;
    const int optionalTotal = known.size() - reqTotal;

    QTimer::singleShot(0, this, [this, requiredFound, reqTotal,
                                  optionalFound, optionalTotal]() {
        m_view->setUiBusy(false);

        if (requiredFound == reqTotal) {
            m_view->setParseStatusIcon(0);
            if (optionalFound > 0)
                m_view->setParseProgress(100,
                    QObject::tr("Analyse OK — %1/%1 Pflicht, %2/%3 Optional")
                    .arg(reqTotal).arg(optionalFound).arg(optionalTotal));
            else
                m_view->setParseProgress(100,
                    QObject::tr("Analyse OK — %1/%1 Pflicht").arg(reqTotal));
        } else {
            m_view->setParseStatusIcon(1);
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

// ── knownXmlNames / requiredXmlNames ──────────────────────────────────────────

// static
const QStringList& PresenterBuyEdit::knownXmlNames()
{
    // Weiterleitung — die Liste selbst liegt seit dem 02.09.2026 in
    // app/config/DocumentFieldNames.cpp. Sie beschreibt Documents.xml,
    // nicht diese Maske, und tst_documentsxml kommt dort ohne den
    // halben MVP-Stack an sie heran. Formularcode fragt weiterhin sein
    // eigenes Formular.
    return DocumentFieldNames::buyKnown();
}

// static
const QStringList& PresenterBuyEdit::requiredXmlNames()
{
    // Weiterleitung — die Liste selbst liegt seit dem 02.09.2026 in
    // app/config/DocumentFieldNames.cpp. Sie beschreibt Documents.xml,
    // nicht diese Maske, und tst_documentsxml kommt dort ohne den
    // halben MVP-Stack an sie heran. Formularcode fragt weiterhin sein
    // eigenes Formular.
    return DocumentFieldNames::buyRequired();
}

// ── xmlNameToViewField ────────────────────────────────────────────────────────

QString PresenterBuyEdit::xmlNameToViewField(const QString& xmlName)
{
    static const QMap<QString, QString> map = {
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

// ── reloadOverview ────────────────────────────────────────────────────────────

void PresenterBuyEdit::reloadOverview()
{
    m_buys = m_model->loadBuys(m_shareGuid);

    QList<BrokerageObject> brokerages;
    brokerages.reserve(m_buys.size());
    for (const BuyObject& b : std::as_const(m_buys))
        brokerages.append(m_model->loadBrokerage(b.guid()));

    // m_splits stammt aus dem Konstruktor und wird hier bewusst nicht neu
    // geladen: die Split-Maske ist aus diesem Dialog heraus nicht erreichbar,
    // die Liste kann sich während einer Sitzung also nicht ändern (derselbe
    // Zwischenspeicher, den auch refreshSplitHint() nutzt).
    m_view->populateOverview(m_buys, brokerages, m_splits);
}

// ── refreshDerivedValues ──────────────────────────────────────────────────────

void PresenterBuyEdit::refreshDerivedValues()
{
    const double vol    = m_view->volume();
    const double price  = m_view->price();
    const double prov   = m_view->provision();
    const double broker = m_view->brokerFee();
    const double trader = m_view->traderFee();
    const double red    = m_view->reduction();

    m_view->setKurswert(vol * price);
    m_view->setGesGebuehren(prov + broker + trader);
    m_view->setEndbetrag(vol * price + prov + broker + trader - red);

    refreshSplitHint();
}

// ── refreshSplitHint ──────────────────────────────────────────────────────────

void PresenterBuyEdit::refreshSplitHint()
{
    const QDate date = QDate::fromString(m_view->dateTime().left(10), Qt::ISODate);

    m_view->setSplitHint(
        ShareSplitHint::footerText(m_splits, date, m_view->volume(), m_view->price()),
        ShareSplitHint::tooltipText(m_splits, date),
        ShareSplitHint::hasSplitAfter(m_splits, date));
}

// ── validateInput ─────────────────────────────────────────────────────────────

QString PresenterBuyEdit::validateInput() const
{
    QStringList missingFields;
    if (m_view->hasMissingRequiredFields(missingFields)) {
        m_view->markMissingFieldsAsFailed();
        return QObject::tr(
            "Es fehlen noch Pflichtangaben.\n"
            "Die fehlenden Felder sind in der Maske rot markiert.");
    }

    if (m_model->orderNumberExists(m_shareGuid,
                                   m_view->orderNumber().trimmed(),
                                   m_currentBuyGuid)) {
        return QObject::tr("Die Auftragsnummer \"%1\" ist für diese Aktie bereits vorhanden.")
               .arg(m_view->orderNumber().trimmed());
    }

    const QString doc = m_view->documentPath().trimmed();
    if (!doc.isEmpty() && m_model->documentExists(doc, m_currentBuyGuid)) {
        return QObject::tr("Das Dokument \"%1\" ist bereits einem anderen Kauf zugeordnet.")
               .arg(doc);
    }

    return QString();
}

// ── isLatestBuy ───────────────────────────────────────────────────────────────

bool PresenterBuyEdit::isLatestBuy(const QString& buyGuid) const
{
    if (m_buys.isEmpty()) return false;

    // Find the buy with the most recent dateTime string.
    // ISO 8601 strings are lexicographically comparable, so a simple
    // string comparison is correct and avoids QDateTime parsing overhead.
    QString latestGuid;
    QString latestDt;
    for (const BuyObject& b : m_buys) {
        if (b.dateTime() > latestDt) {
            latestDt   = b.dateTime();
            latestGuid = b.guid();
        }
    }
    return latestGuid == buyGuid;
}

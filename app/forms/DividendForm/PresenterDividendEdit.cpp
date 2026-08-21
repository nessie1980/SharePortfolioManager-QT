// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "PresenterDividendEdit.h"
#include "../../utils/DocumentClassifier.h"

#include <QTimer>
#include <QUuid>
#include <QDateTime>

// ── Constructor ───────────────────────────────────────────────────────────────

PresenterDividendEdit::PresenterDividendEdit(IViewDividendEdit*  view,
                                             IModelDividendEdit* model,
                                             const QString&      shareGuid,
                                             DocumentsConfig*    config,
                                             QObject*            parent)
    : QObject(parent)
    , m_view(view)
    , m_model(model)
    , m_config(config)
    , m_shareGuid(shareGuid)
{
    connect(&m_parser, &ParserLib::Parser::parserUpdated,
            this,      &PresenterDividendEdit::onParserUpdated);
    connect(&m_pdfExtractor, &PdfTextExtractor::finished,
            this,            &PresenterDividendEdit::onPdfTextExtracted);

    // Splits einmalig laden — sie ändern sich während einer Dialog-Sitzung
    // nicht, ein Abruf je reloadOverview() wäre eine unnötige Abfrage
    // (Phase 3c, 11.08.2026).
    m_splits = m_model->loadSplits(m_shareGuid);

    reloadOverview();
    m_view->clearForm();
    m_view->setButtonStates(/*canRemove=*/false, /*isEdit=*/false);
}

// ── onSave ────────────────────────────────────────────────────────────────────

void PresenterDividendEdit::onSave()
{
    const QString error = validateInput();
    if (!error.isEmpty()) {
        m_view->showError(error);
        return;
    }

    const bool isEdit = !m_currentDividendGuid.isEmpty();

    // ── Neu oder Bearbeitung — alle Felder immer vollständig speicherbar ──
    const QString guid = isEdit
        ? m_currentDividendGuid
        : QUuid::createUuid().toString(QUuid::WithoutBraces);

    const DividendObject dividend(
        guid,
        m_shareGuid,
        m_view->dateTime(),
        m_view->rate(),
        m_view->volume(),
        m_view->taxAtSource(),
        m_view->capitalGainsTax(),
        m_view->solidarityTax(),
        m_view->priceAtPayday(),
        m_view->enableForeignCurrency(),
        m_view->enableForeignCurrency() ? m_view->exchangeRatio() : 1.0,
        m_view->enableForeignCurrency() ? m_view->currency() : QStringLiteral("EUR"),
        m_view->documentPath().trimmed());

    const bool ok = isEdit
        ? m_model->updateDividend(dividend)
        : m_model->addDividend(dividend);

    if (!ok) { m_view->showError(m_model->lastError()); return; }

    emit dataChanged();
    reloadOverview();

    m_currentDividendGuid.clear();
    m_view->setButtonStates(/*canRemove=*/false, /*isEdit=*/false);
    m_view->showOverviewTab();
}

// ── onRemove ──────────────────────────────────────────────────────────────────

void PresenterDividendEdit::onRemove()
{
    if (m_currentDividendGuid.isEmpty()) return;

    if (!m_model->removeDividend(m_currentDividendGuid)) {
        m_view->showError(m_model->lastError());
        return;
    }

    emit dataChanged();
    m_currentDividendGuid.clear();
    reloadOverview();
    m_view->setButtonStates(/*canRemove=*/false, /*isEdit=*/false);
    m_view->showOverviewTab();
}

// ── onReset ───────────────────────────────────────────────────────────────────

void PresenterDividendEdit::onReset()
{
    m_currentDividendGuid.clear();
    m_view->setButtonStates(/*canRemove=*/false, /*isEdit=*/false);
    refreshDerivedValues();
    m_view->showOverviewTab();
}

// ── onClose ───────────────────────────────────────────────────────────────────

void PresenterDividendEdit::onClose()
{
    m_view->acceptAndClose();
}

// ── onRowSelected ─────────────────────────────────────────────────────────────

void PresenterDividendEdit::onRowSelected(const QString& dividendGuid)
{
    if (dividendGuid.isEmpty()) { onReset(); return; }

    for (const DividendObject& d : std::as_const(m_dividends)) {
        if (d.guid() == dividendGuid) {
            m_currentDividendGuid = dividendGuid;

            m_view->loadDividend(d);
            if (!d.document().isEmpty())
                m_view->openPdfPreview(d.document());
            else
                m_view->clearPdfPreview();
            // Every dividend is fully editable and removable.
            m_view->setButtonStates(/*canRemove=*/true, /*isEdit=*/true);
            refreshDerivedValues();

            // Validate loaded fields so icons reflect the current state.
            onDateEdited();
            onRateEdited();
            onVolumeEdited();
            onPriceAtPaydayEdited();
            onTaxEdited(QStringLiteral("taxAtSource"),    m_view->taxAtSource());
            onTaxEdited(QStringLiteral("capitalGainsTax"),m_view->capitalGainsTax());
            onTaxEdited(QStringLiteral("solidarityTax"),  m_view->solidarityTax());
            onDocumentPathEdited();
            return;
        }
    }
}

// ── onValuesChanged ───────────────────────────────────────────────────────────

void PresenterDividendEdit::onValuesChanged()
{
    refreshDerivedValues();
}

// ── onForeignCurrencyToggled ──────────────────────────────────────────────────

void PresenterDividendEdit::onForeignCurrencyToggled(bool /*enabled*/)
{
    // setForeignCurrencyEnabled wird direkt von der View via Connect aufgerufen.
    refreshDerivedValues();
}

// ── Live field validation ─────────────────────────────────────────────────────

void PresenterDividendEdit::onDateEdited()
{
    const QDate d = QDate::fromString(
        m_view->dateTime().left(10), Qt::ISODate);
    if (!d.isValid() || d <= QDate(2000, 1, 1)) {
        m_view->setFieldError(QStringLiteral("date"));
        return;
    }

    m_view->setFieldOk(QStringLiteral("date"), QString());
    applyDailyValuePriceAtPayday(d);
}

void PresenterDividendEdit::onRateEdited()
{
    if (m_view->rate() > 0.0)
        m_view->setFieldOk(QStringLiteral("rate"), QString());
    else
        m_view->setFieldError(QStringLiteral("rate"));
}

void PresenterDividendEdit::onVolumeEdited()
{
    if (m_view->volume() > 0.0)
        m_view->setFieldOk(QStringLiteral("volume"), QString());
    else
        m_view->setFieldError(QStringLiteral("volume"));
}

void PresenterDividendEdit::onPriceAtPaydayEdited()
{
    if (m_view->priceAtPayday() > 0.0)
        m_view->setFieldOk(QStringLiteral("priceAtPayday"), QString());
    else
        m_view->setFieldError(QStringLiteral("priceAtPayday"));
}

// ── applyDailyValuePriceAtPayday ──────────────────────────────────────────────

void PresenterDividendEdit::applyDailyValuePriceAtPayday(const QDate& date)
{
    double closingPrice = 0.0;
    if (!m_model->findClosingPriceForDate(m_shareGuid, date, closingPrice))
        return;  // Kein Treffer in der DB → Feld bleibt unverändert.

    m_view->setFieldOk(
        QStringLiteral("priceAtPayday"),
        QString::number(closingPrice, 'f', 2),
        QObject::tr("Aus Tageswerten übernommen (Kurs vom %1)")
            .arg(date.toString(QStringLiteral("dd.MM.yyyy"))));
    refreshDerivedValues();
}

void PresenterDividendEdit::onExchangeRatioEdited()
{
    // Devisenkurs ist nur relevant wenn FC aktiviert ist.
    if (!m_view->enableForeignCurrency()) return;
    if (m_view->exchangeRatio() > 0.0)
        m_view->setFieldOk(QStringLiteral("exchangeRatio"), QString());
    else
        m_view->setFieldError(QStringLiteral("exchangeRatio"));
}

void PresenterDividendEdit::onTaxEdited(const QString& fieldKey, double value)
{
    // Tax fields are optional; negative values are invalid.
    if (value < 0.0)
        m_view->setFieldError(fieldKey);
    else
        m_view->setFieldOk(fieldKey, QString());
}

void PresenterDividendEdit::onDocumentPathEdited()
{
    const QString path = m_view->documentPath().trimmed();
    if (path.isEmpty()) {
        m_view->setFieldOk(QStringLiteral("document"), QString());
        return;
    }
    if (m_model->documentExists(path, m_currentDividendGuid))
        m_view->setFieldError(QStringLiteral("document"));
    else
        m_view->setFieldOk(QStringLiteral("document"), QString());
}

// ── onDocumentSelected ────────────────────────────────────────────────────────

void PresenterDividendEdit::onDocumentSelected(const QString& path)
{
    if (path.isEmpty()) return;

    m_pendingPdfPath = path;
    // Bugfix 21.08.2026: write the path into the view's document field here,
    // same as ViewDividendEdit::onBrowseDocument() already did — this call is
    // the ONLY path taken when a document is dropped onto "Direkte
    // Dokumentenerfassung" (MainWindow::openCaptureDialog() calls
    // dlg.presenter()->onDocumentSelected() directly, bypassing
    // onBrowseDocument() entirely). Without it the field stayed on "Kein
    // Dokument ausgewählt …" even though parsing succeeded — this was
    // Nessies' original bug report. See ARCHITECTURE.md.
    m_view->setDocumentPath(path);
    m_view->openPdfPreview(path);
    onDocumentPathEdited();

    // Parse the document to pre-fill form fields.
    m_pdfExtractor.extract(path);
}

// ── onPdfTextExtracted ────────────────────────────────────────────────────────
// Replaces the former onPdfConversionFinished(int, int) QProcess slot —
// PdfTextExtractor now owns the pdftotext invocation (see ARCHITECTURE.md).

void PresenterDividendEdit::onPdfTextExtracted(bool success, const QString& text)
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
// Bank-/document-type detection now delegates to DocumentClassifier
// (see ARCHITECTURE.md) — behaviour unchanged, including the fallback to
// DocumentType::Dividend when the bank matched but no identifier did.

void PresenterDividendEdit::startParserForText(const QString& pdfText)
{
    if (!m_config) { m_view->onParseFinished(); return; }

    int bankIndex = -1;
    if (!DocumentClassifier::matchBankIndex(pdfText, *m_config, bankIndex)) {
        const QStringList required = { "date", "rate", "volume" };
        for (const auto& f : required) m_view->setFieldError(f);
        m_view->onParseFinished();
        return;
    }

    const BankEntry matchedBank = m_config->entries().at(bankIndex);
    const DocumentType docType = DocumentClassifier::detectDocumentType(
        pdfText, matchedBank, DocumentType::Dividend);

    const DocumentEntry* docEntry =
        DocumentsConfig::findDocument(matchedBank, docType);
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

void PresenterDividendEdit::onParserUpdated(const ParserLib::ParserInfoState& state)
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

void PresenterDividendEdit::populateFromResult(
    const QMap<QString, QList<QString>>& result)
{
    // ── WKN / ISIN-Prüfung ────────────────────────────────────────────────
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
                                          shareWkn = share.wkn()]() {
                m_view->setUiBusy(false);
                m_view->setParseStatusIcon(1);
                m_view->setParseProgress(100,
                    QObject::tr("Dokument gehört nicht zu dieser Aktie"));
                m_view->showError(
                    QObject::tr(
                        "Das gewählte Dokument gehört nicht zur aktuell geöffneten Aktie.\n\n"
                        "Dokument:  WKN %1 / ISIN %2\n"
                        "Aktie:     WKN %3")
                    .arg(parsedWkn.isEmpty() ? QObject::tr("(nicht gefunden)") : parsedWkn,
                         parsedIsin.isEmpty() ? QObject::tr("(nicht gefunden)") : parsedIsin,
                         shareWkn));
            });
            m_view->onParseFinished();
            return;
        }
    }

    static const QStringList knownXmlNames = {
        "Date", "Time", "Volume", "DividendRate",
        "TaxAtSource", "CapitalGainTax", "SolidarityTax",
        "ExchangeRate", "Currency"
    };
    static const QStringList requiredXmlNames = {
        "Date", "Volume", "DividendRate"
    };

    int found = 0, requiredFound = 0;
    QDate parsedDate;  // für den Preis-Abgleich unten benötigt

    for (const QString& xmlName : knownXmlNames) {
        const QString viewField = xmlNameToViewField(xmlName);
        if (viewField.isEmpty()) continue;

        if (result.contains(xmlName)) {
            const QList<QString>& values = result[xmlName];
            if (!values.isEmpty() && !values.first().trimmed().isEmpty()) {
                const QString value = values.first().trimmed();
                m_view->setFieldOk(viewField, value);
                ++found;
                if (requiredXmlNames.contains(xmlName)) ++requiredFound;

                if (xmlName == QStringLiteral("Date")) {
                    QDate d = QDate::fromString(value, QStringLiteral("d.M.yyyy"));
                    if (!d.isValid()) d = QDate::fromString(value, Qt::ISODate);
                    parsedDate = d;
                }
            }
        }
    }

    // "Preis der Aktie am Auszahlungstag" mit dem GEPARSTEN Datum abgleichen —
    // nicht mit dem Datum, das vor dem Parsen im Formular stand. Ohne diesen
    // Schritt könnte ein Wert stehen bleiben, der zuvor über einen
    // beiläufigen Fokuswechsel auf das (noch mit dem Default "heute"
    // gefüllte) Datumsfeld ausgelöst wurde, bevor das eigentliche
    // Dokumentdatum bekannt war (siehe ARCHITECTURE.md, DividendForm-Details).
    if (parsedDate.isValid())
        applyDailyValuePriceAtPayday(parsedDate);

    m_view->onParseFinished();

    const int reqTotal      = requiredXmlNames.size();
    const int optionalFound = found - requiredFound;
    const int optionalTotal = knownXmlNames.size() - reqTotal;

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

// ── xmlNameToViewField ────────────────────────────────────────────────────────

QString PresenterDividendEdit::xmlNameToViewField(const QString& xmlName)
{
    static const QMap<QString, QString> map = {
        { QStringLiteral("Date"),           QStringLiteral("date")           },
        { QStringLiteral("Time"),           QStringLiteral("time")           },
        { QStringLiteral("Volume"),         QStringLiteral("volume")         },
        { QStringLiteral("DividendRate"),   QStringLiteral("rate")           },
        { QStringLiteral("TaxAtSource"),    QStringLiteral("taxAtSource")    },
        { QStringLiteral("CapitalGainTax"), QStringLiteral("capitalGainsTax")},
        { QStringLiteral("SolidarityTax"),  QStringLiteral("solidarityTax")  },
        { QStringLiteral("ExchangeRate"),   QStringLiteral("exchangeRatio")  },
        { QStringLiteral("Currency"),       QStringLiteral("currency")       },
    };
    return map.value(xmlName, QString());
}

// ── reloadOverview ────────────────────────────────────────────────────────────

void PresenterDividendEdit::reloadOverview()
{
    m_dividends = m_model->loadDividends(m_shareGuid);
    m_view->populateOverview(m_dividends, m_splits);
}

// ── refreshDerivedValues ──────────────────────────────────────────────────────

void PresenterDividendEdit::refreshDerivedValues()
{
    const double rate          = m_view->rate();
    const double volume        = m_view->volume();
    const double taxAtSource   = m_view->taxAtSource();
    const double capitalGains  = m_view->capitalGainsTax();
    const double solidarity    = m_view->solidarityTax();
    const double priceAtPayday = m_view->priceAtPayday();
    const bool   fcEnabled     = m_view->enableForeignCurrency();
    const double exchangeRatio = fcEnabled ? m_view->exchangeRatio() : 1.0;

    const double taxSum        = taxAtSource + capitalGains + solidarity;

    double payout   = 0.0;
    double payoutFc = 0.0;
    if (fcEnabled && exchangeRatio > 0.0) {
        payoutFc = rate * volume;
        payout   = payoutFc / exchangeRatio;
    } else {
        payout = rate * volume;
    }

    double yield = 0.0;
    if (priceAtPayday > 0.0)
        yield = rate / priceAtPayday * 100.0;

    m_view->setDividendPayout(payout);
    m_view->setDividendPayoutFc(payoutFc);
    m_view->setTaxSum(taxSum);
    m_view->setDividendPayoutWithTaxes(payout - taxSum);
    m_view->setYield(yield);
}

// ── validateInput ─────────────────────────────────────────────────────────────

QString PresenterDividendEdit::validateInput() const
{
    QStringList missingFields;
    if (m_view->hasMissingRequiredFields(missingFields)) {
        m_view->markMissingFieldsAsFailed();
        return QObject::tr(
            "Es fehlen noch Pflichtangaben.\n"
            "Die fehlenden Felder sind in der Maske rot markiert.");
    }

    const QString doc = m_view->documentPath().trimmed();
    if (!doc.isEmpty() && m_model->documentExists(doc, m_currentDividendGuid)) {
        return QObject::tr("Das Dokument \"%1\" ist bereits einer anderen Dividende zugeordnet.")
               .arg(doc);
    }

    return QString();
}

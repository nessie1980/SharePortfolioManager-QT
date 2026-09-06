// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "PresenterDividendEdit.h"
#include "../../utils/PdfTextExtractor.h"   // converterInfo()/converterMissingMessage()
#include "../../config/DocumentFieldNames.h"
#include "../../utils/DocumentClassifier.h"
#include "../../utils/DividendVolumeChecker.h"
#include "../../utils/ValueFormatter.h"

#include <QLocale>

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

    // Käufe/Verkäufe für die Stückzahl-Plausibilitätsprüfung (Phase 3,
    // 21.08.2026) — gleiche Überlegung wie bei m_splits, siehe Header.
    m_buys  = m_model->loadBuys(m_shareGuid);
    m_sales = m_model->loadSales(m_shareGuid);

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
        m_view->documentPath().trimmed(),
        m_view->exDate(),
        m_view->depotNumber().trimmed());

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
            onExDateEdited();
            onDepotNumberEdited();
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

    // Der Auszahlungstag ist die obere Schranke für den Ex-Tag (Blockade,
    // Nessies Entscheidung 21.08.2026 — "weil es eben nicht sein darf!").
    // Ändert sich der Auszahlungstag, muss die Ex-Tag-Prüfung live neu
    // laufen, sonst bliebe ein zuvor gültiges Ex-Tag-Feld fälschlich grün,
    // obwohl es jetzt nach dem (neuen) Auszahlungstag läge. VOR
    // applyDailyValuePriceAtPayday() aufgerufen, damit deren setFieldOk()
    // für "priceAtPayday" das zuletzt gesetzte Feld bleibt (Tests/Verhalten
    // erwarten das als sichtbares Ergebnis eines Datumswechsels).
    onExDateEdited();

    applyDailyValuePriceAtPayday(d);
}

void PresenterDividendEdit::onExDateEdited()
{
    const QDate exDate = QDate::fromString(m_view->exDate(), Qt::ISODate);
    if (!exDate.isValid() || exDate <= QDate(2000, 1, 1)) {
        m_view->setFieldError(QStringLiteral("exDate"));
        return;
    }

    // Blockade Ex-Tag > Auszahlungstag (Nessies Entscheidung 21.08.2026,
    // s. validateInput() für die verbindliche Prüfung beim Speichern — hier
    // nur die sofortige visuelle Rückmeldung).
    const QDate payday = QDate::fromString(m_view->dateTime().left(10), Qt::ISODate);
    if (payday.isValid() && exDate > payday) {
        m_view->setFieldError(QStringLiteral("exDate"));
        return;
    }

    m_view->setFieldOk(QStringLiteral("exDate"), QString());
}

void PresenterDividendEdit::onDepotNumberEdited()
{
    if (!m_view->depotNumber().trimmed().isEmpty())
        m_view->setFieldOk(QStringLiteral("depotNumber"), QString());
    else
        m_view->setFieldError(QStringLiteral("depotNumber"));
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

    // Bugfix 06.09.2026: hier stand QString::number(closingPrice, 'f', 2).
    // Zwei Probleme in einer Zeile — zwei Nachkommastellen, obwohl das Feld
    // einen Kurs enthaelt, und QString::number() formatiert immer nach
    // C-Konvention, schrieb also einen Punkt in ein Formular, das sonst
    // durchgaengig deutsch formatiert. Dieselbe Dividende sah je nach Weg
    // (uebernommen oder geladen) unterschiedlich aus.
    //
    // formatPriceForInput() statt formatPrice(), weil der Wert hier in ein
    // QLineEdit geht und von dort ueber parseDouble() zurueckgelesen wird —
    // ein Tausendertrennzeichen wuerde das Zuruecklesen zerstoeren. Siehe
    // ARCHITECTURE.md, "Zahlenfelder verlieren Werte ab 1.000 beim
    // Zuruecklesen".
    m_view->setFieldOk(
        QStringLiteral("priceAtPayday"),
        ValueFormatter::formatPriceForInput(closingPrice),
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
// Depot-/document-type detection now delegates to DocumentClassifier
// (see ARCHITECTURE.md) — behaviour unchanged, including the fallback to
// DocumentType::Dividend when the depot matched but no identifier did.

void PresenterDividendEdit::startParserForText(const QString& pdfText)
{
    if (!m_config) { m_view->onParseFinished(); return; }

    int depotIndex = -1;
    if (!DocumentClassifier::matchDepotIndex(pdfText, *m_config, depotIndex)) {
        // Depot nicht erkannt: alle Pflichtfelder rot markieren, damit klar
        // ist, dass nichts übernommen wurde. Seit Phase 2 gehören Ex-Tag und
        // Depotnummer dazu (ergänzt in Phase 5, 21.08.2026).
        // Bis zum 02.09.2026 stand hier eine dritte, von Hand gepflegte Liste
        // derselben fünf Feldschlüssel — die einzige, die niemand gegen die
        // übrigen Tabellen abgeglichen hat. Sie entsteht jetzt aus
        // requiredXmlNames() über dieselbe Übersetzung wie in
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
        pdfText, matchedDepot, DocumentType::Dividend);

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

    // Die beiden Listen liegen seit dem 02.09.2026 in knownXmlNames() /
    // requiredXmlNames() statt hier lokal — nur so kommen die Tests an sie
    // heran. Inhalt unverändert.
    const QStringList& known    = knownXmlNames();
    const QStringList& required = requiredXmlNames();

    int found = 0, requiredFound = 0;
    QDate parsedDate;  // für den Preis-Abgleich unten benötigt

    for (const QString& xmlName : known) {
        const QString viewField = xmlNameToViewField(xmlName);
        if (viewField.isEmpty()) continue;

        if (result.contains(xmlName)) {
            const QList<QString>& values = result[xmlName];
            if (!values.isEmpty() && !values.first().trimmed().isEmpty()) {
                const QString value = values.first().trimmed();
                // Seit 27.08.2026 zaehlt nur, was die View auch UEBERNOMMEN
                // hat — nicht mehr, was der Parser gefangen hat. Vorher
                // meldete die Statuszeile "Analyse OK — 5/5 Pflicht", waehrend
                // am Feld das rote Symbol stand, weil die View den Rohwert
                // verworfen hatte (unbrauchbares Datum, unbekannte
                // Depotnummer). Siehe ARCHITECTURE.md, "Analyse-Statuszeile
                // und Feldsymbole".
                if (m_view->setFieldOk(viewField, value)) {
                    ++found;
                    if (required.contains(xmlName)) ++requiredFound;
                }

                if (xmlName == QStringLiteral("Date")) {
                    QDate d = QDate::fromString(value, QStringLiteral("d.M.yyyy"));
                    if (!d.isValid()) d = QDate::fromString(value, Qt::ISODate);
                    parsedDate = d;
                }
            }
        }
    }

    // ── Ersatzhinweis, wenn der Beleg keinen Ex-Tag nennt (21.08.2026) ───
    // Cortal Consors nennt den Ex-Tag nicht, wohl aber den "Schlusstag"
    // (Dividenden-Stichtag), der laut Bank "normalerweise einen Tag vor dem
    // Ex-Tag" liegt. Daraus zu RECHNEN wäre geraten: der nächste HANDELStag
    // hängt von Wochenenden und Feiertagen ab, und ein um einen Tag falscher
    // Ex-Tag ginge unmittelbar in die Stückzahl-Plausibilitätsprüfung ein.
    // Der Wert wird deshalb nur angezeigt — eintragen muss ihn der Benutzer.
    //
    // Läuft VOR onParseFinished(), damit dessen allgemeiner Text ("Wert fehlt
    // noch — bitte manuell eingeben") den genaueren Hinweis nicht überschreibt.
    const QString parsedExDate = result.contains(QStringLiteral("ExDate"))
        ? result[QStringLiteral("ExDate")].value(0).trimmed()
        : QString();
    const QString parsedRecordDate = result.contains(QStringLiteral("RecordDate"))
        ? result[QStringLiteral("RecordDate")].value(0).trimmed()
        : QString();

    if (parsedExDate.isEmpty() && !parsedRecordDate.isEmpty()) {
        m_view->setFieldHint(
            QStringLiteral("exDate"),
            QObject::tr(
                "Dieser Beleg nennt keinen Ex-Tag.\n"
                "Schlusstag (Dividenden-Stichtag): %1\n"
                "Der Ex-Tag ist üblicherweise der nächste Handelstag — "
                "bitte selbst eintragen.")
                .arg(parsedRecordDate));
    }

    // ── Fremdwährung aus dem Beleg übernehmen (Phase 5, 21.08.2026) ───────
    // Der Devisenkurs wurde schon vor Phase 5 gelesen (ExchangeRate steht in
    // Documents.xml für alle Banken), landete aber wirkungslos im Feld:
    // onSave() übernimmt exchangeRatio() nur bei aktivem Fremdwährungs-Modus,
    // und den hat nie jemand eingeschaltet. Erst die Währung aus dem Beleg
    // macht den Kurs also nutzbar.
    //
    // Bewusst in BEIDE Richtungen: nennt der Beleg EUR, wird der Modus auch
    // wieder abgeschaltet. Der Beleg ist die Wahrheit über das Dokument — ein
    // aus einem vorherigen Import stehengebliebener Haken würde sonst eine
    // Euro-Ausschüttung durch einen fremden Devisenkurs teilen.
    const QString parsedCurrency = result.contains(QStringLiteral("Currency"))
        ? result[QStringLiteral("Currency")].value(0).trimmed().toUpper()
        : QString();

    if (!parsedCurrency.isEmpty()) {
        const bool isForeign = (parsedCurrency != QStringLiteral("EUR"));
        m_view->setForeignCurrency(isForeign, parsedCurrency);
        // Der Haken wurde ohne toggled()-Signal gesetzt (siehe
        // ViewDividendEdit::setForeignCurrency()), die abgeleiteten Werte
        // müssen deshalb von Hand nachgezogen werden.
        refreshDerivedValues();
        if (isForeign)
            onExchangeRatioEdited();
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

// ── xmlNameToViewField ────────────────────────────────────────────────────────

// static
const QStringList& PresenterDividendEdit::knownXmlNames()
{
    // Weiterleitung — die Liste selbst liegt seit dem 02.09.2026 in
    // app/config/DocumentFieldNames.cpp. Sie beschreibt Documents.xml,
    // nicht diese Maske, und tst_documentsxml kommt dort ohne den
    // halben MVP-Stack an sie heran. Formularcode fragt weiterhin sein
    // eigenes Formular.
    return DocumentFieldNames::dividendKnown();
}

// static
const QStringList& PresenterDividendEdit::requiredXmlNames()
{
    // Weiterleitung — die Liste selbst liegt seit dem 02.09.2026 in
    // app/config/DocumentFieldNames.cpp. Sie beschreibt Documents.xml,
    // nicht diese Maske, und tst_documentsxml kommt dort ohne den
    // halben MVP-Stack an sie heran. Formularcode fragt weiterhin sein
    // eigenes Formular.
    return DocumentFieldNames::dividendRequired();
}

QString PresenterDividendEdit::xmlNameToViewField(const QString& xmlName)
{
    static const QMap<QString, QString> map = {
        { QStringLiteral("Date"),           QStringLiteral("date")           },
        { QStringLiteral("Time"),           QStringLiteral("time")           },
        // Phase 5 (21.08.2026): Ex-Tag und Depotnummer werden jetzt aus dem
        // Beleg gelesen. Beide sind seit Phase 2 Pflichtfelder — bis hierher
        // musste der Benutzer sie nach jedem Import von Hand nachtragen.
        { QStringLiteral("ExDate"),         QStringLiteral("exDate")         },
        { QStringLiteral("DepotNumber"),    QStringLiteral("depotNumber")    },
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

    // Blockade Ex-Tag > Auszahlungstag — Nessies Entscheidung 21.08.2026:
    // "weil es eben nicht sein darf!". hasMissingRequiredFields() oben
    // garantiert bereits, dass exDate() ein gültiges (Nicht-Sentinel-)Datum
    // ist; hier folgt die eigentliche fachliche Prüfung gegen den
    // Auszahlungstag. onExDateEdited() gibt dieselbe Rückmeldung schon
    // live beim Editieren — diese Prüfung ist die verbindliche, die auch
    // greift, wenn kein editingFinished ausgelöst wurde (z.B. Wert kam
    // unverändert aus loadDividend()).
    const QDate exDate = QDate::fromString(m_view->exDate(), Qt::ISODate);
    const QDate payday  = QDate::fromString(m_view->dateTime().left(10), Qt::ISODate);
    if (exDate.isValid() && payday.isValid() && exDate > payday) {
        m_view->setFieldError(QStringLiteral("exDate"));
        return QObject::tr(
            "Der Ex-Tag darf nicht nach dem Auszahlungstag liegen.\n"
            "Bitte prüfen Sie das Datum.");
    }

    // ── Stückzahl-Plausibilitätsprüfung (Phase 3, 21.08.2026) ─────────────
    // Blockade statt Warnung — Nessies Entscheidung 21.08.2026. Mit Ex-Tag UND
    // Depotnummer als Pflichtfeldern darf eine Abweichung fachlich nicht mehr
    // vorkommen; siehe ARCHITECTURE.md, "Plausibilitätsprüfung der
    // Dividenden-Stückzahl". Läuft NACH der Ex-Tag-Prüfung oben, damit ein
    // widersprüchlicher Ex-Tag zuerst benannt wird — mit falschem Ex-Tag wäre
    // auch der errechnete Bestand falsch, und die Meldung würde in die Irre
    // führen.
    const DividendVolumeCheckResult volumeCheck = DividendVolumeChecker::check(
        m_view->volume(), exDate, m_view->depotNumber().trimmed(),
        m_buys, m_sales, m_splits);

    if (volumeCheck.checkable && !volumeCheck.matches) {
        m_view->setFieldError(QStringLiteral("volume"));
        return QObject::tr(
            "Die eingetragenen Anteile passen nicht zum Bestand des gewählten "
            "Depots am Ex-Tag.\n\n"
            "Eingetragen:            %1 Stk.\n"
            "Bestand am %2:  %3 Stk.\n\n"
            "Berücksichtigt wurden %4 Käufe und %5 Verkäufe im Depot \"%6\" "
            "vor dem Ex-Tag.\n"
            "Bitte Ex-Tag, Depotnummer und Stückzahl anhand der Abrechnung "
            "prüfen.")
            .arg(QLocale().toString(volumeCheck.enteredVolume,  'f', 4),
                 QLocale().toString(exDate, QLocale::ShortFormat),
                 QLocale().toString(volumeCheck.expectedVolume, 'f', 4),
                 QString::number(volumeCheck.consideredBuys),
                 QString::number(volumeCheck.consideredSales),
                 m_view->depotNumber().trimmed());
    }

    const QString doc = m_view->documentPath().trimmed();
    if (!doc.isEmpty() && m_model->documentExists(doc, m_currentDividendGuid)) {
        return QObject::tr("Das Dokument \"%1\" ist bereits einer anderen Dividende zugeordnet.")
               .arg(doc);
    }

    return QString();
}

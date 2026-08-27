// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "PresenterSaleEdit.h"
#include "../../utils/ShareSplitHint.h"
#include "../../utils/SplitRatioChecker.h"
#include "../../utils/DocumentClassifier.h"

#include <QTimer>
#include <QUuid>
#include <QDateTime>
#include <QLocale>

#include <utility>

// ── Constructor ───────────────────────────────────────────────────────────────

PresenterSaleEdit::PresenterSaleEdit(IViewSaleEdit*   view,
                                     IModelSaleEdit*  model,
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
            this,      &PresenterSaleEdit::onParserUpdated);
    connect(&m_pdfExtractor, &PdfTextExtractor::finished,
            this,            &PresenterSaleEdit::onPdfTextExtracted);

    // Populate available buys (no depot filter yet — depot not selected)
    m_view->populateAvailableBuys(m_model->loadAvailableBuys(shareGuid));
    // All buys (incl. fully sold) for document lookup in the Details dialog
    m_view->setAllBuys(m_model->loadAllBuys(shareGuid));
    // Splits der Aktie — einmalig, siehe IViewSaleEdit::setSplits() (Phase 2c
    // der Aktiensplit-Behandlung, 07.08.2026, ARCHITECTURE.md "Offene Punkte").
    m_splits = m_model->loadSplits(shareGuid);
    m_view->setSplits(m_splits);

    reloadOverview();
    m_view->clearForm();
    m_view->setButtonStates(/*canRemove=*/false, /*isLastSale=*/false, /*isEdit=*/false);
    refreshSplitHint();
}

// ── onSave ────────────────────────────────────────────────────────────────────

void PresenterSaleEdit::onSave()
{
    const QString error = validateInput();
    if (!error.isEmpty()) {
        m_view->showError(error);
        return;
    }

    const bool isEdit = !m_currentSaleGuid.isEmpty();

    // Non-latest sale: only the document path may change
    if (isEdit && !m_isLastSale) {
        for (SaleObject& s : m_sales) {
            if (s.guid() == m_currentSaleGuid) {
                s.setDocument(m_view->documentPath().trimmed());
                if (!m_model->updateSale(s)) {
                    m_view->showError(m_model->lastError());
                    return;
                }
                emit dataChanged();
                reloadOverview();
                m_currentSaleGuid.clear();
                m_isLastSale = false;
                m_view->setButtonStates(false, false, false);
                m_view->showOverviewTab();
                return;
            }
        }
        return;
    }

    // New sale or latest-sale full edit
    const QString guid = isEdit
        ? m_currentSaleGuid
        : QUuid::createUuid().toString(QUuid::WithoutBraces);

    // FIFO-Zuteilung immer frisch berechnen — sowohl für neue Verkäufe als
    // auch beim vollständigen Bearbeiten des jüngsten Verkaufs (07.08.2026,
    // Nessies Entscheidung: die vorherige Fassung übernahm beim Bearbeiten
    // unverändert die gespeicherten SaleBuyDetails, auch wenn sich die
    // Verkaufsmenge im Formular geändert hatte — siehe ARCHITECTURE.md,
    // "Offene Punkte", "Aktiensplits werden nicht behandelt", Phase 2c).
    // An dieser Stelle ist isEdit nur dann true, wenn zugleich m_isLastSale
    // gilt — der nicht-jüngste Zweig oben hat bereits per return verlassen.
    //
    // Beim Bearbeiten müssen die vom BISHERIGEN Verkauf beanspruchten
    // Anteile zunächst virtuell zurückgebucht werden: buy.volumeSold() in
    // der DB spiegelt bis zum tatsächlichen Speichern noch den alten
    // Verkauf wider, siehe loadAvailableBuysForDepotExcludingSale().
    const QList<BuyObject> available = isEdit
        ? m_model->loadAvailableBuysForDepotExcludingSale(
              m_shareGuid, m_view->depotNumber(), m_currentSaleGuid)
        : m_model->loadAvailableBuysForDepot(m_shareGuid, m_view->depotNumber());

    const QDate saleDate = QDateTime::fromString(m_view->dateTime(), Qt::ISODate).date();
    const QList<ShareSplitObject> splits = m_model->loadSplits(m_shareGuid);

    // Die anteiligen Kauf-Nebenkosten müssen hier mitgeschrieben werden —
    // SaleFifoAllocator kennt sie nicht und soll sie auch nicht kennen
    // (zustandslos und datenbankfrei, siehe SaleFifoAllocator.h). Bis zu
    // diesem Bugfix wurden nur vier der sechs Konstruktor-Parameter belegt;
    // reductionPart/brokeragePart haben Defaultwerte 0.0, weshalb der
    // Verlust ohne Compilerfehler blieb (siehe ARCHITECTURE.md).
    QList<SaleBuyDetail> buyDetails;
    for (const FifoAllocationRow& row :
         SaleFifoAllocator::allocate(m_view->volume(), saleDate, available, splits)) {
        double buyFees = 0.0;
        double buyRed  = 0.0;
        proportionalBuyCosts(row.buyGuid, row.volume, available, buyFees, buyRed);
        buyDetails.append(SaleBuyDetail(row.buyGuid, row.buyDateTime,
                                        row.volume, row.buyPrice,
                                        buyRed, buyFees));
    }

    const SaleObject sale(
        guid,
        m_shareGuid,
        m_view->depotNumber().trimmed(),
        m_view->orderNumber().trimmed(),
        m_view->dateTime(),
        m_view->volume(),
        m_view->salePrice(),
        buyDetails,
        m_view->taxAtSource(),
        m_view->capitalGainsTax(),
        m_view->solidarityTax(),
        QString(),   // brokerageGuid — generated by model
        m_view->provision(),
        m_view->brokerFee(),
        m_view->traderFee(),
        m_view->reduction(),
        m_view->documentPath().trimmed());

    const bool ok = isEdit
        ? m_model->updateSale(sale)
        : m_model->addSale(sale);

    if (!ok) { m_view->showError(m_model->lastError()); return; }

    emit dataChanged();
    reloadOverview();

    m_currentSaleGuid.clear();
    m_isLastSale = false;
    m_view->setButtonStates(false, false, false);
    m_view->showOverviewTab();
}

// ── onRemove ──────────────────────────────────────────────────────────────────

void PresenterSaleEdit::onRemove()
{
    if (m_currentSaleGuid.isEmpty()) return;

    if (!m_isLastSale) {
        m_view->showError(tr("Dieser Verkauf kann nicht entfernt werden.\n"
                              "Es darf nur der jüngste Verkauf gelöscht werden."));
        return;
    }

    if (!m_model->removeSale(m_currentSaleGuid)) {
        m_view->showError(m_model->lastError());
        return;
    }

    emit dataChanged();
    m_currentSaleGuid.clear();
    m_isLastSale = false;
    reloadOverview();
    m_view->setButtonStates(false, false, false);
    m_view->showOverviewTab();
}

// ── onReset ───────────────────────────────────────────────────────────────────

void PresenterSaleEdit::onReset()
{
    m_currentSaleGuid.clear();
    m_isLastSale = false;
    m_view->setButtonStates(false, false, false);
    // Clear depot filter — show all available buys again
    m_view->populateAvailableBuys(m_model->loadAvailableBuys(m_shareGuid));
    refreshDerivedValues();
    m_view->showOverviewTab();
}

// ── onClose ───────────────────────────────────────────────────────────────────

void PresenterSaleEdit::onClose()
{
    m_view->acceptAndClose();
}

// ── onRowSelected ─────────────────────────────────────────────────────────────

void PresenterSaleEdit::onRowSelected(const QString& saleGuid)
{
    if (saleGuid.isEmpty()) { onReset(); return; }

    for (const SaleObject& s : std::as_const(m_sales)) {
        if (s.guid() == saleGuid) {
            m_currentSaleGuid = saleGuid;
            m_isLastSale = isLatestSale(saleGuid);
            const bool canRemove = m_isLastSale;
            m_view->loadSale(s);
            // Refresh available buys for this sale's depot — mit
            // zurückgebuchten eigenen Anteilen, siehe
            // loadAvailableBuysForDepotExcludingSale() (Phase 2c). Für einen
            // nicht-jüngsten Verkauf bleibt das Ergebnis ungenutzt (die
            // Details-Vorschau zeigt dort die gespeicherte Zuteilung), daher
            // hier bewusst unbedingt aufgerufen statt nach isLastSale
            // verzweigt.
            m_view->populateAvailableBuys(
                m_model->loadAvailableBuysForDepotExcludingSale(
                    m_shareGuid, s.depotNumber(), saleGuid));
            if (!s.document().isEmpty())
                m_view->openPdfPreview(s.document());
            else
                m_view->clearPdfPreview();
            m_view->setButtonStates(canRemove, m_isLastSale, /*isEdit=*/true);
            refreshDerivedValues();

            // Validate all fields so icons reflect the loaded state.
            onDateEdited();
            onDepotNumberEdited();
            onOrderNumberEdited();
            onVolumeOrPriceEdited();
            onFeeEdited(QStringLiteral("provision"),       m_view->provision());
            onFeeEdited(QStringLiteral("brokerFee"),       m_view->brokerFee());
            onFeeEdited(QStringLiteral("traderFee"),       m_view->traderFee());
            onFeeEdited(QStringLiteral("reduction"),       m_view->reduction());
            onTaxEdited(QStringLiteral("taxAtSource"),     m_view->taxAtSource());
            onTaxEdited(QStringLiteral("capitalGainsTax"), m_view->capitalGainsTax());
            onTaxEdited(QStringLiteral("solidarityTax"),   m_view->solidarityTax());
            onDocumentPathEdited();
            return;
        }
    }
}

// ── onValuesChanged ───────────────────────────────────────────────────────────

void PresenterSaleEdit::onValuesChanged()
{
    refreshDerivedValues();
}

// ── Live field validation ─────────────────────────────────────────────────────

void PresenterSaleEdit::onDateEdited()
{
    const QDate d = QDate::fromString(
        m_view->dateTime().left(10), Qt::ISODate);
    if (d.isValid() && d > QDate(2000, 1, 1))
        m_view->setFieldOk(QStringLiteral("date"), QString());
    else
        m_view->setFieldError(QStringLiteral("date"));

    // Der Hinweis hängt am Datum und läuft live mit — refreshDerivedValues()
    // wird beim Ändern des Datums nicht aufgerufen.
    refreshSplitHint();
}

void PresenterSaleEdit::onDepotNumberEdited()
{
    if (!m_view->depotNumber().trimmed().isEmpty()) {
        m_view->setFieldOk(QStringLiteral("depotNumber"), QString());
    } else {
        m_view->setFieldError(QStringLiteral("depotNumber"));
    }
    // Refresh the buy list whenever the depot selection changes so the
    // Details dialog and FIFO calculation always use the right buys.
    // Beim Bearbeiten eines Verkaufs (m_currentSaleGuid gesetzt) dessen
    // eigene, bereits gebuchte Anteile zurückbuchen — Phase 2c, siehe
    // onRowSelected() oben.
    m_view->populateAvailableBuys(
        m_currentSaleGuid.isEmpty()
            ? m_model->loadAvailableBuysForDepot(m_shareGuid, m_view->depotNumber())
            : m_model->loadAvailableBuysForDepotExcludingSale(
                  m_shareGuid, m_view->depotNumber(), m_currentSaleGuid));
    refreshDerivedValues();
}

void PresenterSaleEdit::onOrderNumberEdited()
{
    const QString nr = m_view->orderNumber().trimmed();
    if (nr.isEmpty()) {
        m_view->setFieldError(QStringLiteral("orderNumber"));
        return;
    }
    if (m_model->orderNumberExists(m_shareGuid, nr, m_currentSaleGuid))
        m_view->setFieldError(QStringLiteral("orderNumber"));
    else
        m_view->setFieldOk(QStringLiteral("orderNumber"), QString());
}

void PresenterSaleEdit::onVolumeOrPriceEdited()
{
    // Bugfix (siehe ARCHITECTURE.md, "Skalenbewusste Mengenprüfung im
    // Verkaufsformular", 11.08.2026): eine Menge > 0, die die verfügbaren
    // Käufe übersteigt, zeigte bisher trotzdem einen grünen Haken —
    // SaleFifoAllocator::allocate() deckelte die Zuteilung still nach unten.
    if (m_view->volume() > 0.0 && isRequestedVolumeCovered())
        m_view->setFieldOk(QStringLiteral("volume"), QString());
    else
        m_view->setFieldError(QStringLiteral("volume"));

    if (m_view->salePrice() > 0.0)
        m_view->setFieldOk(QStringLiteral("salePrice"), QString());
    else
        m_view->setFieldError(QStringLiteral("salePrice"));
}

void PresenterSaleEdit::onFeeEdited(const QString& fieldKey, double value)
{
    if (value < 0.0)
        m_view->setFieldError(fieldKey);
    else
        m_view->setFieldOk(fieldKey, QString());
}

void PresenterSaleEdit::onTaxEdited(const QString& fieldKey, double value)
{
    if (value < 0.0)
        m_view->setFieldError(fieldKey);
    else
        m_view->setFieldOk(fieldKey, QString());
}

void PresenterSaleEdit::onDocumentPathEdited()
{
    const QString path = m_view->documentPath().trimmed();
    if (path.isEmpty()) {
        m_view->setFieldOk(QStringLiteral("document"), QString());
        return;
    }
    if (m_model->documentExists(path, m_currentSaleGuid))
        m_view->setFieldError(QStringLiteral("document"));
    else
        m_view->setFieldOk(QStringLiteral("document"), QString());
}

// ── onDocumentSelected ────────────────────────────────────────────────────────

void PresenterSaleEdit::onDocumentSelected(const QString& path)
{
    if (path.isEmpty()) return;

    m_pendingPdfPath = path;
    // Bugfix 21.08.2026: write the path into the view's document field here,
    // same as ViewSaleEdit::onBrowseDocument() already did — this call is the
    // ONLY path taken when a document is dropped onto "Direkte
    // Dokumentenerfassung" (MainWindow::openCaptureDialog() calls
    // dlg.presenter()->onDocumentSelected() directly, bypassing
    // onBrowseDocument() entirely). Without it the field stayed on "Kein
    // Dokument ausgewählt …" even though parsing succeeded. See
    // ARCHITECTURE.md.
    m_view->setDocumentPath(path);
    m_view->openPdfPreview(path);
    onDocumentPathEdited();

    // Non-latest sale: only document path changes, no re-parse
    const bool isNonLatestEdit = !m_currentSaleGuid.isEmpty() && !m_isLastSale;
    if (isNonLatestEdit) return;

    m_pdfExtractor.extract(path);
}

// ── onPdfTextExtracted ────────────────────────────────────────────────────────
// Replaces the former onPdfConversionFinished(int, int) QProcess slot —
// PdfTextExtractor now owns the pdftotext invocation (see ARCHITECTURE.md).

void PresenterSaleEdit::onPdfTextExtracted(bool success, const QString& text)
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
// DocumentType::Sale when the depot matched but no identifier did.

void PresenterSaleEdit::startParserForText(const QString& pdfText)
{
    if (!m_config || !m_config->isValid()) return;

    int depotIndex = -1;
    if (!DocumentClassifier::matchDepotIndex(pdfText, *m_config, depotIndex)) {
        const QStringList required = {
            "date","depotNumber","orderNumber","volume","salePrice"
        };
        for (const auto& f : required) m_view->setFieldError(f);
        m_view->onParseFinished();
        return;
    }

    const DepotEntry matchedDepot = m_config->entries().at(depotIndex);
    const DocumentType docType = DocumentClassifier::detectDocumentType(
        pdfText, matchedDepot, DocumentType::Sale);

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

void PresenterSaleEdit::onParserUpdated(const ParserLib::ParserInfoState& state)
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

void PresenterSaleEdit::populateFromResult(
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
                    .arg(parsedWkn.isEmpty() ? QObject::tr("(nicht gefunden)") : parsedWkn)
                    .arg(parsedIsin.isEmpty() ? QObject::tr("(nicht gefunden)") : parsedIsin)
                    .arg(shareWkn));
            });
            m_view->onParseFinished();
            return;
        }
    }

    static const QStringList knownXmlNames = {
        "Date","Time","DepotNumber","OrderNumber",
        "Volume","Price",
        "TaxAtSource","CapitalGainsTax","SolidarityTax",
        "Provision","BrokerFee","TraderPlaceFee","Reduction"
    };
    static const QStringList requiredXmlNames = {
        "Date","DepotNumber","OrderNumber","Volume","Price"
    };

    int found = 0, requiredFound = 0;
    for (const QString& xmlName : knownXmlNames) {
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
                    if (requiredXmlNames.contains(xmlName)) ++requiredFound;
                }
            }
        }
    }

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
            m_view->setParseProgress(100,
                QObject::tr("Analyse fehlgeschlagen — %1/%2 Pflicht")
                .arg(requiredFound).arg(reqTotal));
        }
    });
}

// ── xmlNameToViewField ────────────────────────────────────────────────────────

QString PresenterSaleEdit::xmlNameToViewField(const QString& xmlName)
{
    static const QMap<QString, QString> map = {
        { QStringLiteral("Date"),           QStringLiteral("date")           },
        { QStringLiteral("Time"),           QStringLiteral("time")           },
        { QStringLiteral("DepotNumber"),    QStringLiteral("depotNumber")    },
        { QStringLiteral("OrderNumber"),    QStringLiteral("orderNumber")    },
        { QStringLiteral("Volume"),         QStringLiteral("volume")         },
        { QStringLiteral("Price"),          QStringLiteral("salePrice")      },
        { QStringLiteral("TaxAtSource"),    QStringLiteral("taxAtSource")    },
        { QStringLiteral("CapitalGainsTax"),QStringLiteral("capitalGainsTax")},
        { QStringLiteral("SolidarityTax"),  QStringLiteral("solidarityTax")  },
        { QStringLiteral("Provision"),      QStringLiteral("provision")      },
        { QStringLiteral("BrokerFee"),      QStringLiteral("brokerFee")      },
        { QStringLiteral("TraderPlaceFee"), QStringLiteral("traderFee")      },
        { QStringLiteral("Reduction"),      QStringLiteral("reduction")      },
    };
    return map.value(xmlName, QString());
}

// ── reloadOverview ────────────────────────────────────────────────────────────

void PresenterSaleEdit::reloadOverview()
{
    m_sales = m_model->loadSales(m_shareGuid);
    // Splits als Parameter statt über einen eigenen Setter — sonst entstünde
    // eine unsichtbare Reihenfolge-Abhängigkeit zwischen zwei View-Aufrufen
    // (Phase 3c, 11.08.2026).
    m_view->populateOverview(m_sales, m_splits);
}

// ── refreshDerivedValues ──────────────────────────────────────────────────────

void PresenterSaleEdit::refreshDerivedValues()
{
    const double vol        = m_view->volume();
    const double price      = m_view->salePrice();
    const double prov       = m_view->provision();
    const double broker     = m_view->brokerFee();
    const double trader     = m_view->traderFee();
    const double red        = m_view->reduction();
    const double taxSrc     = m_view->taxAtSource();
    const double taxCap     = m_view->capitalGainsTax();
    const double taxSol     = m_view->solidarityTax();

    const double saleValue    = vol * price;
    const double gesGebuehren = prov + broker + trader;
    const double taxSum       = taxSrc + taxCap + taxSol;
    const double auszahlung   = saleValue - gesGebuehren + red - taxSum;

    // Kaufwert und G/V berechnen
    double kaufwert  = 0.0;
    double gewinnVerlust = 0.0;
    {
        if (!m_currentSaleGuid.isEmpty() && !m_isLastSale) {
            // Älterer, nicht editierbarer Verkauf: gespeicherte Werte aus
            // dem SaleObject anzeigen (Felder sind ohnehin gesperrt).
            for (const SaleObject& s : std::as_const(m_sales)) {
                if (s.guid() == m_currentSaleGuid) {
                    kaufwert     = s.buyValue();                      // Anzeige: ohne Kaufbrokerage
                    gewinnVerlust = s.profitLossBrokerageReduction(); // korrekte G/V inkl. aller Gebühren
                    break;
                }
            }
        } else {
            // Neuer Verkauf ODER Bearbeitung des jüngsten Verkaufs: live
            // FIFO-Vorschau, split-bewusst (Phase 2c, 07.08.2026, siehe
            // SaleFifoAllocator.h). Muss mit onSave() übereinstimmen, sonst
            // zeigt die Vorschau während der Eingabe einen anderen Wert als
            // das, was beim Speichern tatsächlich berechnet wird.
            const QDate saleDate = QDateTime::fromString(m_view->dateTime(), Qt::ISODate).date();

            // 09.08.2026: nutzt den im Konstruktor gefüllten Zwischenspeicher
            // statt eines erneuten loadSplits(). refreshDerivedValues() läuft
            // bei jeder Eingabe, die Splits einer Aktie ändern sich während
            // einer Dialog-Sitzung aber nicht — der Abruf war eine
            // Datenbankabfrage je Tastendruck.
            const QList<ShareSplitObject>& splits = m_splits;
            const QList<BuyObject> available = m_currentSaleGuid.isEmpty()
                ? m_model->loadAvailableBuysForDepot(m_shareGuid, m_view->depotNumber())
                : m_model->loadAvailableBuysForDepotExcludingSale(
                      m_shareGuid, m_view->depotNumber(), m_currentSaleGuid);

            double buyFeesTotal = 0.0;
            double buyRedTotal  = 0.0;
            for (const FifoAllocationRow& row :
                 SaleFifoAllocator::allocate(vol, saleDate, available, splits)) {
                kaufwert += row.volume * row.buyPrice;

                double buyFees = 0.0;
                double buyRed  = 0.0;
                proportionalBuyCosts(row.buyGuid, row.volume, available, buyFees, buyRed);
                buyFeesTotal += buyFees;
                buyRedTotal  += buyRed;
            }
            // G/V-Vorschau inkl. anteiliger Kaufkosten — identisch zu
            // SaleObject::profitLossBrokerageReduction(), das nach dem
            // Speichern angezeigt wird:
            //   (saleValue - Verkaufsgebühren + Rabatt)
            // - (Kaufwert + Kaufbrokerage - Kaufrabatt) - Steuern
            //
            // Die Kaufbrokerage war hier bis zu diesem Bugfix nicht
            // verfügbar; die Vorschau wies den Gewinn dadurch um die
            // Kauf-Nebenkosten zu hoch aus (siehe ARCHITECTURE.md).
            // Der Anzeigewert "Gekaufter Kaufwert" bleibt bewusst OHNE
            // Brokerage — er entspricht SaleObject::buyValue().
            gewinnVerlust = saleValue - gesGebuehren + red
                          - kaufwert - buyFeesTotal + buyRedTotal
                          - taxSum;
        }
    }

    m_view->setSaleValue(saleValue);
    m_view->setKaufwert(kaufwert);
    m_view->setGewinnVerlust(gewinnVerlust);
    m_view->setGesGebuehren(gesGebuehren);
    m_view->setTaxSum(taxSum);
    m_view->setAuszahlung(auszahlung);

    refreshSplitHint();
}

// ── refreshSplitHint ──────────────────────────────────────────────────────────

void PresenterSaleEdit::refreshSplitHint()
{
    const QDate date = QDate::fromString(m_view->dateTime().left(10), Qt::ISODate);

    m_view->setSplitHint(
        ShareSplitHint::footerText(m_splits, date, m_view->volume(), m_view->salePrice()),
        ShareSplitHint::tooltipText(m_splits, date),
        ShareSplitHint::hasSplitAfter(m_splits, date));
}

// ── proportionalBuyCosts ──────────────────────────────────────────────────────

void PresenterSaleEdit::proportionalBuyCosts(const QString&          buyGuid,
                                             double                  detailVolume,
                                             const QList<BuyObject>& buys,
                                             double&                 fees,
                                             double&                 reduction) const
{
    fees      = 0.0;
    reduction = 0.0;

    double buyVolume = 0.0;
    for (const BuyObject& b : buys) {
        if (b.guid() == buyGuid) { buyVolume = b.volume(); break; }
    }
    if (buyVolume <= 0.0) return;   // Kauf nicht in der Liste oder Volumen 0

    // Bewusst KEINE Split-Umrechnung: detailVolume und buy.volume() liegen
    // in derselben Beleg-Skala, der Bruch ist skaleninvariant.
    const double fraction = detailVolume / buyVolume;

    const BrokerageObject brk = m_model->loadBrokerageForBuy(buyGuid);
    fees      = brk.brokerage() * fraction;
    reduction = brk.reduction() * fraction;
}

// ── buildBuyDetailSummary ─────────────────────────────────────────────────────

SaleBuyDetailSummary PresenterSaleEdit::buildBuyDetailSummary() const
{
    SaleBuyDetailSummary summary;

    // Ein bereits gespeicherter Verkauf ist geladen? Steuert nur die
    // Kopfzeile des Dialogs (vormals m_loadedSale.isValid() in der View).
    const bool isEditMode = !m_currentSaleGuid.isEmpty();
    summary.editMode = isEditMode;

    // Der jüngste Verkauf bleibt bis zum Speichern voll editierbar — die
    // Details-Vorschau muss deshalb live neu rechnen, sonst weicht sie vom
    // tatsächlichen Ergebnis von onSave() ab (Aktiensplit-Behandlung,
    // Phase 2c, 07.08.2026). Nur ältere, nicht editierbare Verkäufe zeigen
    // die gespeicherte Zuteilung.
    const bool useLiveFifo = !isEditMode || m_isLastSale;

    // Anzeige durchgängig auf heutiger (split-bereinigter) Skala — diese
    // Ansicht ist eine berechnete Übersicht über ggf. mehrere Lots, keine
    // Beleg-Abschrift, und nur so bleiben Summen über mehrere Lots hinweg
    // sinnvoll, auch wenn ein Split zwischen zwei Lots liegt.
    if (!useLiveFifo) {
        // Alle Käufe inkl. vollständig verbrauchter — ein Kauf, den dieser
        // Verkauf komplett aufgebraucht hat, fehlt in der "verfügbar"-Liste.
        const QList<BuyObject> allBuys = m_model->loadAllBuys(m_shareGuid);

        SaleObject loaded;
        for (const SaleObject& s : m_sales) {
            if (s.guid() == m_currentSaleGuid) { loaded = s; break; }
        }

        const QDate  saleDate       = QDateTime::fromString(loaded.dateTime(), Qt::ISODate).date();
        const double todaySalePrice = ShareSplitAdjuster::adjustedTransactionPrice(
            loaded.salePrice(), m_splits, saleDate);

        for (const SaleBuyDetail& d : loaded.saleBuyDetails()) {
            const QDate  buyDate      = QDateTime::fromString(d.dateTime(), Qt::ISODate).date();
            const double todayVolume  = ShareSplitAdjuster::adjustedVolume(
                d.volume(), m_splits, buyDate);
            const double todayBuyPrice = ShareSplitAdjuster::adjustedTransactionPrice(
                d.buyPrice(), m_splits, buyDate);

            SaleBuyDetailRow r;
            r.date      = QLocale().toString(buyDate, QLocale::ShortFormat);
            r.volume    = todayVolume;
            r.buyPrice  = todayBuyPrice;
            r.fees      = d.brokeragePart();   // Geldbetrag, unskaliert
            r.reduction = d.reductionPart();   // Geldbetrag, unskaliert
            r.buyValue  = todayVolume * todayBuyPrice;
            r.saleValue = todayVolume * todaySalePrice;
            r.profitLoss = r.saleValue - r.buyValue;
            for (const BuyObject& b : allBuys) {
                if (b.guid() == d.buyGuid()) { r.document = b.document(); break; }
            }
            summary.rows.append(r);
        }

        summary.saleFees = loaded.brokerage() + loaded.taxSum();
    } else {
        const QDate  saleDate       = QDateTime::fromString(m_view->dateTime(), Qt::ISODate).date();
        const double todaySalePrice = ShareSplitAdjuster::adjustedTransactionPrice(
            m_view->salePrice(), m_splits, saleDate);

        const QList<BuyObject> available = m_currentSaleGuid.isEmpty()
            ? m_model->loadAvailableBuysForDepot(m_shareGuid, m_view->depotNumber())
            : m_model->loadAvailableBuysForDepotExcludingSale(
                  m_shareGuid, m_view->depotNumber(), m_currentSaleGuid);

        for (const FifoAllocationRow& row :
             SaleFifoAllocator::allocate(m_view->volume(), saleDate, available, m_splits)) {
            const BuyObject* matchedBuy = nullptr;
            for (const BuyObject& b : available) {
                if (b.guid() == row.buyGuid) { matchedBuy = &b; break; }
            }
            const QDate  buyDate      = matchedBuy ? matchedBuy->date() : saleDate;
            const double todayVolume  = ShareSplitAdjuster::adjustedVolume(
                row.volume, m_splits, buyDate);
            const double todayBuyPrice = ShareSplitAdjuster::adjustedTransactionPrice(
                row.buyPrice, m_splits, buyDate);

            // Bis zu diesem Bugfix standen hier hart 0.0 — die Spalte
            // "Kosten" blieb dadurch im Live-Zweig immer leer, und die
            // ausgewiesene G/V war um die Kauf-Nebenkosten zu hoch.
            double buyFees = 0.0;
            double buyRed  = 0.0;
            proportionalBuyCosts(row.buyGuid, row.volume, available, buyFees, buyRed);

            SaleBuyDetailRow r;
            r.date      = matchedBuy ? matchedBuy->dateAsStr() : QString();
            r.volume    = todayVolume;
            r.buyPrice  = todayBuyPrice;
            r.fees      = buyFees;
            r.reduction = buyRed;
            r.buyValue  = todayVolume * todayBuyPrice;
            r.saleValue = todayVolume * todaySalePrice;
            r.profitLoss = r.saleValue - r.buyValue;
            r.document  = matchedBuy ? matchedBuy->document() : QString();
            summary.rows.append(r);
        }

        // Verkaufsgebühren und Steuern des Verkaufs selbst kommen aus dem
        // Formular — beim Bearbeiten des jüngsten Verkaufs sind die Felder
        // editierbar, gespeicherte Werte wären dort veraltet.
        summary.saleFees = m_view->provision() + m_view->brokerFee() + m_view->traderFee()
                         + m_view->taxAtSource() + m_view->capitalGainsTax()
                         + m_view->solidarityTax();
    }

    for (const SaleBuyDetailRow& r : std::as_const(summary.rows)) {
        summary.totalVolume    += r.volume;
        summary.totalFees      += r.fees;
        summary.totalReduction += r.reduction;
        summary.totalBuyValue  += r.buyValue;
        summary.totalSaleValue += r.saleValue;
    }

    // Ges. Kauf inkl. Kosten: Kaufsumme + Brokerage - Rabatt. Der Rabatt
    // wurde in der Summenzeile bisher übergangen, obwohl die Spalte
    // "Gesamt" je Zeile bereits `buyValue + fees - reduction` zeigt —
    // solange reductionPart überall 0 war, fiel das nicht auf.
    // Entspricht jetzt SaleObject::buyValueBrokerageReduction().
    const double totBuyValWithFees =
        summary.totalBuyValue + summary.totalFees - summary.totalReduction;

    summary.totalProfitLoss = summary.totalSaleValue - totBuyValWithFees - summary.saleFees;

    return summary;
}

// ── onShowDetails ─────────────────────────────────────────────────────────────

void PresenterSaleEdit::onShowDetails()
{
    m_view->showBuyDetails(buildBuyDetailSummary());
}

// ── validateInput ─────────────────────────────────────────────────────────────

QString PresenterSaleEdit::validateInput() const
{
    QStringList missingFields;
    if (m_view->hasMissingRequiredFields(missingFields)) {
        m_view->markMissingFieldsAsFailed();
        return QObject::tr(
            "Es fehlen noch Pflichtangaben.\n"
            "Die fehlenden Felder sind in der Maske rot markiert.");
    }

    // Bugfix (siehe ARCHITECTURE.md, "Skalenbewusste Mengenprüfung im
    // Verkaufsformular", 11.08.2026): SaleFifoAllocator::allocate() deckelte
    // eine zu hohe Verkaufsmenge bislang still auf das verfügbare Volumen,
    // statt das Speichern abzulehnen. Die Prüfung erfolgt hier zusätzlich
    // zum Live-Icon in onVolumeOrPriceEdited() — jenes läuft nur bei
    // editingFinished und kann z. B. durch eine geänderte Depot-Auswahl
    // danach wieder veralten.
    if (!isRequestedVolumeCovered()) {
        const QDate saleDate = QDateTime::fromString(m_view->dateTime(), Qt::ISODate).date();
        const QList<BuyObject> available = currentAvailableBuys();
        const double saleToday  = ShareSplitAdjuster::adjustedVolume(
            m_view->volume(), m_splits, saleDate);
        const double availToday = SaleFifoAllocator::totalAvailableVolumeToday(
            available, m_splits);
        QString message = QObject::tr(
            "Die Verkaufsmenge (%1 Stk., heutige Skala) übersteigt die im "
            "gewählten Depot verfügbare Menge (%2 Stk.).")
            .arg(QLocale().toString(saleToday, 'f', 4),
                 QLocale().toString(availToday, 'f', 4));

        // Liegt ein Split dazwischen, ist er die wahrscheinlichere Ursache
        // als eine falsche Verkaufsmenge — siehe splitRatioHint().
        const QString hint = splitRatioHint(saleToday, saleDate, available);
        if (!hint.isEmpty())
            message += QStringLiteral("\n\n") + hint;

        return message;
    }

    if (m_model->orderNumberExists(m_shareGuid,
                                   m_view->orderNumber().trimmed(),
                                   m_currentSaleGuid)) {
        return QObject::tr("Die Auftragsnummer \"%1\" ist für diese Aktie bereits vorhanden.")
               .arg(m_view->orderNumber().trimmed());
    }

    const QString doc = m_view->documentPath().trimmed();
    if (!doc.isEmpty() && m_model->documentExists(doc, m_currentSaleGuid)) {
        return QObject::tr("Das Dokument \"%1\" ist bereits einem anderen Verkauf zugeordnet.")
               .arg(doc);
    }

    return QString();
}

// ── currentAvailableBuys ──────────────────────────────────────────────────────

QList<BuyObject> PresenterSaleEdit::currentAvailableBuys() const
{
    return m_currentSaleGuid.isEmpty()
        ? m_model->loadAvailableBuysForDepot(m_shareGuid, m_view->depotNumber())
        : m_model->loadAvailableBuysForDepotExcludingSale(
              m_shareGuid, m_view->depotNumber(), m_currentSaleGuid);
}

// ── splitRatioHint ────────────────────────────────────────────────────────────

QString PresenterSaleEdit::splitRatioHint(double                  requiredVolumeToday,
                                          const QDate&            saleDate,
                                          const QList<BuyObject>& availableBuys) const
{
    const SplitRatioSuspicion suspicion = SplitRatioChecker::diagnose(
        requiredVolumeToday, saleDate, availableBuys, m_splits);

    if (!suspicion.hasSuspicion)
        return QString();

    // Formatierung über ShareSplitHint, nicht über eine eigene Kopie: die
    // Schreibweise "20:1 am 18.07.2022" steht bereits in den Fusszeilen der
    // Editier-Dialoge und in den Tooltips der Übersichtstabellen. Zwei
    // Kopien derselben Formatierung driften auseinander (dieselbe Begründung
    // wie bei der Auslagerung von ShareSplitHint selbst).
    if (!suspicion.hasProposal) {
        if (suspicion.splitsBetween.size() == 1) {
            return QObject::tr(
                "Zwischen Kauf und Verkauf liegt der Split %1. Falls die "
                "Verkaufsmenge stimmt, bitte das Verhältnis im Dialog "
                "\"Aktiensplits\" gegen die Bankmitteilung prüfen.")
                .arg(ShareSplitHint::describeSplit(suspicion.splitsBetween.first()));
        }
        return QObject::tr(
            "Zwischen Kauf und Verkauf liegen %1 Splits (zuletzt %2). Falls "
            "die Verkaufsmenge stimmt, bitte die Verhältnisse im Dialog "
            "\"Aktiensplits\" gegen die Bankmitteilungen prüfen.")
            .arg(suspicion.splitsBetween.size())
            .arg(ShareSplitHint::describeSplit(suspicion.splitsBetween.constLast()));
    }

    const QString proposed =
        ShareSplitHint::formatRatioPart(suspicion.proposedRatioNew)
        + QStringLiteral(":")
        + ShareSplitHint::formatRatioPart(suspicion.proposedRatioOld);

    // Bank-Schreibweise: alte Seite zu ZUSÄTZLICHEN Stücken, im Beispiel
    // "1:19" für das Umrechnungsverhältnis 20:1.
    const QString bankNotation =
        ShareSplitHint::formatRatioPart(suspicion.proposedRatioOld)
        + QStringLiteral(":")
        + ShareSplitHint::formatRatioPart(suspicion.proposedRatioNew
                                          - suspicion.proposedRatioOld);

    return QObject::tr(
        "Zwischen Kauf und Verkauf liegt der Split %1. Mit dem Verhältnis %2 "
        "ergäben sich genau %3 Stk., die Verkaufsmenge ginge dann exakt auf.\n"
        "Bankmitteilungen nennen das Zuteilungsverhältnis häufig als \"%4\" — "
        "das sind die ZUSÄTZLICHEN Stücke je gehaltenem Stück, nicht das von "
        "der Anwendung erwartete Umrechnungsverhältnis. Bitte das Verhältnis "
        "im Dialog \"Aktiensplits\" gegen die Bankmitteilung prüfen.")
        .arg(ShareSplitHint::describeSplit(suspicion.splitsBetween.first()),
             proposed,
             QLocale().toString(suspicion.proposedAvailableToday, 'f', 4),
             bankNotation);
}

// ── isRequestedVolumeCovered ──────────────────────────────────────────────────

bool PresenterSaleEdit::isRequestedVolumeCovered() const
{
    // Ein älterer, nicht-jüngster Verkauf ist bis auf das Dokument gesperrt
    // (ViewSaleEdit::setButtonStates(), readOnlyMode) — onSave() speichert
    // dort ausschließlich s.setDocument(), die Menge bleibt unverändert.
    // Die Prüfung wäre hier auch inhaltlich nicht belastbar:
    // loadAvailableBuysForDepotExcludingSale() bucht nur DIESEN einen
    // Verkauf zurück, nicht die FIFO-Historie zwischen ihm und heute.
    if (!m_currentSaleGuid.isEmpty() && !m_isLastSale)
        return true;

    const QDate saleDate = QDateTime::fromString(m_view->dateTime(), Qt::ISODate).date();
    return SaleFifoAllocator::isSaleVolumeCovered(
        m_view->volume(), saleDate, currentAvailableBuys(), m_splits);
}

// ── isLatestSale ──────────────────────────────────────────────────────────────

bool PresenterSaleEdit::isLatestSale(const QString& saleGuid) const
{
    if (m_sales.isEmpty()) return false;

    QString latestGuid;
    QString latestDt;
    for (const SaleObject& s : m_sales) {
        if (s.dateTime() > latestDt) {
            latestDt   = s.dateTime();
            latestGuid = s.guid();
        }
    }
    return latestGuid == saleGuid;
}

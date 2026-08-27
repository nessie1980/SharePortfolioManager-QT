// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IViewDividendEdit.h"
#include "IModelDividendEdit.h"
#include "../../libs/parser/src/Parser.h"
#include "../../config/DocumentsConfig.h"
#include "../../utils/PdfTextExtractor.h"

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include <QDate>

/**
 * @brief Presenter for the "Dividenden hinzufügen / editieren" dialog (MVP pattern).
 *
 * Parse pipeline is identical to PresenterBuyEdit:
 * 1. onDocumentSelected() → PdfTextExtractor → onPdfTextExtracted()
 * 2. startParserForText() → DocumentClassifier::matchDepotIndex()/
 *    detectDocumentType() → ParserLib
 * 3. onParserUpdated() → populateFromResult() → setFieldOk/Error + onParseFinished
 *
 * Dividend-specific logic:
 * - Any dividend may be edited or deleted at any time — no latest-entry restriction.
 * - Derived values: dividendPayout, dividendPayoutFc, taxSum,
 *                   dividendPayoutWithTaxes, yield.
 * - Foreign currency mode: enableForeignCurrency checkbox toggles extra fields.
 *
 * @note PDF-to-text conversion and depot-/document-type detection now
 * delegate to the shared `PdfTextExtractor`/`DocumentClassifier` utility
 * classes (see ARCHITECTURE.md, "PDF-Erkennungslogik gebündelt in
 * DocumentClassifier"). Behaviour unchanged, including the fallback to
 * `DocumentType::Dividend` when the depot matched but no explicit
 * identifier did.
 */
class PresenterDividendEdit : public QObject
{
    Q_OBJECT

public:
    explicit PresenterDividendEdit(IViewDividendEdit* view,
                                   IModelDividendEdit* model,
                                   const QString&      shareGuid,
                                   DocumentsConfig*    config,
                                   QObject*            parent = nullptr);

public slots:
    void onSave();
    void onRemove();
    void onReset();
    void onClose();
    void onRowSelected(const QString& dividendGuid);
    void onValuesChanged();
    void onDocumentSelected(const QString& path);
    void onForeignCurrencyToggled(bool enabled);

    // ── Live field validation ─────────────────────────────────────────────
    void onDateEdited();
    void onExDateEdited();
    void onDepotNumberEdited();
    void onRateEdited();
    void onVolumeEdited();
    void onPriceAtPaydayEdited();
    void onExchangeRatioEdited();
    void onTaxEdited(const QString& fieldKey, double value);
    void onDocumentPathEdited();

    /**
     * @brief Verteilt das Parser-Ergebnis auf die View-Felder.
     *
     * @param result  searchResult-Map aus ParserInfoState.
     *
     * Seit 27.08.2026 public statt private — sonst waere die geaenderte
     * Zaehlung (nur UEBERNOMMENE Werte zaehlen, nicht gefangene) von keinem
     * Test erreichbar: die Methode ist kein Slot, QMetaObject::invokeMethod
     * kaeme also nicht heran, und der Weg ueber onDocumentSelected() braucht
     * ein echtes PDF samt pdftotext. Gleiche Ueberlegung wie bei den
     * statischen Helfern, die aus diesem Grund public sind (siehe
     * TESTING.md, "Sichtbarkeit zugunsten der Testbarkeit").
     */
    void populateFromResult(const QMap<QString, QList<QString>>& result);


signals:
    void dataChanged();

private slots:
    /** Called when PdfTextExtractor finishes converting the selected PDF. */
    void onPdfTextExtracted(bool success, const QString& text);
    void onParserUpdated(const ParserLib::ParserInfoState& state);

private:
    void    reloadOverview();
    void    refreshDerivedValues();
    QString validateInput() const;

    /**
     * @brief Look up the closing price for @p date via IModelDividendEdit and,
     *        on a hit, overwrite "priceAtPayday" with it. No-op on a miss.
     *        Shared by onDateEdited() (manual date edit) and
     *        populateFromResult() (date parsed from a PDF document) so both
     *        paths use the actually-current payout date rather than a
     *        possibly stale one (see ARCHITECTURE.md, DividendForm-Details).
     */
    void applyDailyValuePriceAtPayday(const QDate& date);

    void startParserForText(const QString& pdfText);
    static QString xmlNameToViewField(const QString& xmlName);

    IViewDividendEdit*  m_view;
    IModelDividendEdit* m_model;
    DocumentsConfig*    m_config    = nullptr;
    QString             m_shareGuid;

    QList<DividendObject> m_dividends;

    /// Splits der Aktie, einmalig im Konstruktor geladen (Phase 3c,
    /// 11.08.2026). Die Splits einer Aktie ändern sich während einer
    /// Dialog-Sitzung nicht — ein Abruf je reloadOverview() wäre eine
    /// unnötige Datenbankabfrage (gleiche Überlegung wie in
    /// PresenterSaleEdit::refreshDerivedValues(), 09.08.2026).
    QList<ShareSplitObject> m_splits;

    /// Käufe und Verkäufe der Aktie über alle Depots, einmalig im Konstruktor
    /// geladen (Phase 3 der Ex-Tag-Behandlung, 21.08.2026). Datengrundlage der
    /// Stückzahl-Plausibilitätsprüfung in validateInput(). Aus demselben Grund
    /// zwischengespeichert wie m_splits: Käufe und Verkäufe sind aus diesem
    /// Dialog heraus nicht erreichbar und ändern sich während einer
    /// Dialog-Sitzung nicht — ein Abruf je Speichern wären zwei unnötige
    /// Datenbankabfragen.
    QList<BuyObject>      m_buys;
    QList<SaleObject>     m_sales;

    QString               m_currentDividendGuid;

    ParserLib::Parser m_parser;
    PdfTextExtractor  m_pdfExtractor;
    QString           m_pendingPdfPath;
    QString           m_pdfText;
};

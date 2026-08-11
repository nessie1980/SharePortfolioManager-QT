// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IViewSaleEdit.h"
#include "IModelSaleEdit.h"
#include "../../config/DocumentsConfig.h"
#include "../../utils/PdfTextExtractor.h"
#include "../../utils/SaleFifoAllocator.h"
#include "../../utils/ShareSplitAdjuster.h"
#include "../../libs/parser/src/Parser.h"

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>

/**
 * @brief Presenter for the "Verkäufe hinzufügen / editieren" dialog (MVP pattern).
 *
 * Parse pipeline is identical to PresenterBuyEdit:
 * 1. onDocumentSelected() → PdfTextExtractor → onPdfTextExtracted()
 * 2. startParserForText() → DocumentClassifier::matchBankIndex()/
 *    detectDocumentType() → ParserLib
 * 3. onParserUpdated() → populateFromResult() → setFieldOk/Error + onParseFinished
 *
 * Sale-specific logic:
 * - Latest-sale detection via lexicographic ISO 8601 string comparison.
 * - canRemove = isLastSale (no sold-volume guard needed for sales).
 * - Derived values: saleValue, kaufwert, gewinnVerlust, gesGebühren, taxSum, auszahlung.
 *
 * @note PDF-to-text conversion and bank-/document-type detection now
 * delegate to the shared `PdfTextExtractor`/`DocumentClassifier` utility
 * classes (see ARCHITECTURE.md, "PDF-Erkennungslogik gebündelt in
 * DocumentClassifier"). Behaviour unchanged, including the fallback to
 * `DocumentType::Sale` when the bank matched but no explicit identifier did.
 */
class PresenterSaleEdit : public QObject
{
    Q_OBJECT

public:
    explicit PresenterSaleEdit(IViewSaleEdit*   view,
                               IModelSaleEdit*  model,
                               const QString&   shareGuid,
                               DocumentsConfig* config,
                               QObject*         parent = nullptr);

public slots:
    void onSave();
    void onRemove();
    void onReset();
    void onClose();
    void onRowSelected(const QString& saleGuid);
    void onValuesChanged();
    void onDocumentSelected(const QString& path);

    /**
     * @brief Baut den Inhalt des Details-Dialogs und übergibt ihn an die View.
     *
     * Am Details-Button verdrahtet. Die Berechnung lag bis zum Bugfix
     * "anteilige Kauf-Nebenkosten gehen bei der FIFO-Zuteilung verloren"
     * in `ViewSaleEdit::onShowDetails()`, konnte dort aber die anteilige
     * Kauf-Brokerage nicht ermitteln (kein Modellzugriff aus der View).
     */
    void onShowDetails();

    // ── Live field validation ─────────────────────────────────────────────
    void onDateEdited();
    void onDepotNumberEdited();
    void onOrderNumberEdited();
    void onVolumeOrPriceEdited();
    void onFeeEdited(const QString& fieldKey, double value);
    void onTaxEdited(const QString& fieldKey, double value);
    void onDocumentPathEdited();

signals:
    void dataChanged();

private slots:
    /** Called when PdfTextExtractor finishes converting the selected PDF. */
    void onPdfTextExtracted(bool success, const QString& text);
    void onParserUpdated(const ParserLib::ParserInfoState& state);

private:
    void    reloadOverview();
    void    refreshDerivedValues();

    /**
     * @brief Aktualisiert den Split-Hinweis unter den Verkaufsdaten (Phase 3b).
     *
     * Aufgerufen aus refreshDerivedValues() und onDateEdited() — der Hinweis
     * hängt an Datum, Stückzahl und Preis und läuft live mit.
     */
    void    refreshSplitHint();
    QString validateInput() const;
    bool    isLatestSale(const QString& saleGuid) const;

    /**
     * @brief Anteilige Kauf-Nebenkosten einer FIFO-Zuteilungszeile.
     *
     * Bugfix (siehe ARCHITECTURE.md): beim Umbau auf `SaleFifoAllocator`
     * fielen `brokeragePart`/`reductionPart` aus der Erzeugung der
     * `SaleBuyDetail`-Objekte heraus — der Konstruktor hat für beide
     * Defaultwerte 0.0, deshalb blieb der Verlust ohne Compilerfehler.
     * Seither wurden die Kauf-Nebenkosten nicht mehr in der Gewinnermittlung
     * berücksichtigt.
     *
     * Verteilt wird nach dem Bruchteil des verbrauchten Kaufs — dasselbe
     * Pro-Lot-FIFO-Modell, das `ShareCalculator` für gehaltene Anteile
     * verwendet. @p detailVolume und `buy.volume()` liegen beide in der
     * Beleg-Skala DESSELBEN Kaufs, der Bruch ist damit skaleninvariant:
     * ein Split zwischen Kauf und Verkauf verändert ihn nicht, und es darf
     * hier ausdrücklich NICHT über `ShareSplitAdjuster` gerechnet werden.
     *
     * Es wird bewusst nicht gerundet — nur so trifft die Summe der Teile
     * den Gesamtbetrag des Kaufs exakt, wenn mehrere Verkäufe denselben
     * Kauf verbrauchen.
     *
     * @param buyGuid       GUID des referenzierten Kaufs.
     * @param detailVolume  Zugeteilte Menge, Beleg-Skala des Kaufs.
     * @param buys          Liste, in der der Kauf gesucht wird.
     * @param fees          [out] Anteilige Brokerage.
     * @param reduction     [out] Anteiliger Rabatt.
     */
    void proportionalBuyCosts(const QString&          buyGuid,
                              double                  detailVolume,
                              const QList<BuyObject>& buys,
                              double&                 fees,
                              double&                 reduction) const;

    /** Baut Zeilen und Summen für den Details-Dialog auf heutiger Skala. */
    SaleBuyDetailSummary buildBuyDetailSummary() const;

    void startParserForText(const QString& pdfText);
    void populateFromResult(const QMap<QString, QList<QString>>& result);
    static QString xmlNameToViewField(const QString& xmlName);

    IViewSaleEdit*   m_view;
    IModelSaleEdit*  m_model;
    DocumentsConfig* m_config = nullptr;
    QString          m_shareGuid;

    QList<SaleObject> m_sales;

    /// Splits der Aktie, einmalig im Konstruktor geladen — identisch zu dem,
    /// was bereits an IViewSaleEdit::setSplits() geht (Phase 2c).
    QList<ShareSplitObject> m_splits;

    QString           m_currentSaleGuid;
    bool              m_isLastSale = false;

    ParserLib::Parser m_parser;
    PdfTextExtractor  m_pdfExtractor;
    QString           m_pendingPdfPath;
    QString           m_pdfText;
};

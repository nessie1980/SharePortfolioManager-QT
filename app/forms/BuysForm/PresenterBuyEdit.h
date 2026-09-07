// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IViewBuyEdit.h"
#include "IModelBuyEdit.h"
#include "../../config/DocumentsConfig.h"
#include "../../utils/PdfTextExtractor.h"
#include "../../models/ShareSplitObject.h"
#include "../../libs/parser/src/Parser.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>

/**
 * @brief Presenter for the "Käufe hinzufügen / editieren" dialog (MVP pattern).
 *
 * Parse pipeline is identical to PresenterShareAdd:
 * 1. onDocumentSelected() → PdfTextExtractor → onPdfTextExtracted()
 * 2. startParserForText() → DocumentClassifier::matchDepotIndex()/
 *    detectDocumentType() → ParserLib
 * 3. onParserUpdated() → populateFromResult() → setFieldOk/Error + onParseFinished
 *
 * @note PDF-to-text conversion and depot-/document-type detection used to be
 * duplicated inline here (see ARCHITECTURE.md, "PDF-Erkennungslogik
 * gebündelt in DocumentClassifier" / "PDF-Text-Extraktion gebündelt in
 * PdfTextExtractor") — both now delegate to the shared `PdfTextExtractor`
 * and `DocumentClassifier` utility classes in `app/utils/`. Behaviour is
 * unchanged: `startParserForText()` still falls back to
 * `DocumentType::Buy` when the depot matched but no explicit
 * Buy-/Sale-/Dividend-/BrokerageIdentifier does, exactly as before —
 * `DocumentClassifier::detectDocumentType()` takes that fallback as a
 * parameter for precisely this reason.
 */
class PresenterBuyEdit : public QObject
{
    Q_OBJECT

public:
    explicit PresenterBuyEdit(IViewBuyEdit*    view,
                              IModelBuyEdit*   model,
                              const QString&   shareGuid,
                              DocumentsConfig* config,
                              QObject*         parent = nullptr);

public slots:
    void onSave();
    void onRemove();
    void onReset();
    void onClose();
    void onRowSelected(const QString& buyGuid);
    void onValuesChanged();
    void onDocumentSelected(const QString& path);

    // ── Live field validation ─────────────────────────────────────────────
    /** Called when the user leaves the date field. */
    void onDateEdited();
    /** Called when depot number selection changes. */
    void onDepotNumberEdited();
    /** Called when the user leaves the order number field. */
    void onOrderNumberEdited();
    /** Called when the user leaves volume or price. */
    void onVolumeOrPriceEdited();
    /** Called when the user leaves any optional fee field. */
    void onFeeEdited(const QString& fieldKey, double value);
    /** Called when the user selects a new document path. */
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

    // ── Feldnamen-Tabellen (public seit 02.09.2026) ───────────────────────
    //
    // Handgepflegte Uebersetzung zwischen Documents.xml und dieser Maske.
    // Public, damit tst_buysform sie gegeneinander und gegen die tatsaechlich
    // registrierten Felder der View pruefen kann — siehe TESTING.md,
    // "Sichtbarkeit zugunsten der Testbarkeit", und ARCHITECTURE.md,
    // "Feldschluessel-Tabellen sind an keiner Stelle geprueft".

    /** Alle Feldnamen aus Documents.xml, die dieses Formular verarbeitet. */
    static const QStringList& knownXmlNames();

    /** Teilmenge von knownXmlNames(), ohne die nicht gespeichert werden kann. */
    static const QStringList& requiredXmlNames();

    /**
     * @brief Map XML field name → view input widget key.
     *
     * Leerer Rueckgabewert heisst "dieses Formular kennt den Namen nicht".
     */
    static QString xmlNameToViewField(const QString& xmlName);


signals:
    void dataChanged();

private slots:
    /** Called when PdfTextExtractor finishes converting the selected PDF. */
    void onPdfTextExtracted(bool success, const QString& text);

    /** Called by ParserLib::Parser on every state change. */
    void onParserUpdated(const ParserLib::ParserInfoState& state);

private:
    void    reloadOverview();
    void    refreshDerivedValues();

    /**
     * @brief Aktualisiert den Split-Hinweis unter den Kaufdaten (Phase 3b).
     *
     * Wird aus refreshDerivedValues() (Stückzahl/Preis geändert) UND aus
     * onDateEdited() (Datum geändert) heraus aufgerufen — der Hinweis hängt
     * von allen dreien ab und soll live mitlaufen.
     */
    void    refreshSplitHint();
    QString validateInput() const;

    /**
     * @brief Markiert alle Felder rot, deren Inhalt sich nicht als Zahl lesen
     *        laesst, und meldet, ob es welche gab.
     *
     * Fragt IView*::hasUnreadableFields() ab. Die View sagt, WAS sie nicht
     * lesen konnte; was daraus folgt, entscheidet der Presenter — hier die
     * rote Markierung, in validateInput() zusaetzlich die Speichersperre.
     *
     * Wird aus den Live-Slots heraus aufgerufen, damit die Markierung schon
     * beim Verlassen des Feldes steht und nicht erst beim Speichern
     * (Nessies Vorgabe 06.09.2026). Ohne Meldungsfenster: ein modaler Dialog
     * bei jedem Feldwechsel waere unertraeglich, das rote Symbol ist die
     * sofortige Rueckmeldung.
     *
     * Ergaenzt 07.09.2026, siehe ARCHITECTURE.md, "Unlesbare Zahleneingaben
     * werden nicht gemeldet".
     */
    bool markUnreadableFields() const;

    /**
     * @brief true, wenn genau dieses Feld gerade unlesbaren Text enthaelt.
     *
     * Fuer die Live-Slots der OPTIONALEN Felder (Gebuehren, Steuern, Rabatt).
     * Dort ist 0,0 ein gueltiger Wert, ein unlesbarer Text ergab ueber
     * NumberParser aber ebenfalls 0,0 und bekam bisher den gruenen Haken —
     * der Eingabe wurde also ausdruecklich bescheinigt, in Ordnung zu sein,
     * waehrend ihr Inhalt verworfen wurde.
     *
     * Bewusst feldweise statt ueber markUnreadableFields(): beim Verlassen
     * eines Feldes soll nicht die halbe Maske rot werden, sondern das Feld,
     * das der Benutzer gerade bearbeitet hat.
     */
    bool isFieldUnreadable(const QString& fieldKey) const;



    /** Returns true if @p buyGuid has the most recent dateTime in m_buys. */
    bool    isLatestBuy(const QString& buyGuid) const;

    /** Start ParserLib after pdftext was extracted — identical to PresenterShareAdd. */
    void startParserForText(const QString& pdfText);

    IViewBuyEdit*    m_view;
    IModelBuyEdit*   m_model;
    DocumentsConfig* m_config = nullptr;
    QString          m_shareGuid;

    QList<BuyObject> m_buys;

    /// Splits der Aktie, einmalig im Konstruktor geladen — sie ändern sich
    /// während einer Dialog-Sitzung praktisch nie (gleiches Vorgehen wie
    /// IViewSaleEdit::setSplits() in Phase 2c).
    QList<ShareSplitObject> m_splits;

    QString          m_currentBuyGuid;
    bool             m_isLastBuy    = false;  ///< true when selected buy is most recent

    ParserLib::Parser m_parser;
    PdfTextExtractor  m_pdfExtractor;
    QString           m_pendingPdfPath;
    QString           m_pdfText;
};

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>

#include "../../app/utils/DocumentClassifier.h"
#include "../../app/config/DocumentsConfig.h"

/**
 * @brief Unit tests for DocumentClassifier.
 *
 * Pure logic tests — no GUI, no pdftotext, no database. Builds a small
 * Documents.xml fixture per test via QTemporaryDir, following the exact
 * same pattern as tests/config/tst_documentsconfig.cpp (see TESTING.md).
 */
class TestDocumentClassifier : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;

    QString writeXml(const QString& fileName, const QString& content)
    {
        const QString path = m_tempDir.path() + QLatin1Char('/') + fileName;
        QFile file(path);
        const bool opened = file.open(QIODevice::WriteOnly | QIODevice::Text);
        Q_ASSERT(opened);
        QTextStream(&file) << content;
        file.close();
        return path;
    }

    /**
     * @brief One bank ("TestBank") with Buy/Sale/Dividend/Brokerage
     * identifiers and a Buy document entry containing Wkn/Isin/Date rules.
     *
     * - BankIdentifier matches "Depotnummer 123456"
     * - BuyIdentifier matches "Wertpapier Abrechnung Kauf"
     * - SaleIdentifier matches "Wertpapier Abrechnung Verkauf"
     * - DividendIdentifier matches "Dividendengutschrift"
     * - BrokerageIdentifier matches "Kostenausweis"
     */
    QString validXml() const
    {
        return QStringLiteral(
            "<?xml version=\"1.0\"?><Documents>"
            "<Bank Name=\"TestBank\" BankIdentifierValue=\"123456\" Encoding=\"UTF-8\">"
            "<BankIdentifier Name=\"BankIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" "
                "RegexOptions=\"None\">Depotnummer\\s+(\\d+)</BankIdentifier>"
            "<BuyIdentifier Name=\"BuyIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" "
                "RegexOptions=\"None\">Wertpapier Abrechnung Kauf</BuyIdentifier>"
            "<SaleIdentifier Name=\"SaleIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" "
                "RegexOptions=\"None\">Wertpapier Abrechnung Verkauf</SaleIdentifier>"
            "<DividendIdentifier Name=\"DividendIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" "
                "RegexOptions=\"None\">Dividendengutschrift</DividendIdentifier>"
            "<BrokerageIdentifier Name=\"BrokerageIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" "
                "RegexOptions=\"None\">Kostenausweis</BrokerageIdentifier>"
            "<Document Type=\"Buy\" TypeIdentifierValue=\"Wertpapier Abrechnung Kauf\" Encoding=\"UTF-8\">"
            "<Wkn Name=\"Wkn\" FoundIndex=\"0\" ResultEmpty=\"true\" "
                "RegexOptions=\"None\">WKN:\\s+([A-Z0-9]{6})</Wkn>"
            "<Isin Name=\"Isin\" FoundIndex=\"0\" ResultEmpty=\"true\" "
                "RegexOptions=\"None\">ISIN:\\s+([A-Z0-9]{12})</Isin>"
            "<Date Name=\"Date\" FoundIndex=\"0\" ResultEmpty=\"false\" "
                "RegexOptions=\"None\">Datum:\\s+(\\d{2}.\\d{2}.\\d{4})</Date>"
            "</Document>"
            "<Document Type=\"Sale\" TypeIdentifierValue=\"Wertpapier Abrechnung Verkauf\" Encoding=\"UTF-8\">"
            "<Date Name=\"Date\" FoundIndex=\"0\" ResultEmpty=\"false\" "
                "RegexOptions=\"None\">Datum:\\s+(\\d{2}.\\d{2}.\\d{4})</Date>"
            "</Document>"
            "<Document Type=\"Dividend\" TypeIdentifierValue=\"Dividendengutschrift\" Encoding=\"UTF-8\">"
            "<Date Name=\"Date\" FoundIndex=\"0\" ResultEmpty=\"false\" "
                "RegexOptions=\"None\">Datum:\\s+(\\d{2}.\\d{2}.\\d{4})</Date>"
            "</Document>"
            "</Bank>"
            "</Documents>");
    }

    /**
     * @brief Wie validXml(), aber mit LEERER SaleIdentifier-Regel und ohne
     *        Sale-Dokumentblock — der Aufbau von "Cortal Consors" in der
     *        ausgelieferten Documents.xml.
     *
     * Nachbau des Feldfalls vom 21.08.2026 ("Consors-Dividenden werden
     * überhaupt nicht gelesen"). Für diese Bank sind keine Verkaufsbelege
     * konfiguriert; das Element bleibt leer stehen und der zugehörige
     * Dokumentblock fehlt.
     */
    QString xmlWithEmptySaleIdentifier() const
    {
        return QStringLiteral(
            "<?xml version=\"1.0\"?><Documents>"
            "<Bank Name=\"TestBank\" BankIdentifierValue=\"123456\" Encoding=\"UTF-8\">"
            "<BankIdentifier Name=\"BankIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" "
                "RegexOptions=\"None\">Depotnummer\\s+(\\d+)</BankIdentifier>"
            "<BuyIdentifier Name=\"BuyIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" "
                "RegexOptions=\"None\">ORDERABRECHNUNG\\s+KAUF</BuyIdentifier>"
            "<SaleIdentifier Name=\"SaleIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" "
                "RegexOptions=\"None\"></SaleIdentifier>"
            "<DividendIdentifier Name=\"DividendIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" "
                "RegexOptions=\"None\">Dividendengutschrift</DividendIdentifier>"
            "<BrokerageIdentifier Name=\"BrokerageIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" "
                "RegexOptions=\"None\">Kosten</BrokerageIdentifier>"
            "<Document Type=\"Dividend\" TypeIdentifierValue=\"Dividendengutschrift\" Encoding=\"UTF-8\">"
            "<Date Name=\"Date\" FoundIndex=\"0\" ResultEmpty=\"false\" "
                "RegexOptions=\"None\">Valuta\\s+(\\d{2}.\\d{2}.\\d{4})</Date>"
            "</Document>"
            "</Bank>"
            "</Documents>");
    }

    /**
     * @brief Nachbau der DKB-Regeln, die auf die POSITION des Treffers
     *        setzen — `FoundIndex="1"` heisst "das zweite Klammerpaar".
     *
     * Feldfall vom 21.08.2026: ein DKB-Dividendenbeleg, der im Dialog
     * einwandfrei gelesen wird, meldete per Drag&Drop "Keine passende Aktie
     * im Portfolio gefunden". Ursache war, dass `extractFieldValue()` den
     * `FoundIndex` ignorierte und immer den ersten Treffer nahm — das ist
     * hier die Spaltenüberschrift `(WKN)`, nicht die WKN.
     *
     * Der Dividendenblock trägt zusätzlich eine Regel mit Alternativen
     * (`Wkn2`), deren erste Fanggruppe im Testtext leer bleibt: die zweite
     * Prüfung der Auswahlregel des `ParserLib::Parser`.
     */
    QString xmlWithPositionalWkn() const
    {
        return QStringLiteral(
            "<?xml version=\"1.0\"?><Documents>"
            "<Bank Name=\"TestBank\" BankIdentifierValue=\"123456\" Encoding=\"UTF-8\">"
            "<BankIdentifier Name=\"BankIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" "
                "RegexOptions=\"None\">Depotnummer\\s+(\\d+)</BankIdentifier>"
            "<BuyIdentifier Name=\"BuyIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" "
                "RegexOptions=\"None\">Wertpapier Abrechnung Kauf</BuyIdentifier>"
            "<SaleIdentifier Name=\"SaleIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" "
                "RegexOptions=\"None\">Wertpapier Abrechnung Verkauf</SaleIdentifier>"
            "<DividendIdentifier Name=\"DividendIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" "
                "RegexOptions=\"None\">Dividendengutschrift</DividendIdentifier>"
            "<BrokerageIdentifier Name=\"BrokerageIdentifier\" FoundIndex=\"0\" ResultEmpty=\"true\" "
                "RegexOptions=\"None\">Kostenausweis</BrokerageIdentifier>"
            "<Document Type=\"Dividend\" TypeIdentifierValue=\"Dividendengutschrift\" Encoding=\"UTF-8\">"
            "<Wkn Name=\"Wkn\" FoundIndex=\"1\" ResultEmpty=\"true\" "
                "RegexOptions=\"None\">[(]((?:[A-Za-z0-9]{1,}))[)]</Wkn>"
            "<Wkn2 Name=\"Wkn2\" FoundIndex=\"0\" ResultEmpty=\"true\" "
                "RegexOptions=\"None\">WKN-alt:\\s+([A-Z0-9]{6})|WKN:\\s+([A-Z0-9]{6})</Wkn2>"
            "<Date Name=\"Date\" FoundIndex=\"0\" ResultEmpty=\"false\" "
                "RegexOptions=\"None\">Datum:\\s+(\\d{2}.\\d{2}.\\d{4})</Date>"
            "</Document>"
            "</Bank>"
            "</Documents>");
    }

private slots:

    void initTestCase()
    {
        QVERIFY(m_tempDir.isValid());
    }

    // ── Leere Kennungen (Bugfix 21.08.2026) ─────────────────────────────
    // Ein leeres Regex-Muster ist gültig und trifft JEDEN Text (leerer
    // Treffer an Position 0). Ohne Sonderbehandlung gewinnt eine leer
    // gelassene Kennung gegen jede später geprüfte, tatsächlich passende —
    // findMatchingType() geht Buy, Sale, Dividend, Brokerage der Reihe nach
    // durch und nimmt den ersten Treffer.

    void test_classify_emptySaleIdentifier_doesNotSwallowDividend()
    {
        // Der Feldfall: Consors-Dividendengutschrift. Vor dem Bugfix wurde
        // sie von der leeren SaleIdentifier-Regel als "Sale" eingestuft;
        // weil die Bank keinen Sale-Dokumentblock hat, brach die Erkennung
        // danach ab und es wurde kein einziges Feld gelesen.
        DocumentsConfig config;
        QCOMPARE(config.load(writeXml(QStringLiteral("emptysale.xml"),
                                      xmlWithEmptySaleIdentifier())),
                 DocumentsConfig::LoadResult::Success);

        const QString text = QStringLiteral(
            "Depotnummer 123456\nDividendengutschrift\n"
            "Dividende pro Stück 0,39843 EUR   Schlusstag 05.02.2019\n"
            "Valuta 08.02.2019");

        const auto result = DocumentClassifier::classify(text, config);
        QVERIFY(result.matched);
        QCOMPARE(result.type, DocumentType::Dividend);
        QCOMPARE(result.docEntry.type, DocumentType::Dividend);
    }

    void test_detectDocumentType_emptyIdentifier_isSkipped()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("emptysale2.xml"),
                             xmlWithEmptySaleIdentifier()));

        const QString text = QStringLiteral(
            "Depotnummer 123456\nDividendengutschrift\nValuta 08.02.2019");

        const auto banks = config.entries();
        QCOMPARE(banks.size(), 1);
        QCOMPARE(DocumentClassifier::detectDocumentType(text, banks.first(),
                                                        DocumentType::Buy),
                 DocumentType::Dividend);
    }

    // ── Result::bankMatched (21.08.2026) ────────────────────────────────
    // Trennt die beiden Fehlerursachen: unbekannte Bank (Eintrag fehlt in
    // Documents.xml) gegen unbekannten Dokumenttyp (Belegart wird nicht
    // verarbeitet). MainWindow nennt sie in der Meldung getrennt.

    void test_classify_unknownDocumentType_reportsBankMatched()
    {
        // Anlass: eine DKB-"Vorabpauschale Investmentfonds" (Nessie,
        // 21.08.2026). Die Bank steht im Beleg, die Belegart kennt die
        // Anwendung aber nicht.
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("bankonly.xml"), validXml()));

        const QString text = QStringLiteral(
            "Depotnummer 123456\nVorabpauschale Investmentfonds\n"
            "Zahlbarkeitstag 02.01.2019\nEx-Tag 02.01.2019");

        const auto result = DocumentClassifier::classify(text, config);
        QVERIFY(!result.matched);
        QVERIFY(result.bankMatched);
        QCOMPARE(result.bank.name, QStringLiteral("TestBank"));
    }

    void test_classify_unknownBank_reportsBankNotMatched()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("nobank.xml"), validXml()));

        const QString text = QStringLiteral(
            "Konto 999\nWertpapier Abrechnung Kauf\nDatum: 01.07.2026");

        const auto result = DocumentClassifier::classify(text, config);
        QVERIFY(!result.matched);
        QVERIFY(!result.bankMatched);
        QVERIFY(result.bank.name.isEmpty());
    }

    void test_classify_success_alsoSetsBankMatched()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("bothok.xml"), validXml()));

        const QString text = QStringLiteral(
            "Depotnummer 123456\nWertpapier Abrechnung Kauf\nDatum: 01.07.2026");

        const auto result = DocumentClassifier::classify(text, config);
        QVERIFY(result.matched);
        QVERIFY(result.bankMatched);
    }

    void test_detectDocumentType_noIdentifierMatches_usesFallback()
    {
        // Gegenprobe: Trifft KEINE Kennung, bleibt es beim Vorgabewert —
        // die leere Sale-Regel darf auch hier nicht einspringen.
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("emptysale3.xml"),
                             xmlWithEmptySaleIdentifier()));

        const QString text = QStringLiteral(
            "Depotnummer 123456\nIrgendein anderes Schreiben\n");

        const auto banks = config.entries();
        QCOMPARE(DocumentClassifier::detectDocumentType(text, banks.first(),
                                                        DocumentType::Dividend),
                 DocumentType::Dividend);
    }

    // ── classify() ──────────────────────────────────────────────────────

    void test_classify_buyDocument_matched()
    {
        DocumentsConfig config;
        QCOMPARE(config.load(writeXml(QStringLiteral("buy.xml"), validXml())),
                 DocumentsConfig::LoadResult::Success);

        const QString text = QStringLiteral(
            "Depotnummer 123456\nWertpapier Abrechnung Kauf\nDatum: 01.07.2026\n"
            "WKN: BASF11\nISIN: DE000BASF111");

        const auto result = DocumentClassifier::classify(text, config);
        QVERIFY(result.matched);
        QCOMPARE(result.type, DocumentType::Buy);
        QCOMPARE(result.bank.name, QStringLiteral("TestBank"));
        QCOMPARE(result.docEntry.type, DocumentType::Buy);
    }

    void test_classify_saleDocument_matched()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("sale.xml"), validXml()));

        const QString text = QStringLiteral(
            "Depotnummer 123456\nWertpapier Abrechnung Verkauf\nDatum: 02.07.2026");

        const auto result = DocumentClassifier::classify(text, config);
        QVERIFY(result.matched);
        QCOMPARE(result.type, DocumentType::Sale);
    }

    void test_classify_dividendDocument_matched()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("dividend.xml"), validXml()));

        const QString text = QStringLiteral(
            "Depotnummer 123456\nDividendengutschrift\nDatum: 03.07.2026");

        const auto result = DocumentClassifier::classify(text, config);
        QVERIFY(result.matched);
        QCOMPARE(result.type, DocumentType::Dividend);
    }

    void test_classify_unknownBank_notMatched()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("unknownbank.xml"), validXml()));

        const QString text = QStringLiteral("Irgendein Dokument ohne Bank-Bezug.");

        const auto result = DocumentClassifier::classify(text, config);
        QVERIFY(!result.matched);
    }

    void test_classify_knownBank_unknownType_notMatched()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("unknowntype.xml"), validXml()));

        // Bank identifier matches, but none of the four document identifiers do.
        const QString text = QStringLiteral("Depotnummer 123456\nIrgendein anderer Text.");

        const auto result = DocumentClassifier::classify(text, config);
        QVERIFY(!result.matched);
    }

    void test_classify_emptyConfig_notMatched()
    {
        DocumentsConfig config; // never loaded — isValid() == false, entries() empty
        const auto result = DocumentClassifier::classify(QStringLiteral("egal"), config);
        QVERIFY(!result.matched);
    }

    // ── extractFieldValue() / extractWkn() / extractIsin() ────────────────

    void test_extractWkn_found()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("wkn.xml"), validXml()));

        const QString text = QStringLiteral(
            "Depotnummer 123456\nWertpapier Abrechnung Kauf\nWKN: BASF11\nISIN: DE000BASF111");

        const auto result = DocumentClassifier::classify(text, config);
        QVERIFY(result.matched);

        const QString wkn = DocumentClassifier::extractWkn(text, result.docEntry);
        QCOMPARE(wkn, QStringLiteral("BASF11"));
    }

    void test_extractIsin_found()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("isin.xml"), validXml()));

        const QString text = QStringLiteral(
            "Depotnummer 123456\nWertpapier Abrechnung Kauf\nWKN: BASF11\nISIN: DE000BASF111");

        const auto result = DocumentClassifier::classify(text, config);
        QVERIFY(result.matched);

        const QString isin = DocumentClassifier::extractIsin(text, result.docEntry);
        QCOMPARE(isin, QStringLiteral("DE000BASF111"));
    }

    void test_extractWkn_notPresentInDocType_returnsEmpty()
    {
        // The "Sale" document entry in validXml() has no Wkn rule at all.
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("nowkn.xml"), validXml()));

        const QString text = QStringLiteral(
            "Depotnummer 123456\nWertpapier Abrechnung Verkauf\nDatum: 02.07.2026");

        const auto result = DocumentClassifier::classify(text, config);
        QVERIFY(result.matched);
        QCOMPARE(result.type, DocumentType::Sale);

        QVERIFY(DocumentClassifier::extractWkn(text, result.docEntry).isEmpty());
    }

    void test_extractFieldValue_noMatch_returnsEmpty()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("nomatch.xml"), validXml()));

        const auto result = DocumentClassifier::classify(
            QStringLiteral("Depotnummer 123456\nWertpapier Abrechnung Kauf"), config);
        QVERIFY(result.matched);

        // Text has no "WKN: ..." at all.
        QVERIFY(DocumentClassifier::extractWkn(QStringLiteral("kein WKN hier"),
                                               result.docEntry).isEmpty());
    }

    /**
     * @brief Bugfix 21.08.2026 — `FoundIndex` muss beachtet werden.
     *
     * Der Regelsatz der DKB wählt die WKN über die POSITION des Treffers
     * (`FoundIndex="1"`, "das zweite Klammerpaar"). Bis zum Bugfix nahm
     * `extractFieldValue()` immer den ersten Treffer und lieferte damit die
     * Spaltenüberschrift "WKN" statt der Wertpapierkennnummer — worauf
     * `resolveShareGuidForDocument()` keine Aktie fand und die Direkte
     * Dokumentenerfassung mit "Keine passende Aktie im Portfolio gefunden"
     * abbrach, obwohl derselbe Beleg im Dividenden-Dialog fehlerfrei lief.
     */
    void test_extractWkn_honoursFoundIndex()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("posidx.xml"), xmlWithPositionalWkn()));

        // Erstes Klammerpaar ist die Spaltenüberschrift, zweites die WKN —
        // der Aufbau echter DKB-Belege.
        const QString text = QStringLiteral(
            "Depotnummer 123456\n"
            "Dividendengutschrift\n"
            "Nominale  Wertpapierbezeichnung   ISIN          (WKN)\n"
            "Stück 40  MUSTER INC              US0001234567  (654321)\n"
            "Datum: 02.07.2026");

        const auto result = DocumentClassifier::classify(text, config);
        QVERIFY(result.matched);
        QCOMPARE(result.type, DocumentType::Dividend);

        QCOMPARE(DocumentClassifier::extractWkn(text, result.docEntry),
                 QStringLiteral("654321"));
    }

    /**
     * @brief Aus dem gewählten Treffer zählt die erste NICHT-LEERE
     *        Fanggruppe — wie in `ParserLib::Parser::doRegexParsing()`.
     *
     * Bei einem Ausdruck mit Alternativen (`a(x)|b(y)`) füllt jeder Treffer
     * nur eine der beiden Gruppen. Die alte Fassung griff starr auf Gruppe 1
     * zu und lieferte hier eine leere Zeichenkette.
     */
    void test_extractFieldValue_skipsEmptyCaptureGroups()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("altgroups.xml"), xmlWithPositionalWkn()));

        // Nur die ZWEITE Alternative trifft, Fanggruppe 1 bleibt leer.
        const QString text = QStringLiteral(
            "Depotnummer 123456\nDividendengutschrift\nWKN: BASF11\nDatum: 02.07.2026");

        const auto result = DocumentClassifier::classify(text, config);
        QVERIFY(result.matched);

        QCOMPARE(DocumentClassifier::extractFieldValue(text, result.docEntry.regexList,
                                                       QStringLiteral("Wkn2")),
                 QStringLiteral("BASF11"));
    }

    /**
     * @brief Verlangt die Regel einen Treffer, den es nicht gibt, bleibt das
     *        Feld leer — geraten wird nicht.
     *
     * Wichtig für die Direkte Dokumentenerfassung: eine falsch geratene WKN
     * fände entweder gar keine oder — schlimmer — die FALSCHE Aktie.
     */
    void test_extractWkn_foundIndexBeyondLastMatch_returnsEmpty()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("posidx2.xml"), xmlWithPositionalWkn()));

        // Nur EIN Klammerpaar — der geforderte Index 1 existiert nicht.
        const QString text = QStringLiteral(
            "Depotnummer 123456\nDividendengutschrift\n"
            "Stück 40  MUSTER INC  US0001234567  (654321)\nDatum: 02.07.2026");

        const auto result = DocumentClassifier::classify(text, config);
        QVERIFY(result.matched);

        QVERIFY(DocumentClassifier::extractWkn(text, result.docEntry).isEmpty());
    }

    /**
     * @brief Eine leere Regel identifiziert nichts (vgl. `regexMatches()`).
     *
     * `QRegularExpression("")` trifft jeden Text an Position 0; ohne diese
     * Prüfung käme statt "kein Wert" eine leere Zeichenkette zurück, die
     * `resolveShareGuidForDocument()` als gefundene WKN weiterreichen würde.
     */
    void test_extractFieldValue_emptyExpression_returnsEmpty()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("emptyrule.xml"), xmlWithEmptySaleIdentifier()));

        const auto result = DocumentClassifier::classify(
            QStringLiteral("Depotnummer 123456\nDividendengutschrift\nValuta 02.07.2026"),
            config);
        QVERIFY(result.matched);

        ParserLib::RegExList rules = result.docEntry.regexList;
        ParserLib::RegExElement empty;
        empty.regexExpression = QString();
        rules.insert(QStringLiteral("Wkn"), empty);

        QVERIFY(DocumentClassifier::extractFieldValue(
                    QStringLiteral("beliebiger Text"), rules,
                    QStringLiteral("Wkn")).isEmpty());
    }

    // ── matchBankIndex() / detectDocumentType() ────────────────────────────
    // (used by the refactored PresenterBuyEdit/PresenterSaleEdit/
    // PresenterDividendEdit/PresenterShareAdd — see ARCHITECTURE.md)

    void test_matchBankIndex_found()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("bankidx.xml"), validXml()));

        int index = -1;
        const bool found = DocumentClassifier::matchBankIndex(
            QStringLiteral("Depotnummer 123456"), config, index);
        QVERIFY(found);
        QCOMPARE(index, 0);
    }

    void test_matchBankIndex_notFound_leavesIndexUnchanged()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("bankidx2.xml"), validXml()));

        int index = -7;
        const bool found = DocumentClassifier::matchBankIndex(
            QStringLiteral("kein Depot hier"), config, index);
        QVERIFY(!found);
        QCOMPARE(index, -7); // unchanged
    }

    void test_detectDocumentType_matches_buyIdentifier()
    {
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("detecttype.xml"), validXml()));

        int index = -1;
        QVERIFY(DocumentClassifier::matchBankIndex(
            QStringLiteral("Depotnummer 123456\nWertpapier Abrechnung Kauf"), config, index));
        const BankEntry bank = config.entries().at(index);

        const DocumentType type = DocumentClassifier::detectDocumentType(
            QStringLiteral("Depotnummer 123456\nWertpapier Abrechnung Kauf"),
            bank, DocumentType::Dividend /* deliberately "wrong" fallback */);
        QCOMPARE(type, DocumentType::Buy); // identifier match wins over fallback
    }

    void test_detectDocumentType_noIdentifierMatch_returnsFallback()
    {
        // Mirrors e.g. PresenterSaleEdit::startParserForText(), which defaults
        // to DocumentType::Sale when the bank matched but no explicit
        // Buy-/Sale-/Dividend-/BrokerageIdentifier does — the user already
        // chose the "Verkäufe hinzufügen" dialog, so guessing Sale is correct
        // there (unlike DocumentClassifier::classify(), which never guesses).
        DocumentsConfig config;
        config.load(writeXml(QStringLiteral("detecttype2.xml"), validXml()));

        int index = -1;
        QVERIFY(DocumentClassifier::matchBankIndex(
            QStringLiteral("Depotnummer 123456\nirgendein anderer Text"), config, index));
        const BankEntry bank = config.entries().at(index);

        const DocumentType type = DocumentClassifier::detectDocumentType(
            QStringLiteral("Depotnummer 123456\nirgendein anderer Text"),
            bank, DocumentType::Sale);
        QCOMPARE(type, DocumentType::Sale); // fallback, since nothing matched
    }
};

QTEST_MAIN(TestDocumentClassifier)
#include "tst_documentclassifier.moc"

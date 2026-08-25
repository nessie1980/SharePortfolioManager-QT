// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// tst_documentsxml.cpp — prüft die AUSGELIEFERTE app/config/Documents.xml
// gegen repräsentative Beleg-Auszüge (Phase 5 der Ex-Tag-Behandlung,
// 21.08.2026).
//
// Abgrenzung zu tst_documentsconfig.cpp: dort geht es um den XML-PARSER
// (Fehlerfälle, fehlende Attribute, mehrere Banken) mit synthetischen
// Fixtures. Hier geht es um den INHALT der echten Konfigurationsdatei — ob
// die dort hinterlegten regulären Ausdrücke auf echten Dividendengutschriften
// das Richtige treffen. Beides zusammen fängt zwei verschiedene Fehlerarten
// ab; bislang war die zweite gar nicht geprüft.
//
// Die Belegtexte stammen aus anonymisierten Screenshots realer Abrechnungen
// (Nessie, 21.08.2026) und sind so formatiert, wie `pdftotext -layout` sie
// liefert — genau das benutzt PdfTextExtractor. Geschwärzte Stellen
// (Wertpapiername, Kontonummern) sind durch Platzhalter ersetzt.
//
// Ausgeführt wird mit dem ECHTEN ParserLib::Parser, nicht mit einer
// nachgebauten Auswertung: nur so ist auch die Auswahlregel mitgeprüft
// (FoundIndex wählt den Treffer, daraus die erste nicht-leere Fanggruppe —
// siehe Parser::doRegexParsing()). Im Textmodus arbeitet der Parser
// synchron, ein Ereignisschleifen-Umweg ist deshalb nicht nötig.

#include <QtTest>
#include <QFileInfo>
#include <QDate>
#include <QRegularExpression>

#include "../../app/config/DocumentsConfig.h"
#include "../../app/utils/DocumentClassifier.h"
#include "../../libs/parser/src/Parser.h"

namespace {

// ── Belegtexte ────────────────────────────────────────────────────────────────

/// ING DiBa, Ausschüttung in EUR.
const char* kIngEur = R"(Direkt-Depot Nr.:  8006189848

Dividendengutschrift

ISIN (WKN)              DE0001234567 (123456)
Wertpapierbezeichnung   MUSTER AG

Nominale                125,00 Stück
Zins-/Dividendensatz    17,10 EUR
Ex-Tag                  08.05.2026
Zahltag                 12.05.2026

Brutto                            EUR         2.137,50
Kapitalertragsteuer 25,00%        EUR           534,38
Solidaritätszuschlag 5,50%        EUR            29,39
Gesamtbetrag zu Ihren Gunsten     EUR         1.573,73

Valuta                  12.05.2026
)";

/// ING DiBa, Ausschüttung in USD mit Devisenkurs.
const char* kIngUsd = R"(Direkt-Depot Nr.:  8006189848

Dividendengutschrift

ISIN (WKN)              US0001234567 (654321)
Wertpapierbezeichnung   MUSTER INC

Nominale                150,00 Stück
Zins-/Dividendensatz    0,53 USD
Ex-Tag                  15.06.2026
Zahltag                 01.07.2026

Brutto                            USD            79,50
QuSt 15,00 % (EUR 10,39)          USD            11,93
Zwischensumme                     USD            67,57
Umg. z. Dev.-Kurs (1,148693)      EUR            58,82
Kapitalertragsteuer 25,00%        EUR             6,91
Solidaritätszuschlag 5,50%        EUR             0,38
Gesamtbetrag zu Ihren Gunsten     EUR            51,53

Valuta                  01.07.2026
)";

/// DKB, Ausschüttung in USD mit Devisenkurs.
const char* kDkbUsd = R"(Depotnummer  501403950

Dividendengutschrift

Nominale     Wertpapierbezeichnung                    ISIN            (WKN)
Stück 40     MUSTER INC                               US0001234567    (654321)

Zahlbarkeitstag      15.12.2016      Dividende pro Stück        0,35    USD
Bestandsstichtag     28.11.2016      Herkunftsland              USA
Ex-Tag               29.11.2016      Art der Dividende          Quartalsdividende
Geschäftsjahr        01.01.2016 - 31.12.2016
Devisenkurs          EUR / USD  1,04535
Devisenkursdatum     15.12.2016


Dividendengutschrift                            14,00  USD           13,39+  EUR

Umrechnung in EUR                               13,39  EUR
Einbehaltene Quellensteuer 15 % auf 14,00 USD                         2,01-  EUR
Anrechenbare Quellensteuer 15 % auf 13,39 EUR    2,01  EUR

Kapitalertragsteuerpflichtige Dividende         13,39  EUR
Verrechnete anrechenbare ausländische Quellensteuer
(Verhältnis 100/25) auf 2,01 EUR                 8,04 -  EUR
Berechnungsgrundlage für die Kapitalertragsteuer 5,35  EUR

Kapitalertragsteuer 25 % auf 5,35 EUR                                 1,34-  EUR
Solidaritätszuschlag 5,5 % auf 1,34 EUR                               0,07-  EUR

Ausmachender Betrag                                                   9,97+  EUR
)";

/// DKB, Ausschüttung in EUR — KONSTRUIERT, kein echter Beleg.
///
/// Abgeleitet aus dem USD-Beleg ohne die Devisen-Zeilen. Nessie besitzt
/// keinen DKB-Beleg über eine EUR-Ausschüttung (Stand 21.08.2026), es wird
/// also auch keiner nachgereicht.
///
/// Der eigentliche Wert dieses Fixtures liegt deshalb NICHT darin, ein
/// bekanntes Layout nachzubilden, sondern in einer Aussage, die unabhängig
/// vom genauen Aufbau gilt: ein Euro-Beleg darf keinen Devisenkurs liefern
/// (test_eurDocuments_haveNoExchangeRate) — sonst würde der
/// Fremdwährungs-Modus fälschlich anspringen und die Ausschüttung durch
/// einen fremden Kurs geteilt. Die übrigen Erwartungen an dieses Fixture
/// sind Annahme, nicht Befund; sollte je ein echter DKB-EUR-Beleg
/// auftauchen, gehört es durch ihn ersetzt.
const char* kDkbEur = R"(Depotnummer  501403950

Dividendengutschrift

Nominale     Wertpapierbezeichnung                    ISIN            (WKN)
Stück 125    MUSTER AG                                DE0001234567    (123456)

Zahlbarkeitstag      12.05.2026      Dividende pro Stück       17,10    EUR
Bestandsstichtag     07.05.2026      Herkunftsland             DEU
Ex-Tag               08.05.2026      Art der Dividende         Jahresdividende
Geschäftsjahr        01.01.2025 - 31.12.2025


Dividendengutschrift                         2.137,50  EUR

Kapitalertragsteuer 25 % auf 2.137,50 EUR                           534,38-  EUR
Solidaritätszuschlag 5,5 % auf 534,38 EUR                            29,39-  EUR

Ausmachender Betrag                                               1.573,73+  EUR
)";

/// Cortal Consors, Ausschüttung in EUR (Nessies Screenshot, 21.08.2026).
///
/// Der Kopfbereich ist auf dem Screenshot abgeschnitten. Die Zeile mit der
/// Depotnummer MUSS dort stehen: die Depoterkennung
/// (`DocumentClassifier::matchDepotIndex()`) läuft über die
/// `BankIdentifier`-Regel dieses Eintrags, und die sucht genau danach. Ohne
/// sie würde der Beleg gar nicht als Consors-Dokument erkannt.
///
/// @note Die Depotnummer trug hier bis zum 25.08.2026 die Form `878031421`
/// — ohne die führende Null. Nessie hat bestätigt, dass der echte Beleg
/// `0878031421` schreibt, also genau den Wert aus `Documents.xml`. Die
/// Abweichung ist folgenlos, solange die Erkennung nur die BESCHRIFTUNG
/// prüft; sobald sie die Nummer vergleicht, wäre der Beleg mit der alten
/// Fassung seinem eigenen Depot nicht mehr zugeordnet worden. Ein Fixture,
/// das an der entscheidenden Stelle von der Wirklichkeit abweicht, prüft
/// eben nicht die Wirklichkeit.
///
/// @note Die Zeile "Netto zugunsten Konto 878031421" bleibt bewusst wie sie
/// ist: sie trägt eine KONTO-, keine Depotnummer und wird von keiner Regel
/// gelesen.
///
/// @note Der Unterschied zwischen `Depotnummer 0878…` und `Depotnummer: 0878…`
/// ist für die Bankerkennung erheblich: ohne Doppelpunkt trifft auch die
/// DKB-Regel (`Depotnummer\s+([0-9]{1,9})`, die ersten neun Ziffern), und da
/// die DKB in Documents.xml zuerst steht, wird der Consors-Beleg ihr
/// zugeschlagen. Für DIESE Tests ist es gleichgültig — `parseDividend()`
/// schlägt die Bank am Namen nach und umgeht die Erkennung —, und die
/// `DepotNumber`-Regel deckt beide Formen ab. Siehe ARCHITECTURE.md,
/// "Bankerkennung: Mehrdeutigkeit über die Depotnummer".
///
/// @note Dieser Beleg kennt KEINE Beschriftung "Ex-Tag" — siehe
/// test_cortal_hasNoExDateLabel().
const char* kCortalEur = R"(Cortal Consors
Depotnummer 0878031421

Dividendengutschrift

Wertpapierbezeichnung                                  WKN         ISIN
MUSTER AG                                              123456      DE0001234567

10 Stück

Dividende pro Stück 0,39843 EUR                                Schlusstag 05.02.2019

Brutto                                                                3,98 EUR
abzgl. Kapitalertragsteuer          25,00 % von      3,98 EUR         1,00 EUR
abzgl. Solidaritätszuschlag          5,50 % von      1,00 EUR         0,05 EUR
Netto zugunsten Konto 878031421                                       2,93 EUR
Valuta 08.02.2019
)";

/// DKB, Wertpapier-Abrechnung VERKAUF (Nessies Screenshot, 22.08.2026).
///
/// Erster Verkaufsbeleg im Fixture-Bestand — bis dahin prüfte
/// `tst_documentsxml` ausschliesslich Dividendengutschriften. Anlass ist der
/// Feldfall "das Kaufdatum wird nicht ausgelesen": die DKB beschriftet Datum
/// und Uhrzeit gemeinsam mit "Schlusstag/-Zeit".
///
/// @note Der Briefkopf ist gekürzt und anonymisiert; nur die Depotnummer
/// steht wie im Original, weil die Bankerkennung sie braucht. Die Zeile
/// "Ausführungskurs" und der zweispaltige Block darüber sind layoutgetreu
/// nachgebildet: die alte `Date`-Regel verlangte, dass unmittelbar nach der
/// Uhrzeit das Wort "Auftraggeber" folgt — eine Bindung an eine ganz andere
/// Spalte, die mit dem Feld nichts zu tun hat.
///
/// @note Die Spalte `(WKN)` neben der ISIN war auf dem Screenshot NICHT zu
/// sehen — die Dokumentenvorschau schnitt den rechten Seitenrand ab. Nessie
/// hat am 22.08.2026 bestätigt, dass sie dort steht, in Klammern wie auf den
/// übrigen DKB-Belegen. Damit greift auch hier die positionsabhängige
/// `Wkn`-Regel (`FoundIndex="1"`: erst die Überschrift, dann die Nummer), und
/// die Direkte Dokumentenerfassung kann einen Verkaufsbeleg zuordnen. Ohne
/// diese Zeilen wäre das Fixture stillschweigend leichter als die
/// Wirklichkeit.
const char* kDkbSale = R"(                                                Seite 1 von 2
                                                Depotnummer      501403950
                                                Kundennummer
10919 Berlin
                                                Auftragsnummer   267621/08.00
                                                Datum            27.02.2020
                                                Rechnungsnummer
                                                Umsatzsteuer-ID


Wertpapier Abrechnung Verkauf

Nominale     Wertpapierbezeichnung                             ISIN            (WKN)
Stück 40     MUSTER INC   SHARES                               US0001234567    (654321)
             REGISTERED SHARES DL -.25

Handels-/Ausführungsplatz     Tradegate
Börsensegment                 XGAT

Market-Order
Limit                    bestens
Schlusstag/-Zeit  27.02.2020 19:16:37            Auftraggeber             Max Mustermann
Ausführungskurs 51,47 EUR                        Auftragserteilung/ -ort  Online-Banking

Provision                                                          10,00- EUR
Ausmachender Betrag                                             2.048,80+ EUR
)";

/// Die Depotnummern aus `Documents.xml` — der eineindeutige Schlüssel eines
/// Eintrags (siehe DepotEntry). Alle Helfer dieses Testziels schlagen darüber
/// nach, NICHT über den Banknamen: sobald ein zweites Depot bei einer bereits
/// eingetragenen Bank hinzukommt, tragen beide Einträge denselben Namen, und
/// eine Suche danach lieferte stillschweigend den erstbesten Regelsatz.
const char* kDepotDkb     = "501403950";
const char* kDepotIng     = "8006189848";
const char* kDepotConsors = "0878031421";

} // namespace

class TestDocumentsXml : public QObject
{
    Q_OBJECT

    DocumentsConfig m_config;

    // ── Helpers ───────────────────────────────────────────────────────────

    /// Wendet die Dividenden-Regeln des Depots @p depotNumber auf @p text an.
    QMap<QString, QList<QString>> parseDividend(const QString& depotNumber,
                                                 const QString& text)
    {
        return parseDocument(depotNumber, DocumentType::Dividend, text);
    }

    /// Wendet die Regeln des Depots @p depotNumber für @p type auf @p text an.
    ///
    /// Schlägt seit dem 25.08.2026 über die DEPOTNUMMER nach statt über den
    /// Banknamen: der Name ist nicht eindeutig, sobald eine Bank ein zweites
    /// Depot führt, und dieses Testziel prüfte dann heimlich den falschen
    /// Regelsatz.
    QMap<QString, QList<QString>> parseDocument(const QString& depotNumber,
                                                DocumentType   type,
                                                const QString& text)
    {
        // findByDepotNumber() reicht einen Zeiger in die interne Liste von
        // m_config zurück — die lebt so lange wie das Testobjekt, anders als
        // die Kopie aus entries(). Die frühere Lebensdauer-Falle (Zeiger in
        // ein Temporary) entfällt damit.
        const DepotEntry* depot = m_config.findByDepotNumber(depotNumber);
        if (!depot) {
            qWarning() << "Depot nicht in Documents.xml:" << depotNumber;
            return {};
        }

        const DocumentEntry* doc = DocumentsConfig::findDocument(*depot, type);
        if (!doc) {
            qWarning() << "Kein Dokumentblock für Depot" << depotNumber
                       << "Typ" << static_cast<int>(type);
            return {};
        }

        ParserLib::Parser parser;
        QMap<QString, QList<QString>> captured;
        connect(&parser, &ParserLib::Parser::parserUpdated, this,
                [&captured](const ParserLib::ParserInfoState& state) {
                    if (state.lastErrorCode == ParserLib::ParserErrorCode::Finished ||
                        state.lastErrorCode == ParserLib::ParserErrorCode::ParsingFailed)
                        captured = state.searchResult;
                });

        parser.setParsingValues(ParserLib::ParsingValues(
            text,
            doc->encoding.isEmpty() ? QStringLiteral("UTF-8") : doc->encoding,
            doc->regexList));
        parser.startParsing();   // Textmodus ist synchron
        return captured;
    }

    /// Erster Treffer zu @p key, getrimmt — leer, wenn nichts gefunden wurde.
    static QString first(const QMap<QString, QList<QString>>& r, const char* key)
    {
        return r.value(QString::fromLatin1(key)).value(0).trimmed();
    }

    /// Dividendenblock des Depots @p depotNumber als KOPIE — für die Aufrufe
    /// von DocumentClassifier, die einen `DocumentEntry` erwarten.
    DocumentEntry dividendEntry(const QString& depotNumber)
    {
        if (const DepotEntry* depot = m_config.findByDepotNumber(depotNumber)) {
            if (const DocumentEntry* doc =
                    DocumentsConfig::findDocument(*depot, DocumentType::Dividend))
                return *doc;
        }
        qWarning() << "Kein Dividend-Block für Depot" << depotNumber;
        return {};
    }

private slots:

    void initTestCase()
    {
        // SPM_DOCUMENTS_XML zeigt auf die ausgelieferte Datei im Quellbaum
        // (gesetzt in tests/config/CMakeLists.txt).
        const QString path = QStringLiteral(SPM_DOCUMENTS_XML);
        QVERIFY2(QFileInfo::exists(path), qPrintable(path));
        // load() liefert einen LoadResult-Code, kein bool — QCOMPARE nennt im
        // Fehlerfall auch gleich den konkreten Grund.
        QCOMPARE(m_config.load(path), DocumentsConfig::LoadResult::Success);
        QVERIFY2(m_config.isValid(), qPrintable(m_config.lastError()));
    }

    // ── Ex-Tag (Phase 5, der eigentliche Anlass) ──────────────────────────

    void test_ing_eur_exDate()
    {
        QCOMPARE(first(parseDividend(QString::fromLatin1(kDepotIng), QString::fromUtf8(kIngEur)),
                       "ExDate"),
                 QStringLiteral("08.05.2026"));
    }

    void test_ing_usd_exDate()
    {
        QCOMPARE(first(parseDividend(QString::fromLatin1(kDepotIng), QString::fromUtf8(kIngUsd)),
                       "ExDate"),
                 QStringLiteral("15.06.2026"));
    }

    void test_dkb_usd_exDate()
    {
        QCOMPARE(first(parseDividend(QString::fromLatin1(kDepotDkb), QString::fromUtf8(kDkbUsd)),
                       "ExDate"),
                 QStringLiteral("29.11.2016"));
    }

    void test_dkb_eur_exDate()
    {
        QCOMPARE(first(parseDividend(QString::fromLatin1(kDepotDkb), QString::fromUtf8(kDkbEur)),
                       "ExDate"),
                 QStringLiteral("08.05.2026"));
    }

    void test_exDate_isNotConfusedWithPayday()
    {
        // Der wichtigste Test dieser Datei: Ex-Tag und Zahltag stehen in
        // beiden Belegformaten unmittelbar untereinander bzw. nebeneinander.
        // Eine positionsbasierte Regel würde sie leicht vertauschen.
        const auto ing = parseDividend(QString::fromLatin1(kDepotIng), QString::fromUtf8(kIngEur));
        QCOMPARE(first(ing, "ExDate"), QStringLiteral("08.05.2026"));
        QCOMPARE(first(ing, "Date"),   QStringLiteral("12.05.2026"));
        QVERIFY(first(ing, "ExDate") != first(ing, "Date"));
    }

    // ── Devisenkurs ───────────────────────────────────────────────────────

    void test_ing_usd_exchangeRate()
    {
        QCOMPARE(first(parseDividend(QString::fromLatin1(kDepotIng), QString::fromUtf8(kIngUsd)),
                       "ExchangeRate"),
                 QStringLiteral("1,148693"));
    }

    void test_dkb_usd_exchangeRate()
    {
        QCOMPARE(first(parseDividend(QString::fromLatin1(kDepotDkb), QString::fromUtf8(kDkbUsd)),
                       "ExchangeRate"),
                 QStringLiteral("1,04535"));
    }

    void test_eurDocuments_haveNoExchangeRate()
    {
        // Ein Euro-Beleg darf keinen Devisenkurs liefern — sonst würde der
        // Fremdwährungs-Modus fälschlich eingeschaltet.
        QVERIFY(first(parseDividend(QString::fromLatin1(kDepotIng), QString::fromUtf8(kIngEur)),
                      "ExchangeRate").isEmpty());
        QVERIFY(first(parseDividend(QString::fromLatin1(kDepotDkb), QString::fromUtf8(kDkbEur)),
                      "ExchangeRate").isEmpty());
    }

    /**
     * @brief Der Devisenkurs beider Banken hat die Bedeutung, die die
     *        Anwendung erwartet: Fremdwährungsbetrag GETEILT durch den Kurs
     *        ergibt Euro.
     *
     * Genau so rechnet PresenterDividendEdit::refreshDerivedValues()
     * (`payout = payoutFc / exchangeRatio`). Stimmte die Richtung nicht,
     * wären alle Fremdwährungs-Dividenden um den Faktor Kurs² daneben.
     */
    void test_exchangeRateDirection_matchesApplicationSemantics()
    {
        bool ok = false;

        const double ingRate = first(parseDividend(QString::fromLatin1(kDepotIng),
                                                   QString::fromUtf8(kIngUsd)),
                                     "ExchangeRate")
                                   .replace(QLatin1Char(','), QLatin1Char('.'))
                                   .toDouble(&ok);
        QVERIFY(ok);
        // Beleg: Zwischensumme USD 67,57 -> EUR 58,82
        QVERIFY2(qAbs(67.57 / ingRate - 58.82) < 0.01,
                 qPrintable(QString::number(67.57 / ingRate)));

        const double dkbRate = first(parseDividend(QString::fromLatin1(kDepotDkb),
                                                   QString::fromUtf8(kDkbUsd)),
                                     "ExchangeRate")
                                   .replace(QLatin1Char(','), QLatin1Char('.'))
                                   .toDouble(&ok);
        QVERIFY(ok);
        // Beleg: Dividendengutschrift USD 14,00 -> EUR 13,39
        QVERIFY2(qAbs(14.00 / dkbRate - 13.39) < 0.01,
                 qPrintable(QString::number(14.00 / dkbRate)));
    }

    // ── Währung ───────────────────────────────────────────────────────────

    void test_ing_currency()
    {
        QCOMPARE(first(parseDividend(QString::fromLatin1(kDepotIng), QString::fromUtf8(kIngUsd)),
                       "Currency"),
                 QStringLiteral("USD"));
        QCOMPARE(first(parseDividend(QString::fromLatin1(kDepotIng), QString::fromUtf8(kIngEur)),
                       "Currency"),
                 QStringLiteral("EUR"));
    }

    void test_dkb_currency()
    {
        QCOMPARE(first(parseDividend(QString::fromLatin1(kDepotDkb), QString::fromUtf8(kDkbUsd)),
                       "Currency"),
                 QStringLiteral("USD"));
        QCOMPARE(first(parseDividend(QString::fromLatin1(kDepotDkb), QString::fromUtf8(kDkbEur)),
                       "Currency"),
                 QStringLiteral("EUR"));
    }

    // ── Depotnummer ───────────────────────────────────────────────────────

    void test_ing_depotNumber()
    {
        QCOMPARE(first(parseDividend(QString::fromLatin1(kDepotIng), QString::fromUtf8(kIngUsd)),
                       "DepotNumber"),
                 QStringLiteral("8006189848"));
    }

    void test_dkb_depotNumber()
    {
        QCOMPARE(first(parseDividend(QString::fromLatin1(kDepotDkb), QString::fromUtf8(kDkbUsd)),
                       "DepotNumber"),
                 QStringLiteral("501403950"));
    }

    // ── Übrige Pflichtfelder: Regressionsschutz ───────────────────────────
    // Nicht neu in Phase 5, aber bislang nirgends gegen echten Belegtext
    // geprüft. Sie stehen hier, damit eine spätere Regex-Änderung an einer
    // Stelle nicht unbemerkt eine andere zerlegt.

    void test_ing_volumeAndRate()
    {
        const auto r = parseDividend(QString::fromLatin1(kDepotIng), QString::fromUtf8(kIngUsd));
        QCOMPARE(first(r, "Volume"),       QStringLiteral("150,00"));
        QCOMPARE(first(r, "DividendRate"), QStringLiteral("0,53"));
    }

    void test_dkb_volumeAndRate()
    {
        const auto r = parseDividend(QString::fromLatin1(kDepotDkb), QString::fromUtf8(kDkbUsd));
        QCOMPARE(first(r, "Volume"),       QStringLiteral("40"));
        QCOMPARE(first(r, "DividendRate"), QStringLiteral("0,35"));
    }

    void test_ing_taxes()
    {
        const auto r = parseDividend(QString::fromLatin1(kDepotIng), QString::fromUtf8(kIngUsd));
        QCOMPARE(first(r, "TaxAtSource"),    QStringLiteral("10,39"));
        QCOMPARE(first(r, "CapitalGainTax"), QStringLiteral("6,91"));
        QCOMPARE(first(r, "SolidarityTax"),  QStringLiteral("0,38"));
    }

    void test_dkb_paydayIsZahlbarkeitstag()
    {
        // Der Zahltag steht bei der DKB unter "Zahlbarkeitstag" und darf
        // nicht mit Bestandsstichtag oder Ex-Tag verwechselt werden — alle
        // drei stehen im Beleg direkt untereinander.
        const auto r = parseDividend(QString::fromLatin1(kDepotDkb), QString::fromUtf8(kDkbUsd));
        QCOMPARE(first(r, "Date"),   QStringLiteral("15.12.2016"));
        QCOMPARE(first(r, "ExDate"), QStringLiteral("29.11.2016"));
    }

    // @note Die STEUER-Felder der DKB werden hier bewusst NICHT geprüft.
    // Ihre Regeln arbeiten positionsbasiert (`([0-9., ]{1,})-` mit
    // FoundIndex 2/4/5, also "der dritte/fünfte/sechste Betrag mit
    // nachgestelltem Minus im gesamten Text"). Wie viele solcher Treffer vor
    // den Steuerzeilen liegen, hängt auch vom Briefkopf ab — und der ist auf
    // Nessies Screenshots geschwärzt. Ein Fixture ohne diesen Bereich
    // verschiebt die Indizes und würde ein Scheitern melden, das nichts über
    // echte Belege aussagt. Umgekehrt ist genau das der Grund, weshalb die
    // in Phase 5 ergänzten Felder alle über ihre BESCHRIFTUNG suchen: das
    // ist unabhängig davon, was sonst noch im Dokument steht.

    // ── Cortal Consors ────────────────────────────────────────────────────

    void test_cortal_depotNumberAndCurrency()
    {
        const auto r = parseDividend(QString::fromLatin1(kDepotConsors),
                                     QString::fromUtf8(kCortalEur));
        QCOMPARE(first(r, "DepotNumber"), QStringLiteral("0878031421"));
        QCOMPARE(first(r, "Currency"),    QStringLiteral("EUR"));
    }

    void test_cortal_paydayIsValutaNotSchlusstag()
    {
        // Der Beleg nennt zwei Daten: "Schlusstag 05.02.2019" und
        // "Valuta 08.02.2019". Gutgeschrieben wird zur Valuta — das ist der
        // Zahltag. Die frühere Regel nahm schlicht das ERSTE Datum im Text
        // und lieferte damit den Schlusstag.
        const auto r = parseDividend(QString::fromLatin1(kDepotConsors),
                                     QString::fromUtf8(kCortalEur));
        QCOMPARE(first(r, "Date"), QStringLiteral("08.02.2019"));
    }

    void test_cortal_volumeAndRate()
    {
        const auto r = parseDividend(QString::fromLatin1(kDepotConsors),
                                     QString::fromUtf8(kCortalEur));
        QCOMPARE(first(r, "Volume"),       QStringLiteral("10"));
        QCOMPARE(first(r, "DividendRate"), QStringLiteral("0,39843"));
    }

    void test_cortal_taxes()
    {
        const auto r = parseDividend(QString::fromLatin1(kDepotConsors),
                                     QString::fromUtf8(kCortalEur));
        QCOMPARE(first(r, "CapitalGainTax"), QStringLiteral("1,00"));
        QCOMPARE(first(r, "SolidarityTax"),  QStringLiteral("0,05"));
        // Keine Quellensteuer auf einer inländischen Ausschüttung.
        QVERIFY(first(r, "TaxAtSource").isEmpty());
    }

    void test_cortal_hasNoExDateLabel()
    {
        // Bewusst festgehalten statt stillschweigend hingenommen: der
        // Consors-Beleg nennt den Ex-Tag NICHT. Er nennt einen "Schlusstag",
        // und der ist laut Consors' eigener Erläuterung etwas anderes:
        //
        //   "Schlusstag = Dividenden-Stichtag = (bei deutschen
        //    Gesellschaften) Tag der Hauptversammlung. An diesem Tag müssen
        //    Sie die Aktie vor Börsenschluss im Depot haben, damit Sie
        //    Anspruch auf die Dividende haben. Normalerweise ist der
        //    Schlusstag einen Tag vor dem Ex-Tag."
        //
        // "Normalerweise" ist hier das entscheidende Wort. Schlusstag + 1
        // Kalendertag trifft den Ex-Tag nur, solange dazwischen kein
        // Wochenende oder Feiertag liegt — für den nächsten HANDELStag
        // bräuchte die Anwendung einen Börsenkalender, den sie nicht hat.
        //
        // Ein um einen Tag falscher Ex-Tag ginge direkt in die
        // Stückzahl-Plausibilitätsprüfung ein und würde dort entweder
        // fälschlich blockieren oder eine echte Abweichung durchwinken.
        // Deshalb bleibt der Ex-Tag bei Consors ein Handeintrag; die
        // Analyse-Statuszeile weist ihn korrekt als fehlende Pflichtangabe
        // aus. Siehe ARCHITECTURE.md, "Phase 5 — Umsetzungsdetails".
        const auto r = parseDividend(QString::fromLatin1(kDepotConsors),
                                     QString::fromUtf8(kCortalEur));
        QVERIFY(first(r, "ExDate").isEmpty());
    }

    void test_cortal_recordDateIsReadAsSubstituteHint()
    {
        // Ersatz für den fehlenden Ex-Tag: der Schlusstag wird gelesen und
        // von PresenterDividendEdit::populateFromResult() als Hinweis an das
        // Ex-Tag-Feld gehängt — angezeigt, nicht eingetragen. Damit hat der
        // Benutzer die Zahl vor Augen, ohne dass ein geratenes Datum in die
        // Stückzahl-Plausibilitätsprüfung gerät.
        const auto r = parseDividend(QString::fromLatin1(kDepotConsors),
                                     QString::fromUtf8(kCortalEur));
        QCOMPARE(first(r, "RecordDate"), QStringLiteral("05.02.2019"));
        // Und er darf nicht mit dem Zahltag verwechselt werden.
        QVERIFY(first(r, "RecordDate") != first(r, "Date"));
    }

    /**
     * @brief Gegenprobe zur ING-USD-Abrechnung: aus den geparsten Werten
     *        muss der auf dem Beleg ausgewiesene Auszahlungsbetrag entstehen.
     *
     * Rechenweg der Anwendung: `payoutFc = rate × volume`,
     * `payout = payoutFc / exchangeRatio`, davon die drei Steuern ab.
     * Beleg: "Gesamtbetrag zu Ihren Gunsten EUR 51,53".
     */
    void test_ing_usd_endToEndAmountMatchesDocument()
    {
        const auto r = parseDividend(QString::fromLatin1(kDepotIng), QString::fromUtf8(kIngUsd));

        auto num = [&r](const char* key) {
            return first(r, key).replace(QLatin1Char('.'), QString())
                                .replace(QLatin1Char(','), QLatin1Char('.'))
                                .toDouble();
        };

        const double payoutFc = num("DividendRate") * num("Volume");
        QVERIFY2(qAbs(payoutFc - 79.50) < 0.01, qPrintable(QString::number(payoutFc)));

        const double payout = payoutFc / num("ExchangeRate");
        const double net    = payout - num("TaxAtSource")
                                     - num("CapitalGainTax")
                                     - num("SolidarityTax");
        QVERIFY2(qAbs(net - 51.53) < 0.02, qPrintable(QString::number(net)));
    }

    // ── DKB-Verkaufsbeleg: "Schlusstag/-Zeit" ─────────────────────────────
    //
    // Nessies Bugreport 22.08.2026: "Es wird auch das Kaufdatum nicht aus dem
    // Dokument ausgelesen." Die DKB beschriftet Datum und Uhrzeit gemeinsam,
    // die Regeln `Date` und `Time` fangen deshalb aus derselben Zeile.

    void test_dkb_sale_date()
    {
        QCOMPARE(first(parseDocument(QString::fromLatin1(kDepotDkb), DocumentType::Sale,
                                     QString::fromUtf8(kDkbSale)), "Date"),
                 QStringLiteral("27.02.2020"));
    }

    void test_dkb_sale_time()
    {
        QCOMPARE(first(parseDocument(QString::fromLatin1(kDepotDkb), DocumentType::Sale,
                                     QString::fromUtf8(kDkbSale)), "Time"),
                 QStringLiteral("19:16:37"));
    }

    /**
     * @brief Die Regel darf sich nicht an die NACHBARSPALTE hängen.
     *
     * Vorher lautete sie `Schlusstag/-Zeit([0-9:. ]{1,})Auftraggeber` — sie
     * verlangte also, dass rechts neben der Uhrzeit ausgerechnet das Wort
     * "Auftraggeber" steht. Das ist eine ganz andere Spalte des Belegs;
     * verschiebt die Bank dort etwas, bricht die Datumsübernahme, obwohl das
     * Datum unverändert dasteht. Jetzt hängt die Regel nur noch an ihrer
     * eigenen Beschriftung.
     */
    void test_dkb_sale_dateDoesNotDependOnNeighbouringColumn()
    {
        QString text = QString::fromUtf8(kDkbSale);
        text.replace(QStringLiteral("Auftraggeber "), QStringLiteral("Auftraggeberin "));

        // Gegenprobe, dass der Testfall wirklich etwas prüft: die ALTE Regel
        // findet in diesem Text nichts mehr.
        const QRegularExpression oldRule(
            QStringLiteral("Schlusstag/-Zeit([0-9:. ]{1,})Auftraggeber "));
        QVERIFY(!oldRule.match(text).hasMatch());

        QCOMPARE(first(parseDocument(QString::fromLatin1(kDepotDkb), DocumentType::Sale, text),
                       "Date"),
                 QStringLiteral("27.02.2020"));
    }

    /**
     * @brief Festgehalten, WORAN es im Feld lag.
     *
     * Die alte Regel traf auf dem echten Beleg durchaus — sie lieferte nur
     * Datum UND Uhrzeit in einem Stück. Genau das konnte die View nicht
     * verarbeiten, und im Formular blieb das heutige Datum stehen. Der
     * Testfall hält den Rohwert fest, den die alte Regel erzeugte; dass ein
     * solcher Fang heute richtig aufgeteilt wird, prüft
     * `tst_documentfieldvalue`.
     */
    void test_dkb_sale_oldRuleReturnedDateAndTimeTogether()
    {
        const QRegularExpression oldRule(
            QStringLiteral("Schlusstag/-Zeit([0-9:. ]{1,})Auftraggeber"));
        const QRegularExpressionMatch m = oldRule.match(QString::fromUtf8(kDkbSale));

        QVERIFY(m.hasMatch());
        QCOMPARE(m.captured(1).trimmed(), QStringLiteral("27.02.2020 19:16:37"));
        QVERIFY(!QDate::fromString(m.captured(1).trimmed(),
                                   QStringLiteral("d.M.yyyy")).isValid());
    }

    /// Das Datum im Briefkopf (ebenfalls 27.02.2020) darf die Regel nicht
    /// zufällig retten — deshalb hier ein abweichender Schlusstag.
    void test_dkb_sale_dateComesFromSchlusstagNotFromLetterhead()
    {
        QString text = QString::fromUtf8(kDkbSale);
        text.replace(QStringLiteral("Schlusstag/-Zeit  27.02.2020"),
                     QStringLiteral("Schlusstag/-Zeit  26.02.2020"));

        QCOMPARE(first(parseDocument(QString::fromLatin1(kDepotDkb), DocumentType::Sale, text),
                       "Date"),
                 QStringLiteral("26.02.2020"));
    }

    /// Ordernummer 1:1 — der Punkt in "267621/08.00" ist Teil der Nummer.
    void test_dkb_sale_orderNumberKeepsItsDot()
    {
        QCOMPARE(first(parseDocument(QString::fromLatin1(kDepotDkb), DocumentType::Sale,
                                     QString::fromUtf8(kDkbSale)), "OrderNumber"),
                 QStringLiteral("267621/08.00"));
    }

    /// Regressionsschutz für die übrigen Pflichtfelder dieses Belegtyps.
    void test_dkb_sale_volumeAndPrice()
    {
        const auto r = parseDocument(QString::fromLatin1(kDepotDkb), DocumentType::Sale,
                                     QString::fromUtf8(kDkbSale));
        QCOMPARE(first(r, "Volume"),      QStringLiteral("40"));
        QCOMPARE(first(r, "Price"),       QStringLiteral("51,47"));
        QCOMPARE(first(r, "DepotNumber"), QStringLiteral("501403950"));
    }

    /**
     * @brief Auch der Verkaufsbeleg trägt die WKN in Klammern neben der ISIN.
     *
     * Auf dem Screenshot vom 22.08.2026 war diese Spalte abgeschnitten, was
     * die Frage aufwarf, ob ein DKB-VERKAUFSbeleg überhaupt einer Aktie
     * zugeordnet werden kann — der Block hat keine `Isin`-Regel als Fangnetz.
     * Nessies Auskunft: die WKN steht dort, wie auf den übrigen DKB-Belegen.
     * Damit ist der Punkt erledigt, und dieser Testfall hält es fest.
     */
    void test_dkb_sale_wkn()
    {
        QCOMPARE(first(parseDocument(QString::fromLatin1(kDepotDkb), DocumentType::Sale,
                                     QString::fromUtf8(kDkbSale)), "Wkn"),
                 QStringLiteral("654321"));
    }

    /// Und dasselbe über den Weg der Direkten Dokumentenerfassung: erst damit
    /// findet `resolveShareGuidForDocument()` die Aktie zum Verkaufsbeleg.
    void test_capture_dkb_sale_extractWkn()
    {
        const DepotEntry* dkb = m_config.findByDepotNumber(QString::fromLatin1(kDepotDkb));
        QVERIFY(dkb);

        const DocumentEntry* doc =
            DocumentsConfig::findDocument(*dkb, DocumentType::Sale);
        QVERIFY(doc);

        QCOMPARE(DocumentClassifier::extractWkn(QString::fromUtf8(kDkbSale), *doc),
                 QStringLiteral("654321"));
    }

    // ── Direkte Dokumentenerfassung: Zuordnung zur Aktie ──────────────────
    //
    // Beim Ablegen eines Belegs per Drag&Drop sucht
    // MainWindow::resolveShareGuidForDocument() die zugehörige Aktie über
    // DocumentClassifier::extractWkn()/extractIsin() — also NICHT über den
    // ParserLib::Parser, der im Dialog läuft. Zwei Wege, dieselbe
    // Konfiguration: dass sie übereinstimmen, prüft bis zum 21.08.2026
    // niemand nach. Genau daran scheiterte Nessies DKB-Beleg (Bugreport
    // 21.08.2026): im Dialog fehlerfrei, per Drag&Drop "Keine passende Aktie
    // im Portfolio gefunden".

    /**
     * @brief Der Kern des Fehlers: die DKB wählt die WKN über die POSITION
     *        des Treffers (`FoundIndex="1"`).
     *
     * Im Belegkopf steht die Spaltenüberschrift `(WKN)`, erst darunter die
     * Kennnummer in Klammern. Wer den ersten Treffer nimmt, sucht im Portfolio
     * nach einer Aktie mit der WKN "WKN".
     */
    void test_capture_dkb_extractWknIsTheNumberNotTheHeading()
    {
        const DocumentEntry doc = dividendEntry(QString::fromLatin1(kDepotDkb));
        QCOMPARE(DocumentClassifier::extractWkn(QString::fromUtf8(kDkbUsd), doc),
                 QStringLiteral("654321"));
    }

    void test_capture_ing_extractWkn()
    {
        const DocumentEntry doc = dividendEntry(QString::fromLatin1(kDepotIng));
        QCOMPARE(DocumentClassifier::extractWkn(QString::fromUtf8(kIngUsd), doc),
                 QStringLiteral("654321"));
        QCOMPARE(DocumentClassifier::extractIsin(QString::fromUtf8(kIngUsd), doc),
                 QStringLiteral("US0001234567"));
    }

    /**
     * @brief Die eigentliche Zusicherung, und die einzige, die auch bei
     *        künftigen Regeländerungen von selbst mitwächst: BEIDE Lesewege
     *        müssen aus derselben Regel denselben Wert holen.
     *
     * Läuft über alle in Documents.xml hinterlegten Depots — kommt ein
     * viertes hinzu, ist es ohne Zutun mitgeprüft.
     */
    void test_capture_extractWknMatchesParserResult_allDepots()
    {
        const QList<DepotEntry> depots = m_config.entries();
        QVERIFY(!depots.isEmpty());

        const QMap<QString, const char*> texts {
            { QString::fromLatin1(kDepotDkb),     kDkbUsd    },
            { QString::fromLatin1(kDepotIng),     kIngUsd    },
            { QString::fromLatin1(kDepotConsors), kCortalEur },
        };

        for (const DepotEntry& depot : depots) {
            if (!texts.contains(depot.depotNumber)) {
                qWarning() << "Kein Belegtext für Depot" << depot.depotNumber
                           << "(" << depot.bankName << ") — bitte Fixture ergänzen.";
                continue;
            }

            const QString text = QString::fromUtf8(texts.value(depot.depotNumber));
            const QString viaParser =
                first(parseDividend(depot.depotNumber, text), "Wkn");
            const QString viaClassifier =
                DocumentClassifier::extractWkn(text, dividendEntry(depot.depotNumber));

            QVERIFY2(viaParser == viaClassifier,
                     qPrintable(QStringLiteral("%1 (%2): Dialog liest \"%3\", "
                                               "Dokumentenerfassung liest \"%4\"")
                                    .arg(depot.bankName, depot.depotNumber,
                                         viaParser, viaClassifier)));
        }
    }

    // ── Depoterkennung gegen echte Belege (Bugfix 25.08.2026) ─────────────
    //
    // Das eigentliche Prüfnetz der Umstellung, und der Grund, warum es hier
    // steht und nicht in tst_documentclassifier: nur dieses Testziel arbeitet
    // gegen die AUSGELIEFERTE Documents.xml und gegen Belegtexte, die
    // echten Dokumenten nachgebaut sind. Ein synthetisches Fixture kann
    // beweisen, dass die Regel stimmt — nicht, dass sie auf Nessies Belegen
    // greift.
    //
    // Siehe ARCHITECTURE.md, "Bankerkennung: Mehrdeutigkeit ueber die
    // Depotnummer".

    /**
     * @brief Jeder hinterlegte Beleg wird SEINEM Depot zugeordnet.
     *
     * Läuft über alle Depots aus Documents.xml — kommt ein viertes hinzu, ist
     * es ohne Zutun mitgeprüft und meldet sich, sobald ein Belegtext fehlt.
     */
    void test_bankDetection_everyDocumentMatchesItsOwnDepot()
    {
        const QList<DepotEntry> depots = m_config.entries();
        QVERIFY(!depots.isEmpty());

        const QMap<QString, const char*> texts {
            { QString::fromLatin1(kDepotDkb),     kDkbUsd    },
            { QString::fromLatin1(kDepotIng),     kIngUsd    },
            { QString::fromLatin1(kDepotConsors), kCortalEur },
        };

        for (const DepotEntry& depot : depots) {
            if (!texts.contains(depot.depotNumber)) {
                qWarning() << "Kein Belegtext für Depot" << depot.depotNumber
                           << "(" << depot.bankName << ") — bitte Fixture ergänzen.";
                continue;
            }

            const QString text = QString::fromUtf8(texts.value(depot.depotNumber));

            int index = -1;
            QVERIFY2(DocumentClassifier::matchDepotIndex(text, m_config, index),
                     qPrintable(QStringLiteral("Beleg von %1 (%2) wurde keinem "
                                               "Depot zugeordnet")
                                    .arg(depot.bankName, depot.depotNumber)));

            const DepotEntry matched = m_config.entries().at(index);
            QVERIFY2(matched.depotNumber == depot.depotNumber,
                     qPrintable(QStringLiteral("Beleg von %1 (%2) landete bei "
                                               "%3 (%4)")
                                    .arg(depot.bankName, depot.depotNumber,
                                         matched.bankName, matched.depotNumber)));
        }
    }

    /**
     * @brief Die Gegenprobe zum Feldfall: der Consors-Beleg darf NICHT bei
     *        der DKB landen.
     *
     * Der Test oben würde auch dann grün, wenn Consors zufällig zuerst
     * geprüft würde. Diese Prüfung nennt die falsche Zuordnung beim Namen —
     * sie ist genau das, was bis zum 25.08.2026 geschah.
     */
    void test_bankDetection_consorsDocumentIsNotAssignedToDkb()
    {
        int index = -1;
        QVERIFY(DocumentClassifier::matchDepotIndex(
            QString::fromUtf8(kCortalEur), m_config, index));

        const DepotEntry matched = m_config.entries().at(index);
        QCOMPARE(matched.bankName,    QStringLiteral("Cortal Consors"));
        QCOMPARE(matched.depotNumber, QString::fromLatin1(kDepotConsors));
    }

    /**
     * @brief Der Grund, warum die Beschriftung allein nicht genügt: die
     *        DKB-Regel TRIFFT auf dem Consors-Beleg.
     *
     * Sie fängt die ersten neun der zehn Ziffern. Vor dem Bugfix entschied
     * genau dieser Treffer, weil die DKB in Documents.xml zuerst steht. Ohne
     * diese Prüfung bliebe der Test darüber mehrdeutig — er wäre auch dann
     * grün, wenn die DKB-Regel gar nicht mehr anschlüge, und die Umstellung
     * hätte ihre Rechtfertigung verloren.
     */
    void test_bankDetection_dkbRuleStillMatchesConsorsDocument()
    {
        const DepotEntry* dkb =
            m_config.findByDepotNumber(QString::fromLatin1(kDepotDkb));
        QVERIFY(dkb);

        const QString captured = DocumentClassifier::extractFieldValue(
            QString::fromUtf8(kCortalEur), dkb->identifierRegexList,
            QStringLiteral("BankIdentifier"));

        QVERIFY(!captured.isEmpty());              // die Regel trifft …
        QVERIFY(captured != dkb->depotNumber);     // … aber nicht mit ihrer Nummer
    }

    /**
     * @brief Auch der DKB-VERKAUFSbeleg wird korrekt zugeordnet.
     *
     * Der einzige Nicht-Dividendenbeleg im Fixture-Bestand; sein Briefkopf
     * schreibt die Depotnummer eingerückt in einer Spalte, nicht am
     * Zeilenanfang.
     */
    void test_bankDetection_dkbSaleDocumentMatchesDkb()
    {
        int index = -1;
        QVERIFY(DocumentClassifier::matchDepotIndex(
            QString::fromUtf8(kDkbSale), m_config, index));
        QCOMPARE(m_config.entries().at(index).depotNumber,
                 QString::fromLatin1(kDepotDkb));
    }

    /**
     * @brief Ein Beleg aus einem nicht hinterlegten Depot wird nicht
     *        zugeordnet — auch wenn die Bank eingetragen ist.
     *
     * Nessies Vorgabe vom 25.08.2026: ein Eintrag in Documents.xml gilt für
     * genau ein Depot. Wird ein zweites eröffnet, muss die Datei erweitert
     * werden; bis dahin ist "nicht erkannt" die richtige Antwort und rote
     * Pflichtfelder sind das richtige Verhalten.
     */
    void test_bankDetection_unknownDepotNumber_notMatched()
    {
        QString foreign = QString::fromUtf8(kDkbUsd);
        foreign.replace(QString::fromLatin1(kDepotDkb), QStringLiteral("111111111"));
        QVERIFY(!foreign.contains(QString::fromLatin1(kDepotDkb)));

        int index = -9;
        QVERIFY(!DocumentClassifier::matchDepotIndex(foreign, m_config, index));
        QCOMPARE(index, -9);
    }

    /**
     * @brief Dieselbe Regel trägt bei der DKB auch die Kauf- und
     *        Verkaufsbelege — der Fehler betraf also alle drei Belegarten.
     */
    void test_capture_dkb_buyAndSaleUseTheSamePositionalRule()
    {
        const DepotEntry* dkb = m_config.findByDepotNumber(QString::fromLatin1(kDepotDkb));
        QVERIFY(dkb);

        for (const DocumentType type : { DocumentType::Buy, DocumentType::Sale }) {
            const DocumentEntry* doc = DocumentsConfig::findDocument(*dkb, type);
            QVERIFY(doc);
            QCOMPARE(doc->regexList.value(QStringLiteral("Wkn")).regexFoundPosition, 1);
        }
    }
};

QTEST_MAIN(TestDocumentsXml)
#include "tst_documentsxml.moc"

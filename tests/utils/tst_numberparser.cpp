// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// tst_numberparser.cpp — Unit tests für NumberParser (06.09.2026).
//
// Der Parser ersetzt sieben zeichengleiche Kopien in sechs Formularen, von
// denen sechs ein Tausendertrennzeichen nicht verkrafteten und das Feld
// stillschweigend auf 0,0 fallen ließen. Siehe ARCHITECTURE.md,
// "Zahlenfelder verlieren Werte ab 1.000 beim Zurücklesen".
//
// QLocale::setDefault(QLocale::German) in main(), wie in allen Testzielen
// dieses Projekts — der Parser arbeitet über QLocale(), und die CI-Runner
// laufen nicht deutsch.

#include <QtTest>
#include <QLocale>

#include "utils/NumberParser.h"

class TestNumberParser : public QObject
{
    Q_OBJECT

private slots:
    // ── Der Fehler, um den es geht ────────────────────────────────────────

    void test_parse_thousandsSeparatorIsRead()
    {
        // Regression: die alten Kopien machten aus "1.003,00" die Zeichenkette
        // "1.003.00" und lieferten 0,0 — ein Kurs über tausend Euro fiel beim
        // Speichern lautlos auf null.
        bool ok = false;
        QCOMPARE(NumberParser::parse(QStringLiteral("1.003,00"), &ok), 1003.0);
        QVERIFY(ok);
    }

    void test_parse_millionsWithTwoSeparators()
    {
        QCOMPARE(NumberParser::parse(QStringLiteral("1.234.567,89")), 1234567.89);
    }

    // ── Normalfälle ───────────────────────────────────────────────────────

    void test_parse_germanDecimal()
    {
        QCOMPARE(NumberParser::parse(QStringLiteral("48,5950")), 48.595);
    }

    void test_parse_plainInteger()
    {
        QCOMPARE(NumberParser::parse(QStringLiteral("50")), 50.0);
    }

    void test_parse_withoutGroupingIsAlsoValid()
    {
        // Gruppierung ist bei der Eingabe erlaubt, aber nicht verlangt —
        // ValueFormatter::formatPriceForInput() schreibt bewusst ohne.
        QCOMPARE(NumberParser::parse(QStringLiteral("1003,0000")), 1003.0);
    }

    void test_parse_trimsSurroundingWhitespace()
    {
        QCOMPARE(NumberParser::parse(QStringLiteral("  12,50  ")), 12.5);
    }

    void test_parse_negativeValue()
    {
        QCOMPARE(NumberParser::parse(QStringLiteral("-5,25")), -5.25);
    }

    // ── Leeres Feld ist kein Fehler ───────────────────────────────────────

    void test_parse_emptyIsZeroAndOk()
    {
        // Bei den optionalen Feldern — Gebühren, Steuern, Rabatt — bedeutet
        // "nichts eingetragen" null und darf keine Meldung auslösen.
        // Fehlende Pflichtfelder fängt hasMissingRequiredFields() ab.
        bool ok = false;
        QCOMPARE(NumberParser::parse(QString(), &ok), 0.0);
        QVERIFY(ok);
    }

    void test_parse_whitespaceOnlyIsZeroAndOk()
    {
        bool ok = false;
        QCOMPARE(NumberParser::parse(QStringLiteral("   "), &ok), 0.0);
        QVERIFY(ok);
    }

    // ── Streng deutsch: Mehrdeutiges wird gemeldet, nicht geraten ─────────

    void test_parse_cStyleDecimalIsRejected()
    {
        // "204.71" ist keine gültige deutsche Zahl: als Gruppierung gelesen
        // müssten hinter dem Punkt drei Ziffern stehen. Die alte Umwandlung
        // machte daraus 204,71 — bequem, aber sie öffnete die Tür für den
        // umgekehrten Fehler bei "1.003". C-formatierte Belegwerte übersetzt
        // DocumentFieldValue::forNumericField() vorher.
        bool ok = true;
        QCOMPARE(NumberParser::parse(QStringLiteral("204.71"), &ok), 0.0);
        QVERIFY(!ok);
    }

    void test_parse_malformedGroupingIsRejected()
    {
        bool ok = true;
        QCOMPARE(NumberParser::parse(QStringLiteral("1.5"), &ok), 0.0);
        QVERIFY(!ok);
    }

    void test_parse_lettersAreRejected()
    {
        bool ok = true;
        QCOMPARE(NumberParser::parse(QStringLiteral("abc"), &ok), 0.0);
        QVERIFY(!ok);
    }

    void test_parse_trailingUnitIsRejected()
    {
        // Ein aus einer Tabelle kopiertes "48,59 €" ist keine Zahl. Vorher
        // lieferte das ebenfalls 0,0 — jetzt ist der Fehlschlag meldbar.
        bool ok = true;
        QCOMPARE(NumberParser::parse(QStringLiteral("48,59 €"), &ok), 0.0);
        QVERIFY(!ok);
    }

    void test_parse_okPointerIsOptional()
    {
        // Alle heutigen Aufrufstellen übergeben kein Flag; der Aufruf darf
        // deshalb nicht abstürzen.
        QCOMPARE(NumberParser::parse(QStringLiteral("7,00")), 7.0);
    }

    // ── Zusammenspiel mit ValueFormatter ──────────────────────────────────

    void test_parse_roundTripWithFormatPriceForInput()
    {
        // Was die Anwendung in ein Eingabefeld schreibt, muss sie auch
        // wieder lesen können. Genau dieser Kreis war offen.
        const double original = 1003.0;
        bool ok = false;
        const QString written = QLocale().toString(original, 'f', 4);  // "1.003,0000"
        QCOMPARE(NumberParser::parse(written, &ok), original);
        QVERIFY(ok);
    }
};

int main(int argc, char* argv[])
{
    // Kein QCoreApplication nötig — zustandslos, kein Qt SQL, keine Widgets
    // (gleiche Bauweise wie tst_valueformatter und tst_sharesplithint).
    QLocale::setDefault(QLocale::German);

    TestNumberParser t;
    return QTest::qExec(&t, argc, argv);
}

#include "tst_numberparser.moc"

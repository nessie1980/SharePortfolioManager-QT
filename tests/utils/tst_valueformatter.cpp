// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// tst_valueformatter.cpp — Unit tests für ValueFormatter (05.09.2026).
//
// Der Helfer ist absichtlich winzig, die Tests sind es auch. Sie halten die
// eine Eigenschaft fest, um die es geht: ein Kurs wird mit VIER
// Nachkommastellen angezeigt, nicht mit zwei. Vor dieser Änderung war das an
// jeder Aufrufstelle einzeln entschieden und an mehreren Stellen falsch —
// siehe ARCHITECTURE.md, "Kurs-Anzeige durchgängig mit vier
// Nachkommastellen".
//
// main() setzt QLocale::setDefault(QLocale::German), wie alle anderen
// Testziele dieses Projekts: ValueFormatter formatiert über QLocale(), und
// die CI-Runner laufen nicht mit deutschem Gebietsschema. Siehe
// ARCHITECTURE.md, "System-Locale-abhängiges Zahlenformat".

#include <QtTest>
#include <QLocale>

#include "utils/ValueFormatter.h"

class TestValueFormatter : public QObject
{
    Q_OBJECT

private slots:
    // ── formatPrice ───────────────────────────────────────────────────────

    void test_formatPrice_hasFourDecimals()
    {
        QCOMPARE(ValueFormatter::formatPrice(48.595), QStringLiteral("48,5950"));
    }

    void test_formatPrice_padsShortValues()
    {
        // Ein glatter Kurs bekommt dieselbe Breite wie ein krummer — die
        // Spalte soll nicht je Zeile springen.
        QCOMPARE(ValueFormatter::formatPrice(50.0), QStringLiteral("50,0000"));
    }

    void test_formatPrice_doesNotRoundAwayTheFourthDecimal()
    {
        // Der Feldfall aus ARCHITECTURE.md: mit zwei Stellen stand hier
        // 48,59, und 200 × 48,59 ergab sichtbar nicht die angezeigte Summe.
        const QString text = ValueFormatter::formatPrice(48.595);
        QVERIFY2(text.endsWith(QStringLiteral("5950")), qPrintable(text));
    }

    void test_formatPrice_usesGroupSeparatorForLargeValues()
    {
        // Vierstellige Kurse gibt es (Alphabet vor dem Split lag über 1.000 €).
        QCOMPARE(ValueFormatter::formatPrice(1003.0), QStringLiteral("1.003,0000"));
    }

    void test_formatPrice_negativeKeepsSign()
    {
        // Kursdifferenzen (Vortagsentwicklung) laufen über dieselbe Funktion
        // und können negativ sein. Ein etwaiges "+" setzt die Aufrufstelle,
        // das "−" kommt von QLocale selbst.
        QCOMPARE(ValueFormatter::formatPrice(-5.25), QStringLiteral("-5,2500"));
    }

    void test_formatPrice_zeroIsNotEmpty()
    {
        QCOMPARE(ValueFormatter::formatPrice(0.0), QStringLiteral("0,0000"));
    }

    void test_formatPrice_hasNoUnitSuffix()
    {
        // Die Einheit hängt die Aufrufstelle an — in den Tabellen steht
        // " €", im Chart-Tooltip "€" ohne Leerzeichen.
        QVERIFY(!ValueFormatter::formatPrice(12.5).contains(QStringLiteral("€")));
    }

    // ── formatExchangeRate ────────────────────────────────────────────────

    void test_formatExchangeRate_hasFourDecimals()
    {
        QCOMPARE(ValueFormatter::formatExchangeRate(1.0834), QStringLiteral("1,0834"));
    }

    void test_formatExchangeRate_neutralRatioIsPadded()
    {
        // 1,0000 ist der Vorgabewert im Dividendenformular, wenn keine
        // Fremdwährung im Spiel ist.
        QCOMPARE(ValueFormatter::formatExchangeRate(1.0), QStringLiteral("1,0000"));
    }
};

int main(int argc, char* argv[])
{
    // Kein QCoreApplication nötig — der Helfer ist zustandslos, greift nicht
    // auf Qt SQL zu und instanziiert keine Widgets (gleiche Bauweise wie
    // tst_sharesplithint). QLocale::setDefault() wirkt auch ohne
    // Applikationsobjekt.
    QLocale::setDefault(QLocale::German);

    TestValueFormatter t;
    return QTest::qExec(&t, argc, argv);
}

#include "tst_valueformatter.moc"

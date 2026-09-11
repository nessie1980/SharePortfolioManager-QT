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

    // ── formatPriceForInput ───────────────────────────────────────────────

    void test_formatPriceForInput_omitsGroupSeparator()
    {
        // Der Unterschied zu formatPrice(): kein Tausendertrennzeichen.
        // In einem Eingabefeld waere es ein Zeichen, das die Gegenrichtung
        // wieder entfernen muesste — und genau das tut parseDouble() nicht.
        QCOMPARE(ValueFormatter::formatPriceForInput(1003.0),
                 QStringLiteral("1003,0000"));
    }

    void test_formatPriceForInput_keepsGermanDecimalSeparator()
    {
        // Nur das Gruppierungszeichen faellt weg, das Dezimalkomma bleibt.
        QCOMPARE(ValueFormatter::formatPriceForInput(204.715),
                 QStringLiteral("204,7150"));
    }

    void test_formatPriceForInput_matchesFormatPriceBelowThousand()
    {
        // Unterhalb von 1.000 gibt es nichts zu gruppieren — beide
        // Funktionen liefern dasselbe.
        QCOMPARE(ValueFormatter::formatPriceForInput(48.595),
                 ValueFormatter::formatPrice(48.595));
    }

    void test_formatPriceForInput_doesNotLeakOptionIntoDefaultLocale()
    {
        // Die NumberOption wird auf einer lokalen Kopie gesetzt. Wuerde sie
        // die Standard-Locale veraendern, verloeren alle Tabellen der
        // Anwendung ihr Tausendertrennzeichen.
        ValueFormatter::formatPriceForInput(1003.0);
        QCOMPARE(ValueFormatter::formatPrice(1003.0),
                 QStringLiteral("1.003,0000"));
    }

    void test_formatForInput_omitsGroupSeparatorForMoney()
    {
        // Zwei Nachkommastellen fuer Geldbetraege in Eingabefeldern
        // (08.09.2026, Ausweitung auf alle editierbaren Zahlenfelder).
        QCOMPARE(ValueFormatter::formatForInput(1234.56, 2),
                 QStringLiteral("1234,56"));
    }

    void test_formatForInput_omitsGroupSeparatorForVolume()
    {
        QCOMPARE(ValueFormatter::formatForInput(2500.0, 4),
                 QStringLiteral("2500,0000"));
    }

    void test_formatPriceForInput_isFormatForInputWithFourDecimals()
    {
        // formatPriceForInput() ist seit 1.21.6 nur noch die benannte
        // Kurzschreibweise — die beiden duerfen nicht auseinanderlaufen.
        QCOMPARE(ValueFormatter::formatPriceForInput(1003.5),
                 ValueFormatter::formatForInput(1003.5, 4));
    }

    void test_formatExchangeRate_omitsGroupSeparator()
    {
        // Der Devisenkurs steht in einem Eingabefeld; vierstellige Kurse
        // gibt es (indonesischer Rupiah).
        QCOMPARE(ValueFormatter::formatExchangeRate(16250.0),
                 QStringLiteral("16250,0000"));
    }

    // ── formatMoney / formatVolume / formatPercent ────────────────────────

    void test_formatMoney_hasTwoDecimals()
    {
        QCOMPARE(ValueFormatter::formatMoney(1234.5), QStringLiteral("1.234,50"));
    }

    void test_formatMoney_keepsGroupSeparator()
    {
        // Geldbetraege stehen in Tabellen und Anzeigefeldern — dort ist das
        // Tausendertrennzeichen eine Lesehilfe. Nur in EINGABEfeldern nicht,
        // dafuer gibt es formatForInput().
        QVERIFY(ValueFormatter::formatMoney(9999.99).contains(QLatin1Char('.')));
    }

    void test_formatVolume_hasFourDecimals()
    {
        // Der Anlass der Zusammenlegung: PresenterShareDetails hatte ein
        // eigenes formatVolume() mit ZWEI Stellen, waehrend alle anderen vier
        // zeigten. Ein Fondsbestand von 168,50796 Anteilen erschien dort als
        // 168,51 — in einer Box, die "Anteile mal Kurs" rechnet.
        QCOMPARE(ValueFormatter::formatVolume(168.50796),
                 QStringLiteral("168,5080"));
    }

    void test_formatPercent_hasTwoDecimals()
    {
        QCOMPARE(ValueFormatter::formatPercent(3.456), QStringLiteral("3,46"));
    }

    void test_formatMoney_hasNoUnitSuffix()
    {
        // Wie bei formatPrice(): die Einheit haengt die Aufrufstelle an,
        // weil sie sich je Tabelle unterscheidet (" €", " stk.", " %").
        QVERIFY(!ValueFormatter::formatMoney(5.0).contains(QStringLiteral("€")));
        QVERIFY(!ValueFormatter::formatVolume(5.0).contains(QStringLiteral("stk")));
        QVERIFY(!ValueFormatter::formatPercent(5.0).contains(QLatin1Char('%')));
    }

    void test_formatMoney_andFormatPriceDifferInPrecision()
    {
        // Die fachliche Trennung, um die es in dieser Reihe ging: ein Kurs
        // ist kein Geldbetrag.
        QVERIFY(ValueFormatter::formatMoney(48.595)
                != ValueFormatter::formatPrice(48.595));
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

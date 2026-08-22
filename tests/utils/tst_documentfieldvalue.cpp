// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include <QtTest>

#include "../../app/utils/DocumentFieldValue.h"

/**
 * @brief Unit tests für DocumentFieldValue.
 *
 * Die Umwandlung der Rohwerte aus `Documents.xml` in Widget-taugliche Werte
 * (siehe ARCHITECTURE.md, "Rohwerte aus Belegen: eine Regel je Zieltyp").
 *
 * Anlass sind zwei Feldfälle vom 22.08.2026:
 *
 *  - Die Ordernummer "670835/66.00" wurde zu "670835/66,00" — die Views
 *    behandelten jedes einzeilige Feld als Zahlenfeld.
 *  - Der DKB-Verkaufsbeleg beschriftet Datum und Uhrzeit gemeinsam
 *    ("Schlusstag/-Zeit  27.02.2020 19:16:37"); der ganze Fang landete in
 *    `QDate::fromString(…, "d.M.yyyy")`, schlug fehl, und im Formular blieb
 *    still das HEUTIGE Datum stehen.
 *
 * Reine Funktionen, kein Widget, keine Datenbank — `QTEST_APPLESS_MAIN` wie
 * bei `tst_shareupdaterules`.
 */
class TestDocumentFieldValue : public QObject
{
    Q_OBJECT

private slots:

    // ── toDate() ──────────────────────────────────────────────────────────

    void test_toDate_plainGermanDate()
    {
        QCOMPARE(DocumentFieldValue::toDate(QStringLiteral("27.02.2020")),
                 QDate(2020, 2, 27));
    }

    void test_toDate_singleDigitDayAndMonth()
    {
        QCOMPARE(DocumentFieldValue::toDate(QStringLiteral("8.5.2026")),
                 QDate(2026, 5, 8));
    }

    void test_toDate_isoFormat()
    {
        QCOMPARE(DocumentFieldValue::toDate(QStringLiteral("2020-02-27")),
                 QDate(2020, 2, 27));
    }

    /**
     * @brief Der eigentliche Feldfall: Datum UND Uhrzeit in einem Fang.
     *
     * Der DKB-Verkaufsbeleg beschriftet beides gemeinsam mit
     * "Schlusstag/-Zeit"; die Regeln `Date` und `Time` fangen deshalb
     * denselben Text, jedes Feld holt sich seinen Teil.
     */
    void test_toDate_combinedDateAndTime()
    {
        QCOMPARE(DocumentFieldValue::toDate(
                     QStringLiteral(" 27.02.2020 19:16:37 ")),
                 QDate(2020, 2, 27));
    }

    void test_toDate_surroundedByText()
    {
        QCOMPARE(DocumentFieldValue::toDate(
                     QStringLiteral("Schlusstag/-Zeit 27.02.2020 19:16:37")),
                 QDate(2020, 2, 27));
    }

    /**
     * @brief Zweistellige Jahreszahlen werden NICHT geraten.
     *
     * "27.02.20" wäre 1920 oder 2020. Ein falsches Jahrhundert im Kaufdatum
     * verschiebt die gesamte FIFO-Zuordnung; ein gemeldeter Fehlschlag ist
     * das kleinere Übel.
     */
    void test_toDate_twoDigitYear_isRejected()
    {
        QVERIFY(!DocumentFieldValue::toDate(QStringLiteral("27.02.20")).isValid());
    }

    void test_toDate_impossibleDate_isRejected()
    {
        QVERIFY(!DocumentFieldValue::toDate(QStringLiteral("31.02.2020")).isValid());
    }

    void test_toDate_noDateAtAll_isInvalid()
    {
        QVERIFY(!DocumentFieldValue::toDate(QStringLiteral("Schlusstag/-Zeit")).isValid());
        QVERIFY(!DocumentFieldValue::toDate(QString()).isValid());
    }

    // ── toTime() ──────────────────────────────────────────────────────────

    void test_toTime_plain()
    {
        QCOMPARE(DocumentFieldValue::toTime(QStringLiteral("19:16:37")),
                 QTime(19, 16, 37));
    }

    void test_toTime_withoutSeconds()
    {
        QCOMPARE(DocumentFieldValue::toTime(QStringLiteral("19:16")),
                 QTime(19, 16, 0));
    }

    /// Gegenstück zu test_toDate_combinedDateAndTime(): derselbe Rohwert,
    /// anderes Zielfeld.
    void test_toTime_combinedDateAndTime()
    {
        QCOMPARE(DocumentFieldValue::toTime(
                     QStringLiteral(" 27.02.2020 19:16:37 ")),
                 QTime(19, 16, 37));
    }

    void test_toTime_noTimeAtAll_isInvalid()
    {
        QVERIFY(!DocumentFieldValue::toTime(QStringLiteral("27.02.2020")).isValid());
        QVERIFY(!DocumentFieldValue::toTime(QString()).isValid());
    }

    // ── forTextField() ────────────────────────────────────────────────────

    /**
     * @brief Der Punkt in einer Ordernummer ist ein Punkt.
     *
     * Nessies Bugreport 22.08.2026: der Beleg zeigt "670835/66.00", das
     * Formular machte "670835/66,00" daraus. Eine Ordernummer ist keine
     * Zahl — was die Bank druckt, gehört unverändert übernommen.
     */
    void test_forTextField_keepsDotInOrderNumber()
    {
        QCOMPARE(DocumentFieldValue::forTextField(QStringLiteral(" 670835/66.00\n")),
                 QStringLiteral("670835/66.00"));
    }

    void test_forTextField_keepsDotsInUrl()
    {
        QCOMPARE(DocumentFieldValue::forTextField(
                     QStringLiteral("https://www.example.com/kurse")),
                 QStringLiteral("https://www.example.com/kurse"));
    }

    void test_forTextField_collapsesWhitespaceFromPdftotext()
    {
        QCOMPARE(DocumentFieldValue::forTextField(
                     QStringLiteral("  MUSTER\n  AG   \r\n")),
                 QStringLiteral("MUSTER AG"));
    }

    // ── forNumericField() ─────────────────────────────────────────────────

    /// Kein Komma im Fang → der Punkt ist der Dezimaltrenner.
    void test_forNumericField_dotBecomesComma()
    {
        QCOMPARE(DocumentFieldValue::forNumericField(QStringLiteral(" 39.998 ")),
                 QStringLiteral("39,998"));
    }

    /// Bereits deutsche Schreibweise → unverändert.
    void test_forNumericField_germanValueUnchanged()
    {
        QCOMPARE(DocumentFieldValue::forNumericField(QStringLiteral(" 51,47 ")),
                 QStringLiteral("51,47"));
    }

    /**
     * @brief Komma im Fang → der Punkt kann nur Tausendertrenner sein.
     *
     * Neu am 22.08.2026. Vorher wurde auch hier der Punkt zum Komma, aus
     * "1.234,56" wurde "1,234,56", und `parseDouble()` machte daraus 0,00 —
     * ein Betrag über tausend Euro fiel lautlos auf null.
     */
    void test_forNumericField_thousandsSeparatorIsRemoved()
    {
        QCOMPARE(DocumentFieldValue::forNumericField(QStringLiteral("1.234,56")),
                 QStringLiteral("1234,56"));
        QCOMPARE(DocumentFieldValue::forNumericField(QStringLiteral("1.234.567,89")),
                 QStringLiteral("1234567,89"));
    }

    void test_forNumericField_stripsNewlines()
    {
        QCOMPARE(DocumentFieldValue::forNumericField(QStringLiteral("\n 40 \n")),
                 QStringLiteral("40"));
    }
};

QTEST_APPLESS_MAIN(TestDocumentFieldValue)
#include "tst_documentfieldvalue.moc"

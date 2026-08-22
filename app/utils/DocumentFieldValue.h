// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QString>
#include <QDate>
#include <QTime>
#include <QRegularExpression>

/**
 * @brief Wandelt die ROHWERTE aus `Documents.xml` in Widget-taugliche Werte.
 *
 * Was ein Regex aus einem Bankbeleg herausschneidet, ist selten schon das,
 * was in ein Eingabefeld gehört: da hängen Leerzeichen dran, ein Zeilenumbruch
 * von `pdftotext`, oder es steht mehr im Fang als das eine Feld. Genau daran
 * scheiterten zwei Feldfälle vom 22.08.2026 (siehe ARCHITECTURE.md,
 * "Rohwerte aus Belegen: eine Regel je Zieltyp"):
 *
 *  - Der DKB-Verkaufsbeleg beschriftet Datum UND Uhrzeit gemeinsam
 *    ("Schlusstag/-Zeit  27.02.2020 19:16:37"). Die Regeln `Date` und `Time`
 *    fangen deshalb DENSELBEN Text; jedes Feld muss sich seinen Teil daraus
 *    holen. Vorher wurde der ganze Fang an `QDate::fromString(…, "d.M.yyyy")`
 *    gereicht, schlug fehl, und im Formular blieb still das HEUTIGE Datum
 *    stehen — bei einem Verkauf aus dem Jahr 2020 kein Schönheitsfehler.
 *  - Die Ordernummer "670835/66.00" wurde zu "670835/66,00", weil die Views
 *    jedem einzeiligen Textfeld die Dezimalpunkt-Umschreibung verpassten.
 *
 * Deshalb hier: eine Umwandlung je ZIELTYP, an einer Stelle, für alle vier
 * Formulare (Kauf, Verkauf, Dividende, Aktie anlegen).
 *
 * Header-only wie `ShareUpdateRules.h` — reine Funktionen, kein Zustand, kein
 * Qt-Objekt, kein zusätzliches CMake-Ziel nötig.
 */
namespace DocumentFieldValue {

/**
 * @brief Erste Datumsangabe in @p raw, gleichgültig was drum herum steht.
 * @return Ungültiges `QDate`, wenn keine gefunden wurde — der Aufrufer muss
 *         das ANZEIGEN und darf nicht stillschweigend beim Vorgabewert
 *         bleiben.
 *
 * Erkannt werden `T.M.JJJJ` (auch einstellig) und ISO `JJJJ-MM-TT`. Zweistellige
 * Jahreszahlen bleiben bewusst aussen vor: "27.02.20" wäre 1920 oder 2020, und
 * ein geratenes Jahrhundert ist schlimmer als ein gemeldeter Fehlschlag.
 */
inline QDate toDate(const QString& raw)
{
    const QString text = raw.trimmed();
    if (text.isEmpty())
        return QDate();

    // Häufigster Fall zuerst: der Fang enthält NUR das Datum.
    QDate d = QDate::fromString(text, QStringLiteral("d.M.yyyy"));
    if (d.isValid())
        return d;
    d = QDate::fromString(text, Qt::ISODate);
    if (d.isValid())
        return d;

    static const QRegularExpression german(
        QStringLiteral("(\\d{1,2})\\.(\\d{1,2})\\.(\\d{4})"));
    const QRegularExpressionMatch gm = german.match(text);
    if (gm.hasMatch()) {
        d = QDate(gm.captured(3).toInt(), gm.captured(2).toInt(),
                  gm.captured(1).toInt());
        if (d.isValid())
            return d;
    }

    static const QRegularExpression iso(
        QStringLiteral("(\\d{4})-(\\d{1,2})-(\\d{1,2})"));
    const QRegularExpressionMatch im = iso.match(text);
    if (im.hasMatch()) {
        d = QDate(im.captured(1).toInt(), im.captured(2).toInt(),
                  im.captured(3).toInt());
        if (d.isValid())
            return d;
    }

    return QDate();
}

/**
 * @brief Erste Uhrzeit in @p raw, gleichgültig was drum herum steht.
 * @return Ungültiges `QTime`, wenn keine gefunden wurde.
 *
 * Sekunden sind optional (`19:16` gilt ebenso wie `19:16:37`).
 */
inline QTime toTime(const QString& raw)
{
    const QString text = raw.trimmed();
    if (text.isEmpty())
        return QTime();

    QTime t = QTime::fromString(text, QStringLiteral("h:m:s"));
    if (t.isValid())
        return t;
    t = QTime::fromString(text, QStringLiteral("h:m"));
    if (t.isValid())
        return t;

    static const QRegularExpression clock(
        QStringLiteral("(\\d{1,2}):(\\d{2})(?::(\\d{2}))?"));
    const QRegularExpressionMatch m = clock.match(text);
    if (!m.hasMatch())
        return QTime();

    t = QTime(m.captured(1).toInt(), m.captured(2).toInt(),
              m.captured(3).isEmpty() ? 0 : m.captured(3).toInt());
    return t.isValid() ? t : QTime();
}

/**
 * @brief Bereinigt einen Rohwert für ein einzeiliges TEXTfeld — ohne jede
 *        Umdeutung von Zeichen.
 *
 * `pdftotext` bringt Zeilenumbrüche und Mehrfach-Leerzeichen mit; die kommen
 * weg. Alles andere bleibt ZEICHENGETREU stehen. Insbesondere der Punkt: eine
 * Ordernummer wie "670835/66.00" ist keine Zahl, und was die Bank druckt,
 * gehört unverändert übernommen.
 */
inline QString forTextField(const QString& raw)
{
    QString clean = raw;
    clean.replace(QLatin1Char('\n'), QLatin1Char(' '));
    clean.replace(QLatin1Char('\r'), QLatin1Char(' '));
    return clean.simplified();   // fasst Leerraum zusammen und trimmt
}

/**
 * @brief Bereinigt einen Rohwert für ein ZAHLENfeld und bringt ihn auf die
 *        deutsche Schreibweise, die die Formulare erwarten (Komma als
 *        Dezimaltrenner, kein Tausendertrenner).
 *
 * Zwei Fälle, unterschieden am Komma:
 *
 *  - Der Fang enthält ein Komma → das Komma IST der Dezimaltrenner, ein Punkt
 *    kann dann nur Tausendertrenner sein und fliegt raus.
 *    "1.234,56" → "1234,56"
 *  - Kein Komma, aber ein Punkt → der Punkt ist der Dezimaltrenner und wird
 *    zum Komma. "39.998" → "39,998"
 *
 * @note Der erste Fall ist neu (22.08.2026). Vorher wurde der Punkt auch dort
 * zum Komma, aus "1.234,56" wurde "1,234,56", und `parseDouble()` machte
 * daraus 0,00 — ein Preis über tausend Euro fiel damit lautlos auf null.
 * Aufgefallen ist das beim Beheben der Ordernummer; ein Beleg mit einem
 * solchen Betrag lag nicht vor.
 */
inline QString forNumericField(const QString& raw)
{
    QString clean = forTextField(raw);

    if (clean.contains(QLatin1Char(',')))
        clean.remove(QLatin1Char('.'));
    else
        clean.replace(QLatin1Char('.'), QLatin1Char(','));

    return clean;
}

} // namespace DocumentFieldValue

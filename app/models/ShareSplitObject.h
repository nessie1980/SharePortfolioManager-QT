// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QString>
#include <QDate>
#include <QLocale>

/**
 * @brief Represents a single stock split (or reverse split) of a share.
 *
 * Grundlage für die Aktiensplit-Behandlung — siehe ARCHITECTURE.md,
 * "Offene Punkte", "Aktiensplits werden nicht behandelt", sowie
 * "ShareSplitObject / ShareSplitRepository / ShareSplitAdjuster".
 *
 * Die Datenbank speichert durchgehend die Beleg-Wahrheit: `buys`, `sales`
 * und `daily_values` werden bei einem erfassten Split NICHT umgeschrieben.
 * Die Umrechnung auf heutige Stücke passiert ausschliesslich zur Laufzeit
 * über `ShareSplitAdjuster`, mit den hier gespeicherten Splits als
 * Eingabe.
 *
 * ### Verhältnis
 *
 * `ratioNew`/`ratioOld` bilden das Split-Verhältnis ab: ein 20:1-Split
 * (z. B. Alphabet Inc. Cl. A, 18.07.2022) ist `ratioNew=20, ratioOld=1`,
 * ein Reverse-Split 1:10 ist `ratioNew=1, ratioOld=10`. `factor()` liefert
 * daraus den Vervielfachungsfaktor der Stückzahl.
 *
 * ### prices_adjusted je Split
 *
 * `pricesAdjusted` ist bewusst ein Feld je Split, nicht je Aktie
 * (Nessies Entscheidung 07.08.2026) — bei mehreren Splits derselben Aktie
 * kann die Kurshistorie unterschiedlich weit bereinigt vorliegen, je
 * nachdem, wann und mit welchem Datenanbieter sie zuletzt abgerufen wurde.
 */
class ShareSplitObject
{
public:
    ShareSplitObject() = default;

    /**
     * @brief Full constructor.
     * @param guid            Unique identifier (UUID string).
     * @param shareGuid       GUID of the parent share.
     * @param date            Ex-Tag des Splits (erster Handelstag danach).
     * @param ratioNew        Neue Stückzahl-Seite des Verhältnisses (> 0).
     * @param ratioOld        Alte Stückzahl-Seite des Verhältnisses (> 0).
     * @param pricesAdjusted  true, wenn die in `daily_values` gespeicherte
     *        Kurshistorie vor @p date bereits split-bereinigt vorliegt.
     * @param comment         Freitext, z. B. Quelle oder Anlass des Splits.
     */
    ShareSplitObject(const QString& guid,
                     const QString& shareGuid,
                     const QDate&   date,
                     double ratioNew,
                     double ratioOld,
                     bool   pricesAdjusted = false,
                     const QString& comment = QString());

    // ── Identity ──────────────────────────────────────────────────────────
    QString guid()      const { return m_guid; }      ///< Unique identifier
    QString shareGuid() const { return m_shareGuid; }  ///< Parent share GUID

    // ── Date ──────────────────────────────────────────────────────────────
    QDate   date()      const { return m_date; }                                           ///< Ex-Tag des Splits
    QString dateAsStr() const { return QLocale().toString(m_date, QLocale::ShortFormat); } ///< Datum formatiert für die Anzeige

    // ── Ratio ─────────────────────────────────────────────────────────────
    double ratioNew() const { return m_ratioNew; } ///< Neue Stückzahl-Seite
    double ratioOld() const { return m_ratioOld; } ///< Alte Stückzahl-Seite

    /**
     * @brief Vervielfachungsfaktor der Stückzahl (`ratioNew / ratioOld`).
     *
     * 20:1-Split → 20.0. Reverse-Split 1:10 → 0.1. Bei `ratioOld <= 0`
     * (sollte durch die Datenbank-CHECK-Constraint bereits ausgeschlossen
     * sein) liefert die Methode defensiv 1.0 statt zu dividieren.
     */
    double factor() const { return (m_ratioOld > 0.0) ? (m_ratioNew / m_ratioOld) : 1.0; }

    // ── Kurshistorie ──────────────────────────────────────────────────────
    bool pricesAdjusted() const { return m_pricesAdjusted; }             ///< Siehe Klassendoku
    void setPricesAdjusted(bool value) { m_pricesAdjusted = value; }

    // ── Comment ───────────────────────────────────────────────────────────
    QString comment() const { return m_comment; }
    void    setComment(const QString& value) { m_comment = value; }

    // ── Validity ──────────────────────────────────────────────────────────
    /**
     * @brief Returns true if the object contains valid data.
     *
     * Ein ShareSplitObject ist gültig, wenn guid und shareGuid gesetzt,
     * date gültig und beide Verhältnis-Seiten positiv sind.
     * @return true if valid.
     */
    bool isValid() const
    {
        return !m_guid.isEmpty() && !m_shareGuid.isEmpty() && m_date.isValid()
            && m_ratioNew > 0.0 && m_ratioOld > 0.0;
    }

private:
    QString m_guid;
    QString m_shareGuid;
    QDate   m_date;

    double  m_ratioNew = 1.0;
    double  m_ratioOld = 1.0;

    bool    m_pricesAdjusted = false;
    QString m_comment;
};

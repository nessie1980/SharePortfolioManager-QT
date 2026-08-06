// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QDate>
#include <QList>
#include <QString>
#include <QStringList>

/**
 * @brief Zeitaufgelöste Gewinn-/Verlustkurve über das gesamte Portfolio.
 *
 * Der Rechenkern hinter dem Depotwert-Chart (Feature 05.08.2026). Er liefert
 * für jeden Stichtag eines Datumsrasters die kumulierte Wertentwicklung des
 * Portfolios in Euro und Prozent.
 *
 * @note Die Kurve ist bewusst KEINE Vermögenskurve. Sie bewegt sich
 * ausschliesslich durch Dinge, die echten Wert schaffen oder vernichten:
 * Kursänderungen, Dividenden, realisierte Verkaufsgewinne und Kosten. Ein
 * Kauf über 5.000 Euro verschiebt die Linie nicht, weil Einzahlen kein Gewinn
 * ist. Nach Nessies Vorgabe (05.08.2026) — die Vermögenskurve mit Sprüngen bei
 * Ein- und Auszahlungen ist als eigenes Feature vorgemerkt, siehe
 * ARCHITECTURE.md, "Offene Punkte".
 *
 * ### Formel je Stichtag t
 *
 * Linie(t) = Bestandswert(t) + realisierter Gewinn(t) + Dividenden(t)
 *            - Kosten(t) - Kaufwert der gehaltenen Anteile(t)
 *
 * mit, jeweils über alle Aktien summiert:
 *
 * | Term | Definition |
 * |------|------------|
 * | Bestandswert(t) | gehaltene Stück(t) x Schlusskurs(t) |
 * | realisierter Gewinn(t) | Summe über Verkäufe bis t: (Verkaufswert - Steuer) - Kaufwert der verkauften Anteile |
 * | Dividenden(t) | Summe der Netto-Dividenden bis t (brutto abzüglich Steuern) |
 * | Kosten(t) | Summe aller Gebühren bis t, abzüglich Rabatt |
 * | Kaufwert gehalten(t) | Summe über die zum Stichtag noch gehaltenen Lots: Restmenge x Kaufkurs |
 *
 * @note Steuern werden direkt am Zahlungsstrom abgezogen (Dividende brutto
 * minus Steuern, Verkaufswert minus Steuer), Gebühren sammeln sich im
 * Kosten-Term. Nessies Vorgabe vom 05.08.2026. Dividenden haben fachlich keine
 * Gebühren, entsprechend kennt DividendObject auch kein Gebührenfeld.
 *
 * @note Der Kosten-Term umfasst alle Brokerage-Einträge einer Aktie, also auch
 * freistehende Kosteneinträge ohne Bezug zu einem Kauf oder Verkauf. Dadurch
 * weicht die Linie am heutigen Stichtag von der Footer-Spalte "Komplette
 * Entwicklung" ab, solange dort der Bug besteht, dass
 * ShareCalculator::compute() freistehende Kosten nur in totalBrokerage, nicht
 * aber in completePurchase berücksichtigt. Von Nessie am 05.08.2026 bestätigt:
 * der Chart rechnet korrekt, der Footer-Fix folgt separat, danach stimmen
 * beide automatisch überein. Siehe ARCHITECTURE.md, "Offene Punkte".
 *
 * ### FIFO
 *
 * Verkäufe werden intern FIFO abgespielt: was zuerst gekauft wurde, wird
 * zuerst verkauft. Die gespeicherten SaleBuyDetail-Zuordnungen werden bewusst
 * NICHT verwendet — sie geben nur den heutigen Stand her, für historische
 * Stichtage muss die Zuordnung ohnehin rekonstruiert werden, und laut
 * Kommentar in ShareCalculator::compute() können sie unvollständig oder leer
 * gespeichert sein.
 *
 * ### Aktien ohne Tageswert-Historie
 *
 * Eine Aktie ohne einen einzigen Eintrag in daily_values (Update-Typ "Nur
 * Kurs" oder "Kein Update") kann zu keinem Stichtag bewertet werden. Sie wird
 * vollständig aus der Berechnung ausgeschlossen — mit allen Käufen, Verkäufen,
 * Dividenden und Kosten — und in
 * PortfolioSeriesResult::sharesWithoutHistory gemeldet, damit die View eine
 * Warnzeile anzeigen kann. Würde nur ihr Bestandswert entfallen, ihr Kaufwert
 * aber zählen, zeigte die Kurve einen Verlust, den es nicht gibt.
 *
 * Aus demselben Grund trägt eine Aktie an Stichtagen VOR ihrem ersten
 * Kursdatum nichts bei, auch wenn zu diesem Zeitpunkt bereits Käufe
 * verbucht sind.
 *
 * ### Ungültige Datumsangaben
 *
 * Einträge ohne gültiges Datum werden vollständig ignoriert und in
 * PortfolioShareDiagnostics::invalidDates gezählt (Bugfix 06.08.2026). Ein
 * ungültiges QDate ist in Qt KLEINER als jedes gültige — die Schleifen
 * "solange Datum kleiner oder gleich Stichtag" hätten solche Einträge sonst
 * allesamt am allerersten Stichtag verbucht. Im Feldtest zeigte die Kurve
 * dadurch bereits Jahre vor dem ersten Kauf einen Kostenblock, den es zu
 * diesem Zeitpunkt gar nicht geben konnte. Entstehen können solche Einträge
 * z.B. durch ein leeres datetime-Feld oder ein nicht ISO-8601-konformes
 * Datumsformat, das QDateTime::fromString(..., Qt::ISODate) nicht liest.
 */

/**
 * @brief Ein Kauf, reduziert auf das für die Kurve Nötige.
 */
struct PortfolioBuyEvent
{
    QDate  date;            ///< Kaufdatum
    double volume = 0.0;    ///< Gekaufte Stückzahl
    double price  = 0.0;    ///< Kaufkurs je Stück
};

/**
 * @brief Ein Verkauf, reduziert auf das für die Kurve Nötige.
 *
 * Gebühren fehlen hier absichtlich: sie stecken vollständig im Kosten-Term
 * (PortfolioCostEvent) und dürfen nicht zusätzlich vom Erlös abgezogen
 * werden, sonst wären sie doppelt gerechnet.
 */
struct PortfolioSaleEvent
{
    QDate  date;            ///< Verkaufsdatum
    double volume = 0.0;    ///< Verkaufte Stückzahl
    double price  = 0.0;    ///< Verkaufskurs je Stück
    double taxSum = 0.0;    ///< Gezahlte Steuern auf diesen Verkauf
};

/**
 * @brief Eine Dividendenzahlung, netto nach Steuern.
 */
struct PortfolioDividendEvent
{
    QDate  date;                     ///< Zahltag
    double payoutWithTaxes = 0.0;    ///< Auszahlung brutto abzüglich Steuern
};

/**
 * @brief Ein Kosteneintrag, netto nach Rabatt.
 *
 * Deckt kaufgebundene, verkaufsgebundene und freistehende Brokerage-Einträge
 * gleichermassen ab — für die Kurve zählt allein Datum und Betrag.
 */
struct PortfolioCostEvent
{
    QDate  date;            ///< Datum des Kosteneintrags
    double amount = 0.0;    ///< Provision + Broker-Gebühr + Händler-Gebühr - Rabatt
};

/**
 * @brief Ein Schlusskurs an einem Börsentag.
 */
struct PortfolioPriceEvent
{
    QDate  date;                   ///< Börsentag
    double closingPrice = 0.0;     ///< Schlusskurs
};

/**
 * @brief Alle Eingangsdaten einer einzelnen Aktie.
 *
 * Die Listen müssen nicht sortiert übergeben werden, der Rechenkern sortiert
 * selbst nach Datum.
 */
struct PortfolioShareSeriesInput
{
    QString shareGuid;                          ///< GUID der Aktie
    QString name;                               ///< Anzeigename, nur für die Warnzeile
    QList<PortfolioBuyEvent>      buys;         ///< Alle Käufe
    QList<PortfolioSaleEvent>     sales;        ///< Alle Verkäufe
    QList<PortfolioDividendEvent> dividends;    ///< Alle Dividenden
    QList<PortfolioCostEvent>     costs;        ///< Alle Kosteneinträge
    QList<PortfolioPriceEvent>    prices;       ///< Tageswert-Historie (Schlusskurse)
};

/**
 * @brief Ein Punkt der Kurve, inklusive seiner Bestandteile.
 *
 * Die Bestandteile werden mitgeliefert, damit sie in Tests einzeln geprüft
 * werden können, ohne die Rechnung nachbauen zu müssen.
 */
struct PortfolioSeriesPoint
{
    QDate  date;                        ///< Stichtag
    double holdingsValue      = 0.0;    ///< Bestandswert aller gehaltenen Anteile
    double realizedGain       = 0.0;    ///< Kumulierter realisierter Verkaufsgewinn
    double dividends          = 0.0;    ///< Kumulierte Netto-Dividenden
    double costs              = 0.0;    ///< Kumulierte Kosten
    double purchaseValueHeld  = 0.0;    ///< Kaufwert der noch gehaltenen Anteile
    double purchaseValueTotal = 0.0;    ///< Kaufwert aller Käufe bis zum Stichtag
    double development        = 0.0;    ///< Die dargestellte Linie, in Euro
    double developmentPct     = 0.0;    ///< Dieselbe Linie in Prozent
};

/**
 * @brief Zählwerte je Aktie für den Diagnose-Export.
 *
 * Ergänzt 06.08.2026: bei der Fehlersuche an einem realen Portfolio liess
 * sich aus dem gezeichneten Chart allein nicht ablesen, welcher Term
 * ausbricht. Diese Zahlen fallen bei der Berechnung ohnehin an und kosten
 * nichts.
 */
struct PortfolioShareDiagnostics
{
    QString name;                  ///< Anzeigename der Aktie
    int buys          = 0;         ///< Übergebene Käufe
    int sales         = 0;         ///< Übergebene Verkäufe
    int dividends     = 0;         ///< Übergebene Dividenden
    int costs         = 0;         ///< Übergebene Kosteneinträge
    int prices        = 0;         ///< Übergebene Tageswerte
    int invalidDates  = 0;         ///< Davon Einträge mit ungültigem Datum, siehe compute()
    QDate firstBuy;                ///< Ältester Kauf mit gültigem Datum
    QDate firstPrice;              ///< Ältester Tageswert mit gültigem Datum
    QDate lastPrice;               ///< Jüngster Tageswert mit gültigem Datum
    bool excluded     = false;     ///< True, wenn die Aktie keine Tageswert-Historie hat
};

/**
 * @brief Werte einer einzelnen Aktie an einem einzelnen Stichtag.
 *
 * Ergänzt 06.08.2026: die Portfoliosummen allein reichten bei der
 * Fehlersuche nicht aus. Im Feldtest wich der Bestandswert um mehr als das
 * Vierfache vom Kaufwert ab, ohne dass sich zuordnen liess, welche Aktie das
 * verursacht. Wird nur gefüllt, wenn compute() mit @p withPerShareDetail
 * aufgerufen wird — sonst entstünden Stichtage mal Aktien Datensätze für
 * nichts.
 */
struct PortfolioSharePoint
{
    QDate   date;                      ///< Stichtag
    QString name;                      ///< Anzeigename der Aktie
    double  volume            = 0.0;   ///< Gehaltene Stückzahl
    double  price             = 0.0;   ///< Verwendeter Schlusskurs, ggf. fortgeschrieben
    double  holdingsValue     = 0.0;   ///< volume x price
    double  purchaseValueHeld = 0.0;   ///< Kaufwert der gehaltenen Anteile
    double  realizedGain      = 0.0;   ///< Kumulierter realisierter Gewinn
    double  dividends         = 0.0;   ///< Kumulierte Netto-Dividenden
    double  costs             = 0.0;   ///< Kumulierte Kosten
};

/**
 * @brief Ergebnis einer Berechnung.
 */
struct PortfolioSeriesResult
{
    QList<PortfolioSeriesPoint>      points;               ///< Kurvenpunkte, aufsteigend nach Datum
    QStringList                      sharesWithoutHistory; ///< Namen der ausgeschlossenen Aktien
    QList<PortfolioShareDiagnostics> diagnostics;          ///< Zählwerte je Aktie, siehe Struktur
    QList<PortfolioSharePoint>       sharePoints;          ///< Nur bei withPerShareDetail, siehe Struktur
};

/**
 * @brief Berechnet die Portfolio-Entwicklungskurve.
 *
 * Zustandslos und vollständig datenbankfrei: alle Eingangsdaten kommen als
 * Parameter herein, damit die Klasse ohne Datenbank und ohne Widgets testbar
 * ist. Das Laden übernimmt ModelPortfolioChart.
 *
 * Gerundet wird durchgängig über ShareCalculator::roundAway(), damit die
 * Cent-Semantik projektweit identisch bleibt.
 */
class PortfolioSeriesCalculator
{
public:
    PortfolioSeriesCalculator() = delete;

    /**
     * @brief Berechnet die Kurve über alle übergebenen Aktien.
     * @param shares  Eingangsdaten je Aktie.
     * @param from    Erster berücksichtigter Stichtag; ungültig = keine Grenze.
     * @param to      Letzter berücksichtigter Stichtag; ungültig = keine Grenze.
     * @return Kurvenpunkte plus Liste der ausgeschlossenen Aktien.
     */
    /**
     * @param withPerShareDetail  Füllt zusätzlich
     *        PortfolioSeriesResult::sharePoints mit einem Datensatz je Aktie
     *        und Stichtag. Nur für den Diagnose-Export gedacht.
     */
    static PortfolioSeriesResult compute(const QList<PortfolioShareSeriesInput>& shares,
                                         const QDate& from = QDate(),
                                         const QDate& to   = QDate(),
                                         bool withPerShareDetail = false);

    /**
     * @brief Bildet das Datumsraster der Kurve.
     *
     * Vereinigungsmenge aller Kursdaten und aller Transaktionsdaten im
     * Fenster, aufsteigend sortiert und ohne Duplikate. Die Transaktionsdaten
     * müssen mit hinein, weil die Linie sich auch an Kauf-, Verkaufs-,
     * Dividenden- und Kostentagen ändert — fehlten sie, fehlten genau die
     * Sprungstellen. Aktien ohne Kursdaten liefern nichts, auch keine
     * Transaktionsdaten, da sie ganz ausgeschlossen sind.
     *
     * @param shares  Eingangsdaten je Aktie.
     * @param from    Untere Grenze (inklusive); ungültig = keine Grenze.
     * @param to      Obere Grenze (inklusive); ungültig = keine Grenze.
     * @return Sortierte, duplikatfreie Datumsliste.
     */
    static QList<QDate> buildDateGrid(const QList<PortfolioShareSeriesInput>& shares,
                                      const QDate& from = QDate(),
                                      const QDate& to   = QDate());

    /**
     * @brief Schlusskurs zum Stichtag, mit Vorwärts-Fortschreibung.
     *
     * Liefert den Kurs des jüngsten Eintrags mit Datum kleiner oder gleich
     * @p date. Existiert kein solcher Eintrag, wird 0.0 zurückgegeben. Damit
     * bricht die Portfoliosumme an einem Tag, an dem eine einzelne Aktie
     * keinen Eintrag hat (Feiertag, andere Börse), nicht künstlich ein.
     *
     * @param prices  Kursliste, aufsteigend nach Datum sortiert.
     * @param date    Stichtag.
     * @return Fortgeschriebener Schlusskurs, oder 0.0.
     */
    static double closingPriceAt(const QList<PortfolioPriceEvent>& prices,
                                 const QDate& date);
};

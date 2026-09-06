// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ShareSplitHint.h"
#include "ShareSplitAdjuster.h"
#include "ValueFormatter.h"

#include <QCoreApplication>
#include <QLocale>
#include <QStringList>
#include <QtGlobal>

// ── hasSplitAfter ─────────────────────────────────────────────────────────────

bool ShareSplitHint::hasSplitAfter(const QList<ShareSplitObject>& splits,
                                   const QDate& date)
{
    if (!date.isValid())
        return false;

    for (const ShareSplitObject& s : splits) {
        if (s.date() > date)
            return true;
    }
    return false;
}

// ── footerText ────────────────────────────────────────────────────────────────

QString ShareSplitHint::footerText(const QList<ShareSplitObject>& splits,
                                   const QDate& date,
                                   double volume,
                                   double price)
{
    if (!hasSplitAfter(splits, date)) {
        return QCoreApplication::translate(
            "ShareSplitHint",
            "Kein Split nach diesem Datum — Stückzahl entspricht dem heutigen Stand");
    }

    // Jüngster Split nach dem Datum. Die Liste kommt aufsteigend sortiert
    // (ShareSplitRepository::findByShare()), der letzte Treffer ist damit
    // der jüngste.
    ShareSplitObject latest;
    int count = 0;
    for (const ShareSplitObject& s : splits) {
        if (s.date() > date) {
            latest = s;
            ++count;
        }
    }

    // Umrechnung durchgängig über ShareSplitAdjuster — die Regel, welche
    // Splits zählen und wie Stückzahl und Preis gegenläufig skalieren, darf
    // es im Projekt nur an einer Stelle geben. Das Produkt aus beiden bleibt
    // dadurch unverändert, was der Text ja gerade zeigen soll.
    const QLocale loc;
    const QString adjustedVolume =
        loc.toString(ShareSplitAdjuster::adjustedVolume(volume, splits, date), 'f', 4);
    // Kurs ueber ValueFormatter (05.09.2026): identische Ausgabe wie zuvor,
    // die Genauigkeit haengt jetzt aber projektweit an einer Stelle.
    const QString adjustedPrice =
        ValueFormatter::formatPrice(
            ShareSplitAdjuster::adjustedTransactionPrice(price, splits, date));

    if (count == 1) {
        return QCoreApplication::translate(
                   "ShareSplitHint", "Split %1 — entspricht heute %2 stk. à %3 €")
            .arg(describeSplit(latest), adjustedVolume, adjustedPrice);
    }

    return QCoreApplication::translate(
               "ShareSplitHint",
               "%1 Splits, zuletzt %2 — entspricht heute %3 stk. à %4 €")
        .arg(QString::number(count), describeSplit(latest),
             adjustedVolume, adjustedPrice);
}

// ── tooltipText ───────────────────────────────────────────────────────────────

QString ShareSplitHint::tooltipText(const QList<ShareSplitObject>& splits,
                                    const QDate& date)
{
    QStringList lines;
    for (const ShareSplitObject& s : splits) {
        if (s.date() > date)
            lines << describeSplit(s);
    }
    return lines.join(QLatin1Char('\n'));
}

// ── marker / withMarker ───────────────────────────────────────────────────────

QString ShareSplitHint::marker()
{
    return QStringLiteral("*");
}

QString ShareSplitHint::withMarker(const QString& cellText, bool affected)
{
    if (!affected)
        return cellText;
    return cellText + QLatin1Char(' ') + marker();
}

// ── overviewRowTooltip ────────────────────────────────────────────────────────

QString ShareSplitHint::overviewRowTooltip(const QList<ShareSplitObject>& splits,
                                           const QDate& date,
                                           double volume,
                                           double price)
{
    if (!hasSplitAfter(splits, date))
        return {};

    // Erste Zeile ist derselbe Satz wie die Fusszeile der Editier-Dialoge —
    // dieselbe Aussage soll auch dieselben Worte haben.
    QString text = QCoreApplication::translate(
                       "ShareSplitHint", "Stückzahl laut Beleg. %1")
                       .arg(footerText(splits, date, volume, price));

    // Bei genau einem Split nennt footerText() ihn bereits vollständig; die
    // Liste würde die Zeile nur wiederholen.
    const QString list = tooltipText(splits, date);
    if (list.contains(QLatin1Char('\n')))
        text += QLatin1Char('\n') + list;

    return text;
}

// ── overviewAggregateTooltip ──────────────────────────────────────────────────

QString ShareSplitHint::overviewAggregateTooltip(const QList<ShareSplitObject>& splits,
                                                 const QDate& earliestDate)
{
    if (!hasSplitAfter(splits, earliestDate))
        return {};

    return QCoreApplication::translate(
               "ShareSplitHint",
               "Summe über Belege unterschiedlicher Stückelung — auf heutige "
               "Stücke umgerechnet.")
           + QLatin1Char('\n')
           + tooltipText(splits, earliestDate);
}

// ── describeSplit / formatRatioPart ───────────────────────────────────────────

QString ShareSplitHint::describeSplit(const ShareSplitObject& split)
{
    return QCoreApplication::translate("ShareSplitHint", "%1:%2 am %3")
        .arg(formatRatioPart(split.ratioNew()),
             formatRatioPart(split.ratioOld()),
             QLocale().toString(split.date(), QLocale::ShortFormat));
}

QString ShareSplitHint::formatRatioPart(double value)
{
    // Ganze Verhältnisse ohne Nachkommastellen ("20:1" statt "20,00:1,00").
    // Gebrochene Verhältnisse gibt es aber (z. B. 3:2), deshalb kein
    // pauschales Abschneiden.
    const QLocale loc;
    const double rounded = static_cast<double>(qRound(value));
    if (qAbs(value - rounded) < 1e-9)
        return loc.toString(qRound(value));
    return loc.toString(value, 'f', 2);
}

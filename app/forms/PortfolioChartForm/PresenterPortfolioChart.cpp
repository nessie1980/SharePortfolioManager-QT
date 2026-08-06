// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "PresenterPortfolioChart.h"

#include <QLocale>
#include <QDateTime>
#include <algorithm>

// -- Constructor ---------------------------------------------------------------

PresenterPortfolioChart::PresenterPortfolioChart(IViewPortfolioChart* view,
                                                 IModelPortfolioChart* model,
                                                 QObject* parent)
    : QObject(parent)
    , m_view(view)
    , m_model(model)
{
}

// -- loadAndDisplay ------------------------------------------------------------

void PresenterPortfolioChart::loadAndDisplay()
{
    m_input = m_model->loadPortfolioInput();

    // "Daten vorhanden" heisst: mindestens eine Aktie hat Tageswerte. Aktien
    // ohne Historie werden vom Rechenkern ohnehin ausgeschlossen.
    m_hasData = std::any_of(m_input.cbegin(), m_input.cend(),
                            [](const PortfolioShareSeriesInput& share) {
                                return !share.prices.isEmpty();
                            });

    // Vorgabe laut Nessie (05.08.2026): Start-Datum = heute, Interval = Jahr,
    // Anzahl = 1. Die beiden letzten setzt die View selbst als Default.
    m_view->setDefaultStartDate(QDate::currentDate());

    if (!m_hasData) {
        m_view->setMaxIntervalCount(1);
        m_view->showEmptyChart(tr("Keine Tageswerte vorhanden."));

        QStringList names;
        for (const PortfolioShareSeriesInput& share : m_input)
            names.append(share.name.isEmpty() ? share.shareGuid : share.name);
        m_view->setWarning(buildWarningText(names));

        m_view->setRangeInfo(QString());
        return;
    }

    refresh();
}

// -- reload --------------------------------------------------------------------

void PresenterPortfolioChart::reload()
{
    m_input.clear();
    m_hasData = false;
    loadAndDisplay();
}

// -- onControlsChanged ---------------------------------------------------------

void PresenterPortfolioChart::onControlsChanged()
{
    refresh();
}

// -- refresh -------------------------------------------------------------------

void PresenterPortfolioChart::refresh()
{
    if (!m_hasData)
        return;

    const QDate rangeEnd   = m_view->startDate();
    const int   count      = std::max(1, m_view->intervalCount());
    const QDate rangeStart = computeRangeStart(rangeEnd, m_view->intervalUnit(), count);

    m_view->setMaxIntervalCount(computeMaxIntervalCount(rangeEnd, m_view->intervalUnit(),
                                                        m_model->earliestRelevantDate()));

    // Sichtbare Zwischenanzeige, bevor die Aggregation läuft (Nessies Vorgabe
    // 05.08.2026). Die Berechnung selbst bleibt synchron — sie ist schnell
    // genug, und ein Worker-Thread würde die MVP-Verdrahtung ohne echten
    // Gewinn verkomplizieren.
    m_view->showCalculating(tr("Berechnung läuft…"));

    const PortfolioSeriesResult series =
        PortfolioSeriesCalculator::compute(m_input, rangeStart, rangeEnd);

    m_view->setWarning(buildWarningText(series.sharesWithoutHistory));

    if (series.points.isEmpty()) {
        m_view->showEmptyChart(tr("Keine Tageswerte im gewählten Zeitraum."));
        m_view->setRangeInfo(buildRangeInfo(rangeStart, rangeEnd, {}));
        return;
    }

    PortfolioChartData data;
    data.points.reserve(series.points.size());
    for (const PortfolioSeriesPoint& point : series.points)
        data.points.append(PortfolioChartPoint{ point.date, point.development,
                                                point.developmentPct });

    m_view->setChartData(data);
    m_view->setRangeInfo(buildRangeInfo(rangeStart, rangeEnd, data.points));
}

// -- computeRangeStart ---------------------------------------------------------

QDate PresenterPortfolioChart::computeRangeStart(const QDate& rangeEnd,
                                                 IntervalUnit unit, int count)
{
    const int steps = std::max(1, count);

    switch (unit) {
    case IntervalUnit::Day:   return rangeEnd.addDays(-steps);
    case IntervalUnit::Week:  return rangeEnd.addDays(-steps * 7);
    case IntervalUnit::Month: return rangeEnd.addMonths(-steps);
    case IntervalUnit::Year:  return rangeEnd.addYears(-steps);
    }
    return rangeEnd;
}

// -- computeMaxIntervalCount ---------------------------------------------------

int PresenterPortfolioChart::computeMaxIntervalCount(const QDate& rangeEnd,
                                                     IntervalUnit unit,
                                                     const QDate& earliestDate)
{
    if (!earliestDate.isValid() || !rangeEnd.isValid() || earliestDate >= rangeEnd)
        return 1;

    // Erste Bremse: bei Interval=Tag die exakt richtige Grenze, bei
    // Woche/Monat/Jahr grosszügig genug, da jede dieser Stufen mindestens so
    // gross ist wie ein Tag.
    const qint64 dayLimit = earliestDate.daysTo(rangeEnd);
    const int    ceiling  = static_cast<int>(std::min<qint64>(dayLimit, kAbsoluteSafetyCeiling));

    int count = 1;
    while (count < ceiling && computeRangeStart(rangeEnd, unit, count) > earliestDate)
        ++count;

    return std::max(1, count);
}

// -- buildWarningText ----------------------------------------------------------

QString PresenterPortfolioChart::buildWarningText(const QStringList& shareNames)
{
    if (shareNames.isEmpty())
        return QString();

    // Bewusst ohne %n-Pluralform: der Satz liest sich für eine wie für
    // mehrere Aktien gleich gut, und die Übersetzungsdateien bleiben einfach.
    return tr("Aktien ohne Tageswert-Historie sind im Chart nicht enthalten: %1")
           .arg(shareNames.join(QStringLiteral(", ")));
}

// -- buildRangeInfo ------------------------------------------------------------

QString PresenterPortfolioChart::buildRangeInfo(const QDate& from, const QDate& to,
                                                const QList<PortfolioChartPoint>& points)
{
    const QString range = tr("Zeitraum: %1 - %2")
        .arg(from.toString(QStringLiteral("dd.MM.yyyy")),
             to.toString(QStringLiteral("dd.MM.yyyy")));

    if (points.isEmpty())
        return range;

    const PortfolioChartPoint& last = points.constLast();
    return range + tr(" / Entwicklung: %1 (%2)")
        .arg(formatEuro(last.development), formatPercent(last.developmentPct));
}

// -- Formatting ----------------------------------------------------------------

QString PresenterPortfolioChart::formatEuro(double value)
{
    return QLocale().toString(value, 'f', 2) + QStringLiteral(" €");
}

QString PresenterPortfolioChart::formatPercent(double value)
{
    return QLocale().toString(value, 'f', 2) + QStringLiteral(" %");
}

// -- buildDiagnosticsCsv -------------------------------------------------------

QString PresenterPortfolioChart::buildDiagnosticsCsv() const
{
    const QLocale locale;

    const QDate rangeEnd   = m_view->startDate();
    const int   count      = std::max(1, m_view->intervalCount());
    const QDate rangeStart = computeRangeStart(rangeEnd, m_view->intervalUnit(), count);

    const PortfolioSeriesResult series =
        PortfolioSeriesCalculator::compute(m_input, rangeStart, rangeEnd,
                                           /*withPerShareDetail*/ true);

    const auto dateOrDash = [](const QDate& date) {
        return date.isValid() ? date.toString(QStringLiteral("dd.MM.yyyy"))
                              : QStringLiteral("-");
    };
    const auto money = [&locale](double value) { return locale.toString(value, 'f', 2); };

    QString csv;

    // ── Kopf ──────────────────────────────────────────────────────────────
    csv += tr("Depotwert-Chart Diagnose") + QLatin1Char('\n');
    csv += tr("Erstellt") + QLatin1Char(';')
         + QDateTime::currentDateTime().toString(QStringLiteral("dd.MM.yyyy HH:mm:ss"))
         + QLatin1Char('\n');
    csv += tr("Zeitraum") + QLatin1Char(';') + dateOrDash(rangeStart)
         + QLatin1Char(';') + dateOrDash(rangeEnd) + QLatin1Char('\n');
    csv += tr("Aktien im Portfolio") + QLatin1Char(';')
         + QString::number(m_input.size()) + QLatin1Char('\n');
    csv += tr("Stichtage") + QLatin1Char(';')
         + QString::number(series.points.size()) + QLatin1Char('\n');
    csv += QLatin1Char('\n');

    // ── Block 1: je Aktie ─────────────────────────────────────────────────
    csv += tr("Name;Kaeufe;Verkaeufe;Dividenden;Kosten;Tageswerte;"
              "Ungueltige Datumsangaben;Erster Kauf;Erster Kurs;Letzter Kurs;"
              "Ausgeschlossen") + QLatin1Char('\n');

    // Zeilen über join() statt über arg()-Platzhalter: bei mehr als neun
    // Spalten wird die Nummerierung (%10, %11) unübersichtlich und leicht
    // fehleranfällig.
    for (const PortfolioShareDiagnostics& diag : series.diagnostics) {
        const QStringList fields = {
            diag.name,
            QString::number(diag.buys),
            QString::number(diag.sales),
            QString::number(diag.dividends),
            QString::number(diag.costs),
            QString::number(diag.prices),
            QString::number(diag.invalidDates),
            dateOrDash(diag.firstBuy),
            dateOrDash(diag.firstPrice),
            dateOrDash(diag.lastPrice),
            diag.excluded ? tr("ja") : tr("nein")
        };
        csv += fields.join(QLatin1Char(';')) + QLatin1Char('\n');
    }

    csv += QLatin1Char('\n');

    // ── Block 2: je Stichtag ──────────────────────────────────────────────
    csv += tr("Datum;Bestandswert;Realisierter Gewinn;Dividenden;Kosten;"
              "Kaufwert gehalten;Kaufwert gesamt;Entwicklung;Entwicklung %")
         + QLatin1Char('\n');

    for (const PortfolioSeriesPoint& point : series.points) {
        const QStringList fields = {
            point.date.toString(QStringLiteral("dd.MM.yyyy")),
            money(point.holdingsValue),
            money(point.realizedGain),
            money(point.dividends),
            money(point.costs),
            money(point.purchaseValueHeld),
            money(point.purchaseValueTotal),
            money(point.development),
            money(point.developmentPct)
        };
        csv += fields.join(QLatin1Char(';')) + QLatin1Char('\n');
    }

    csv += QLatin1Char('\n');

    // ── Block 3: je Aktie und Stichtag ────────────────────────────────────
    // Der teuerste Block — Stichtage mal Aktien Zeilen. Bei einer
    // auffälligen Stelle im Kurvenverlauf lohnt es sich, den Zeitraum vorher
    // eng um das fragliche Datum zu legen (z.B. Interval "Tag", Anzahl 5).
    csv += tr("Datum;Aktie;Stueck;Kurs;Bestandswert;Kaufwert gehalten;"
              "Realisierter Gewinn;Dividenden;Kosten") + QLatin1Char('\n');

    for (const PortfolioSharePoint& detail : series.sharePoints) {
        const QStringList fields = {
            detail.date.toString(QStringLiteral("dd.MM.yyyy")),
            detail.name,
            locale.toString(detail.volume, 'f', 4),
            money(detail.price),
            money(detail.holdingsValue),
            money(detail.purchaseValueHeld),
            money(detail.realizedGain),
            money(detail.dividends),
            money(detail.costs)
        };
        csv += fields.join(QLatin1Char(';')) + QLatin1Char('\n');
    }

    return csv;
}

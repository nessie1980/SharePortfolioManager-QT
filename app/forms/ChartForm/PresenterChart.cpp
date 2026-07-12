// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "PresenterChart.h"

#include <QLocale>
#include <QCoreApplication>
#include <algorithm>
#include <limits>

namespace {

QString seriesLabel(SeriesKind kind)
{
    switch (kind) {
    case SeriesKind::ClosingPrice: return QCoreApplication::translate("PresenterChart", "Schluss-Kurs");
    case SeriesKind::OpeningPrice: return QCoreApplication::translate("PresenterChart", "Eröffnungs-Kurs");
    case SeriesKind::High:         return QCoreApplication::translate("PresenterChart", "Höchstwert");
    case SeriesKind::Low:          return QCoreApplication::translate("PresenterChart", "Tiefstwert");
    case SeriesKind::HeldVolume:   return QCoreApplication::translate("PresenterChart", "Anteile");
    case SeriesKind::TradedVolume: return QCoreApplication::translate("PresenterChart", "Gehandelte Anteile");
    }
    return QString();
}

QColor seriesColor(SeriesKind kind)
{
    switch (kind) {
    case SeriesKind::ClosingPrice: return Qt::black;
    case SeriesKind::OpeningPrice: return Qt::darkGray;
    case SeriesKind::High:         return QColor(0, 150, 0);
    case SeriesKind::Low:          return QColor(230, 150, 0);
    case SeriesKind::HeldVolume:   return QColor(128, 0, 128);
    case SeriesKind::TradedVolume: return QColor(0, 150, 150);
    }
    return Qt::black;
}

ChartAxis axisForKind(SeriesKind kind)
{
    switch (kind) {
    case SeriesKind::HeldVolume:
    case SeriesKind::TradedVolume:
        return ChartAxis::Volume;
    default:
        return ChartAxis::Price;
    }
}

/** Whether @p kind plots on the "Stück"-Achse (HeldVolume/TradedVolume)
 *  rather than the price axis — used for legend formatting (no "(€)" suffix,
 *  no decimals). Beide teilen sich seit 12.07.2026 dieselbe ChartAxis::Volume
 *  (Checkboxen gegenseitig exklusiv, siehe ViewChart::setupSelektionBox()). */
bool isVolumeKind(SeriesKind kind)
{
    return axisForKind(kind) != ChartAxis::Price;
}

double valueForKind(const DailyValuesObject& dv, SeriesKind kind)
{
    switch (kind) {
    case SeriesKind::ClosingPrice: return dv.closingPrice();
    case SeriesKind::OpeningPrice: return dv.openingPrice();
    case SeriesKind::High:         return dv.top();
    case SeriesKind::Low:          return dv.bottom();
    case SeriesKind::HeldVolume:   return 0.0; // filled separately via heldVolumeSeries()
    case SeriesKind::TradedVolume: return dv.volume();
    }
    return 0.0;
}

} // namespace

// ── Constructor ───────────────────────────────────────────────────────────────

PresenterChart::PresenterChart(IViewChart* view, IModelChart* model,
                               QString shareGuid, QObject* parent)
    : QObject(parent)
    , m_view(view)
    , m_model(model)
    , m_shareGuid(std::move(shareGuid))
{}

// ── loadAndDisplay ────────────────────────────────────────────────────────────

void PresenterChart::loadAndDisplay()
{
    const QDate latest = m_model->latestDailyValueDate(m_shareGuid);
    if (!latest.isValid()) {
        m_hasData = false;
        m_view->showEmptyChart(tr("Für diese Aktie liegen keine Kursdaten vor."));
        m_view->setLegendEntries({});
        m_view->setReferenceLines({});
        m_view->setRangeInfo(QString());
        return;
    }

    m_hasData = true;
    m_view->setDefaultStartDate(latest);
    refresh();
}

// ── onControlsChanged ─────────────────────────────────────────────────────────

void PresenterChart::onControlsChanged()
{
    if (!m_hasData)
        return;
    refresh();
}

// ── refresh ───────────────────────────────────────────────────────────────────

void PresenterChart::refresh()
{
    const QDate rangeEnd = m_view->startDate();
    const IntervalUnit unit = m_view->intervalUnit();

    // Obergrenze für "Anzahl" — verhindert, dass sich das Fenster über den
    // Punkt hinaus vergrößern lässt, an dem der älteste vorhandene Tageswert
    // bereits dargestellt wird (ergänzt 12.07.2026, auf Nessies Vorgabe).
    // Wird unabhängig davon berechnet, ob die aktuelle "Anzahl" sie bereits
    // überschreitet — setMaxIntervalCount() lässt die View das UI-seitig
    // (Spinbox-Maximum) durchsetzen, der anschließende std::min() macht die
    // Presenter-Logik selbst aber unabhängig davon korrekt, ob die View das
    // tatsächlich tut (z. B. in Tests mit einer einfachen Fake-View).
    const QDate earliestDate = m_model->earliestDailyValueDate(m_shareGuid);
    const int maxCount = computeMaxIntervalCount(rangeEnd, unit, earliestDate);
    m_view->setMaxIntervalCount(maxCount);

    const int count = std::min(maxCount, std::max(1, m_view->intervalCount()));
    const QDate rangeStart = computeRangeStart(rangeEnd, unit, count);

    const auto dailyValues = m_model->loadDailyValues(m_shareGuid, rangeStart, rangeEnd);
    if (dailyValues.isEmpty()) {
        m_view->showEmptyChart(tr("Keine Kursdaten im gewählten Zeitraum."));
        m_view->setLegendEntries({});
        m_view->setReferenceLines({});
        m_view->setRangeInfo(QString());
        return;
    }

    QList<QDate> dates;
    dates.reserve(dailyValues.size());
    for (const auto& dv : dailyValues)
        dates.append(dv.date());

    // ── Build the requested series ──────────────────────────────────────────
    QList<ChartSeriesData> seriesList;
    LegendEntries legend;

    for (SeriesKind kind : allSeriesKinds()) {
        if (!m_view->isSeriesSelected(kind))
            continue;

        ChartSeriesData series;
        series.kind  = kind;
        series.dates = dates;
        series.color = seriesColor(kind);
        series.axis  = axisForKind(kind);

        if (kind == SeriesKind::HeldVolume) {
            const auto held = m_model->heldVolumeSeries(m_shareGuid, dates);
            series.values.reserve(dates.size());
            for (const QDate& d : dates)
                series.values.append(held.value(d, 0.0));
        } else {
            series.values.reserve(dailyValues.size());
            for (const auto& dv : dailyValues)
                series.values.append(valueForKind(dv, kind));
        }

        double minV = std::numeric_limits<double>::max();
        double maxV = std::numeric_limits<double>::lowest();
        for (double v : series.values) {
            minV = std::min(minV, v);
            maxV = std::max(maxV, v);
        }

        LegendEntry entry;
        entry.color = series.color;
        entry.title = isVolumeKind(kind)
                        ? seriesLabel(kind)
                        : QStringLiteral("%1(€)").arg(seriesLabel(kind));
        entry.line1 = tr("Min: %1 / Max: %2")
                        .arg(formatNumber(minV, isVolumeKind(kind) ? 0 : 2),
                             formatNumber(maxV, isVolumeKind(kind) ? 0 : 2));
        legend.append(entry);

        seriesList.append(series);
    }

    if (seriesList.isEmpty()) {
        m_view->showEmptyChart(tr("Keine Kurve ausgewählt — bitte mindestens eine Selektion aktivieren."));
        m_view->setLegendEntries({});
        m_view->setReferenceLines({});
        m_view->setRangeInfo(QString());
        return;
    }

    // ── "Letzter Kauf" / "Letzter Verkauf" reference rows ────────────────────
    // Reference price is the highest Schluss-Kurs within the displayed range —
    // always computed from dailyValues directly (independent of whether the
    // Schluss-Kurs series itself is currently selected), matching the C#
    // reference's "Max" figure used for both the Legende's Schluss-Kurs entry
    // and the Kauf-/Verkauf-Entwicklung.
    double rangeMaxClose = std::numeric_limits<double>::lowest();
    for (const auto& dv : dailyValues)
        rangeMaxClose = std::max(rangeMaxClose, dv.closingPrice());

    auto addReferenceEntry = [&](const QString& title, const ChartReferenceInfo& info, const QColor& color) {
        if (!info.valid)
            return;
        LegendEntry entry;
        entry.color = color;
        entry.title = title;
        entry.line1 = tr("%1: %2").arg(info.date.toString(QStringLiteral("dd.MM.yyyy")), formatEuro(info.price));

        const double diff = rangeMaxClose - info.price;
        const double pct  = (info.price != 0.0) ? (diff / info.price * 100.0) : 0.0;
        entry.line2 = tr("%1 - %2 = %3 (%4 %)")
                        .arg(formatEuro(rangeMaxClose), formatEuro(info.price),
                             formatEuro(diff), formatPercent(pct));
        legend.append(entry);
    };

    const ChartReferenceInfo latestBuyInfo  = m_model->latestBuy(m_shareGuid);
    const ChartReferenceInfo latestSaleInfo = m_model->latestSale(m_shareGuid);

    addReferenceEntry(tr("Letzter Kauf:"),    latestBuyInfo,  Qt::blue);
    addReferenceEntry(tr("Letzter Verkauf:"), latestSaleInfo, Qt::red);

    // ── Vertikale Kauf-/Verkauf-Markerlinien im Chart ─────────────────────────
    // Ported from the C# reference: jeder Kauf/Verkauf im angezeigten
    // Zeitraum bekommt eine vertikale Linie über die Preis-Achse. Der jeweils
    // global letzte Kauf/Verkauf (dieselbe Definition wie "Letzter Kauf"/
    // "Letzter Verkauf" oben) ist dunkelblau bzw. rot, ältere im Zeitraum
    // liegende Käufe/Verkäufe türkis bzw. orange.
    QList<ChartReferenceLine> referenceLines;
    for (const auto& buy : m_model->buysInRange(m_shareGuid, rangeStart, rangeEnd)) {
        const bool isLatest = latestBuyInfo.valid && buy.date == latestBuyInfo.date;
        referenceLines.append({ buy.date, isLatest ? Qt::blue : QColor(0, 170, 170),
                                 ChartReferenceLineKind::Buy, buy.price, buy.volume });
    }
    for (const auto& sale : m_model->salesInRange(m_shareGuid, rangeStart, rangeEnd)) {
        const bool isLatest = latestSaleInfo.valid && sale.date == latestSaleInfo.date;
        referenceLines.append({ sale.date, isLatest ? Qt::red : QColor(255, 140, 0),
                                 ChartReferenceLineKind::Sale, sale.price, sale.volume });
    }

    m_view->setChartData(seriesList);
    m_view->setLegendEntries(legend);
    m_view->setReferenceLines(referenceLines);

    // ── Title info: "Zeitraum: ... - ... / Entwicklung: X€ (Y %)" ───────────
    const double firstClose = dailyValues.constFirst().closingPrice();
    const double lastClose  = dailyValues.constLast().closingPrice();
    const double devAbs     = lastClose - firstClose;
    const double devPct     = (firstClose != 0.0) ? (devAbs / firstClose * 100.0) : 0.0;

    const QString titleInfo = tr("Zeitraum: %1 - %2 / Entwicklung: %3€ (%4 %)")
        .arg(dates.constFirst().toString(QStringLiteral("dd.MM.yyyy")),
             dates.constLast().toString(QStringLiteral("dd.MM.yyyy")),
             formatNumber(devAbs, 1), formatPercent(devPct));
    m_view->setRangeInfo(titleInfo);
}

// ── computeRangeStart ─────────────────────────────────────────────────────────

QDate PresenterChart::computeRangeStart(const QDate& rangeEnd, IntervalUnit unit, int count)
{
    switch (unit) {
    case IntervalUnit::Day:   return rangeEnd.addDays(-count);
    case IntervalUnit::Week:  return rangeEnd.addDays(-7 * count);
    case IntervalUnit::Month: return rangeEnd.addMonths(-count);
    case IntervalUnit::Year:  return rangeEnd.addYears(-count);
    }
    return rangeEnd.addMonths(-count);
}

// ── computeMaxIntervalCount ───────────────────────────────────────────────────

int PresenterChart::computeMaxIntervalCount(const QDate& rangeEnd, IntervalUnit unit,
                                            const QDate& earliestDate)
{
    // Kein Tageswert überhaupt vorhanden, oder der älteste liegt bereits
    // auf/nach rangeEnd selbst -> es gibt nichts mehr zu erreichen, "Anzahl"
    // bleibt bei ihrem Minimum von 1 (spiegelt m_countSpin's Minimum in
    // ViewChart).
    if (!earliestDate.isValid() || earliestDate >= rangeEnd)
        return 1;

    // Wächst "Anzahl" so lange, wie das nächstgrößere Fenster den ältesten
    // Wert noch nicht überschritten hat (rangeStart(count+1) >= earliestDate
    // bedeutet: der älteste Wert liegt noch im oder am Rand des Fensters).
    // Sobald das nicht mehr gilt, würde eine weitere Vergrößerung ohnehin
    // keine zusätzlichen Daten mehr zeigen — genau der von Nessie
    // beschriebene Fall.
    int count = 1;
    while (count < kIntervalCountCeiling &&
           computeRangeStart(rangeEnd, unit, count + 1) >= earliestDate) {
        ++count;
    }
    return count;
}

// ── Formatting helpers ────────────────────────────────────────────────────────

QString PresenterChart::formatEuro(double value)
{
    return formatNumber(value, 2) + QStringLiteral("€");
}

QString PresenterChart::formatPercent(double value)
{
    return formatNumber(value, 2);
}

QString PresenterChart::formatNumber(double value, int decimals)
{
    return QLocale().toString(value, 'f', decimals);
}

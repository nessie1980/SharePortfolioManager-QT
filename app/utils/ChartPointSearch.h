// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QList>
#include <QPointF>
#include <QtGlobal>

#include <algorithm>
#include <iterator>

/**
 * @brief Suche des nächstgelegenen Datenpunkts für Chart-Tooltips
 * (zusammengeführt 19.09.2026).
 *
 * QLineSeries::hovered() liefert die Mausposition in Achsenkoordinaten, nicht
 * den Datenpunkt unter dem Zeiger. Beide Charts müssen deshalb auf den
 * nächstgelegenen echten Datenpunkt einrasten, bevor sie einen Wert anzeigen:
 * ViewPortfolioChart seit dem 06.08.2026, ViewChart seit dem 19.09.2026.
 * Die beiden Umsetzungen lagen zunächst getrennt vor; genau deshalb wurde der
 * Fix im Depotwert-Chart nicht auf den Aktien-Chart übertragen. Siehe
 * ARCHITECTURE.md, "Nächster-Datenpunkt-Suche liegt in beiden Charts doppelt
 * vor" unter "Erledigt / Archiv".
 *
 * Header-only und zustandslos, gleiche Bauweise wie ValueFormatter und
 * NumberParser.
 *
 * Liefert bewusst einen Index statt eines Punkts: ViewPortfolioChart braucht
 * den zugehörigen PortfolioChartPoint (samt Prozentwert), nicht nur die
 * Koordinaten.
 */
namespace ChartPointSearch {

/**
 * @brief Index des Elements mit dem kleinsten X-Abstand zu @p x.
 *
 * Binäre Suche, @p sorted muss daher nach dem X-Wert aufsteigend sortiert
 * sein — beide Charts hängen ihre Punkte in Datumsreihenfolge an. Maßgeblich
 * ist allein der X-Abstand (Datum), da jede Serie genau einen Wert pro Datum
 * hat. Bei exakt gleichem Abstand zu beiden Nachbarn gewinnt der spätere
 * Punkt — das entspricht dem Verhalten beider früheren Umsetzungen.
 *
 * @param sorted  Nach X aufsteigend sortierte Elemente.
 * @param x       X-Koordinate der Mausposition.
 * @param xOf     Liefert den X-Wert eines Elements als double.
 * @return Index in @p sorted, oder -1 bei leerer Liste.
 */
template <typename T, typename XOf>
qsizetype nearestIndex(const QList<T>& sorted, double x, XOf xOf)
{
    if (sorted.isEmpty())
        return -1;

    // Erstes Element mit X >= x.
    const auto it = std::lower_bound(sorted.cbegin(), sorted.cend(), x,
        [&xOf](const T& element, double value) { return xOf(element) < value; });

    if (it == sorted.cend())
        return sorted.size() - 1;
    if (it == sorted.cbegin())
        return 0;

    const auto prev = std::prev(it);
    // Strikt kleiner: bei exakt gleichem Abstand gewinnt der spätere Punkt.
    const auto nearest = (x - xOf(*prev) < xOf(*it) - x) ? prev : it;
    return static_cast<qsizetype>(std::distance(sorted.cbegin(), nearest));
}

/** Überladung für Serienpunkte (ViewChart: QLineSeries::points()). */
inline qsizetype nearestIndex(const QList<QPointF>& sorted, double x)
{
    return nearestIndex(sorted, x, [](const QPointF& p) { return p.x(); });
}

/** Überladung für reine X-Koordinaten in Millisekunden (ViewPortfolioChart). */
inline qsizetype nearestIndex(const QList<qint64>& sortedMSecs, double x)
{
    return nearestIndex(sortedMSecs, x, [](qint64 ms) { return static_cast<double>(ms); });
}

} // namespace ChartPointSearch

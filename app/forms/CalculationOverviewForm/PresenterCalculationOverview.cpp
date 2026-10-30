// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "PresenterCalculationOverview.h"

namespace {

using B = CalculationBreakdown;

QString dateText(const QDate& date)
{
    return date.isValid() ? date.toString(QStringLiteral("dd.MM.yyyy")) : QStringLiteral("-");
}

QString countText(int count, const QString& singular, const QString& plural)
{
    return QStringLiteral("%1 %2").arg(count).arg(count == 1 ? singular : plural);
}

bool isSold(const CalculationReportShare& share)
{
    return share.values.volume <= CalculationReport::kVolumeTolerance;
}

} // namespace

// ── Konstruktor ───────────────────────────────────────────────────────────────

PresenterCalculationOverview::PresenterCalculationOverview(IViewCalculationOverview* view,
                                                           IModelCalculationOverview* model,
                                                           QObject* parent)
    : QObject(parent)
    , m_view(view)
    , m_model(model)
{
}

// ── loadAndDisplay ────────────────────────────────────────────────────────────

void PresenterCalculationOverview::loadAndDisplay()
{
    const QString previous = m_view->selectedKey();

    m_input = m_model->load();

    const QList<CalculationOverviewEntry> entries = buildEntries(m_input);
    m_view->setEntries(entries);
    m_view->setSummary(buildSummary(m_input));

    // Auswahl erhalten, solange es den Eintrag noch gibt (z. B. nach einer
    // Kursaktualisierung) — sonst zurück auf "Gesamt / Footer".
    QString key = totalKey();
    for (const CalculationOverviewEntry& e : entries) {
        if (!previous.isEmpty() && e.key == previous) {
            key = previous;
            break;
        }
    }
    m_view->setSelectedKey(key);
    showDetailFor(key);
}

// ── onSelectionChanged ────────────────────────────────────────────────────────

void PresenterCalculationOverview::onSelectionChanged()
{
    const QString key = m_view->selectedKey();
    if (key.isEmpty())
        return; // Gruppenüberschrift oder leere Liste — Detail bleibt stehen
    showDetailFor(key);
}

void PresenterCalculationOverview::showDetailFor(const QString& key)
{
    if (key != totalKey()) {
        for (const CalculationReportShare& s : m_input.shares) {
            if (s.share.guid() == key) {
                m_view->showDetail(buildShareDetail(s));
                return;
            }
        }
    }
    m_view->showDetail(buildTotalDetail(m_input));
}

// ── buildEntries / buildSummary ───────────────────────────────────────────────

QList<CalculationOverviewEntry> PresenterCalculationOverview::buildEntries(
    const CalculationReportInput& input)
{
    QList<CalculationOverviewEntry> entries;

    const CalculationFindings all = B::findings(input);
    CalculationOverviewEntry total;
    total.key     = totalKey();
    total.title   = QStringLiteral("Gesamt / Footer");
    total.group   = CalculationOverviewEntry::Group::Total;
    total.tone    = toneOf(all);
    total.toolTip = findingsText(all);
    entries.append(total);

    for (const CalculationReportShare& s : input.shares) {
        const CalculationFindings f = B::shareFindings(s);
        CalculationOverviewEntry e;
        e.key     = s.share.guid();
        e.title   = s.share.wkn().isEmpty()
                        ? B::shareTitle(s)
                        : QStringLiteral("%1 (%2)").arg(B::shareTitle(s), s.share.wkn());
        e.group   = isSold(s) ? CalculationOverviewEntry::Group::Sold
                              : CalculationOverviewEntry::Group::Held;
        e.tone    = toneOf(f);
        e.toolTip = findingsText(f);
        entries.append(e);
    }
    return entries;
}

QString PresenterCalculationOverview::buildSummary(const CalculationReportInput& input)
{
    int held = 0;
    for (const CalculationReportShare& s : input.shares) {
        if (!isSold(s))
            ++held;
    }
    const int sold = int(input.shares.size()) - held;

    return QStringLiteral("Stand %1 · %2 (%3 im Bestand, %4 verkauft) · %5")
        .arg(input.created.toString(QStringLiteral("dd.MM.yyyy HH:mm")),
             countText(int(input.shares.size()), QStringLiteral("Aktie"), QStringLiteral("Aktien")))
        .arg(held)
        .arg(sold)
        .arg(findingsText(B::findings(input)));
}

// ── Detail ────────────────────────────────────────────────────────────────────

CalculationOverviewDetail PresenterCalculationOverview::buildShareDetail(
    const CalculationReportShare& s)
{
    const ShareValues& v = s.values;

    CalculationOverviewDetail d;
    d.title    = B::shareTitle(s);
    d.subtitle = QStringLiteral("WKN %1 · ISIN %2 · Kurs Tabelle %3 € (Vortag %4 €) · Bestand %5 Stk.")
                     .arg(s.share.wkn().isEmpty()  ? QStringLiteral("-") : s.share.wkn(),
                          s.share.isin().isEmpty() ? QStringLiteral("-") : s.share.isin(),
                          B::price(s.share.curPrice()), B::price(s.share.prevDayPrice()),
                          B::volume(v.volume));
    if (isSold(s))
        d.subtitle += QStringLiteral(" · vollständig verkauft");

    d.comparisonTitle = QStringLiteral("Tabellen \"Kompletter Marktwert\" und \"Kompletter Depotwert\"");
    const QList<CalculationBreakdownRow> rows = B::shareRows(v);
    for (int i = 0; i < rows.size(); ++i)
        d.comparison.append(toComparisonRow(i + 1, rows.at(i)));

    d.checkTitle = QStringLiteral("Abgleich Tabelle gegen Chart");
    if (s.chartExcluded) {
        d.checkNotes << QStringLiteral("Keine Tageswerte — die Aktie fehlt in beiden Charts.");
    } else if (!s.chart.date.isValid()) {
        d.checkNotes << QStringLiteral("Kein Chartpunkt.");
    } else {
        const PortfolioSharePoint& c = s.chart;
        d.checks = toCheckRows(B::shareChecks(s));
        d.checkNotes
            << QStringLiteral("Chart-Stichtag %1, Schlusskurs %2 € — die Tabelle rechnet mit dem "
                              "aktuellen Kurs, die Charts mit dem letzten Tageswert.")
                   .arg(dateText(c.date), B::price(c.price))
            << QStringLiteral("Davon durch Kursunterschied: %1 (%2 × (%3 − %4))")
                   .arg(B::euro(B::priceEffect(s)), B::volume(c.volume),
                        B::price(v.curPrice), B::price(c.price));
    }

    d.findings = toFindings(B::shareFindings(s));
    return d;
}

CalculationOverviewDetail PresenterCalculationOverview::buildTotalDetail(
    const CalculationReportInput& input)
{
    const CalculationFooterTotals totals = B::footerTotals(input.shares);

    CalculationOverviewDetail d;
    d.title    = QStringLiteral("Gesamt / Footer");
    d.subtitle = QStringLiteral("Summen über %1 — genau die Footer der Tabs \"Kompletter "
                                "Marktwert\" und \"Kompletter Depotwert\".")
                     .arg(countText(int(input.shares.size()), QStringLiteral("Aktie"),
                                    QStringLiteral("Aktien")));

    d.comparisonTitle = QStringLiteral("Footer \"Kompletter Marktwert\" und \"Kompletter Depotwert\"");
    const QList<CalculationBreakdownRow> rows = B::footerRows(totals);
    for (int i = 0; i < rows.size(); ++i)
        d.comparison.append(toComparisonRow(i + 1, rows.at(i)));

    d.checkTitle = QStringLiteral("Abgleich Footer gegen Chart");
    const PortfolioSeriesPoint& ct = input.chartTotal;
    if (!ct.date.isValid()) {
        d.checkNotes << QStringLiteral("Kein Chartpunkt — keine Aktie mit Tageswerten.");
    } else {
        const CalculationTotalChecks checks = B::totalChecks(input, totals);
        d.checks = toCheckRows(checks.checks);
        d.checkNotes
            << QStringLiteral("Chart-Stichtag %1 — die Tabellen rechnen mit dem aktuellen Kurs, "
                              "die Charts mit dem letzten Tageswert.").arg(dateText(ct.date))
            << QStringLiteral("Davon durch Kursunterschied: %1 (Summe je Aktie)")
                   .arg(B::euro(checks.byPrice));

        bool anyExcluded = false;
        for (const CalculationReportShare& s : input.shares)
            anyExcluded = anyExcluded || s.chartExcluded;
        if (anyExcluded) {
            d.checkNotes << QStringLiteral("Aktien ohne Tageswerte: Kurswert %1, Kompl. Entwicklung "
                                           "%2 — zählen in der Tabelle, fehlen im Chart.")
                                .arg(B::euro(checks.excludedCurValue),
                                     B::euro(checks.excludedProfit));
        }
    }

    d.findings = toFindings(B::findings(input));
    return d;
}

// ── toComparisonRow ───────────────────────────────────────────────────────────

CalculationOverviewComparisonRow PresenterCalculationOverview::toComparisonRow(
    int number, const CalculationBreakdownRow& r)
{
    const auto fmt = [&r](double value) { return r.percent ? B::percent(value) : B::euro(value); };
    const QString dash = QStringLiteral("–");

    CalculationOverviewComparisonRow row;
    row.number = QString::number(number);
    row.label  = r.label;
    row.market = r.hasMarket ? fmt(r.market) : dash;
    row.depot  = r.hasDepot  ? fmt(r.depot)  : dash;
    row.diff   = (r.hasMarket && r.hasDepot) ? fmt(r.depot - r.market) : dash;

    // Wie im Bericht: "beide:" bei gleichem Rechenweg, sonst beide Sichten
    // untereinander.
    row.way = (r.marketWay == r.depotWay)
                  ? QStringLiteral("beide: ") + r.marketWay
                  : QStringLiteral("Marktwert: %1\nDepotwert: %2").arg(r.marketWay, r.depotWay);

    row.usedIn = r.usedIn.join(QLatin1Char('\n'));

    // Tooltip: Rechenweg je Sicht mit den eingesetzten Zahlen, soweit es sie
    // gibt (nur für abgeleitete Werte, nicht für Summen über Transaktionen).
    QStringList tip;
    const auto addSide = [&tip](const QString& name, bool has, const QString& way,
                                const QString& figures) {
        if (!has)
            return;
        tip << QStringLiteral("%1: %2").arg(name, way);
        if (!figures.isEmpty())
            tip << QStringLiteral("    ") + figures;
    };
    addSide(QStringLiteral("Marktwert"), r.hasMarket, r.marketWay, r.marketFigures);
    addSide(QStringLiteral("Depotwert"), r.hasDepot,  r.depotWay,  r.depotFigures);
    row.toolTip = tip.join(QLatin1Char('\n'));
    return row;
}

// ── Hilfsfunktionen ───────────────────────────────────────────────────────────

CalculationOverviewTone PresenterCalculationOverview::toneOf(const CalculationFindings& f)
{
    if (!f.deviations.isEmpty())
        return CalculationOverviewTone::Deviation;
    if (!f.hints.isEmpty())
        return CalculationOverviewTone::Hint;
    return CalculationOverviewTone::Ok;
}

QString PresenterCalculationOverview::findingsText(const CalculationFindings& f)
{
    if (f.deviations.isEmpty() && f.hints.isEmpty())
        return QStringLiteral("keine Auffälligkeiten");

    QStringList parts;
    if (!f.deviations.isEmpty())
        parts << countText(int(f.deviations.size()), QStringLiteral("Abweichung"),
                           QStringLiteral("Abweichungen"));
    if (!f.hints.isEmpty())
        parts << countText(int(f.hints.size()), QStringLiteral("Hinweis"),
                           QStringLiteral("Hinweise"));
    return parts.join(QStringLiteral(", "));
}

QList<CalculationOverviewCheckRow> PresenterCalculationOverview::toCheckRows(
    const QList<CalculationCheck>& checks)
{
    QList<CalculationOverviewCheckRow> rows;
    for (const CalculationCheck& c : checks) {
        const auto fmt = [&c](double v) { return c.isVolume ? B::volume(v) : B::money(v); };

        CalculationOverviewCheckRow row;
        row.label      = c.label;
        row.tableValue = fmt(c.tableValue);
        row.chartValue = fmt(c.chartValue);
        row.diff       = fmt(c.diff);
        row.result     = c.resultText;
        switch (c.result) {
        case CalculationCheckResult::Ok:        row.tone = CalculationOverviewTone::Ok;        break;
        case CalculationCheckResult::Explained: row.tone = CalculationOverviewTone::Hint;      break;
        case CalculationCheckResult::Hint:      row.tone = CalculationOverviewTone::Hint;      break;
        case CalculationCheckResult::Deviation: row.tone = CalculationOverviewTone::Deviation; break;
        }
        rows.append(row);
    }
    return rows;
}

QList<CalculationOverviewFinding> PresenterCalculationOverview::toFindings(
    const CalculationFindings& f)
{
    QList<CalculationOverviewFinding> findings;
    for (const QString& text : f.deviations)
        findings.append({ text, CalculationOverviewTone::Deviation });
    for (const QString& text : f.hints)
        findings.append({ text, CalculationOverviewTone::Hint });
    return findings;
}

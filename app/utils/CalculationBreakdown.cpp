// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "CalculationBreakdown.h"

#include "ShareCalculator.h"
#include "ValueFormatter.h"

#include <cmath>

namespace {

// ── Bausteine für "Fließt ein in" ─────────────────────────────────────────────
// Spaltennamen genau wie die Überschriften in MainWindow::setupCentralWidget()
// (dort zweizeilig mit "\n", hier mit " / "). Die Footer-Werte stehen immer
// in derselben Spalte wie der Tabellenwert darüber — "→ Footer" meint deshalb
// stets die eigene Spalte. Bewusst knapp gehalten, damit die Spalte im Tab
// lesbar bleibt (27.10.2026).

const QString kBoth  = QStringLiteral("beide Tabs");
const QString kDepot = QStringLiteral("Kompletter Depotwert");

const QString kColPurchase = QStringLiteral("Einzahlung / Marktwert");
const QString kColComplete = QStringLiteral("Kpl. Einzahlung / Kpl. Marktwert");
const QString kColPerf     = QStringLiteral("Aktuelle Entwicklung");
const QString kColCPerf    = QStringLiteral("Komplette Entwicklung");
const QString kColBrokDiv  = QStringLiteral("Kosten / Dividenden");

const QString kRowPurchase = QStringLiteral("Einzahlung (gesamt)");
const QString kRowPerf     = QStringLiteral("Entwicklung (gesamt)");
const QString kRowDepot    = QStringLiteral("Aktueller Depotstand");

/// "beide Tabs: Spalte "Einzahlung / Marktwert" oben"
QString cell(const QString& where, const QString& column, bool top)
{
    return QStringLiteral("%1: Spalte \"%2\" %3")
        .arg(where, column, top ? QStringLiteral("oben") : QStringLiteral("unten"));
}

/// "→ Footer "Einzahlung (gesamt)" (Summe)" — in derselben Spalte.
QString footer(const QString& row, const QString& how = QStringLiteral("Summe"))
{
    return QStringLiteral("→ Footer \"%1\" (%2)").arg(row, how);
}

/// Für die Footer-Gegenüberstellung: "beide Footer: Zeile "…", Spalte "…"".
QString footerCell(const QString& where, const QString& row, const QString& column,
                   const QString& suffix = QString())
{
    QString text = QStringLiteral("%1: Zeile \"%2\", Spalte \"%3\"").arg(where, row, column);
    if (!suffix.isEmpty())
        text += QStringLiteral(" ") + suffix;
    return text;
}

const QString kMarketChart = QStringLiteral("Marktwert-Chart: Endwert (mit letztem Tageswert)");
const QString kDepotChart  = QStringLiteral("Depotwert-Chart: Endwert (mit letztem Tageswert)");

/// "a − b = c"
QString minus(double a, double b, double result)
{
    return QStringLiteral("%1 − %2 = %3").arg(CalculationBreakdown::money(a),
                                             CalculationBreakdown::money(b),
                                             CalculationBreakdown::money(result));
}

/// "a / b × 100 = c %"
QString ratio(double a, double b, double result)
{
    return QStringLiteral("%1 / %2 × 100 = %3").arg(CalculationBreakdown::money(a),
                                                   CalculationBreakdown::money(b),
                                                   CalculationBreakdown::percent(result));
}

} // namespace

// ── Formatierung ──────────────────────────────────────────────────────────────

double CalculationBreakdown::clean(double value, double tolerance)
{
    return std::abs(value) < tolerance ? 0.0 : value;
}

QString CalculationBreakdown::money(double value)
{
    return ValueFormatter::formatMoney(clean(value, 0.005));
}

QString CalculationBreakdown::euro(double value)
{
    return money(value) + QStringLiteral(" €");
}

QString CalculationBreakdown::volume(double value)
{
    return ValueFormatter::formatVolume(clean(value, 0.00005));
}

QString CalculationBreakdown::price(double value)
{
    return ValueFormatter::formatPrice(clean(value, 0.00005));
}

QString CalculationBreakdown::percent(double value)
{
    return ValueFormatter::formatPercent(value) + QStringLiteral(" %");
}

// ── shareRows ─────────────────────────────────────────────────────────────────

QList<CalculationBreakdownRow> CalculationBreakdown::shareRows(const ShareValues& v)
{
    // Bezeichnungen und Rechenwege unverändert aus dem Bericht v1.27.0
    // (dort bis 27.10.2026 direkt in buildShareSection()). Reihenfolge und
    // Wortlaut sind Teil des Berichts — nicht ohne Not ändern.
    const QString held = QStringLiteral("%1 × %2").arg(volume(v.volume), price(v.curPrice));

    QList<CalculationBreakdownRow> rows;

    {   // 1
        CalculationBreakdownRow r;
        r.label     = QStringLiteral("Einzahlung (gehaltene Stücke)");
        r.hasMarket = true;  r.market = v.purchaseValue;
        r.hasDepot  = true;  r.depot  = v.purchaseValueFinal;
        r.marketWay = QStringLiteral("Σ Reststück × Kaufkurs je Kauf");
        r.depotWay  = QStringLiteral("Σ je Kauf: Reststück × Kaufkurs + Kaufkosten × Reststück / Stück "
                                     "− anteiliger Rabatt (FIFO, Kosten je Kauf)");
        r.usedIn    = { cell(kBoth, kColPurchase, true), footer(kRowPurchase) };
        rows.append(r);
    }
    {   // 2
        CalculationBreakdownRow r;
        r.label     = QStringLiteral("Kurswert gehaltener Stücke");
        r.hasMarket = true;  r.market = v.curValue;
        r.hasDepot  = true;  r.depot  = v.curValue;
        r.marketWay = held;
        r.depotWay  = held;
        r.marketFigures = r.depotFigures = held + QStringLiteral(" = ") + money(v.curValue);
        r.usedIn    = { cell(kBoth, kColPurchase, false), footer(kRowDepot), kMarketChart };
        rows.append(r);
    }
    {   // 3
        CalculationBreakdownRow r;
        r.label     = QStringLiteral("Aktuelle Entwicklung");
        r.hasMarket = true;  r.market = v.profitLoss;
        r.hasDepot  = true;  r.depot  = v.profitLossFinal;
        r.marketWay = r.depotWay = QStringLiteral("Kurswert − Einzahlung");
        r.marketFigures = minus(v.curValue, v.purchaseValue, v.profitLoss);
        r.depotFigures  = minus(v.curValue, v.purchaseValueFinal, v.profitLossFinal);
        r.usedIn    = { cell(kBoth, kColPerf, true),
                        footer(kRowPerf, QStringLiteral("Markt: Summe, Depot: Depotstand − Einzahlung")) };
        rows.append(r);
    }
    {   // 4
        CalculationBreakdownRow r;
        r.label     = QStringLiteral("Aktuelle Entwicklung %");
        r.hasMarket = true;  r.market = v.profitLossPct;
        r.hasDepot  = true;  r.depot  = v.profitLossPctFinal;
        r.percent   = true;
        r.marketWay = r.depotWay = QStringLiteral("Aktuelle Entwicklung / Einzahlung");
        r.marketFigures = ratio(v.profitLoss, v.purchaseValue, v.profitLossPct);
        r.depotFigures  = ratio(v.profitLossFinal, v.purchaseValueFinal, v.profitLossPctFinal);
        r.usedIn    = { cell(kBoth, kColPerf, false) };
        rows.append(r);
    }
    {   // 5
        CalculationBreakdownRow r;
        r.label     = QStringLiteral("Kosten gesamt");
        r.hasDepot  = true;  r.depot = v.totalBrokerage;
        r.marketWay = QStringLiteral("nicht enthalten");
        r.depotWay  = QStringLiteral("alle Kosteneinträge (Kauf, Verkauf, freistehend) − Rabatt");
        r.usedIn    = { cell(kDepot, kColBrokDiv, true), footer(QStringLiteral("Kosten (ges.)")) };
        rows.append(r);
    }
    {   // 6
        CalculationBreakdownRow r;
        r.label     = QStringLiteral("Dividenden netto");
        r.hasDepot  = true;  r.depot = v.totalDividend;
        r.marketWay = QStringLiteral("nicht enthalten");
        r.depotWay  = QStringLiteral("Σ Dividende brutto − Steuern");
        r.usedIn    = { cell(kDepot, kColBrokDiv, false), footer(QStringLiteral("Dividenden (ges.)")),
                        QStringLiteral("steckt im Kpl. Wert (Nr. 11)") };
        rows.append(r);
    }
    {   // 7
        CalculationBreakdownRow r;
        r.label     = QStringLiteral("Verkäufe (Markt brutto / Depot netto)");
        r.hasMarket = true;  r.market = v.salePayoutMarket;
        r.hasDepot  = true;  r.depot  = v.salePayoutFinal;
        r.marketWay = QStringLiteral("Σ Stück × Verkaufskurs (ohne Kosten, ohne Steuern)");
        r.depotWay  = QStringLiteral("Σ Stück × Verkaufskurs − Kosten + Rabatt − Steuern");
        r.usedIn    = { QStringLiteral("keine eigene Spalte — steckt in Nr. 11") };
        rows.append(r);
    }
    {   // 8
        CalculationBreakdownRow r;
        r.label     = QStringLiteral("Realisiert rein Kurs");
        r.hasMarket = true;  r.market = v.saleProfitLoss;
        r.marketWay = QStringLiteral("Verkäufe brutto − Kaufwert verkaufter Stücke; nur informativ, "
                                     "steckt bereits im Kpl. Marktwert");
        r.depotWay  = QStringLiteral("nicht verwendet");
        r.usedIn    = { QStringLiteral("keine eigene Spalte — nur informativ") };
        rows.append(r);
    }
    {   // 9
        CalculationBreakdownRow r;
        r.label     = QStringLiteral("Realisiert mit Kosten und Steuern");
        r.hasDepot  = true;  r.depot = v.saleProfitLossFinal;
        r.marketWay = QStringLiteral("nicht verwendet");
        r.depotWay  = QStringLiteral("Verkäufe netto − Kaufwert verkaufter Stücke inkl. anteiliger "
                                     "Kaufkosten; nur informativ, steckt bereits im Kpl. Wert");
        r.usedIn    = { QStringLiteral("keine eigene Spalte — nur informativ") };
        rows.append(r);
    }
    {   // 10
        CalculationBreakdownRow r;
        r.label     = QStringLiteral("Kpl. Einzahlung");
        r.hasMarket = true;  r.market = v.completePurchaseMarket;
        r.hasDepot  = true;  r.depot  = v.completePurchase;
        r.marketWay = QStringLiteral("Σ Stück × Kaufkurs aller Käufe");
        r.depotWay  = QStringLiteral("alle Käufe + Kaufgebühren − Rabatt + freistehende Kosten");
        r.usedIn    = { cell(kBoth, kColComplete, true), footer(kRowPurchase) };
        rows.append(r);
    }
    {   // 11
        CalculationBreakdownRow r;
        r.label     = QStringLiteral("Kpl. Marktwert / Kpl. Wert");
        r.hasMarket = true;  r.market = v.completeCurValueMarket;
        r.hasDepot  = true;  r.depot  = v.completeCurValue;
        r.marketWay = QStringLiteral("Kurswert + Verkäufe brutto");
        r.depotWay  = QStringLiteral("Kurswert + Verkäufe netto + Dividenden netto");
        r.marketFigures = QStringLiteral("%1 + %2 = %3")
                              .arg(money(v.curValue), money(v.salePayoutMarket),
                                   money(v.completeCurValueMarket));
        r.depotFigures  = QStringLiteral("%1 + %2 + %3 = %4")
                              .arg(money(v.curValue), money(v.salePayoutFinal),
                                   money(v.totalDividend), money(v.completeCurValue));
        r.usedIn    = { cell(kBoth, kColComplete, false), footer(kRowDepot) };
        rows.append(r);
    }
    {   // 12
        CalculationBreakdownRow r;
        r.label     = QStringLiteral("Komplette Entwicklung");
        r.hasMarket = true;  r.market = v.completeProfitLossMarket;
        r.hasDepot  = true;  r.depot  = v.completeProfitLoss;
        r.marketWay = QStringLiteral("Kpl. Marktwert − Kpl. Einzahlung");
        r.depotWay  = QStringLiteral("Kpl. Wert − Kpl. Einzahlung");
        r.marketFigures = minus(v.completeCurValueMarket, v.completePurchaseMarket,
                                v.completeProfitLossMarket);
        r.depotFigures  = minus(v.completeCurValue, v.completePurchase, v.completeProfitLoss);
        r.usedIn    = { cell(kBoth, kColCPerf, true),
                        footer(kRowPerf, QStringLiteral("Kpl. Wert − Kpl. Einzahlung")), kDepotChart };
        rows.append(r);
    }
    {   // 13
        CalculationBreakdownRow r;
        r.label     = QStringLiteral("Komplette Entwicklung %");
        r.hasMarket = true;  r.market = v.completeProfitPctMarket;
        r.hasDepot  = true;  r.depot  = v.completeProfitPct;
        r.percent   = true;
        r.marketWay = r.depotWay = QStringLiteral("Komplette Entwicklung / Kpl. Einzahlung");
        r.marketFigures = ratio(v.completeProfitLossMarket, v.completePurchaseMarket,
                                v.completeProfitPctMarket);
        r.depotFigures  = ratio(v.completeProfitLoss, v.completePurchase, v.completeProfitPct);
        r.usedIn    = { cell(kBoth, kColCPerf, false) + QStringLiteral(" und Pfeil davor") };
        rows.append(r);
    }
    {   // 14
        CalculationBreakdownRow r;
        r.label     = QStringLiteral("Beitrag Footer \"Aktueller Depotstand\"");
        r.hasMarket = true;  r.market = v.curValue;
        r.hasDepot  = true;  r.depot  = v.curValue;
        r.marketWay = r.depotWay = QStringLiteral("Kurswert");
        r.usedIn    = { footerCell(QStringLiteral("beide Footer"), kRowDepot, kColPurchase,
                                   QStringLiteral("(Summe)")) };
        rows.append(r);
    }
    return rows;
}

// ── priceEffect / chartDevelopment / shareTitle ───────────────────────────────

double CalculationBreakdown::priceEffect(const CalculationReportShare& s)
{
    if (s.chartExcluded || !s.chart.date.isValid())
        return 0.0;
    return ShareCalculator::roundAway(s.chart.volume * s.values.curPrice)
         - s.chart.holdingsValue;
}

double CalculationBreakdown::chartDevelopment(const PortfolioSharePoint& p)
{
    return ShareCalculator::roundAway(p.holdingsValue + p.realizedGain + p.dividends
                                      - p.costs - p.purchaseValueHeld);
}

QString CalculationBreakdown::shareTitle(const CalculationReportShare& s)
{
    return s.share.name().isEmpty() ? s.share.guid() : s.share.name();
}

// ── check ─────────────────────────────────────────────────────────────────────

CalculationCheck CalculationBreakdown::check(const QString& subject, const QString& label,
                                             double tableValue, double chartValue, bool isVolume,
                                             bool hintOnly, double priceExplained,
                                             double excludedExplained)
{
    // Logik und Texte unverändert aus dem Bericht v1.27.0 (dort bis
    // 27.10.2026 als compare()).
    CalculationCheck c;
    c.label      = label;
    c.tableValue = tableValue;
    c.chartValue = chartValue;
    c.diff       = tableValue - chartValue;
    c.isVolume   = isVolume;

    const double tolerance   = isVolume ? CalculationReport::kVolumeTolerance
                                        : CalculationReport::kMoneyTolerance;
    const bool   hasPrice    = std::abs(priceExplained)    >= tolerance;
    const bool   hasExcluded = std::abs(excludedExplained) >= tolerance;
    const double explained   = (hasPrice ? priceExplained : 0.0)
                             + (hasExcluded ? excludedExplained : 0.0);
    const bool   ok          = std::abs(c.diff) < tolerance;
    const bool   byKnown     = !ok && (hasPrice || hasExcluded)
                               && std::abs(c.diff - explained) < tolerance;
    const auto   fmt         = [isVolume](double v) { return isVolume ? volume(v) : money(v); };

    QString reason;
    if (hasPrice && hasExcluded)
        reason = QStringLiteral("durch Kurs und Aktien o. Tageswerte erklärt");
    else if (hasExcluded)
        reason = QStringLiteral("durch Aktien o. Tageswerte erklärt");
    else
        reason = QStringLiteral("durch Kurs erklärt");

    if (ok) {
        c.result     = CalculationCheckResult::Ok;
        c.resultText = QStringLiteral("ok");
        return c;
    }
    if (byKnown) {
        c.result     = CalculationCheckResult::Explained;
        c.resultText = reason;
    } else if (hintOnly) {
        c.result     = CalculationCheckResult::Hint;
        c.resultText = QStringLiteral("Hinweis");
    } else {
        c.result     = CalculationCheckResult::Deviation;
        c.resultText = QStringLiteral("ABWEICHUNG");
    }

    QString prefix;
    if (byKnown)
        prefix = QStringLiteral("(Hinweis, %1) ").arg(reason);
    else if (hintOnly)
        prefix = QStringLiteral("(Hinweis) ");

    QString line = QStringLiteral("%1%2: %3 — Tabelle %4, Chart %5, Differenz %6")
                       .arg(prefix, subject, label, fmt(tableValue), fmt(chartValue), fmt(c.diff));
    if (!byKnown && !hintOnly && (hasPrice || hasExcluded)) {
        if (hasPrice)
            line += QStringLiteral(", davon durch Kurs %1").arg(fmt(priceExplained));
        if (hasExcluded)
            line += QStringLiteral(", durch Aktien ohne Tageswerte %1").arg(fmt(excludedExplained));
        line += QStringLiteral(", Rest %1").arg(fmt(c.diff - explained));
    }
    c.finding = line;
    return c;
}

// ── shareChecks / shareFindings ───────────────────────────────────────────────

QList<CalculationCheck> CalculationBreakdown::shareChecks(const CalculationReportShare& s)
{
    if (s.chartExcluded || !s.chart.date.isValid())
        return {};

    const ShareValues&         v       = s.values;
    const PortfolioSharePoint& c       = s.chart;
    const QString              name    = shareTitle(s);
    const double               byPrice = priceEffect(s);

    return {
        check(name, QStringLiteral("Bestand (Stück)"), v.volume, c.volume, true),
        check(name, QStringLiteral("Kurs (Tabelle aktuell / Chart Schluss)"),
              v.curPrice, c.price, true, /*hintOnly*/ true),
        check(name, QStringLiteral("Kaufwert gehalten (ohne Gebühren)"),
              v.purchaseValue, c.purchaseValueHeld, false),
        check(name, QStringLiteral("Marktwert / Marktwert-Chart"),
              v.curValue, c.marketValue, false, false, byPrice),
        check(name, QStringLiteral("Kompl. Entwicklung Depotwert / Depot-Chart"),
              v.completeProfitLoss, chartDevelopment(c), false, false, byPrice)
    };
}

namespace {

void addFinding(CalculationFindings& findings, const CalculationCheck& c)
{
    switch (c.result) {
    case CalculationCheckResult::Ok:
        break;
    case CalculationCheckResult::Deviation:
        findings.deviations << c.finding;
        break;
    case CalculationCheckResult::Explained:
    case CalculationCheckResult::Hint:
        findings.hints << c.finding;
        break;
    }
}

} // namespace

CalculationFindings CalculationBreakdown::shareFindings(const CalculationReportShare& s)
{
    CalculationFindings findings;
    if (s.chartExcluded) {
        findings.deviations << QStringLiteral("%1: keine Tageswerte, fehlt in beiden Charts")
                                   .arg(shareTitle(s));
        return findings;
    }
    for (const CalculationCheck& c : shareChecks(s))
        addFinding(findings, c);
    return findings;
}

// ── footerTotals / footerRows ─────────────────────────────────────────────────

CalculationFooterTotals CalculationBreakdown::footerTotals(const QList<CalculationReportShare>& shares)
{
    QList<ShareValues> values;
    values.reserve(shares.size());
    for (const CalculationReportShare& s : shares)
        values.append(s.values);

    // Exakt die Aufrufe aus MainWindow::updatePortfolioFooters().
    CalculationFooterTotals t;
    ShareCalculator::portfolioTotalsMarket(values, t.marketPurchase, t.marketCurValue,
                                           t.marketProfit, t.marketProfitPct);
    ShareCalculator::portfolioCompleteTotalsMarket(values, t.marketCompletePurchase,
                                                   t.marketCompleteValue, t.marketCompleteProfit,
                                                   t.marketCompleteProfitPct);
    double brokDividend = 0.0;
    ShareCalculator::portfolioTotalsFinal(values, brokDividend, t.depotPurchase, t.depotCurValue,
                                          t.depotProfit, t.depotProfitPct,
                                          t.depotCompletePurchase, t.depotCompleteValue,
                                          t.depotCompleteProfit, t.depotCompleteProfitPct);
    for (const ShareValues& v : values) {
        t.depotBrokerage += v.totalBrokerage;
        t.depotDividend  += v.totalDividend;
    }
    return t;
}

QList<CalculationBreakdownRow> CalculationBreakdown::footerRows(const CalculationFooterTotals& t)
{
    QList<CalculationBreakdownRow> rows;

    const auto both = [](const QString& row, const QString& column, const QString& suffix = QString()) {
        return QStringList{ footerCell(QStringLiteral("beide Footer"), row, column, suffix) };
    };

    {   // 1
        CalculationBreakdownRow r;
        r.label     = QStringLiteral("Einzahlung (gesamt)");
        r.hasMarket = true;  r.market = t.marketPurchase;
        r.hasDepot  = true;  r.depot  = t.depotPurchase;
        r.marketWay = r.depotWay = QStringLiteral("Σ Einzahlung (gehaltene Stücke) aller Aktien");
        r.usedIn    = both(kRowPurchase, kColPurchase);
        rows.append(r);
    }
    {   // 2
        CalculationBreakdownRow r;
        r.label     = QStringLiteral("Kosten (ges.)");
        r.hasDepot  = true;  r.depot = t.depotBrokerage;
        r.marketWay = QStringLiteral("nicht enthalten");
        r.depotWay  = QStringLiteral("Σ Kosten gesamt aller Aktien");
        r.usedIn    = { footerCell(QStringLiteral("Footer Depotwert"), kRowPerf, kColBrokDiv,
                                   QStringLiteral("oben")) };
        rows.append(r);
    }
    {   // 3
        CalculationBreakdownRow r;
        r.label     = QStringLiteral("Dividenden (ges.)");
        r.hasDepot  = true;  r.depot = t.depotDividend;
        r.marketWay = QStringLiteral("nicht enthalten");
        r.depotWay  = QStringLiteral("Σ Dividenden netto aller Aktien");
        r.usedIn    = { footerCell(QStringLiteral("Footer Depotwert"), kRowPerf, kColBrokDiv,
                                   QStringLiteral("unten")) };
        rows.append(r);
    }
    {   // 4
        CalculationBreakdownRow r;
        r.label     = QStringLiteral("Entwicklung (gesamt)");
        r.hasMarket = true;  r.market = t.marketProfit;
        r.hasDepot  = true;  r.depot  = t.depotProfit;
        r.marketWay = QStringLiteral("Σ Aktuelle Entwicklung aller Aktien");
        r.depotWay  = QStringLiteral("Aktueller Depotstand − Einzahlung (gesamt)");
        r.depotFigures = minus(t.depotCurValue, t.depotPurchase, t.depotProfit);
        r.usedIn    = both(kRowPerf, kColPerf, QStringLiteral("oben"));
        rows.append(r);
    }
    {   // 5
        CalculationBreakdownRow r;
        r.label     = QStringLiteral("Entwicklung (gesamt) %");
        r.hasMarket = true;  r.market = t.marketProfitPct;
        r.hasDepot  = true;  r.depot  = t.depotProfitPct;
        r.percent   = true;
        r.marketWay = r.depotWay = QStringLiteral("Entwicklung (gesamt) / Einzahlung (gesamt)");
        r.marketFigures = ratio(t.marketProfit, t.marketPurchase, t.marketProfitPct);
        r.depotFigures  = ratio(t.depotProfit, t.depotPurchase, t.depotProfitPct);
        r.usedIn    = both(kRowPerf, kColPerf, QStringLiteral("unten"));
        rows.append(r);
    }
    {   // 6
        CalculationBreakdownRow r;
        r.label     = QStringLiteral("Aktueller Depotstand");
        r.hasMarket = true;  r.market = t.marketCurValue;
        r.hasDepot  = true;  r.depot  = t.depotCurValue;
        r.marketWay = r.depotWay = QStringLiteral("Σ Kurswert gehaltener Stücke aller Aktien");
        r.usedIn    = both(kRowDepot, kColPurchase);
        r.usedIn << kMarketChart;
        rows.append(r);
    }
    {   // 7
        CalculationBreakdownRow r;
        r.label     = QStringLiteral("Kpl. Einzahlung");
        r.hasMarket = true;  r.market = t.marketCompletePurchase;
        r.hasDepot  = true;  r.depot  = t.depotCompletePurchase;
        r.marketWay = r.depotWay = QStringLiteral("Σ Kpl. Einzahlung aller Aktien");
        r.usedIn    = both(kRowPurchase, kColComplete);
        rows.append(r);
    }
    {   // 8
        CalculationBreakdownRow r;
        r.label     = QStringLiteral("Kpl. Marktwert / Kpl. Wert");
        r.hasMarket = true;  r.market = t.marketCompleteValue;
        r.hasDepot  = true;  r.depot  = t.depotCompleteValue;
        r.marketWay = QStringLiteral("Σ Kpl. Marktwert aller Aktien");
        r.depotWay  = QStringLiteral("Σ Kpl. Wert aller Aktien");
        r.usedIn    = both(kRowDepot, kColComplete);
        rows.append(r);
    }
    {   // 9
        CalculationBreakdownRow r;
        r.label     = QStringLiteral("Komplette Entwicklung");
        r.hasMarket = true;  r.market = t.marketCompleteProfit;
        r.hasDepot  = true;  r.depot  = t.depotCompleteProfit;
        r.marketWay = QStringLiteral("Kpl. Marktwert − Kpl. Einzahlung");
        r.depotWay  = QStringLiteral("Kpl. Wert − Kpl. Einzahlung");
        r.marketFigures = minus(t.marketCompleteValue, t.marketCompletePurchase,
                                t.marketCompleteProfit);
        r.depotFigures  = minus(t.depotCompleteValue, t.depotCompletePurchase,
                                t.depotCompleteProfit);
        r.usedIn    = both(kRowPerf, kColCPerf, QStringLiteral("oben"));
        r.usedIn << kDepotChart;
        rows.append(r);
    }
    {   // 10
        CalculationBreakdownRow r;
        r.label     = QStringLiteral("Komplette Entwicklung %");
        r.hasMarket = true;  r.market = t.marketCompleteProfitPct;
        r.hasDepot  = true;  r.depot  = t.depotCompleteProfitPct;
        r.percent   = true;
        r.marketWay = r.depotWay = QStringLiteral("Komplette Entwicklung / Kpl. Einzahlung");
        r.marketFigures = ratio(t.marketCompleteProfit, t.marketCompletePurchase,
                                t.marketCompleteProfitPct);
        r.depotFigures  = ratio(t.depotCompleteProfit, t.depotCompletePurchase,
                                t.depotCompleteProfitPct);
        r.usedIn    = both(kRowPerf, kColCPerf, QStringLiteral("unten und Pfeil davor"));
        rows.append(r);
    }
    return rows;
}

// ── totalChecks / findings ────────────────────────────────────────────────────

CalculationTotalChecks CalculationBreakdown::totalChecks(const CalculationReportInput& input,
                                                         const CalculationFooterTotals& t)
{
    CalculationTotalChecks result;
    for (const CalculationReportShare& s : input.shares) {
        result.byPrice += priceEffect(s);
        // Aktien ohne Tageswerte zählen im Footer, fehlen aber im Chart
        // (Nessies Nebenbefund 26.10.2026) — ihr Footer-Beitrag gilt wie der
        // Kursunterschied als Erklärung.
        if (s.chartExcluded) {
            result.excludedCurValue += s.values.curValue;
            result.excludedProfit   += s.values.completeProfitLoss;
        }
    }

    const PortfolioSeriesPoint& ct = input.chartTotal;
    if (!ct.date.isValid())
        return result;

    const QString total = QStringLiteral("GESAMT");
    result.checks = {
        check(total, QStringLiteral("Marktwert: Footer \"Aktueller Depotstand\""),
              t.marketCurValue, ct.marketValue, false, false, result.byPrice,
              result.excludedCurValue),
        check(total, QStringLiteral("Depotwert: Footer \"Komplette Entwicklung\""),
              t.depotCompleteProfit, ct.development, false, false, result.byPrice,
              result.excludedProfit),
        check(total, QStringLiteral("Depotwert: Footer \"Aktueller Depotstand\""),
              t.depotCurValue, ct.holdingsValue, false, false, result.byPrice,
              result.excludedCurValue)
    };
    return result;
}

CalculationFindings CalculationBreakdown::findings(const CalculationReportInput& input)
{
    CalculationFindings all;
    for (const CalculationReportShare& s : input.shares) {
        const CalculationFindings f = shareFindings(s);
        all.deviations << f.deviations;
        all.hints      << f.hints;
    }
    const CalculationTotalChecks totals = totalChecks(input, footerTotals(input.shares));
    for (const CalculationCheck& c : totals.checks)
        addFinding(all, c);
    return all;
}

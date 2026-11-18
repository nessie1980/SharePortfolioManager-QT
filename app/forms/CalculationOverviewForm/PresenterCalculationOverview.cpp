// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "PresenterCalculationOverview.h"

#include <cmath>

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

// ── Gruppen zum Aufklappen (ergänzt 18.11.2026, v1.31.0) ─────────────────────
// Spalten, Summenzeilen und Fußnoten wie die Tabellen des Berechnungsberichts;
// die Zahlen kommen aus CalculationBreakdown (seit v1.30.1 gemeinsam mit dem
// Bericht).

QString stk(double v) { return B::volume(v) + QStringLiteral(" Stk."); }
QString px(double v)  { return B::price(v) + QStringLiteral(" €"); }

bool differs(double a, double b)
{
    return std::abs(a - b) >= CalculationReport::kVolumeTolerance;
}

/// Tabelle mit Spaltenköpfen; @p right markiert die rechtsbündigen Spalten.
CalculationOverviewTable makeTable(const QStringList& headers, const QList<bool>& right)
{
    CalculationOverviewTable t;
    t.headers      = headers;
    t.rightAligned = right;
    return t;
}

QString groupTitle(const QString& name, qsizetype count)
{
    return QStringLiteral("%1 (%2)").arg(name).arg(count);
}

CalculationOverviewGroup buysGroup(const CalculationReportShare& s)
{
    CalculationOverviewGroup g;
    g.kind  = CalculationOverviewGroup::Kind::Buys;
    g.title = groupTitle(QStringLiteral("Käufe"), s.buys.size());
    if (s.buys.isEmpty()) {
        g.summary = QStringLiteral("keine");
        return g;
    }

    const CalculationBuyTable buys = B::buyTable(s);
    QStringList headers = { QStringLiteral("Datum"), QStringLiteral("Depot"), QStringLiteral("Stück"),
                            QStringLiteral("Kurs"), QStringLiteral("Kaufwert o. Kosten"),
                            QStringLiteral("Kosten"), QStringLiteral("Kaufwert m. Kosten"),
                            QStringLiteral("verk. gespeich."), QStringLiteral("verk. FIFO") };
    QList<bool> right = { false, false, true, true, true, true, true, true, true };
    if (buys.withSplits) {
        headers << QStringLiteral("Stück heute") << QStringLiteral("Kurs heute");
        right   << true << true;
    }
    CalculationOverviewTable t = makeTable(headers, right);

    for (const CalculationBuyRow& b : buys.rows) {
        QStringList row = { dateText(b.date), b.depotNumber, stk(b.volume), px(b.price),
                            B::euro(b.value), B::euro(b.costs), B::euro(b.valueWithCosts),
                            stk(b.soldStored), stk(b.soldFifo) };
        if (buys.withSplits)
            row << stk(b.volumeToday) << px(b.priceToday);
        t.rows.append(row);
    }
    // Wie im Bericht: mit Splits steht die Stück-Summe in "Stück heute".
    t.sumRow = { QStringLiteral("Summe"), QString(),
                 buys.withSplits ? QString() : stk(buys.sumVolume), QString(),
                 B::euro(buys.sumValue), B::euro(buys.sumCosts), B::euro(buys.sumValueWithCosts),
                 stk(buys.sumSoldStoredToday), stk(buys.sumSoldFifoToday) };
    if (buys.withSplits)
        t.sumRow << stk(buys.sumVolumeToday) << QString();

    t.note = QStringLiteral("\"Kosten\" = dem Kauf zugeordnete Gebühren abzüglich Rabatt. "
                            "\"verk. gespeich.\" = in der Datenbank gespeichert, daraus rechnet die "
                            "Tabelle den Bestand; \"verk. FIFO\" = alle Verkäufe neu nach FIFO "
                            "zugeteilt.");
    if (buys.withSplits)
        t.note += QStringLiteral(" Summen der Stückzahlen in heutiger Skala.");
    g.tables.append(t);

    g.summary = QStringLiteral("%1 · %2").arg(stk(buys.sumVolumeToday), B::euro(buys.sumValueWithCosts));
    return g;
}

CalculationOverviewGroup salesGroup(const CalculationReportShare& s)
{
    CalculationOverviewGroup g;
    g.kind  = CalculationOverviewGroup::Kind::Sales;
    g.title = groupTitle(QStringLiteral("Verkäufe"), s.sales.size());
    if (s.sales.isEmpty()) {
        g.summary = QStringLiteral("keine");
        return g;
    }

    const CalculationSaleTable sales = B::saleTable(s);
    CalculationOverviewTable t = makeTable(
        { QStringLiteral("Datum"), QStringLiteral("Depot"), QStringLiteral("Stück"),
          QStringLiteral("Kurs"), QStringLiteral("Verkaufswert"), QStringLiteral("Gebühren"),
          QStringLiteral("Rabatt"), QStringLiteral("Steuern"), QStringLiteral("Auszahlung") },
        { false, false, true, true, true, true, true, true, true });
    for (const CalculationSaleRow& r : sales.rows) {
        t.rows.append(QStringList{ dateText(r.date), r.depotNumber, stk(r.volume), px(r.price),
                        B::euro(r.value), B::euro(r.fees), B::euro(r.reduction),
                        B::euro(r.taxes), B::euro(r.payout) });
    }
    t.sumRow = { QStringLiteral("Summe"), QString(), stk(sales.sumVolumeToday), QString(),
                 B::euro(sales.sumValue), B::euro(sales.sumFees), B::euro(sales.sumReduction),
                 B::euro(sales.sumTaxes), B::euro(sales.sumPayout) };
    t.note = QStringLiteral("Auszahlung = Verkaufswert − Gebühren + Rabatt − Steuern.");
    if (!s.splits.isEmpty())
        t.note += QStringLiteral(" Summe der Stückzahlen in heutiger Skala.");
    g.tables.append(t);

    g.summary = QStringLiteral("%1 · Auszahlung %2").arg(stk(sales.sumVolumeToday),
                                                          B::euro(sales.sumPayout));
    return g;
}

CalculationOverviewGroup dividendsGroup(const CalculationReportShare& s)
{
    CalculationOverviewGroup g;
    g.kind  = CalculationOverviewGroup::Kind::Dividends;
    g.title = groupTitle(QStringLiteral("Dividenden"), s.dividends.size());
    if (s.dividends.isEmpty()) {
        g.summary = QStringLiteral("keine");
        return g;
    }

    const CalculationDividendTable dividends = B::dividendTable(s);
    CalculationOverviewTable t = makeTable(
        { QStringLiteral("Zahltag"), QStringLiteral("Stück"), QStringLiteral("je Stück"),
          QStringLiteral("Brutto"), QStringLiteral("Quellensteuer"),
          QStringLiteral("Kapitalertragst."), QStringLiteral("Soli"),
          QStringLiteral("Steuern ges."), QStringLiteral("Netto") },
        { false, true, true, true, true, true, true, true, true });
    for (const CalculationDividendRow& d : dividends.rows) {
        t.rows.append(QStringList{ dateText(d.date), stk(d.volume),
                        B::price(d.rate) + QLatin1Char(' ') + d.currency,
                        B::euro(d.gross), B::euro(d.taxAtSource), B::euro(d.capitalGainsTax),
                        B::euro(d.solidarityTax), B::euro(d.taxes), B::euro(d.net) });
    }
    t.sumRow = { QStringLiteral("Summe"), QString(), QString(), B::euro(dividends.sumGross),
                 B::euro(dividends.sumTaxAtSource), B::euro(dividends.sumCapitalGainsTax),
                 B::euro(dividends.sumSolidarityTax), B::euro(dividends.sumTaxes),
                 B::euro(dividends.sumNet) };
    t.note = QStringLiteral("Brutto bei Fremdwährung in Euro umgerechnet; Netto = Brutto − "
                            "Steuern ges.");
    g.tables.append(t);

    g.summary = QStringLiteral("netto %1").arg(B::euro(dividends.sumNet));
    return g;
}

CalculationOverviewGroup brokeragesGroup(const CalculationReportShare& s)
{
    CalculationOverviewGroup g;
    g.kind  = CalculationOverviewGroup::Kind::Brokerages;
    g.title = groupTitle(QStringLiteral("Kosten"), s.brokerages.size());
    if (s.brokerages.isEmpty()) {
        g.summary = QStringLiteral("keine");
        return g;
    }

    const CalculationBrokerageTable brokerages = B::brokerageTable(s);
    CalculationOverviewTable t = makeTable(
        { QStringLiteral("Datum"), QStringLiteral("Zuordnung"), QStringLiteral("Provision"),
          QStringLiteral("Makler"), QStringLiteral("Börse"), QStringLiteral("Brutto"),
          QStringLiteral("Rabatt"), QStringLiteral("Netto") },
        { false, false, true, true, true, true, true, true });
    for (const CalculationBrokerageRow& b : brokerages.rows) {
        QString assignment;
        switch (b.assignment) {
        case CalculationBrokerageAssignment::Buy:        assignment = QStringLiteral("Kauf");        break;
        case CalculationBrokerageAssignment::Sale:       assignment = QStringLiteral("Verkauf");     break;
        case CalculationBrokerageAssignment::Standalone: assignment = QStringLiteral("freistehend"); break;
        }
        t.rows.append(QStringList{ dateText(b.date), assignment, B::euro(b.provision), B::euro(b.brokerFee),
                        B::euro(b.traderFee), B::euro(b.gross), B::euro(b.reduction),
                        B::euro(b.net) });
    }
    t.sumRow = { QStringLiteral("Summe"), QString(), B::euro(brokerages.sumProvision),
                 B::euro(brokerages.sumBrokerFee), B::euro(brokerages.sumTraderFee),
                 B::euro(brokerages.sumGross), B::euro(brokerages.sumReduction),
                 B::euro(brokerages.sumNet) };
    t.note = QStringLiteral("Brutto = Provision + Makler + Börse; Netto = Brutto − Rabatt.");
    g.tables.append(t);

    g.summary = QStringLiteral("netto %1").arg(B::euro(brokerages.sumNet));
    return g;
}

CalculationOverviewGroup splitsGroup(const CalculationReportShare& s)
{
    CalculationOverviewGroup g;
    g.kind  = CalculationOverviewGroup::Kind::Splits;
    g.title = groupTitle(QStringLiteral("Splits"), s.splits.size());
    if (s.splits.isEmpty()) {
        g.summary = QStringLiteral("keine");
        return g;
    }

    // Splits sind Stammdaten ohne Rechnung — direkt aus ShareSplitObject,
    // wie im Bericht.
    CalculationOverviewTable t = makeTable(
        { QStringLiteral("Ex-Tag"), QStringLiteral("Verhältnis"), QStringLiteral("Faktor") },
        { false, true, true });
    QStringList brief;
    for (const ShareSplitObject& split : s.splits) {
        const QString ratio = QStringLiteral("%1 : %2").arg(B::volume(split.ratioNew()),
                                                            B::volume(split.ratioOld()));
        t.rows.append(QStringList{ dateText(split.date()), ratio, B::volume(split.factor()) });
        brief << QStringLiteral("Faktor %1 am %2").arg(B::volume(split.factor()),
                                                        dateText(split.date()));
    }
    t.note = QStringLiteral("Käufe und Verkäufe stehen in Beleg-Skala; die Spalten \"heute\" "
                            "rechnen in heutige Skala um.");
    g.tables.append(t);

    g.summary = brief.join(QStringLiteral(", "));
    return g;
}

/**
 * Bestand / FIFO: je Kauf gespeichert gegen FIFO, darunter die fünf
 * Bestandszeilen. Eine Abweichung färbt die Zellen und klappt die Gruppe auf
 * (Nessie, 17.11.2026, Punkt 1b), wird aber keine Auffälligkeit (Punkt 4b).
 */
CalculationOverviewGroup stockGroup(const CalculationReportShare& s)
{
    using Tone = CalculationOverviewTone;

    CalculationOverviewGroup g;
    g.kind  = CalculationOverviewGroup::Kind::Stock;
    g.title = QStringLiteral("Bestand / FIFO");

    const CalculationBuyTable     buys  = B::buyTable(s);
    const CalculationStockSummary stock = B::stockSummary(s);

    int deviatingBuys = 0;
    if (!buys.rows.isEmpty()) {
        CalculationOverviewTable t = makeTable(
            { QStringLiteral("Kauf vom"), QStringLiteral("Depot"), QStringLiteral("Stück"),
              QStringLiteral("verkauft gespeichert"), QStringLiteral("verkauft FIFO"),
              QStringLiteral("Differenz"), QStringLiteral("Rest FIFO") },
            { false, false, true, true, true, true, true });
        for (const CalculationBuyRow& b : buys.rows) {
            const double diff = b.soldStored - b.soldFifo;
            const bool   bad  = differs(b.soldStored, b.soldFifo);
            if (bad)
                ++deviatingBuys;
            t.rows.append(QStringList{ dateText(b.date), b.depotNumber, stk(b.volume), stk(b.soldStored),
                            stk(b.soldFifo), B::volume(diff), stk(b.remainingFifo) });
            t.tones.append(QList<Tone>{ Tone::Normal, Tone::Normal, Tone::Normal, Tone::Normal, Tone::Normal,
                             bad ? Tone::Deviation : Tone::Ok, Tone::Normal });
        }
        t.sumRow = { QStringLiteral("Summe"), QString(),
                     buys.withSplits ? QString() : stk(buys.sumVolume),
                     stk(buys.sumSoldStoredToday), stk(buys.sumSoldFifoToday),
                     B::volume(buys.sumSoldStoredToday - buys.sumSoldFifoToday), stk(stock.fifo) };
        t.note = QStringLiteral("Differenz = gespeichert − FIFO. Je Kauf in Beleg-Skala");
        t.note += buys.withSplits ? QStringLiteral(", Summen in heutiger Skala.")
                                  : QStringLiteral(".");
        g.tables.append(t);
    }

    // Die fünf Bestandszeilen wie im Bericht ("Bestand (heutige Skala)").
    const bool fifoBad  = differs(stock.fifo, stock.computed);
    const bool tableBad = differs(stock.table, stock.computed) || differs(stock.table, stock.fifo);
    CalculationOverviewTable summary = makeTable(
        { QStringLiteral("Bestand (heutige Skala)"), QStringLiteral("Stück") }, { false, true });
    summary.rows = { { QStringLiteral("Gekauft gesamt"), B::volume(stock.bought) },
                     { QStringLiteral("Verkauft gesamt"), B::volume(stock.sold) },
                     { QStringLiteral("Bestand rechnerisch (gekauft − verkauft)"),
                       B::volume(stock.computed) },
                     { QStringLiteral("Bestand nach FIFO-Nachrechnung"), B::volume(stock.fifo) },
                     { QStringLiteral("Bestand Tabelle"), B::volume(stock.table) } };
    summary.tones = { {}, {}, {},
                      { Tone::Normal, fifoBad ? Tone::Deviation : Tone::Normal },
                      { Tone::Normal, tableBad ? Tone::Deviation : Tone::Ok } };
    g.tables.append(summary);

    QStringList parts;
    if (deviatingBuys > 0) {
        parts << (deviatingBuys == 1 ? QStringLiteral("1 Kauf abweichend")
                                     : QStringLiteral("%1 Käufe abweichend").arg(deviatingBuys));
    }
    parts << ((fifoBad || tableBad) ? QStringLiteral("Bestand abweichend")
                                    : QStringLiteral("Bestand ok"));
    g.summary  = parts.join(QStringLiteral(" · "));
    g.tone     = (deviatingBuys > 0 || fifoBad || tableBad) ? Tone::Deviation : Tone::Ok;
    g.expanded = (g.tone == Tone::Deviation);
    return g;
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
        // Seit 30.10.2026 bewerten die Charts am rechten Rand mit dem
        // aktuellen Kurs; ein Unterschied bleibt nur, wenn der letzte
        // Tageswert neuer ist als der letzte Kursabruf.
        if (s.substitutePrices)
            d.checkNotes << QStringLiteral("Keine Tageswerte — die Charts bewerten die Aktie mit den "
                                           "Kursen ihrer Käufe und Verkäufe (Ersatzkurse).");
        // Kursart am Chart-Stichtag; die Erklärung zum älteren Kursabruf nur,
        // wenn der Kurs bei vorhandenem Bestand tatsächlich abweicht.
        const bool held = std::abs(c.volume) >= CalculationReport::kVolumeTolerance;
        QString priceNote;
        if (c.currentPrice) {
            priceNote = QStringLiteral("Chart-Stichtag %1, Kurs %2 € (aktueller Kurs, wie in der Tabelle).");
        } else if (c.substitutePrice) {
            priceNote = QStringLiteral("Chart-Stichtag %1, Kurs %2 € (Ersatzkurs aus Kauf/Verkauf).");
        } else if (held && std::abs(v.curPrice - c.price) >= CalculationReport::kVolumeTolerance) {
            priceNote = QStringLiteral("Chart-Stichtag %1, Kurs %2 € (Schlusskurs) — der letzte "
                                       "Tageswert ist neuer als der letzte Kursabruf, die Tabelle "
                                       "rechnet noch mit dem älteren aktuellen Kurs.");
        } else {
            priceNote = QStringLiteral("Chart-Stichtag %1, Kurs %2 € (Schlusskurs).");
        }
        d.checkNotes
            << priceNote.arg(dateText(c.date), B::price(c.price))
            << QStringLiteral("Davon durch Kursunterschied: %1 (%2 × (%3 − %4))")
                   .arg(B::euro(B::priceEffect(s)), B::volume(c.volume),
                        B::price(v.curPrice), B::price(c.price));
    }

    d.findings = toFindings(B::shareFindings(s));
    applyGroupStates(d);

    // Transaktionen und Bestand (ergänzt 18.11.2026, v1.31.0), in dieser
    // Reihenfolge (Nessies Entscheidung 17.11.2026).
    d.groups = { buysGroup(s), salesGroup(s), dividendsGroup(s), brokeragesGroup(s),
                 splitsGroup(s), stockGroup(s) };
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
            << (ct.usesCurrentPrice
                    ? QStringLiteral("Chart-Stichtag %1 (aktueller Kurs).").arg(dateText(ct.date))
                    : QStringLiteral("Chart-Stichtag %1.").arg(dateText(ct.date)))
            << QStringLiteral("Davon durch Kursunterschied: %1 (Summe je Aktie)")
                   .arg(B::euro(checks.byPrice));

        bool anyExcluded = false;
        for (const CalculationReportShare& s : input.shares)
            anyExcluded = anyExcluded || s.chartExcluded;
        if (anyExcluded) {
            d.checkNotes << QStringLiteral("Aktien ohne Kursdaten (ohne Tageswerte und Transaktionen): "
                                           "Kurswert %1, Kompl. Entwicklung %2 — zählen in der "
                                           "Tabelle, fehlen im Chart.")
                                .arg(B::euro(checks.excludedCurValue),
                                     B::euro(checks.excludedProfit));
        }
    }

    d.findings = toFindings(B::findings(input));
    applyGroupStates(d);
    return d;
}

// ── applyGroupStates ──────────────────────────────────────────────────────────

void PresenterCalculationOverview::applyGroupStates(CalculationOverviewDetail& d)
{
    using Tone = CalculationOverviewTone;

    // Abgleich: immer offen (Nessie, 17.11.2026, Punkt 7). Die Kurzinfo zählt
    // Abweichungen und Hinweise; ohne Abgleich nennt sie den Grund.
    int deviations = 0, hints = 0;
    for (const CalculationOverviewCheckRow& c : d.checks) {
        if (c.tone == Tone::Deviation) ++deviations;
        if (c.tone == Tone::Hint)      ++hints;
    }
    if (d.checks.isEmpty()) {
        d.checkSummary = QStringLiteral("kein Abgleich möglich");
        d.checkTone    = Tone::Hint;
    } else {
        CalculationFindings counts;
        for (int i = 0; i < deviations; ++i) counts.deviations << QString();
        for (int i = 0; i < hints; ++i)      counts.hints      << QString();
        d.checkSummary = (deviations == 0 && hints == 0) ? QStringLiteral("alles ok")
                                                         : findingsText(counts);
        d.checkTone    = toneOf(counts);
    }
    d.checkExpanded = true;

    // Auffälligkeiten: offen, sobald es einen Eintrag gibt — auch wenn es nur
    // Hinweise sind (Nessie, 17.11.2026, Punkt 8a).
    CalculationFindings counts;
    for (const CalculationOverviewFinding& f : d.findings) {
        if (f.tone == Tone::Deviation) counts.deviations << f.text;
        else                           counts.hints      << f.text;
    }
    d.findingsSummary  = d.findings.isEmpty() ? QStringLiteral("keine") : findingsText(counts);
    d.findingsTone     = toneOf(counts);
    d.findingsExpanded = !d.findings.isEmpty();
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

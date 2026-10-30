// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "CalculationReport.h"

#include "CalculationBreakdown.h"
#include "SaleFifoAllocator.h"
#include "ShareSplitAdjuster.h"
#include "ValueFormatter.h"

#include "../repositories/ShareRepository.h"
#include "../repositories/BuyRepository.h"
#include "../repositories/SaleRepository.h"
#include "../repositories/DividendRepository.h"
#include "../repositories/BrokerageRepository.h"
#include "../repositories/ShareSplitRepository.h"
#include "../forms/PortfolioChartForm/ModelPortfolioChart.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QHash>
#include <QLocale>
#include <QStringList>
#include <QTextStream>

#include <algorithm>
#include <numeric>

namespace {

// ── Formatierung ──────────────────────────────────────────────────────────────

constexpr int kLineWidth = 100;

// Seit 27.10.2026 in CalculationBreakdown, damit Bericht und Tab
// "Depot- vs. Marktwert" gleich formatieren; die kurzen Namen bleiben, weil
// der Bericht sie überall verwendet.
QString money(double value) { return CalculationBreakdown::money(value); }
QString euro(double value)  { return CalculationBreakdown::euro(value); }
QString vol(double value)   { return CalculationBreakdown::volume(value); }
QString price(double value) { return CalculationBreakdown::price(value); }

QString dateText(const QDate& date)
{
    return date.isValid() ? date.toString(QStringLiteral("dd.MM.yyyy")) : QStringLiteral("-");
}

QString rule(QChar c) { return QString(kLineWidth, c) + QLatin1Char('\n'); }

QString heading(const QString& title)
{
    return QLatin1Char('\n') + title + QLatin1Char('\n')
         + QString(title.size(), QLatin1Char('-')) + QLatin1Char('\n');
}

/// "  Bezeichnung ............      Wert   Rechenweg"
QString valueLine(const QString& label, const QString& value, const QString& formula = QString())
{
    QString line = QStringLiteral("  ") + label.leftJustified(44, QLatin1Char(' '))
                 + value.rightJustified(16, QLatin1Char(' '));
    if (!formula.isEmpty())
        line += QStringLiteral("   ") + formula;
    return line + QLatin1Char('\n');
}

/**
 * Einfache Texttabelle. Die angegebene Breite ist eine Mindestbreite: ist ein
 * Zellinhalt oder die Überschrift länger, wächst die Spalte für alle Zeilen
 * mit (Bugfix 23.10.2026, Nessies Rückmeldung: eine neunstellige Depotnummer
 * in einer achtstelligen Spalte verschob die Datenzeilen um ein Zeichen
 * gegenüber Kopf- und Summenzeile).
 */
class TextTable
{
public:
    struct Column { QString title; int width; bool right; };

    explicit TextTable(QList<Column> columns) : m_columns(std::move(columns)) {}

    void addRow(const QStringList& cells) { m_rows.append(cells); }
    /// Trennlinie, z. B. vor einer Summenzeile.
    void addRule() { m_rows.append(QStringList{ ruleMarker() }); }
    bool isEmpty() const { return m_rows.isEmpty(); }

    QString render() const
    {
        // Tatsächliche Breite je Spalte: Mindestbreite, Überschrift oder
        // längster Zellinhalt — was am größten ist.
        QList<int> widths;
        for (int i = 0; i < m_columns.size(); ++i) {
            int w = std::max<int>(m_columns.at(i).width, m_columns.at(i).title.size());
            for (const QStringList& row : m_rows) {
                if (isRule(row) || i >= row.size())
                    continue;
                w = std::max<int>(w, row.at(i).size());
            }
            widths.append(w);
        }

        QString out = formatRow(titles(), widths);
        out.reserve(out.size() * (m_rows.size() + 2));
        // Linie über alle Spalten. Eine linksbündige letzte Spalte ist
        // Freitext (z. B. der Rechenweg) und kann sehr lang werden — die Linie
        // endet dort mit der Überschrift statt mit dem längsten Text.
        int total = 0;
        for (int i = 0; i < widths.size(); ++i) {
            const bool freeText = (i + 1 == widths.size()) && !m_columns.at(i).right;
            total += (freeText ? int(m_columns.at(i).title.size()) : widths.at(i)) + 2;
        }
        const QString line = QStringLiteral("  ") + QString(total - 2, QLatin1Char('-')) + QLatin1Char('\n');
        out += line;
        const QString header = formatRow(titles(), widths) + line;

        // Bei langen Tabellen nach je kHeaderRepeatRows Datenzeilen die
        // Titelleiste (Überschrift und Linie) wiederholen, damit beim
        // Scrollen klar bleibt, welche Spalte was enthält (Nessies Wunsch
        // 23.10.2026). Nur zwischen Datenzeilen: nie am Tabellenende und nie
        // vor einer Summenzeile, also einer Zeile direkt nach einer Linie.
        int  rowsSinceHeader = 0;
        bool afterRule       = false;
        for (const QStringList& row : m_rows) {
            if (isRule(row)) {
                out += line;
                afterRule = true;
                continue;
            }
            if (!afterRule && rowsSinceHeader == CalculationReport::kHeaderRepeatRows) {
                out += header;
                rowsSinceHeader = 0;
            }
            out += formatRow(row, widths);
            ++rowsSinceHeader;
            afterRule = false;
        }
        return out;
    }

private:
    static QString ruleMarker() { return QStringLiteral("\x01rule"); }
    static bool isRule(const QStringList& row)
    {
        return row.size() == 1 && row.constFirst() == ruleMarker();
    }

    QStringList titles() const
    {
        QStringList t;
        for (const Column& c : m_columns)
            t << c.title;
        return t;
    }

    QString formatRow(const QStringList& cells, const QList<int>& widths) const
    {
        QString line = QStringLiteral("  ");
        for (int i = 0; i < m_columns.size(); ++i) {
            const Column& c = m_columns.at(i);
            const QString text = (i < cells.size()) ? cells.at(i) : QString();
            line += c.right ? text.rightJustified(widths.at(i), QLatin1Char(' '))
                            : text.leftJustified(widths.at(i), QLatin1Char(' '));
            if (i + 1 < m_columns.size())
                line += QStringLiteral("  ");
        }
        // Rechtsbündige letzte Spalte erzeugt keine Leerzeichen am Ende,
        // linksbündige schon — abschneiden, damit Editoren nichts markieren.
        while (line.endsWith(QLatin1Char(' ')))
            line.chop(1);
        return line + QLatin1Char('\n');
    }

    QList<Column>      m_columns;
    QList<QStringList> m_rows;
};

// ── Abgleich ──────────────────────────────────────────────────────────────────

// Vergleichslogik, Kurseffekt und Befunde liegen seit 27.10.2026 in
// CalculationBreakdown (check(), priceEffect(), shareChecks(), findings()),
// gemeinsam mit dem Tab "Depot- vs. Marktwert". Hier bleibt nur die
// Textdarstellung.

TextTable makeCompareTable()
{
    return TextTable({ { QStringLiteral("Wert"),      44, false },
                       { QStringLiteral("Tabelle"),   14, true  },
                       { QStringLiteral("Chart"),     14, true  },
                       { QStringLiteral("Differenz"), 12, true  },
                       { QStringLiteral("Ergebnis"),  18, false } });
}

/// Eine Vergleichszeile "Tabelle / Chart / Differenz / Ergebnis".
void addCheckRow(TextTable& table, const CalculationCheck& c)
{
    const auto fmt = [&c](double v) { return c.isVolume ? vol(v) : money(v); };
    table.addRow({ c.label, fmt(c.tableValue), fmt(c.chartValue), fmt(c.diff), c.resultText });
}

QString shareTitle(const CalculationReportShare& s) { return CalculationBreakdown::shareTitle(s); }

// ── Teil 1: je Aktie ──────────────────────────────────────────────────────────

QString buildShareSection(const CalculationReportShare& s)
{
    const ShareValues& v = s.values;
    const QString name   = shareTitle(s);
    QString out;

    out += QLatin1Char('\n') + rule(QLatin1Char('='));
    out += QStringLiteral("AKTIE: %1\n").arg(name);
    out += QStringLiteral("WKN %1   ISIN %2\n")
               .arg(s.share.wkn().isEmpty()  ? QStringLiteral("-") : s.share.wkn(),
                    s.share.isin().isEmpty() ? QStringLiteral("-") : s.share.isin());
    out += QStringLiteral("Kurs Tabelle %1 (Vortag %2)   Bestand Tabelle %3 Stk.%4\n")
               .arg(price(s.share.curPrice()), price(s.share.prevDayPrice()), vol(v.volume),
                    v.volume <= CalculationReport::kVolumeTolerance
                        ? QStringLiteral("   — vollständig verkauft") : QString());
    out += rule(QLatin1Char('='));

    // ── Splits ────────────────────────────────────────────────────────────
    if (!s.splits.isEmpty()) {
        out += heading(QStringLiteral("Splits"));
        TextTable t({ { QStringLiteral("Ex-Tag"),     10, false },
                      { QStringLiteral("Verhältnis"), 14, true  },
                      { QStringLiteral("Faktor"),     10, true  } });
        for (const ShareSplitObject& split : s.splits)
            t.addRow({ dateText(split.date()),
                       QStringLiteral("%1 : %2").arg(vol(split.ratioNew()), vol(split.ratioOld())),
                       vol(split.factor()) });
        out += t.render();
        out += QStringLiteral("  Käufe und Verkäufe stehen unten in Beleg-Skala; gerechnet wird "
                              "in heutiger Skala (Spalten \"heute\").\n");
    }

    // ── Käufe ─────────────────────────────────────────────────────────────
    const QList<double> fifoSold = CalculationReport::replayFifoSold(s.buys, s.sales, s.splits);

    out += heading(QStringLiteral("Käufe"));
    if (s.buys.isEmpty()) {
        out += QStringLiteral("  keine\n");
    } else {
        // Kosten je Kauf (Nessies Wunsch 23.10.2026): der dem Kauf
        // zugeordnete Kosteneintrag, Gebühren abzüglich Rabatt. Zuordnung
        // über BrokerageObject::buyGuid() oder BuyObject::brokerageGuid().
        const auto costsOf = [&s](const BuyObject& b) {
            double sum = 0.0;
            for (const BrokerageObject& k : s.brokerages) {
                if (k.buyGuid() == b.guid()
                    || (!b.brokerageGuid().isEmpty() && k.guid() == b.brokerageGuid()))
                    sum += k.brokerageReduction();
            }
            return sum;
        };

        // Spalten in heutiger Skala nur, wenn es überhaupt Splits gibt —
        // sonst wären sie identisch mit Stück und Kurs.
        const bool withSplits = !s.splits.isEmpty();

        QList<TextTable::Column> columns = {
            { QStringLiteral("Datum"),             10, false },
            { QStringLiteral("Depot"),              8, false },
            { QStringLiteral("Stück"),             14, true  },
            { QStringLiteral("Kurs"),              13, true  },
            { QStringLiteral("Kaufwert o. Kosten"),18, true  },
            { QStringLiteral("Kosten"),            11, true  },
            { QStringLiteral("Kaufwert m. Kosten"),18, true  },
            { QStringLiteral("verk. gespeich."),   15, true  },
            { QStringLiteral("verk. FIFO"),        14, true  } };
        if (withSplits) {
            columns.append({ QStringLiteral("Stück heute"), 14, true });
            columns.append({ QStringLiteral("Kurs heute"),  13, true });
        }
        TextTable t(columns);

        const auto stk = [](double v) { return vol(v) + QStringLiteral(" Stk."); };
        const auto eur = [](double v) { return euro(v); };
        const auto px  = [](double v) { return price(v) + QStringLiteral(" €"); };

        double sumVolume = 0.0, sumVolumeToday = 0.0, sumValue = 0.0, sumCosts = 0.0;
        double sumSoldStored = 0.0, sumSoldFifo = 0.0;
        for (int i = 0; i < s.buys.size(); ++i) {
            const BuyObject& b = s.buys.at(i);
            const double value  = ShareCalculator::roundAway(b.volume() * b.price());
            const double costs  = costsOf(b);
            const double adjVol = ShareSplitAdjuster::adjustedVolume(b.volume(), s.splits, b.date());

            QStringList row = { dateText(b.date()), b.depotNumber(), stk(b.volume()), px(b.price()),
                                eur(value), eur(costs), eur(value + costs),
                                stk(b.volumeSold()), stk(fifoSold.value(i)) };
            if (withSplits) {
                row << stk(adjVol)
                    << px(ShareSplitAdjuster::adjustedTransactionPrice(b.price(), s.splits, b.date()));
            }
            t.addRow(row);

            sumVolume      += b.volume();
            sumVolumeToday += adjVol;
            sumValue       += value;
            sumCosts       += costs;
            sumSoldStored  += ShareSplitAdjuster::adjustedVolume(b.volumeSold(), s.splits, b.date());
            sumSoldFifo    += ShareSplitAdjuster::adjustedVolume(fifoSold.value(i), s.splits, b.date());
        }

        // Summenzeile. Mit Splits liegen die Stückzahlen der Käufe in
        // verschiedenen Beleg-Skalen und lassen sich nicht addieren — dann
        // steht die Summe in der Spalte "Stück heute", die Spalte "Stück"
        // bleibt leer. Die verkauften Mengen werden für die Summe immer auf
        // die heutige Skala umgerechnet.
        t.addRule();
        QStringList sumRow = { QStringLiteral("Summe"), QString(),
                               withSplits ? QString() : stk(sumVolume), QString(),
                               eur(sumValue), eur(sumCosts), eur(sumValue + sumCosts),
                               stk(sumSoldStored), stk(sumSoldFifo) };
        if (withSplits)
            sumRow << stk(sumVolumeToday) << QString();
        t.addRow(sumRow);

        out += t.render();
        out += QStringLiteral("  \"Kosten\" = dem Kauf zugeordnete Gebühren abzüglich Rabatt.\n"
                              "  \"verk. gespeich.\" = BuyObject::volumeSold() aus der Datenbank, daraus "
                              "rechnet die Tabelle den Bestand.\n"
                              "  \"verk. FIFO\" = alle Verkäufe neu nach FIFO zugeteilt (je Depot, wie "
                              "die Verkaufsmaske).\n");
        if (withSplits)
            out += QStringLiteral("  Summen der Stückzahlen in heutiger Skala.\n");
    }

    // ── Verkäufe ──────────────────────────────────────────────────────────
    out += heading(QStringLiteral("Verkäufe"));
    if (s.sales.isEmpty()) {
        out += QStringLiteral("  keine\n");
    } else {
        TextTable t({ { QStringLiteral("Datum"),        10, false },
                      { QStringLiteral("Depot"),        10, false },
                      { QStringLiteral("Stück"),        14, true  },
                      { QStringLiteral("Kurs"),         13, true  },
                      { QStringLiteral("Verkaufswert"), 14, true  },
                      { QStringLiteral("Gebühren"),     11, true  },
                      { QStringLiteral("Rabatt"),       10, true  },
                      { QStringLiteral("Steuern"),      11, true  },
                      { QStringLiteral("Auszahlung"),   14, true  } });

        const auto stk = [](double v) { return vol(v) + QStringLiteral(" Stk."); };
        const auto px  = [](double v) { return price(v) + QStringLiteral(" €"); };

        double sumVolumeToday = 0.0, sumValue = 0.0, sumFees = 0.0, sumReduction = 0.0;
        double sumTax = 0.0, sumPayout = 0.0;
        for (const SaleObject& sale : s.sales) {
            const double saleValue = ShareCalculator::roundAway(sale.volume() * sale.salePrice());
            const double payout    = saleValue - sale.brokerage() + sale.reduction() - sale.taxSum();
            t.addRow({ dateText(sale.date()), sale.depotNumber(), stk(sale.volume()),
                       px(sale.salePrice()), euro(saleValue), euro(sale.brokerage()),
                       euro(sale.reduction()), euro(sale.taxSum()), euro(payout) });

            sumVolumeToday += ShareSplitAdjuster::adjustedVolume(sale.volume(), s.splits, sale.date());
            sumValue       += saleValue;
            sumFees        += sale.brokerage();
            sumReduction   += sale.reduction();
            sumTax         += sale.taxSum();
            sumPayout      += payout;
        }

        // Summenzeile (Nessies Wunsch 23.10.2026), wie bei den Käufen. Die
        // Stückzahl wird auf die heutige Skala umgerechnet — ohne Splits ist
        // das dieselbe Summe.
        t.addRule();
        t.addRow({ QStringLiteral("Summe"), QString(), stk(sumVolumeToday), QString(),
                   euro(sumValue), euro(sumFees), euro(sumReduction), euro(sumTax),
                   euro(sumPayout) });
        out += t.render();
        if (!s.splits.isEmpty())
            out += QStringLiteral("  Summe der Stückzahlen in heutiger Skala.\n");
    }

    // ── Dividenden ────────────────────────────────────────────────────────
    out += heading(QStringLiteral("Dividenden"));
    if (s.dividends.isEmpty()) {
        out += QStringLiteral("  keine\n");
    } else {
        // Steuern aufgeschlüsselt, Einheiten und Summenzeile wie bei Käufen
        // und Verkäufen (Nessies Wunsch 23.10.2026).
        TextTable t({ { QStringLiteral("Zahltag"),        10, false },
                      { QStringLiteral("Stück"),          14, true  },
                      { QStringLiteral("je Stück"),       13, true  },
                      { QStringLiteral("Brutto"),         13, true  },
                      { QStringLiteral("Quellensteuer"),  13, true  },
                      { QStringLiteral("Kapitalertragst."),16, true },
                      { QStringLiteral("Soli"),           10, true  },
                      { QStringLiteral("Steuern ges."),   13, true  },
                      { QStringLiteral("Netto"),          13, true  } });

        double sumGross = 0.0, sumSource = 0.0, sumCapital = 0.0, sumSoli = 0.0;
        double sumTax = 0.0, sumNet = 0.0;
        for (const DividendObject& d : s.dividends) {
            t.addRow({ dateText(d.date()), vol(d.volume()) + QStringLiteral(" Stk."),
                       // Satz je Stück in der Währung des Belegs — bei
                       // Fremdwährung z. B. USD, nicht Euro (Bugfix 24.10.2026).
                       price(d.rate()) + QLatin1Char(' ')
                           + (d.enableForeignCurrency() && !d.currency().isEmpty()
                                  ? d.currency() : QStringLiteral("€")),
                       euro(d.dividendPayout()),
                       euro(d.taxAtSource()), euro(d.capitalGainsTax()), euro(d.solidarityTax()),
                       euro(d.taxSum()), euro(d.dividendPayoutWithTaxes()) });
            sumGross   += d.dividendPayout();
            sumSource  += d.taxAtSource();
            sumCapital += d.capitalGainsTax();
            sumSoli    += d.solidarityTax();
            sumTax     += d.taxSum();
            sumNet     += d.dividendPayoutWithTaxes();
        }

        // Stückzahl und Satz je Stück lassen sich über mehrere Zahltage
        // nicht sinnvoll addieren — deshalb in der Summe leer.
        t.addRule();
        t.addRow({ QStringLiteral("Summe"), QString(), QString(), euro(sumGross), euro(sumSource),
                   euro(sumCapital), euro(sumSoli), euro(sumTax), euro(sumNet) });
        out += t.render();
        out += QStringLiteral("  Brutto bei Fremdwährung in Euro umgerechnet; Netto = Brutto − "
                              "Steuern ges.\n");
    }

    // ── Kosten ────────────────────────────────────────────────────────────
    out += heading(QStringLiteral("Kosten"));
    if (s.brokerages.isEmpty()) {
        out += QStringLiteral("  keine\n");
    } else {
        // Einheiten und Summenzeile wie in den übrigen Tabellen (23.10.2026).
        TextTable t({ { QStringLiteral("Datum"),      10, false },
                      { QStringLiteral("Zuordnung"),  12, false },
                      { QStringLiteral("Provision"),  12, true  },
                      { QStringLiteral("Makler"),     11, true  },
                      { QStringLiteral("Börse"),      11, true  },
                      { QStringLiteral("Brutto"),     12, true  },
                      { QStringLiteral("Rabatt"),     11, true  },
                      { QStringLiteral("Netto"),      12, true  } });
        double sumProvision = 0.0, sumBroker = 0.0, sumTrader = 0.0, sumGross = 0.0;
        double sumReduction = 0.0, sumNet = 0.0;
        for (const BrokerageObject& b : s.brokerages) {
            const QString assignment = !b.buyGuid().isEmpty()  ? QStringLiteral("Kauf")
                                     : !b.saleGuid().isEmpty() ? QStringLiteral("Verkauf")
                                                               : QStringLiteral("freistehend");
            t.addRow({ dateText(b.date()), assignment, euro(b.provision()), euro(b.brokerFee()),
                       euro(b.traderFee()), euro(b.brokerage()), euro(b.reduction()),
                       euro(b.brokerageReduction()) });
            sumProvision += b.provision();
            sumBroker    += b.brokerFee();
            sumTrader    += b.traderFee();
            sumGross     += b.brokerage();
            sumReduction += b.reduction();
            sumNet       += b.brokerageReduction();
        }
        t.addRule();
        t.addRow({ QStringLiteral("Summe"), QString(), euro(sumProvision), euro(sumBroker),
                   euro(sumTrader), euro(sumGross), euro(sumReduction), euro(sumNet) });
        out += t.render();
        out += QStringLiteral("  Brutto = Provision + Makler + Börse; Netto = Brutto − Rabatt.\n");
    }

    // ── Bestand ───────────────────────────────────────────────────────────
    double boughtToday = 0.0, soldToday = 0.0, heldFifoToday = 0.0;
    for (int i = 0; i < s.buys.size(); ++i) {
        const BuyObject& b = s.buys.at(i);
        boughtToday   += ShareSplitAdjuster::adjustedVolume(b.volume(), s.splits, b.date());
        heldFifoToday += ShareSplitAdjuster::adjustedVolume(b.volume() - fifoSold.value(i),
                                                            s.splits, b.date());
    }
    for (const SaleObject& sale : s.sales)
        soldToday += ShareSplitAdjuster::adjustedVolume(sale.volume(), s.splits, sale.date());

    out += heading(QStringLiteral("Bestand (heutige Skala)"));
    out += valueLine(QStringLiteral("Gekauft gesamt"), vol(boughtToday));
    out += valueLine(QStringLiteral("Verkauft gesamt"), vol(soldToday));
    out += valueLine(QStringLiteral("Bestand rechnerisch"), vol(boughtToday - soldToday),
                     QStringLiteral("gekauft − verkauft"));
    out += valueLine(QStringLiteral("Bestand nach FIFO-Nachrechnung"), vol(heldFifoToday));
    out += valueLine(QStringLiteral("Bestand Tabelle"), vol(v.volume),
                     QStringLiteral("Σ (Stück − verk. gespeichert) je Kauf"));

    // ── Tabellen "Kompletter Marktwert" und "Kompletter Depotwert" ────────
    // Nebeneinander (Nessies Wunsch 23.10.2026), damit die Unterschiede
    // zwischen beiden Sichten auf einen Blick sichtbar sind. Die Rechenwege
    // stehen nummeriert unter der Tabelle, sonst würde sie zu breit.
    out += heading(QStringLiteral("Tabellen \"Kompletter Marktwert\" und \"Kompletter Depotwert\""));
    {
        // Zeilen seit 27.10.2026 aus CalculationBreakdown::shareRows(),
        // gemeinsam mit dem Tab "Depot- vs. Marktwert".
        const QList<CalculationBreakdownRow> rows = CalculationBreakdown::shareRows(v);

        // Rechenweg rechts neben der jeweiligen Zeile statt als Liste unter
        // der Tabelle (Nessies Wunsch 23.10.2026). Unterscheiden sich die
        // Rechenwege beider Sichten, folgt der Depotwert-Rechenweg in einer
        // Fortsetzungszeile direkt darunter — so bleibt die Tabelle schmal
        // genug für einen normalen Editor.
        TextTable t({ { QStringLiteral("Nr"),            3, true  },
                      { QStringLiteral("Wert"),         38, false },
                      { QStringLiteral("Marktwert"),    14, true  },
                      { QStringLiteral("Depotwert"),    14, true  },
                      { QStringLiteral("Depot − Markt"),14, true  },
                      { QStringLiteral("Rechenweg"),     0, false } });
        for (int i = 0; i < rows.size(); ++i) {
            const CalculationBreakdownRow& r = rows.at(i);
            const auto fmt = [&r](double value) {
                return r.percent ? ValueFormatter::formatPercent(value) + QStringLiteral(" %")
                                 : euro(value);
            };
            const QString diff = (r.hasMarket && r.hasDepot) ? fmt(r.depot - r.market)
                                                             : QStringLiteral("–");
            const bool    same = (r.marketWay == r.depotWay);
            t.addRow({ QString::number(i + 1), r.label,
                       r.hasMarket ? fmt(r.market) : QStringLiteral("–"),
                       r.hasDepot  ? fmt(r.depot)  : QStringLiteral("–"), diff,
                       same ? QStringLiteral("beide:     ") + r.marketWay
                            : QStringLiteral("Marktwert: ") + r.marketWay });
            if (!same) {
                t.addRow({ QString(), QString(), QString(), QString(), QString(),
                           QStringLiteral("Depotwert: ") + r.depotWay });
            }
            // Trennlinie nach jedem Punkt, damit Zeile und Fortsetzungszeile
            // klar zusammengehören (Nessies Wunsch 23.10.2026).
            t.addRule();
        }
        out += t.render();
    }

    // ── Charts ────────────────────────────────────────────────────────────
    out += heading(QStringLiteral("Charts (letzter Stichtag dieser Aktie)"));
    if (s.chartExcluded) {
        // Der Befund dazu kommt aus CalculationBreakdown::shareFindings().
        out += QStringLiteral("  Keine Tageswerte — die Aktie fehlt in beiden Charts.\n");
        return out;
    }
    if (!s.chart.date.isValid()) {
        out += QStringLiteral("  Kein Chartpunkt.\n");
        return out;
    }

    const PortfolioSharePoint& c = s.chart;
    const double chartDev = CalculationBreakdown::chartDevelopment(c);
    out += valueLine(QStringLiteral("Stichtag"), dateText(c.date));
    out += valueLine(QStringLiteral("Bestand (FIFO aus Verkäufen, ohne Depot)"), vol(c.volume));
    out += valueLine(QStringLiteral("Schlusskurs (letzter Tageswert)"), price(c.price));
    out += valueLine(QStringLiteral("Bestandswert"), euro(c.holdingsValue),
                     QStringLiteral("%1 × %2").arg(vol(c.volume), price(c.price)));
    out += valueLine(QStringLiteral("Kaufwert gehalten"), euro(c.purchaseValueHeld));
    out += valueLine(QStringLiteral("Realisierter Gewinn (nach Steuer)"), euro(c.realizedGain));
    out += valueLine(QStringLiteral("Dividenden netto"), euro(c.dividends));
    out += valueLine(QStringLiteral("Kosten"), euro(c.costs));
    out += valueLine(QStringLiteral("Depotwert-Chart: Entwicklung"), euro(chartDev),
                     QStringLiteral("Bestandswert + realisiert + Dividenden − Kosten − Kaufwert gehalten"));
    out += valueLine(QStringLiteral("Marktwert-Chart: Marktwert"), euro(c.marketValue));

    // ── Abgleich ──────────────────────────────────────────────────────────
    out += heading(QStringLiteral("Abgleich Tabelle gegen Chart"));
    TextTable t = makeCompareTable();
    for (const CalculationCheck& check : CalculationBreakdown::shareChecks(s))
        addCheckRow(t, check);
    const double byPrice = CalculationBreakdown::priceEffect(s);
    out += t.render();
    out += valueLine(QStringLiteral("Davon durch Kursunterschied"), euro(byPrice),
                     QStringLiteral("%1 × (%2 − %3)")
                         .arg(vol(c.volume), price(v.curPrice), price(c.price)));

    return out;
}

// ── Teil 2: Gesamtübersicht ───────────────────────────────────────────────────

QString buildTotals(const CalculationReportInput& input)
{
    QString out;
    out += QLatin1Char('\n') + rule(QLatin1Char('#'));
    out += QStringLiteral("TEIL 2 — GESAMTÜBERSICHT\n");
    out += rule(QLatin1Char('#'));

    // ── Je Aktie, Marktwert-Sicht ─────────────────────────────────────────
    out += heading(QStringLiteral("Marktwert-Sicht je Aktie"));
    {
        TextTable t({ { QStringLiteral("Aktie"),           28, false },
                      { QStringLiteral("Bestand"),         12, true  },
                      { QStringLiteral("Einzahlung"),      13, true  },
                      { QStringLiteral("Marktwert"),       13, true  },
                      { QStringLiteral("Verkäufe"),        13, true  },
                      { QStringLiteral("Kpl. Marktwert"),  14, true  },
                      { QStringLiteral("Marktwert-Chart"), 15, true  } });
        double sPurchase = 0, sCur = 0, sSales = 0, sComplete = 0, sChart = 0;
        for (const CalculationReportShare& s : input.shares) {
            const ShareValues& v = s.values;
            const double chart = s.chartExcluded ? 0.0 : s.chart.marketValue;
            t.addRow({ shareTitle(s).left(28), vol(v.volume), money(v.purchaseValue),
                       money(v.curValue), money(v.salePayoutMarket),
                       money(v.completeCurValueMarket),
                       s.chartExcluded ? QStringLiteral("–") : money(chart) });
            sPurchase += v.purchaseValue; sCur += v.curValue; sSales += v.salePayoutMarket;
            sComplete += v.completeCurValueMarket; sChart += chart;
        }
        t.addRow({ QStringLiteral("Summe"), QString(), money(sPurchase), money(sCur),
                   money(sSales), money(sComplete), money(sChart) });
        out += t.render();
        out += QStringLiteral("  Verkäufe brutto (Stück × Verkaufskurs), Kpl. Marktwert = "
                              "Marktwert + Verkäufe.\n");
    }

    // ── Je Aktie, Depotwert-Sicht ─────────────────────────────────────────
    out += heading(QStringLiteral("Depotwert-Sicht je Aktie"));
    {
        TextTable t({ { QStringLiteral("Aktie"),          28, false },
                      { QStringLiteral("Einzahlung"),     13, true  },
                      { QStringLiteral("Kurswert"),       13, true  },
                      { QStringLiteral("Kpl. Einzahl."),  13, true  },
                      { QStringLiteral("Kpl. Wert"),      13, true  },
                      { QStringLiteral("Kpl. Entw."),     12, true  },
                      { QStringLiteral("Depot-Chart"),    12, true  } });
        double sPurchase = 0, sCur = 0, sCPurchase = 0, sCValue = 0, sCProfit = 0, sChart = 0;
        for (const CalculationReportShare& s : input.shares) {
            const ShareValues& v = s.values;
            const double chart = (s.chartExcluded || !s.chart.date.isValid())
                                     ? 0.0 : CalculationBreakdown::chartDevelopment(s.chart);
            t.addRow({ shareTitle(s).left(28), money(v.purchaseValueFinal), money(v.curValue),
                       money(v.completePurchase), money(v.completeCurValue),
                       money(v.completeProfitLoss),
                       s.chartExcluded ? QStringLiteral("–") : money(chart) });
            sPurchase += v.purchaseValueFinal; sCur += v.curValue; sCPurchase += v.completePurchase;
            sCValue += v.completeCurValue; sCProfit += v.completeProfitLoss; sChart += chart;
        }
        t.addRow({ QStringLiteral("Summe"), money(sPurchase), money(sCur), money(sCPurchase),
                   money(sCValue), money(sCProfit), money(sChart) });
        out += t.render();
    }

    // ── Footer "Kompletter Marktwert" ─────────────────────────────────────
    // Exakt die Aufrufe aus MainWindow::updatePortfolioFooters(), seit
    // 27.10.2026 gebündelt in CalculationBreakdown::footerTotals().
    const CalculationFooterTotals ft = CalculationBreakdown::footerTotals(input.shares);
    const double mPurchase  = ft.marketPurchase,         mCurValue   = ft.marketCurValue;
    const double mProfit    = ft.marketProfit,           mProfitPct  = ft.marketProfitPct;
    const double mcPurchase = ft.marketCompletePurchase, mcCurValue  = ft.marketCompleteValue;
    const double mcProfit   = ft.marketCompleteProfit,   mcProfitPct = ft.marketCompleteProfitPct;

    out += heading(QStringLiteral("Footer \"Kompletter Marktwert\" (wie angezeigt)"));
    out += valueLine(QStringLiteral("Einzahlung (gesamt)"), euro(mPurchase), QStringLiteral("Σ Einzahlung"));
    out += valueLine(QStringLiteral("Entwicklung (gesamt)"), euro(mProfit),
                     QStringLiteral("Σ Aktuelle Entwicklung (%1 %)").arg(ValueFormatter::formatPercent(mProfitPct)));
    out += valueLine(QStringLiteral("Aktueller Depotstand"), euro(mCurValue), QStringLiteral("Σ Kurswert"));
    out += valueLine(QStringLiteral("Kpl. Einzahlung"), euro(mcPurchase), QStringLiteral("Σ Kpl. Einzahlung"));
    out += valueLine(QStringLiteral("Komplette Entwicklung"), euro(mcProfit),
                     QStringLiteral("Kpl. Marktwert − Kpl. Einzahlung (%1 %)").arg(ValueFormatter::formatPercent(mcProfitPct)));
    out += valueLine(QStringLiteral("Kpl. Marktwert"), euro(mcCurValue), QStringLiteral("Σ Kpl. Marktwert"));

    // ── Footer "Kompletter Depotwert" ─────────────────────────────────────
    const double tPurchase  = ft.depotPurchase,          tCurValue   = ft.depotCurValue;
    const double tProfit    = ft.depotProfit,            tProfitPct  = ft.depotProfitPct;
    const double cPurchase  = ft.depotCompletePurchase,  cCurValue   = ft.depotCompleteValue;
    const double cProfit    = ft.depotCompleteProfit,    cProfitPct  = ft.depotCompleteProfitPct;
    const double tBrokerage = ft.depotBrokerage,         tDividend   = ft.depotDividend;

    out += heading(QStringLiteral("Footer \"Kompletter Depotwert\" (wie angezeigt)"));
    out += valueLine(QStringLiteral("Einzahlung (gesamt)"), euro(tPurchase), QStringLiteral("Σ Einzahlung"));
    out += valueLine(QStringLiteral("Kosten (ges.)"), euro(tBrokerage));
    out += valueLine(QStringLiteral("Dividenden (ges.)"), euro(tDividend));
    out += valueLine(QStringLiteral("Entwicklung (gesamt)"), euro(tProfit),
                     QStringLiteral("Aktueller Depotstand − Einzahlung (%1 %)").arg(ValueFormatter::formatPercent(tProfitPct)));
    out += valueLine(QStringLiteral("Aktueller Depotstand"), euro(tCurValue), QStringLiteral("Σ Kurswert"));
    out += valueLine(QStringLiteral("Kpl. Einzahlung"), euro(cPurchase), QStringLiteral("Σ Kpl. Einzahlung"));
    out += valueLine(QStringLiteral("Komplette Entwicklung"), euro(cProfit),
                     QStringLiteral("Kpl. Wert − Kpl. Einzahlung (%1 %)").arg(ValueFormatter::formatPercent(cProfitPct)));
    out += valueLine(QStringLiteral("Kpl. Wert"), euro(cCurValue), QStringLiteral("Σ Kpl. Wert"));

    // ── Charts gesamt ─────────────────────────────────────────────────────
    out += heading(QStringLiteral("Charts gesamt (letzter Stichtag)"));
    const PortfolioSeriesPoint& ct = input.chartTotal;
    if (!ct.date.isValid()) {
        out += QStringLiteral("  Kein Chartpunkt — keine Aktie mit Tageswerten.\n");
        return out;
    }
    out += valueLine(QStringLiteral("Stichtag"), dateText(ct.date));
    out += valueLine(QStringLiteral("Bestandswert"), euro(ct.holdingsValue));
    out += valueLine(QStringLiteral("Kaufwert gehalten"), euro(ct.purchaseValueHeld));
    out += valueLine(QStringLiteral("Kaufwert gesamt"), euro(ct.purchaseValueTotal));
    out += valueLine(QStringLiteral("Eingesetztes Kapital"), euro(ct.investedTotal),
                     QStringLiteral("Kaufwert gesamt + Kauf- und freistehende Kosten (Nenner %)"));
    out += valueLine(QStringLiteral("Realisierter Gewinn (nach Steuer)"), euro(ct.realizedGain));
    out += valueLine(QStringLiteral("Dividenden netto"), euro(ct.dividends));
    out += valueLine(QStringLiteral("Kosten"), euro(ct.costs));
    out += valueLine(QStringLiteral("Depotwert-Chart: Entwicklung"), euro(ct.development),
                     QStringLiteral("%1 %").arg(ValueFormatter::formatPercent(ct.developmentPct)));
    out += valueLine(QStringLiteral("Marktwert-Chart: Marktwert"), euro(ct.marketValue));

    // ── Abgleich Footer gegen Chart ───────────────────────────────────────
    // Aktien ohne Tageswerte zählen im Footer, fehlen aber im Chart (Nessies
    // Nebenbefund 26.10.2026) — ihr Footer-Beitrag gilt wie der
    // Kursunterschied als Erklärung, siehe CalculationBreakdown::totalChecks().
    const CalculationTotalChecks totals = CalculationBreakdown::totalChecks(input, ft);
    const double totalByPrice     = totals.byPrice;
    const double excludedCurValue = totals.excludedCurValue;
    const double excludedProfit   = totals.excludedProfit;

    out += heading(QStringLiteral("Abgleich Footer gegen Chart"));
    TextTable t = makeCompareTable();
    for (const CalculationCheck& check : totals.checks)
        addCheckRow(t, check);
    out += t.render();
    out += valueLine(QStringLiteral("Davon durch Kursunterschied"), euro(totalByPrice),
                     QStringLiteral("Σ je Aktie, siehe Teil 1"));
    out += valueLine(QStringLiteral("Aktien ohne Tageswerte: Kurswert"), euro(excludedCurValue),
                     QStringLiteral("zählen in der Tabelle, fehlen im Chart"));
    out += valueLine(QStringLiteral("Aktien ohne Tageswerte: Kompl. Entw."), euro(excludedProfit),
                     QStringLiteral("zählen in der Tabelle, fehlen im Chart"));

    return out;
}

} // namespace

// ── replayFifoSold ────────────────────────────────────────────────────────────

QList<double> CalculationReport::replayFifoSold(const QList<BuyObject>& buys,
                                                const QList<SaleObject>& sales,
                                                const QList<ShareSplitObject>& splits)
{
    // Arbeitskopien mit leerem "verkauft" — die Nachrechnung beginnt bei null,
    // unabhängig davon, was in der Datenbank steht.
    QList<BuyObject> work = buys;
    for (BuyObject& b : work)
        b.setVolumeSold(0.0);

    QList<int> buyOrder(work.size());
    std::iota(buyOrder.begin(), buyOrder.end(), 0);
    std::stable_sort(buyOrder.begin(), buyOrder.end(), [&work](int a, int b) {
        return work.at(a).date() < work.at(b).date();
    });

    QList<SaleObject> sortedSales = sales;
    std::stable_sort(sortedSales.begin(), sortedSales.end(),
                     [](const SaleObject& a, const SaleObject& b) { return a.date() < b.date(); });

    for (const SaleObject& sale : sortedSales) {
        const QString depot = sale.depotNumber().trimmed();

        QList<BuyObject> available;
        QList<int>       availableIndex;
        for (int idx : buyOrder) {
            const BuyObject& b = work.at(idx);
            if (b.date() > sale.date())
                continue;
            if (!depot.isEmpty() && b.depotNumber().trimmed() != depot)
                continue;
            if (b.volume() - b.volumeSold() <= 0.0)
                continue;
            available.append(b);
            availableIndex.append(idx);
        }

        const QList<FifoAllocationRow> rows =
            SaleFifoAllocator::allocate(sale.volume(), sale.date(), available, splits);

        for (const FifoAllocationRow& row : rows) {
            for (int i = 0; i < available.size(); ++i) {
                if (available.at(i).guid() == row.buyGuid) {
                    BuyObject& target = work[availableIndex.at(i)];
                    target.setVolumeSold(target.volumeSold() + row.volume);
                    break;
                }
            }
        }
    }

    QList<double> result;
    result.reserve(work.size());
    for (const BuyObject& b : work)
        result.append(b.volumeSold());
    return result;
}

// ── collect ───────────────────────────────────────────────────────────────────

CalculationReportInput CalculationReport::collect(const QDateTime& created,
                                                  const QString& portfolioPath)
{
    CalculationReportInput input;
    input.created       = created;
    input.portfolioPath = portfolioPath;

    ShareRepository      shareRepo;
    BuyRepository        buyRepo;
    SaleRepository       saleRepo;
    DividendRepository   dividendRepo;
    BrokerageRepository  brokerageRepo;
    ShareSplitRepository splitRepo;

    // Chart-Eingangsdaten genau wie die Charts selbst (split-bereinigt).
    ModelPortfolioChart chartModel;
    const QList<PortfolioShareSeriesInput> chartInput = chartModel.loadPortfolioInput();

    for (const ShareObject& share : shareRepo.findAll()) {
        CalculationReportShare s;
        s.share      = share;
        s.buys       = buyRepo.findByShare(share.guid());
        s.sales      = saleRepo.findByShare(share.guid());
        s.dividends  = dividendRepo.findByShare(share.guid());
        s.brokerages = brokerageRepo.findByShare(share.guid());
        s.splits     = splitRepo.findByShare(share.guid());
        s.values     = ShareCalculator::compute(share.guid(), share.curPrice(), share.prevDayPrice());

        const auto byDate = [](const auto& a, const auto& b) { return a.date() < b.date(); };
        std::stable_sort(s.buys.begin(),       s.buys.end(),       byDate);
        std::stable_sort(s.sales.begin(),      s.sales.end(),      byDate);
        std::stable_sort(s.dividends.begin(),  s.dividends.end(),  byDate);
        std::stable_sort(s.brokerages.begin(), s.brokerages.end(), byDate);

        // Chartwerte nur dieser Aktie über die gesamte Historie: der letzte
        // Punkt entspricht ihrem Beitrag zum rechten Rand beider Charts, da
        // alle Werte danach unverändert fortgeschrieben werden.
        for (const PortfolioShareSeriesInput& in : chartInput) {
            if (in.shareGuid != share.guid())
                continue;
            const PortfolioSeriesResult r =
                PortfolioSeriesCalculator::compute({ in }, QDate(), QDate(), /*withPerShareDetail*/ true);
            s.chartExcluded = !r.sharesWithoutHistory.isEmpty();
            if (!r.sharePoints.isEmpty())
                s.chart = r.sharePoints.constLast();
            break;
        }

        input.shares.append(s);
    }

    const PortfolioSeriesResult total = PortfolioSeriesCalculator::compute(chartInput);
    if (!total.points.isEmpty())
        input.chartTotal = total.points.constLast();

    return input;
}

// ── anonymize ─────────────────────────────────────────────────────────────────

CalculationReportInput CalculationReport::anonymize(const CalculationReportInput& input)
{
    CalculationReportInput out = input;
    out.anonymized    = true;
    out.portfolioPath = QStringLiteral("(anonymisiert)");

    QHash<QString, QString> depots;
    const auto depot = [&depots](const QString& number) {
        const QString key = number.trimmed();
        if (key.isEmpty())
            return QString();
        auto it = depots.constFind(key);
        if (it == depots.constEnd())
            it = depots.insert(key, QStringLiteral("Depot %1").arg(depots.size() + 1));
        return it.value();
    };

    for (int i = 0; i < out.shares.size(); ++i) {
        CalculationReportShare& s = out.shares[i];
        const QString n = QString::number(i + 1);

        s.share.setName(QStringLiteral("Aktie ") + n);
        if (!s.share.wkn().isEmpty())
            s.share.setWkn(QStringLiteral("WKN-") + n);
        if (!s.share.isin().isEmpty())
            s.share.setIsin(QStringLiteral("ISIN-") + n);
        s.chart.name = s.share.name();

        // BuyObject/SaleObject haben keine Setter für die Depotnummer — neu
        // aufbauen, alle übrigen Werte unverändert übernehmen.
        for (BuyObject& b : s.buys) {
            b = BuyObject(b.guid(), b.shareGuid(), depot(b.depotNumber()), QString(),
                          b.dateTime(), b.volume(), b.volumeSold(), b.price(),
                          b.brokerageGuid(), QString());
        }
        for (SaleObject& sale : s.sales) {
            sale = SaleObject(sale.guid(), sale.shareGuid(), depot(sale.depotNumber()), QString(),
                              sale.dateTime(), sale.volume(), sale.salePrice(),
                              sale.saleBuyDetails(), sale.taxAtSource(), sale.capitalGainsTax(),
                              sale.solidarityTax(), sale.brokerageGuid(), sale.provision(),
                              sale.brokerFee(), sale.traderFee(), sale.reduction(), QString());
        }
    }
    return out;
}

// ── anonymizedFilePath ────────────────────────────────────────────────────────

QString CalculationReport::anonymizedFilePath(const QString& path)
{
    const QFileInfo info(path);
    const QString suffix = info.suffix().isEmpty() ? QStringLiteral("txt") : info.suffix();
    const QString name   = info.completeBaseName() + QStringLiteral("-anonymisiert.") + suffix;
    return info.dir().filePath(name);
}

// ── writeToFile ───────────────────────────────────────────────────────────────

bool CalculationReport::writeToFile(const QString& path, const QString& text,
                                    QString* errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream.setGenerateByteOrderMark(true);
    stream << text;
    stream.flush();

    if (stream.status() != QTextStream::Ok) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }
    return true;
}

// ── build ─────────────────────────────────────────────────────────────────────

QString CalculationReport::build(const CalculationReportInput& input)
{
    QString out;

    out += rule(QLatin1Char('#'));
    out += QStringLiteral("SharePortfolioManager — Berechnungsbericht\n");
    out += QStringLiteral("Erstellt:  %1\n").arg(input.created.toString(QStringLiteral("dd.MM.yyyy HH:mm:ss")));
    out += QStringLiteral("Portfolio: %1\n").arg(input.portfolioPath.isEmpty() ? QStringLiteral("-")
                                                                                : input.portfolioPath);
    out += QStringLiteral("Aktien:    %1\n").arg(input.shares.size());
    if (input.anonymized) {
        out += QStringLiteral(
            "ANONYMISIERT: Aktiennamen, WKN, ISIN, Depotnummern und Portfolio-Pfad sind ersetzt.\n"
            "Beträge, Kurse, Stückzahlen und Daten sind unverändert — über Kurs und Datum lässt sich\n"
            "eine bekannte Aktie trotzdem zurückverfolgen.\n");
    }
    out += rule(QLatin1Char('#'));
    out += QStringLiteral(
        "Inhalt: Teil 1 je Aktie (auch vollständig verkaufte), Teil 2 Gesamtübersicht mit Footer und\n"
        "Charts, Teil 3 Auffälligkeiten. Alle Beträge in Euro, Stückzahlen und Kurse mit vier\n"
        "Nachkommastellen. Tabellen rechnen mit dem aktuellen Kurs der Aktie, die Charts mit dem\n"
        "letzten Tageswert über die gesamte Historie (unabhängig vom eingestellten Zeitraum).\n");

    out += QLatin1Char('\n') + rule(QLatin1Char('#'));
    out += QStringLiteral("TEIL 1 — JE AKTIE\n");
    out += rule(QLatin1Char('#'));
    if (input.shares.isEmpty())
        out += QStringLiteral("  Keine Aktien im Portfolio.\n");
    for (const CalculationReportShare& s : input.shares)
        out += buildShareSection(s);

    out += buildTotals(input);

    // Teil 3 aus derselben Quelle wie der Tab "Depot- vs. Marktwert"
    // (27.10.2026): erst je Aktie, dann Footer gegen Chart.
    const CalculationFindings findings = CalculationBreakdown::findings(input);

    out += QLatin1Char('\n') + rule(QLatin1Char('#'));
    out += QStringLiteral("TEIL 3 — AUFFÄLLIGKEITEN\n");
    out += rule(QLatin1Char('#'));
    out += heading(QStringLiteral("Abweichungen"));
    if (findings.deviations.isEmpty()) {
        out += QStringLiteral("  Keine — Tabellen, Footer und Charts stimmen überein.\n");
    } else {
        for (const QString& line : findings.deviations)
            out += QStringLiteral("  • ") + line + QLatin1Char('\n');
    }

    out += heading(QStringLiteral("Hinweise"));
    if (findings.hints.isEmpty()) {
        out += QStringLiteral("  Keine.\n");
    } else {
        for (const QString& line : findings.hints)
            out += QStringLiteral("  • ") + line + QLatin1Char('\n');
    }

    return out;
}

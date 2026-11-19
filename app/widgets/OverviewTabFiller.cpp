// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "OverviewTabFiller.h"

#include "OverviewTabWidget.h"
#include "../IconProvider.h"
#include "../utils/ShareSplitAdjuster.h"
#include "../utils/ShareSplitHint.h"
#include "../utils/ValueFormatter.h"

#include <QDate>
#include <QFileInfo>
#include <QLabel>
#include <QMap>
#include <QTableWidget>
#include <QTableWidgetItem>

#include <algorithm>
#include <functional>
#include <utility>

namespace {

QString money(double v)  { return ValueFormatter::formatMoney(v) + QStringLiteral(" €"); }
QString volume(double v) { return ValueFormatter::formatVolume(v) + QStringLiteral(" stk."); }
// Dividende je Anteil liegt auf der Kurs-Skala (05.09.2026).
QString rate(double v)   { return ValueFormatter::formatPrice(v) + QStringLiteral(" €"); }

/// Jahre der Einträge, absteigend (neuestes Jahr zuerst, Projektkonvention).
template <typename T>
QList<int> yearsOf(const QList<T>& entries)
{
    QList<int> years;
    for (const T& e : entries) {
        if (!years.contains(e.year()))
            years.append(e.year());
    }
    std::sort(years.begin(), years.end(), std::greater<int>());
    return years;
}

/// Summe je Jahr und daraus die Gesamtsumme über die Jahre.
template <typename T>
QMap<int, double> sumPerYear(const QList<T>& entries, const std::function<double(const T&)>& value)
{
    QMap<int, double> perYear;
    for (const T& e : entries)
        perYear[e.year()] += value(e);
    return perYear;
}

double total(const QList<int>& years, const QMap<int, double>& perYear)
{
    double sum = 0.0;
    for (int y : years)
        sum += perYear.value(y);
    return sum;
}

} // namespace

// ── Bausteine ─────────────────────────────────────────────────────────────────

QTableWidgetItem* OverviewTabFiller::centeredItem(const QString& text)
{
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(Qt::AlignCenter);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

QWidget* OverviewTabFiller::documentIconWidget(const QString& documentPath)
{
    const QString ext = QFileInfo(documentPath).suffix().toLower();
    IconProvider::IconName iconName;
    if (ext == QStringLiteral("pdf"))
        iconName = IconProvider::DocPdfImage16;
    else if (ext == QStringLiteral("doc") || ext == QStringLiteral("docx"))
        iconName = IconProvider::DocWordImage16;
    else if (ext == QStringLiteral("xls") || ext == QStringLiteral("xlsx"))
        iconName = IconProvider::DocExcelImage16;
    else
        iconName = IconProvider::SearchFailed2;

    auto* label = new QLabel;
    label->setAlignment(Qt::AlignCenter);
    label->setPixmap(IconProvider::icon(iconName).pixmap(16, 16));
    label->setToolTip(documentPath);
    return label;
}

void OverviewTabFiller::setDocumentCell(QTableWidget* table, int row, int column,
                                        const QString& documentPath)
{
    if (documentPath.isEmpty()) {
        table->setItem(row, column, centeredItem(QStringLiteral("-")));
        return;
    }
    auto* item = new QTableWidgetItem;
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    item->setData(Qt::UserRole, documentPath);
    table->setItem(row, column, item);
    table->setCellWidget(row, column, documentIconWidget(documentPath));
}

// ── Verkäufe ──────────────────────────────────────────────────────────────────

void OverviewTabFiller::fillSales(OverviewTabWidget*             tabs,
                                  const QList<SaleObject>&       sales,
                                  const QList<ShareSplitObject>& splits,
                                  SaleAmounts                    amounts)
{
    if (!tabs)
        return;

    const bool market = (amounts == SaleAmounts::Market);
    const std::function<double(const SaleObject&)> payoutOf = [market](const SaleObject& s) {
        return market ? s.saleValue() : s.payoutBrokerageReduction();
    };
    const std::function<double(const SaleObject&)> profitLossOf = [market](const SaleObject& s) {
        return market ? s.saleValue() - s.buyValue() : s.profitLossBrokerageReduction();
    };
    // Stückzahlen je Beleg auf heutige Skala, erst danach summieren — niemals
    // die Summe skalieren (Phase 3c, 10./11.08.2026). Geldbeträge bleiben
    // unberührt: über einen Split ist Stückzahl × Preis invariant.
    const std::function<double(const SaleObject&)> volumeTodayOf = [&splits](const SaleObject& s) {
        return ShareSplitAdjuster::adjustedVolume(s.volume(), splits, s.date());
    };

    const QList<int> years = yearsOf(sales);
    const QMap<int, double> yearVolToday = sumPerYear(sales, volumeTodayOf);
    const QMap<int, double> yearPayout   = sumPerYear(sales, payoutOf);
    const QMap<int, double> yearGV       = sumPerYear(sales, profitLossOf);
    const double totVolToday = total(years, yearVolToday);
    const double totPayout   = total(years, yearPayout);
    const double totGV       = total(years, yearGV);

    // Je Jahr das ÄLTESTE Verkaufsdatum: liegt danach ein Split, war
    // mindestens ein summierter Beleg umzurechnen — genau dann tragen Zelle
    // und Fußzeile den Marker. Ohne Verkäufe gibt es keinen Marker.
    QMap<int, QDate> yearEarliest;
    QDate            earliestOverall;
    for (const SaleObject& s : sales) {
        const QDate d = s.date();
        if (!d.isValid())
            continue;
        if (!yearEarliest.contains(s.year()) || d < yearEarliest.value(s.year()))
            yearEarliest[s.year()] = d;
        if (!earliestOverall.isValid() || d < earliestOverall)
            earliestOverall = d;
    }
    QMap<int, bool>    yearVolAffected;
    QMap<int, QString> yearVolTooltip;
    for (int y : years) {
        yearVolAffected[y] = ShareSplitHint::hasSplitAfter(splits, yearEarliest.value(y));
        yearVolTooltip[y]  = ShareSplitHint::overviewAggregateTooltip(splits, yearEarliest.value(y));
    }
    const bool    totalVolAffected = !sales.isEmpty()
                                     && ShareSplitHint::hasSplitAfter(splits, earliestOverall);
    const QString totalVolTooltip  = sales.isEmpty()
                                     ? QString()
                                     : ShareSplitHint::overviewAggregateTooltip(splits, earliestOverall);

    auto populateUebersichtData = [&](QTableWidget* tbl) {
        tbl->setRowCount(years.size());
        for (int i = 0; i < years.size(); ++i) {
            const int y = years.at(i);
            auto* iYear = centeredItem(QString::number(y));
            iYear->setData(Qt::UserRole, y);
            auto* iVol = centeredItem(ShareSplitHint::withMarker(volume(yearVolToday.value(y)),
                                                                 yearVolAffected.value(y)));
            if (!yearVolTooltip.value(y).isEmpty())
                iVol->setToolTip(yearVolTooltip.value(y));
            tbl->setItem(i, 0, iYear);
            tbl->setItem(i, 1, iVol);
            tbl->setItem(i, 2, centeredItem(money(yearPayout.value(y))));
            tbl->setItem(i, 3, centeredItem(money(yearGV.value(y))));
        }
    };

    auto populateUebersichtFooter = [&](QTableWidget* f) {
        auto* iVol = centeredItem(ShareSplitHint::withMarker(volume(totVolToday), totalVolAffected));
        if (!totalVolTooltip.isEmpty())
            iVol->setToolTip(totalVolTooltip);
        f->setItem(0, 0, centeredItem(tr("Gesamt:")));
        f->setItem(0, 1, iVol);
        f->setItem(0, 2, centeredItem(money(totPayout)));
        f->setItem(0, 3, centeredItem(money(totGV)));
    };

    constexpr int kColDoc = 4;

    auto populateJahresData = [&](int year, QTableWidget* tbl) {
        QList<SaleObject> yearSales;
        for (const SaleObject& s : sales)
            if (s.year() == year)
                yearSales.append(s);

        tbl->setRowCount(yearSales.size());
        for (int i = 0; i < yearSales.size(); ++i) {
            const SaleObject& s = yearSales.at(i);

            auto* iDate = centeredItem(s.dateAsStr());
            iDate->setData(Qt::UserRole, s.guid());

            // Belegzeile bleibt in Beleg-Skala; Marker und Tooltip nennen
            // die heutige Entsprechung (Phase 3c).
            const bool    volAffected = ShareSplitHint::hasSplitAfter(splits, s.date());
            const QString volTooltip  = ShareSplitHint::overviewRowTooltip(
                splits, s.date(), s.volume(), s.salePrice());
            auto* iVol = centeredItem(ShareSplitHint::withMarker(volume(s.volume()), volAffected));
            if (!volTooltip.isEmpty())
                iVol->setToolTip(volTooltip);

            tbl->setItem(i, 0, iDate);
            tbl->setItem(i, 1, iVol);
            tbl->setItem(i, 2, centeredItem(money(payoutOf(s))));
            tbl->setItem(i, 3, centeredItem(money(profitLossOf(s))));
            setDocumentCell(tbl, i, kColDoc, s.document());
        }
    };

    auto populateJahresFooter = [&](int year, QTableWidget* f) {
        double volToday = 0.0, payout = 0.0, gv = 0.0;
        for (const SaleObject& s : sales) {
            if (s.year() != year)
                continue;
            volToday += volumeTodayOf(s);
            payout   += payoutOf(s);
            gv       += profitLossOf(s);
        }
        auto* iVol = centeredItem(ShareSplitHint::withMarker(volume(volToday),
                                                             yearVolAffected.value(year)));
        if (!yearVolTooltip.value(year).isEmpty())
            iVol->setToolTip(yearVolTooltip.value(year));
        f->setItem(0, 0, centeredItem(tr("Gesamt:")));
        f->setItem(0, 1, iVol);
        f->setItem(0, 2, centeredItem(money(payout)));
        f->setItem(0, 3, centeredItem(money(gv)));
        f->setItem(0, kColDoc, centeredItem(QStringLiteral("-")));
    };

    auto jahresTitleForYear = [&](int year) {
        return tr("%1 (%2)").arg(year).arg(money(yearPayout.value(year)));
    };

    tabs->populateOverview(
        years,
        tr("Übersicht (%1)").arg(money(totPayout)),
        { tr("Jahr"), tr("Anteile"), tr("Auszahlung"), tr("Gewinn / Verlust") },
        { 100, -1, -1, -1 },
        populateUebersichtData,
        populateUebersichtFooter,
        { tr("Datum"), tr("Anteile"), tr("Auszahlung"), tr("Gewinn / Verlust"), QString() },
        { 100, -1, -1, -1, kDocColumnWidth },
        jahresTitleForYear,
        populateJahresData,
        populateJahresFooter,
        kColDoc,
        tr("Keine Verkäufe vorhanden."));
}

// ── Dividenden ────────────────────────────────────────────────────────────────

void OverviewTabFiller::fillDividends(OverviewTabWidget*             tabs,
                                      const QList<DividendObject>&   dividends,
                                      const QList<ShareSplitObject>& splits)
{
    if (!tabs)
        return;

    const std::function<double(const DividendObject&)> netOf = [](const DividendObject& d) {
        return d.dividendPayoutWithTaxes();
    };

    const QList<int>        years    = yearsOf(dividends);
    const QMap<int, double> yearVal  = sumPerYear(dividends, netOf);
    const double            totalVal = total(years, yearVal);

    auto populateUebersichtData = [&](QTableWidget* tbl) {
        tbl->setRowCount(years.size());
        for (int i = 0; i < years.size(); ++i) {
            const int y = years.at(i);
            auto* iYear = centeredItem(QString::number(y));
            iYear->setData(Qt::UserRole, y);
            tbl->setItem(i, 0, iYear);
            tbl->setItem(i, 1, centeredItem(money(yearVal.value(y))));
        }
    };

    auto populateUebersichtFooter = [&](QTableWidget* f) {
        f->setItem(0, 0, centeredItem(tr("Gesamt:")));
        f->setItem(0, 1, centeredItem(money(totalVal)));
    };

    constexpr int kColDoc = 4;

    auto populateJahresData = [&](int year, QTableWidget* tbl) {
        QList<DividendObject> yearDivs;
        for (const DividendObject& d : dividends)
            if (d.year() == year)
                yearDivs.append(d);

        tbl->setRowCount(yearDivs.size());
        for (int i = 0; i < yearDivs.size(); ++i) {
            const DividendObject& d = yearDivs.at(i);

            auto* iDate = centeredItem(d.dateAsStr());
            iDate->setData(Qt::UserRole, d.guid());

            // Belegzeile in Beleg-Skala: "Anteile am Auszahlungstag" ist die
            // Stückzahl, auf die die Bank ausgeschüttet hat. Maßstab für den
            // Marker ist der EX-TAG, nicht der Zahltag (Phase 4 der
            // Ex-Tag-Behandlung, 21.08.2026); volumeReferenceDate() fällt für
            // Alt-Dividenden ohne Ex-Tag auf date() zurück.
            const QDate   volRefDate  = d.volumeReferenceDate();
            const bool    volAffected = ShareSplitHint::hasSplitAfter(splits, volRefDate);
            const QString volTooltip  = ShareSplitHint::overviewRowTooltip(
                splits, volRefDate, d.volume(), d.rate());
            auto* iVol = centeredItem(ShareSplitHint::withMarker(volume(d.volume()), volAffected));
            if (!volTooltip.isEmpty())
                iVol->setToolTip(volTooltip);

            tbl->setItem(i, 0, iDate);
            tbl->setItem(i, 1, centeredItem(rate(d.rate())));
            tbl->setItem(i, 2, iVol);
            tbl->setItem(i, 3, centeredItem(money(d.dividendPayoutWithTaxes())));
            setDocumentCell(tbl, i, kColDoc, d.document());
        }
    };

    auto populateJahresFooter = [&](int year, QTableWidget* f) {
        f->setItem(0, 0, centeredItem(tr("Gesamt:")));
        // Dividendensatz und Anteile bewusst "-" (Nessies Entscheidung
        // 11.08.2026, Phase 3c): "Anteile am Auszahlungstag" bezieht sich auf
        // je einen Stichtag, eine Summe beschreibt keinen Bestand, den es je
        // gab — auch nicht in heutiger Skala.
        f->setItem(0, 1, centeredItem(QStringLiteral("-")));
        auto* iVol = centeredItem(QStringLiteral("-"));
        iVol->setToolTip(tr("Anteile beziehen sich auf verschiedene "
                            "Auszahlungstage und lassen sich nicht summieren."));
        f->setItem(0, 2, iVol);
        f->setItem(0, 3, centeredItem(money(yearVal.value(year))));
        f->setItem(0, kColDoc, centeredItem(QStringLiteral("-")));
    };

    auto jahresTitleForYear = [&](int year) {
        return tr("%1 (%2)").arg(year).arg(money(yearVal.value(year)));
    };

    tabs->populateOverview(
        years,
        tr("Übersicht (%1)").arg(money(totalVal)),
        { tr("Jahr"), tr("Dividende") },
        { 100, -1 },
        populateUebersichtData,
        populateUebersichtFooter,
        { tr("Datum"), tr("Dividendensatz"), tr("Anteile"), tr("Dividende"), QString() },
        { 100, -1, -1, -1, kDocColumnWidth },
        jahresTitleForYear,
        populateJahresData,
        populateJahresFooter,
        kColDoc,
        tr("Keine Dividenden vorhanden."));
}

// ── Kosten ────────────────────────────────────────────────────────────────────

void OverviewTabFiller::fillBrokerages(OverviewTabWidget*            tabs,
                                       const QList<BrokerageObject>& brokerages)
{
    if (!tabs)
        return;

    const std::function<double(const BrokerageObject&)> netOf = [](const BrokerageObject& b) {
        return b.brokerageReduction();
    };

    const QList<int>        years      = yearsOf(brokerages);
    const QMap<int, double> yearNetto  = sumPerYear(brokerages, netOf);
    const double            totalNetto = total(years, yearNetto);

    auto populateUebersichtData = [&](QTableWidget* tbl) {
        tbl->setRowCount(years.size());
        for (int i = 0; i < years.size(); ++i) {
            const int y = years.at(i);
            auto* iYear = centeredItem(QString::number(y));
            iYear->setData(Qt::UserRole, y);
            tbl->setItem(i, 0, iYear);
            tbl->setItem(i, 1, centeredItem(money(yearNetto.value(y))));
        }
    };

    auto populateUebersichtFooter = [&](QTableWidget* f) {
        f->setItem(0, 0, centeredItem(tr("Gesamt:")));
        f->setItem(0, 1, centeredItem(money(totalNetto)));
    };

    constexpr int kColDoc = 5;

    auto populateJahresData = [&](int year, QTableWidget* tbl) {
        QList<BrokerageObject> yearBrokerages;
        for (const BrokerageObject& b : brokerages)
            if (b.year() == year)
                yearBrokerages.append(b);

        tbl->setRowCount(yearBrokerages.size());
        for (int i = 0; i < yearBrokerages.size(); ++i) {
            const BrokerageObject& b = yearBrokerages.at(i);

            const QString typ = !b.buyGuid().isEmpty()  ? tr("Kauf")
                              : !b.saleGuid().isEmpty() ? tr("Verkauf")
                                                        : tr("Sonstig");

            auto* iDate = centeredItem(b.dateAsStr());
            iDate->setData(Qt::UserRole, b.guid());

            tbl->setItem(i, 0, iDate);
            tbl->setItem(i, 1, centeredItem(typ));
            tbl->setItem(i, 2, centeredItem(money(b.brokerage())));
            tbl->setItem(i, 3, centeredItem(money(b.reduction())));
            tbl->setItem(i, 4, centeredItem(money(b.brokerageReduction())));
            setDocumentCell(tbl, i, kColDoc, b.document());
        }
    };

    auto populateJahresFooter = [&](int year, QTableWidget* f) {
        double sumGeb = 0.0, sumRabatt = 0.0, sumNetto = 0.0;
        for (const BrokerageObject& b : brokerages) {
            if (b.year() != year)
                continue;
            sumGeb    += b.brokerage();
            sumRabatt += b.reduction();
            sumNetto  += b.brokerageReduction();
        }
        f->setItem(0, 0, centeredItem(tr("Gesamt:")));
        f->setItem(0, 1, centeredItem(QStringLiteral("-")));
        f->setItem(0, 2, centeredItem(money(sumGeb)));
        f->setItem(0, 3, centeredItem(money(sumRabatt)));
        f->setItem(0, 4, centeredItem(money(sumNetto)));
        f->setItem(0, kColDoc, centeredItem(QStringLiteral("-")));
    };

    auto jahresTitleForYear = [&](int year) {
        return tr("%1 (%2)").arg(year).arg(money(yearNetto.value(year)));
    };

    tabs->populateOverview(
        years,
        tr("Übersicht (%1)").arg(money(totalNetto)),
        { tr("Jahr"), tr("Netto-Kosten") },
        { 100, -1 },
        populateUebersichtData,
        populateUebersichtFooter,
        { tr("Datum"), tr("Typ"), tr("Ges. Gebühren"), tr("Rabatt"), tr("Netto-Kosten"), QString() },
        { 100, -1, -1, -1, -1, kDocColumnWidth },
        jahresTitleForYear,
        populateJahresData,
        populateJahresFooter,
        kColDoc,
        tr("Keine Kosten vorhanden."));
}

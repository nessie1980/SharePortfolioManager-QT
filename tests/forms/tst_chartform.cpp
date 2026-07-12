// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// Testet ausschliesslich PresenterChart ueber ein Fake-View/Fake-Model-Paar,
// analog zu tst_sharedetailsform.cpp - keine echte Datenbank, kein QWidget,
// keine QtCharts-Instanziierung. FakeModelChart gibt ChartReferenceInfo fuer
// "Letzter Kauf"/"Letzter Verkauf" direkt zurueck, ohne echte BuyObject/
// SaleObject-Instanzen zu benoetigen.

#include <QtTest>

#include "../../app/forms/ChartForm/PresenterChart.h"

namespace {
const QString kShareGuid = QStringLiteral("share-1");
}

// ── Fake View ──────────────────────────────────────────────────────────────

class FakeViewChart : public IViewChart
{
public:
    // Control values read by the presenter
    QDate m_startDate;
    IntervalUnit m_intervalUnit = IntervalUnit::Month;
    int m_intervalCount = 1;
    QMap<SeriesKind, bool> m_selected{
        { SeriesKind::ClosingPrice, true },
        { SeriesKind::OpeningPrice, false },
        { SeriesKind::High,         false },
        { SeriesKind::Low,          false },
        { SeriesKind::HeldVolume,   false },
        { SeriesKind::TradedVolume, false },
    };

    // Captured setter calls
    QDate               lastDefaultStartDate;
    bool                defaultStartDateSet = false;
    QList<ChartSeriesData> lastChartData;
    bool                chartDataSet = false;
    QString             lastEmptyMessage;
    bool                emptyShown = false;
    LegendEntries        lastLegendEntries;
    QList<ChartReferenceLine> lastReferenceLines;
    bool                 referenceLinesSet = false;
    QString             lastRangeInfo;
    QString             lastError;

    QDate        startDate()      const override { return m_startDate; }
    IntervalUnit intervalUnit()   const override { return m_intervalUnit; }
    int          intervalCount()  const override { return m_intervalCount; }
    bool         isSeriesSelected(SeriesKind kind) const override
    {
        return m_selected.value(kind, false);
    }

    void setDefaultStartDate(const QDate& date) override
    {
        lastDefaultStartDate = date;
        defaultStartDateSet  = true;
        m_startDate          = date; // mirrors ViewChart::setDefaultStartDate()
    }

    void setChartData(const QList<ChartSeriesData>& series) override
    {
        lastChartData = series;
        chartDataSet  = true;
        emptyShown    = false;
    }

    void showEmptyChart(const QString& message) override
    {
        lastEmptyMessage = message;
        emptyShown       = true;
        chartDataSet     = false;
    }

    void setLegendEntries(const LegendEntries& entries) override
    {
        lastLegendEntries = entries;
    }

    void setReferenceLines(const QList<ChartReferenceLine>& lines) override
    {
        lastReferenceLines = lines;
        referenceLinesSet  = true;
    }

    void setRangeInfo(const QString& infoText) override
    {
        lastRangeInfo = infoText;
    }

    void showError(const QString& message) override
    {
        lastError = message;
    }

    /** Finds a chart series by kind — fails via QVERIFY if absent. */
    const ChartSeriesData* findSeries(SeriesKind kind) const
    {
        for (const auto& s : lastChartData)
            if (s.kind == kind)
                return &s;
        return nullptr;
    }

    /** Finds a legend entry by (partial) title match. */
    const LegendEntry* findLegendEntry(const QString& titleContains) const
    {
        for (const auto& e : lastLegendEntries)
            if (e.title.contains(titleContains))
                return &e;
        return nullptr;
    }
};

// ── Fake Model ─────────────────────────────────────────────────────────────

class FakeModelChart : public IModelChart
{
public:
    QList<DailyValuesObject> m_dailyValues;
    QDate                    m_latestDate;
    QMap<QDate, double>      m_heldVolume;
    ChartReferenceInfo       m_latestBuy;
    ChartReferenceInfo       m_latestSale;
    QList<ChartReferenceInfo> m_buysInRange;
    QList<ChartReferenceInfo> m_salesInRange;

    QList<DailyValuesObject> loadDailyValues(const QString& /*shareGuid*/,
                                              const QDate& from,
                                              const QDate& to) const override
    {
        QList<DailyValuesObject> result;
        for (const auto& dv : m_dailyValues)
            if (dv.date() >= from && dv.date() <= to)
                result.append(dv);
        return result;
    }

    QDate latestDailyValueDate(const QString& /*shareGuid*/) const override
    {
        return m_latestDate;
    }

    QMap<QDate, double> heldVolumeSeries(const QString& /*shareGuid*/,
                                         const QList<QDate>& dates) const override
    {
        QMap<QDate, double> result;
        for (const QDate& d : dates)
            result.insert(d, m_heldVolume.value(d, 0.0));
        return result;
    }

    ChartReferenceInfo latestBuy(const QString& /*shareGuid*/) const override { return m_latestBuy; }
    ChartReferenceInfo latestSale(const QString& /*shareGuid*/) const override { return m_latestSale; }

    QList<ChartReferenceInfo> buysInRange(const QString& /*shareGuid*/,
                                          const QDate& from, const QDate& to) const override
    {
        QList<ChartReferenceInfo> result;
        for (const auto& info : m_buysInRange)
            if (info.date >= from && info.date <= to)
                result.append(info);
        return result;
    }

    QList<ChartReferenceInfo> salesInRange(const QString& /*shareGuid*/,
                                           const QDate& from, const QDate& to) const override
    {
        QList<ChartReferenceInfo> result;
        for (const auto& info : m_salesInRange)
            if (info.date >= from && info.date <= to)
                result.append(info);
        return result;
    }
};

// ── Test class ─────────────────────────────────────────────────────────────

class TestChartForm : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase()
    {
        // PresenterChart formats all numbers via the default QLocale (same
        // convention as PresenterShareDetails). This test binary doesn't link
        // AppStartup.cpp (which sets this for the real app), so it must be
        // set explicitly here — otherwise the exact-string assertions below
        // (e.g. "379,70") would depend on the test machine's system locale.
        QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));
    }

    void test_loadAndDisplay_noData_showsEmptyAndClearsRangeInfo()
    {
        FakeViewChart view;
        FakeModelChart model; // m_latestDate stays invalid -> no data

        PresenterChart presenter(&view, &model, kShareGuid);
        presenter.loadAndDisplay();

        QVERIFY(view.emptyShown);
        QVERIFY(!view.defaultStartDateSet);
        QCOMPARE(view.lastRangeInfo, QString());
        QVERIFY(view.lastLegendEntries.isEmpty());
    }

    void test_loadAndDisplay_withData_setsDefaultStartDateToLatest()
    {
        FakeViewChart view;
        FakeModelChart model;
        model.m_latestDate = QDate(2026, 7, 10);
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 7, 10), 100.0, 105.0, 106.0, 99.0, 1000.0));

        PresenterChart presenter(&view, &model, kShareGuid);
        presenter.loadAndDisplay();

        QVERIFY(view.defaultStartDateSet);
        QCOMPARE(view.lastDefaultStartDate, QDate(2026, 7, 10));
        QVERIFY(view.chartDataSet);
        QVERIFY(!view.emptyShown);
    }

    void test_refresh_defaultSelection_onlyClosingPriceSeries()
    {
        FakeViewChart view;
        FakeModelChart model;
        model.m_latestDate = QDate(2026, 7, 10);
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 6, 20), 100.0, 102.0, 103.0, 98.0, 500.0));
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 7, 10), 110.0, 115.0, 116.0, 109.0, 600.0));

        PresenterChart presenter(&view, &model, kShareGuid);
        presenter.loadAndDisplay(); // default view selection: only ClosingPrice

        QCOMPARE(view.lastChartData.size(), 1);
        const auto* closing = view.findSeries(SeriesKind::ClosingPrice);
        if (!closing) QFAIL("ClosingPrice series missing");
        QCOMPARE(closing->values.size(), 2);
        QCOMPARE(closing->values.at(0), 102.0);
        QCOMPARE(closing->values.at(1), 115.0);
        QCOMPARE(closing->axis, ChartAxis::Price);
    }

    void test_refresh_dayInterval_computesCorrectRangeStart()
    {
        FakeViewChart view;
        view.m_intervalUnit  = IntervalUnit::Day;
        view.m_intervalCount = 5;

        FakeModelChart model;
        model.m_latestDate = QDate(2026, 7, 10);
        // One value inside the 5-day window, one clearly outside it.
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 7, 6),  100.0, 101.0, 102.0, 99.0, 100.0));
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 6, 1),  90.0,  91.0,  92.0,  89.0, 100.0));

        PresenterChart presenter(&view, &model, kShareGuid);
        presenter.loadAndDisplay();

        const auto* closing = view.findSeries(SeriesKind::ClosingPrice);
        if (!closing) QFAIL("ClosingPrice series missing");
        // rangeStart = 10.07.2026 - 5 Tage = 05.07.2026 -> only the 06.07. value qualifies.
        QCOMPARE(closing->dates.size(), 1);
        QCOMPARE(closing->dates.at(0), QDate(2026, 7, 6));
    }

    void test_refresh_heldVolumeSeries_usesModelValuesAndOwnAxis()
    {
        FakeViewChart view;
        view.m_selected[SeriesKind::HeldVolume] = true;

        FakeModelChart model;
        model.m_latestDate = QDate(2026, 7, 10);
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 7, 10), 100.0, 105.0, 106.0, 99.0, 1000.0));
        model.m_heldVolume.insert(QDate(2026, 7, 10), 42.0);

        PresenterChart presenter(&view, &model, kShareGuid);
        presenter.loadAndDisplay();

        const auto* held = view.findSeries(SeriesKind::HeldVolume);
        if (!held) QFAIL("HeldVolume series missing");
        QCOMPARE(held->axis, ChartAxis::HeldVolume);
        QCOMPARE(held->values.size(), 1);
        QCOMPARE(held->values.at(0), 42.0);
    }

    void test_refresh_tradedVolumeSeries_usesDailyValuesVolumeAndOwnAxis()
    {
        // TradedVolume ("Gehandelte Anteile") kommt direkt aus DailyValuesObject::
        // volume() (Spalte daily_values.volume) — anders als HeldVolume braucht es
        // keinen eigenen Model-Aufruf, siehe ARCHITECTURE.md "ChartForm-Details".
        FakeViewChart view;
        view.m_selected[SeriesKind::TradedVolume] = true;

        FakeModelChart model;
        model.m_latestDate = QDate(2026, 7, 10);
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 7, 10), 100.0, 105.0, 106.0, 99.0, 1250000.0));

        PresenterChart presenter(&view, &model, kShareGuid);
        presenter.loadAndDisplay();

        const auto* traded = view.findSeries(SeriesKind::TradedVolume);
        if (!traded) QFAIL("TradedVolume series missing");
        QCOMPARE(traded->axis, ChartAxis::TradedVolume);
        QCOMPARE(traded->values.size(), 1);
        QCOMPARE(traded->values.at(0), 1250000.0);
    }

    void test_refresh_heldAndTradedVolume_useDifferentAxes()
    {
        // Dritte eigene Skala für Gehandelte Anteile (ergänzt 12.07.2026, auf
        // Nessies Vorgabe nach visueller Prüfung) — Anteile (Depotbestand) und
        // Gehandelte Anteile (Börsenvolumen) dürfen sich NICHT mehr eine Achse
        // teilen, da ihre Größenordnungen zu weit auseinanderliegen.
        FakeViewChart view;
        view.m_selected[SeriesKind::HeldVolume]   = true;
        view.m_selected[SeriesKind::TradedVolume] = true;

        FakeModelChart model;
        model.m_latestDate = QDate(2026, 7, 10);
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 7, 10), 100.0, 105.0, 106.0, 99.0, 500000.0));
        model.m_heldVolume.insert(QDate(2026, 7, 10), 42.0);

        PresenterChart presenter(&view, &model, kShareGuid);
        presenter.loadAndDisplay();

        const auto* held   = view.findSeries(SeriesKind::HeldVolume);
        const auto* traded = view.findSeries(SeriesKind::TradedVolume);
        if (!held)   QFAIL("HeldVolume series missing");
        if (!traded) QFAIL("TradedVolume series missing");
        QCOMPARE(held->axis, ChartAxis::HeldVolume);
        QCOMPARE(traded->axis, ChartAxis::TradedVolume);
        QVERIFY(held->axis != traded->axis);
    }

    void test_refresh_noSeriesSelected_showsEmptyMessage()
    {
        FakeViewChart view;
        view.m_selected[SeriesKind::ClosingPrice] = false; // deselect the only default series

        FakeModelChart model;
        model.m_latestDate = QDate(2026, 7, 10);
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 7, 10), 100.0, 105.0, 106.0, 99.0, 1000.0));

        PresenterChart presenter(&view, &model, kShareGuid);
        presenter.loadAndDisplay();

        QVERIFY(view.emptyShown);
        QVERIFY(!view.chartDataSet);
    }

    void test_refresh_legendEntries_minMaxForClosingPrice()
    {
        FakeViewChart view;
        FakeModelChart model;
        model.m_latestDate = QDate(2026, 7, 10);
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 6, 20), 100.0, 379.7, 103.0, 98.0, 500.0));
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 7, 10), 110.0, 422.4, 116.0, 109.0, 600.0));

        PresenterChart presenter(&view, &model, kShareGuid);
        presenter.loadAndDisplay();

        const auto* entry = view.findLegendEntry(QStringLiteral("Schluss-Kurs"));
        if (!entry) QFAIL("Schluss-Kurs Legende-Eintrag fehlt");
        QVERIFY(entry->line1.contains(QStringLiteral("379,70")));
        QVERIFY(entry->line1.contains(QStringLiteral("422,40")));
    }

    void test_refresh_legendEntries_lastBuyAndSaleReference()
    {
        FakeViewChart view;
        FakeModelChart model;
        model.m_latestDate = QDate(2026, 7, 10);
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 7, 10), 100.0, 422.4, 106.0, 99.0, 1000.0));
        model.m_latestBuy  = ChartReferenceInfo{ true, QDate(2022, 5, 12), 198.36 };
        model.m_latestSale = ChartReferenceInfo{ true, QDate(2020, 2, 27), 205.25 };

        PresenterChart presenter(&view, &model, kShareGuid);
        presenter.loadAndDisplay();

        const auto* buyEntry = view.findLegendEntry(QStringLiteral("Letzter Kauf"));
        if (!buyEntry) QFAIL("Letzter-Kauf Legende-Eintrag fehlt");
        QVERIFY(buyEntry->line1.contains(QStringLiteral("12.05.2022")));
        QVERIFY(buyEntry->line1.contains(QStringLiteral("198,36")));
        // Entwicklung: 422,40 - 198,36 = 224,04 (=~112,95%)
        QVERIFY(buyEntry->line2.contains(QStringLiteral("224,04")));

        const auto* saleEntry = view.findLegendEntry(QStringLiteral("Letzter Verkauf"));
        if (!saleEntry) QFAIL("Letzter-Verkauf Legende-Eintrag fehlt");
        QVERIFY(saleEntry->line1.contains(QStringLiteral("27.02.2020")));
    }

    void test_refresh_noReferenceEntries_whenModelReturnsInvalid()
    {
        FakeViewChart view;
        FakeModelChart model;
        model.m_latestDate = QDate(2026, 7, 10);
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 7, 10), 100.0, 105.0, 106.0, 99.0, 1000.0));
        // m_latestBuy/m_latestSale stay default-constructed (valid == false)

        PresenterChart presenter(&view, &model, kShareGuid);
        presenter.loadAndDisplay();

        QVERIFY(!view.findLegendEntry(QStringLiteral("Letzter Kauf")));
        QVERIFY(!view.findLegendEntry(QStringLiteral("Letzter Verkauf")));
    }

    void test_refresh_referenceLines_latestBuyIsBlueOlderIsTurquoise()
    {
        FakeViewChart view;
        FakeModelChart model;
        model.m_latestDate = QDate(2026, 7, 10);
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 7, 10), 100.0, 422.4, 106.0, 99.0, 1000.0));
        model.m_latestBuy = ChartReferenceInfo{ true, QDate(2026, 7, 5), 400.0 };
        // Zwei Käufe im Zeitraum: der spätere (05.07.) ist der global letzte
        // Kauf -> Blau, der frühere (10.06.) ist "älter" -> Türkis.
        model.m_buysInRange = {
            ChartReferenceInfo{ true, QDate(2026, 6, 10), 380.0, 20.0 },
            ChartReferenceInfo{ true, QDate(2026, 7, 5),  400.0, 30.0 },
        };

        PresenterChart presenter(&view, &model, kShareGuid);
        presenter.loadAndDisplay();

        QVERIFY(view.referenceLinesSet);
        QCOMPARE(view.lastReferenceLines.size(), 2);

        const ChartReferenceLine* latest = nullptr;
        const ChartReferenceLine* older  = nullptr;
        for (const auto& line : view.lastReferenceLines) {
            if (line.date == QDate(2026, 7, 5)) latest = &line;
            if (line.date == QDate(2026, 6, 10)) older = &line;
        }
        if (!latest) QFAIL("Referenzlinie für den letzten Kauf fehlt");
        if (!older)  QFAIL("Referenzlinie für den aelteren Kauf fehlt");
        QCOMPARE(latest->color, QColor(Qt::blue));
        QCOMPARE(older->color,  QColor(0, 170, 170));
        QVERIFY(latest->kind == ChartReferenceLineKind::Buy);
        QCOMPARE(latest->price,  400.0);
        QCOMPARE(latest->volume, 30.0);
        QCOMPARE(older->price,   380.0);
        QCOMPARE(older->volume,  20.0);
    }

    void test_refresh_referenceLines_latestSaleIsRedOlderIsOrange()
    {
        FakeViewChart view;
        FakeModelChart model;
        model.m_latestDate = QDate(2026, 7, 10);
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 7, 10), 100.0, 422.4, 106.0, 99.0, 1000.0));
        model.m_latestSale = ChartReferenceInfo{ true, QDate(2026, 7, 8), 410.0 };
        model.m_salesInRange = {
            ChartReferenceInfo{ true, QDate(2026, 6, 15), 395.0, 10.0 },
            ChartReferenceInfo{ true, QDate(2026, 7, 8),  410.0, 15.0 },
        };

        PresenterChart presenter(&view, &model, kShareGuid);
        presenter.loadAndDisplay();

        const ChartReferenceLine* latest = nullptr;
        const ChartReferenceLine* older  = nullptr;
        for (const auto& line : view.lastReferenceLines) {
            if (line.date == QDate(2026, 7, 8))  latest = &line;
            if (line.date == QDate(2026, 6, 15)) older = &line;
        }
        if (!latest) QFAIL("Referenzlinie fuer den letzten Verkauf fehlt");
        if (!older)  QFAIL("Referenzlinie fuer den aelteren Verkauf fehlt");
        QCOMPARE(latest->color, QColor(Qt::red));
        QCOMPARE(older->color,  QColor(255, 140, 0));
        QVERIFY(latest->kind == ChartReferenceLineKind::Sale);
        QCOMPARE(latest->price, 410.0);
    }

    void test_refresh_referenceLines_onlyDatesWithinComputedRange()
    {
        // Die Referenzlinien muessen exakt denselben berechneten Zeitraum
        // respektieren wie die Daten-Serien — der Presenter reicht rangeStart/
        // rangeEnd 1:1 an buysInRange()/salesInRange() durch, das Fake-Model
        // filtert hier selbst (siehe FakeModelChart), genau wie das echte
        // ModelChart per SQL/Iteration filtert.
        FakeViewChart view;
        view.m_intervalUnit  = IntervalUnit::Day;
        view.m_intervalCount = 5;

        FakeModelChart model;
        model.m_latestDate = QDate(2026, 7, 10);
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 7, 10), 100.0, 105.0, 106.0, 99.0, 100.0));
        // rangeStart = 05.07.2026 -> nur der zweite Kauf liegt im Fenster.
        model.m_buysInRange = {
            ChartReferenceInfo{ true, QDate(2026, 6, 1), 90.0, 5.0 },
            ChartReferenceInfo{ true, QDate(2026, 7, 6), 95.0, 8.0 },
        };

        PresenterChart presenter(&view, &model, kShareGuid);
        presenter.loadAndDisplay();

        QCOMPARE(view.lastReferenceLines.size(), 1);
        QCOMPARE(view.lastReferenceLines.constFirst().date, QDate(2026, 7, 6));
    }

    void test_loadAndDisplay_noData_clearsReferenceLines()
    {
        FakeViewChart view;
        FakeModelChart model; // m_latestDate stays invalid -> no data

        PresenterChart presenter(&view, &model, kShareGuid);
        presenter.loadAndDisplay();

        QVERIFY(view.referenceLinesSet);
        QVERIFY(view.lastReferenceLines.isEmpty());
    }

    void test_onControlsChanged_beforeAnyData_doesNotCrashOrRefresh()
    {
        FakeViewChart view;
        FakeModelChart model; // no data at all

        PresenterChart presenter(&view, &model, kShareGuid);
        // No loadAndDisplay() call yet — m_hasData stays false internally.
        presenter.onControlsChanged();

        QVERIFY(!view.chartDataSet);
        QVERIFY(!view.emptyShown);
    }

    void test_onControlsChanged_afterLoad_reflectsNewIntervalCount()
    {
        FakeViewChart view;
        FakeModelChart model;
        model.m_latestDate = QDate(2026, 7, 10);
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 7, 1),  100.0, 101.0, 102.0, 99.0, 100.0));
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 7, 10), 110.0, 111.0, 112.0, 109.0, 100.0));

        PresenterChart presenter(&view, &model, kShareGuid);
        presenter.loadAndDisplay();

        const auto* before = view.findSeries(SeriesKind::ClosingPrice);
        if (!before) QFAIL("ClosingPrice series missing (before)");
        QCOMPARE(before->dates.size(), 2); // Default: Monat/1 -> beide Werte im Fenster

        // Shrink the window to 1 day back -> only the 10.07. value should remain.
        view.m_intervalUnit  = IntervalUnit::Day;
        view.m_intervalCount = 1;
        presenter.onControlsChanged();

        const auto* after = view.findSeries(SeriesKind::ClosingPrice);
        if (!after) QFAIL("ClosingPrice series missing (after)");
        QCOMPARE(after->dates.size(), 1);
        QCOMPARE(after->dates.at(0), QDate(2026, 7, 10));
    }
};

QTEST_MAIN(TestChartForm)
#include "tst_chartform.moc"

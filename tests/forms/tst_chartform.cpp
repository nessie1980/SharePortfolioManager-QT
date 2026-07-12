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
    int                  lastMaxIntervalCount = -1;
    bool                 maxIntervalCountSet  = false;

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

    void setMaxIntervalCount(int maxCount) override
    {
        lastMaxIntervalCount = maxCount;
        maxIntervalCountSet  = true;
        // Bewusst KEIN automatisches Klemmen von m_intervalCount hier, anders
        // als das echte QSpinBox::setMaximum() — die Presenter-seitige
        // std::min()-Klammer in PresenterChart::refresh() muss die effektive
        // Anzahl unabhängig davon korrekt begrenzen, ob die View das UI-
        // seitig auch tut (siehe ARCHITECTURE.md, "ChartForm-Details").
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

    // Erfasst die zuletzt an loadDailyValues() übergebene [from, to]-Spanne —
    // ergänzt 12.07.2026, um in Tests zu prüfen, dass PresenterChart die
    // Anzahl-Kappung wirklich VOR der Datenabfrage anwendet, statt die vom
    // Nutzer angeforderte (ggf. zu große) Anzahl unverändert durchzureichen.
    mutable QDate lastQueryFrom;
    mutable QDate lastQueryTo;
    mutable bool  loadDailyValuesCalled = false;

    QList<DailyValuesObject> loadDailyValues(const QString& /*shareGuid*/,
                                              const QDate& from,
                                              const QDate& to) const override
    {
        lastQueryFrom         = from;
        lastQueryTo           = to;
        loadDailyValuesCalled = true;
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

    /**
     * @brief Ältester Termin über ALLE m_dailyValues hinweg (nicht auf ein
     * Abfrage-Fenster beschränkt) — spiegelt genau das Verhalten von
     * ModelChart::earliestDailyValueDate()/DailyValuesRepository::
     * earliestDate() gegen die volle Historie in der DB, nicht gegen einen
     * bereits gefilterten Ausschnitt.
     */
    QDate earliestDailyValueDate(const QString& /*shareGuid*/) const override
    {
        QDate earliest;
        for (const auto& dv : m_dailyValues) {
            if (!earliest.isValid() || dv.date() < earliest)
                earliest = dv.date();
        }
        return earliest;
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

    void test_refresh_heldVolumeSeries_usesModelValuesAndVolumeAxis()
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
        QCOMPARE(held->axis, ChartAxis::Volume);
        QCOMPARE(held->values.size(), 1);
        QCOMPARE(held->values.at(0), 42.0);
    }

    void test_refresh_tradedVolumeSeries_usesDailyValuesVolumeAndVolumeAxis()
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
        QCOMPARE(traded->axis, ChartAxis::Volume);
        QCOMPARE(traded->values.size(), 1);
        QCOMPARE(traded->values.at(0), 1250000.0);
    }

    void test_refresh_heldAndTradedVolume_shareSameVolumeAxis()
    {
        // Seit dem Umbau auf gegenseitigen Checkbox-Ausschluss (12.07.2026,
        // siehe ARCHITECTURE.md "ChartForm-Details") teilen sich HeldVolume
        // und TradedVolume wieder eine gemeinsame ChartAxis::Volume — die
        // Exklusivität selbst wird ausschließlich in
        // ViewChart::setupSelektionBox() durchgesetzt (View-Ebene, siehe
        // tst_mainwindow.cpp, test_chartCheckboxes_heldAndTradedVolumeAreMutuallyExclusive),
        // nicht hier auf Presenter-Ebene. Diese FakeView bildet die reale
        // Exklusivität bewusst nicht nach und erlaubt beide Flags
        // gleichzeitig — dieser Test prüft nur noch, dass PresenterChart
        // in diesem (in der echten UI unmöglichen) Fall beiden Serien
        // dieselbe Achse zuweisen würde.
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
        QCOMPARE(held->axis, ChartAxis::Volume);
        QCOMPARE(traded->axis, ChartAxis::Volume);
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
        // Zweiter, deutlich älterer Tageswert — ohne ihn wäre der einzige
        // vorhandene Wert exakt das Start-Datum selbst, wodurch die seit
        // 12.07.2026 bestehende Anzahl-Kappung (siehe computeMaxIntervalCount())
        // das angeforderte 5-Tage-Fenster auf 1 zurückstutzen würde, bevor der
        // eigentliche Testzweck (Referenzlinien-Filterung) zum Tragen kommt.
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 5, 1),  80.0,  82.0,  83.0,  79.0, 50.0));
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

    // ─────────────────────────────────────────────────────────────────────
    // Anzahl-Kappung (ergänzt 12.07.2026, auf Nessies Vorgabe): "Anzahl"
    // darf nicht über den Punkt hinaus wachsen, an dem der älteste
    // vorhandene Tageswert bereits im Fenster liegt.
    // ─────────────────────────────────────────────────────────────────────

    void test_refresh_setsMaxIntervalCount_basedOnEarliestDailyValue()
    {
        // Day-Intervall macht die Bezugsrechnung leicht nachvollziehbar: 9
        // Tage liegen zwischen dem ältesten (01.07.) und dem Start-Datum
        // (10.07., = Ende des Zeitraums).
        FakeViewChart view;
        view.m_intervalUnit  = IntervalUnit::Day;
        view.m_intervalCount = 3;

        FakeModelChart model;
        model.m_latestDate = QDate(2026, 7, 10);
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 7, 1),  100.0, 101.0, 102.0, 99.0, 100.0));
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 7, 10), 110.0, 111.0, 112.0, 109.0, 100.0));

        PresenterChart presenter(&view, &model, kShareGuid);
        presenter.loadAndDisplay();

        QVERIFY(view.maxIntervalCountSet);
        QCOMPARE(view.lastMaxIntervalCount, 9); // 10.07. minus 9 Tage = 01.07. (ältester Wert)
    }

    void test_refresh_intervalCountBeyondMax_clampsQueryToEarliestDate()
    {
        // Angeforderte Anzahl (50) liegt weit über dem, was an Historie
        // existiert (nur 9 Tage bis zum ältesten Wert). PresenterChart muss
        // die tatsächlich abgefragte Spanne auf den erreichbaren Maximalwert
        // kappen, statt eine rangeStart weit vor dem ältesten Wert zu
        // berechnen — geprüft über FakeModelChart::lastQueryFrom, nicht nur
        // über das Ergebnis (das bei zu wenig Historie ohnehin identisch
        // aussähe).
        FakeViewChart view;
        view.m_intervalUnit  = IntervalUnit::Day;
        view.m_intervalCount = 50;

        FakeModelChart model;
        model.m_latestDate = QDate(2026, 7, 10);
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 7, 1),  100.0, 101.0, 102.0, 99.0, 100.0));
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 7, 10), 110.0, 111.0, 112.0, 109.0, 100.0));

        PresenterChart presenter(&view, &model, kShareGuid);
        presenter.loadAndDisplay();

        QCOMPARE(view.lastMaxIntervalCount, 9);
        QVERIFY(model.loadDailyValuesCalled);
        // 10.07. minus 9 (gekappte) Tage = 01.07. — NICHT 10.07. minus 50
        // Tage (22.05.), wie es die unbegrenzte Anzahl ergäbe.
        QCOMPARE(model.lastQueryFrom, QDate(2026, 7, 1));
    }

    void test_refresh_singleValueAtRangeEnd_maxIntervalCountStaysAtOne()
    {
        // Nur ein einziger Tageswert existiert, exakt am Start-Datum (=Ende
        // des Zeitraums) selbst — es gibt nichts Älteres zu erreichen,
        // "Anzahl" darf über 1 hinaus gar nicht erst wachsen.
        FakeViewChart view;
        view.m_intervalUnit  = IntervalUnit::Month;
        view.m_intervalCount = 1;

        FakeModelChart model;
        model.m_latestDate = QDate(2026, 7, 10);
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 7, 10), 100.0, 105.0, 106.0, 99.0, 1000.0));

        PresenterChart presenter(&view, &model, kShareGuid);
        presenter.loadAndDisplay();

        QCOMPARE(view.lastMaxIntervalCount, 1);
    }

    void test_onControlsChanged_countAboveMax_clampsEffectiveQueryRange()
    {
        // Simuliert ein Mausrad-/Spinbox-Event, das die Anzahl über den
        // erreichbaren Maximalwert hinaus erhöht — in der echten View würde
        // QSpinBox::setMaximum() (siehe ViewChart::setMaxIntervalCount())
        // das bereits verhindern; hier wird die Presenter-Seite unabhängig
        // davon geprüft, da FakeViewChart absichtlich NICHT automatisch
        // klemmt (siehe FakeViewChart::setMaxIntervalCount()).
        FakeViewChart view;
        FakeModelChart model;
        model.m_latestDate = QDate(2026, 7, 10);
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 7, 1),  100.0, 101.0, 102.0, 99.0, 100.0));
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 7, 10), 110.0, 111.0, 112.0, 109.0, 100.0));

        PresenterChart presenter(&view, &model, kShareGuid);
        presenter.loadAndDisplay();

        view.m_intervalUnit  = IntervalUnit::Day;
        view.m_intervalCount = 50;
        presenter.onControlsChanged();

        QCOMPARE(view.lastMaxIntervalCount, 9);
        QCOMPARE(model.lastQueryFrom, QDate(2026, 7, 1));
    }

    void test_refresh_monthIntervalNotLandingOnEarliestDate_maxIntervalCountStillReachesIt()
    {
        // Regressionstest für den Off-by-one-Bugfix vom 12.07.2026 (Nessies
        // Rückmeldung: "nicht alle Werte werden angezeigt, wenn ich den
        // Zeitraum größer mache"). Anders als bei Interval=Tag (siehe
        // test_refresh_setsMaxIntervalCount_basedOnEarliestDailyValue, wo
        // jede Stufe exakt einen Tag verschiebt und die alte Berechnung
        // dadurch zufällig richtig lag) landet Interval=Monat hier bewusst
        // NICHT exakt auf dem Datum des ältesten Werts: Start-Datum ist der
        // 10., der älteste Wert liegt am 15. eines Monats fünfeinhalb Monate
        // zuvor. Mit der alten, um eins verschobenen Schleife blieb
        // maxIntervalCount bei 5 hängen (Fenster ab 10.02.2026 — der Wert
        // vom 15.01.2026 wäre nie erreichbar gewesen); korrekt ist 6
        // (Fenster ab 10.01.2026, schließt den 15.01. mit ein).
        FakeViewChart view;
        view.m_intervalUnit  = IntervalUnit::Month;
        view.m_intervalCount = 6;

        FakeModelChart model;
        model.m_latestDate = QDate(2026, 7, 10);
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 1, 15), 100.0, 101.0, 102.0, 99.0, 100.0));
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 7, 10), 110.0, 111.0, 112.0, 109.0, 100.0));

        PresenterChart presenter(&view, &model, kShareGuid);
        presenter.loadAndDisplay();

        QCOMPARE(view.lastMaxIntervalCount, 6);
        QVERIFY(model.loadDailyValuesCalled);
        QCOMPARE(model.lastQueryFrom, QDate(2026, 1, 10));

        const auto* closing = view.findSeries(SeriesKind::ClosingPrice);
        if (!closing) QFAIL("ClosingPrice series missing");
        QVERIFY(closing->dates.contains(QDate(2026, 1, 15))); // ältester Wert muss enthalten sein
    }

    void test_refresh_weekIntervalNotLandingOnEarliestDate_maxIntervalCountStillReachesIt()
    {
        // Gleicher Off-by-one-Bugfix wie beim Monats-Test oben, hier für
        // Interval=Woche: Start-Datum 10.07.2026, älteste Wert am
        // 03.06.2026 — liegt NICHT auf einer vollen 7-Tage-Grenze zum
        // Start-Datum (die Wochenschritte treffen 05.06., nicht 03.06.).
        // Alte, um eins verschobene Schleife hätte bei Anzahl=5 gestoppt
        // (Fenster ab 05.06.2026 — der Wert vom 03.06. wäre draußen
        // geblieben); korrekt ist 6 (Fenster ab 29.05.2026).
        FakeViewChart view;
        view.m_intervalUnit  = IntervalUnit::Week;
        view.m_intervalCount = 6;

        FakeModelChart model;
        model.m_latestDate = QDate(2026, 7, 10);
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 6, 3),  100.0, 101.0, 102.0, 99.0, 100.0));
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 7, 10), 110.0, 111.0, 112.0, 109.0, 100.0));

        PresenterChart presenter(&view, &model, kShareGuid);
        presenter.loadAndDisplay();

        QCOMPARE(view.lastMaxIntervalCount, 6);
        QVERIFY(model.loadDailyValuesCalled);
        QCOMPARE(model.lastQueryFrom, QDate(2026, 5, 29));

        const auto* closing = view.findSeries(SeriesKind::ClosingPrice);
        if (!closing) QFAIL("ClosingPrice series missing");
        QVERIFY(closing->dates.contains(QDate(2026, 6, 3))); // ältester Wert muss enthalten sein
    }

    void test_refresh_yearIntervalNotLandingOnEarliestDate_maxIntervalCountStillReachesIt()
    {
        // Gleicher Off-by-one-Bugfix wie oben, hier für Interval=Jahr:
        // Start-Datum 10.07.2026, ältester Wert am 15.03.2023 — liegt NICHT
        // auf einer vollen Jahres-Grenze zum Start-Datum (die Jahresschritte
        // treffen jeweils den 10.07., nie den 15.03.). Alte, um eins
        // verschobene Schleife hätte bei Anzahl=3 gestoppt (Fenster ab
        // 10.07.2023 — der Wert vom 15.03.2023 wäre draußen geblieben);
        // korrekt ist 4 (Fenster ab 10.07.2022).
        FakeViewChart view;
        view.m_intervalUnit  = IntervalUnit::Year;
        view.m_intervalCount = 4;

        FakeModelChart model;
        model.m_latestDate = QDate(2026, 7, 10);
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2023, 3, 15), 100.0, 101.0, 102.0, 99.0, 100.0));
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 7, 10), 110.0, 111.0, 112.0, 109.0, 100.0));

        PresenterChart presenter(&view, &model, kShareGuid);
        presenter.loadAndDisplay();

        QCOMPARE(view.lastMaxIntervalCount, 4);
        QVERIFY(model.loadDailyValuesCalled);
        QCOMPARE(model.lastQueryFrom, QDate(2022, 7, 10));

        const auto* closing = view.findSeries(SeriesKind::ClosingPrice);
        if (!closing) QFAIL("ClosingPrice series missing");
        QVERIFY(closing->dates.contains(QDate(2023, 3, 15))); // ältester Wert muss enthalten sein
    }

    void test_refresh_monthIntervalLandingExactlyOnEarliestDate_maxIntervalCountDoesNotOvershoot()
    {
        // Pendant zum bereits vorhandenen Tag-Test
        // (test_refresh_setsMaxIntervalCount_basedOnEarliestDailyValue), hier
        // für Interval=Monat: ältester Wert trifft die Fenstergrenze EXAKT
        // (10.07.2026 minus 6 Monate = 10.01.2026). Prüft, dass die
        // korrigierte Schleife (rangeStart(count) > earliestDate, ohne "+1")
        // im Gleichheitsfall korrekt stoppt und nicht "eine Stufe zu viel"
        // liefert — die spiegelbildliche Regression zum ursprünglichen
        // Off-by-one-Bug.
        FakeViewChart view;
        view.m_intervalUnit  = IntervalUnit::Month;
        view.m_intervalCount = 6;

        FakeModelChart model;
        model.m_latestDate = QDate(2026, 7, 10);
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 1, 10), 100.0, 101.0, 102.0, 99.0, 100.0));
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 7, 10), 110.0, 111.0, 112.0, 109.0, 100.0));

        PresenterChart presenter(&view, &model, kShareGuid);
        presenter.loadAndDisplay();

        QCOMPARE(view.lastMaxIntervalCount, 6); // NICHT 7 — würde nur dieselben Daten nochmal zeigen
        QVERIFY(model.loadDailyValuesCalled);
        QCOMPARE(model.lastQueryFrom, QDate(2026, 1, 10));

        const auto* closing = view.findSeries(SeriesKind::ClosingPrice);
        if (!closing) QFAIL("ClosingPrice series missing");
        QVERIFY(closing->dates.contains(QDate(2026, 1, 10))); // ältester Wert muss enthalten sein
    }

    void test_refresh_dayIntervalWithLongHistory_maxIntervalCountExceedsOldFixedCeiling()
    {
        // Regressionstest für den zweiten Bugfix vom 12.07.2026 (Nessies
        // Rückmeldung: "beim Intervall Tag ist maximal 999 möglich"). Die
        // alte feste Schleifen-Obergrenze (kIntervalCountCeiling = 999) war
        // bei Interval=Tag eine echte, spürbare Grenze (~2,7 Jahre), keine
        // reine Sicherheitsbremse — reale Kurshistorien (wie das Allianz-
        // SE-Referenzportfolio, 2016–2026) reichen deutlich weiter zurück.
        // Ältester Wert und Start-Datum liegen hier 3843 Tage auseinander
        // (01.01.2016 bis 10.07.2026) — mit der alten Konstante wäre
        // maxIntervalCount fälschlich bei 999 hängengeblieben, obwohl
        // deutlich mehr Historie existiert. Die neue dynamische Grenze
        // (earliestDate.daysTo(rangeEnd), zusätzlich per
        // kAbsoluteSafetyCeiling abgesichert) muss die volle Spanne
        // erreichen.
        FakeViewChart view;
        view.m_intervalUnit  = IntervalUnit::Day;
        view.m_intervalCount = 3843;

        FakeModelChart model;
        model.m_latestDate = QDate(2026, 7, 10);
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2016, 1, 1),  50.0, 51.0, 52.0, 49.0, 100.0));
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 7, 10), 110.0, 111.0, 112.0, 109.0, 100.0));

        PresenterChart presenter(&view, &model, kShareGuid);
        presenter.loadAndDisplay();

        QCOMPARE(view.lastMaxIntervalCount, 3843); // NICHT 999, die alte feste Grenze
        QVERIFY(model.loadDailyValuesCalled);
        QCOMPARE(model.lastQueryFrom, QDate(2016, 1, 1));

        const auto* closing = view.findSeries(SeriesKind::ClosingPrice);
        if (!closing) QFAIL("ClosingPrice series missing");
        QVERIFY(closing->dates.contains(QDate(2016, 1, 1))); // ältester Wert muss enthalten sein
    }

    void test_refresh_corruptEarliestDate_maxIntervalCountClampedByAbsoluteSafetyCeiling()
    {
        // Deckt die absolute Notbremse (kAbsoluteSafetyCeiling) ab, die von
        // keinem der obigen Tests tatsächlich ausgelöst wird — die laufen
        // alle über den normalen dynamischen Pfad (earliestDate.daysTo(
        // rangeEnd) bleibt dort weit unter der Bremse). Simuliert ein
        // korruptes/unplausibles Datum in der DB (Jahr -1000, weit vor jeder
        // realen Kurshistorie) — daysTo(rangeEnd) läge dabei bei rund 1,1
        // Millionen Tagen, deutlich über kAbsoluteSafetyCeiling (1.000.000).
        // Erwartung: die Schleife wird exakt bei kAbsoluteSafetyCeiling
        // gekappt, statt bis zum korrupten Datum hochzulaufen — und der Test
        // selbst läuft dank der Bremse in Millisekunden statt zu hängen
        // (eine echte Endlosschleife würde diesen Test zum Timeout bringen).
        FakeViewChart view;
        view.m_intervalUnit  = IntervalUnit::Day;
        view.m_intervalCount = 2000000; // absichtlich weit über jede sinnvolle Grenze angefordert

        FakeModelChart model;
        model.m_latestDate = QDate(2026, 7, 10);
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(-1000, 1, 1), 50.0, 51.0, 52.0, 49.0, 100.0));
        model.m_dailyValues.append(DailyValuesObject(kShareGuid, QDate(2026, 7, 10), 110.0, 111.0, 112.0, 109.0, 100.0));

        PresenterChart presenter(&view, &model, kShareGuid);
        presenter.loadAndDisplay();

        // 1.000.000 == kAbsoluteSafetyCeiling — NICHT die volle Tagesspanne
        // zum korrupten Datum (~1,1 Mio.), die Notbremse hat gegriffen.
        QCOMPARE(view.lastMaxIntervalCount, 1000000);
    }
};

QTEST_MAIN(TestChartForm)
#include "tst_chartform.moc"

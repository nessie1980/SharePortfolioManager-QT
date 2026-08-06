// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// Testet ausschliesslich PresenterPortfolioChart über ein Fake-View/
// Fake-Model-Paar, analog zu tst_chartform.cpp — keine echte Datenbank, kein
// QWidget, keine QtCharts-Instanziierung.
//
// Die eigentliche Rechenlogik liegt in PortfolioSeriesCalculator und ist dort
// eigenständig getestet (tests/utils/tst_portfolioseriescalculator.cpp). Hier
// geht es um das Zusammenspiel: welche Setter der Presenter in welcher
// Reihenfolge bedient, wie er den Zeitraum umrechnet und wie er die Texte
// aufbaut.

#include <QtTest>

#include "../../app/forms/PortfolioChartForm/PresenterPortfolioChart.h"

// ── Fake View ──────────────────────────────────────────────────────────────

class FakeViewPortfolioChart : public IViewPortfolioChart
{
public:
    // Steuerwerte, die der Presenter liest
    QDate        m_startDate     = QDate(2026, 8, 5);
    IntervalUnit m_intervalUnit  = IntervalUnit::Year;
    int          m_intervalCount = 1;

    // Mitgeschriebene Setter-Aufrufe
    QDate              lastDefaultStartDate;
    bool               defaultStartDateSet = false;
    int                lastMaxIntervalCount = -1;
    PortfolioChartData lastChartData;
    bool               chartDataSet = false;
    QString            lastEmptyMessage;
    bool               emptyShown = false;
    QString            lastCalculatingMessage;
    QString            lastWarning;
    QString            lastRangeInfo;
    QString            lastError;

    /// Aufrufreihenfolge, für Tests die auf die Abfolge prüfen.
    QStringList callLog;

    QDate        startDate()     const override { return m_startDate; }
    IntervalUnit intervalUnit()  const override { return m_intervalUnit; }
    int          intervalCount() const override { return m_intervalCount; }

    void setDefaultStartDate(const QDate& date) override
    {
        lastDefaultStartDate = date;
        defaultStartDateSet  = true;
        m_startDate          = date; // spiegelt ViewPortfolioChart
        callLog << QStringLiteral("setDefaultStartDate");
    }

    void setMaxIntervalCount(int maxCount) override
    {
        lastMaxIntervalCount = maxCount;
        callLog << QStringLiteral("setMaxIntervalCount");
        // Bewusst kein automatisches Klemmen von m_intervalCount — anders als
        // QSpinBox::setMaximum(). Gleiche Begründung wie in tst_chartform.cpp.
    }

    void setChartData(const PortfolioChartData& data) override
    {
        lastChartData = data;
        chartDataSet  = true;
        callLog << QStringLiteral("setChartData");
    }

    void showEmptyChart(const QString& message) override
    {
        lastEmptyMessage = message;
        emptyShown       = true;
        callLog << QStringLiteral("showEmptyChart");
    }

    void showCalculating(const QString& message) override
    {
        lastCalculatingMessage = message;
        callLog << QStringLiteral("showCalculating");
    }

    void setWarning(const QString& message) override
    {
        lastWarning = message;
        callLog << QStringLiteral("setWarning");
    }

    void setRangeInfo(const QString& infoText) override
    {
        lastRangeInfo = infoText;
        callLog << QStringLiteral("setRangeInfo");
    }

    void showError(const QString& message) override
    {
        lastError = message;
        callLog << QStringLiteral("showError");
    }
};

// ── Fake Model ─────────────────────────────────────────────────────────────

class FakeModelPortfolioChart : public IModelPortfolioChart
{
public:
    QList<PortfolioShareSeriesInput> m_input;
    QDate                            m_earliest;
    mutable int                      loadCount = 0;

    QList<PortfolioShareSeriesInput> loadPortfolioInput() const override
    {
        ++loadCount;
        return m_input;
    }

    QDate earliestRelevantDate() const override { return m_earliest; }
};

// ── Tests ──────────────────────────────────────────────────────────────────

class TestPortfolioChartForm : public QObject
{
    Q_OBJECT

private:
    static QDate d(int year, int month, int day) { return QDate(year, month, day); }

    /// One share, bought at 100, price rising to 110 — a clean +10 % curve.
    static PortfolioShareSeriesInput risingShare()
    {
        PortfolioShareSeriesInput share;
        share.shareGuid = QStringLiteral("guid-a");
        share.name      = QStringLiteral("Aktie A");
        share.buys      = { { d(2026, 6, 1), 10.0, 100.0 } };
        share.prices    = { { d(2026, 6, 1), 100.0 },
                            { d(2026, 7, 1), 110.0 } };
        return share;
    }

    static PortfolioShareSeriesInput shareWithoutHistory(const QString& name)
    {
        PortfolioShareSeriesInput share;
        share.shareGuid = QStringLiteral("guid-x");
        share.name      = name;
        share.buys      = { { d(2026, 6, 1), 10.0, 100.0 } };
        // prices bleibt leer — Update-Typ "Nur Kurs" / "Kein Update"
        return share;
    }

private slots:

    // ── loadAndDisplay ───────────────────────────────────────────────────

    void test_loadAndDisplay_setsTodayAsDefaultStartDate()
    {
        FakeViewPortfolioChart  view;
        FakeModelPortfolioChart model;
        model.m_input    = { risingShare() };
        model.m_earliest = d(2026, 6, 1);

        PresenterPortfolioChart presenter(&view, &model);
        presenter.loadAndDisplay();

        QVERIFY(view.defaultStartDateSet);
        QCOMPARE(view.lastDefaultStartDate, QDate::currentDate());
    }

    void test_loadAndDisplay_drawsCurveAndRangeInfo()
    {
        FakeViewPortfolioChart  view;
        FakeModelPortfolioChart model;
        model.m_input    = { risingShare() };
        model.m_earliest = d(2026, 6, 1);
        // Start-Datum wird von setDefaultStartDate() auf heute gesetzt; damit
        // das Fenster die Testdaten umfasst, genügt Interval = Jahr.
        view.m_intervalUnit  = IntervalUnit::Year;
        view.m_intervalCount = 5;

        PresenterPortfolioChart presenter(&view, &model);
        presenter.loadAndDisplay();

        QVERIFY(view.chartDataSet);
        QCOMPARE(view.lastChartData.points.size(), 2);
        QVERIFY(qAbs(view.lastChartData.points.constFirst().development -   0.0) < 0.005);
        QVERIFY(qAbs(view.lastChartData.points.constLast().development  - 100.0) < 0.005);
        QVERIFY(view.lastRangeInfo.contains(QStringLiteral("Zeitraum:")));
        QVERIFY(view.lastRangeInfo.contains(QStringLiteral("Entwicklung:")));
    }

    void test_loadAndDisplay_showsCalculatingBeforeChartData()
    {
        FakeViewPortfolioChart  view;
        FakeModelPortfolioChart model;
        model.m_input    = { risingShare() };
        model.m_earliest = d(2026, 6, 1);
        view.m_intervalCount = 5;

        PresenterPortfolioChart presenter(&view, &model);
        presenter.loadAndDisplay();

        const int calculating = view.callLog.indexOf(QStringLiteral("showCalculating"));
        const int chartData   = view.callLog.indexOf(QStringLiteral("setChartData"));
        QVERIFY(calculating >= 0);
        QVERIFY(chartData   >= 0);
        QVERIFY(calculating < chartData);
    }

    void test_loadAndDisplay_noSharesAtAll_showsEmptyChart()
    {
        FakeViewPortfolioChart  view;
        FakeModelPortfolioChart model; // leeres Portfolio

        PresenterPortfolioChart presenter(&view, &model);
        presenter.loadAndDisplay();

        QVERIFY(view.emptyShown);
        QVERIFY(!view.chartDataSet);
        QVERIFY(view.lastWarning.isEmpty());
        QVERIFY(view.lastRangeInfo.isEmpty());
    }

    void test_loadAndDisplay_onlySharesWithoutHistory_warnsAndShowsEmpty()
    {
        FakeViewPortfolioChart  view;
        FakeModelPortfolioChart model;
        model.m_input = { shareWithoutHistory(QStringLiteral("Ohne Historie")) };

        PresenterPortfolioChart presenter(&view, &model);
        presenter.loadAndDisplay();

        QVERIFY(view.emptyShown);
        QVERIFY(!view.chartDataSet);
        QVERIFY(view.lastWarning.contains(QStringLiteral("Ohne Historie")));
    }

    void test_warningNamesExcludedShares()
    {
        FakeViewPortfolioChart  view;
        FakeModelPortfolioChart model;
        model.m_input    = { risingShare(), shareWithoutHistory(QStringLiteral("Stille Aktie")) };
        model.m_earliest = d(2026, 6, 1);
        view.m_intervalCount = 5;

        PresenterPortfolioChart presenter(&view, &model);
        presenter.loadAndDisplay();

        QVERIFY(view.chartDataSet);
        QVERIFY(view.lastWarning.contains(QStringLiteral("Stille Aktie")));
        QVERIFY(!view.lastWarning.contains(QStringLiteral("Aktie A")));
    }

    void test_warningIsClearedWhenAllSharesHaveHistory()
    {
        FakeViewPortfolioChart  view;
        FakeModelPortfolioChart model;
        model.m_input    = { risingShare() };
        model.m_earliest = d(2026, 6, 1);
        view.m_intervalCount = 5;

        PresenterPortfolioChart presenter(&view, &model);
        presenter.loadAndDisplay();

        QVERIFY(view.lastWarning.isEmpty());
    }

    // ── Refresh / reload ─────────────────────────────────────────────────

    void test_onControlsChanged_recomputesWithNarrowerWindow()
    {
        FakeViewPortfolioChart  view;
        FakeModelPortfolioChart model;
        model.m_input    = { risingShare() };
        model.m_earliest = d(2026, 6, 1);
        view.m_intervalCount = 5;

        PresenterPortfolioChart presenter(&view, &model);
        presenter.loadAndDisplay();
        QCOMPARE(view.lastChartData.points.size(), 2);

        // Ein Tag Fenster — die Testdaten liegen ausserhalb, es bleibt nichts.
        view.m_intervalUnit  = IntervalUnit::Day;
        view.m_intervalCount = 1;
        view.emptyShown = false;
        presenter.onControlsChanged();

        QVERIFY(view.emptyShown);
    }

    void test_reload_readsTheModelAgain()
    {
        FakeViewPortfolioChart  view;
        FakeModelPortfolioChart model;
        model.m_input    = { risingShare() };
        model.m_earliest = d(2026, 6, 1);
        view.m_intervalCount = 5;

        PresenterPortfolioChart presenter(&view, &model);
        presenter.loadAndDisplay();
        QCOMPARE(model.loadCount, 1);

        presenter.reload();
        QCOMPARE(model.loadCount, 2);
    }

    void test_onControlsChanged_doesNotReadTheModelAgain()
    {
        // Die Daten werden gecacht — nur reload() liest neu.
        FakeViewPortfolioChart  view;
        FakeModelPortfolioChart model;
        model.m_input    = { risingShare() };
        model.m_earliest = d(2026, 6, 1);
        view.m_intervalCount = 5;

        PresenterPortfolioChart presenter(&view, &model);
        presenter.loadAndDisplay();
        presenter.onControlsChanged();

        QCOMPARE(model.loadCount, 1);
    }

    // ── computeRangeStart ────────────────────────────────────────────────

    void test_computeRangeStart_allUnits()
    {
        const QDate end = d(2026, 8, 5);

        QCOMPARE(PresenterPortfolioChart::computeRangeStart(end, IntervalUnit::Day,   3),
                 d(2026, 8, 2));
        QCOMPARE(PresenterPortfolioChart::computeRangeStart(end, IntervalUnit::Week,  2),
                 d(2026, 7, 22));
        QCOMPARE(PresenterPortfolioChart::computeRangeStart(end, IntervalUnit::Month, 1),
                 d(2026, 7, 5));
        QCOMPARE(PresenterPortfolioChart::computeRangeStart(end, IntervalUnit::Year,  1),
                 d(2025, 8, 5));
    }

    void test_computeRangeStart_countBelowOneIsTreatedAsOne()
    {
        const QDate end = d(2026, 8, 5);
        QCOMPARE(PresenterPortfolioChart::computeRangeStart(end, IntervalUnit::Day, 0),
                 d(2026, 8, 4));
    }

    // ── computeMaxIntervalCount ──────────────────────────────────────────

    void test_computeMaxIntervalCount_stopsAtOldestValue()
    {
        // Ältester Wert 10 Tage zurück -> mehr als 10 Tagesschritte bringen
        // nichts mehr.
        const int max = PresenterPortfolioChart::computeMaxIntervalCount(
            d(2026, 8, 11), IntervalUnit::Day, d(2026, 8, 1));
        QCOMPARE(max, 10);
    }

    void test_computeMaxIntervalCount_withoutHistoryReturnsOne()
    {
        QCOMPARE(PresenterPortfolioChart::computeMaxIntervalCount(
                     d(2026, 8, 5), IntervalUnit::Month, QDate()), 1);
    }

    void test_computeMaxIntervalCount_oldestNotBeforeRangeEndReturnsOne()
    {
        QCOMPARE(PresenterPortfolioChart::computeMaxIntervalCount(
                     d(2026, 8, 5), IntervalUnit::Month, d(2026, 8, 5)), 1);
    }

    void test_computeMaxIntervalCount_yearsAreCounted()
    {
        const int max = PresenterPortfolioChart::computeMaxIntervalCount(
            d(2026, 8, 5), IntervalUnit::Year, d(2023, 8, 5));
        QCOMPARE(max, 3);
    }

    // ── Textaufbau ───────────────────────────────────────────────────────

    void test_buildWarningText_emptyForEmptyList()
    {
        QVERIFY(PresenterPortfolioChart::buildWarningText({}).isEmpty());
    }

    void test_buildWarningText_joinsNames()
    {
        const QString text = PresenterPortfolioChart::buildWarningText(
            { QStringLiteral("A"), QStringLiteral("B") });
        QVERIFY(text.contains(QStringLiteral("A, B")));
    }

    void test_buildRangeInfo_withoutPointsShowsOnlyRange()
    {
        const QString text = PresenterPortfolioChart::buildRangeInfo(
            d(2025, 8, 5), d(2026, 8, 5), {});
        QVERIFY(text.contains(QStringLiteral("05.08.2025")));
        QVERIFY(text.contains(QStringLiteral("05.08.2026")));
        QVERIFY(!text.contains(QStringLiteral("Entwicklung:")));
    }

    void test_diagnosticsCsv_containsBothBlocks()
    {
        FakeViewPortfolioChart  view;
        FakeModelPortfolioChart model;
        model.m_input    = { risingShare(), shareWithoutHistory(QStringLiteral("Stille Aktie")) };
        model.m_earliest = d(2026, 6, 1);
        view.m_intervalCount = 5;

        PresenterPortfolioChart presenter(&view, &model);
        presenter.loadAndDisplay();

        const QString csv = presenter.buildDiagnosticsCsv();

        // Kopf, beide Blockueberschriften und je eine Zeile pro Aktie.
        QVERIFY(csv.contains(QStringLiteral("Zeitraum;")));
        QVERIFY(csv.contains(QStringLiteral("Ausgeschlossen")));
        QVERIFY(csv.contains(QStringLiteral("Entwicklung %")));
        QVERIFY(csv.contains(QStringLiteral("Aktie A;")));
        QVERIFY(csv.contains(QStringLiteral("Stille Aktie;")));
    }

    void test_buildRangeInfo_usesLastPoint()
    {
        const QList<PortfolioChartPoint> points = {
            { d(2026, 6, 1),   0.0,  0.0 },
            { d(2026, 7, 1), 259.0, 12.95 }
        };

        const QString text = PresenterPortfolioChart::buildRangeInfo(
            d(2026, 6, 1), d(2026, 7, 1), points);

        QVERIFY(text.contains(QStringLiteral("Entwicklung:")));
        QVERIFY(text.contains(QLocale().toString(259.0, 'f', 2)));
        QVERIFY(text.contains(QLocale().toString(12.95, 'f', 2)));
    }
};

QTEST_MAIN(TestPortfolioChartForm)
#include "tst_portfoliochartform.moc"

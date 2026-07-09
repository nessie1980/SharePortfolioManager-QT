// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// Testet ausschliesslich PresenterShareDetails ueber ein Fake-View/Fake-Model-
// Paar (analog zum FakeNetworkAccessManager-Muster bei tst_parser.cpp) - es
// wird bewusst keine echte Datenbank, kein QWidget und kein ShareCalculator
// instanziiert. Die Zuordnung ShareValues-Feld -> Box-Zeile ist gegen
// TabControl.cs (C#-Referenz) sowie ShareCalculator.h abgeglichen
// (09.07.2026) - siehe ARCHITECTURE.md "ShareDetailsForm-Details".

#include <QtTest>

#include "../../app/forms/ShareDetailsForm/PresenterShareDetails.h"

// ── Fake View ──────────────────────────────────────────────────────────────

class FakeViewShareDetails : public IViewShareDetails
{
public:
    QString headerName;
    QString statusLine;

    CalculationRows gesamtRows;
    CalculationRows vortagRows;
    CalculationRows aktuelleRows;

    QString errorMessage;
    bool    closed = false;

    void setHeaderName(const QString& name) override { headerName = name; }
    void setStatusLine(const QString& statusText) override { statusLine = statusText; }

    void populateGesamtBox(const CalculationRows& rows) override { gesamtRows = rows; }
    void populateVortagBox(const CalculationRows& rows) override { vortagRows = rows; }
    void populateAktuelleBox(const CalculationRows& rows) override { aktuelleRows = rows; }

    void showError(const QString& message) override { errorMessage = message; }
    void closeDialog() override { closed = true; }

    /** Finds a row by its (translated) label — fails the calling test via QVERIFY if absent. */
    static const CalculationRow* findRow(const CalculationRows& rows, const QString& label)
    {
        for (const CalculationRow& r : rows)
            if (r.label == label)
                return &r;
        return nullptr;
    }
};

// ── Fake Model ─────────────────────────────────────────────────────────────

class FakeModelShareDetails : public IModelShareDetails
{
public:
    ShareObject share; // default-constructed => isValid() == false
    ShareValues values;

    ShareObject loadShare(const QString&) const override { return share; }

    ShareValues computeShareValues(const QString&, double, double) const override
    {
        return values;
    }
};

// ── Test class ─────────────────────────────────────────────────────────────

class TestShareDetailsForm : public QObject
{
    Q_OBJECT

private slots:

    void test_loadAndDisplay_shareNotFound_showsErrorAndCloses()
    {
        FakeViewShareDetails view;
        FakeModelShareDetails model;
        // model.share stays default-constructed => invalid

        PresenterShareDetails presenter(view, model, QStringLiteral("unknown-guid"));
        const bool result = presenter.loadAndDisplay();

        QVERIFY(!result);
        QVERIFY(!view.errorMessage.isEmpty());
        QVERIFY(view.closed);
        QVERIFY(view.headerName.isEmpty());   // nothing else was populated
        QVERIFY(view.gesamtRows.isEmpty());
    }

    void test_loadAndDisplay_validShare_setsHeaderAndStatusLine()
    {
        FakeViewShareDetails view;
        FakeModelShareDetails model;

        model.share = ShareObject(QStringLiteral("g1"), QStringLiteral("BAS001"),
                                   QStringLiteral("DE000BAS0011"), QStringLiteral("BASF SE"),
                                   ShareType::Share, QStringLiteral("EUR"));

        PresenterShareDetails presenter(view, model, QStringLiteral("g1"));
        const bool result = presenter.loadAndDisplay();

        QVERIFY(result);
        QVERIFY(!view.closed);
        QCOMPARE(view.headerName, QStringLiteral("BASF SE"));
        QVERIFY(view.statusLine.contains(QStringLiteral("noch nicht aktualisiert")));
        QVERIFY(view.statusLine.contains(QStringLiteral("Aktie")));
    }

    void test_loadAndDisplay_lastInternetUpdateSet_appearsInStatusLine()
    {
        FakeViewShareDetails view;
        FakeModelShareDetails model;

        model.share = ShareObject(QStringLiteral("g2"), QStringLiteral("XYZ001"),
                                   QStringLiteral("DE000XYZ0019"), QStringLiteral("Test AG"),
                                   ShareType::Fond);
        model.share.setLastInternetUpdate(QStringLiteral("09.07.2026 20:34"));

        PresenterShareDetails presenter(view, model, QStringLiteral("g2"));
        QVERIFY(presenter.loadAndDisplay());

        QVERIFY(view.statusLine.contains(QStringLiteral("09.07.2026 20:34")));
        QVERIFY(!view.statusLine.contains(QStringLiteral("noch nicht aktualisiert")));
        QVERIFY(view.statusLine.contains(QStringLiteral("Fonds")));
    }

    void test_loadAndDisplay_gesamtBox_mapsShareValuesFieldsDirectly()
    {
        FakeViewShareDetails view;
        FakeModelShareDetails model;
        model.share = ShareObject(QStringLiteral("g3"), QStringLiteral("WKN003"),
                                   QStringLiteral("ISIN0000003"), QStringLiteral("Drei AG"));

        ShareValues& v = model.values;
        v.volume             = 40.0;
        v.curPrice           = 484.40;
        v.curValue           = 19376.00;
        v.totalDividend      = 0.0;
        v.salePayoutFinal    = 33253.75;
        v.completeCurValue   = 52629.75;
        v.completePurchase   = 39439.21;
        v.completeProfitLoss = 13190.54;
        v.completeProfitPct  = 33.45;

        PresenterShareDetails presenter(view, model, QStringLiteral("g3"));
        QVERIFY(presenter.loadAndDisplay());

        const QLocale locale;

        const CalculationRow* purchase = FakeViewShareDetails::findRow(view.gesamtRows, QStringLiteral("Einzahlungen:"));
        QVERIFY(purchase);
        QCOMPARE(purchase->value, locale.toString(19376.00, 'f', 2) + QStringLiteral(" €"));
        QVERIFY(purchase->emphasize);

        const CalculationRow* sales = FakeViewShareDetails::findRow(view.gesamtRows, QStringLiteral("Verkäufe:"));
        QVERIFY(sales);
        QCOMPARE(sales->value, locale.toString(33253.75, 'f', 2) + QStringLiteral(" €"));

        const CalculationRow* pl = FakeViewShareDetails::findRow(view.gesamtRows, QStringLiteral("Gewinn / Verlust (gesamt):"));
        QVERIFY(pl);
        QCOMPARE(pl->value, locale.toString(13190.54, 'f', 2) + QStringLiteral(" €"));
        QCOMPARE(pl->color, QColor(QStringLiteral("green"))); // >= 0
        QVERIFY(pl->emphasize);

        const CalculationRow* perf = FakeViewShareDetails::findRow(view.gesamtRows, QStringLiteral("Entwicklung:"));
        QVERIFY(perf);
        QCOMPARE(perf->value, locale.toString(33.45, 'f', 2) + QStringLiteral(" %"));
        QCOMPARE(perf->color, QColor(QStringLiteral("green")));
    }

    void test_loadAndDisplay_gesamtBox_negativeProfitLoss_setsRedColor()
    {
        FakeViewShareDetails view;
        FakeModelShareDetails model;
        model.share = ShareObject(QStringLiteral("g4"), QStringLiteral("WKN004"),
                                   QStringLiteral("ISIN0000004"), QStringLiteral("Vier AG"));
        model.values.completeProfitLoss = -50.0;
        model.values.completeProfitPct  = -2.5;

        PresenterShareDetails presenter(view, model, QStringLiteral("g4"));
        QVERIFY(presenter.loadAndDisplay());

        const CalculationRow* pl = FakeViewShareDetails::findRow(view.gesamtRows, QStringLiteral("Gewinn / Verlust (gesamt):"));
        QVERIFY(pl);
        QCOMPARE(pl->color, QColor(Qt::red));
    }

    void test_loadAndDisplay_vortagBox_computesProfitLossFromVolumeTimesDiff()
    {
        FakeViewShareDetails view;
        FakeModelShareDetails model;
        model.share = ShareObject(QStringLiteral("g5"), QStringLiteral("WKN005"),
                                   QStringLiteral("ISIN0000005"), QStringLiteral("Fuenf AG"));

        ShareValues& v = model.values;
        v.volume       = 40.0;
        v.curPrice     = 484.40;
        v.prevDayPrice = 442.50;
        v.prevDayDiff  = 41.90;
        v.prevDayPct   = 9.47;

        PresenterShareDetails presenter(view, model, QStringLiteral("g5"));
        QVERIFY(presenter.loadAndDisplay());

        const QLocale locale;

        const CalculationRow* diff = FakeViewShareDetails::findRow(view.vortagRows, QStringLiteral("Preis-Entw.:"));
        QVERIFY(diff);
        QCOMPARE(diff->value, locale.toString(41.90, 'f', 2) + QStringLiteral(" €"));
        QCOMPARE(diff->color, QColor(QStringLiteral("green")));

        // 40.00 * 41.90 = 1676.00 (matches the C# reference screenshot exactly)
        const CalculationRow* pl = FakeViewShareDetails::findRow(view.vortagRows, QStringLiteral("Gewinn / Verlust:"));
        QVERIFY(pl);
        QCOMPARE(pl->value, locale.toString(1676.00, 'f', 2) + QStringLiteral(" €"));
        QCOMPARE(pl->color, QColor(QStringLiteral("green")));
    }

    void test_loadAndDisplay_vortagBox_negativeDiff_setsRedColor()
    {
        FakeViewShareDetails view;
        FakeModelShareDetails model;
        model.share = ShareObject(QStringLiteral("g6"), QStringLiteral("WKN006"),
                                   QStringLiteral("ISIN0000006"), QStringLiteral("Sechs AG"));
        model.values.volume      = 10.0;
        model.values.prevDayDiff = -5.0;

        PresenterShareDetails presenter(view, model, QStringLiteral("g6"));
        QVERIFY(presenter.loadAndDisplay());

        const CalculationRow* diff = FakeViewShareDetails::findRow(view.vortagRows, QStringLiteral("Preis-Entw.:"));
        QVERIFY(diff);
        QCOMPARE(diff->color, QColor(Qt::red));

        const CalculationRow* pl = FakeViewShareDetails::findRow(view.vortagRows, QStringLiteral("Gewinn / Verlust:"));
        QVERIFY(pl);
        const QLocale locale;
        // 10.00 * -5.00 = -50.00
        QCOMPARE(pl->value, locale.toString(-50.00, 'f', 2) + QStringLiteral(" €"));
    }

    void test_loadAndDisplay_aktuelleBox_sumAddsCurValueDividendAndSaleProfitLoss()
    {
        FakeViewShareDetails view;
        FakeModelShareDetails model;
        model.share = ShareObject(QStringLiteral("g7"), QStringLiteral("WKN007"),
                                   QStringLiteral("ISIN0000007"), QStringLiteral("Sieben AG"));

        ShareValues& v = model.values;
        v.volume             = 40.0;
        v.curPrice           = 484.40;
        v.curValue           = 19376.00;
        v.totalDividend      = 0.0;
        v.saleProfitLossFinal = -252.20;

        PresenterShareDetails presenter(view, model, QStringLiteral("g7"));
        QVERIFY(presenter.loadAndDisplay());

        const QLocale locale;

        const CalculationRow* saleProfit = FakeViewShareDetails::findRow(
            view.aktuelleRows, QStringLiteral("Gewinn / Verlust (Verkäufe):"));
        QVERIFY(saleProfit);
        QCOMPARE(saleProfit->value, locale.toString(-252.20, 'f', 2) + QStringLiteral(" €"));

        // 19376.00 + 0.00 + (-252.20) = 19123.80
        const CalculationRow* sum = FakeViewShareDetails::findRow(view.aktuelleRows, QStringLiteral("Summe:"));
        QVERIFY(sum);
        QCOMPARE(sum->value, locale.toString(19123.80, 'f', 2) + QStringLiteral(" €"));
        QVERIFY(sum->emphasize);
    }
};

QTEST_MAIN(TestShareDetailsForm)
#include "tst_sharedetailsform.moc"

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
#include <QDateTime>

#include "../../app/forms/ShareDetailsForm/PresenterShareDetails.h"

// ── Fake View ──────────────────────────────────────────────────────────────

class FakeViewShareDetails : public IViewShareDetails
{
public:
    QString headerName;
    QString statusLine;
    QString websiteUpdateLine;
    QString boxesTabTitle;

    CalculationRows gesamtRows;
    CalculationRows vortagRows;
    CalculationRows aktuelleRows;

    QString errorMessage;
    bool    closed = false;

    void setHeaderName(const QString& name) override { headerName = name; }
    void setStatusLine(const QString& statusText) override { statusLine = statusText; }
    void setWebsiteUpdateLine(const QString& statusText) override { websiteUpdateLine = statusText; }
    void setBoxesTabTitle(const QString& title) override { boxesTabTitle = title; }

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
        // Realistisches Eingabeformat: ShareObject::lastInternetUpdate() liefert
        // den in der DB gespeicherten ISO-8601-String (z. B. "2026-07-09T20:34:00"),
        // KEINEN bereits formatierten Text — bestätigt durch Prüfung der echten
        // Allianz-SE-Validierungs-DB (11.07.2026). PresenterShareDetails muss ihn
        // über QLocale formatieren (Länderschema-Bug, behoben 11.07.2026).
        FakeViewShareDetails view;
        FakeModelShareDetails model;

        model.share = ShareObject(QStringLiteral("g2"), QStringLiteral("XYZ001"),
                                   QStringLiteral("DE000XYZ0019"), QStringLiteral("Test AG"),
                                   ShareType::Fond);
        const QString isoDateTime = QStringLiteral("2026-07-09T20:34:00");
        model.share.setLastInternetUpdate(isoDateTime);

        PresenterShareDetails presenter(view, model, QStringLiteral("g2"));
        QVERIFY(presenter.loadAndDisplay());

        const QLocale locale;
        const QString expected = locale.toString(
            QDateTime::fromString(isoDateTime, Qt::ISODate), QLocale::ShortFormat);

        QVERIFY(view.statusLine.contains(expected));
        // Regression: der rohe ISO-String darf NICHT mehr unformatiert durchgereicht werden.
        QVERIFY(!view.statusLine.contains(isoDateTime));
        QVERIFY(!view.statusLine.contains(QStringLiteral("noch nicht aktualisiert")));
        QVERIFY(view.statusLine.contains(QStringLiteral("Fonds")));
    }

    void test_loadAndDisplay_malformedInternetUpdate_fallsBackToRawString()
    {
        // Kein gültiges ISO 8601 -> formatDateTime() zeigt den Rohwert statt
        // die Zeile stillschweigend verschwinden zu lassen.
        FakeViewShareDetails view;
        FakeModelShareDetails model;
        model.share = ShareObject(QStringLiteral("g2z"), QStringLiteral("XYZ00Z"),
                                   QStringLiteral("DE000XYZ00Z1"), QStringLiteral("Test AG Z"));
        model.share.setLastInternetUpdate(QStringLiteral("kein-datum"));

        PresenterShareDetails presenter(view, model, QStringLiteral("g2z"));
        QVERIFY(presenter.loadAndDisplay());

        QVERIFY(view.statusLine.contains(QStringLiteral("kein-datum")));
    }

    void test_loadAndDisplay_lastPriceUpdateSet_appearsInWebsiteUpdateLine()
    {
        // "Letzte Website-Aktualisierung" = Zeitpunkt der letzten Marktwert-/
        // Kurs-Aktualisierung, bestätigt von Nessie (10.07.2026) — getrennt
        // von lastInternetUpdate() (äußere StatusLine, allgemeines Update).
        FakeViewShareDetails view;
        FakeModelShareDetails model;

        model.share = ShareObject(QStringLiteral("g2b"), QStringLiteral("XYZ002"),
                                   QStringLiteral("DE000XYZ0027"), QStringLiteral("Test AG 2"));
        const QString isoDateTime = QStringLiteral("2026-07-10T11:53:00");
        model.share.setLastPriceUpdate(isoDateTime);

        PresenterShareDetails presenter(view, model, QStringLiteral("g2b"));
        QVERIFY(presenter.loadAndDisplay());

        const QLocale locale;
        const QString expected = locale.toString(
            QDateTime::fromString(isoDateTime, Qt::ISODate), QLocale::ShortFormat);

        QVERIFY(view.websiteUpdateLine.contains(expected));
        QVERIFY(!view.websiteUpdateLine.contains(isoDateTime));
        QVERIFY(!view.websiteUpdateLine.contains(QStringLiteral("noch nicht aktualisiert")));
    }

    void test_loadAndDisplay_noPriceUpdate_websiteUpdateLineShowsPlaceholder()
    {
        FakeViewShareDetails view;
        FakeModelShareDetails model;
        model.share = ShareObject(QStringLiteral("g2c"), QStringLiteral("XYZ003"),
                                   QStringLiteral("DE000XYZ0035"), QStringLiteral("Test AG 3"));
        // lastPriceUpdate() bleibt leer (default-konstruiert)

        PresenterShareDetails presenter(view, model, QStringLiteral("g2c"));
        QVERIFY(presenter.loadAndDisplay());

        QVERIFY(view.websiteUpdateLine.contains(QStringLiteral("noch nicht aktualisiert")));
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

        const CalculationRow* bestandswert = FakeViewShareDetails::findRow(view.gesamtRows, QStringLiteral("Aktueller Bestandswert:"));
        QVERIFY(bestandswert);
        QCOMPARE(bestandswert->value, locale.toString(19376.00, 'f', 2) + QStringLiteral(" €"));
        QVERIFY(bestandswert->emphasize);

        const CalculationRow* sales = FakeViewShareDetails::findRow(view.gesamtRows, QStringLiteral("Verkäufe:"));
        QVERIFY(sales);
        QCOMPARE(sales->value, locale.toString(33253.75, 'f', 2) + QStringLiteral(" €"));

        // "Alle Einzahlungen:" — Label bewusst umbenannt gegenüber dem C#-Original
        // ("Verkaufte Einzahlungen"), der Wert (completePurchase) ist die
        // Summe ALLER Käufe (verkauft + gehalten), von Nessie bestätigt (10.07.2026).
        const CalculationRow* allDeposits = FakeViewShareDetails::findRow(view.gesamtRows, QStringLiteral("Alle Einzahlungen:"));
        QVERIFY(allDeposits);
        QCOMPARE(allDeposits->value, locale.toString(39439.21, 'f', 2) + QStringLiteral(" €"));

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

        const CalculationRow* diff = FakeViewShareDetails::findRow(view.vortagRows, QStringLiteral("Kurswert-Entw.:"));
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

        const CalculationRow* diff = FakeViewShareDetails::findRow(view.vortagRows, QStringLiteral("Kurswert-Entw.:"));
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

    // ── Marktwert-Modus (marketValueMode = true) ────────────────────────────
    // Fixture-Werte sind der echten C#-Referenz entnommen (Screenshot
    // "AGIF-Allianz Glo.Eq.Insights", 10.07.2026) und wie folgt gegengerechnet:
    //   sum        = curValue + salePayoutMarket      = 37969.39 + 7766.03 = 45735.42
    //   profitLoss = sum - completePurchaseMarket      = 45735.42 - 29442.56 = 16292.86
    //   profitPct  = profitLoss / completePurchaseMarket * 100 = 55.34 %
    //   marketValue (Aktuelle "Summe") = curValue + saleProfitLoss = 37969.39 + 152.82 = 38122.21

    void test_loadAndDisplay_marketMode_setsTabTitle()
    {
        FakeViewShareDetails view;
        FakeModelShareDetails model;
        model.share = ShareObject(QStringLiteral("m1"), QStringLiteral("AGIF001"),
                                   QStringLiteral("LU0000000001"),
                                   QStringLiteral("AGIF-Allianz Glo.Eq.Insights"), ShareType::Fond);

        PresenterShareDetails presenter(view, model, QStringLiteral("m1"), /*marketValueMode=*/true);
        QVERIFY(presenter.loadAndDisplay());

        QCOMPARE(view.boxesTabTitle, QStringLiteral("Komplette Marktbewertung"));
    }

    void test_loadAndDisplay_depotwertMode_setsTabTitle()
    {
        // Regression: default (marketValueMode = false) must still say
        // "Komplette Depotbewertung", unaffected by the new parameter.
        FakeViewShareDetails view;
        FakeModelShareDetails model;
        model.share = ShareObject(QStringLiteral("m2"), QStringLiteral("WKN002"),
                                   QStringLiteral("ISIN0000002"), QStringLiteral("Zwei AG"));

        PresenterShareDetails presenter(view, model, QStringLiteral("m2"));
        QVERIFY(presenter.loadAndDisplay());

        QCOMPARE(view.boxesTabTitle, QStringLiteral("Komplette Depotbewertung"));
    }

    void test_loadAndDisplay_marketMode_gesamtBox_matchesScreenshotValues()
    {
        FakeViewShareDetails view;
        FakeModelShareDetails model;
        model.share = ShareObject(QStringLiteral("m3"), QStringLiteral("AGIF001"),
                                   QStringLiteral("LU0000000001"),
                                   QStringLiteral("AGIF-Allianz Glo.Eq.Insights"), ShareType::Fond);

        ShareValues& v = model.values;
        v.volume                = 168.50796;
        v.curPrice               = 225.327;
        v.curValue               = 37969.39;
        v.salePayoutMarket       = 7766.03;
        v.completePurchaseMarket = 29442.56;

        PresenterShareDetails presenter(view, model, QStringLiteral("m3"), /*marketValueMode=*/true);
        QVERIFY(presenter.loadAndDisplay());

        const QLocale locale;

        const CalculationRow* div = FakeViewShareDetails::findRow(view.gesamtRows, QStringLiteral("Dividenden:"));
        QVERIFY(div);
        QCOMPARE(div->value, QStringLiteral("-"));
        QCOMPARE(div->color, QColor(Qt::gray));

        const CalculationRow* sales = FakeViewShareDetails::findRow(view.gesamtRows, QStringLiteral("Verkäufe:"));
        QVERIFY(sales);
        QCOMPARE(sales->value, locale.toString(7766.03, 'f', 2) + QStringLiteral(" €"));

        const CalculationRow* sum = FakeViewShareDetails::findRow(view.gesamtRows, QStringLiteral("Summe:"));
        QVERIFY(sum);
        QCOMPARE(sum->value, locale.toString(45735.42, 'f', 2) + QStringLiteral(" €"));

        const CalculationRow* purchase = FakeViewShareDetails::findRow(
            view.gesamtRows, QStringLiteral("Alle Einzahlungen:"));
        QVERIFY(purchase);
        QCOMPARE(purchase->value, locale.toString(29442.56, 'f', 2) + QStringLiteral(" €"));

        const CalculationRow* pl = FakeViewShareDetails::findRow(
            view.gesamtRows, QStringLiteral("Gewinn / Verlust (gesamt):"));
        QVERIFY(pl);
        QCOMPARE(pl->value, locale.toString(16292.86, 'f', 2) + QStringLiteral(" €"));
        QCOMPARE(pl->color, QColor(QStringLiteral("green")));

        const CalculationRow* perf = FakeViewShareDetails::findRow(view.gesamtRows, QStringLiteral("Entwicklung:"));
        QVERIFY(perf);
        QCOMPARE(perf->value, locale.toString(55.34, 'f', 2) + QStringLiteral(" %"));
    }

    void test_loadAndDisplay_marketMode_aktuelleBox_matchesScreenshotValues()
    {
        FakeViewShareDetails view;
        FakeModelShareDetails model;
        model.share = ShareObject(QStringLiteral("m4"), QStringLiteral("AGIF001"),
                                   QStringLiteral("LU0000000001"),
                                   QStringLiteral("AGIF-Allianz Glo.Eq.Insights"), ShareType::Fond);

        ShareValues& v = model.values;
        v.volume        = 168.50796;
        v.curPrice      = 225.327;
        v.curValue      = 37969.39;
        v.saleProfitLoss = 152.82;
        v.marketValue    = 38122.21;

        PresenterShareDetails presenter(view, model, QStringLiteral("m4"), /*marketValueMode=*/true);
        QVERIFY(presenter.loadAndDisplay());

        const QLocale locale;

        const CalculationRow* div = FakeViewShareDetails::findRow(view.aktuelleRows, QStringLiteral("Dividenden:"));
        QVERIFY(div);
        QCOMPARE(div->value, QStringLiteral("-"));
        QCOMPARE(div->color, QColor(Qt::gray));

        const CalculationRow* saleProfit = FakeViewShareDetails::findRow(
            view.aktuelleRows, QStringLiteral("Gewinn / Verlust (Verkäufe):"));
        QVERIFY(saleProfit);
        QCOMPARE(saleProfit->value, locale.toString(152.82, 'f', 2) + QStringLiteral(" €"));

        const CalculationRow* sum = FakeViewShareDetails::findRow(view.aktuelleRows, QStringLiteral("Summe:"));
        QVERIFY(sum);
        QCOMPARE(sum->value, locale.toString(38122.21, 'f', 2) + QStringLiteral(" €"));
    }

    void test_loadAndDisplay_marketMode_vortagBox_unaffectedByMode()
    {
        // Vortag-Box is mode-independent (no brokerage involved at all) —
        // same field mapping in both modes.
        FakeViewShareDetails view;
        FakeModelShareDetails model;
        model.share = ShareObject(QStringLiteral("m5"), QStringLiteral("AGIF001"),
                                   QStringLiteral("LU0000000001"),
                                   QStringLiteral("AGIF-Allianz Glo.Eq.Insights"), ShareType::Fond);

        ShareValues& v = model.values;
        v.volume       = 168.50796;
        v.curPrice     = 225.327;
        v.prevDayPrice = 225.009;
        v.prevDayDiff  = 0.318;
        v.prevDayPct   = 0.318 / 225.009 * 100.0;

        PresenterShareDetails presenter(view, model, QStringLiteral("m5"), /*marketValueMode=*/true);
        QVERIFY(presenter.loadAndDisplay());

        const QLocale locale;

        // 168.50796 * 0.318 = 53.5945... -> 53.59 (matches the screenshot exactly)
        const CalculationRow* pl = FakeViewShareDetails::findRow(view.vortagRows, QStringLiteral("Gewinn / Verlust:"));
        QVERIFY(pl);
        QCOMPARE(pl->value, locale.toString(53.59, 'f', 2) + QStringLiteral(" €"));
    }
};

QTEST_MAIN(TestShareDetailsForm)
#include "tst_sharedetailsform.moc"

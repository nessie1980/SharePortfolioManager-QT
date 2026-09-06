// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// Testet ausschliesslich PresenterShareDetails ueber ein Fake-View/Fake-Model-
// Paar (analog zum FakeNetworkAccessManager-Muster bei tst_parser.cpp) - es
// wird bewusst keine echte Datenbank, kein QWidget und kein ShareCalculator
// instanziiert. Die Zuordnung ShareValues-Feld -> Box-Zeile ist gegen
// TabControl.cs (C#-Referenz) sowie ShareCalculator.h abgeglichen
// (09.07.2026) - siehe ARCHITECTURE.md "ShareDetailsForm-Details".
//
// Erweitert 13.07.2026 um die Gewinne/Verluste-, Dividenden- und Kosten-Tabs
// (zunächst nur Depotwert-Modus) - FakeViewShareDetails/FakeModelShareDetails
// decken die drei neuen Interface-Methoden ab, analog zum bestehenden Muster.
// Gewinne/Verluste-Tab am 14.07.2026 auf den Marktwert-Modus erweitert
// (brokeragefreie Werte, siehe ViewShareDetails::populateGewinneVerluste());
// Dividenden/Kosten bleiben Depotwert-only.

#include <QtTest>
#include <QDateTime>

#include "../../app/forms/ShareDetailsForm/PresenterShareDetails.h"
#include "../../app/models/ShareSplitObject.h"

// ── Fake View ──────────────────────────────────────────────────────────────

class FakeViewShareDetails : public IViewShareDetails
{
public:
    QString headerName;
    QString statusLine;
    QString websiteUpdateLine;
    QString updateWarning;
    QString boxesTabTitle;

    CalculationRows gesamtRows;
    CalculationRows vortagRows;
    CalculationRows aktuelleRows;

    // Gewinne/Verluste- (beide Modi), Dividenden-, Kosten-Tabs (Depotwert-
    // only) — "*Called" getrennt von den Listen selbst, damit Tests auch den
    // Fall "gar nicht aufgerufen" (Dividenden/Kosten im Marktwert-Modus) von
    // "mit leerer Liste aufgerufen" unterscheiden können.
    QList<SaleObject>      saleRows;
    bool                    gewinneVerlusteCalled = false;
    QList<DividendObject>  dividendRows;
    bool                    dividendenCalled = false;
    QList<BrokerageObject> brokerageRows;
    bool                    kostenCalled = false;

    // Phase 3c (11.08.2026): zuletzt übergebene Splits je Tab.
    QList<ShareSplitObject> gewinneVerlusteSplits;
    QList<ShareSplitObject> dividendenSplits;

    QString errorMessage;
    bool    closed = false;

    void setHeaderName(const QString& name) override { headerName = name; }
    void setStatusLine(const QString& statusText) override { statusLine = statusText; }
    void setWebsiteUpdateLine(const QString& statusText) override { websiteUpdateLine = statusText; }
    void setUpdateWarning(const QString& text) override { updateWarning = text; }
    void setBoxesTabTitle(const QString& title) override { boxesTabTitle = title; }

    void populateGesamtBox(const CalculationRows& rows) override { gesamtRows = rows; }
    void populateVortagBox(const CalculationRows& rows) override { vortagRows = rows; }
    void populateAktuelleBox(const CalculationRows& rows) override { aktuelleRows = rows; }

    void populateGewinneVerluste(const QList<SaleObject>&       sales,
                                 const QList<ShareSplitObject>& splits) override
    {
        saleRows = sales;
        // Phase 3c (11.08.2026): die Splits kommen als Parameter herein.
        gewinneVerlusteSplits = splits;
        gewinneVerlusteCalled = true;
    }
    void populateDividenden(const QList<DividendObject>&   dividends,
                            const QList<ShareSplitObject>& splits) override
    {
        dividendRows = dividends;
        dividendenSplits = splits;
        dividendenCalled = true;
    }
    void populateKosten(const QList<BrokerageObject>& brokerages) override
    {
        brokerageRows = brokerages;
        kostenCalled = true;
    }

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

    QList<SaleObject>      sales;
    QList<DividendObject>  dividends;
    QList<BrokerageObject> brokerages;
    QList<ShareSplitObject> splits;   // Phase 3c (11.08.2026)
    QDate                   latestDailyValueDateResult; // default invalid -> "keine Tageswerte"

    ShareObject loadShare(const QString&) const override { return share; }

    ShareValues computeShareValues(const QString&, double, double) const override
    {
        return values;
    }

    QList<SaleObject> loadSales(const QString&) const override { return sales; }
    QList<DividendObject> loadDividends(const QString&) const override { return dividends; }
    QList<BrokerageObject> loadBrokerages(const QString&) const override { return brokerages; }
    QList<ShareSplitObject> loadSplits(const QString&) const override { return splits; }

    QDate latestDailyValueDate(const QString&) const override { return latestDailyValueDateResult; }
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
        QVERIFY(!view.gewinneVerlusteCalled);
        QVERIFY(!view.dividendenCalled);
        QVERIFY(!view.kostenCalled);
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
        // Kurswert-Entw. ist die Differenz zweier Kurse und wird seit
        // 05.09.2026 vierstellig angezeigt (ValueFormatter::formatPrice) —
        // die Box ist eine Gleichung, in der dieser Wert als Faktor auftaucht.
        QCOMPARE(diff->value, locale.toString(41.90, 'f', 4) + QStringLiteral(" €"));
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

    // ── Gewinne/Verluste-, Dividenden-, Kosten-Tabs (13.07.2026, Gewinne/
    // Verluste auf beide Modi erweitert 14.07.2026) ─────────────────────────

    void test_loadAndDisplay_marketMode_populatesOnlyGewinneVerluste()
    {
        // Marktwert-Modus: ViewShareDetails legt seit 14.07.2026 den
        // Gewinne/Verluste-Tab auch hier an (siehe ViewShareDetails::
        // setupUi()) — Dividenden-/Kosten-Tab bleiben Depotwert-only, da
        // beides laut C#-Referenz reine Depotwert-Konzepte sind (siehe
        // ARCHITECTURE.md, "Marktwert- vs. Depotwert-Modus").
        FakeViewShareDetails view;
        FakeModelShareDetails model;
        model.share = ShareObject(QStringLiteral("mv1"), QStringLiteral("AGIF001"),
                                   QStringLiteral("LU0000000001"),
                                   QStringLiteral("AGIF-Allianz Glo.Eq.Insights"), ShareType::Fond);
        model.sales      = { SaleObject(), SaleObject() };
        model.dividends  = { DividendObject() };
        model.brokerages = { BrokerageObject(), BrokerageObject(), BrokerageObject() };

        PresenterShareDetails presenter(view, model, QStringLiteral("mv1"), /*marketValueMode=*/true);
        QVERIFY(presenter.loadAndDisplay());

        QVERIFY(view.gewinneVerlusteCalled);
        QVERIFY(!view.dividendenCalled);
        QVERIFY(!view.kostenCalled);
        QCOMPARE(view.saleRows.size(), 2);
        QVERIFY(view.dividendRows.isEmpty());
        QVERIFY(view.brokerageRows.isEmpty());
    }

    // ── Split-Übergabe an die Übersichts-Tabs (Phase 3c, 11.08.2026) ──────

    void test_loadAndDisplay_depotwertMode_passesSplitsToGewinneVerlusteAndDividenden()
    {
        // Die Splits gehen als Parameter an die View, nicht über einen
        // eigenen Setter — sonst entstünde eine unsichtbare Reihenfolge-
        // Abhängigkeit zwischen zwei View-Aufrufen.
        FakeViewShareDetails  view;
        FakeModelShareDetails model;
        model.share = ShareObject(QStringLiteral("sp1"), QStringLiteral("WKN020"),
                                  QStringLiteral("ISIN0000020"), QStringLiteral("Split AG"));
        model.sales     = { SaleObject() };
        model.dividends = { DividendObject() };
        model.splits    = { ShareSplitObject(QStringLiteral("split-1"),
                                             QStringLiteral("sp1"),
                                             QDate(2022, 7, 18), 20.0, 1.0) };

        PresenterShareDetails presenter(view, model, QStringLiteral("sp1"));
        QVERIFY(presenter.loadAndDisplay());

        QCOMPARE(view.gewinneVerlusteSplits.size(), 1);
        QCOMPARE(view.gewinneVerlusteSplits.first().ratioNew(), 20.0);
        QCOMPARE(view.dividendenSplits.size(), 1);
        QCOMPARE(view.dividendenSplits.first().ratioNew(), 20.0);
    }

    void test_loadAndDisplay_marketValueMode_passesSplitsToGewinneVerluste()
    {
        // Der Gewinne/Verluste-Tab existiert in beiden Modi und bekommt in
        // beiden dieselben Splits — die Stückzahlen sind identisch, nur die
        // Geldbeträge unterscheiden sich (brokeragefrei im Marktwert-Modus,
        // Nessies Vorgabe 11.08.2026).
        FakeViewShareDetails  view;
        FakeModelShareDetails model;
        model.share = ShareObject(QStringLiteral("sp2"), QStringLiteral("WKN021"),
                                  QStringLiteral("ISIN0000021"), QStringLiteral("Split AG"));
        model.sales  = { SaleObject() };
        model.splits = { ShareSplitObject(QStringLiteral("split-1"),
                                          QStringLiteral("sp2"),
                                          QDate(2022, 7, 18), 20.0, 1.0) };

        PresenterShareDetails presenter(view, model, QStringLiteral("sp2"),
                                        /*marketValueMode=*/true);
        QVERIFY(presenter.loadAndDisplay());

        QVERIFY(view.gewinneVerlusteCalled);
        QCOMPARE(view.gewinneVerlusteSplits.size(), 1);
        // Dividenden-Tab existiert im Marktwert-Modus nicht.
        QVERIFY(!view.dividendenCalled);
        QVERIFY(view.dividendenSplits.isEmpty());
    }

    void test_loadAndDisplay_withoutSplits_passesEmptySplitList()
    {
        FakeViewShareDetails  view;
        FakeModelShareDetails model;
        model.share = ShareObject(QStringLiteral("sp3"), QStringLiteral("WKN022"),
                                  QStringLiteral("ISIN0000022"), QStringLiteral("Ohne AG"));
        model.sales     = { SaleObject() };
        model.dividends = { DividendObject() };

        PresenterShareDetails presenter(view, model, QStringLiteral("sp3"));
        QVERIFY(presenter.loadAndDisplay());

        QVERIFY(view.gewinneVerlusteCalled);
        QVERIFY(view.gewinneVerlusteSplits.isEmpty());
        QVERIFY(view.dividendenSplits.isEmpty());
    }

    void test_loadAndDisplay_depotwertMode_populatesGewinneVerlusteDividendenKosten()
    {
        // Depotwert-Modus: alle drei neuen Tabs werden mit genau den Listen
        // befüllt, die das Model liefert — reines Durchreichen, keine eigene
        // Presenter-Logik (siehe PresenterShareDetails::populateGewinneVerluste()
        // etc.).
        FakeViewShareDetails view;
        FakeModelShareDetails model;
        model.share = ShareObject(QStringLiteral("dv1"), QStringLiteral("WKN008"),
                                   QStringLiteral("ISIN0000008"), QStringLiteral("Acht AG"));
        model.sales      = { SaleObject(), SaleObject() };
        model.dividends  = { DividendObject() };
        model.brokerages = { BrokerageObject(), BrokerageObject(), BrokerageObject() };

        PresenterShareDetails presenter(view, model, QStringLiteral("dv1")); // default: Depotwert-Modus
        QVERIFY(presenter.loadAndDisplay());

        QVERIFY(view.gewinneVerlusteCalled);
        QVERIFY(view.dividendenCalled);
        QVERIFY(view.kostenCalled);
        QCOMPARE(view.saleRows.size(), 2);
        QCOMPARE(view.dividendRows.size(), 1);
        QCOMPARE(view.brokerageRows.size(), 3);
    }

    // ── "Aktie sollte aktualisiert werden!"-Warnzeile (ergänzt 30.07.2026) ──
    //
    // previousBusinessDay()/needsUpdateWarning() sind public static, damit
    // diese Tests mit festen Datums-/Enum-Kombinationen arbeiten können,
    // statt von QDate::currentDate() abzuhängen (Nessies Vorgabe: nur einmal
    // beim Öffnen prüfen, siehe ShareDetailsForm.cs, ShareDetailsForm_Shown()).
    // Montag 03.08.2026 / Dienstag 04.08.2026 / ... als feste Referenzdaten.

    void test_previousBusinessDay_monday_returnsPreviousFriday()
    {
        // Montag überspringt das komplette Wochenende -> Freitag davor.
        QCOMPARE(PresenterShareDetails::previousBusinessDay(QDate(2026, 8, 3)), QDate(2026, 7, 31));
    }

    void test_previousBusinessDay_tuesday_returnsMonday()
    {
        QCOMPARE(PresenterShareDetails::previousBusinessDay(QDate(2026, 8, 4)), QDate(2026, 8, 3));
    }

    void test_previousBusinessDay_wednesday_returnsTuesday()
    {
        QCOMPARE(PresenterShareDetails::previousBusinessDay(QDate(2026, 8, 5)), QDate(2026, 8, 4));
    }

    void test_previousBusinessDay_thursday_returnsWednesday()
    {
        QCOMPARE(PresenterShareDetails::previousBusinessDay(QDate(2026, 8, 6)), QDate(2026, 8, 5));
    }

    void test_previousBusinessDay_friday_returnsThursday()
    {
        QCOMPARE(PresenterShareDetails::previousBusinessDay(QDate(2026, 8, 7)), QDate(2026, 8, 6));
    }

    void test_previousBusinessDay_saturday_returnsFriday()
    {
        QCOMPARE(PresenterShareDetails::previousBusinessDay(QDate(2026, 8, 8)), QDate(2026, 8, 7));
    }

    void test_previousBusinessDay_sunday_returnsFriday()
    {
        // Zwei Tage zurück (Samstag übersprungen) -> derselbe Freitag wie beim Samstag-Fall.
        QCOMPARE(PresenterShareDetails::previousBusinessDay(QDate(2026, 8, 9)), QDate(2026, 8, 7));
    }

    void test_needsUpdateWarning_marketPriceOnly_neverWarns()
    {
        // Bewusste Einstellung (keine Tageswerte werden abgerufen) -> nie
        // eine Warnung, unabhängig vom Datenstand.
        QVERIFY(!PresenterShareDetails::needsUpdateWarning(
            ShareUpdateType::MarketPrice, QDate(), QDate(2026, 8, 3)));
        QVERIFY(!PresenterShareDetails::needsUpdateWarning(
            ShareUpdateType::MarketPrice, QDate(2020, 1, 1), QDate(2026, 8, 3)));
    }

    void test_needsUpdateWarning_none_neverWarns()
    {
        QVERIFY(!PresenterShareDetails::needsUpdateWarning(
            ShareUpdateType::None, QDate(), QDate(2026, 8, 3)));
    }

    void test_needsUpdateWarning_dailyValues_noData_warns()
    {
        QVERIFY(PresenterShareDetails::needsUpdateWarning(
            ShareUpdateType::DailyValues, QDate(), QDate(2026, 8, 3)));
    }

    void test_needsUpdateWarning_both_noData_warns()
    {
        QVERIFY(PresenterShareDetails::needsUpdateWarning(
            ShareUpdateType::Both, QDate(), QDate(2026, 8, 3)));
    }

    void test_needsUpdateWarning_dataExactlyOnPreviousBusinessDay_noWarning()
    {
        // Heute = Dienstag 04.08.2026 -> letzter Werktag = Montag 03.08.2026.
        // Tageswert genau vom letzten Werktag -> KEINE Warnung (Grenzfall).
        QVERIFY(!PresenterShareDetails::needsUpdateWarning(
            ShareUpdateType::DailyValues, QDate(2026, 8, 3), QDate(2026, 8, 4)));
    }

    void test_needsUpdateWarning_dataOneBusinessDayOlderThanThreshold_warns()
    {
        // Wie oben, aber Tageswert vom Freitag 31.07. (einen Werktag zu alt) -> Warnung.
        QVERIFY(PresenterShareDetails::needsUpdateWarning(
            ShareUpdateType::DailyValues, QDate(2026, 7, 31), QDate(2026, 8, 4)));
    }

    void test_needsUpdateWarning_dataFromToday_noWarning()
    {
        QVERIFY(!PresenterShareDetails::needsUpdateWarning(
            ShareUpdateType::Both, QDate(2026, 8, 4), QDate(2026, 8, 4)));
    }

    void test_loadAndDisplay_dailyValuesUpdateType_noData_setsUpdateWarningText()
    {
        FakeViewShareDetails view;
        FakeModelShareDetails model;
        model.share = ShareObject(QStringLiteral("uw1"), QStringLiteral("WKN009"),
                                   QStringLiteral("ISIN0000009"), QStringLiteral("Neun AG"));
        model.share.setUpdateType(ShareUpdateType::DailyValues);
        // model.latestDailyValueDateResult bleibt ungültig -> keine Tageswerte vorhanden

        PresenterShareDetails presenter(view, model, QStringLiteral("uw1"));
        QVERIFY(presenter.loadAndDisplay());

        QCOMPARE(view.updateWarning,
                 QStringLiteral("Aktie sollte aktualisiert werden! Daten sind evtl. nicht auf dem aktuellen Stand."));
    }

    void test_loadAndDisplay_marketPriceOnlyUpdateType_noWarningRegardlessOfData()
    {
        FakeViewShareDetails view;
        FakeModelShareDetails model;
        model.share = ShareObject(QStringLiteral("uw2"), QStringLiteral("WKN010"),
                                   QStringLiteral("ISIN0000010"), QStringLiteral("Zehn AG"));
        model.share.setUpdateType(ShareUpdateType::MarketPrice);
        // Keine Tageswerte vorhanden, aber MarketPrice-only -> trotzdem keine Warnung.

        PresenterShareDetails presenter(view, model, QStringLiteral("uw2"));
        QVERIFY(presenter.loadAndDisplay());

        QVERIFY(view.updateWarning.isEmpty());
    }

    void test_loadAndDisplay_dailyValuesUpdateType_freshData_noWarning()
    {
        FakeViewShareDetails view;
        FakeModelShareDetails model;
        model.share = ShareObject(QStringLiteral("uw3"), QStringLiteral("WKN011"),
                                   QStringLiteral("ISIN0000011"), QStringLiteral("Elf AG"));
        model.share.setUpdateType(ShareUpdateType::Both);
        model.latestDailyValueDateResult = QDate::currentDate(); // stets "aktuell genug", unabhängig vom Testdatum

        PresenterShareDetails presenter(view, model, QStringLiteral("uw3"));
        QVERIFY(presenter.loadAndDisplay());

        QVERIFY(view.updateWarning.isEmpty());
    }
};

QTEST_MAIN(TestShareDetailsForm)
#include "tst_sharedetailsform.moc"

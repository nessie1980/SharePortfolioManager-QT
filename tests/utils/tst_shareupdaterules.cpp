// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// tst_shareupdaterules.cpp — Unit tests for ShareUpdateRules.
//
// Rein funktionale Tests: keine Datenbank, keine Widgets, kein QApplication.
// Genau darum liegt die Regel in einem eigenen Modul und nicht in einem der
// beiden Presenter (siehe ARCHITECTURE.md, "Tageswert-Historie bei Bestand > 0
// erzwingen").

#include <QtTest>

#include "../../app/utils/ShareUpdateRules.h"

using namespace ShareUpdateRules;

class TestShareUpdateRules : public QObject
{
    Q_OBJECT

private:
    /** Kurzform zum Bauen eines ShareState — nur die zwei relevanten Felder. */
    static ShareState state(ShareUpdateType type, double volume,
                            const QString& name = QStringLiteral("Test AG"))
    {
        ShareState s;
        s.guid          = QStringLiteral("guid-") + name;
        s.wkn           = QStringLiteral("WKN001");
        s.name          = name;
        s.updateType    = type;
        s.currentVolume = volume;
        return s;
    }

private slots:

    // ── requiresDailyValues ───────────────────────────────────────────────

    void test_requiresDailyValues_zeroVolume_false()
    {
        QVERIFY(!requiresDailyValues(0.0));
    }

    void test_requiresDailyValues_positiveVolume_true()
    {
        QVERIFY(requiresDailyValues(1.0));
        QVERIFY(requiresDailyValues(0.0001));
    }

    void test_requiresDailyValues_floatingPointNoise_false()
    {
        // Eine vollständig verkaufte Position kann durch die Summierung über
        // (volume - volumeSold) ein paar ULP neben 0 landen. Das darf nicht
        // als Bestand gewertet werden — gleiche Schwelle wie
        // ModelSaleEdit::loadAvailableBuys().
        QVERIFY(!requiresDailyValues(1e-12));
        QVERIFY(!requiresDailyValues(kVolumeEpsilon));
    }

    void test_requiresDailyValues_justAboveEpsilon_true()
    {
        QVERIFY(requiresDailyValues(kVolumeEpsilon * 10.0));
    }

    void test_requiresDailyValues_negativeVolume_false()
    {
        // Sollte in der Praxis nicht vorkommen (mehr verkauft als gekauft),
        // darf aber keinesfalls als Bestand durchgehen.
        QVERIFY(!requiresDailyValues(-5.0));
    }

    // ── updateTypeIncludesDailyValues ─────────────────────────────────────

    void test_updateTypeIncludesDailyValues_allFourTypes()
    {
        QVERIFY( updateTypeIncludesDailyValues(ShareUpdateType::Both));
        QVERIFY( updateTypeIncludesDailyValues(ShareUpdateType::DailyValues));
        QVERIFY(!updateTypeIncludesDailyValues(ShareUpdateType::MarketPrice));
        QVERIFY(!updateTypeIncludesDailyValues(ShareUpdateType::None));
    }

    // ── isUpdateTypeAllowed ───────────────────────────────────────────────

    void test_isUpdateTypeAllowed_withHolding_onlyDailyVariants()
    {
        constexpr double held = 12.5;
        QVERIFY( isUpdateTypeAllowed(ShareUpdateType::Both,        held));
        QVERIFY( isUpdateTypeAllowed(ShareUpdateType::DailyValues, held));
        QVERIFY(!isUpdateTypeAllowed(ShareUpdateType::MarketPrice, held));
        QVERIFY(!isUpdateTypeAllowed(ShareUpdateType::None,        held));
    }

    void test_isUpdateTypeAllowed_withoutHolding_everythingAllowed()
    {
        // Ohne Bestand gibt es nichts mehr zu bewerten — die fehlende
        // Historie kostet dann nichts, jede Einstellung ist zulässig.
        QVERIFY(isUpdateTypeAllowed(ShareUpdateType::Both,        0.0));
        QVERIFY(isUpdateTypeAllowed(ShareUpdateType::DailyValues, 0.0));
        QVERIFY(isUpdateTypeAllowed(ShareUpdateType::MarketPrice, 0.0));
        QVERIFY(isUpdateTypeAllowed(ShareUpdateType::None,        0.0));
    }

    void test_isUpdateTypeAllowed_boundary_epsilonCountsAsNoHolding()
    {
        QVERIFY( isUpdateTypeAllowed(ShareUpdateType::None, kVolumeEpsilon));
        QVERIFY(!isUpdateTypeAllowed(ShareUpdateType::None, kVolumeEpsilon * 10.0));
    }

    // ── sharesNeedingDailyValues ──────────────────────────────────────────

    void test_sharesNeedingDailyValues_emptyList_returnsEmpty()
    {
        QVERIFY(sharesNeedingDailyValues({}).isEmpty());
    }

    void test_sharesNeedingDailyValues_allCompliant_returnsEmpty()
    {
        const QList<ShareState> shares = {
            state(ShareUpdateType::Both,        10.0),
            state(ShareUpdateType::DailyValues,  5.0),
            state(ShareUpdateType::None,         0.0),   // verkauft -> egal
            state(ShareUpdateType::MarketPrice,  0.0),   // verkauft -> egal
        };
        QVERIFY(sharesNeedingDailyValues(shares).isEmpty());
    }

    void test_sharesNeedingDailyValues_mixed_returnsOnlyOffenders()
    {
        const QList<ShareState> shares = {
            state(ShareUpdateType::Both,        10.0, QStringLiteral("Eins AG")),
            state(ShareUpdateType::None,         3.0, QStringLiteral("Zwei AG")),
            state(ShareUpdateType::MarketPrice,  0.0, QStringLiteral("Vier AG")),
            state(ShareUpdateType::MarketPrice,  7.5, QStringLiteral("Fuenf AG")),
        };

        const QList<ShareState> offenders = sharesNeedingDailyValues(shares);

        QCOMPARE(offenders.size(), 2);
        // Eingangsreihenfolge bleibt erhalten — die Meldung listet die Aktien
        // in derselben Reihenfolge wie das Grid.
        QCOMPARE(offenders.at(0).name, QStringLiteral("Zwei AG"));
        QCOMPARE(offenders.at(1).name, QStringLiteral("Fuenf AG"));
    }

    void test_sharesNeedingDailyValues_keepsAllFields()
    {
        ShareState s = state(ShareUpdateType::None, 42.0, QStringLiteral("Sechs AG"));
        s.guid = QStringLiteral("abc-123");
        s.wkn  = QStringLiteral("WKN999");

        const QList<ShareState> offenders = sharesNeedingDailyValues({ s });

        QCOMPARE(offenders.size(), 1);
        QCOMPARE(offenders.at(0).guid, QStringLiteral("abc-123"));
        QCOMPARE(offenders.at(0).wkn,  QStringLiteral("WKN999"));
        QCOMPARE(offenders.at(0).name, QStringLiteral("Sechs AG"));
        QCOMPARE(offenders.at(0).updateType, ShareUpdateType::None);
        QCOMPARE(offenders.at(0).currentVolume, 42.0);
    }
};

QTEST_APPLESS_MAIN(TestShareUpdateRules)
#include "tst_shareupdaterules.moc"

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// Unit tests for ShareCalculator::compute() and roundAway().
//
// compute() reads buys, sales, brokerage and dividends fresh from the
// repositories, so these tests populate a real in-memory SQLite database
// (same pattern as the repository tests) and assert the resulting
// ShareValues against reference values verified against the C# application.
//
// Focus is the Marktwert tab and the fixes made while porting it:
//   - realized P/L in the "Komplette Entwicklung" includes brokerage,
//   - that realized P/L is derived from aggregates (all buys minus held
//     buys), NOT from SaleBuyDetail records — so an empty / unpopulated
//     detail list must NOT overstate the gain,
//   - the column identity  Kpl. Marktwert = Kpl. Einzahlung + Kpl. Entwicklung,
//   - cent-exact half-away-from-zero rounding,
//   - edge cases volume = 0 (fully sold) and no sales,
//   - (10.07.2026) Marktwert figures exclude Rabatt (reduction) as well as
//     brokerage — Rabatt is a discount on brokerage costs, so it belongs
//     together with "Kosten" and was previously still subtracted even
//     though brokerage itself was already excluded.
//   - (20.08.2026) freestanding brokerage/cost entries (no buyGuid/saleGuid,
//     from the standalone Kosten-Verwaltung UI) fold into completePurchase
//     — and therefore into "Komplette Entwicklung" on BOTH tabs — but never
//     into completePurchaseMarket (Footer-Lücke bug, 05.08.2026).

#include <QtTest>
#include <QSqlDatabase>
#include <QUuid>

#include "../../app/core/Database.h"
#include "../../app/models/BuyObject.h"
#include "../../app/models/SaleObject.h"
#include "../../app/models/BrokerageObject.h"
#include "../../app/models/ShareSplitObject.h"
#include "../../app/repositories/BuyRepository.h"
#include "../../app/repositories/SaleRepository.h"
#include "../../app/repositories/BrokerageRepository.h"
#include "../../app/repositories/ShareSplitRepository.h"
#include "../../app/utils/ShareCalculator.h"

// Compare two monetary doubles to within a tiny tolerance. Avoids the
// qFuzzyCompare-with-zero pitfall and gives a readable failure message.
#define CMP_MONEY(actual, expected)                                            \
    QVERIFY2(qAbs((actual) - (expected)) < 1e-6,                               \
             qPrintable(QStringLiteral("got %1, expected %2")                  \
                            .arg(actual, 0, 'f', 4).arg(expected, 0, 'f', 4)))

class TestShareCalculator : public QObject
{
    Q_OBJECT

private:
    const QString k_shareGuid = QStringLiteral("test-share-calc-0001");

    QString newGuid() const { return QUuid::createUuid().toString(QUuid::WithoutBraces); }

    // Insert a buy and (if non-zero) its linked brokerage record.
    // Returns the buy's guid so it can be referenced by SaleBuyDetails.
    // dateTime defaults to the pre-existing hardcoded fixture date so all
    // tests written before split-awareness (07.08.2026) remain unaffected —
    // only the new split tests below pass an explicit, earlier/later date.
    QString addBuy(double volume, double volumeSold, double price,
                   double brokerage, double reduction,
                   const QString& dateTime = QStringLiteral("2024-01-01T10:00:00"))
    {
        BuyRepository       buyRepo;
        BrokerageRepository brokRepo;

        const QString buyGuid = newGuid();
        buyRepo.insert(BuyObject(buyGuid, k_shareGuid, QString(), newGuid(),
                                 dateTime,
                                 volume, volumeSold, price));

        if (brokerage != 0.0 || reduction != 0.0) {
            // brokerage() == provision + brokerFee + traderFee
            brokRepo.insert(BrokerageObject(newGuid(), k_shareGuid, buyGuid, QString(),
                                            dateTime,
                                            brokerage, 0.0, 0.0, reduction));
        }
        return buyGuid;
    }

    // Insert a sale with its linked brokerage record (the sale's brokerage and
    // reduction are loaded via JOIN on brokerage_guid) and optional details.
    // dateTime defaults as in addBuy() above.
    void addSale(double volume, double salePrice, double brokerage,
                 double reduction, double tax, const QList<SaleBuyDetail>& details,
                 const QString& dateTime = QStringLiteral("2024-06-01T10:00:00"))
    {
        SaleRepository      saleRepo;
        BrokerageRepository brokRepo;

        const QString saleGuid = newGuid();
        const QString brokGuid = newGuid();

        // Insert order matters: brokerage.sale_guid has an SQL FK to
        // sales(guid) (sales.brokerage_guid, by contrast, has none — see
        // Database.cpp), so the sale row must exist BEFORE the brokerage row
        // that references it, exactly as ModelSaleEdit::addSale() does it
        // (1. insert sale, 2. insert brokerage with sale.guid(), 2b. update
        // sales.brokerage_guid). Doing it the other way round, as this
        // helper did until 20.08.2026, throws "FOREIGN KEY constraint
        // failed" on the brokerage insert.
        saleRepo.insert(SaleObject(saleGuid, k_shareGuid, QString(), newGuid(),
                                   dateTime,
                                   volume, salePrice, details,
                                   tax, 0.0, 0.0,        // taxAtSource / capitalGains / solidarity
                                   brokGuid,             // brokerageGuid -> JOIN source
                                   0.0, 0.0, 0.0,        // provision/fees (ignored on insert)
                                   0.0));                // reduction (ignored on insert; from brokerage)

        // sale_guid IS set here, mirroring production (ModelSaleEdit::
        // addSale()): SaleRepository's own JOIN for loading a sale's
        // brokerage always goes forward via sales.brokerage_guid, never via
        // brokerage.sale_guid — but brokerage.sale_guid is still populated
        // on insert and is exactly what ShareCalculator::compute() now uses
        // (20.08.2026, Footer-Lücke-Fix) to tell a sale-/buy-linked
        // brokerage entry apart from a freestanding one via findByShare().
        // Leaving it empty here (as before the fix) made every sale-linked
        // entry look freestanding and silently double-counted its brokerage.
        brokRepo.insert(BrokerageObject(brokGuid, k_shareGuid, QString(), saleGuid,
                                        dateTime,
                                        brokerage, 0.0, 0.0, reduction));
    }

    // Insert a freestanding brokerage/cost entry (neither buyGuid nor
    // saleGuid set — created via the standalone Kosten-Verwaltung UI).
    // Used to cover the Footer-Lücke fix (Bug, 05.08.2026, siehe
    // ARCHITECTURE.md "Footer-Lücke bei freistehenden Kosteneinträgen").
    QString addFreestandingCost(double brokerage, double reduction,
                                const QString& dateTime = QStringLiteral("2024-02-01T10:00:00"))
    {
        BrokerageRepository brokRepo;
        const QString guid = newGuid();
        brokRepo.insert(BrokerageObject(guid, k_shareGuid, QString(), QString(),
                                        dateTime, brokerage, 0.0, 0.0, reduction));
        return guid;
    }

    // Insert a split for the test share (Phase 2 der Aktiensplit-Behandlung,
    // 07.08.2026, siehe ARCHITECTURE.md "Offene Punkte").
    void addSplit(const QDate& date, double ratioNew, double ratioOld,
                  bool pricesAdjusted = false)
    {
        ShareSplitRepository splitRepo;
        splitRepo.insert(ShareSplitObject(newGuid(), k_shareGuid, date,
                                          ratioNew, ratioOld, pricesAdjusted));
    }

    // Insert a dividend. payout = rate * volume (no FX), net = payout - tax.
    // Other tax columns and FX use their schema defaults (0 / 1). Enough to
    // exercise the net-dividend contribution to the Depotwert complete columns.
    void addDividend(double rate, double volume, double tax)
    {
        Database::instance().execute(
            QStringLiteral("INSERT INTO dividends "
                           "(guid, share_guid, datetime, rate, volume, tax_at_source) "
                           "VALUES ('%1', '%2', '2024-03-01T10:00:00', %3, %4, %5)")
                .arg(newGuid())
                .arg(k_shareGuid)
                .arg(rate,   0, 'f', 6)
                .arg(volume, 0, 'f', 6)
                .arg(tax,    0, 'f', 6));
    }

private slots:

    void initTestCase()
    {
        QVERIFY(Database::instance().open(QStringLiteral(":memory:")));
        Database::instance().execute(
            QStringLiteral("INSERT INTO shares (guid, wkn, name) "
                           "VALUES ('%1', 'TEST01', 'Test Share')")
                .arg(k_shareGuid));
    }

    void cleanupTestCase()
    {
        Database::instance().close();
    }

    void init()
    {
        // Delete in FK-safe order: child / referencing tables first, parents
        // last. Per Database.cpp's CREATE TABLE statements, the real FKs are
        // sale_buy_details.sale_guid/buy_guid -> sales/buys.guid and
        // brokerage.buy_guid/sale_guid -> buys/sales.guid (sales.brokerage_guid
        // is a plain, constraint-free column — NOT the FK direction, despite
        // the misleading comment this replaces). brokerage can reference BOTH
        // sales and buys, so it must be cleared before either of them.
        //
        // Corrected 20.08.2026 (Footer-Lücke-Fix, see ARCHITECTURE.md):
        // before that fix, addSale()'s linked brokerage row always had an
        // empty sale_guid, so "DELETE FROM sales" before "DELETE FROM
        // brokerage" never actually violated the FK (NULL never matches in a
        // foreign key check) — the bug in this ordering was latent. Now that
        // addSale() sets sale_guid correctly, the old order breaks with
        // "FOREIGN KEY constraint failed" on every test after the first one
        // that inserts a sale with linked brokerage.
        Database::instance().execute(QStringLiteral("DELETE FROM sale_buy_details"));
        Database::instance().execute(QStringLiteral("DELETE FROM brokerage"));
        Database::instance().execute(QStringLiteral("DELETE FROM sales"));
        Database::instance().execute(QStringLiteral("DELETE FROM buys"));
        Database::instance().execute(QStringLiteral("DELETE FROM dividends"));
        Database::instance().execute(QStringLiteral("DELETE FROM share_splits"));
    }

    // ── roundAway ─────────────────────────────────────────────────────────
    void test_roundAway_halfAwayFromZero()
    {
        CMP_MONEY(ShareCalculator::roundAway(2.345), 2.35);
        CMP_MONEY(ShareCalculator::roundAway(2.344), 2.34);
        CMP_MONEY(ShareCalculator::roundAway(-2.345), -2.35);
        CMP_MONEY(ShareCalculator::roundAway(2.5, 0), 3.0);
        CMP_MONEY(ShareCalculator::roundAway(-2.5, 0), -3.0);
    }

    // ── Core scenario (one buy partly sold, a second held, one sale) ──────
    // Buy1: 10 @ 100, 4 sold, brokerage 9.90
    // Buy2:  5 @ 120, held,   brokerage 5.00, reduction 2.00
    // Sale:  4 @ 130, brokerage 7.00, tax 3.00, details reference Buy1
    // curPrice 125, prevDay 120
    void test_marktwert_coreScenario()
    {
        const QString b1 = addBuy(10.0, 4.0, 100.0, 9.90, 0.0);
        addBuy(5.0, 0.0, 120.0, 5.00, 2.00);
        addSale(4.0, 130.0, 7.0, 0.0, 3.0,
                { SaleBuyDetail(b1, QStringLiteral("2024-06-01T10:00:00"),
                                4.0, 100.0, 0.0, 0.0) });

        const ShareValues v = ShareCalculator::compute(k_shareGuid, 125.0, 120.0);

        CMP_MONEY(v.volume,                   11.0);
        CMP_MONEY(v.curValue,                 1375.0);
        // purchaseValue/completePurchaseMarket now exclude Rabatt (reduction)
        // as well as brokerage — corrected 10.07.2026 (Buy2's reduction 2.00
        // no longer subtracted; buy1's reduction was already 0, so unaffected
        // there). Values below recomputed accordingly.
        CMP_MONEY(v.purchaseValue,            1200.0);  // held basis, no brokerage, no reduction
        CMP_MONEY(v.profitLoss,               175.0);   // Aktuelle Entwicklung
        CMP_MONEY(v.completePurchaseMarket,   1600.0);  // Kpl. Einzahlung
        // "Rein Kurs" (Nessies Rechenregel vom 25.10.2026): keine Kosten,
        // keine Steuern, keine Dividenden.
        //   salePayoutMarket = round(4 * 130)               =  520.00 (brutto)
        //   Kpl. Marktwert   = curValue + Verkäufe = 1375 + 520 = 1895.00
        //   Kompl. Entw.     = 1895 - 1600                  =  295.00
        //   saleProfitLoss   = 520 - (1600 - 1200)          =  120.00
        // Bis 25.10.2026: 281.04 / 1881.04 / 517.00 (Steuer abgezogen und
        // realisierter G/V mit Kosten eingerechnet).
        CMP_MONEY(v.completeProfitLossMarket, 295.00);
        CMP_MONEY(v.completeCurValueMarket,   1895.00); // Kpl. Marktwert
        CMP_MONEY(v.salePayoutMarket,          520.00);
        CMP_MONEY(v.saleProfitLoss,            120.00);

        // Depotwert basics (with brokerage) — unaffected by the Marktwert fix
        CMP_MONEY(v.purchaseValueFinal,       1208.94);
        CMP_MONEY(v.completePurchase,         1612.90);
        CMP_MONEY(v.completeProfitLoss,       272.10);
    }

    // ── Depotwert tab: …Final fields (with brokerage), per-lot attribution ──
    // Same core fixture. Verifies the exact values the Depotwert tab displays —
    // "Aktuelle Entwicklung" (profitLossFinal / profitLossPctFinal) and the
    // Depotwert complete columns. Buy-brokerage is attributed per held lot:
    //   Buy1 (6/10 held): 600 + round(9.90*0.6)=5.94             = 605.94
    //   Buy2 (5/5 held):  600 + 5.00 - 2.00                      = 603.00
    //   purchaseValueFinal = 1208.94 ; curValue = 1375
    //   profitLossFinal    = 1375 - 1208.94                      = 166.06
    //   completeCurValue   = curValue + salePayoutFinal(510) + 0 = 1885.00
    void test_depotwert_finalFields()
    {
        const QString b1 = addBuy(10.0, 4.0, 100.0, 9.90, 0.0);
        addBuy(5.0, 0.0, 120.0, 5.00, 2.00);
        addSale(4.0, 130.0, 7.0, 0.0, 3.0,
                { SaleBuyDetail(b1, QStringLiteral("2024-06-01T10:00:00"),
                                4.0, 100.0, 0.0, 0.0) });

        const ShareValues v = ShareCalculator::compute(k_shareGuid, 125.0, 120.0);

        // Aktuelle Entwicklung (Depotwert, WITH brokerage)
        CMP_MONEY(v.purchaseValueFinal, 1208.94);
        CMP_MONEY(v.profitLossFinal,    166.06);
        CMP_MONEY(v.profitLossPctFinal, 166.06 / 1208.94 * 100.0);

        // Depotwert complete columns
        CMP_MONEY(v.completePurchase,   1612.90);
        CMP_MONEY(v.completeCurValue,   1885.00);
        CMP_MONEY(v.completeProfitLoss, 272.10);
        CMP_MONEY(v.completeProfitPct,  272.10 / 1612.90 * 100.0);

        // salePayoutFinal / saleProfitLossFinal (ergänzt 09.07.2026, für die
        // "Komplette Depotbewertung"-Box in ShareDetailsForm — "+ Verkäufe" /
        // "+ Gewinn / Verlust (Verkäufe)"-Zeilen):
        //   saleValue           = round(4 * 130)                     = 520.00
        //   salePayoutFinal     = round(520 - brokerage(7) + 0 - tax(3)) = 510.00
        //   soldCostFinal       = completePurchase - purchaseValueFinal
        //                       = 1612.90 - 1208.94                  = 403.96
        //   saleProfitLossFinal = salePayoutFinal - soldCostFinal    = 106.04
        // Cross-check: profitLossFinal + saleProfitLossFinal + totalDividend
        //            = 166.06 + 106.04 + 0                           = 272.10 = completeProfitLoss ✓
        CMP_MONEY(v.salePayoutFinal,     510.00);
        CMP_MONEY(v.saleProfitLossFinal, 106.04);
    }

    // ── Depotwert: brokerage AND reduction prorated per held lot ───────────
    // One buy partly sold (6 of 10 held) carrying both brokerage and a
    // reduction. Both are prorated to the held fraction (0.6) and rounded per
    // lot:  heldBrokerage = round(9.94*0.6) = 5.96,
    //       heldReduction = round(5.51*0.6) = 3.31.
    //   purchaseValueFinal = 600 + 5.96 - 3.31 = 602.65
    // The full-attribution model (600 + 9.94 - 5.51 = 604.43) is explicitly
    // wrong; this pins the per-lot behaviour for both terms.
    void test_depotwert_partialLotBrokerageAndReduction()
    {
        addBuy(10.0, 4.0, 100.0, 9.94, 5.51);
        addSale(4.0, 110.0, 0.0, 0.0, 0.0, {});

        const ShareValues v = ShareCalculator::compute(k_shareGuid, 100.0, 100.0);

        CMP_MONEY(v.purchaseValueFinal, 602.65);  // per-lot, NOT 604.43
        // purchaseValue (Marktwert) now excludes reduction too (corrected
        // 10.07.2026) — held basis is simply heldVolume x price, no
        // brokerage-related adjustment of any kind: 6 * 100 = 600.00
        // (previously 596.69, which still subtracted the prorated reduction).
        CMP_MONEY(v.purchaseValue,      600.00);
        CMP_MONEY(v.profitLossFinal,    -2.65);
        CMP_MONEY(v.profitLossPctFinal, -2.65 / 602.65 * 100.0);
    }

    // ── Depotwert: net dividends contribute to the complete columns ────────
    // One held buy, no sale, a single dividend (2.00 * 6 = 12.00, no tax).
    //   completeCurValue = curValue(1000) + salePayout(0) + dividend(12) = 1012
    // Without the dividend term this would be 1000.00.
    void test_depotwert_dividendInCompleteValue()
    {
        addBuy(10.0, 0.0, 100.0, 0.0, 0.0);
        addDividend(2.0, 6.0, 0.0);

        const ShareValues v = ShareCalculator::compute(k_shareGuid, 100.0, 100.0);

        CMP_MONEY(v.completeCurValue,   1012.00); // 1000.00 without the dividend
        CMP_MONEY(v.completePurchase,   1000.00);
        CMP_MONEY(v.completeProfitLoss, 12.00);
    }

    // ── Regression: empty SaleBuyDetails must NOT overstate the gain ──────
    // Same inputs as the core scenario, but the sale has NO detail records.
    // Because compute() derives the sold cost basis from aggregates (all buys
    // minus still-held buys) the result must be identical — not the full sale
    // proceeds (which was the "viel zu hoch" bug).
    void test_marktwert_emptyDetails_sameResult()
    {
        addBuy(10.0, 4.0, 100.0, 9.90, 0.0);
        addBuy(5.0, 0.0, 120.0, 5.00, 2.00);
        addSale(4.0, 130.0, 7.0, 0.0, 3.0, {}); // no details

        const ShareValues v = ShareCalculator::compute(k_shareGuid, 125.0, 120.0);

        // Same fixture and values as test_marktwert_coreScenario.
        CMP_MONEY(v.completeProfitLossMarket, 295.00);
        CMP_MONEY(v.completeCurValueMarket,   1895.00);
    }

    // ── Column identity: Kpl. Marktwert == Kpl. Einzahlung + Kpl. Entwicklung
    void test_marktwert_columnIdentity()
    {
        addBuy(10.0, 4.0, 100.0, 9.90, 0.0);
        addBuy(5.0, 0.0, 120.0, 5.00, 2.00);
        addSale(4.0, 130.0, 7.0, 0.0, 3.0, {});

        const ShareValues v = ShareCalculator::compute(k_shareGuid, 125.0, 120.0);

        CMP_MONEY(v.completeCurValueMarket,
                  v.completePurchaseMarket + v.completeProfitLossMarket);
    }

    // ── Edge case: position fully sold (held volume 0) ───────────────────
    void test_marktwert_fullySold()
    {
        addBuy(10.0, 10.0, 100.0, 10.0, 0.0);
        addSale(10.0, 130.0, 8.0, 0.0, 0.0, {});

        const ShareValues v = ShareCalculator::compute(k_shareGuid, 125.0, 120.0);

        CMP_MONEY(v.volume,                   0.0);
        CMP_MONEY(v.curValue,                 0.0);
        CMP_MONEY(v.purchaseValue,            0.0);
        CMP_MONEY(v.profitLossPct,            0.0);   // guarded division
        // Rein Kurs: Verkauf 1300 brutto - Kauf 1000, Kosten zählen nicht.
        CMP_MONEY(v.completeProfitLossMarket, 300.0);
        CMP_MONEY(v.completeCurValueMarket,   1300.0);
    }

    // ── saleProfitLossFinal: identisch zu completeProfitLoss (Depotwert),
    // sobald nichts mehr gehalten wird und keine Dividenden vorliegen (held = 0
    // → unrealisierter Anteil entfällt, es bleibt nur der realisierte,
    // brokeragehaltige G/V). Bis 25.10.2026 gegen completeProfitLossMarket
    // geprüft, das seither "rein Kurs" rechnet.
    // Ergänzt 09.07.2026 für die ShareDetailsForm-Depotwert-Box
    // ("+ Gewinn / Verlust (Verkäufe)"-Zeile) — reine Algebra-Prüfung, ohne
    // von Hand nachgerechnete Zahlen.
    void test_depotwert_saleProfitLossFinal_matchesRealizedWhenFullySold()
    {
        addBuy(10.0, 10.0, 100.0, 10.0, 0.0);
        addSale(10.0, 130.0, 8.0, 0.0, 0.0, {});

        const ShareValues v = ShareCalculator::compute(k_shareGuid, 125.0, 120.0);

        CMP_MONEY(v.saleProfitLossFinal, v.completeProfitLoss);
        CMP_MONEY(v.saleProfitLossFinal, 282.0);

        // salePayoutFinal = round(10*130 - brokerage(8) + 0 - tax(0)) = 1292.00
        CMP_MONEY(v.salePayoutFinal, 1292.0);
    }

    // ── Edge case: no sales — complete == current, identity trivially holds
    void test_marktwert_noSales()
    {
        addBuy(10.0, 0.0, 100.0, 10.0, 0.0);

        const ShareValues v = ShareCalculator::compute(k_shareGuid, 125.0, 120.0);

        CMP_MONEY(v.purchaseValue,            1000.0);
        CMP_MONEY(v.curValue,                 1250.0);
        CMP_MONEY(v.completeProfitLossMarket, 250.0);
        CMP_MONEY(v.completeCurValueMarket,   1250.0);
        CMP_MONEY(v.completeCurValueMarket,   v.curValue); // nothing realized
    }

    // ── Rechenbeispiele aus ARCHITECTURE.md, "Rechenregeln Depotwert und
    // Marktwert" (Nessies Rechenregel vom 25.10.2026) ─────────────────────
    //
    // Echte Zahlen (Alphabet-Analyse 24./25.10.2026). Die Tests pinnen genau
    // die dort dokumentierten Ergebnisse — wer eine Regel ändert, muss
    // Beispiel und Test gemeinsam anpassen.

    // Beispiel (a): vollständig verkauft.
    //   Kauf     200 x 48,595 = 9.719,00 + Kosten 30,95 = 9.749,95
    //   Verkauf  200 x 145,00 = 29.000,00 - Kosten 69,90 - Steuern 5.058,76
    //   Dividenden netto 109,80
    void test_beispielA_vollstaendigVerkauft()
    {
        addBuy(200.0, 200.0, 48.595, 30.95, 0.0);
        addSale(200.0, 145.0, 69.90, 0.0, 5058.76, {});
        addDividend(0.549, 200.0, 0.0); // 109,80

        const ShareValues v = ShareCalculator::compute(k_shareGuid, 293.85, 293.85);

        // Beide Sichten: kein Bestand, aktuell alles 0.
        CMP_MONEY(v.volume,             0.0);
        CMP_MONEY(v.curValue,           0.0);
        CMP_MONEY(v.purchaseValue,      0.0);
        CMP_MONEY(v.purchaseValueFinal, 0.0);
        CMP_MONEY(v.profitLoss,         0.0);
        CMP_MONEY(v.profitLossFinal,    0.0);

        // Depotwert
        CMP_MONEY(v.completePurchase,   9749.95);
        CMP_MONEY(v.salePayoutFinal,   23871.34);
        CMP_MONEY(v.totalDividend,       109.80);
        CMP_MONEY(v.completeCurValue,  23981.14);  // Rückflüsse, NICHT Gewinn
        CMP_MONEY(v.completeProfitLoss, 14231.19);
        QVERIFY(qAbs(v.completeProfitPct - 145.96) < 0.005);

        // Marktwert
        CMP_MONEY(v.completePurchaseMarket,    9719.00);
        CMP_MONEY(v.salePayoutMarket,         29000.00);
        CMP_MONEY(v.completeCurValueMarket,   29000.00);
        CMP_MONEY(v.completeProfitLossMarket, 19281.00);
        QVERIFY(qAbs(v.completeProfitPctMarket - 198.38) < 0.005);
    }

    // Beispiel (b): Teilverkauf, 120 von 200 verkauft, 80 gehalten.
    //   Verkauf 120 x 145,00 = 17.400,00 - Kosten 69,90 - Steuern 3.027,88
    //   Kurs 293,85
    void test_beispielB_teilverkauf()
    {
        addBuy(200.0, 120.0, 48.595, 30.95, 0.0);
        addSale(120.0, 145.0, 69.90, 0.0, 3027.88, {});
        addDividend(0.549, 200.0, 0.0); // 109,80

        const ShareValues v = ShareCalculator::compute(k_shareGuid, 293.85, 293.85);

        CMP_MONEY(v.volume,   80.0);
        CMP_MONEY(v.curValue, 23508.00);

        // Depotwert: Einzahlung gehalten = 80 x 48,595 + 30,95 x 80/200
        CMP_MONEY(v.purchaseValueFinal,   3899.98);
        CMP_MONEY(v.profitLossFinal,     19608.02);
        QVERIFY(qAbs(v.profitLossPctFinal - 502.77) < 0.005);
        CMP_MONEY(v.salePayoutFinal,     14302.22);
        CMP_MONEY(v.completeCurValue,    37920.02);
        CMP_MONEY(v.completeProfitLoss,  28170.07);
        QVERIFY(qAbs(v.completeProfitPct - 288.93) < 0.005);
        // Gegenprobe: realisiert + Dividenden + aktuell = komplett
        CMP_MONEY(v.saleProfitLossFinal, 8452.25);
        CMP_MONEY(v.saleProfitLossFinal + v.totalDividend + v.profitLossFinal,
                  v.completeProfitLoss);

        // Marktwert
        CMP_MONEY(v.purchaseValue,             3887.60);
        CMP_MONEY(v.profitLoss,               19620.40);
        QVERIFY(qAbs(v.profitLossPct - 504.69) < 0.005);
        CMP_MONEY(v.salePayoutMarket,         17400.00);
        CMP_MONEY(v.completeCurValueMarket,   40908.00);
        CMP_MONEY(v.completeProfitLossMarket, 31189.00);
        QVERIFY(qAbs(v.completeProfitPctMarket - 320.91) < 0.005);
        // Gegenprobe: realisiert + aktuell = komplett
        CMP_MONEY(v.saleProfitLoss, 11568.60);
        CMP_MONEY(v.saleProfitLoss + v.profitLoss, v.completeProfitLossMarket);
    }

    // Beispiel (c): FIFO mit Kosten je Kauf. Kauf 1 ist vollständig verkauft,
    // von Kauf 2 sind 80 Stück übrig — gehalten werden nur dessen Kosten
    // anteilig: 10,95 x 80/100 = 8,76 (pauschal über beide Käufe wären es
    // 30,95 x 80/200 = 12,38, das wäre falsch).
    void test_beispielC_fifoKostenJeKauf()
    {
        addBuy(100.0, 100.0, 40.00, 20.00, 0.0);
        addBuy(100.0,  20.0, 57.19, 10.95, 0.0, QStringLiteral("2024-02-01T10:00:00"));
        addSale(120.0, 145.0, 0.0, 0.0, 0.0, {});

        const ShareValues v = ShareCalculator::compute(k_shareGuid, 293.85, 293.85);

        CMP_MONEY(v.volume,             80.0);
        CMP_MONEY(v.purchaseValue,      4575.20);           // 80 x 57,19
        CMP_MONEY(v.purchaseValueFinal, 4575.20 + 8.76);    // + 10,95 x 80/100
        CMP_MONEY(v.completePurchase,   4000.00 + 20.00 + 5719.00 + 10.95);
    }

    // ── Previous-day figures ─────────────────────────────────────────────
    void test_prevDay_diffAndPct()
    {
        addBuy(10.0, 0.0, 100.0, 0.0, 0.0);

        const ShareValues v = ShareCalculator::compute(k_shareGuid, 125.0, 120.0);

        CMP_MONEY(v.prevDayDiff, 5.0);
        QVERIFY(qAbs(v.prevDayPct - (5.0 / 120.0 * 100.0)) < 1e-6);
    }

    // ── Aktiensplit-Behandlung, Phase 2 (07.08.2026, siehe ARCHITECTURE.md
    // "Offene Punkte") ──────────────────────────────────────────────────
    //
    // Fixture angelehnt an den Alphabet-Fall aus der Architektur-Doku: ein
    // Kauf vom 18.03.2020 zu 1.003,00 € (Beleg, vor dem 20:1-Split am
    // 18.07.2022) entspricht danach 20 Mal so vielen Stücken zu einem
    // Zwanzigstel des Preises — der Wert bleibt exakt gleich.

    void test_split_heldVolumeAndCurValueUseTodayScale()
    {
        // Reiner Bestandsfall, keine Verkäufe: 5 Stück à 1.003,00 € vor
        // einem 20:1-Split entsprechen 100 Stück à 50,15 €.
        addBuy(5.0, 0.0, 1003.0, 0.0, 0.0, QStringLiteral("2020-03-18T10:00:00"));
        addSplit(QDate(2022, 7, 18), 20.0, 1.0);

        const ShareValues v = ShareCalculator::compute(k_shareGuid, 50.15, 50.15);

        CMP_MONEY(v.volume,        100.0);
        CMP_MONEY(v.curValue,      5015.00);
        CMP_MONEY(v.purchaseValue, 5015.00); // gleicher Kurs -> keine Entwicklung
        CMP_MONEY(v.profitLoss,    0.0);
    }

    void test_split_reverseSplit_scalesDownHeldVolume()
    {
        // Reverse-Split 1:10: 100 Stück à 5,00 € vor dem Split entsprechen
        // 10 Stück à 50,00 € danach.
        addBuy(100.0, 0.0, 5.0, 0.0, 0.0, QStringLiteral("2020-01-01T10:00:00"));
        addSplit(QDate(2021, 1, 1), 1.0, 10.0);

        const ShareValues v = ShareCalculator::compute(k_shareGuid, 50.0, 50.0);

        CMP_MONEY(v.volume,        10.0);
        CMP_MONEY(v.curValue,      500.0);
        CMP_MONEY(v.purchaseValue, 500.0);
    }

    // Kauf vor dem Split (5 Stück à 1.000 €, davon 2 Stück Beleg-Skala
    // verkauft -> 40 von 100 heutigen Stücken), Verkauf nach dem Split
    // (40 Stück à 60,00 €). Deckt sowohl die Bestands- als auch die
    // realisierte Seite in einer split-übergreifenden Position ab.
    //   adjVolume(Kauf) = 100, adjVolumeSold = 40, adjPreis = 50,00 €
    //   remVol = 60, frac = 0,6
    //   fullBuyValue = 5.000,00 ; heldBuyValue = 3.000,00
    //   curValue = 60 * 55,00 = 3.300,00
    //   Verkauf (nach dem Split, keine weitere Umrechnung nötig):
    //     saleValue = 40 * 60,00 = 2.400,00
    //   soldCost = 5.000,00 - 3.000,00 = 2.000,00
    //   realisierter G/V = 2.400,00 - 2.000,00 = 400,00
    void test_split_realizedAndHeldValuesUseTodayScale()
    {
        addBuy(5.0, 2.0, 1000.0, 0.0, 0.0, QStringLiteral("2020-01-01T10:00:00"));
        addSplit(QDate(2021, 1, 1), 20.0, 1.0);
        addSale(40.0, 60.0, 0.0, 0.0, 0.0, {}, QStringLiteral("2022-01-01T10:00:00"));

        const ShareValues v = ShareCalculator::compute(k_shareGuid, 55.0, 55.0);

        CMP_MONEY(v.volume,                   60.0);
        CMP_MONEY(v.curValue,                 3300.00);
        CMP_MONEY(v.purchaseValue,            3000.00);
        CMP_MONEY(v.profitLoss,               300.00);
        CMP_MONEY(v.saleProfitLoss,           400.00);
        CMP_MONEY(v.completePurchaseMarket,   5000.00);
        CMP_MONEY(v.completeProfitLossMarket, 700.00);
        CMP_MONEY(v.completeCurValueMarket,   5700.00);
    }

    // Brokerage ist ein reiner Geldbetrag und bleibt unskaliert — nur die
    // Pro-Lot-Zuordnung (frac) verwendet die split-bereinigten Stückzahlen.
    // Kauf: 5 Stück à 1.000 €, Brokerage 20,00 €, 2 Stück (Beleg-Skala)
    // verkauft -> 60 % gehalten (40/100 verkauft, 60/100 gehalten).
    //   heldBrokerage = round(20,00 * 0,6) = 12,00
    //   purchaseValueFinal = heldBuyValue(3.000,00) + 12,00 - 0 = 3.012,00
    void test_split_brokerageStaysUnscaled()
    {
        addBuy(5.0, 2.0, 1000.0, 20.0, 0.0, QStringLiteral("2020-01-01T10:00:00"));
        addSplit(QDate(2021, 1, 1), 20.0, 1.0);

        const ShareValues v = ShareCalculator::compute(k_shareGuid, 55.0, 55.0);

        CMP_MONEY(v.purchaseValueFinal, 3012.00);
    }

    // ── Footer-Lücke bei freistehenden Kosteneinträgen (Bug, 05.08.2026,
    // fixed 20.08.2026, siehe ARCHITECTURE.md "Offene Punkte") ────────────
    //
    // A freestanding brokerage/cost entry (buyGuid and saleGuid both empty,
    // created via the standalone Kosten-Verwaltung UI) must fold into
    // completePurchase — like a buy-linked entry does — but must NOT touch
    // completePurchaseMarket, which stays brokerage-free by design.

    void test_freestandingCost_addedToCompletePurchaseOnly()
    {
        addBuy(10.0, 0.0, 100.0, 0.0, 0.0);
        addFreestandingCost(15.0, 5.0); // brokerageReduction() = 15 - 5 = 10.00

        const ShareValues v = ShareCalculator::compute(k_shareGuid, 100.0, 100.0);

        // completePurchase (Depotwert, WITH brokerage) gains the freestanding
        // entry's brokerageReduction().
        CMP_MONEY(v.completePurchase, 1010.00);
        // completePurchaseMarket stays brokerage-free — unaffected by design.
        CMP_MONEY(v.completePurchaseMarket, 1000.00);
        // totalBrokerage already summed freestanding entries before this fix
        // (it is a display-only aggregate over ALL brokerage records) — pinned
        // here too so the fix does not double count it anywhere else.
        CMP_MONEY(v.totalBrokerage, 15.00);
    }

    void test_freestandingCost_reducesCompleteEntwicklungDepotOnly()
    {
        addBuy(10.0, 0.0, 100.0, 0.0, 0.0);
        addFreestandingCost(15.0, 5.0); // brokerageReduction() = 10.00

        const ShareValues v = ShareCalculator::compute(k_shareGuid, 100.0, 100.0);

        // Before the fix this freestanding entry flowed only into
        // totalBrokerage (display), never into completePurchase — so
        // "Komplette Entwicklung" was 10.00 € too high. Seit der Rechenregel
        // vom 25.10.2026 ("rein Kurs") trifft das nur noch den Depotwert:
        // der Marktwert kennt keine Kosten und bleibt bei 0.00.
        CMP_MONEY(v.completeProfitLossMarket,   0.00);
        CMP_MONEY(v.completeProfitLoss,       -10.00);
    }

    void test_freestandingCost_doesNotDoubleCountLinkedBrokerage()
    {
        // Same core-scenario fixture as test_marktwert_coreScenario — only
        // buy-/sale-linked brokerage, nothing freestanding. Pins that the
        // freestanding-cost loop leaves completePurchase unchanged when
        // findByShare() has nothing with empty buyGuid/saleGuid to find.
        addBuy(10.0, 4.0, 100.0, 9.90, 0.0);
        addBuy(5.0, 0.0, 120.0, 5.00, 2.00);
        addSale(4.0, 130.0, 7.0, 0.0, 3.0, {});

        const ShareValues v = ShareCalculator::compute(k_shareGuid, 125.0, 120.0);

        CMP_MONEY(v.completePurchase, 1612.90); // unchanged from test_marktwert_coreScenario
    }
};

QTEST_MAIN(TestShareCalculator)
#include "tst_sharecalculator.moc"

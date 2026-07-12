// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "PresenterShareDetails.h"

#include <QLocale>
#include <QDateTime>
#include <cmath>

namespace {

QString formatMoney(const QLocale& locale, double value)
{
    return locale.toString(value, 'f', 2) + QStringLiteral(" €");
}

QString formatVolume(const QLocale& locale, double value)
{
    return locale.toString(value, 'f', 2) + QStringLiteral(" stk.");
}

QString formatPercent(const QLocale& locale, double value)
{
    return locale.toString(value, 'f', 2) + QStringLiteral(" %");
}

/**
 * @brief Formats an ISO 8601 datetime string (as stored in ShareObject::
 * lastInternetUpdate()/lastPriceUpdate(), e.g. "2026-07-11T00:45:00") using
 * the app's configured locale — same QLocale::ShortFormat convention as
 * BuyObject::dateAsStr()/SaleObject::dateAsStr()/etc. elsewhere in the
 * project, so the display adapts to the configured Länderschema instead of
 * showing the raw ISO string verbatim (bug found 11.07.2026 — ShareObject's
 * two update-timestamp getters return the stored string unformatted, unlike
 * the transaction objects' date getters).
 *
 * Falls back to the raw string if it doesn't parse as ISO 8601, so a
 * malformed value is still visible (and debuggable) rather than silently
 * disappearing.
 */
QString formatDateTime(const QLocale& locale, const QString& isoDateTime)
{
    if (isoDateTime.isEmpty())
        return QString();

    const QDateTime dt = QDateTime::fromString(isoDateTime, Qt::ISODate);
    if (!dt.isValid())
        return isoDateTime;

    return locale.toString(dt, QLocale::ShortFormat);
}

/**
 * @brief Round-half-away-from-zero, duplicated from ShareCalculator::roundAway()
 * on purpose: several presenter-side arithmetic rows (Vortag-Box "Gewinn /
 * Verlust", Aktuelle-Box "Summe" in Depotwert mode, Gesamt-Box "Summe"/
 * "Gewinn / Verlust (gesamt)" in Marktwert mode) round values that are
 * computed here rather than in ShareCalculator — pulling in ShareCalculator.cpp
 * for just this one static utility would drag BuyRepository/SaleRepository/
 * DividendRepository/BrokerageRepository (and therefore Qt6::Sql) into
 * tst_sharedetailsform, defeating its deliberately DB-free design (see
 * ARCHITECTURE.md, "ShareDetailsForm-Details"). Keep in sync with
 * ShareCalculator::roundAway() if that ever changes.
 */
double roundAway(double value, int digits = 2)
{
    const double factor = std::pow(10.0, digits);
    const double scaled = value * factor;
    const double sign   = (scaled < 0.0) ? -1.0 : 1.0;
    return sign * std::floor(std::abs(scaled) + 0.5 + 1e-9) / factor;
}

} // namespace

// ── Constructor ─────────────────────────────────────────────────────────────

PresenterShareDetails::PresenterShareDetails(IViewShareDetails& view,
                                              IModelShareDetails& model,
                                              QString shareGuid,
                                              bool marketValueMode)
    : m_view(view)
    , m_model(model)
    , m_shareGuid(std::move(shareGuid))
    , m_marketValueMode(marketValueMode)
{
}

// ── loadAndDisplay ────────────────────────────────────────────────────────────

bool PresenterShareDetails::loadAndDisplay()
{
    const ShareObject share = m_model.loadShare(m_shareGuid);

    // Explicit error detection over silent workarounds: a missing share is
    // reported and the dialog is closed, rather than silently rendering an
    // empty/half-populated view.
    if (!share.isValid()) {
        m_view.showError(tr("Die Aktie wurde nicht gefunden."));
        m_view.closeDialog();
        return false;
    }

    buildHeader(share);

    const ShareValues v = m_model.computeShareValues(m_shareGuid, share.curPrice(), share.prevDayPrice());

    m_view.populateGesamtBox(buildGesamtBox(v));
    m_view.populateVortagBox(buildVortagBox(v));
    m_view.populateAktuelleBox(buildAktuelleBox(v));

    return true;
}

// ── buildHeader ───────────────────────────────────────────────────────────────

void PresenterShareDetails::buildHeader(const ShareObject& share)
{
    m_view.setHeaderName(share.name());

    const QLocale locale;

    const QString updateText = share.lastInternetUpdate().isEmpty()
        ? tr("noch nicht aktualisiert")
        : formatDateTime(locale, share.lastInternetUpdate());

    m_view.setStatusLine(
        tr("Letzte Internet-Aktualisierung: %1 / Typ: %2")
            .arg(updateText, shareTypeToString(share.shareType())));

    // "Letzte Website-Aktualisierung" = Zeitpunkt der letzten Marktwert-/
    // Kurs-Aktualisierung, von Nessie bestätigt (10.07.2026) — mapped auf
    // ShareObject::lastPriceUpdate(), getrennt von lastInternetUpdate() oben.
    const QString websiteUpdateText = share.lastPriceUpdate().isEmpty()
        ? tr("noch nicht aktualisiert")
        : formatDateTime(locale, share.lastPriceUpdate());

    m_view.setWebsiteUpdateLine(tr("Letzte Website- Aktualisierung: %1").arg(websiteUpdateText));

    m_view.setBoxesTabTitle(m_marketValueMode
        ? tr("Komplette Marktbewertung")
        : tr("Komplette Depotbewertung"));
}

// ── buildGesamtBox ────────────────────────────────────────────────────────────

CalculationRows PresenterShareDetails::buildGesamtBox(const ShareValues& v) const
{
    const QLocale locale;

    // Label "Alle Einzahlungen:" (statt "Verkaufte Einzahlungen:" wie im
    // C#-Original) — der zugrundeliegende Wert (completePurchase/
    // completePurchaseMarket) ist die Summe ALLER Käufe (verkauft + noch
    // gehalten), bestätigt gegen ShareObjectFinalValue.cs:
    // "BuyValueBrokerageReduction => AllBuyEntries.BuyValueBrokerageReductionTotal"
    // — AllBuyEntries wird bei JEDEM Kauf befüllt (AddBuy()), unabhängig vom
    // Verkaufsstatus. Das C#-Label war irreführend benannt; von Nessie
    // bestätigt (10.07.2026, zunächst "Alle Käufe:", dann final auf
    // "Alle Einzahlungen:" korrigiert — passt besser zur ersten Zeile der Box,
    // "= Einzahlungen:").

    if (m_marketValueMode) {
        // Pure market-only arithmetic — deliberately NOT completeCurValueMarket/
        // completeProfitLossMarket/completeProfitPctMarket, see class comment.
        const double sum        = roundAway(v.curValue + v.salePayoutMarket);
        const double profitLoss = roundAway(sum - v.completePurchaseMarket);
        const double profitPct  = (v.completePurchaseMarket > 0.0)
                                  ? (profitLoss / v.completePurchaseMarket * 100.0)
                                  : 0.0;
        const QColor plColor = performanceColor(profitLoss);

        CalculationRows rows;
        rows << CalculationRow{ QString(), tr("Anteile:"), formatVolume(locale, v.volume), QColor(), false }
             << CalculationRow{ QStringLiteral("×"), tr("Aktueller Kurswert:"), formatMoney(locale, v.curPrice), QColor(), false }
             << CalculationRow{ QStringLiteral("="), tr("Aktueller Bestandswert:"), formatMoney(locale, v.curValue), QColor(), true }
             << disabledRow(QStringLiteral("+"), tr("Dividenden:"))
             << CalculationRow{ QStringLiteral("+"), tr("Verkäufe:"), formatMoney(locale, v.salePayoutMarket), QColor(), false }
             << CalculationRow{ QStringLiteral("="), tr("Summe:"), formatMoney(locale, sum), QColor(), true }
             << CalculationRow{ QStringLiteral("−"), tr("Alle Einzahlungen:"), formatMoney(locale, v.completePurchaseMarket), QColor(), false }
             << CalculationRow{ QStringLiteral("="), tr("Gewinn / Verlust (gesamt):"), formatMoney(locale, profitLoss), plColor, true }
             << CalculationRow{ QString(), tr("Entwicklung:"), formatPercent(locale, profitPct), plColor, false };
        return rows;
    }

    // Depotwert mode — unchanged, uses existing ShareValues fields directly.
    const QColor plColor = performanceColor(v.completeProfitLoss);

    CalculationRows rows;
    rows << CalculationRow{ QString(), tr("Anteile:"), formatVolume(locale, v.volume), QColor(), false }
         << CalculationRow{ QStringLiteral("×"), tr("Aktueller Kurswert:"), formatMoney(locale, v.curPrice), QColor(), false }
         << CalculationRow{ QStringLiteral("="), tr("Aktueller Bestandswert:"), formatMoney(locale, v.curValue), QColor(), true }
         << CalculationRow{ QStringLiteral("+"), tr("Dividenden:"), formatMoney(locale, v.totalDividend), QColor(), false }
         << CalculationRow{ QStringLiteral("+"), tr("Verkäufe:"), formatMoney(locale, v.salePayoutFinal), QColor(), false }
         << CalculationRow{ QStringLiteral("="), tr("Summe:"), formatMoney(locale, v.completeCurValue), QColor(), true }
         << CalculationRow{ QStringLiteral("−"), tr("Alle Einzahlungen:"), formatMoney(locale, v.completePurchase), QColor(), false }
         << CalculationRow{ QStringLiteral("="), tr("Gewinn / Verlust (gesamt):"), formatMoney(locale, v.completeProfitLoss), plColor, true }
         << CalculationRow{ QString(), tr("Entwicklung:"), formatPercent(locale, v.completeProfitPct), plColor, false };

    return rows;
}

// ── buildVortagBox ────────────────────────────────────────────────────────────

CalculationRows PresenterShareDetails::buildVortagBox(const ShareValues& v) const
{
    const QLocale locale;
    const QColor  diffColor = performanceColor(v.prevDayDiff);

    // Simple arithmetic over already-computed fields (volume, prevDayDiff) —
    // no repository access involved, so kept here rather than in ShareCalculator.
    const double prevDayProfitLoss = roundAway(v.volume * v.prevDayDiff);

    CalculationRows rows;
    rows << CalculationRow{ QString(), tr("Aktueller Kurswert:"), formatMoney(locale, v.curPrice), QColor(), false }
         << CalculationRow{ QStringLiteral("−"), tr("Vortages-Kurswert:"), formatMoney(locale, v.prevDayPrice), QColor(), false }
         << CalculationRow{ QStringLiteral("="), tr("Kurswert-Entw.:"), formatMoney(locale, v.prevDayDiff), diffColor, true }
         << CalculationRow{ QString(), tr("Entwicklung:"), formatPercent(locale, v.prevDayPct), diffColor, false }
         << CalculationRow{ QString(), tr("Anteile:"), formatVolume(locale, v.volume), QColor(), false }
         << CalculationRow{ QStringLiteral("×"), tr("Kurswert-Entw.:"), formatMoney(locale, v.prevDayDiff), QColor(), false }
         << CalculationRow{ QStringLiteral("="), tr("Gewinn / Verlust:"), formatMoney(locale, prevDayProfitLoss), diffColor, true };

    return rows;
}

// ── buildAktuelleBox ──────────────────────────────────────────────────────────

CalculationRows PresenterShareDetails::buildAktuelleBox(const ShareValues& v) const
{
    const QLocale locale;

    if (m_marketValueMode) {
        // marketValue already equals curValue + saleProfitLoss (see
        // ShareCalculator::compute()) — no fresh rounding needed here.
        CalculationRows rows;
        rows << CalculationRow{ QString(), tr("Anteile:"), formatVolume(locale, v.volume), QColor(), false }
             << CalculationRow{ QStringLiteral("×"), tr("Aktueller Kurswert:"), formatMoney(locale, v.curPrice), QColor(), false }
             << CalculationRow{ QStringLiteral("="), tr("Aktueller Bestandswert:"), formatMoney(locale, v.curValue), QColor(), true }
             << disabledRow(QStringLiteral("+"), tr("Dividenden:"))
             << CalculationRow{ QStringLiteral("+"), tr("Gewinn / Verlust (Verkäufe):"), formatMoney(locale, v.saleProfitLoss), QColor(), false }
             << CalculationRow{ QStringLiteral("="), tr("Summe:"), formatMoney(locale, v.marketValue), QColor(), true };
        return rows;
    }

    // Depotwert mode — unchanged. Simple arithmetic over already-computed
    // fields (curValue, totalDividend, saleProfitLossFinal) — see class comment.
    const double sum = roundAway(v.curValue + v.totalDividend + v.saleProfitLossFinal);

    CalculationRows rows;
    rows << CalculationRow{ QString(), tr("Anteile:"), formatVolume(locale, v.volume), QColor(), false }
         << CalculationRow{ QStringLiteral("×"), tr("Aktueller Kurswert:"), formatMoney(locale, v.curPrice), QColor(), false }
         << CalculationRow{ QStringLiteral("="), tr("Aktueller Bestandswert:"), formatMoney(locale, v.curValue), QColor(), true }
         << CalculationRow{ QStringLiteral("+"), tr("Dividenden:"), formatMoney(locale, v.totalDividend), QColor(), false }
         << CalculationRow{ QStringLiteral("+"), tr("Gewinn / Verlust (Verkäufe):"), formatMoney(locale, v.saleProfitLossFinal), QColor(), false }
         << CalculationRow{ QStringLiteral("="), tr("Summe:"), formatMoney(locale, sum), QColor(), true };

    return rows;
}

// ── disabledRow ───────────────────────────────────────────────────────────────

CalculationRow PresenterShareDetails::disabledRow(const QString& operatorSymbol, const QString& label)
{
    // Marktwert mode has no dividend figures at all (dividends are a
    // Depotwert-only concept) — shown as a greyed-out "-" placeholder,
    // matching the disabled label in the C# reference screenshot.
    return CalculationRow{ operatorSymbol, label, QStringLiteral("-"), QColor(Qt::gray), false };
}

// ── shareTypeToString ─────────────────────────────────────────────────────────

QString PresenterShareDetails::shareTypeToString(ShareType type)
{
    switch (type) {
    case ShareType::Share: return tr("Aktie");
    case ShareType::Fond:  return tr("Fonds");
    case ShareType::Etf:   return tr("ETF");
    default:                return tr("Unbekannt");
    }
}

// ── performanceColor ──────────────────────────────────────────────────────────

QColor PresenterShareDetails::performanceColor(double value)
{
    return value >= 0.0 ? QColor(QStringLiteral("green")) : QColor(QStringLiteral("red"));
}

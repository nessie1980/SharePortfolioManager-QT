// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "PresenterShareDetails.h"

#include <QLocale>
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
 * @brief Round-half-away-from-zero, duplicated from ShareCalculator::roundAway()
 * on purpose: PresenterShareDetails only ever rounds the two arithmetic rows
 * below (volume x prevDayDiff, curValue + totalDividend + saleProfitLossFinal)
 * — pulling in ShareCalculator.cpp for just this one static utility would
 * drag BuyRepository/SaleRepository/DividendRepository/BrokerageRepository
 * (and therefore Qt6::Sql) into tst_sharedetailsform, defeating its
 * deliberately DB-free design (see ARCHITECTURE.md, "ShareDetailsForm-Details").
 * Keep in sync with ShareCalculator::roundAway() if that ever changes.
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
                                              QString shareGuid)
    : m_view(view)
    , m_model(model)
    , m_shareGuid(std::move(shareGuid))
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

    const QString updateText = share.lastInternetUpdate().isEmpty()
        ? tr("noch nicht aktualisiert")
        : share.lastInternetUpdate();

    m_view.setStatusLine(
        tr("Letzte Internet-Aktualisierung: %1 / Typ: %2")
            .arg(updateText, shareTypeToString(share.shareType())));
}

// ── buildGesamtBox ────────────────────────────────────────────────────────────

CalculationRows PresenterShareDetails::buildGesamtBox(const ShareValues& v) const
{
    const QLocale locale;
    const QColor  plColor = performanceColor(v.completeProfitLoss);

    CalculationRows rows;
    rows << CalculationRow{ QString(), tr("Anteile:"), formatVolume(locale, v.volume), QColor(), false }
         << CalculationRow{ QStringLiteral("×"), tr("Aktueller Preis:"), formatMoney(locale, v.curPrice), QColor(), false }
         << CalculationRow{ QStringLiteral("="), tr("Einzahlungen:"), formatMoney(locale, v.curValue), QColor(), true }
         << CalculationRow{ QStringLiteral("+"), tr("Dividenden:"), formatMoney(locale, v.totalDividend), QColor(), false }
         << CalculationRow{ QStringLiteral("+"), tr("Verkäufe:"), formatMoney(locale, v.salePayoutFinal), QColor(), false }
         << CalculationRow{ QStringLiteral("="), tr("Summe:"), formatMoney(locale, v.completeCurValue), QColor(), true }
         << CalculationRow{ QStringLiteral("−"), tr("Verkaufte Einzahlungen:"), formatMoney(locale, v.completePurchase), QColor(), false }
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
    rows << CalculationRow{ QString(), tr("Aktueller Preis:"), formatMoney(locale, v.curPrice), QColor(), false }
         << CalculationRow{ QStringLiteral("−"), tr("Vortages-Preis:"), formatMoney(locale, v.prevDayPrice), QColor(), false }
         << CalculationRow{ QStringLiteral("="), tr("Preis-Entw.:"), formatMoney(locale, v.prevDayDiff), diffColor, true }
         << CalculationRow{ QString(), tr("Entwicklung:"), formatPercent(locale, v.prevDayPct), diffColor, false }
         << CalculationRow{ QString(), tr("Anteile:"), formatVolume(locale, v.volume), QColor(), false }
         << CalculationRow{ QStringLiteral("×"), tr("Preis-Entw.:"), formatMoney(locale, v.prevDayDiff), QColor(), false }
         << CalculationRow{ QStringLiteral("="), tr("Gewinn / Verlust:"), formatMoney(locale, prevDayProfitLoss), diffColor, true };

    return rows;
}

// ── buildAktuelleBox ──────────────────────────────────────────────────────────

CalculationRows PresenterShareDetails::buildAktuelleBox(const ShareValues& v) const
{
    const QLocale locale;

    // Simple arithmetic over already-computed fields — see class comment.
    const double sum = roundAway(v.curValue + v.totalDividend + v.saleProfitLossFinal);

    CalculationRows rows;
    rows << CalculationRow{ QString(), tr("Anteile:"), formatVolume(locale, v.volume), QColor(), false }
         << CalculationRow{ QStringLiteral("×"), tr("Aktueller Preis:"), formatMoney(locale, v.curPrice), QColor(), false }
         << CalculationRow{ QStringLiteral("="), tr("Einzahlungen:"), formatMoney(locale, v.curValue), QColor(), true }
         << CalculationRow{ QStringLiteral("+"), tr("Dividenden:"), formatMoney(locale, v.totalDividend), QColor(), false }
         << CalculationRow{ QStringLiteral("+"), tr("Gewinn / Verlust (Verkäufe):"), formatMoney(locale, v.saleProfitLossFinal), QColor(), false }
         << CalculationRow{ QStringLiteral("="), tr("Summe:"), formatMoney(locale, sum), QColor(), true };

    return rows;
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

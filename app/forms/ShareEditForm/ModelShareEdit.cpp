// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ModelShareEdit.h"

// ── loadShare ─────────────────────────────────────────────────────────────────

ShareObject ModelShareEdit::loadShare(const QString& guid) const
{
    m_lastError.clear();
    const ShareObject share = m_shareRepo.findByGuid(guid);
    if (!share.isValid())
        m_lastError = QStringLiteral("Aktie nicht gefunden (GUID: %1)").arg(guid);
    return share;
}

// ── saveShare ─────────────────────────────────────────────────────────────────

bool ModelShareEdit::saveShare(const ShareObject& share)
{
    m_lastError.clear();
    if (!m_shareRepo.update(share)) {
        m_lastError = QStringLiteral("Aktie konnte nicht gespeichert werden: ")
                      + m_shareRepo.lastError().text();
        return false;
    }
    return true;
}

// ── Aggregates ────────────────────────────────────────────────────────────────

double ModelShareEdit::totalBuyValue(const QString& shareGuid) const
{
    return m_buyRepo.totalBuyValueBrokerageReduction(shareGuid);
}

int ModelShareEdit::buyCount(const QString& shareGuid) const
{
    return m_buyRepo.findByShare(shareGuid).size();
}

double ModelShareEdit::totalSaleValue(const QString& shareGuid) const
{
    return m_saleRepo.totalPayoutBrokerageReduction(shareGuid);
}

double ModelShareEdit::totalProfitLoss(const QString& shareGuid) const
{
    return m_saleRepo.totalProfitLossBrokerageReduction(shareGuid);
}

int ModelShareEdit::saleCount(const QString& shareGuid) const
{
    return m_saleRepo.findByShare(shareGuid).size();
}

double ModelShareEdit::totalDividendValue(const QString& shareGuid) const
{
    return m_dividendRepo.totalPayoutWithTaxes(shareGuid);
}

int ModelShareEdit::dividendCount(const QString& shareGuid) const
{
    return m_dividendRepo.findByShare(shareGuid).size();
}

double ModelShareEdit::totalBrokerageValue(const QString& shareGuid) const
{
    // All brokerage costs are stored as linked BrokerageObject entries —
    // one per buy (buyGuid set) and one per sale (saleGuid set).
    // Standalone records (neither buyGuid nor saleGuid) are included too.
    return m_brokerageRepo.totalBrokerageReduction(shareGuid);
}

int ModelShareEdit::brokerageCount(const QString& shareGuid) const
{
    return m_brokerageRepo.findByShare(shareGuid).size();
}

double ModelShareEdit::currentVolume(const QString& shareGuid) const
{
    // Total bought minus total already sold = shares currently in depot
    const QList<BuyObject> buys = m_buyRepo.findByShare(shareGuid);
    double total = 0.0;
    for (const BuyObject& b : buys)
        total += b.volume() - b.volumeSold();
    return total;
}

QString ModelShareEdit::firstBuyDate(const QString& shareGuid) const
{
    // findByShare returns rows ordered by datetime ASC → first() is the earliest
    const QList<BuyObject> buys = m_buyRepo.findByShare(shareGuid);
    if (buys.isEmpty())
        return QString();
    return buys.first().dateAsStr();
}

// ── loadSplits ────────────────────────────────────────────────────────────────

QList<ShareSplitObject> ModelShareEdit::loadSplits(const QString& shareGuid) const
{
    // Reine Weiterleitung — findByShare() liefert bereits aufsteigend nach Datum,
    // was ViewShareEdit::setSplitInfo() für "zuletzt ..." voraussetzt.
    return m_splitRepo.findByShare(shareGuid);
}

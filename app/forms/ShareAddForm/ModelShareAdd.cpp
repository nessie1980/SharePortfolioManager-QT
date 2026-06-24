// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ModelShareAdd.h"
#include "../../core/Database.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QUuid>

// ─────────────────────────────────────────────────────────────────────────────
bool ModelShareAdd::saveShareWithBuy(const ShareObject& share,
                                     const BuyObject&   buy,
                                     double provision,
                                     double brokerFee,
                                     double traderFee,
                                     double reduction)
{
    m_lastError.clear();

    QSqlDatabase db = QSqlDatabase::database(Database::connectionName());
    if (!db.transaction()) {
        m_lastError = QStringLiteral("Transaktion konnte nicht gestartet werden: ")
                      + db.lastError().text();
        return false;
    }

    // 1. Insert share
    if (!m_shareRepo.insert(share)) {
        m_lastError = QStringLiteral("Aktie konnte nicht gespeichert werden: ")
                      + m_shareRepo.lastError().text();
        db.rollback();
        return false;
    }

    // 2. Insert buy first (brokerage references buys(guid) via FK)
    const QString brokerageGuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    BuyObject buyWithRef(
        buy.guid(),
        buy.shareGuid(),
        buy.depotNumber(),
        buy.orderNumber(),
        buy.dateTime(),
        buy.volume(),
        buy.volumeSold(),
        buy.price(),
        brokerageGuid,
        buy.document());

    if (!m_buyRepo.insert(buyWithRef)) {
        m_lastError = QStringLiteral("Kauf konnte nicht gespeichert werden: ")
                      + m_buyRepo.lastError().text();
        db.rollback();
        return false;
    }

    // 3. Insert brokerage — buy already exists, FK constraint satisfied
    const BrokerageObject brokerage(
        brokerageGuid,
        share.guid(),
        buy.guid(),    // buyGuid
        QString(),     // saleGuid
        buy.dateTime(),
        provision,
        brokerFee,
        traderFee,
        reduction,
        buy.document());

    if (!m_brokerageRepo.insert(brokerage)) {
        m_lastError = QStringLiteral("Brokerage-Eintrag konnte nicht gespeichert werden: ")
                      + m_brokerageRepo.lastError().text();
        db.rollback();
        return false;
    }

    if (!db.commit()) {
        m_lastError = QStringLiteral("Commit fehlgeschlagen: ")
                      + db.lastError().text();
        db.rollback();
        return false;
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
bool ModelShareAdd::wknExists(const QString& wkn) const
{
    return m_shareRepo.wknExists(wkn);
}

// ─────────────────────────────────────────────────────────────────────────────
bool ModelShareAdd::isinExists(const QString& isin) const
{
    return m_shareRepo.isinExists(isin);
}

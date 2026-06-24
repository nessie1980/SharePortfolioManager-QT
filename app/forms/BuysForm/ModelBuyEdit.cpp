// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ModelBuyEdit.h"
#include "../../core/Database.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

// ── loadBuys ──────────────────────────────────────────────────────────────────

QList<BuyObject> ModelBuyEdit::loadBuys(const QString& shareGuid) const
{
    return m_buyRepo.findByShare(shareGuid);
}

ShareObject ModelBuyEdit::loadShare(const QString& shareGuid) const
{
    return m_shareRepo.findByGuid(shareGuid);
}

// ── loadBrokerage ─────────────────────────────────────────────────────────────

BrokerageObject ModelBuyEdit::loadBrokerage(const QString& buyGuid) const
{
    return m_brokerageRepo.findByBuyGuid(buyGuid);
}

// ── addBuy ────────────────────────────────────────────────────────────────────

bool ModelBuyEdit::addBuy(const BuyObject& buy,
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

    // Generate a brokerage GUID so the buy can reference it.
    const QString brokerageGuid = QUuid::createUuid().toString(QUuid::WithoutBraces);

    const BuyObject buyWithRef(
        buy.guid(),
        buy.shareGuid(),
        buy.depotNumber(),
        buy.orderNumber(),
        buy.dateTime(),
        buy.volume(),
        0.0,           // volumeSold starts at 0
        buy.price(),
        brokerageGuid,
        buy.document());

    if (!m_buyRepo.insert(buyWithRef)) {
        m_lastError = QStringLiteral("Kauf konnte nicht gespeichert werden: ")
                      + m_buyRepo.lastError().text();
        db.rollback();
        return false;
    }

    const BrokerageObject brokerage(
        brokerageGuid,
        buy.shareGuid(),
        buy.guid(),   // buyGuid
        QString(),    // saleGuid — empty for buy-linked brokerage
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

// ── updateBuy ─────────────────────────────────────────────────────────────────

bool ModelBuyEdit::updateBuy(const BuyObject& buy,
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

    if (!m_buyRepo.update(buy)) {
        m_lastError = QStringLiteral("Kauf konnte nicht aktualisiert werden: ")
                      + m_buyRepo.lastError().text();
        db.rollback();
        return false;
    }

    BrokerageObject existing = m_brokerageRepo.findByBuyGuid(buy.guid());

    if (existing.isValid()) {
        const BrokerageObject updated(
            existing.guid(),
            existing.shareGuid(),
            buy.guid(),
            existing.saleGuid(),
            buy.dateTime(),
            provision,
            brokerFee,
            traderFee,
            reduction,
            buy.document());

        if (!m_brokerageRepo.update(updated)) {
            m_lastError = QStringLiteral("Brokerage konnte nicht aktualisiert werden: ")
                          + m_brokerageRepo.lastError().text();
            db.rollback();
            return false;
        }
    } else {
        // No linked brokerage yet — create one.
        const QString brokerageGuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const BrokerageObject newBrokerage(
            brokerageGuid,
            buy.shareGuid(),
            buy.guid(),
            QString(),
            buy.dateTime(),
            provision,
            brokerFee,
            traderFee,
            reduction,
            buy.document());

        if (!m_brokerageRepo.insert(newBrokerage)) {
            m_lastError = QStringLiteral("Brokerage konnte nicht erstellt werden: ")
                          + m_brokerageRepo.lastError().text();
            db.rollback();
            return false;
        }
    }

    if (!db.commit()) {
        m_lastError = QStringLiteral("Commit fehlgeschlagen: ")
                      + db.lastError().text();
        db.rollback();
        return false;
    }

    return true;
}

// ── removeBuy ─────────────────────────────────────────────────────────────────

bool ModelBuyEdit::removeBuy(const QString& buyGuid)
{
    m_lastError.clear();

    QSqlDatabase db = QSqlDatabase::database(Database::connectionName());
    if (!db.transaction()) {
        m_lastError = QStringLiteral("Transaktion konnte nicht gestartet werden: ")
                      + db.lastError().text();
        return false;
    }

    // Remove brokerage first (it references buys.guid via FK).
    const BrokerageObject brokerage = m_brokerageRepo.findByBuyGuid(buyGuid);
    if (brokerage.isValid()) {
        if (!m_brokerageRepo.remove(brokerage.guid())) {
            m_lastError = QStringLiteral("Brokerage konnte nicht gelöscht werden: ")
                          + m_brokerageRepo.lastError().text();
            db.rollback();
            return false;
        }
    }

    if (!m_buyRepo.remove(buyGuid)) {
        m_lastError = QStringLiteral("Kauf konnte nicht gelöscht werden: ")
                      + m_buyRepo.lastError().text();
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

// ── orderNumberExists ─────────────────────────────────────────────────────────

bool ModelBuyEdit::orderNumberExists(const QString& shareGuid,
                                     const QString& orderNumber,
                                     const QString& excludeGuid) const
{
    for (const BuyObject& b : m_buyRepo.findByShare(shareGuid)) {
        if (b.orderNumber() == orderNumber && b.guid() != excludeGuid)
            return true;
    }
    return false;
}

// ── documentExists ────────────────────────────────────────────────────────────

bool ModelBuyEdit::documentExists(const QString& document,
                                  const QString& excludeGuid) const
{
    if (document.trimmed().isEmpty()) return false;

    QSqlDatabase db = QSqlDatabase::database(Database::connectionName());
    QSqlQuery q(db);
    if (excludeGuid.isEmpty()) {
        q.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM buys WHERE document = :doc"));
        q.bindValue(QStringLiteral(":doc"), document.trimmed());
    } else {
        q.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM buys WHERE document = :doc AND guid != :excl"));
        q.bindValue(QStringLiteral(":doc"),  document.trimmed());
        q.bindValue(QStringLiteral(":excl"), excludeGuid);
    }
    if (q.exec() && q.next())
        return q.value(0).toInt() > 0;
    return false;
}

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ModelBrokerageEdit.h"
#include "../../core/Database.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

// ── loadBrokerages ────────────────────────────────────────────────────────────

QList<BrokerageObject> ModelBrokerageEdit::loadBrokerages(const QString& shareGuid) const
{
    return m_brokerageRepo.findByShare(shareGuid);
}

// ── addBrokerage ──────────────────────────────────────────────────────────────

bool ModelBrokerageEdit::addBrokerage(const BrokerageObject& brokerage)
{
    m_lastError.clear();
    if (!m_brokerageRepo.insert(brokerage)) {
        m_lastError = QStringLiteral("Kosten konnten nicht gespeichert werden: ")
                      + m_brokerageRepo.lastError().text();
        return false;
    }
    return true;
}

// ── updateBrokerage ───────────────────────────────────────────────────────────

bool ModelBrokerageEdit::updateBrokerage(const BrokerageObject& brokerage)
{
    m_lastError.clear();
    if (!m_brokerageRepo.update(brokerage)) {
        m_lastError = QStringLiteral("Kosten konnten nicht aktualisiert werden: ")
                      + m_brokerageRepo.lastError().text();
        return false;
    }
    return true;
}

// ── updateDocument ────────────────────────────────────────────────────────────

bool ModelBrokerageEdit::updateDocument(const QString& guid, const QString& document)
{
    m_lastError.clear();
    if (!m_brokerageRepo.updateDocument(guid, document)) {
        m_lastError = QStringLiteral("Dokument konnte nicht aktualisiert werden: ")
                      + m_brokerageRepo.lastError().text();
        return false;
    }
    return true;
}

// ── removeBrokerage ───────────────────────────────────────────────────────────

bool ModelBrokerageEdit::removeBrokerage(const QString& guid)
{
    m_lastError.clear();
    if (!m_brokerageRepo.remove(guid)) {
        m_lastError = QStringLiteral("Kosten konnten nicht gelöscht werden: ")
                      + m_brokerageRepo.lastError().text();
        return false;
    }
    return true;
}

// ── documentExists ────────────────────────────────────────────────────────────

bool ModelBrokerageEdit::documentExists(const QString& document,
                                        const QString& excludeGuid) const
{
    if (document.trimmed().isEmpty()) return false;

    QSqlDatabase db = QSqlDatabase::database(Database::connectionName());
    QSqlQuery q(db);
    if (excludeGuid.isEmpty()) {
        q.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM brokerage WHERE document = :doc"));
        q.bindValue(QStringLiteral(":doc"), document.trimmed());
    } else {
        q.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM brokerage WHERE document = :doc AND guid != :excl"));
        q.bindValue(QStringLiteral(":doc"),  document.trimmed());
        q.bindValue(QStringLiteral(":excl"), excludeGuid);
    }
    if (q.exec() && q.next())
        return q.value(0).toInt() > 0;
    return false;
}

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ModelDividendEdit.h"
#include "../../core/Database.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

// ── loadDividends ─────────────────────────────────────────────────────────────

QList<DividendObject> ModelDividendEdit::loadDividends(const QString& shareGuid) const
{
    return m_dividendRepo.findByShare(shareGuid);
}

ShareObject ModelDividendEdit::loadShare(const QString& shareGuid) const
{
    return m_shareRepo.findByGuid(shareGuid);
}

// ── findClosingPriceForDate ────────────────────────────────────────────────────

bool ModelDividendEdit::findClosingPriceForDate(const QString& shareGuid,
                                                const QDate&    date,
                                                double&         outPrice) const
{
    const DailyValuesObject dailyValue =
        m_dailyValuesRepo.findByShareAndDate(shareGuid, date);

    if (!dailyValue.isValid() || dailyValue.closingPrice() <= 0.0)
        return false;

    outPrice = dailyValue.closingPrice();
    return true;
}

// ── addDividend ───────────────────────────────────────────────────────────────

bool ModelDividendEdit::addDividend(const DividendObject& dividend)
{
    m_lastError.clear();
    if (!m_dividendRepo.insert(dividend)) {
        m_lastError = QStringLiteral("Dividende konnte nicht gespeichert werden: ")
                      + m_dividendRepo.lastError().text();
        return false;
    }
    return true;
}

// ── updateDividend ────────────────────────────────────────────────────────────

bool ModelDividendEdit::updateDividend(const DividendObject& dividend)
{
    m_lastError.clear();
    if (!m_dividendRepo.update(dividend)) {
        m_lastError = QStringLiteral("Dividende konnte nicht aktualisiert werden: ")
                      + m_dividendRepo.lastError().text();
        return false;
    }
    return true;
}

// ── removeDividend ────────────────────────────────────────────────────────────

bool ModelDividendEdit::removeDividend(const QString& dividendGuid)
{
    m_lastError.clear();
    if (!m_dividendRepo.remove(dividendGuid)) {
        m_lastError = QStringLiteral("Dividende konnte nicht gelöscht werden: ")
                      + m_dividendRepo.lastError().text();
        return false;
    }
    return true;
}

// ── documentExists ────────────────────────────────────────────────────────────

bool ModelDividendEdit::documentExists(const QString& document,
                                       const QString& excludeGuid) const
{
    if (document.trimmed().isEmpty()) return false;

    QSqlDatabase db = QSqlDatabase::database(Database::connectionName());
    QSqlQuery q(db);
    if (excludeGuid.isEmpty()) {
        q.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM dividends WHERE document = :doc"));
        q.bindValue(QStringLiteral(":doc"), document.trimmed());
    } else {
        q.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM dividends WHERE document = :doc AND guid != :excl"));
        q.bindValue(QStringLiteral(":doc"),  document.trimmed());
        q.bindValue(QStringLiteral(":excl"), excludeGuid);
    }
    if (q.exec() && q.next())
        return q.value(0).toInt() > 0;
    return false;
}

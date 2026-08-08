// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ModelShareSplitEdit.h"
#include "../../models/BuyObject.h"
#include "../../core/Database.h"

#include <QSqlDatabase>
#include <QSqlQuery>

// ── Read ──────────────────────────────────────────────────────────────────────

QList<ShareSplitObject> ModelShareSplitEdit::loadSplits(const QString& shareGuid) const
{
    return m_splitRepo.findByShare(shareGuid);
}

bool ModelShareSplitEdit::existsForDate(const QString& shareGuid, const QDate& date) const
{
    return m_splitRepo.existsForDate(shareGuid, date);
}

QList<OpenBuyLot> ModelShareSplitEdit::openLots(const QString& shareGuid) const
{
    QList<OpenBuyLot> lots;
    const QList<BuyObject> buys = m_buyRepo.findByShare(shareGuid);
    for (const BuyObject& b : buys) {
        const double remaining = b.volume() - b.volumeSold();
        if (remaining > 0.0)
            lots.append(OpenBuyLot{ b.date(), remaining });
    }
    return lots;
}

bool ModelShareSplitEdit::documentExists(const QString& document,
                                         const QString& excludeGuid) const
{
    // Steht bewusst hier und nicht in ShareSplitRepository — dieselbe
    // Platzierung wie ModelBuyEdit, ModelSaleEdit, ModelDividendEdit und
    // ModelBrokerageEdit (Nessies Entscheidung 08.08.2026). Die Prüfung ist
    // eine Formular-Angelegenheit, kein allgemeiner Persistenzdienst; das
    // Repository führt nur updateDocument(), das DocumentRootMigrator braucht.
    const QString trimmed = document.trimmed();
    if (trimmed.isEmpty())
        return false;

    // Zwei getrennte Abfragen statt einer mit "guid != :excl", und zwar aus
    // einem zwingenden Grund: beim Anlegen ist excludeGuid ein
    // default-konstruiertes QString(), also NULL und nicht bloss leer. Qt
    // bindet das als SQL-NULL, und "guid != NULL" ergibt in SQL nicht true,
    // sondern NULL — eine WHERE-Bedingung, die zu NULL auswertet, filtert die
    // Zeile heraus. Die zusammengefasste Abfrage lieferte damit immer 0 und
    // die Prüfung wäre still wirkungslos (Bugfix 08.08.2026).
    QSqlDatabase db = QSqlDatabase::database(Database::connectionName());
    QSqlQuery q(db);
    if (excludeGuid.isEmpty()) {
        q.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM share_splits WHERE document = :doc"));
        q.bindValue(QStringLiteral(":doc"), trimmed);
    } else {
        q.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM share_splits WHERE document = :doc AND guid != :excl"));
        q.bindValue(QStringLiteral(":doc"),  trimmed);
        q.bindValue(QStringLiteral(":excl"), excludeGuid);
    }
    if (q.exec() && q.next())
        return q.value(0).toInt() > 0;
    return false;
}

// ── Create / Update / Delete ──────────────────────────────────────────────────

bool ModelShareSplitEdit::addSplit(const ShareSplitObject& split)
{
    m_lastError.clear();
    if (!m_splitRepo.insert(split)) {
        m_lastError = QStringLiteral("Split konnte nicht gespeichert werden: ")
                      + m_splitRepo.lastError().text();
        return false;
    }
    return true;
}

bool ModelShareSplitEdit::updateSplit(const ShareSplitObject& split)
{
    m_lastError.clear();
    if (!m_splitRepo.update(split)) {
        m_lastError = QStringLiteral("Split konnte nicht aktualisiert werden: ")
                      + m_splitRepo.lastError().text();
        return false;
    }
    return true;
}

bool ModelShareSplitEdit::removeSplit(const QString& guid)
{
    m_lastError.clear();
    if (!m_splitRepo.remove(guid)) {
        m_lastError = QStringLiteral("Split konnte nicht entfernt werden: ")
                      + m_splitRepo.lastError().text();
        return false;
    }
    return true;
}

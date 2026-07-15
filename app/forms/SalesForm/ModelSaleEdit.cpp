// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "ModelSaleEdit.h"
#include "../../core/Database.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

// ── loadSales ─────────────────────────────────────────────────────────────────

QList<SaleObject> ModelSaleEdit::loadSales(const QString& shareGuid) const
{
    return m_saleRepo.findByShare(shareGuid);
}

// ── loadShare ─────────────────────────────────────────────────────────────────

ShareObject ModelSaleEdit::loadShare(const QString& shareGuid) const
{
    return m_shareRepo.findByGuid(shareGuid);
}

// ── loadAvailableBuys ─────────────────────────────────────────────────────────
// Returns all buys that still have remaining (unsold) volume for a share,
// ordered oldest → newest (ascending dateTime — BuyRepository returns ASC).

QList<BuyObject> ModelSaleEdit::loadAvailableBuys(const QString& shareGuid) const
{
    QList<BuyObject> all = m_buyRepo.findByShare(shareGuid);
    QList<BuyObject> available;
    available.reserve(all.size());
    for (const BuyObject& b : std::as_const(all)) {
        if (b.volume() - b.volumeSold() > 1e-9)   // float-safe remaining > 0
            available.append(b);
    }
    return available;
}

// ── loadAllBuys ───────────────────────────────────────────────────────────────
// Returns all buys for a share regardless of remaining volume.

QList<BuyObject> ModelSaleEdit::loadAllBuys(const QString& shareGuid) const
{
    return m_buyRepo.findByShare(shareGuid);
}

// ── loadAvailableBuysForDepot ─────────────────────────────────────────────────
// Like loadAvailableBuys but filtered to a specific depot number (FIFO, oldest first).

QList<BuyObject> ModelSaleEdit::loadAvailableBuysForDepot(const QString& shareGuid,
                                                           const QString& depotNumber) const
{
    QList<BuyObject> all = loadAvailableBuys(shareGuid);
    const QString depot = depotNumber.trimmed();
    if (depot.isEmpty())
        return all;   // no depot selected yet → show everything
    QList<BuyObject> filtered;
    filtered.reserve(all.size());
    for (const BuyObject& b : std::as_const(all)) {
        if (b.depotNumber().trimmed() == depot)
            filtered.append(b);
    }
    return filtered;
}

// ── loadBrokerage ─────────────────────────────────────────────────────────────

BrokerageObject ModelSaleEdit::loadBrokerage(const QString& saleGuid) const
{
    return m_brokerageRepo.findBySaleGuid(saleGuid);
}

// ── loadBrokerageForBuy ───────────────────────────────────────────────────────

BrokerageObject ModelSaleEdit::loadBrokerageForBuy(const QString& buyGuid) const
{
    return m_brokerageRepo.findByBuyGuid(buyGuid);
}

// ── addSale ───────────────────────────────────────────────────────────────────

bool ModelSaleEdit::addSale(const SaleObject& sale)
{
    m_lastError.clear();

    QSqlDatabase db = QSqlDatabase::database(Database::connectionName());
    if (!db.transaction()) {
        m_lastError = QStringLiteral("Transaktion konnte nicht gestartet werden: ")
                      + db.lastError().text();
        return false;
    }

    // 1. Insert sale (brokerage values are stored in brokerage table only)
    if (!m_saleRepo.insert(sale)) {
        m_lastError = QStringLiteral("Verkauf konnte nicht gespeichert werden: ")
                      + m_saleRepo.lastError().text();
        db.rollback();
        return false;
    }

    // 2. Insert linked brokerage entry
    const QString brokerageGuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const BrokerageObject brokerage(
        brokerageGuid,
        sale.shareGuid(),
        QString(),        // buyGuid — empty for sale-linked brokerage
        sale.guid(),      // saleGuid
        sale.dateTime(),
        sale.provision(),
        sale.brokerFee(),
        sale.traderFee(),
        sale.reduction(),
        sale.document());

    if (!m_brokerageRepo.insert(brokerage)) {
        m_lastError = QStringLiteral("Brokerage-Eintrag konnte nicht gespeichert werden: ")
                      + m_brokerageRepo.lastError().text();
        db.rollback();
        return false;
    }

    // 2b. Sale zurückverlinken (Vorwärts-FK sales.brokerage_guid, den
    // SaleRepository::findByShare()/findByGuid()/findByShareAndYear() für
    // ihren Brokerage-JOIN nutzen — siehe kSelectWithBrokerage). Bugfix
    // 15.07.2026: fehlte bisher komplett, wodurch ein frisch gespeicherter
    // Verkauf beim erneuten Laden über loadSales() immer mit Brokerage 0
    // zurückkam, obwohl der Brokerage-Datensatz selbst korrekt in der DB
    // stand (nur über den — hier ungenutzten — Rückwärts-Link
    // brokerage.sale_guid auffindbar, z.B. via loadBrokerage()).
    if (!m_saleRepo.updateBrokerageGuid(sale.guid(), brokerageGuid)) {
        m_lastError = QStringLiteral("Verkauf konnte nicht mit Brokerage verknüpft werden: ")
                      + m_saleRepo.lastError().text();
        db.rollback();
        return false;
    }

    // 3. Update volumeSold on all contributing buys
    for (const SaleBuyDetail& detail : sale.saleBuyDetails()) {
        BuyObject buy = m_buyRepo.findByGuid(detail.buyGuid());
        if (!buy.isValid()) continue;
        buy.setVolumeSold(buy.volumeSold() + detail.volume());
        if (!m_buyRepo.update(buy)) {
            m_lastError = QStringLiteral("Kauf-Volumen konnte nicht aktualisiert werden: ")
                          + m_buyRepo.lastError().text();
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

// ── updateSale ────────────────────────────────────────────────────────────────

bool ModelSaleEdit::updateSale(const SaleObject& sale)
{
    m_lastError.clear();

    QSqlDatabase db = QSqlDatabase::database(Database::connectionName());
    if (!db.transaction()) {
        m_lastError = QStringLiteral("Transaktion konnte nicht gestartet werden: ")
                      + db.lastError().text();
        return false;
    }

    // Reload old sale to reverse volumeSold changes before re-applying
    SaleObject oldSale = m_saleRepo.findByGuid(sale.guid());

    // Reverse volumeSold for old buy details
    if (oldSale.isValid()) {
        for (const SaleBuyDetail& detail : oldSale.saleBuyDetails()) {
            BuyObject buy = m_buyRepo.findByGuid(detail.buyGuid());
            if (!buy.isValid()) continue;
            buy.setVolumeSold(qMax(0.0, buy.volumeSold() - detail.volume()));
            if (!m_buyRepo.update(buy)) {
                m_lastError = QStringLiteral("Rückgabe Kauf-Volumen fehlgeschlagen: ")
                              + m_buyRepo.lastError().text();
                db.rollback();
                return false;
            }
        }
    }

    // Update sale record (SaleRepository::update also replaces buy details)
    if (!m_saleRepo.update(sale)) {
        m_lastError = QStringLiteral("Verkauf konnte nicht aktualisiert werden: ")
                      + m_saleRepo.lastError().text();
        db.rollback();
        return false;
    }

    // Update brokerage entry
    BrokerageObject existing = m_brokerageRepo.findBySaleGuid(sale.guid());
    if (existing.isValid()) {
        const BrokerageObject updated(
            existing.guid(),
            existing.shareGuid(),
            existing.buyGuid(),
            sale.guid(),
            sale.dateTime(),
            sale.provision(),
            sale.brokerFee(),
            sale.traderFee(),
            sale.reduction(),
            sale.document());
        if (!m_brokerageRepo.update(updated)) {
            m_lastError = QStringLiteral("Brokerage konnte nicht aktualisiert werden: ")
                          + m_brokerageRepo.lastError().text();
            db.rollback();
            return false;
        }

        // Vorwärts-Link absichern (Bugfix 15.07.2026, siehe addSale()):
        // m_saleRepo.update(sale) oben hat sales.brokerage_guid ggf. bereits
        // mit sale.brokerageGuid() überschrieben — falls der Aufrufer dort
        // einen leeren/veralteten Wert übergeben hat, bliebe der Link trotz
        // gültigem Brokerage-Datensatz verwaist. existing.guid() bleibt beim
        // Update unverändert, ist also immer der korrekte Ziel-Wert.
        if (!m_saleRepo.updateBrokerageGuid(sale.guid(), existing.guid())) {
            m_lastError = QStringLiteral("Verkauf konnte nicht mit Brokerage verknüpft werden: ")
                          + m_saleRepo.lastError().text();
            db.rollback();
            return false;
        }
    } else {
        // No brokerage yet — create one
        const QString brokerageGuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const BrokerageObject newBrokerage(
            brokerageGuid,
            sale.shareGuid(),
            QString(),
            sale.guid(),
            sale.dateTime(),
            sale.provision(),
            sale.brokerFee(),
            sale.traderFee(),
            sale.reduction(),
            sale.document());
        if (!m_brokerageRepo.insert(newBrokerage)) {
            m_lastError = QStringLiteral("Brokerage konnte nicht erstellt werden: ")
                          + m_brokerageRepo.lastError().text();
            db.rollback();
            return false;
        }

        // Sale zurückverlinken — derselbe Bugfix wie in addSale() (15.07.2026,
        // siehe dortiger Kommentar): ohne diesen Vorwärts-Link (sales.
        // brokerage_guid) findet SaleRepository::findByShare() etc. den
        // gerade neu angelegten Brokerage-Eintrag beim nächsten Laden nicht.
        if (!m_saleRepo.updateBrokerageGuid(sale.guid(), brokerageGuid)) {
            m_lastError = QStringLiteral("Verkauf konnte nicht mit Brokerage verknüpft werden: ")
                          + m_saleRepo.lastError().text();
            db.rollback();
            return false;
        }
    }

    // Re-apply volumeSold for new buy details
    for (const SaleBuyDetail& detail : sale.saleBuyDetails()) {
        BuyObject buy = m_buyRepo.findByGuid(detail.buyGuid());
        if (!buy.isValid()) continue;
        buy.setVolumeSold(buy.volumeSold() + detail.volume());
        if (!m_buyRepo.update(buy)) {
            m_lastError = QStringLiteral("Kauf-Volumen konnte nicht aktualisiert werden: ")
                          + m_buyRepo.lastError().text();
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

// ── removeSale ────────────────────────────────────────────────────────────────

bool ModelSaleEdit::removeSale(const QString& saleGuid)
{
    m_lastError.clear();

    QSqlDatabase db = QSqlDatabase::database(Database::connectionName());
    if (!db.transaction()) {
        m_lastError = QStringLiteral("Transaktion konnte nicht gestartet werden: ")
                      + db.lastError().text();
        return false;
    }

    // Load sale details before deletion to reverse volumeSold
    SaleObject sale = m_saleRepo.findByGuid(saleGuid);
    if (sale.isValid()) {
        for (const SaleBuyDetail& detail : sale.saleBuyDetails()) {
            BuyObject buy = m_buyRepo.findByGuid(detail.buyGuid());
            if (!buy.isValid()) continue;
            buy.setVolumeSold(qMax(0.0, buy.volumeSold() - detail.volume()));
            if (!m_buyRepo.update(buy)) {
                m_lastError = QStringLiteral("Rückgabe Kauf-Volumen fehlgeschlagen: ")
                              + m_buyRepo.lastError().text();
                db.rollback();
                return false;
            }
        }
    }

    // Remove brokerage first (references sales.guid via FK)
    const BrokerageObject brokerage = m_brokerageRepo.findBySaleGuid(saleGuid);
    if (brokerage.isValid()) {
        if (!m_brokerageRepo.remove(brokerage.guid())) {
            m_lastError = QStringLiteral("Brokerage konnte nicht gelöscht werden: ")
                          + m_brokerageRepo.lastError().text();
            db.rollback();
            return false;
        }
    }

    // Remove the sale (cascade deletes sale_buy_details via FK)
    if (!m_saleRepo.remove(saleGuid)) {
        m_lastError = QStringLiteral("Verkauf konnte nicht gelöscht werden: ")
                      + m_saleRepo.lastError().text();
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

bool ModelSaleEdit::orderNumberExists(const QString& shareGuid,
                                      const QString& orderNumber,
                                      const QString& excludeGuid) const
{
    for (const SaleObject& s : m_saleRepo.findByShare(shareGuid)) {
        if (s.orderNumber() == orderNumber && s.guid() != excludeGuid)
            return true;
    }
    return false;
}

// ── documentExists ────────────────────────────────────────────────────────────

bool ModelSaleEdit::documentExists(const QString& document,
                                   const QString& excludeGuid) const
{
    if (document.trimmed().isEmpty()) return false;

    QSqlDatabase db = QSqlDatabase::database(Database::connectionName());
    QSqlQuery q(db);
    if (excludeGuid.isEmpty()) {
        q.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM sales WHERE document = :doc"));
        q.bindValue(QStringLiteral(":doc"), document.trimmed());
    } else {
        q.prepare(QStringLiteral(
            "SELECT COUNT(*) FROM sales WHERE document = :doc AND guid != :excl"));
        q.bindValue(QStringLiteral(":doc"),  document.trimmed());
        q.bindValue(QStringLiteral(":excl"), excludeGuid);
    }
    if (q.exec() && q.next())
        return q.value(0).toInt() > 0;
    return false;
}

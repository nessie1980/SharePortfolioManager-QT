// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "PresenterShareEdit.h"
#include "../../utils/ShareUpdateRules.h"

#include <QUuid>

// ── Constructor ───────────────────────────────────────────────────────────────

PresenterShareEdit::PresenterShareEdit(IViewShareEdit*  view,
                                       IModelShareEdit* model,
                                       const QString&   shareGuid,
                                       QObject*         parent)
    : QObject(parent)
    , m_view(view)
    , m_model(model)
    , m_shareGuid(shareGuid)
{
    loadAndPopulate();
}

// ── onSave ────────────────────────────────────────────────────────────────────

void PresenterShareEdit::onSave()
{
    const QString error = validateInput();
    if (!error.isEmpty()) {
        m_view->showError(error);
        return;
    }

    // Rebuild the ShareObject from the view's current field values,
    // keeping all fields that are not exposed in the edit form unchanged
    // by loading the original first and then overlaying the editable values.
    ShareObject share = m_model->loadShare(m_shareGuid);
    if (!share.isValid()) {
        m_view->showError(QObject::tr("Aktie konnte nicht geladen werden."));
        return;
    }

    share.setWkn(m_view->wkn().trimmed());
    share.setIsin(m_view->isin().trimmed());
    share.setName(m_view->name().trimmed());
    share.setShareType(m_view->shareType());
    share.setCurrency(QStringLiteral("EUR")); // currency kept from original for now

    // listingDate is stored in addDateTime for compatibility with the C# predecessor
    share.setAddDateTime(m_view->listingDate().toString(Qt::ISODate));

    share.setUpdateType(m_view->updateType());
    share.setMarketPriceParsingType(m_view->marketPriceParsingType());
    share.setMarketPriceUrl(m_view->marketPriceUrl().trimmed());
    share.setDailyValuesParsingType(m_view->dailyValuesParsingType());
    share.setDailyValuesUrl(m_view->dailyValuesUrl().trimmed());
    share.setDetailsWebSiteUrl(m_view->detailsWebsite().trimmed());

    if (!m_model->saveShare(share)) {
        m_view->showError(m_model->lastError());
        return;
    }

    m_view->acceptAndClose();
}

// ── onCancel ──────────────────────────────────────────────────────────────────

void PresenterShareEdit::onCancel()
{
    // Nothing to clean up — view handles the close itself.
}

// ── Sub-dialog signals ────────────────────────────────────────────────────────

void PresenterShareEdit::onEditBuys()
{
    emit openBuysRequested(m_shareGuid);
}

void PresenterShareEdit::onEditSales()
{
    emit openSalesRequested(m_shareGuid);
}

void PresenterShareEdit::onEditDividends()
{
    emit openDividendsRequested(m_shareGuid);
}

void PresenterShareEdit::onEditBrokerages()
{
    emit openBrokeragesRequested(m_shareGuid);
}

// ── loadAndPopulate ───────────────────────────────────────────────────────────

void PresenterShareEdit::loadAndPopulate()
{
    const ShareObject share = m_model->loadShare(m_shareGuid);
    if (!share.isValid()) {
        m_view->showError(QObject::tr("Aktie nicht gefunden."));
        return;
    }

    const double currentVolume = m_model->currentVolume(m_shareGuid);
    m_loadedUpdateType = share.updateType();

    m_view->loadShare(share);
    m_view->setFirstBuyDate(m_model->firstBuyDate(m_shareGuid));
    m_view->setCurrentVolume(currentVolume);

    // 06.08.2026: Muss NACH loadShare() laufen. loadShare() hakt den
    // gespeicherten Update-Typ an; würde die Sperre vorher gesetzt, hätte
    // setChecked() auf einem deaktivierten Radiobutton zwar trotzdem Wirkung,
    // die Reihenfolge wäre aber unnötig fragil.
    m_view->setDailyValuesRequired(
        ShareUpdateRules::requiresDailyValues(currentVolume));

    populateSummary();
}

// ── refreshSummary ────────────────────────────────────────────────────────────

void PresenterShareEdit::refreshSummary()
{
    populateSummary();
}

// ── populateSummary ───────────────────────────────────────────────────────────

void PresenterShareEdit::populateSummary()
{
    m_view->setTotalBuys(
        m_model->totalBuyValue(m_shareGuid),
        m_model->buyCount(m_shareGuid));

    m_view->setTotalSales(
        m_model->totalSaleValue(m_shareGuid),
        m_model->saleCount(m_shareGuid));

    m_view->setTotalProfitLoss(
        m_model->totalProfitLoss(m_shareGuid),
        m_model->saleCount(m_shareGuid));

    m_view->setTotalDividends(
        m_model->totalDividendValue(m_shareGuid),
        m_model->dividendCount(m_shareGuid));

    m_view->setTotalBrokerages(
        m_model->totalBrokerageValue(m_shareGuid),
        m_model->brokerageCount(m_shareGuid));
}

// ── validateInput ─────────────────────────────────────────────────────────────

QString PresenterShareEdit::validateInput() const
{
    if (m_view->wkn().trimmed().isEmpty())
        return QObject::tr("WKN darf nicht leer sein.");
    if (m_view->name().trimmed().isEmpty())
        return QObject::tr("Name darf nicht leer sein.");

    // 06.08.2026: Zweite Verteidigungslinie hinter setDailyValuesRequired().
    // Die View sperrt die unzulässigen Radiobuttons nur — ohne diese Prüfung
    // käme ein gespeicherter, jetzt unzulässiger Wert unverändert zurück in
    // die Datenbank.
    //
    // Geprüft wird ausschliesslich die AKTIVE Änderung: ein unveränderter
    // Altbestandswert darf mitgespeichert werden. Andernfalls liesse sich an
    // einer Aktie mit Bestand und Update-Typ "Keine" gar nichts mehr ändern,
    // auch keine Namenskorrektur — und für ein delistetes Papier, das keine
    // Quelle mehr führt, gäbe es überhaupt keinen Ausweg ausser dem Verkauf.
    // Der Weg bleibt eine Einbahnstrasse: "Beide"/"Tages-Werte" sind wählbar,
    // zurück auf "Markt-Preis"/"Keine" kommt man bei Bestand nicht mehr.
    const ShareUpdateType selected = m_view->updateType();
    if (selected != m_loadedUpdateType
        && !ShareUpdateRules::isUpdateTypeAllowed(
               selected, m_model->currentVolume(m_shareGuid)))
    {
        return QObject::tr(
            "Diese Aktie hat Anteile im Bestand und muss deshalb Tageswerte "
            "abrufen. Ohne Tageswert-Historie fehlt ihr die Kurshistorie und "
            "sie wird aus dem Depotwert-Chart ausgeschlossen.\n\n"
            "Bitte \"Beide\" oder \"Tages-Werte\" wählen.");
    }

    return QString();
}

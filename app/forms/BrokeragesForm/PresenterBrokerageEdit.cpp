// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "PresenterBrokerageEdit.h"

#include <QUuid>
#include <QDateTime>
#include <QObject>

// ── Constructor ───────────────────────────────────────────────────────────────

PresenterBrokerageEdit::PresenterBrokerageEdit(IViewBrokerageEdit*  view,
                                               IModelBrokerageEdit* model,
                                               const QString&       shareGuid,
                                               QObject*             parent)
    : QObject(parent)
    , m_view(view)
    , m_model(model)
    , m_shareGuid(shareGuid)
{
    reloadOverview();
    m_view->clearForm();
    m_view->setButtonStates(/*canRemove=*/false, /*isEdit=*/false, /*readOnly=*/false);
}

// ── onSave ────────────────────────────────────────────────────────────────────

void PresenterBrokerageEdit::onSave()
{
    const bool isEdit = !m_currentGuid.isEmpty();

    if (isLinkedRecord()) {
        // Linked records are fully read-only — nothing may be changed here.
        // The document path comes from the associated buy or sale.
        m_view->showError(QObject::tr(
            "Dieser Eintrag gehört zu einem Kauf oder Verkauf und kann hier nicht "
            "bearbeitet werden."));
        return;
    }

    // Full save — validate required fields first.
    const QString validationError = validateInput();
    if (!validationError.isEmpty()) {
        m_view->markMissingFieldsAsFailed();
        m_view->showError(validationError);
        return;
    }

    // Document duplicate check
    const QString doc = m_view->documentPath().trimmed();
    if (!doc.isEmpty()) {
        if (m_model->documentExists(doc, isEdit ? m_currentGuid : QString())) {
            m_view->showError(QObject::tr(
                "Dieses Dokument ist bereits einem anderen Eintrag zugeordnet."));
            return;
        }
    }

    const QString guid   = isEdit ? m_currentGuid
                                  : QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString dtStr  = m_view->dateTime();

    BrokerageObject brokerage(
        guid,
        m_shareGuid,
        /*buyGuid=*/  QString(),
        /*saleGuid=*/ QString(),
        dtStr,
        m_view->provision(),
        m_view->brokerFee(),
        m_view->traderFee(),
        m_view->reduction(),
        doc
    );

    bool ok = isEdit ? m_model->updateBrokerage(brokerage)
                     : m_model->addBrokerage(brokerage);
    if (!ok) {
        m_view->showError(m_model->lastError());
        return;
    }

    reloadOverview();
    emit dataChanged();
    m_currentGuid.clear();
    m_view->showOverviewTab();
}

// ── onRemove ──────────────────────────────────────────────────────────────────

void PresenterBrokerageEdit::onRemove()
{
    if (m_currentGuid.isEmpty()) return;

    if (isLinkedRecord()) {
        m_view->showError(QObject::tr(
            "Dieser Eintrag gehört zu einem Kauf oder Verkauf und kann hier nicht "
            "gelöscht werden."));
        return;
    }

    if (!m_model->removeBrokerage(m_currentGuid)) {
        m_view->showError(m_model->lastError());
        return;
    }

    reloadOverview();
    emit dataChanged();
    m_currentGuid.clear();
    m_view->showOverviewTab();
}

// ── onReset ───────────────────────────────────────────────────────────────────

void PresenterBrokerageEdit::onReset()
{
    m_currentGuid.clear();
    m_view->clearForm();
    m_view->clearPdfPreview();
    m_view->setButtonStates(/*canRemove=*/false, /*isEdit=*/false, /*readOnly=*/false);
    m_view->showOverviewTab();
}

// ── onClose ───────────────────────────────────────────────────────────────────

void PresenterBrokerageEdit::onClose()
{
    m_view->acceptAndClose();
}

// ── onRowSelected ─────────────────────────────────────────────────────────────

void PresenterBrokerageEdit::onRowSelected(const QString& brokerageGuid)
{
    if (brokerageGuid.isEmpty()) {
        m_currentGuid.clear();
        m_view->clearForm();
        m_view->setButtonStates(false, false, false);
        return;
    }

    // Find record in cached list
    const BrokerageObject* found = nullptr;
    for (const auto& b : m_brokerages) {
        if (b.guid() == brokerageGuid) {
            found = &b;
            break;
        }
    }
    if (!found) return;

    m_currentGuid = brokerageGuid;
    m_view->loadBrokerage(*found);

    const QString doc = found->document();
    if (!doc.isEmpty())
        m_view->openPdfPreview(doc);
    else
        m_view->clearPdfPreview();

    const bool linked    = isLinkedRecord();
    const bool canRemove = !linked;

    m_view->setButtonStates(canRemove, /*isEdit=*/true, /*readOnly=*/linked);
    refreshDerivedValues();
}

// ── onValuesChanged ───────────────────────────────────────────────────────────

void PresenterBrokerageEdit::onValuesChanged()
{
    refreshDerivedValues();
}

// ── onDocumentSelected ────────────────────────────────────────────────────────

void PresenterBrokerageEdit::onDocumentSelected(const QString& path)
{
    m_view->setDocumentPreview(path);
    m_view->openPdfPreview(path);
    onDocumentPathEdited();
}

// ── onDocumentPathEdited ──────────────────────────────────────────────────────

void PresenterBrokerageEdit::onDocumentPathEdited()
{
    const QString doc = m_view->documentPath().trimmed();
    if (doc.isEmpty()) return;

    const QString excludeGuid = m_currentGuid;
    if (m_model->documentExists(doc, excludeGuid)) {
        // Show via showError so the user gets clear feedback; no field-error
        // API defined in the simpler interface.
        m_view->showError(QObject::tr(
            "Dieses Dokument ist bereits einem anderen Eintrag zugeordnet."));
    }
}

// ── onDateEdited ──────────────────────────────────────────────────────────────

void PresenterBrokerageEdit::onDateEdited()
{
    // Validation happens implicitly in validateInput() on save.
    // The sentinel-date check uses the same ISO-string comparison as other forms.
    // Nothing extra to do here; QDateEdit already prevents invalid dates.
}

// ── onFeeEdited ───────────────────────────────────────────────────────────────

void PresenterBrokerageEdit::onFeeEdited(const QString& /*fieldKey*/, double /*value*/)
{
    refreshDerivedValues();
}

// ── Private helpers ───────────────────────────────────────────────────────────

void PresenterBrokerageEdit::reloadOverview()
{
    m_brokerages = m_model->loadBrokerages(m_shareGuid);
    m_view->populateOverview(m_brokerages);
}

void PresenterBrokerageEdit::refreshDerivedValues()
{
    const double gesamtGebuehren = m_view->provision()
                                 + m_view->brokerFee()
                                 + m_view->traderFee();
    m_view->setGesamtGebuehren(gesamtGebuehren);
    m_view->setBrokerageReduction(gesamtGebuehren - m_view->reduction());
}

QString PresenterBrokerageEdit::validateInput() const
{
    // Date must be after the sentinel 2000-01-01
    const QString dtStr = m_view->dateTime();
    if (dtStr.isEmpty() || dtStr <= QStringLiteral("2000-01-01")) {
        return QObject::tr("Bitte geben Sie ein gültiges Datum ein.");
    }

    QStringList missing;
    if (m_view->hasMissingRequiredFields(missing)) {
        return QObject::tr("Es fehlen noch Pflichtangaben.\n"
                           "Die fehlenden Felder sind in der Maske rot markiert.");
    }

    // At least one field must be greater than zero — an entry where all four
    // fields are 0 has no meaningful content.
    const bool anyValue = m_view->provision()  > 0.0
                       || m_view->brokerFee()  > 0.0
                       || m_view->traderFee()  > 0.0
                       || m_view->reduction()  > 0.0;
    if (!anyValue) {
        return QObject::tr("Es muss mindestens ein Wert (Provision, Courtage, "
                           "Handelsplatzgebühr oder Rabatt) größer als 0,00 € sein.");
    }

    return QString();
}

bool PresenterBrokerageEdit::isLinkedRecord() const
{
    if (m_currentGuid.isEmpty()) return false;
    for (const auto& b : m_brokerages) {
        if (b.guid() == m_currentGuid)
            return !b.buyGuid().isEmpty() || !b.saleGuid().isEmpty();
    }
    return false;
}

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "PresenterShareSplitEdit.h"
#include "../../utils/ShareSplitAdjuster.h"

#include <QLocale>
#include <QUuid>
#include <QtGlobal>

#include <utility>

namespace {
/// Sentinel wie in allen anderen Formen: ein Datum <= 01.01.2000 gilt als "nicht gesetzt".
const QDate kDateSentinel(2000, 1, 1);

/// Toleranz für den Vergleich des Split-Faktors gegen 1,0.
constexpr double kFactorEpsilon = 1e-9;
}

// ── Constructor ───────────────────────────────────────────────────────────────

PresenterShareSplitEdit::PresenterShareSplitEdit(IViewShareSplitEdit*  view,
                                                 IModelShareSplitEdit* model,
                                                 const QString&        shareGuid,
                                                 QObject*              parent)
    : QObject(parent)
    , m_view(view)
    , m_model(model)
    , m_shareGuid(shareGuid)
{
    reloadOverview();
    m_view->setButtonStates(/*canRemove=*/false, /*isEdit=*/false);
    refreshFactorPreview();
}

// ── onSave ────────────────────────────────────────────────────────────────────

void PresenterShareSplitEdit::onSave()
{
    const QString error = validateInput();
    if (!error.isEmpty()) {
        m_view->showError(error);
        return;
    }

    const bool isEdit = !m_currentGuid.isEmpty();

    const ShareSplitObject split(
        isEdit ? m_currentGuid : QUuid::createUuid().toString(QUuid::WithoutBraces),
        m_shareGuid,
        m_view->splitDate(),
        m_view->ratioNew(),
        m_view->ratioOld(),
        m_view->pricesAdjusted(),
        m_view->comment().trimmed(),
        m_view->documentPath().trimmed());

    const bool ok = isEdit ? m_model->updateSplit(split)
                           : m_model->addSplit(split);
    if (!ok) {
        m_view->showError(m_model->lastError());
        return;
    }

    emit dataChanged();
    onReset();
}

// ── onRemove ──────────────────────────────────────────────────────────────────

void PresenterShareSplitEdit::onRemove()
{
    if (m_currentGuid.isEmpty())
        return;

    // Den zu löschenden Split aus dem Zwischenspeicher holen — für Meldungstext
    // und für die Bestandsvorschau ohne ihn.
    ShareSplitObject victim;
    QList<ShareSplitObject> without;
    for (const ShareSplitObject& s : std::as_const(m_splits)) {
        if (s.guid() == m_currentGuid)
            victim = s;
        else
            without.append(s);
    }
    if (!victim.isValid())
        return;

    // Bestandsvergleich mit und ohne den Split. Die Datenbank bleibt dabei
    // unangetastet — ein Split ist nichts als eine Rechenvorschrift, gelöscht
    // wird ausschliesslich diese eine Zeile in `share_splits` (siehe
    // ARCHITECTURE.md, "ShareSplitsForm-Details").
    const double volumeWith    = volumeForSplits(m_splits);
    const double volumeWithout = volumeForSplits(without);

    const QLocale loc;
    const QString message =
        QObject::tr("Split %1 wirklich entfernen?\n\n"
                    "Alle Käufe und Verkäufe vor diesem Datum werden danach "
                    "wieder in ihrer Beleg-Stückzahl gerechnet. Der Bestand "
                    "ändert sich dadurch von %2 auf %3 Stück.\n\n"
                    "Die Transaktionen selbst bleiben unverändert — der Split "
                    "kann jederzeit wieder erfasst werden.")
            .arg(describeSplit(victim),
                 loc.toString(volumeWith,    'f', 4),
                 loc.toString(volumeWithout, 'f', 4));

    if (!m_view->confirm(QObject::tr("Split entfernen"), message))
        return;

    if (!m_model->removeSplit(m_currentGuid)) {
        m_view->showError(m_model->lastError());
        return;
    }

    emit dataChanged();
    onReset();
}

// ── onReset ───────────────────────────────────────────────────────────────────

void PresenterShareSplitEdit::onReset()
{
    m_currentGuid.clear();
    m_view->clearForm();
    m_view->clearPdfPreview();
    reloadOverview();
    m_view->setButtonStates(/*canRemove=*/false, /*isEdit=*/false);
    refreshFactorPreview();
}

// ── onClose ───────────────────────────────────────────────────────────────────

void PresenterShareSplitEdit::onClose()
{
    m_view->acceptAndClose();
}

// ── onRowSelected ─────────────────────────────────────────────────────────────

void PresenterShareSplitEdit::onRowSelected(const QString& splitGuid)
{
    if (splitGuid.isEmpty()) {
        onReset();
        return;
    }

    for (const ShareSplitObject& s : std::as_const(m_splits)) {
        if (s.guid() == splitGuid) {
            m_currentGuid = splitGuid;
            m_view->loadSplit(s);

            if (!s.document().isEmpty())
                m_view->openPdfPreview(s.document());
            else
                m_view->clearPdfPreview();

            // Jeder Split ist jederzeit editier- und löschbar, analog
            // DividendForm/BrokeragesForm — keine Letzter-Eintrag-Sperre.
            m_view->setButtonStates(/*canRemove=*/true, /*isEdit=*/true);
            refreshFactorPreview();
            return;
        }
    }
}

// ── onValuesChanged ───────────────────────────────────────────────────────────

void PresenterShareSplitEdit::onValuesChanged()
{
    refreshFactorPreview();
}

// ── onDocumentSelected ────────────────────────────────────────────────────────

void PresenterShareSplitEdit::onDocumentSelected(const QString& path)
{
    m_view->setDocumentPath(path);
    m_view->openPdfPreview(path);
    onDocumentPathEdited();
}

// ── onDocumentPathEdited ──────────────────────────────────────────────────────

void PresenterShareSplitEdit::onDocumentPathEdited()
{
    const QString document = m_view->documentPath().trimmed();
    if (document.isEmpty())
        return;

    // Nur ein Hinweis, keine Blockade — dieselbe Handhabung wie in
    // PresenterBrokerageEdit. Dass zwei Splits denselben Beleg führen, kann
    // legitim sein (eine Bankmitteilung über mehrere Kapitalmassnahmen), es
    // ist aber häufiger ein Versehen.
    if (m_model->documentExists(document, m_currentGuid)) {
        m_view->showError(QObject::tr(
            "Dieses Dokument ist bereits einem anderen Split zugeordnet."));
    }
}

// ── reloadOverview ────────────────────────────────────────────────────────────

void PresenterShareSplitEdit::reloadOverview()
{
    m_splits = m_model->loadSplits(m_shareGuid);
    m_view->populateOverview(m_splits);
}

// ── refreshFactorPreview ──────────────────────────────────────────────────────

void PresenterShareSplitEdit::refreshFactorPreview()
{
    const double newSide = m_view->ratioNew();
    const double oldSide = m_view->ratioOld();

    if (newSide <= 0.0 || oldSide <= 0.0) {
        m_view->setFactorPreview(QStringLiteral("-"));
        return;
    }

    const QString from = formatRatioPart(oldSide);
    const QString to   = formatRatioPart(newSide);

    // Singular/Plural getrennt, damit "aus 10 Stk. wird 1 Stk." beim
    // Reverse-Split grammatikalisch stimmt.
    m_view->setFactorPreview(
        qAbs(newSide - 1.0) < kFactorEpsilon
            ? QObject::tr("aus %1 Stk. wird %2 Stk.").arg(from, to)
            : QObject::tr("aus %1 Stk. werden %2 Stk.").arg(from, to));
}

// ── validateInput ─────────────────────────────────────────────────────────────

QString PresenterShareSplitEdit::validateInput() const
{
    const QDate date = m_view->splitDate();
    if (!date.isValid() || date <= kDateSentinel)
        return QObject::tr("Bitte den Ex-Tag des Splits angeben.");

    const double newSide = m_view->ratioNew();
    const double oldSide = m_view->ratioOld();

    if (newSide <= 0.0 || oldSide <= 0.0)
        return QObject::tr("Beide Seiten des Verhältnisses müssen grösser als 0 sein.");

    // 08.08.2026 (Nessies Entscheidung): ein Verhältnis mit Faktor 1,0 ist
    // fachlich kein Split. Er würde nichts umrechnen und trotzdem in jeder
    // Berechnung mitlaufen — deshalb hart abgewiesen statt still gespeichert.
    if (qAbs((newSide / oldSide) - 1.0) < kFactorEpsilon)
        return QObject::tr("Ein Verhältnis von %1:%2 ist kein Split — "
                           "Stückzahl und Kurs blieben unverändert.")
            .arg(formatRatioPart(newSide), formatRatioPart(oldSide));

    // UNIQUE(share_guid, date) vorweggenommen. Beim Bearbeiten zählt das
    // eigene, unveränderte Datum nicht als Duplikat.
    bool dateIsUnchanged = false;
    if (!m_currentGuid.isEmpty()) {
        for (const ShareSplitObject& s : m_splits) {
            if (s.guid() == m_currentGuid) {
                dateIsUnchanged = (s.date() == date);
                break;
            }
        }
    }
    if (!dateIsUnchanged && m_model->existsForDate(m_shareGuid, date)) {
        return QObject::tr("Für diese Aktie ist am %1 bereits ein Split erfasst.")
            .arg(QLocale().toString(date, QLocale::ShortFormat));
    }

    return QString();
}

// ── volumeForSplits ───────────────────────────────────────────────────────────

double PresenterShareSplitEdit::volumeForSplits(const QList<ShareSplitObject>& splits) const
{
    double total = 0.0;
    const QList<OpenBuyLot> lots = m_model->openLots(m_shareGuid);
    for (const OpenBuyLot& lot : lots)
        total += ShareSplitAdjuster::adjustedVolume(lot.remainingVolume, splits, lot.date);
    return total;
}

// ── describeSplit / formatRatioPart ───────────────────────────────────────────

QString PresenterShareSplitEdit::describeSplit(const ShareSplitObject& split)
{
    return QObject::tr("%1:%2 vom %3")
        .arg(formatRatioPart(split.ratioNew()),
             formatRatioPart(split.ratioOld()),
             QLocale().toString(split.date(), QLocale::ShortFormat));
}

QString PresenterShareSplitEdit::formatRatioPart(double value)
{
    const QLocale loc;
    const double rounded = static_cast<double>(qRound(value));
    // Ganze Verhältnisse ohne Nachkommastellen ("20:1" statt "20,00:1,00") —
    // gebrochene Verhältnisse gibt es (z. B. 3:2), deshalb kein pauschales
    // Abschneiden.
    if (qAbs(value - rounded) < 1e-9)
        return loc.toString(qRound(value));
    return loc.toString(value, 'f', 2);
}

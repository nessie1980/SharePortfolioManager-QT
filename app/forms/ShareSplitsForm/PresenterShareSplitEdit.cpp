// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "PresenterShareSplitEdit.h"
#include "../../utils/ShareSplitAdjuster.h"
#include "../../utils/SplitPriceJumpDetector.h"
#include "../../utils/ValueFormatter.h"

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

    // Rückfrage bei einem Ex-Tag in der Zukunft (Punkt 5, siehe
    // ARCHITECTURE.md). Steht VOR der Historienprüfung: liegt der Ex-Tag in
    // der Zukunft, findet checkAgainstHistory() ohnehin praktisch nie einen
    // Widerspruch — Verkäufe nach dem heutigen Tag gibt es nicht. In der
    // Praxis erscheint damit genau ein Dialog und nie zwei hintereinander.
    // Und wer verneint, korrigiert das Datum; die Verhältnisprüfung läuft
    // danach gegen den richtigen Stichtag.
    if (!confirmSaveDespiteFutureExDate(split.date()))
        return;

    // Plausibilitätsprüfung des Verhältnisses gegen die eigenen Belege
    // (Punkt 2, siehe ARCHITECTURE.md, "Plausibilitätsprüfung des
    // Split-Verhältnisses"). Rückfrage statt Blockade — die Begründung steht
    // im Klassenkopf.
    if (!confirmSaveDespiteConflict(split))
        return;

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
    // Nicht const: der Konflikthinweis unten wird gegebenenfalls angehängt.
    QString message =
        QObject::tr("Split %1 wirklich entfernen?\n\n"
                    "Alle Käufe und Verkäufe vor diesem Datum werden danach "
                    "wieder in ihrer Beleg-Stückzahl gerechnet. Der Bestand "
                    "ändert sich dadurch von %2 auf %3 Stück.\n\n"
                    "Die Transaktionen selbst bleiben unverändert — der Split "
                    "kann jederzeit wieder erfasst werden.")
            .arg(describeSplit(victim),
                 loc.toString(volumeWith,    'f', 4),
                 loc.toString(volumeWithout, 'f', 4));

    // Ohne den Split fallen alle Belege vor seinem Ex-Tag auf ihre
    // Beleg-Stückzahl zurück — das kann die Verkaufshistorie unschlüssig
    // machen (Punkt 2, siehe ARCHITECTURE.md).
    const QString conflictHint = removalConflictHint(without, victim);
    if (!conflictHint.isEmpty())
        message += QStringLiteral("\n\n") + conflictHint;

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

// ── onCheckPriceJump ──────────────────────────────────────────────────────────

void PresenterShareSplitEdit::onCheckPriceJump()
{
    const QDate date = m_view->splitDate();
    if (!date.isValid() || date <= kDateSentinel) {
        m_view->showError(QObject::tr("Bitte zuerst den Ex-Tag angeben."));
        return;
    }

    const double newSide = m_view->ratioNew();
    const double oldSide = m_view->ratioOld();
    if (newSide <= 0.0 || oldSide <= 0.0) {
        m_view->showError(QObject::tr("Bitte zuerst ein gültiges Verhältnis angeben."));
        return;
    }
    const double factor = newSide / oldSide;

    // Nachbar-Splits derselben Aktie begrenzen das Suchfenster — der gerade
    // bearbeitete Split selbst zählt dabei nicht als eigener Nachbar (sonst
    // würde das Fenster beim Editieren auf das eigene, unter Umständen noch
    // ungespeicherte Datum kollabieren).
    QDate previousSplitDate;
    QDate nextSplitDate;
    for (const ShareSplitObject& s : std::as_const(m_splits)) {
        if (s.guid() == m_currentGuid)
            continue;
        if (s.date() < date && (!previousSplitDate.isValid() || s.date() > previousSplitDate))
            previousSplitDate = s.date();
        if (s.date() > date && (!nextSplitDate.isValid() || s.date() < nextSplitDate))
            nextSplitDate = s.date();
    }

    const QDate rangeStart = date.addDays(-SplitPriceJumpDetector::kDefaultMaxLookbackDays);
    const QDate rangeEnd   = date.addDays( SplitPriceJumpDetector::kDefaultMaxLookbackDays);
    const QList<DailyValuesObject> dailyValues =
        m_model->dailyValuesInRange(m_shareGuid, rangeStart, rangeEnd);

    const SplitPriceJumpDetector::Outcome outcome = SplitPriceJumpDetector::detect(
        dailyValues, date, factor, previousSplitDate, nextSplitDate);

    const QLocale loc;

    // Gegenprobe des Verhältnisses (Punkt 3 der Split-Plausibilitätsprüfung,
    // 22.08.2026, siehe ARCHITECTURE.md). Der gemessene Sprung ist die
    // einzige Gegenprobe, die ein ZU GROSSES Verhältnis bemerken kann — die
    // Bestandsprüfungen können das nicht, weil ein zu grosses Verhältnis nie
    // eine Unterdeckung erzeugt.
    QString ratioHint;
    if (outcome.ratioMismatch) {
        ratioHint = QObject::tr(
            "\nDer gemessene Sprung passt eher zum Verhältnis %1 als zum "
            "eingetragenen %2 — bitte gegen die Bankmitteilung prüfen.")
            .arg(describeImpliedRatio(outcome.impliedFactor),
                 formatRatioPart(newSide) + QStringLiteral(":")
                     + formatRatioPart(oldSide));
    }

    switch (outcome.result) {
    case SplitPriceJumpDetector::Result::Adjusted:
        m_view->setPricesAdjusted(true);
        m_view->setPriceJumpHint(QObject::tr(
            "Kein Kurssprung erkannt (%1: %2 → %3: %4) — Kurshistorie scheint "
            "bereits bereinigt. Haken gesetzt.")
                .arg(loc.toString(outcome.dateBefore, QLocale::ShortFormat),
                     ValueFormatter::formatPrice(outcome.priceBefore),
                     loc.toString(outcome.dateAfter, QLocale::ShortFormat),
                     ValueFormatter::formatPrice(outcome.priceAfter)),
            IViewShareSplitEdit::PriceJumpTone::Adopted);
        break;
    case SplitPriceJumpDetector::Result::NotAdjusted:
        m_view->setPricesAdjusted(false);
        // Der Haken wird auch bei einem Verhältnis-Verdacht gesetzt: ob die
        // Kurshistorie bereinigt ist, ist eine ANDERE Frage als das
        // Verhältnis. Nur die Einfärbung wechselt, damit die Zeile nicht
        // "alles übernommen" signalisiert, während etwas zu prüfen ist.
        m_view->setPriceJumpHint(QObject::tr(
            "Kurssprung erkannt (%1: %2 → %3: %4, Faktor ≈ %5) — Kurshistorie "
            "scheint nicht bereinigt. Haken entfernt.")
                .arg(loc.toString(outcome.dateBefore, QLocale::ShortFormat),
                     ValueFormatter::formatPrice(outcome.priceBefore),
                     loc.toString(outcome.dateAfter, QLocale::ShortFormat),
                     ValueFormatter::formatPrice(outcome.priceAfter),
                     loc.toString(outcome.observedRatio, 'f', 1))
                + ratioHint,
            outcome.ratioMismatch
                ? IViewShareSplitEdit::PriceJumpTone::ManualDecisionNeeded
                : IViewShareSplitEdit::PriceJumpTone::Adopted);
        break;
    case SplitPriceJumpDetector::Result::Ambiguous:
        m_view->setPriceJumpHint(QObject::tr(
            "Ergebnis nicht eindeutig (%1: %2 → %3: %4) — bitte manuell "
            "entscheiden, ob die Kurshistorie bereits bereinigt ist.")
                .arg(loc.toString(outcome.dateBefore, QLocale::ShortFormat),
                     ValueFormatter::formatPrice(outcome.priceBefore),
                     loc.toString(outcome.dateAfter, QLocale::ShortFormat),
                     ValueFormatter::formatPrice(outcome.priceAfter))
                + ratioHint,
            IViewShareSplitEdit::PriceJumpTone::ManualDecisionNeeded);
        break;
    case SplitPriceJumpDetector::Result::InsufficientData:
        m_view->setPriceJumpHint(QObject::tr(
            "Nicht genug Kursdaten im Zeitraum um den Ex-Tag vorhanden — "
            "bitte manuell entscheiden, ob die Kurshistorie bereits bereinigt "
            "ist."),
            IViewShareSplitEdit::PriceJumpTone::ManualDecisionNeeded);
        break;
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
    if (!dateIsUnchangedForLoadedSplit(date)
        && m_model->existsForDate(m_shareGuid, date)) {
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

// ── confirmSaveDespiteConflict ────────────────────────────────────────────────

bool PresenterShareSplitEdit::confirmSaveDespiteConflict(const ShareSplitObject& candidate) const
{
    // Resultierende Liste: alle übrigen Splits plus der Kandidat. Beim
    // Bearbeiten fliegt der alte Stand desselben Splits heraus, sonst träte
    // er gegen seine eigene neue Fassung an (gleiche Ausklammerung wie in
    // onCheckPriceJump()).
    QList<ShareSplitObject> resulting;
    for (const ShareSplitObject& s : std::as_const(m_splits)) {
        if (s.guid() != m_currentGuid)
            resulting.append(s);
    }
    resulting.append(candidate);

    const SplitHistoryConflict conflict = SplitRatioChecker::checkAgainstHistory(
        resulting, candidate.date(),
        m_model->loadBuys(m_shareGuid), m_model->loadSales(m_shareGuid));

    if (!conflict.hasConflict)
        return true;

    QString message = QObject::tr("Mit diesem Split geht die Verkaufshistorie nicht auf.\n\n%1")
                          .arg(describeConflict(conflict));

    if (conflict.suspicion.hasProposal) {
        const SplitRatioSuspicion& s = conflict.suspicion;
        const QString proposed = formatRatioPart(s.proposedRatioNew)
                                 + QStringLiteral(":")
                                 + formatRatioPart(s.proposedRatioOld);
        // Bank-Schreibweise: alte Seite zu ZUSÄTZLICHEN Stücken, im Beispiel
        // "1:19" für das Umrechnungsverhältnis 20:1.
        const QString bankNotation = formatRatioPart(s.proposedRatioOld)
                                     + QStringLiteral(":")
                                     + formatRatioPart(s.proposedRatioNew - s.proposedRatioOld);

        message += QObject::tr(
            "\n\nMit dem Verhältnis %1 ergäben sich genau %2 Stk., die Rechnung "
            "ginge dann exakt auf. Bankmitteilungen nennen das "
            "Zuteilungsverhältnis häufig als \"%3\" — das sind die ZUSÄTZLICHEN "
            "Stücke je gehaltenem Stück, nicht das von der Anwendung erwartete "
            "Umrechnungsverhältnis.")
            .arg(proposed,
                 QLocale().toString(s.proposedAvailableToday, 'f', 4),
                 bankNotation);
    } else {
        message += QObject::tr(
            "\n\nMöglich ist auch, dass die Kaufhistorie unvollständig ist — "
            "etwa nach einem Depotübertrag von einer anderen Bank.");
    }

    message += QObject::tr("\n\nTrotzdem speichern?");

    return m_view->confirm(QObject::tr("Split speichern"), message);
}

// ── confirmSaveDespiteFutureExDate ────────────────────────────────────────────

bool PresenterShareSplitEdit::confirmSaveDespiteFutureExDate(const QDate& date) const
{
    // Heute oder früher: keine Frage. Das Datumsfeld startet seit dem
    // 25.08.2026 unbelegt (Sentinel statt "heute", siehe ViewShareSplitEdit),
    // ein eingetragenes Datum ist damit immer eine bewusste Eingabe. Genau
    // deshalb entfällt hier die ursprünglich erwogene Warnung für den
    // heutigen Tag: sie wäre nur gegen die alte Vorbelegung gerichtet
    // gewesen, die es nicht mehr gibt.
    if (!date.isValid() || date <= QDate::currentDate())
        return true;

    // Keine Wiederholung für ein bereits bestätigtes Datum — sonst käme die
    // Frage bei jeder Kommentar- oder Belegänderung an einem angekündigten
    // Split erneut.
    if (dateIsUnchangedForLoadedSplit(date))
        return true;

    return m_view->confirm(
        QObject::tr("Split speichern"),
        QObject::tr("Der Ex-Tag %1 liegt in der Zukunft.\n\n"
                    "Das ist zulässig — ein angekündigter Split darf sofort "
                    "erfasst werden und bleibt bis zu seinem Ex-Tag ohne "
                    "Wirkung auf Bestände und Kurse.\n\n"
                    "Bitte nur prüfen, ob das Datum wirklich so auf der "
                    "Bankmitteilung steht.\n\n"
                    "Speichern?")
            .arg(QLocale().toString(date, QLocale::ShortFormat)));
}

// ── dateIsUnchangedForLoadedSplit ─────────────────────────────────────────────

bool PresenterShareSplitEdit::dateIsUnchangedForLoadedSplit(const QDate& date) const
{
    if (m_currentGuid.isEmpty())
        return false;

    for (const ShareSplitObject& s : m_splits) {
        if (s.guid() == m_currentGuid)
            return s.date() == date;
    }
    return false;
}

// ── removalConflictHint ───────────────────────────────────────────────────────

QString PresenterShareSplitEdit::removalConflictHint(const QList<ShareSplitObject>& without,
                                                     const ShareSplitObject&        victim) const
{
    const SplitHistoryConflict conflict = SplitRatioChecker::checkAgainstHistory(
        without, victim.date(),
        m_model->loadBuys(m_shareGuid), m_model->loadSales(m_shareGuid));

    if (!conflict.hasConflict)
        return QString();

    // conflict.suspicion bleibt hier bewusst ungenutzt: der fragliche Split
    // ist gar nicht mehr in der Liste, ein Verhältnis-Vorschlag zu einem
    // ANDEREN Split wäre in der Löschabfrage nur verwirrend.
    return QObject::tr("Achtung: Danach geht die Verkaufshistorie nicht mehr auf.\n%1")
        .arg(describeConflict(conflict));
}

// ── describeConflict ──────────────────────────────────────────────────────────

QString PresenterShareSplitEdit::describeConflict(const SplitHistoryConflict& conflict)
{
    const QLocale loc;
    const QString depot = conflict.depotNumber.isEmpty()
        ? QObject::tr("ohne Depotnummer")
        : QObject::tr("Depot %1").arg(conflict.depotNumber);

    return QObject::tr("Bis zum %1 sind %2 Stk. verkauft, gekauft wurden bis dahin "
                       "nur %3 Stk. (heutige Skala, %4).")
        .arg(loc.toString(conflict.conflictDate, QLocale::ShortFormat),
             loc.toString(conflict.requiredToday,  'f', 4),
             loc.toString(conflict.availableToday, 'f', 4),
             depot);
}

// ── describeSplit / formatRatioPart ───────────────────────────────────────────

QString PresenterShareSplitEdit::describeSplit(const ShareSplitObject& split)
{
    return QObject::tr("%1:%2 vom %3")
        .arg(formatRatioPart(split.ratioNew()),
             formatRatioPart(split.ratioOld()),
             QLocale().toString(split.date(), QLocale::ShortFormat));
}

QString PresenterShareSplitEdit::describeImpliedRatio(double factor)
{
    if (factor <= 0.0)
        return QString();

    // Faktor >= 1 ist ein normaler Split ("20:1"), darunter ein
    // Reverse-Split, den man als "1:10" schreibt.
    if (factor >= 1.0)
        return formatRatioPart(factor) + QStringLiteral(":1");

    return QStringLiteral("1:") + formatRatioPart(1.0 / factor);
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

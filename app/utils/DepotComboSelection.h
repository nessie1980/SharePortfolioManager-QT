// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QComboBox>
#include <QCoreApplication>
#include <QSignalBlocker>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QString>
#include <QVariant>

/**
 * @brief Wählt beim Laden eines gespeicherten Datensatzes das Depot in der
 * Depot-Auswahl aus — und macht eine unbekannte Depotnummer sichtbar.
 *
 * Feature 18.09.2026, Nachbefund zum Bugfix "Depotnummern im alten Format
 * (Nummer - Bank)". `loadBuy()`, `loadSale()` und `loadDividend()` hängten
 * eine Depotnummer, die nicht in `Documents.xml` steht, still als neuen
 * Eintrag an die Auswahl und wählten ihn aus — mit dem Wert als item data.
 * Der Datensatz sah dadurch völlig normal aus und liess sich unverändert
 * wieder speichern. Genau deshalb fiel "8006189848 - ING diba" monatelang
 * niemandem auf. Beim Einlesen eines Belegs (`setFieldOk()`) ist ein
 * unbekannter Wert dagegen schon seit 27.08.2026 ein Fehler.
 *
 * Jetzt gilt beim Laden dasselbe:
 * - Der gespeicherte Wert bleibt SICHTBAR — als Eintrag
 *   `<Wert> (nicht in Documents.xml)`, damit der Benutzer sieht, was
 *   tatsächlich gespeichert ist.
 * - Der Eintrag trägt aber KEINE item data. `depotNumber()` der Views liest
 *   item data und liefert damit einen leeren String; die bestehende
 *   Pflichtfeldprüfung (`hasMissingRequiredFields()`, `validateInput()`)
 *   sperrt das Speichern, und `onDepotNumberEdited()` des Presenters setzt
 *   das rote Symbol — ohne eine neue Prüfregel.
 * - Der Eintrag ist deaktiviert: nach Auswahl eines gültigen Depots lässt er
 *   sich nicht wieder anwählen.
 * - Vor jedem Laden und in `clearForm()` wird ein solcher Eintrag entfernt,
 *   damit sich beim Durchblättern keine Einträge ansammeln.
 *
 * Header-only und als eine Stelle für alle drei Views, weil der Ladecode
 * dort bisher dreimal nahezu zeichengleich stand.
 */
namespace DepotComboSelection
{

/// Rolle, mit der ein Eintrag als "unbekannte Depotnummer" markiert ist.
/// Qt::UserRole selbst ist die item data (die Depotnummer) und bleibt bei
/// diesem Eintrag bewusst leer.
constexpr int UnknownValueRole = Qt::UserRole + 1;

/// Rolle mit dem gespeicherten Rohwert eines Hinweis-Eintrags, damit
/// selectedUnknownValue() ihn nicht aus dem übersetzten Anzeigetext
/// zurückgewinnen muss.
constexpr int UnknownRawValueRole = Qt::UserRole + 2;

/// Ergebnis von select().
enum class Result
{
    Matched,   ///< Gespeicherte Nummer steht in der Auswahl und ist ausgewählt.
    Empty,     ///< Keine Nummer gespeichert — Platzhalter (Index 0) ausgewählt.
    Unknown    ///< Nummer steht nicht in Documents.xml — Hinweis-Eintrag ausgewählt.
};

/// Anzeigetext des Hinweis-Eintrags für eine unbekannte Depotnummer.
inline QString unknownDisplayText(const QString& value)
{
    return QCoreApplication::translate("DepotComboSelection",
                                       "%1 (nicht in Documents.xml)").arg(value);
}

/// Tooltip für Feldsymbol und Eintrag bei einer unbekannten Depotnummer.
inline QString unknownTooltip(const QString& value)
{
    return QCoreApplication::translate(
        "DepotComboSelection",
        "Gespeicherte Depotnummer „%1“ ist in Documents.xml nicht hinterlegt. "
        "Bitte ein gültiges Depot wählen oder Documents.xml ergänzen.").arg(value);
}

/**
 * @brief Entfernt alle Hinweis-Einträge für unbekannte Depotnummern.
 * @param combo  Depot-Auswahl; nullptr wird ignoriert.
 */
inline void removeUnknownEntries(QComboBox* combo)
{
    if (!combo)
        return;
    QSignalBlocker block(combo);
    for (int i = combo->count() - 1; i >= 0; --i) {
        if (combo->itemData(i, UnknownValueRole).toBool())
            combo->removeItem(i);
    }
}

/**
 * @brief Gespeicherter Wert, wenn gerade ein Hinweis-Eintrag ausgewählt ist.
 *
 * Für den Tooltip des Feldsymbols in `setFieldError()` der Views.
 *
 * @param combo  Depot-Auswahl.
 * @return Der unbekannte Wert, oder ein leerer String, wenn ein gewöhnlicher
 *         Eintrag (oder der Platzhalter) ausgewählt ist.
 */
inline QString selectedUnknownValue(const QComboBox* combo)
{
    if (!combo || combo->currentIndex() < 0)
        return QString();
    const int idx = combo->currentIndex();
    if (!combo->itemData(idx, UnknownValueRole).toBool())
        return QString();
    return combo->itemData(idx, UnknownRawValueRole).toString();
}

/**
 * @brief Wählt den gespeicherten Wert @p storedValue in @p combo aus.
 *
 * Vergleicht getrimmt mit der item data der Einträge, wie der bisherige
 * Ladecode. Löst keine Signale aus — die Symbole setzt der Presenter über
 * `onDepotNumberEdited()` im Anschluss an das Laden (siehe
 * `onRowSelected()` der drei Presenter).
 *
 * @param combo        Depot-Auswahl; Index 0 ist der Platzhalter
 *                     "— bitte wählen —".
 * @param storedValue  Depotnummer des geladenen Datensatzes.
 * @return Siehe Result.
 */
inline Result select(QComboBox* combo, const QString& storedValue)
{
    if (!combo)
        return Result::Empty;

    removeUnknownEntries(combo);

    QSignalBlocker block(combo);
    const QString value = storedValue.trimmed();

    if (value.isEmpty()) {
        // Nichts gespeichert: ausdrücklich auf den Platzhalter, statt die
        // Auswahl des zuvor geladenen Datensatzes stehen zu lassen.
        combo->setCurrentIndex(combo->count() > 0 ? 0 : -1);
        return Result::Empty;
    }

    for (int i = 0; i < combo->count(); ++i) {
        const QVariant data = combo->itemData(i);
        if (data.isValid() && data.toString().trimmed() == value) {
            combo->setCurrentIndex(i);
            return Result::Matched;
        }
    }

    // Unbekannt: sichtbar, aber ohne item data und nicht wieder anwählbar.
    combo->addItem(unknownDisplayText(value));
    const int idx = combo->count() - 1;
    combo->setItemData(idx, true, UnknownValueRole);
    combo->setItemData(idx, unknownTooltip(value), Qt::ToolTipRole);
    combo->setItemData(idx, value, UnknownRawValueRole);
    if (auto* model = qobject_cast<QStandardItemModel*>(combo->model())) {
        if (QStandardItem* item = model->item(idx))
            item->setEnabled(false);
    }
    combo->setCurrentIndex(idx);
    return Result::Unknown;
}

} // namespace DepotComboSelection

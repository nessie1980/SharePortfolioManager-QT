// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../../models/ShareSplitObject.h"
#include "../../models/DailyValuesObject.h"

#include <QDate>
#include <QList>
#include <QString>

/**
 * @brief Ein offener Kauf-Posten in Beleg-Skala.
 *
 * Eingabe für die Löschfolgen-Abschätzung in PresenterShareSplitEdit::onRemove()
 * (siehe ARCHITECTURE.md, "ShareSplitsForm-Details"). Enthält bewusst nur die
 * beiden Grössen, die für die Umrechnung gebraucht werden — Datum (bestimmt den
 * anzuwendenden Split-Faktor) und verbliebene Stückzahl laut Beleg. Preise,
 * Gebühren und Steuern spielen keine Rolle: ein Split lässt sie unberührt.
 */
struct OpenBuyLot
{
    QDate  date;                    ///< Kaufdatum laut Beleg
    double remainingVolume = 0.0;   ///< `volume - volumeSold` in Beleg-Skala
};

/**
 * @brief Abstraktes Model-Interface für den Dialog "Aktiensplits".
 *
 * Deckt reines CRUD auf `share_splits` ab, ergänzt um zwei Lesezugriffe, die
 * der Presenter für Duplikat-Prüfung (existsForDate()) und Löschfolgen-Anzeige
 * (openLots()) braucht. Bewusst ohne jeden Qt-Widget-Bezug, damit der Presenter
 * gegen einen Stub testbar bleibt.
 */
class IModelShareSplitEdit
{
public:
    virtual ~IModelShareSplitEdit() = default;

    // ── Read ──────────────────────────────────────────────────────────────

    /**
     * @brief Alle Splits einer Aktie, aufsteigend nach Datum.
     * @param shareGuid  GUID der Aktie.
     */
    virtual QList<ShareSplitObject> loadSplits(const QString& shareGuid) const = 0;

    /**
     * @brief Prüft, ob für die Aktie am angegebenen Tag bereits ein Split existiert.
     *
     * Gegenstück zur `UNIQUE(share_guid, date)`-Bedingung des Schemas — der
     * Presenter fängt den Fall damit vor dem SQL-Fehler ab.
     */
    virtual bool existsForDate(const QString& shareGuid, const QDate& date) const = 0;

    /**
     * @brief Alle Käufe mit verbliebener Stückzahl > 0, in Beleg-Skala.
     *
     * Grundlage für die Angabe "Bestand ändert sich von X auf Y" in der
     * Löschabfrage. Verkäufe fliessen bereits über `volume_sold` ein und
     * brauchen deshalb keinen eigenen Abruf.
     */
    virtual QList<OpenBuyLot> openLots(const QString& shareGuid) const = 0;

    /**
     * @brief Prüft, ob ein Dokument bereits einem anderen Split zugeordnet ist.
     *
     * Ergänzt 08.08.2026. Bewusst nur innerhalb von `share_splits`, nicht
     * tabellenübergreifend (Nessies Entscheidung 08.08.2026) — dieselbe
     * Reichweite wie die Prüfung in BrokeragesForm.
     *
     * @param document    Zu prüfender Pfad.
     * @param excludeGuid GUID des gerade bearbeiteten Splits, leer beim Anlegen.
     */
    virtual bool documentExists(const QString& document,
                                const QString& excludeGuid = QString()) const = 0;

    /**
     * @brief Kurshistorie der Aktie in einem Datumsbereich (inklusive).
     *
     * Ergänzt 13.08.2026 für `SplitPriceJumpDetector` (Prüfen-Knopf beim
     * "Kurshistorie bereits bereinigt"-Haken, siehe ARCHITECTURE.md,
     * "Split-Verhaeltnis: Notation der Bankmitteilungen"). Reine
     * Weiterleitung an `DailyValuesRepository::findByShareAndDateRange()`.
     *
     * @param shareGuid  GUID der Aktie.
     * @param from       Startdatum (inklusive).
     * @param to         Enddatum (inklusive).
     */
    virtual QList<DailyValuesObject> dailyValuesInRange(const QString& shareGuid,
                                                        const QDate& from,
                                                        const QDate& to) const = 0;

    // ── Create / Update / Delete ──────────────────────────────────────────

    virtual bool addSplit(const ShareSplitObject& split)    = 0;
    virtual bool updateSplit(const ShareSplitObject& split) = 0;
    virtual bool removeSplit(const QString& guid)           = 0;

    // ── Error handling ────────────────────────────────────────────────────

    /** Lesbare Beschreibung des letzten Fehlers. */
    virtual QString lastError() const = 0;
};

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../models/BuyObject.h"
#include "../models/ShareSplitObject.h"

#include <QDate>
#include <QList>
#include <QString>

/**
 * @brief Ergebnis-Zeile einer FIFO-Zuteilung: das einem Kauf zugeteilte
 * Stück eines Verkaufs.
 *
 * `volume` liegt in der Beleg-Skala DES REFERENZIERTEN KAUFS vor — passt
 * also direkt zu `buyPrice` (`buy.price()`, unverändert) und kann von
 * `ModelSaleEdit` unverändert für `buy.volumeSold() += detail.volume()`
 * verwendet werden. Aufrufer, die das Ergebnis anzeigen wollen, rechnen bei
 * Bedarf selbst über `ShareSplitAdjuster` auf eine andere Skala um (siehe
 * `ViewSaleEdit::onShowDetails()`).
 */
struct FifoAllocationRow
{
    QString buyGuid;
    QString buyDateTime;
    double  volume   = 0.0; ///< Beleg-Skala des referenzierten Kaufs
    double  buyPrice = 0.0; ///< Beleg-Kaufpreis, unverändert
};

/**
 * @brief Verteilt eine Verkaufsmenge FIFO über verfügbare Käufe, unter
 * Berücksichtigung von Aktiensplits zwischen Kauf- und Verkaufsdatum.
 *
 * Zustandslos und vollständig datenbankfrei — Käufe und Splits kommen als
 * Parameter herein, gleicher Stil wie `ShareSplitAdjuster`/
 * `PortfolioSeriesCalculator`. Ersetzt die vormals dreifach duplizierte
 * FIFO-Schleife in `PresenterSaleEdit::onSave()`,
 * `PresenterSaleEdit::refreshDerivedValues()` und
 * `ViewSaleEdit::onShowDetails()` (siehe ARCHITECTURE.md, "Offene Punkte",
 * "Aktiensplits werden nicht behandelt", Phase 2c).
 *
 * ### Warum zwei Skalen im Spiel sind
 *
 * Verkaufsmenge und Kauf-Restmenge (`buy.volume() - buy.volumeSold()`)
 * liegen jeweils in der Beleg-Skala IHRES EIGENEN Datums vor. Liegt
 * zwischen Kauf- und Verkaufsdatum ein Split, sind das zwei verschiedene
 * Skalen — ein direkter Vergleich (wie die alte Schleife ihn machte) wäre
 * dann falsch, und zwar genau um den Split-Faktor. `allocate()` rechnet
 * beide Seiten intern über `ShareSplitAdjuster::adjustedVolume()` auf die
 * heutige Skala um, vergleicht dort, und rechnet das zugeteilte Stück je
 * Kauf über `ShareSplitAdjuster::belegVolume()` wieder zurück auf DESSEN
 * EIGENE Beleg-Skala — siehe `FifoAllocationRow`.
 *
 * Ohne Splits liefert `ShareSplitAdjuster` überall den Faktor 1,0; das
 * Ergebnis ist dann bitgenau wie vor dieser Klasse.
 */
class SaleFifoAllocator
{
public:
    SaleFifoAllocator() = delete;

    /**
     * @brief Verteilt @p saleVolume FIFO über @p availableBuysOldestFirst.
     *
     * @param saleVolume               Verkaufsmenge, Beleg-Skala des Verkaufsdatums.
     * @param saleDate                 Datum des Verkaufs.
     * @param availableBuysOldestFirst Verfügbare Käufe (Restvolumen > 0),
     *        aufsteigend nach Datum — z. B. `loadAvailableBuysForDepot()`
     *        bzw. `loadAvailableBuysForDepotExcludingSale()`.
     * @param splits                   Alle Splits der betroffenen Aktie.
     * @return Zuteilungszeilen, je Kauf in dessen eigener Beleg-Skala.
     *         Bricht ab, sobald die Verkaufsmenge vollständig zugeteilt ist;
     *         reicht das verfügbare Volumen nicht aus, bleibt der Rest
     *         offen (unverändertes Verhalten wie in der vormaligen
     *         Einzel-Implementierungen).
     */
    static QList<FifoAllocationRow> allocate(
        double saleVolume, const QDate& saleDate,
        const QList<BuyObject>& availableBuysOldestFirst,
        const QList<ShareSplitObject>& splits);

    /**
     * @brief Summe des verfügbaren Volumens aller @p availableBuysOldestFirst,
     * umgerechnet auf die heutige Skala.
     *
     * Öffentlich für Aufrufer, die im Fehlerfall den konkreten Fehlbetrag in
     * einer Meldung ausweisen wollen (siehe `isSaleVolumeCovered()`,
     * `PresenterSaleEdit::validateInput()`).
     *
     * @param availableBuysOldestFirst Verfügbare Käufe (Restvolumen > 0).
     * @param splits                   Alle Splits der betroffenen Aktie.
     * @return Summe von `buy.volume() - buy.volumeSold()` je Kauf, jeweils
     *         über `ShareSplitAdjuster::adjustedVolume()` auf die heutige
     *         Skala umgerechnet.
     */
    static double totalAvailableVolumeToday(
        const QList<BuyObject>& availableBuysOldestFirst,
        const QList<ShareSplitObject>& splits);

    /**
     * @brief Prüft, ob @p saleVolume durch @p availableBuysOldestFirst gedeckt ist.
     *
     * `allocate()` deckelt eine zu hohe Verkaufsmenge bislang still auf das
     * verfügbare Volumen — der nicht zuteilbare Rest bleibt unzugeteilt, ohne
     * dass das nach außen sichtbar wird. Im Feldfall, der zur Aufnahme dieses
     * Punkts führte, zeigte das Verkaufsformular dadurch grüne Haken und eine
     * vollständige Gewinnermittlung, obwohl 3.800 Stück angefordert, aber nur
     * 190 verfügbar waren (beides auf heutiger Skala) — siehe ARCHITECTURE.md,
     * "Offene Punkte"/"Erledigt", "Skalenbewusste Mengenprüfung im
     * Verkaufsformular" (11.08.2026). Aufrufer sollen diese Methode VOR dem
     * Aufruf von `allocate()` nutzen, um eine zu hohe Menge explizit
     * abzuweisen, statt sich auf eine zufällig passende Gesamtsumme zu
     * verlassen (z. B. weil ohnehin die komplette Position verkauft wird).
     *
     * Beide Seiten werden vor dem Vergleich auf die heutige Skala
     * umgerechnet, genau wie in `allocate()` — ein direkter Vergleich
     * unskalierter Werte wäre bei einem Split zwischen Kauf- und
     * Verkaufsdatum falsch.
     *
     * @param saleVolume               Verkaufsmenge, Beleg-Skala des Verkaufsdatums.
     * @param saleDate                 Datum des Verkaufs.
     * @param availableBuysOldestFirst Verfügbare Käufe (Restvolumen > 0).
     * @param splits                   Alle Splits der betroffenen Aktie.
     * @return true, wenn saleVolume (heutige Skala, Toleranz 1e-9) durch die
     *         Summe der verfügbaren Käufe (ebenfalls heutige Skala) gedeckt
     *         ist.
     */
    static bool isSaleVolumeCovered(
        double saleVolume, const QDate& saleDate,
        const QList<BuyObject>& availableBuysOldestFirst,
        const QList<ShareSplitObject>& splits);
};

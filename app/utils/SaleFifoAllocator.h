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
};

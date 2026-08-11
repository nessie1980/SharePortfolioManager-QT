// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QList>
#include <QString>

/**
 * @brief Eine Zeile des Details-Dialogs "Verwendete Käufe".
 *
 * Reine Transportstruktur zwischen `PresenterSaleEdit` und `ViewSaleEdit`.
 * Alle Werte liegen bereits in der heutigen (split-bereinigten) Skala vor und
 * sind fertig zur Anzeige — die View rechnet nichts mehr.
 *
 * Eingeführt mit dem Bugfix "anteilige Kauf-Nebenkosten gehen bei der
 * FIFO-Zuteilung verloren" (siehe ARCHITECTURE.md). Die Berechnung der
 * anteiligen Brokerage braucht `IModelSaleEdit::loadBrokerageForBuy()` und
 * damit Modellzugriff, den die View per MVP nicht hat — deshalb wandert die
 * gesamte Aufbereitung in den Presenter.
 */
struct SaleBuyDetailRow
{
    QString date;             ///< Kaufdatum, bereits formatiert
    double  volume     = 0.0; ///< Zugeteilte Anteile, heutige Skala
    double  buyPrice   = 0.0; ///< Kaufkurs, heutige Skala
    double  fees       = 0.0; ///< Anteilige Kauf-Brokerage, Geldbetrag (unskaliert)
    double  reduction  = 0.0; ///< Anteiliger Kauf-Rabatt, Geldbetrag (unskaliert)
    double  buyValue   = 0.0; ///< volume × buyPrice
    double  saleValue  = 0.0; ///< volume × Verkaufskurs (heutige Skala)
    double  profitLoss = 0.0; ///< saleValue − buyValue
    QString document;         ///< Pfad zum Kauf-Dokument (leer = kein Dokument)
};

/**
 * @brief Vollständiger Inhalt des Details-Dialogs: Zeilen plus Summen.
 *
 * @note `fees` und `reduction` sind Geldbeträge und werden NICHT mit dem
 * Split-Faktor skaliert — ein Split verändert weder Kosten noch Gewinn
 * (gleiche Regel wie in `ShareCalculator`).
 */
struct SaleBuyDetailSummary
{
    QList<SaleBuyDetailRow> rows;

    double totalVolume     = 0.0;
    double totalFees       = 0.0;
    double totalReduction  = 0.0;
    double totalBuyValue   = 0.0;  ///< Summe der Kaufsummen OHNE Kosten
    double totalSaleValue  = 0.0;
    double saleFees        = 0.0;  ///< Verkaufsgebühren + Steuern des Verkaufs selbst
    double totalProfitLoss = 0.0;

    /**
     * @brief true, wenn der Dialog für einen bereits gespeicherten Verkauf
     * geöffnet wurde (steuert nur die Kopfzeile des Dialogs).
     *
     * Entspricht dem bisherigen `m_loadedSale.isValid()` in
     * `ViewSaleEdit::onShowDetails()`.
     */
    bool editMode = false;
};

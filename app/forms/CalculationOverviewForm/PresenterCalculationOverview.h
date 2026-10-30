// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IViewCalculationOverview.h"
#include "IModelCalculationOverview.h"

#include "../../utils/CalculationBreakdown.h"

#include <QObject>
#include <QString>

/**
 * @brief Presenter des Tabs "Depot- vs. Marktwert" (MVP, ergänzt 27.10.2026).
 *
 * Stellt Marktwert- und Depotwert-Sicht nebeneinander — je Aktie und für die
 * Footer — und zeigt, wohin jeder Wert in den Tabs "Kompletter Depotwert" und
 * "Kompletter Marktwert", ihren Footern und den Charts fließt. Inhaltlich
 * dieselben Zahlen wie der Berechnungsbericht: beide lesen über
 * CalculationReport::collect() und bauen ihre Zeilen aus
 * CalculationBreakdown.
 *
 * Umfang nach Nessies Entscheidung vom 27.10.2026: je Aktie die
 * Vergleichstabelle und der Abgleich gegen die Charts; Transaktionen und
 * FIFO-Bestandsabgleich stehen weiter nur im Bericht (offener Punkt).
 *
 * @note Die Daten werden in loadAndDisplay() einmal geladen und gehalten.
 * Ein Wechsel der Auswahl rechnet nichts neu. MainWindow ruft reload() nach
 * jeder Änderung an Portfolio oder Kursen auf.
 */
class PresenterCalculationOverview : public QObject
{
    Q_OBJECT

public:
    explicit PresenterCalculationOverview(IViewCalculationOverview* view,
                                          IModelCalculationOverview* model,
                                          QObject* parent = nullptr);

    /** Schlüssel des Eintrags "Gesamt / Footer". Eine GUID beginnt nie mit '#'. */
    static QString totalKey() { return QStringLiteral("#gesamt"); }

    /**
     * @brief Lädt die Daten und baut Liste und Detail auf.
     *
     * Eine bestehende Auswahl bleibt erhalten, solange es den Eintrag noch
     * gibt; sonst und beim ersten Laden ist "Gesamt / Footer" gewählt.
     */
    void loadAndDisplay();

    /** Verwirft die gehaltenen Daten und lädt neu, siehe loadAndDisplay(). */
    void reload() { loadAndDisplay(); }

    // ── Für Tests direkt erreichbar (public static, siehe ARCHITECTURE.md) ──

    /// Auswahlliste: "Gesamt / Footer", dann alle Aktien in Berichtsreihenfolge.
    static QList<CalculationOverviewEntry> buildEntries(const CalculationReportInput& input);

    /// Kopfzeile "Stand … · n Aktien (m im Bestand) · Auffälligkeiten".
    static QString buildSummary(const CalculationReportInput& input);

    /// Detail einer Aktie: Gegenüberstellung (14 Zeilen) und Abgleich.
    static CalculationOverviewDetail buildShareDetail(const CalculationReportShare& share);

    /// Detail "Gesamt / Footer": beide Footer (10 Zeilen), Abgleich, alle Auffälligkeiten.
    static CalculationOverviewDetail buildTotalDetail(const CalculationReportInput& input);

    /// Formatiert eine Zeile des Zeilenmodells für die Tabelle.
    static CalculationOverviewComparisonRow toComparisonRow(int number,
                                                            const CalculationBreakdownRow& row);

public slots:
    /** Reagiert auf eine neue Auswahl in der Liste. */
    void onSelectionChanged();

private:
    void showDetailFor(const QString& key);

    static CalculationOverviewTone toneOf(const CalculationFindings& findings);
    static QString findingsText(const CalculationFindings& findings);
    static QList<CalculationOverviewCheckRow> toCheckRows(const QList<CalculationCheck>& checks);
    static QList<CalculationOverviewFinding> toFindings(const CalculationFindings& findings);

    IViewCalculationOverview*  m_view;
    IModelCalculationOverview* m_model;

    CalculationReportInput m_input;   ///< zuletzt geladene Daten, siehe Klassendoku
};

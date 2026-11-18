// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IViewCalculationOverview.h"
#include "ModelCalculationOverview.h"
#include "PresenterCalculationOverview.h"

#include <QColor>
#include <QIcon>
#include <QWidget>

class CollapsibleSection;
class QGroupBox;
class QLabel;
class QListWidget;
class QPushButton;
class QTableWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;

/**
 * @brief Einbettbares Widget für den Tab "Depot- vs. Marktwert"
 * (ergänzt 27.10.2026).
 *
 * Wie ViewPortfolioChart ein QWidget, kein QDialog: MainWindow hängt es als
 * fünften Tab ein und erzeugt es erst beim ersten Betreten, weil der
 * Konstruktor das gesamte Portfolio samt Chart-Historie liest.
 *
 * Aufbau (Nessies Vorgaben vom 27.10.2026):
 * - links die Auswahl: "Gesamt / Footer", darunter die Aktien in den Gruppen
 *   "Im Bestand" und "Vollständig verkauft", jeweils mit Symbol für
 *   Abweichung, Hinweis oder ok;
 * - rechts die Gegenüberstellung Marktwert / Depotwert mit Differenz,
 *   Rechenweg und "Fließt ein in", darunter der Abgleich gegen die Charts und
 *   die Auffälligkeiten;
 * - unten "Bericht speichern…" für den Berechnungsbericht als Textdatei.
 *
 * Seit v1.31.0 (18.11.2026, Mischform nach Nessies Entscheidung vom
 * 17.11.2026) steht unter dem Teiler ein eigener Bildlaufbereich mit Gruppen
 * zum Aufklappen (CollapsibleSection): Abgleich, Auffälligkeiten und je
 * Aktie Käufe, Verkäufe, Dividenden, Kosten, Splits und Bestand / FIFO. Ob
 * eine Gruppe offen ist, gibt der Presenter mit jedem showDetail() vor; ein
 * Auf- oder Zuklappen von Hand gilt nur bis zur nächsten Auswahl.
 * Seit v1.31.1 (18.11.2026) liegen Teiler und Gruppen mit in der Groupbox
 * der Gegenüberstellung (Variante A), und die Tabellen der Gruppen füllen
 * die Breite: Textspalten so breit wie ihr Inhalt, Zahlenspalten teilen
 * sich den Rest.
 *
 * Reine MVP-View: keine Fachlogik, keine Formatierung von Zahlen. Nur die
 * Bewertung (CalculationOverviewTone) setzt sie in Farbe und Symbol um.
 */
class ViewCalculationOverview : public QWidget, public IViewCalculationOverview
{
    Q_OBJECT

public:
    explicit ViewCalculationOverview(QWidget* parent = nullptr);

    // ── IViewCalculationOverview ──────────────────────────────────────────
    QString selectedKey() const override;
    void setEntries(const QList<CalculationOverviewEntry>& entries) override;
    void setSelectedKey(const QString& key) override;
    void setSummary(const QString& text) override;
    void showDetail(const CalculationOverviewDetail& detail) override;

    /** Liest die Daten neu und zeichnet; von MainWindow nach Änderungen an
     *  Portfolio oder Kursen aufgerufen. Die Auswahl bleibt erhalten. */
    void reload() { m_presenter.reload(); }

signals:
    /** "Bericht speichern…" — MainWindow schreibt den Bericht wie über das
     *  Hilfe-Menü (Dateidialog, zwei Dateien). */
    void saveReportRequested();

private:
    void setupUi();
    static QColor toneColor(CalculationOverviewTone tone);
    static QIcon  toneIcon(CalculationOverviewTone tone);
    static void   fitHeightToRows(QTableWidget* table);
    static void   fitHeightToRows(QListWidget* list);
    /// Baut eine Gruppe mit ihren Tabellen (ergänzt 18.11.2026).
    CollapsibleSection* makeGroupSection(const CalculationOverviewGroup& group);
    /// Eine Tabelle einer Gruppe, so hoch und breit wie ihr Inhalt.
    QTableWidget* makeGroupTable(const CalculationOverviewTable& table);

    QLabel*       m_summaryLabel      = nullptr;
    QTreeWidget*  m_entryTree         = nullptr;
    QLabel*       m_titleLabel        = nullptr;
    QLabel*       m_subtitleLabel     = nullptr;
    QGroupBox*    m_comparisonBox     = nullptr;
    QTableWidget* m_comparisonTable   = nullptr;
    CollapsibleSection* m_checkSection    = nullptr;
    QTableWidget* m_checkTable        = nullptr;
    QLabel*       m_checkNotesLabel   = nullptr;
    CollapsibleSection* m_findingsSection = nullptr;
    QListWidget*  m_findingsList      = nullptr;
    QVBoxLayout*  m_groupsLayout      = nullptr;  ///< nimmt die Gruppen je Aktie auf
    QList<CollapsibleSection*> m_groupSections;    ///< bei jedem showDetail() neu
    QPushButton*  m_saveButton        = nullptr;

    ModelCalculationOverview     m_model;
    PresenterCalculationOverview m_presenter;
};

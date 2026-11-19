// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QCoreApplication>
#include <QList>
#include <QString>

#include "../models/BrokerageObject.h"
#include "../models/DividendObject.h"
#include "../models/SaleObject.h"
#include "../models/ShareSplitObject.h"

class OverviewTabWidget;
class QTableWidget;
class QTableWidgetItem;
class QWidget;

/**
 * @brief Befüllt ein OverviewTabWidget mit Verkäufen, Dividenden oder Kosten
 * (Refactor 19.11.2026, v1.31.2).
 *
 * Bis v1.31.1 standen dieselben Übersichten zweimal im Code: einmal im
 * jeweiligen Editier-Dialog (ViewSaleEdit, ViewDividendEdit,
 * ViewBrokerageEdit, je populateOverview()) und einmal nachgebaut im
 * Aktiendetails-Dialog (ViewShareDetails::populateGewinneVerluste(),
 * populateDividenden(), populateKosten()). Beide Seiten rufen jetzt diese
 * Funktionen auf — Spalten, Summen, Split-Marker und Fußzeilen stehen nur
 * noch hier (Nessies Vorgabe 18.11.2026: gemeinsame Funktion statt Kopie).
 *
 * Was ein Klick auf eine Zeile auslöst, bleibt Sache des Aufrufers (Editier-
 * Dialog: Eintrag in die Maske laden; Aktiendetails: Dokumentvorschau) und
 * hängt an den Signalen des OverviewTabWidget, nicht an diesen Funktionen.
 *
 * Bei der Zusammenführung angeglichen (Nessies Entscheidungen 18./19.11.2026,
 * vorher je nach Dialog verschieden):
 * - leere Liste: Übersicht-Tab "Übersicht (0,00 €)" mit dem Hinweis "Keine …
 *   vorhanden." statt gar keiner Tabs (OverviewTabWidget, Parameter
 *   emptyText);
 * - Kosten, Jahres-Tabs: Beträge mit " €" wie überall sonst (vorher im
 *   Editier-Dialog ohne);
 * - Kosten, Fußzeile der Jahres-Tabs: "-" in der Dokument-Spalte (vorher im
 *   Editier-Dialog leer);
 * - Kosten, Übersicht: Spalte "Jahr" 100 px (vorher im Aktiendetails-Dialog
 *   80 px);
 * - Zellen durchgehend nicht bearbeitbar (die Tabellen sind ohnehin über
 *   NoEditTriggers schreibgeschützt; Auswählen und Laden in die Maske laufen
 *   über cellClicked und bleiben unberührt);
 * - Summen je Jahr gebildet und daraus die Gesamtsumme.
 *
 * Zeilen der Jahres-Tabs bleiben in Beleg-Skala, alle Summen stehen in
 * heutiger Skala (ARCHITECTURE.md, "Split-Marker und Summen in den
 * Übersichtstabellen").
 */
class OverviewTabFiller
{
    Q_DECLARE_TR_FUNCTIONS(OverviewTabFiller)

public:
    /// Welche Beträge die Verkäufe zeigen.
    enum class SaleAmounts
    {
        /// Auszahlung und Gewinn/Verlust nach Kosten und Steuern
        /// (payoutBrokerageReduction(), profitLossBrokerageReduction()) —
        /// Editier-Dialog und Aktiendetails im Depotwert-Modus.
        Depot,
        /// "Rein Kurs" (Nessies Rechenregel 25.10.2026): Auszahlung =
        /// Verkaufswert brutto, Gewinn/Verlust = Verkaufswert − Kaufwert der
        /// verkauften Stücke, ohne Kosten und Steuern — Aktiendetails im
        /// Marktwert-Modus.
        Market
    };

    /// Übersicht Jahr | Anteile | Auszahlung | Gewinn / Verlust; Jahres-Tabs
    /// Datum | Anteile | Auszahlung | Gewinn / Verlust | Dokument.
    static void fillSales(OverviewTabWidget*             tabs,
                          const QList<SaleObject>&       sales,
                          const QList<ShareSplitObject>& splits,
                          SaleAmounts                    amounts = SaleAmounts::Depot);

    /// Übersicht Jahr | Dividende; Jahres-Tabs Datum | Dividendensatz |
    /// Anteile | Dividende | Dokument.
    static void fillDividends(OverviewTabWidget*             tabs,
                              const QList<DividendObject>&   dividends,
                              const QList<ShareSplitObject>& splits);

    /// Übersicht Jahr | Netto-Kosten; Jahres-Tabs Datum | Typ | Ges. Gebühren |
    /// Rabatt | Netto-Kosten | Dokument.
    static void fillBrokerages(OverviewTabWidget*            tabs,
                               const QList<BrokerageObject>& brokerages);

    /// Zentrierte, nicht bearbeitbare Zelle.
    static QTableWidgetItem* centeredItem(const QString& text);

    /// Dokument-Icon (PDF/Word/Excel, sonst "nicht gefunden") mit dem Pfad als
    /// Tooltip.
    static QWidget* documentIconWidget(const QString& documentPath);

    /**
     * @brief Dokument-Spalte einer Belegzeile: mit Dokument ein Item mit dem
     * Pfad im Qt::UserRole (für OverviewTabWidget::documentActivated() und
     * rowActivatedWithDocument()) plus Icon, sonst "-".
     */
    static void setDocumentCell(QTableWidget* table, int row, int column,
                                const QString& documentPath);

    /// Breite der Dokument-Spalte (reine Icon-Spalte, 16.07.2026).
    static constexpr int kDocColumnWidth = 36;
};

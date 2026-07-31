// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "ViewChart.h"

#include <QWidget>
#include <QLabel>
#include <QPoint>

/**
 * @brief Rahmenloses Popup-Fenster mit nur dem Aktien-Chart (Graph +
 * Legende) — ported from the C# reference's FrmChart (siehe
 * ARCHITECTURE.md, "ChartPopup — Rechtsklick-Popup-Chart").
 *
 * Geöffnet per einfachem Rechtsklick auf eine Portfolio-Zeile in MainWindow
 * (siehe MainWindow::onPortfolioRowRightClicked()) — unabhängig von einer
 * eventuell bereits geöffneten ShareDetailsForm. Enthält eine eigene,
 * eigenständige ViewChart-Instanz im Compact-Modus (nur Graph + Legende,
 * keine Selektion-/Zeitraum-Steuerelemente sichtbar); Mausrad-Zoom bleibt
 * über ViewChart's bestehende eventFilter()-/applyWheelStep()-Logik voll
 * funktionsfähig, ganz ohne Duplizierung.
 *
 * Kein eigenes MVP-Presenter/Model nötig: ViewChart bringt bereits ihre
 * eigene ModelChart-/PresenterChart-Instanz mit (siehe ViewChart.h) — dieses
 * Popup ist rein ein View-seitiges Rahmenfenster.
 *
 * Schließt sich automatisch, sobald die Maus den Fensterbereich verlässt
 * (leaveEvent()) — Pendant zu OnChartDailyValues_MouseLeave/
 * OnLblNoDataMessage_MouseLeave in der C#-Referenz (dort an zwei einzelne
 * Kind-Widgets gebunden; hier genügt ein einziger Handler auf dem
 * Popup-Fenster selbst, da es außer ViewChart keine weiteren
 * Geschwister-Widgets enthält).
 *
 * @note **Spurious-Leave-Fix (ergänzt 31.07.2026, Nessies Rückmeldung
 * "Dialog geht zu, auch wenn die Maus noch auf dem Dialog ist"):**
 * QChartView (QGraphicsView) hat einen eigenen Viewport, an dessen inneren
 * Widget-Grenzen Qt gelegentlich ein Leave auf ChartPopup selbst auslöst,
 * obwohl die Maus tatsächlich noch innerhalb des Popups steht — ein
 * bekannter Qt-Effekt bei QGraphicsView-Kindwidgets. leaveEvent() prüft
 * deshalb zusätzlich die tatsächliche Cursor-Position (QCursor::pos())
 * gegen die eigene Bildschirmgeometrie und schließt nur, wenn die Maus
 * WIRKLICH außerhalb liegt.
 *
 * @note **Überschrift (ergänzt 31.07.2026, Nessies Rückmeldung "Was auch
 * fehlt ist die Überschrift mit Informationen!"):** Da das Popup rahmenlos
 * ist (kein Fenstertitel sichtbar, anders als bei ViewShareDetails' echter
 * Titelleiste), zeigt ein eigenes, zentriertes Label oberhalb des Charts
 * "Aktienname" (fett) + "Zeitraum: ... / Entwicklung: ..." — Pendant zum
 * C#-Referenz-Chart-Titel (`FrmChart.Title`, direkt auf dem WinForms-Chart-
 * Steuerelement gezeichnet). Der zweite Teil kommt über ViewChart's
 * titleInfoChanged()-Signal; da dessen erste Emission bereits während der
 * ViewChart-Konstruktion (im Presenter-Konstruktor-Aufruf) erfolgt, BEVOR
 * ChartPopup sich verbinden kann, wird der initiale Wert stattdessen einmalig
 * über ViewChart::rangeInfo() nachträglich abgegriffen (siehe ChartPopup.cpp).
 *
 * Lebenszyklus: Qt::WA_DeleteOnClose — MainWindow erzeugt das Popup per
 * `new ChartPopup(...)` ohne Owner und muss sich um Zerstörung nicht weiter
 * kümmern; es löscht sich selbst, sobald es schließt.
 */
class ChartPopup : public QWidget
{
    Q_OBJECT

public:
    /**
     * @param shareGuid  GUID der Aktie, für die der Chart angezeigt wird.
     * @param shareName  Anzeigename der Aktie für die Überschrift (siehe
     *                   Klassendoku) — vom Aufrufer übergeben statt selbst
     *                   per Repository nachgeschlagen, da MainWindow den
     *                   Namen aus der Portfolio-Zeile ohnehin schon kennt.
     * @param parent     Optionaler Owner. Üblicherweise nullptr — siehe
     *                   Klassendoku zum Lebenszyklus (Qt::WA_DeleteOnClose).
     */
    explicit ChartPopup(const QString& shareGuid, const QString& shareName,
                        QWidget* parent = nullptr);
    ~ChartPopup() override = default;

    /**
     * @brief Zeigt das Popup so an, dass sein oberer linker Rand an
     * @p globalPos liegt, geklemmt an die verfügbare Geometrie des
     * Bildschirms unter dem Cursor — verhindert, dass das Fenster bei einem
     * Rechtsklick nahe am Bildschirmrand rechts/unten abgeschnitten wird.
     *
     * Die Größe vor diesem Aufruf ist die kompakte Standardgröße (siehe
     * ChartPopup.cpp, kPopupWidth/kPopupHeight) — der Aufrufer kann sie
     * vorher per resize() überschreiben (siehe MainWindow::
     * onPortfolioRowRightClicked(), das die Breite an die des Hauptfensters
     * angleicht, Nessies Vorgabe 31.07.2026); showAt() klemmt die Position
     * anhand der zu diesem Zeitpunkt tatsächlich gesetzten Größe.
     */
    void showAt(const QPoint& globalPos);

protected:
    void leaveEvent(QEvent* event) override;

private slots:
    /**
     * @brief Aktualisiert m_headerLabel aus m_shareName (fett, konstant) +
     * @p rangeInfoText (ViewChart's "Zeitraum: .../Entwicklung: ..." bzw.
     * leer ohne Daten) — verbunden mit ViewChart::titleInfoChanged() und
     * einmalig direkt im Konstruktor aufgerufen (siehe Klassendoku).
     */
    void updateHeaderText(const QString& rangeInfoText);

private:
    QString    m_shareName;
    QLabel*    m_headerLabel = nullptr;
    ViewChart* m_chart       = nullptr;
};

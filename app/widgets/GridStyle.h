// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QTableWidget>
#include <QString>

/**
 * @brief Zentrale, App-weit einheitliche Selektionsfarbe für QTableWidget-Grids.
 *
 * In der C#-Referenzanwendung wird die selektierte Zeile in allen Grids mit
 * blauem Hintergrund und gelber Schrift dargestellt (statt der Standard-
 * Highlight-Farbe der Qt-Palette/des Systemthemes). Damit dieses Verhalten
 * konsistent in der gesamten Qt-Portierung erscheint, statt an jeder
 * Tabellen-Erzeugungsstelle einzeln dupliziert zu werden, kapselt dieser
 * Header-only-Helper den nötigen Stylesheet-Anteil (analog zu
 * CenterIconDelegate.h/TwoLineDelegate.h — kein Q_OBJECT, keine eigene .cpp).
 *
 * Farbwerte theme-neutral gewählt (angelehnt an den Log-Farben-Fix vom
 * 24.07.2026, siehe ARCHITECTURE.md): ausreichender Kontrast auf hellem und
 * dunklem Hintergrund, da das Linux-AppImage mangels Platform-Theme-Plugin
 * immer auf die helle Palette zurückfällt.
 *
 * Nur auf selektierbare Daten-Tabellen anwenden — die Frozen-Footer-Tabellen
 * (Gesamt-Zeile in OverviewTabWidget bzw. MainWindow-Footer) haben
 * `QAbstractItemView::NoSelection` und bleiben daher unangetastet.
 *
 * @code
 * table->setSelectionBehavior(QAbstractItemView::SelectRows);
 * GridStyle::applySelectionStyle(table);
 * @endcode
 */
namespace GridStyle {

/// Hintergrundfarbe der selektierten Zeile.
inline const QString kSelectionBackground = QStringLiteral("#1c3f8f");
/// Schriftfarbe der selektierten Zeile.
inline const QString kSelectionForeground = QStringLiteral("#ffd400");

/**
 * @brief Setzt den einheitlichen Blau/Gelb-Selektionsstil auf @p table.
 *
 * Additiv: ein evtl. bereits vorhandenes Stylesheet auf @p table wird nicht
 * überschrieben, sondern der Selektions-Anteil angehängt.
 */
inline void applySelectionStyle(QTableWidget* table)
{
    if (!table)
        return;

    const QString style = QStringLiteral(
        "QTableWidget::item:selected {"
        " background-color: %1;"
        " color: %2;"
        "}").arg(kSelectionBackground, kSelectionForeground);

    table->setStyleSheet(table->styleSheet() + style);
}

} // namespace GridStyle

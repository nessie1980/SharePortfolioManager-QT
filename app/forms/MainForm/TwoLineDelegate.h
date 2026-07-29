// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QStyledItemDelegate>
#include <QPainter>
#include <QApplication>

#include "../../widgets/GridStyle.h"

/**
 * @brief Qt roles used to supply the two text lines and their colors.
 *
 * Store these on QTableWidgetItem via setData():
 * @code
 * item->setData(TwoLineDelegate::TopRole,    "274,50 €");
 * item->setData(TwoLineDelegate::BottomRole, "273,10 €");
 * item->setData(TwoLineDelegate::TopColorRole,    QColor(Qt::red));
 * item->setData(TwoLineDelegate::BottomColorRole, QColor(Qt::green));
 * @endcode
 *
 * If TopColorRole / BottomColorRole are not set, the palette's
 * default text color is used.
 */
namespace TwoLineRole {
    static constexpr int Top         = Qt::UserRole + 10; ///< QString — upper line text
    static constexpr int Bottom      = Qt::UserRole + 11; ///< QString — lower line text
    static constexpr int TopColor    = Qt::UserRole + 12; ///< QColor  — upper line color
    static constexpr int BottomColor = Qt::UserRole + 13; ///< QColor  — lower line color
}

/**
 * @brief Delegate that renders two text lines inside a single table cell.
 *
 * Both lines use the cell font (equal size); the lower line is rendered in
 * a muted grey by default (overridable via TwoLineRole::BottomColor). When a
 * cell has no second line, the single value is centered vertically.
 *
 * Text is right-aligned by default; override alignment via
 * Qt::TextAlignmentRole on the item.
 *
 * ### Usage
 * Assign to individual columns:
 * @code
 * auto* del = new TwoLineDelegate(table);
 * table->setItemDelegateForColumn(colIndex, del);
 * @endcode
 */
class TwoLineDelegate : public QStyledItemDelegate
{
public:
    explicit TwoLineDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        // Draw selection / hover background via the style.
        //
        // Bugfix (29.07.2026, Nessies Vorgabe "Grid-Selektionsfarbe in allen
        // Grids") — zweiter Anlauf: Der erste Versuch (opt.widget->style()
        // statt QApplication::style()) traf zwar Qt's eigenen Default-Pfad
        // (siehe QStyledItemDelegate::paint()), löste das eigentliche Problem
        // aber nicht vollständig — Qt spiegelt eine per Stylesheet gesetzte
        // `item:selected { color: ...; background-color: ...; }`-Regel NICHT
        // in eine über QPalette abfragbare Farbe zurück. `pal.color(
        // QPalette::HighlightedText)` lieferte also weiterhin die alte
        // System-Highlight-Farbe (meist Weiß) statt unser Gelb — nur der
        // Hintergrund näherte sich zufällig dem System-Blau an, wodurch der
        // Unterschied kaum auffiel.
        //
        // Robuster: bei Selektion direkt dieselben GridStyle-Konstanten
        // verwenden, die auch table->setStyleSheet() setzt — garantiert
        // exakt dieselbe Farbe wie im Rest der Zeile, unabhängig von Qt's
        // QSS-zu-Palette-Übersetzung.
        const bool selected = option.state & QStyle::State_Selected;

        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        opt.text.clear(); // suppress default text rendering

        if (selected) {
            painter->fillRect(option.rect, QColor(GridStyle::kSelectionBackground));
        } else {
            QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
            style->drawControl(QStyle::CE_ItemViewItem, &opt, painter);
        }

        painter->save();

        const QRect rect = option.rect;
        const int   hpad = 4; // horizontal padding

        // ── Fonts ─────────────────────────────────────────────────────────
        // Both lines use the same size.
        QFont topFont    = option.font;
        QFont bottomFont = option.font;

        const QFontMetrics fmTop(topFont);
        const QFontMetrics fmBot(bottomFont);

        const int lineHeight = rect.height() / 2;
        // When there is no second line, center the single value vertically in
        // the whole cell instead of drawing it in the upper half (which would
        // look like a two-line cell with an empty second line).
        const bool hasBottom =
            !index.data(TwoLineRole::Bottom).toString().isEmpty();
        const int topY = hasBottom
            ? rect.top() + (lineHeight - fmTop.height()) / 2 + fmTop.ascent()
            : rect.top() + (rect.height() - fmTop.height()) / 2 + fmTop.ascent();
        const int bottomY = rect.top() + lineHeight
                          + (lineHeight - fmBot.height()) / 2 + fmBot.ascent();

        // ── Colors ────────────────────────────────────────────────────────
        const QPalette& pal = option.palette;

        auto resolveColor = [&](int role, const QColor& fallback) -> QColor {
            // Bugfix (29.07.2026, wie oben): Selektion hat immer Vorrang vor
            // einer explizit gesetzten TopColor/BottomColor (z.B. Grün/Rot
            // bei Gewinn/Verlust) — sonst gewinnt praktisch immer die alte
            // Farbe, da fast jede Zelle hier eine explizite Farbe trägt und
            // der Selektions-Fallback dadurch nie zum Zug kam. Direkt
            // GridStyle::kSelectionForeground statt pal.color(HighlightedText)
            // — Qt spiegelt die per Stylesheet gesetzte item:selected-Farbe
            // nicht in die QPalette zurück, siehe Kommentar oben.
            if (selected)
                return QColor(GridStyle::kSelectionForeground);
            QVariant v = index.data(role);
            if (v.isValid() && v.canConvert<QColor>())
                return v.value<QColor>();
            return fallback;
        };

        const QColor topColor = resolveColor(
            TwoLineRole::TopColor,
            pal.color(QPalette::Text));

        const QColor bottomColor = resolveColor(
            TwoLineRole::BottomColor,
            pal.color(QPalette::PlaceholderText));

        // ── Alignment ─────────────────────────────────────────────────────
        const int alignFlags = index.data(Qt::TextAlignmentRole).isValid()
            ? index.data(Qt::TextAlignmentRole).toInt()
            : static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
        const bool rightAlign = (alignFlags & Qt::AlignRight);

        auto drawLine = [&](const QString& text,
                            const QFont&   font,
                            const QColor&  color,
                            int            y)
        {
            if (text.isEmpty()) return;
            painter->setFont(font);
            painter->setPen(color);
            const QFontMetrics fm(font);
            const int textW = fm.horizontalAdvance(text);
            int x;
            if (rightAlign)
                x = rect.right() - hpad - textW;
            else
                x = rect.left() + hpad;
            painter->drawText(x, y, text);
        };

        const QString topText    = index.data(TwoLineRole::Top).toString();
        const QString bottomText = index.data(TwoLineRole::Bottom).toString();

        drawLine(topText,    topFont,    topColor,    topY);
        drawLine(bottomText, bottomFont, bottomColor, bottomY);

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override
    {
        Q_UNUSED(index)
        // Two equal-size lines (both rendered in the cell font) + padding.
        const int lineH = option.fontMetrics.height();
        return { 120, 2 * lineH + 8 };
    }
};

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>

/**
 * @brief Delegate that horizontally centers an item's decoration (icon).
 *
 * The default item delegate left-aligns decorations, so icon-only cells
 * (update-state icon, development chart icons) sit on the left edge of the
 * column. Setting decorationAlignment to Qt::AlignCenter centers them.
 *
 * Apply to the icon columns of both the main tables and their footers:
 * @code
 * table->setItemDelegateForColumn(iconColumn, new CenterIconDelegate(table));
 * @endcode
 */
class CenterIconDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

protected:
    void initStyleOption(QStyleOptionViewItem* option,
                         const QModelIndex& index) const override
    {
        QStyledItemDelegate::initStyleOption(option, index);
        option->decorationAlignment = Qt::AlignCenter;
    }
};

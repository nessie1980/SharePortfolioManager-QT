// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

/**
 * @brief Shared UI sizing constants used across all form dialogs.
 *
 * Centralising these values ensures that input fields (QLineEdit, QComboBox,
 * QDateEdit, QTimeEdit, QDoubleSpinBox) and action buttons always have the
 * same fixed height regardless of which dialog they appear in.
 *
 * Usage in addRow() implementations:
 * @code
 *   field->setFixedHeight(UiConstants::kFieldHeight);
 * @endcode
 *
 * Usage for buttons:
 * @code
 *   btn->setFixedHeight(UiConstants::kButtonHeight);
 * @endcode
 */
namespace UiConstants
{
    /** Fixed height for all editable / selectable input widgets:
     *  QLineEdit, QComboBox, QDateEdit, QTimeEdit, QDoubleSpinBox. */
    constexpr int kFieldHeight  = 24;

    /** Fixed height for all action QPushButtons. */
    constexpr int kButtonHeight = 24;
}

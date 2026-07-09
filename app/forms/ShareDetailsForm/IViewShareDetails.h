// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QString>
#include <QList>
#include <QColor>

/**
 * @brief One line of a "Bestandsberechnung" box (Gesamt/Vortag/Aktuelle).
 *
 * Rendered as three columns: a small leading operator ("×", "=", "+", "−",
 * or empty), a label, and a right-aligned value. All text is pre-formatted
 * by PresenterShareDetails (locale-aware number formatting, sign handling) —
 * the View performs no computation, only layout.
 */
struct CalculationRow
{
    QString operatorSymbol; ///< "", "×", "=", "+", "−"
    QString label;
    QString value;
    QColor  color;          ///< Invalid QColor() -> default text color.
    bool    emphasize = false; ///< Bold label+value (used for "=" result rows).
};

using CalculationRows = QList<CalculationRow>;

/**
 * @brief Passive view interface for the share-details dialog.
 *
 * Implemented by ViewShareDetails (production, QDialog-based) and by a fake
 * in tst_sharedetailsform.cpp for isolated Presenter tests.
 */
class IViewShareDetails
{
public:
    virtual ~IViewShareDetails() = default;

    /** Window title / share name. Plain name for now — see ViewShareDetails
     *  for why the full "Zeitraum: ... / Entwicklung: ..." subtitle from the
     *  C# reference is deferred until the chart tab is implemented. */
    virtual void setHeaderName(const QString& name) = 0;

    /** "Letzte Internet-Aktualisierung: ... / Typ: ..." status line. */
    virtual void setStatusLine(const QString& statusText) = 0;

    virtual void populateGesamtBox(const CalculationRows& rows) = 0;
    virtual void populateVortagBox(const CalculationRows& rows) = 0;
    virtual void populateAktuelleBox(const CalculationRows& rows) = 0;

    // ── Fehler / Lifecycle ────────────────────────────────────────────────
    /** Shows a modal error message (QMessageBox::critical convention). */
    virtual void showError(const QString& message) = 0;
    /** Closes the dialog (QDialog::reject()) — used when the share was not found. */
    virtual void closeDialog() = 0;
};

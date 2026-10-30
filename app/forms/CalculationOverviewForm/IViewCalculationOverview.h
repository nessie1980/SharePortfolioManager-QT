// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "CalculationOverviewTypes.h"

#include <QList>
#include <QString>

/**
 * @brief Passives View-Interface des Tabs "Depot- vs. Marktwert"
 * (ergänzt 27.10.2026).
 *
 * Implementiert von ViewCalculationOverview (Produktion, einbettbares
 * QWidget wie ViewPortfolioChart) und von einem Fake in
 * tst_calculationoverviewform.cpp.
 *
 * Gleiche Konvention wie IViewPortfolioChart: der Presenter liest die Auswahl
 * über den Getter und schreibt fertig formatierte Inhalte über die Setter.
 * Die View formatiert nichts selbst.
 */
class IViewCalculationOverview
{
public:
    virtual ~IViewCalculationOverview() = default;

    // ── Steuerelemente (vom Presenter gelesen) ────────────────────────────

    /** Schlüssel des ausgewählten Eintrags; leer, wenn nichts gewählt ist. */
    virtual QString selectedKey() const = 0;

    // ── Anzeige (vom Presenter geschrieben) ───────────────────────────────

    /** Ersetzt die Auswahlliste. Löst keine Auswahlmeldung aus. */
    virtual void setEntries(const QList<CalculationOverviewEntry>& entries) = 0;

    /** Wählt den Eintrag mit @p key aus. Löst keine Auswahlmeldung aus. */
    virtual void setSelectedKey(const QString& key) = 0;

    /** Kopfzeile über allem, z. B. "Stand 27.10.2026 21:30 · 12 Aktien · …". */
    virtual void setSummary(const QString& text) = 0;

    /** Füllt die rechte Seite. */
    virtual void showDetail(const CalculationOverviewDetail& detail) = 0;
};

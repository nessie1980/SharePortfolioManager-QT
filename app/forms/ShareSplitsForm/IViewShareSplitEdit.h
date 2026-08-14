// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../../models/ShareSplitObject.h"

#include <QDate>
#include <QList>
#include <QString>

/**
 * @brief Abstraktes View-Interface für den Dialog "Aktiensplits".
 *
 * Deutlich schlanker als IViewBuyEdit / IViewDividendEdit: keine
 * Parse-Pipeline und keine Dokumenten-Vorschau — ein Split hat keinen Beleg,
 * er wird von Hand erfasst.
 *
 * @note confirm() gehört bewusst ins View-Interface statt der Presenter
 * OwnMessageBox direkt aufzurufen. Das Löschen eines Splits verändert Bestand,
 * Grid und Charts schlagartig und braucht deshalb eine Rückfrage (Nessies
 * Entscheidung 08.08.2026); über das Interface bleibt sie im Test steuerbar,
 * ohne dass ein modaler Dialog die Ereignisschleife blockiert.
 */
class IViewShareSplitEdit
{
public:
    virtual ~IViewShareSplitEdit() = default;

    // ── Form read (Benutzereingabe) ───────────────────────────────────────
    virtual QDate   splitDate()      const = 0;  ///< Ex-Tag des Splits
    virtual double  ratioNew()       const = 0;  ///< Neue Stückzahl-Seite
    virtual double  ratioOld()       const = 0;  ///< Alte Stückzahl-Seite
    virtual bool    pricesAdjusted() const = 0;  ///< Kurshistorie bereits bereinigt?
    virtual QString comment()        const = 0;

    /// Pfad zum Beleg der Bank (ergänzt 08.08.2026); leer, wenn keiner gewählt ist.
    virtual QString documentPath()   const = 0;

    // ── Form population (Presenter → View) ───────────────────────────────
    virtual void loadSplit(const ShareSplitObject& split) = 0;
    virtual void clearForm()                              = 0;

    /**
     * @brief Zeigt die Umrechnung im Klartext, z. B. "aus 1 Stk. werden 20 Stk.".
     * @param text  Fertig formatierter Text; leer bzw. "-" bei ungültiger Eingabe.
     */
    virtual void setFactorPreview(const QString& text) = 0;

    /// Schreibt den Dokumentpfad ins Eingabefeld.
    virtual void setDocumentPath(const QString& path) = 0;

    /**
     * @brief Setzt den "Kurshistorie bereits bereinigt"-Haken.
     *
     * Ergänzt 13.08.2026 für den "Prüfen"-Knopf (`SplitPriceJumpDetector`) —
     * bislang kam der Haken nur über loadSplit() aus gespeicherten Daten,
     * jetzt kann ihn der Presenter auch nach einer erkannten Prüfung setzen.
     */
    virtual void setPricesAdjusted(bool value) = 0;

    /**
     * @brief Einordnung des Prüfergebnisses für die Einfärbung von
     * setPriceJumpHint().
     *
     * Ergänzt 14.08.2026 (Nessies Vorgabe): `SplitPriceJumpDetector::Result`
     * kennt vier Ausprägungen, fürs Auge zählen aber nur zwei — entweder
     * wurde der Haken automatisch gesetzt/entfernt ("übernommen"), oder er
     * blieb unverändert, weil das Ergebnis uneindeutig war oder Daten
     * fehlten ("nicht übernommen", manuelle Entscheidung nötig).
     */
    enum class PriceJumpTone
    {
        Adopted,              ///< Ergebnis eindeutig — Haken automatisch gesetzt/entfernt.
        ManualDecisionNeeded, ///< Uneindeutig oder zu wenig Daten — Haken unverändert.
    };

    /**
     * @brief Zeigt das Ergebnis der Split-Kurssprung-Prüfung als Text an.
     *
     * Ergänzt 13.08.2026. Erscheint unter dem "Kurshistorie"-Haken, egal ob
     * die Prüfung eindeutig war oder nicht — bei Uneindeutigkeit macht der
     * Text das explizit und der Haken bleibt unverändert.
     * @param text  Anzuzeigender Ergebnistext.
     * @param tone  Steuert die Einfärbung des Textes, siehe PriceJumpTone.
     */
    virtual void setPriceJumpHint(const QString& text, PriceJumpTone tone) = 0;

    /// Lädt das Dokument in die Vorschau; leerer Pfad wirkt wie clearPdfPreview().
    virtual void openPdfPreview(const QString& path) = 0;

    /// Setzt die Vorschau auf den Platzhalter-Zustand zurück.
    virtual void clearPdfPreview() = 0;

    // ── Übersicht ─────────────────────────────────────────────────────────

    /**
     * @brief Füllt die Split-Tabelle neu.
     *
     * Bewusst eine flache Tabelle statt OverviewTabWidget mit Jahres-Tabs
     * (Nessies Entscheidung 08.08.2026): eine Aktie hat typischerweise null
     * bis drei Splits, eine Jahresgliederung wäre reiner Ballast.
     */
    virtual void populateOverview(const QList<ShareSplitObject>& splits) = 0;

    // ── Button-Zustand ────────────────────────────────────────────────────

    /**
     * @brief Setzt Beschriftung und Verfügbarkeit der Aktions-Buttons.
     * @param canRemove  true, wenn ein Eintrag geladen ist und gelöscht werden darf.
     * @param isEdit     true, wenn ein bestehender Eintrag geladen ist
     *        (Button heisst dann "Speichern" statt "Hinzufügen").
     */
    virtual void setButtonStates(bool canRemove, bool isEdit) = 0;

    // ── Rückmeldung ───────────────────────────────────────────────────────
    virtual void showError(const QString& message) = 0;

    /**
     * @brief Ja/Nein-Rückfrage; liefert true bei "Ja".
     */
    virtual bool confirm(const QString& title, const QString& message) = 0;

    virtual void acceptAndClose() = 0;
};

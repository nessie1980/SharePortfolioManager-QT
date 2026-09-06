// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IViewShareSplitEdit.h"

#include <QDialog>
#include <QDateEdit>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QGridLayout>
#include <QTableWidget>

class PresenterShareSplitEdit;
class DocumentPreviewPanel;

/**
 * @brief Qt-Dialog, der IViewShareSplitEdit umsetzt — "Aktiensplits".
 *
 * Layout:
 *
 * @code{.unparsed}
 * +-- Split hinzufuegen ---------------------+ +-- Dokumenten-Vorschau --+
 * |  Ex-Tag:        [date]                   | |                         |
 * |  Verhaeltnis:   [neu] : [alt] [Hinweis]  | |                         |
 * |  Umrechnung:    [read-only]              | |                         |
 * |  Kurshistorie:  [x] bereits bereinigt    | |      (PDF-Anzeige)      |
 * |  Pruefung:      [ro, 2-zeilig] [Pruefen] | |                         |
 * |  Kommentar:     [edit]                   | |                         |
 * |  Dokument:      [edit]              [...]| |                         |
 * +------------------------------------------+ |                         |
 * |  [Hinzufuegen] [Entfernen] [Reset] [Zu]  | |                         |
 * +-- Erfasste Splits -----------------------+ |                         |
 * |  Datum | Verh. | Umrechnung | Kurse | Kom.| |                        |
 * +------------------------------------------+ +-------------------------+
 * @endcode
 *
 * @note Bewusst ohne `OverviewTabWidget` (Nessies Entscheidung 08.08.2026):
 * eine Aktie hat typischerweise null bis drei Splits, Jahres-Tabs waeren
 * reiner Ballast.
 *
 * @note Dokument und Vorschau kamen am 08.08.2026 dazu (Nessies Vorgabe) —
 * Banken verschicken sehr wohl Mitteilungen ueber anstehende Splits. Der
 * Beleg wird gespeichert und angezeigt, aber nicht ausgewertet; ob ein
 * Parsing lohnt, ist als offener Punkt festgehalten.
 */
class ViewShareSplitEdit : public QDialog, public IViewShareSplitEdit
{
    Q_OBJECT

public:
    explicit ViewShareSplitEdit(const QString& shareGuid,
                                QWidget* parent = nullptr);
    ~ViewShareSplitEdit() override = default;

    PresenterShareSplitEdit* presenter() const { return m_presenter; }

    // ── IViewShareSplitEdit read ──────────────────────────────────────────
    QDate   splitDate()      const override;
    double  ratioNew()       const override;
    double  ratioOld()       const override;
    bool    pricesAdjusted() const override;
    QString comment()        const override;

    // ── IViewShareSplitEdit write ─────────────────────────────────────────
    void loadSplit(const ShareSplitObject& split)                  override;
    void clearForm()                                                override;
    QString documentPath()                                   const override;
    void setFactorPreview(const QString& text)                      override;
    void setDocumentPath(const QString& path)                       override;
    void setPricesAdjusted(bool value)                              override;
    void setPriceJumpHint(const QString& text, PriceJumpTone tone)  override;
    void openPdfPreview(const QString& path)                        override;
    void clearPdfPreview()                                          override;
    void populateOverview(const QList<ShareSplitObject>& splits)    override;
    void setButtonStates(bool canRemove, bool isEdit)               override;
    void showError(const QString& message)                          override;
    bool confirm(const QString& title, const QString& message)      override;
    void acceptAndClose()                                           override;

private slots:
    /** Übersetzt die Tabellen-Auswahl in einen onRowSelected()-Aufruf. */
    void onTableSelectionChanged();

    /** Öffnet den Dateidialog für den Beleg und prüft den Dokument-Root. */
    void onBrowseDocument();

    /** Zeigt den Hinweis-Dialog zu Bruchstücken bei Reverse-Splits. */
    void onShowReverseSplitHint();

private:
    void       setupUi();
    QGroupBox* createSplitDataGroup();
    QGroupBox* createDocumentGroup();
    QWidget*   createButtonBar();
    QGroupBox* createOverviewGroup();
    QWidget*   createPreviewPanel();

    /**
     * @brief Leert m_priceJumpResult und setzt seine Textfarbe auf den
     * ungefärbten Ausgangszustand zurück.
     *
     * Ergänzt 14.08.2026: setPriceJumpHint() färbt den Text grün/rot ein
     * (siehe PriceJumpTone) — ein einfaches clear() allein würde diese Farbe
     * stehen lassen, sodass der Platzhaltertext "Noch nicht geprüft …" nach
     * einem Reset fälschlich rot oder grün erschiene.
     */
    void resetPriceJumpResult();

    /**
     * @brief Baut den Text für den "Hinweis Reverse-Split"-Dialog.
     *
     * In sich geschlossen (14.08.2026, Nessies Vorgabe): keine Verweise auf
     * ARCHITECTURE.md oder interne Klassennamen. Nutzt das aktuell
     * eingetragene Verhältnis für eine konkrete Beispielrechnung, wenn es
     * ein echtes Reverse-Split-Verhältnis ist (neu < alt, beide > 0);
     * andernfalls ein festes 1:10-Beispiel.
     */
    QString reverseSplitHintMessage() const;

    /**
     * @brief Liest ein Zahlenfeld dieses Dialogs (delegiert an NumberParser).
     * @param ok  Optional; false bei nicht leerem, unlesbarem Text.
     *            Vorgabe nullptr, damit die bestehenden Aufrufstellen
     *            unveraendert bleiben — ausgewertet wird das Flag erst mit
     *            der Rueckmeldung unlesbarer Eingaben.
     */
    static double  parseDouble(const QString& text, bool* ok = nullptr);
    static QString formatRatioPart(double value);

    // ── Split-Daten ───────────────────────────────────────────────────────
    QGroupBox* m_splitDataGroup = nullptr;  ///< Titel wechselt je Modus
    QDateEdit* m_date           = nullptr;
    QLineEdit* m_ratioNew       = nullptr;
    QLineEdit* m_ratioOld       = nullptr;
    QPushButton* m_btnReverseSplitHint = nullptr;  ///< öffnet Hinweis-Dialog zu Bruchstücken bei Reverse-Splits
    QLineEdit* m_factorPreview  = nullptr;  ///< read-only; Notationshinweis als Tooltip
    QCheckBox* m_pricesAdjusted = nullptr;
    QPushButton*    m_btnCheckPriceJump = nullptr;  ///< löst SplitPriceJumpDetector aus
    QPlainTextEdit* m_priceJumpResult   = nullptr;  ///< read-only, feste Zweizeilen-Höhe
    QPalette        m_priceJumpDefaultPalette;      ///< Ausgangsfarbe vor setPriceJumpHint()
    QLineEdit* m_comment        = nullptr;

    // ── Dokument ──────────────────────────────────────────────────────────
    // Eigene Groupbox seit 13.08.2026, read-only wie bei den anderen fünf
    // Dialogen — der Pfad kommt nur noch über den Dateidialog, nicht mehr
    // per Handeingabe.
    QLineEdit*   m_documentPath = nullptr;
    QPushButton* m_btnBrowseDoc = nullptr;

    // ── Buttons ───────────────────────────────────────────────────────────
    QPushButton* m_btnAdd    = nullptr;
    QPushButton* m_btnRemove = nullptr;
    QPushButton* m_btnReset  = nullptr;
    QPushButton* m_btnClose  = nullptr;

    // ── Übersicht ─────────────────────────────────────────────────────────
    QTableWidget* m_table = nullptr;

    // ── Dokumenten-Vorschau ───────────────────────────────────────────────
    DocumentPreviewPanel* m_previewPanel = nullptr;

    /// Unterdrückt onTableSelectionChanged() waehrend populateOverview().
    bool m_suppressSelectionSignal = false;

    PresenterShareSplitEdit* m_presenter = nullptr;
};

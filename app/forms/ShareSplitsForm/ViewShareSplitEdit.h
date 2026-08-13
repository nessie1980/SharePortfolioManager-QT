// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IViewShareSplitEdit.h"

#include <QDialog>
#include <QDateEdit>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
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
 * |  Verhaeltnis:   [neu] : [alt]            | |                         |
 * |  Umrechnung:    [read-only]              | |                         |
 * |  Kurshistorie:  [x] bereits bereinigt    | |      (PDF-Anzeige)      |
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

private:
    void       setupUi();
    QGroupBox* createSplitDataGroup();
    QWidget*   createButtonBar();
    QGroupBox* createOverviewGroup();
    QWidget*   createPreviewPanel();

    static double  parseDouble(const QString& text);
    static QString formatRatioPart(double value);

    // ── Split-Daten ───────────────────────────────────────────────────────
    QGroupBox* m_splitDataGroup = nullptr;  ///< Titel wechselt je Modus
    QDateEdit* m_date           = nullptr;
    QLineEdit* m_ratioNew       = nullptr;
    QLineEdit* m_ratioOld       = nullptr;
    QLineEdit* m_factorPreview  = nullptr;  ///< read-only; Notationshinweis als Tooltip
    QCheckBox* m_pricesAdjusted = nullptr;
    QLineEdit* m_comment        = nullptr;
    QLineEdit* m_documentPath   = nullptr;
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

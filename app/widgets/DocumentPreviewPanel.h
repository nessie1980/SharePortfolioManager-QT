// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QWidget>
#include <QString>

class QLabel;
class QScrollArea;
class QProcess;

#ifdef SPM_HAVE_QTPDF
class QPdfDocument;
class QPdfView;
#endif

/**
 * @brief Wiederverwendbares "Dokumenten-Vorschau"-Panel für PDF-Dateien.
 *
 * Extrahiert 13.07.2026 aus `ViewDividendEdit::createPreviewPanel()` /
 * `openPdfPreview()` / `clearPdfPreview()` (identisch auch in ViewBuyEdit/
 * ViewSaleEdit/ViewBrokerageEdit/ViewShareAdd vorhanden) — siehe
 * ARCHITECTURE.md, Abschnitt "DocumentPreviewPanel". Zeigt PDF-Dokumente an:
 * nativer `QPdfView`, wenn `SPM_HAVE_QTPDF` beim Build gesetzt ist (inkl.
 * Zoom-Leiste), sonst ein per `pdftoppm` gerendertes PNG der ersten Seite in
 * einer QScrollArea.
 *
 * Rein passiv: kennt weder Presenter noch Datenmodell, nur einen Dateipfad.
 * Der Aufrufer entscheidet, wann welches Dokument angezeigt wird (z.B. bei
 * Zeilenauswahl in einem Formular, oder — wie in ShareDetailsForm — bei
 * `OverviewTabWidget::documentActivated()`).
 *
 * Existiert die Datei unter `showDocument(path)` nicht (mehr), wird das
 * **inline** im Panel angezeigt (`m_notFoundLabel`) statt über einen
 * blockierenden `OwnMessageBox::critical()`-Dialog — ein reines
 * Anzeige-Widget soll den aufrufenden Dialog/die Testausführung nicht durch
 * einen modalen Dialog unterbrechen (geändert 19.07.2026, siehe
 * ARCHITECTURE.md).
 *
 * Verwendet von:
 * - `ViewBuyEdit`, `ViewSaleEdit`, `ViewDividendEdit`, `ViewBrokerageEdit`,
 *   `ViewShareAdd` (Editier-Dialoge, rechtes Panel neben dem Formular).
 * - `ViewShareDetails` (reine Anzeige, rechtes Panel neben je einer
 *   OverviewTabWidget-Instanz in den Tabs Gewinne/Verluste, Dividenden, Kosten).
 */
class DocumentPreviewPanel : public QWidget
{
    Q_OBJECT

public:
    explicit DocumentPreviewPanel(QWidget* parent = nullptr);

    /**
     * @brief Lädt und zeigt das PDF-Dokument unter @p path an.
     * Ein leerer Pfad wirkt wie clearDocument(). Existiert die Datei nicht,
     * wird das inline im Panel angezeigt (kein blockierender Dialog).
     */
    void showDocument(const QString& path);

    /** Setzt die Anzeige auf den Platzhalter-Zustand zurück ("Kein Dokument ausgewählt."). */
    void clearDocument();

private:
    void buildUi();

    /// Inline-Fehlerlabel für "Datei nicht gefunden" — unabhängig vom
    /// Render-Pfad (SPM_HAVE_QTPDF oder pdftoppm-Fallback) vorhanden,
    /// standardmäßig ausgeblendet.
    QLabel* m_notFoundLabel = nullptr;

#ifdef SPM_HAVE_QTPDF
    QPdfDocument* m_pdfDocument = nullptr;
    QPdfView*     m_pdfView     = nullptr;
    QLabel*       m_zoomLabel   = nullptr;
#else
    QLabel*      m_pdfLabel      = nullptr;
    QScrollArea* m_pdfScroll     = nullptr;
    QString      m_pdfImagePath;
    QProcess*    m_pdfRenderProc = nullptr;
#endif
};

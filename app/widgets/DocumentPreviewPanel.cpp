// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "DocumentPreviewPanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QFrame>
#include <QTimer>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

#ifdef SPM_HAVE_QTPDF
#  include <QPdfView>
#  include <QPdfDocument>
#else
#  include <QPixmap>
#  include <QProcess>
#endif

// ── Constructor ───────────────────────────────────────────────────────────────

DocumentPreviewPanel::DocumentPreviewPanel(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

// ── buildUi ────────────────────────────────────────────────────────────────────
//
// Identisch zu ViewDividendEdit::createPreviewPanel() (auch in ViewBuyEdit/
// ViewSaleEdit/ViewBrokerageEdit/ViewShareAdd so vorhanden).
void DocumentPreviewPanel::buildUi()
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto* gb     = new QGroupBox(tr("  Dokumenten-Vorschau"));
    auto* layout = new QVBoxLayout(gb);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(4);

    // Inline-Fehleranzeige "Datei nicht gefunden" — unabhängig vom
    // Render-Pfad, standardmäßig ausgeblendet und nimmt dann keinen Platz
    // im Layout ein.
    m_notFoundLabel = new QLabel;
    m_notFoundLabel->setWordWrap(true);
    m_notFoundLabel->setStyleSheet(QStringLiteral("color: #ff4444;"));
    m_notFoundLabel->setVisible(false);
    layout->addWidget(m_notFoundLabel);

#ifdef SPM_HAVE_QTPDF
    m_pdfDocument = new QPdfDocument(this);
    m_pdfView     = new QPdfView(this);
    m_pdfView->setDocument(m_pdfDocument);
    m_pdfView->setPageMode(QPdfView::PageMode::MultiPage);
    m_pdfView->setZoomMode(QPdfView::ZoomMode::FitInView);
    m_pdfView->setFrameShape(QFrame::StyledPanel);
    m_pdfView->setStyleSheet(
        QStringLiteral("QPdfView { background-color: #ffffff; }"
                        "QPdfView > QWidget { background-color: #ffffff; }"));

    auto* zoomBar    = new QWidget;
    auto* zoomLayout = new QHBoxLayout(zoomBar);
    zoomLayout->setContentsMargins(0, 0, 0, 2);
    zoomLayout->setSpacing(4);

    auto* btnZoomOut = new QPushButton(QStringLiteral("−"));
    auto* btnZoomIn  = new QPushButton(QStringLiteral("+"));
    auto* btnFit     = new QPushButton(tr("Anpassen"));
    btnZoomOut->setFixedWidth(28);
    btnZoomIn->setFixedWidth(28);
    btnFit->setFixedWidth(80);

    m_zoomLabel = new QLabel(QStringLiteral("100%"));
    m_zoomLabel->setFixedWidth(48);
    m_zoomLabel->setAlignment(Qt::AlignCenter);

    zoomLayout->addWidget(btnZoomOut);
    zoomLayout->addWidget(btnZoomIn);
    zoomLayout->addWidget(btnFit);
    zoomLayout->addWidget(m_zoomLabel);
    zoomLayout->addStretch(1);

    connect(btnZoomIn, &QPushButton::clicked, this, [this] {
        const qreal z = qMin(m_pdfView->zoomFactor() * 1.25, 4.0);
        m_pdfView->setZoomMode(QPdfView::ZoomMode::Custom);
        m_pdfView->setZoomFactor(z);
        m_zoomLabel->setText(QString::number(qRound(z * 100)) + QStringLiteral("%"));
    });
    connect(btnZoomOut, &QPushButton::clicked, this, [this] {
        const qreal z = qMax(m_pdfView->zoomFactor() * 0.8, 0.25);
        m_pdfView->setZoomMode(QPdfView::ZoomMode::Custom);
        m_pdfView->setZoomFactor(z);
        m_zoomLabel->setText(QString::number(qRound(z * 100)) + QStringLiteral("%"));
    });
    connect(btnFit, &QPushButton::clicked, this, [this] {
        m_pdfView->setZoomMode(QPdfView::ZoomMode::FitInView);
        m_zoomLabel->setText(tr("Anp."));
    });

    layout->addWidget(zoomBar);
    layout->addWidget(m_pdfView, 1);
#else
    m_pdfLabel = new QLabel;
    m_pdfLabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    m_pdfLabel->setText(tr("Kein Dokument ausgewählt."));
    m_pdfLabel->setWordWrap(true);
    m_pdfScroll = new QScrollArea;
    m_pdfScroll->setWidget(m_pdfLabel);
    m_pdfScroll->setWidgetResizable(true);
    m_pdfScroll->setFrameShape(QFrame::StyledPanel);
    layout->addWidget(m_pdfScroll, 1);
#endif

    outer->addWidget(gb);
}

// ── clearDocument ──────────────────────────────────────────────────────────────

void DocumentPreviewPanel::clearDocument()
{
    m_notFoundLabel->setVisible(false);

#ifdef SPM_HAVE_QTPDF
    m_pdfDocument->close();
#else
    if (m_pdfRenderProc) {
        m_pdfRenderProc->kill();
        m_pdfRenderProc->deleteLater();
        m_pdfRenderProc = nullptr;
    }
    m_pdfLabel->clear();
    m_pdfLabel->setText(tr("Kein Dokument ausgewählt."));
#endif
}

// ── showDocument ───────────────────────────────────────────────────────────────

void DocumentPreviewPanel::showDocument(const QString& path)
{
    if (path.isEmpty()) {
        clearDocument();
        return;
    }

    // Explizite Existenzprüfung statt stillschweigend leer/fehlerhaft zu
    // rendern — der Benutzer bekommt eine klare Fehlermeldung, wenn das
    // Dokument (z.B. nach Verschieben/Löschen der Datei) nicht mehr existiert.
    //
    // Ursprünglich (13.07.2026) über OwnMessageBox::critical() als
    // blockierenden Dialog gelöst — auf ein reines Anzeige-Widget bezogen
    // ist ein modaler Dialog aber unpassend (unterbricht z.B. automatisierte
    // Tests, die diesen Pfad auslösen, siehe test_viewBrokerageEdit_
    // openPdfPreview_nonExistentFile_doesNotCrash in tst_mainwindow.cpp) —
    // daher seit 19.07.2026 stattdessen eine inline Anzeige im Panel selbst
    // (m_notFoundLabel), analog zu den bereits vorhandenen Inline-Fehlern
    // im pdftoppm-Fallback-Zweig ("PDF-Vorschau konnte nicht gerendert
    // werden.", "Vorschaubild nicht gefunden.").
    if (!QFileInfo::exists(path)) {
        qWarning() << "[DocumentPreviewPanel] Document not found:" << path;
        clearDocument();
        m_notFoundLabel->setText(tr("Das Dokument wurde nicht gefunden:\n%1").arg(path));
        m_notFoundLabel->setVisible(true);
        return;
    }

    m_notFoundLabel->setVisible(false);

#ifdef SPM_HAVE_QTPDF
    m_pdfDocument->close();
    m_pdfDocument->load(path);
    m_pdfView->setZoomMode(QPdfView::ZoomMode::FitInView);
    QTimer::singleShot(100, this, [this]() {
        m_pdfView->setZoomMode(QPdfView::ZoomMode::FitInView);
    });
#else
    m_pdfImagePath = QDir::tempPath() + QStringLiteral("/spm_docpreview_panel");
    if (m_pdfRenderProc) { m_pdfRenderProc->kill(); m_pdfRenderProc->deleteLater(); }
    m_pdfRenderProc = new QProcess(this);
    connect(m_pdfRenderProc,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
        if (m_pdfRenderProc) { m_pdfRenderProc->deleteLater(); m_pdfRenderProc = nullptr; }
        if (exitCode != 0) {
            m_pdfLabel->setText(tr("PDF-Vorschau konnte nicht gerendert werden."));
            return;
        }
        const QString imgPath = m_pdfImagePath + QStringLiteral("-1.png");
        QPixmap px(imgPath);
        if (px.isNull()) { m_pdfLabel->setText(tr("Vorschaubild nicht gefunden.")); return; }
        const int availW = m_pdfScroll->viewport()->width() - 4;
        if (availW > 0 && px.width() > availW)
            px = px.scaledToWidth(availW, Qt::SmoothTransformation);
        m_pdfLabel->setPixmap(px);
        m_pdfLabel->resize(px.size());
    });
    m_pdfRenderProc->start(QStringLiteral("pdftoppm"),
                           { QStringLiteral("-r"),   QStringLiteral("150"),
                             QStringLiteral("-png"),
                             QStringLiteral("-f"),   QStringLiteral("1"),
                             QStringLiteral("-l"),   QStringLiteral("1"),
                             path, m_pdfImagePath });
#endif
}

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "PdfTextExtractor.h"

#include <QProcess>

// ── Constructor ───────────────────────────────────────────────────────────────

PdfTextExtractor::PdfTextExtractor(QObject* parent)
    : QObject(parent)
{
}

// ── extract ───────────────────────────────────────────────────────────────────

void PdfTextExtractor::extract(const QString& pdfPath)
{
    // Same invocation as previously duplicated in PresenterBuyEdit::
    // onDocumentSelected() / PresenterSaleEdit::onDocumentSelected() /
    // PresenterDividendEdit::onDocumentSelected() / PresenterShareAdd::
    // onDocumentSelected() — kept byte-for-byte identical so behaviour does
    // not change for any existing call site that migrates to this class.
    m_process = new QProcess(this);
    connect(m_process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,
            &PdfTextExtractor::onProcessFinished);

    const QStringList args = {
        QStringLiteral("-enc"),    QStringLiteral("UTF-8"),
        QStringLiteral("-layout"),
        pdfPath,
        QStringLiteral("-")        // write to stdout
    };
    m_process->start(QStringLiteral("pdftotext"), args);
}

// ── onProcessFinished ─────────────────────────────────────────────────────────

void PdfTextExtractor::onProcessFinished(int exitCode, int /*exitStatus*/)
{
    auto* proc = qobject_cast<QProcess*>(sender());
    const QByteArray stdoutData = proc ? proc->readAllStandardOutput() : QByteArray();
    if (proc) proc->deleteLater();
    m_process = nullptr;

    if (exitCode != 0 || stdoutData.isEmpty()) {
        emit finished(false, QString());
        return;
    }

    emit finished(true, QString::fromUtf8(stdoutData));
}

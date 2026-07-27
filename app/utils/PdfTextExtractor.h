// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QObject>
#include <QString>

class QProcess;

/**
 * @brief Converts a PDF file to plain text via the external `pdftotext` tool.
 *
 * Wraps the `QProcess`-based `pdftotext` invocation that was previously
 * duplicated in `PresenterBuyEdit`, `PresenterSaleEdit`,
 * `PresenterDividendEdit` and `PresenterShareAdd` (see ARCHITECTURE.md,
 * "PDF-Text-Extraktion gebündelt in PdfTextExtractor"). Those presenters
 * still run their own `pdftotext` call directly for now — only the new
 * "Direkte Dokumentenerfassung" drop feature in `MainWindow` uses this
 * class, since it needs the raw text *before* any dialog/presenter exists
 * to hand it to.
 *
 * Runs `pdftotext -enc UTF-8 -layout <path> -` asynchronously (writes to
 * stdout). Connect to finished() to receive the result.
 *
 * One `PdfTextExtractor` instance handles one conversion at a time; create
 * a new instance (or wait for finished()) before starting another.
 */
class PdfTextExtractor : public QObject
{
    Q_OBJECT

public:
    explicit PdfTextExtractor(QObject* parent = nullptr);
    ~PdfTextExtractor() override = default;

    /**
     * @brief Start converting the given PDF file to text.
     * @param pdfPath  Full path to the PDF file.
     *
     * Asynchronous — returns immediately. Emits finished() once `pdftotext`
     * exits, on both success and failure.
     */
    void extract(const QString& pdfPath);

signals:
    /**
     * @brief Emitted when the pdftotext conversion has finished.
     * @param success  true if `pdftotext` exited with code 0 and produced
     *                 non-empty output.
     * @param text     Extracted plain text (UTF-8), empty on failure.
     */
    void finished(bool success, const QString& text);

private slots:
    void onProcessFinished(int exitCode, int exitStatus);

private:
    QProcess* m_process = nullptr; ///< Owned via QObject parent-child (this)
};

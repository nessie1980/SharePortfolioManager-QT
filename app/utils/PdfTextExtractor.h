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
 * "PDF-Text-Extraktion gebündelt in PdfTextExtractor"). All four presenters
 * plus the "Direkte Dokumentenerfassung" drop feature in `MainWindow` use
 * this class today.
 *
 * Runs `pdftotext -enc UTF-8 -layout <path> -` asynchronously (writes to
 * stdout). Connect to finished() to receive the result.
 *
 * ## Process lifetime
 *
 * One instance runs at most one `pdftotext` process at a time. The class
 * owns that process end to end — callers do not have to sequence their
 * calls (see ARCHITECTURE.md, "Zurückbleibender pdftotext-Prozess"):
 *
 * - extract() while a conversion is running cancels the running one and
 *   starts the new one. The cancelled conversion emits nothing.
 * - cancel() stops a running conversion without emitting anything.
 * - The destructor stops a running conversion. Nothing is left behind and
 *   no signal is emitted from the teardown.
 *
 * finished() is therefore emitted at most once per extract() call, and
 * never for a conversion the caller has already replaced or cancelled.
 */
class PdfTextExtractor : public QObject
{
    Q_OBJECT

public:
    explicit PdfTextExtractor(QObject* parent = nullptr);

    /**
     * @brief Stops a running conversion, then destroys the object.
     *
     * Deliberately not `= default`: the compiler-generated destructor left
     * the running `QProcess` to `~QObject`, which kills it and then waits
     * for it WITHOUT a timeout — in the GUI thread. Closing a dialog during
     * a conversion blocked the user interface for as long as `pdftotext`
     * took to die.
     */
    ~PdfTextExtractor() override;

    /**
     * @brief Start converting the given PDF file to text.
     * @param pdfPath  Full path to the PDF file.
     *
     * Asynchronous — returns immediately. Emits finished() once `pdftotext`
     * exits, on both success and failure, including the case where the tool
     * is not installed at all.
     *
     * A conversion that is still running when this is called is cancelled
     * (see cancel()); its result is discarded. The caller always hears back
     * about the LAST path it passed in, never about an earlier one.
     */
    void extract(const QString& pdfPath);

    /**
     * @brief Stop a running conversion.
     *
     * Does nothing if none is running. Emits NO signal — a cancellation is
     * not a failed conversion, and a caller that cancels is by definition
     * no longer interested in the result. Safe to call repeatedly.
     */
    void cancel();

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
    /**
     * @brief Kill and delete the running process without emitting anything.
     *
     * Disconnects first, so neither onProcessFinished() nor the start-failure
     * handler runs as a side effect of the teardown.
     */
    void stopProcess();

    /**
     * @brief Report `QProcess::FailedToStart` as a failed conversion.
     *
     * `QProcess` emits errorOccurred() but NOT finished() when the program
     * cannot be started at all. Without this, a missing `pdftotext` left the
     * caller waiting for a signal that never came.
     */
    void handleStartFailure();

    QProcess* m_process = nullptr; ///< Owned via QObject parent-child (this)
};

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
 *
 * ## Fehlender Wandler
 *
 * extract() prueft NICHT, ob `pdftotext` ueberhaupt vorhanden ist — es
 * startet den Prozess und meldet den Fehlstart als `finished(false, …)`.
 * Die Vorpruefung liegt bei den Aufrufstellen, die converterInfo() vor dem
 * Aufruf befragen und converterMissingMessage() anzeigen (Nessies
 * Entscheidung gegen einen zweiten Riegel hier, 03.09.2026). Ein Riegel an
 * beiden Enden waere doppelt gemoppelt; der Preis ist, dass ein kuenftiger
 * sechster Aufrufer die Pruefung selbst mitbringen muss.
 */
class PdfTextExtractor : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Name und Version des installierten PDF-Wandlers.
     *
     * Ermittelt aus der Ausgabe von `pdftotext -v` (siehe converterInfo()).
     * Die Struktur lag bis zum 03.09.2026 als privat verschachtelter Typ in
     * `AboutForm`; sie ist hierher gewandert, weil die Ermittlung jetzt zwei
     * Aufrufer hat und ein Anzeigefenster kein Ort fuer Systempruefungen ist.
     */
    struct ConverterInfo {
        /**
         * @brief true, wenn ein benutzbarer Wandler geantwortet hat.
         *
         * Ergaenzt 03.09.2026. Bis dahin liess sich der Fall nur daran
         * ablesen, dass @ref name auf dem Text "nicht gefunden" stand — ein
         * uebersetzter Anzeigetext als Zustandsmerkmal, der bei jeder
         * Sprachumstellung gebrochen waere.
         *
         * false deckt zwei Faelle ab, die sich fuer den Aufrufer gleich
         * auswirken: `pdftotext` ist nicht installiert, oder es antwortet in
         * einer Form, aus der sich nichts lesen laesst. In beiden Faellen
         * koennen keine Belege eingelesen werden.
         */
        bool    available = false;

        QString name;    ///< "Poppler", "XpdfReader" oder "nicht gefunden"
        QString version; ///< Versionsangabe oder "unbekannt"
    };

    /**
     * @brief Ermittelt einmalig, welcher PDF-Wandler installiert ist.
     *
     * Ruft `pdftotext -v` auf und wertet die Ausgabe aus. Das Ergebnis wird
     * beim ersten Aufruf gemerkt und danach unveraendert zurueckgegeben —
     * auch ein negatives (Nessies Entscheidung, 03.09.2026). Es gibt also
     * genau einen Prozessstart je Programmlauf.
     *
     * @note Die Folge des Merkens: wird `pdftotext` waehrend des laufenden
     * Programms nachinstalliert, bemerkt die Anwendung das bis zum Neustart
     * nicht. Deshalb nennt converterMissingMessage() den Neustart
     * ausdruecklich — sonst stuende der Benutzer vor einer Sperre, die er
     * gerade beseitigt hat.
     *
     * @note Die Wartezeit liegt bei einer Sekunde (vorher drei in
     * `AboutForm`). Ein fehlendes Programm meldet `FailedToStart` und wartet
     * gar nicht; die Schranke greift nur, wenn ein vorhandenes `pdftotext`
     * haengt — und `pdftotext -v` antwortet in Millisekunden oder nie.
     */
    static const ConverterInfo& converterInfo();

    /**
     * @brief Der Text, den alle Aufrufstellen bei fehlendem Wandler zeigen.
     *
     * An einer Stelle, damit die fuenf Aufrufstellen und die Startmeldung
     * nicht auseinanderlaufen und nur ein `tr()`-Eintrag entsteht.
     */
    static QString converterMissingMessage();

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

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "PdfTextExtractor.h"

#include <QProcess>
#include <QRegularExpression>
#include <QDebug>

namespace {

/**
 * @brief Fragt `pdftotext -v` ab und wertet die Ausgabe aus.
 *
 * Wortgleich uebernommen aus AboutForm::pdftotextInfo() (dort entfernt),
 * mit zwei Aenderungen: die Wartezeit liegt bei einer statt drei Sekunden,
 * und das Ergebnis traegt ein `available`-Kennzeichen statt den Zustand
 * "nicht gefunden" nur im uebersetzten Anzeigenamen zu fuehren.
 */
PdfTextExtractor::ConverterInfo detectConverter()
{
    QProcess process;
    process.start(QStringLiteral("pdftotext"),
                  QStringList() << QStringLiteral("-v"));

    // Eine Sekunde statt drei: ein fehlendes Programm meldet FailedToStart
    // und wartet gar nicht, die Schranke greift also nur bei einem
    // vorhandenen, aber haengenden pdftotext. "pdftotext -v" antwortet in
    // Millisekunden oder nie.
    process.waitForFinished(1000);

    // pdftotext schreibt die Versionsangabe nach stderr.
    const QString output = QString::fromLocal8Bit(process.readAllStandardError())
                         + QString::fromLocal8Bit(process.readAllStandardOutput());

    if (output.isEmpty() || process.error() == QProcess::FailedToStart) {
        qWarning() << "[PdfTextExtractor] pdftotext nicht gefunden.";
        return { false,
                 PdfTextExtractor::tr("nicht gefunden"),
                 PdfTextExtractor::tr("unbekannt") };
    }

    // Welche Implementierung liegt vor?
    //   XpdfReader:  "pdftotext version 4.xx"            (ohne "Poppler")
    //   Poppler:     "pdftotext version 24.xx" plus eine Zeile mit "Poppler"
    const bool isPoppler =
        output.contains(QStringLiteral("Poppler"), Qt::CaseInsensitive);

    const QRegularExpression versionRegex(
        QStringLiteral("version\\s+([0-9]+\\.[0-9]+(?:\\.[0-9]+)?)"),
        QRegularExpression::CaseInsensitiveOption);
    const auto match = versionRegex.match(output);
    const QString version = match.hasMatch()
        ? match.captured(1)
        : PdfTextExtractor::tr("unbekannt");

    return { true,
             isPoppler ? QStringLiteral("Poppler") : QStringLiteral("XpdfReader"),
             version };
}

} // namespace

// ── converterInfo ─────────────────────────────────────────────────────────────

const PdfTextExtractor::ConverterInfo& PdfTextExtractor::converterInfo()
{
    // Funktionslokales static: der Prozess laeuft genau einmal je
    // Programmlauf, beim ersten Aufruf. Das Ergebnis wird in BEIDEN
    // Richtungen gemerkt (Nessies Entscheidung, 03.09.2026) — auch ein
    // "nicht gefunden" bleibt bis zum Neustart stehen. Siehe die Notiz im
    // Header und converterMissingMessage(), die den Neustart deshalb nennt.
    static const ConverterInfo info = detectConverter();
    return info;
}

// ── converterMissingMessage ───────────────────────────────────────────────────

QString PdfTextExtractor::converterMissingMessage()
{
    // Bewusst nicht "Poppler ist nicht installiert": converterInfo() meldet
    // auch dann kein Ergebnis, wenn ein vorhandenes pdftotext in einer Form
    // antwortet, die sich nicht lesen laesst. Der Satz muss in beiden Faellen
    // stimmen und trotzdem sagen, was zu tun ist.
    return tr("Kein PDF-Wandler gefunden — Belege können nicht eingelesen "
              "werden. Bitte Poppler (pdftotext) installieren und die "
              "Anwendung neu starten.");
}

// ── Constructor ───────────────────────────────────────────────────────────────

PdfTextExtractor::PdfTextExtractor(QObject* parent)
    : QObject(parent)
{
}

// ── Destructor ────────────────────────────────────────────────────────────────

PdfTextExtractor::~PdfTextExtractor()
{
    stopProcess();
}

// ── extract ───────────────────────────────────────────────────────────────────

void PdfTextExtractor::extract(const QString& pdfPath)
{
    // A conversion that is still running belongs to a document the caller has
    // already moved on from — pick another file in the dialog and the old
    // result is of no use to anyone. Cancel it instead of letting a second
    // process run alongside: the previous implementation overwrote m_process
    // and the orphan reported back through sender(), delivering the text of
    // the PREVIOUS document under the new one's name.
    stopProcess();

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

    // QProcess reports a program that cannot be started at all through
    // errorOccurred() ONLY — finished() never arrives in that case. A lambda
    // rather than a named slot so the header can keep its forward declaration
    // of QProcess instead of pulling in the full header for the enum.
    connect(m_process, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
                if (error == QProcess::FailedToStart)
                    handleStartFailure();
            });

    const QStringList args = {
        QStringLiteral("-enc"),    QStringLiteral("UTF-8"),
        QStringLiteral("-layout"),
        pdfPath,
        QStringLiteral("-")        // write to stdout
    };

    // start() stays the LAST statement on purpose: on Windows a failing
    // CreateProcess makes QProcess emit errorOccurred() synchronously, so
    // handleStartFailure() — and with it finished(false, …) — can run before
    // this call returns. Nothing may touch m_process afterwards.
    m_process->start(QStringLiteral("pdftotext"), args);
}

// ── cancel ────────────────────────────────────────────────────────────────────

void PdfTextExtractor::cancel()
{
    stopProcess();
}

// ── stopProcess ───────────────────────────────────────────────────────────────

void PdfTextExtractor::stopProcess()
{
    if (!m_process)
        return;

    QProcess* proc = m_process;
    m_process = nullptr;

    // Disconnect BEFORE killing: otherwise kill() triggers finished() with a
    // crash exit status and the caller would be told the conversion failed,
    // when in truth it was withdrawn.
    proc->disconnect(this);

    if (proc->state() != QProcess::NotRunning) {
        // kill() rather than terminate(): pdftotext writes its output to
        // stdout and holds no state worth shutting down cleanly, and
        // terminate() does not reach a console application on Windows.
        proc->kill();
        proc->waitForFinished(500);
    }

    // Deleted right here, not via deleteLater(): the destructor is one of the
    // callers, and there may be no event loop left to run deferred deletions.
    delete proc;
}

// ── handleStartFailure ────────────────────────────────────────────────────────

void PdfTextExtractor::handleStartFailure()
{
    QProcess* proc = m_process;
    m_process = nullptr;
    if (!proc)
        return;

    proc->disconnect(this);
    proc->deleteLater();   // we are inside one of its own signals

    emit finished(false, QString());
}

// ── onProcessFinished ─────────────────────────────────────────────────────────

void PdfTextExtractor::onProcessFinished(int exitCode, int /*exitStatus*/)
{
    QProcess* proc = m_process;
    m_process = nullptr;
    if (!proc)
        return;

    const QByteArray stdoutData = proc->readAllStandardOutput();

    proc->disconnect(this);
    proc->deleteLater();   // we are inside one of its own signals

    // m_process is already cleared at this point: a receiver that starts the
    // next conversion straight from this slot must not find a half-torn-down
    // predecessor.
    if (exitCode != 0 || stdoutData.isEmpty()) {
        emit finished(false, QString());
        return;
    }

    emit finished(true, QString::fromUtf8(stdoutData));
}

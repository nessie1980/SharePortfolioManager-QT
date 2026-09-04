// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../../utils/PdfTextExtractor.h"

#include <QDialog>
#include <QPushButton>
#include <QString>

/**
 * @brief About dialog showing version info, PDF converter and icon credits.
 *
 * Displays:
 * - Application and library version numbers
 * - PDF converter name (Poppler or XpdfReader) and version, detected at runtime
 * - PDF converter and icon set credits with links
 *
 * The "Zwischenablage" button copies all version info to the clipboard.
 */
class AboutForm : public QDialog
{
    Q_OBJECT

public:
    explicit AboutForm(QWidget* parent = nullptr);

private slots:
    void onCopyToClipboard();

private:
    void setupUi();

    /**
     * @brief Ermitteltes Ergebnis, gesetzt in setupUi().
     *
     * Die Ermittlung selbst liegt seit dem 03.09.2026 in
     * `PdfTextExtractor::converterInfo()`. Bis dahin gehoerte sie diesem
     * Dialog — als privat verschachtelte Struktur samt privater statischer
     * Methode. Sie hat jetzt einen zweiten Aufrufer
     * (`MainWindow::checkAndLoadConfigurations()`), und ein Anzeigefenster
     * ist kein Ort, an dem Systempruefungen wohnen. Siehe ARCHITECTURE.md,
     * "Fehlendes pdftotext wird nicht als solches benannt".
     *
     * @note Der Wert kommt aus einem gemerkten Ergebnis: der Prozess laeuft
     * genau einmal je Programmlauf. Wird `pdftotext` waehrend des Betriebs
     * nachinstalliert, zeigt dieser Dialog bis zum Neustart weiterhin
     * "nicht gefunden".
     */
    PdfTextExtractor::ConverterInfo m_pdfInfo;

    QPushButton* m_btnClipboard = nullptr;
    QPushButton* m_btnOk        = nullptr;
};

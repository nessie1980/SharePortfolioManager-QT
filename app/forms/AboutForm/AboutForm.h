// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

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
    /**
     * @brief Holds the name and version of the installed PDF converter.
     */
    struct PdfConverterInfo {
        QString name;    ///< "XpdfReader", "Poppler" or "nicht gefunden"
        QString version; ///< Version string or "unbekannt"
    };

    void setupUi();

    /**
     * @brief Detect the installed pdftotext implementation and version.
     *
     * Runs `pdftotext -v` via QProcess and inspects the output to determine
     * whether XpdfReader or Poppler is installed.
     * @return PdfConverterInfo with name and version string.
     */
    static PdfConverterInfo pdftotextInfo();

    PdfConverterInfo m_pdfInfo; ///< Cached PDF converter info (set in setupUi)

    QPushButton* m_btnClipboard = nullptr;
    QPushButton* m_btnOk        = nullptr;
};

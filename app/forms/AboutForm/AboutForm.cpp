// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "AboutForm.h"
#include "../../IconProvider.h"
#include "../../core/Database.h"
#include "../../../libs/logger/src/Logger.h"
#include "../../../libs/parser/src/Parser.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QCoreApplication>
#include <QClipboard>
#include <QGuiApplication>
#include <QDebug>

// ── Constructor ───────────────────────────────────────────────────────────────

AboutForm::AboutForm(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Über"));
    setMinimumWidth(400);
    setModal(true);
    setupUi();
}

// ── setupUi ───────────────────────────────────────────────────────────────────

void AboutForm::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    // ── Versionen ─────────────────────────────────────────────────────────
    auto* grpVersions = new QGroupBox(tr("  Versionen"), this);
    auto* versionsLayout = new QGridLayout(grpVersions);
    versionsLayout->setColumnStretch(0, 2);
    versionsLayout->setColumnStretch(1, 1);

    auto addVersionRow = [&](int row, const QString& label, const QString& value) {
        auto* lblName = new QLabel(label, grpVersions);
        lblName->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto* lblValue = new QLabel(value, grpVersions);
        lblValue->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        versionsLayout->addWidget(lblName,  row, 0);
        versionsLayout->addWidget(lblValue, row, 1);
    };

    // Ermittlung liegt seit dem 03.09.2026 in PdfTextExtractor; der Wert ist
    // dort gemerkt, dieser Aufruf startet also keinen weiteren Prozess.
    const auto pdfInfo = PdfTextExtractor::converterInfo();
    const QString pdfLabel = tr("PDF- Konverter (%1):").arg(pdfInfo.name);

    addVersionRow(0, tr("Programm- Version:"),      QCoreApplication::applicationVersion());
    addVersionRow(1, tr("Parser- Lib- Version:"),   ParserLib::Parser::version());
    addVersionRow(2, tr("Logger- Lib- Version:"),   Logging::Logger::version());
    addVersionRow(3, tr("Database- Lib- Version:"), Database::version());
    addVersionRow(4, pdfLabel,                      pdfInfo.version);

    mainLayout->addWidget(grpVersions);

    // ── PDF-Konverter ─────────────────────────────────────────────────────
    auto* grpPdf = new QGroupBox(
        tr("  PDF- Konverter (%1)").arg(pdfInfo.name), this);
    auto* pdfLayout = new QVBoxLayout(grpPdf);

    auto* lblPdfText = new QLabel(
        tr("Der benutzte PDF- Konverter stammt von:"), grpPdf);
    lblPdfText->setAlignment(Qt::AlignCenter);

    // Link depends on which converter is installed
    const QString pdfUrl = pdfInfo.name == QStringLiteral("Poppler")
        ? QStringLiteral("https://poppler.freedesktop.org")
        : QStringLiteral("https://www.xpdfreader.com");
    const QString pdfLinkText = pdfInfo.name == QStringLiteral("Poppler")
        ? tr("Link zu Poppler")
        : tr("Link zum XpdfReader");

    auto* lblPdfLink = new QLabel(
        QStringLiteral("<a href=\"%1\">%2</a>").arg(pdfUrl, pdfLinkText), grpPdf);
    lblPdfLink->setAlignment(Qt::AlignCenter);
    lblPdfLink->setOpenExternalLinks(true);
    lblPdfLink->setTextInteractionFlags(Qt::TextBrowserInteraction);

    pdfLayout->addWidget(lblPdfText);
    pdfLayout->addWidget(lblPdfLink);
    mainLayout->addWidget(grpPdf);

    // ── Icons ─────────────────────────────────────────────────────────────
    auto* grpIcons = new QGroupBox(tr("  Icons"), this);
    auto* iconsLayout = new QVBoxLayout(grpIcons);

    auto* lblIconText = new QLabel(
        tr("Die benutzten Icons stammen von:"), grpIcons);
    lblIconText->setAlignment(Qt::AlignCenter);

    auto* lblIconLink = new QLabel(
        QStringLiteral("<a href=\"https://icons8.com\">Link zu Icon8</a>"),
        grpIcons);
    lblIconLink->setAlignment(Qt::AlignCenter);
    lblIconLink->setOpenExternalLinks(true);
    lblIconLink->setTextInteractionFlags(Qt::TextBrowserInteraction);

    iconsLayout->addWidget(lblIconText);
    iconsLayout->addWidget(lblIconLink);
    mainLayout->addWidget(grpIcons);

    // ── Buttons ───────────────────────────────────────────────────────────
    auto* btnLayout = new QHBoxLayout();

    m_btnClipboard = new QPushButton(
        IconProvider::icon(IconProvider::ButtonClipboard),
        tr("Zwischenablage"), this);
    m_btnOk = new QPushButton(tr("Ok"), this);
    m_btnOk->setDefault(true);

    btnLayout->addWidget(m_btnClipboard);
    btnLayout->addStretch();
    btnLayout->addWidget(m_btnOk);
    mainLayout->addLayout(btnLayout);

    // Store pdfInfo for clipboard use
    m_pdfInfo = pdfInfo;

    connect(m_btnClipboard, &QPushButton::clicked,
            this, &AboutForm::onCopyToClipboard);
    connect(m_btnOk, &QPushButton::clicked, this, &QDialog::accept);
}

// ── onCopyToClipboard ─────────────────────────────────────────────────────────

void AboutForm::onCopyToClipboard()
{
    const QString text =
        QStringLiteral("Share Portfolio Manager\n"
                       "Programm-Version:         %1\n"
                       "Parser-Lib-Version:        %2\n"
                       "Logger-Lib-Version:        %3\n"
                       "Database-Lib-Version:      %4\n"
                       "PDF-Konverter (%5):        %6\n")
            .arg(QCoreApplication::applicationVersion(),
                 ParserLib::Parser::version(),
                 Logging::Logger::version(),
                 Database::version(),
                 m_pdfInfo.name,
                 m_pdfInfo.version);

    QGuiApplication::clipboard()->setText(text);
    qInfo() << "[AboutForm] Version info copied to clipboard.";
}

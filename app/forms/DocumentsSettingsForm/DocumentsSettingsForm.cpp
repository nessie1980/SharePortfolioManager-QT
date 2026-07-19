// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "DocumentsSettingsForm.h"
#include "../../config/AppSettings.h"
#include "../../core/DocumentRootMigrator.h"
#include "../../IconProvider.h"
#include "../UiConstants.h"
#include "../OwnMessageBoxForm/OwnMessageBox.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

// ── Constructor ───────────────────────────────────────────────────────────────

DocumentsSettingsForm::DocumentsSettingsForm(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Dokumente"));
    setMinimumWidth(560);
    setModal(true);

    setupUi();
    loadSettings();
}

// ── setupUi ───────────────────────────────────────────────────────────────────

void DocumentsSettingsForm::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    // ── Hinweistext ─────────────────────────────────────────────────────────
    m_lblInfo = new QLabel(this);
    m_lblInfo->setWordWrap(true);
    m_lblInfo->setText(
        tr("Alle Dokumentpfade (Kauf, Verkauf, Kosten, Dividende), die mit dem "
           "alten Root-Pfad beginnen, werden beim Bestätigen auf den neuen "
           "Root-Pfad umgestellt. Die Dateien selbst müssen dafür bereits am "
           "neuen Ort liegen — es werden keine Dateien verschoben oder kopiert."));
    mainLayout->addWidget(m_lblInfo);

    m_lblHint = new QLabel(this);
    m_lblHint->setWordWrap(true);
    m_lblHint->setVisible(false);
    mainLayout->addWidget(m_lblHint);

    // ── Alter Root-Pfad ─────────────────────────────────────────────────────
    auto* grpOld = new QGroupBox(tr("  Alter Root-Pfad"), this);
    auto* oldLayout = new QHBoxLayout(grpOld);
    m_editOldRoot = new QLineEdit(grpOld);
    m_editOldRoot->setObjectName(QStringLiteral("editOldRoot"));
    m_editOldRoot->setFixedHeight(UiConstants::kFieldHeight);
    m_editOldRoot->setPlaceholderText(tr("z. B. B:\\Depot oder /home/alt/Belege"));
    oldLayout->addWidget(m_editOldRoot);
    mainLayout->addWidget(grpOld);

    // ── Neuer Root-Pfad ─────────────────────────────────────────────────────
    auto* grpNew = new QGroupBox(tr("  Neuer Root-Pfad"), this);
    auto* newLayout = new QHBoxLayout(grpNew);
    m_editNewRoot = new QLineEdit(grpNew);
    m_editNewRoot->setObjectName(QStringLiteral("editNewRoot"));
    m_editNewRoot->setFixedHeight(UiConstants::kFieldHeight);
    m_editNewRoot->setReadOnly(true);
    newLayout->addWidget(m_editNewRoot, 1);
    m_btnBrowseNew = new QPushButton(tr("Durchsuchen..."), grpNew);
    m_btnBrowseNew->setFixedHeight(UiConstants::kFieldHeight);
    connect(m_btnBrowseNew, &QPushButton::clicked, this, &DocumentsSettingsForm::onBrowseNewRoot);
    newLayout->addWidget(m_btnBrowseNew);
    mainLayout->addWidget(grpNew);

    // ── Buttons ─────────────────────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();

    m_btnCancel = new QPushButton(IconProvider::icon(IconProvider::ButtonCancel),
                                  tr("Abbrechen"), this);
    m_btnCancel->setFixedHeight(UiConstants::kButtonHeight);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    btnRow->addWidget(m_btnCancel);

    m_btnOk = new QPushButton(IconProvider::icon(IconProvider::ButtonSave),
                              tr("OK"), this);
    m_btnOk->setFixedHeight(UiConstants::kButtonHeight);
    m_btnOk->setDefault(true);
    connect(m_btnOk, &QPushButton::clicked, this, &DocumentsSettingsForm::onOk);
    btnRow->addWidget(m_btnOk);

    mainLayout->addLayout(btnRow);
}

// ── loadSettings ──────────────────────────────────────────────────────────────

void DocumentsSettingsForm::loadSettings()
{
    const QString configured = AppSettings::instance().documentsRootPath();
    if (!configured.isEmpty()) {
        // Ein Root ist bereits bekannt — das ist der naheliegende Ausgangspunkt
        // für "alt", falls der Benutzer erneut umziehen will.
        m_editOldRoot->setText(configured);
        return;
    }

    // Noch kein Root konfiguriert: automatische, betriebssystemunabhängige
    // Erkennung anhand der bereits vorhandenen Dokumente versuchen — rein
    // lesend, kein DB-Write. Der Benutzer sieht und kann den Vorschlag
    // jederzeit überschreiben.
    const auto detection = DocumentRootMigrator::detectCommonRoot();

    if (!detection.suggestedRoot.isEmpty()) {
        m_editOldRoot->setText(detection.suggestedRoot);
        m_lblHint->setText(
            tr("Anhand der vorhandenen Dokumente automatisch erkannt. Bei "
               "Bedarf anpassen, bevor Sie fortfahren."));
        m_lblHint->setVisible(true);
        return;
    }

    if (detection.ambiguous) {
        m_lblHint->setText(
            tr("Die vorhandenen Dokumente liegen nicht alle im selben "
               "Verzeichnis — es kann kein alter Root-Pfad automatisch "
               "vorgeschlagen werden. Bitte tragen Sie ihn manuell ein, oder "
               "lassen Sie das Feld leer, wenn nur der neue Root-Pfad "
               "gesetzt werden soll."));
        m_lblHint->setVisible(true);
        return;
    }

    if (detection.absoluteCount == 0 && detection.relativeCount > 0) {
        m_lblHint->setText(
            tr("Für die %1 vorhandenen Dokumente konnte kein Verzeichnis "
               "automatisch ermittelt werden. Bitte tragen Sie den alten "
               "Root-Pfad manuell ein, oder lassen Sie das Feld leer, wenn "
               "nur der neue Root-Pfad gesetzt werden soll.")
                .arg(detection.relativeCount));
        m_lblHint->setVisible(true);
    }

    // detection.total() == 0 — noch keine Dokumente vorhanden, kein Hinweis nötig.
}

// ── onBrowseNewRoot ───────────────────────────────────────────────────────────

void DocumentsSettingsForm::onBrowseNewRoot()
{
    const QString startDir = m_editNewRoot->text().isEmpty()
        ? QDir::homePath() : m_editNewRoot->text();
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Neuen Root-Pfad auswählen"), startDir);
    if (!dir.isEmpty())
        m_editNewRoot->setText(dir);
}

// ── onOk ──────────────────────────────────────────────────────────────────────

void DocumentsSettingsForm::onOk()
{
    const QString oldRoot = m_editOldRoot->text().trimmed();
    const QString newRootRaw = m_editNewRoot->text().trimmed();

    if (newRootRaw.isEmpty()) {
        OwnMessageBox::critical(this, tr("Fehler"),
            tr("Bitte wählen Sie einen neuen Root-Pfad aus."));
        return;
    }

    const QString newRoot = QDir::cleanPath(QFileInfo(newRootRaw).absoluteFilePath());

    // Der neue Root-Pfad muss bereits existieren — anders als in einer
    // früheren Version dieses Dialogs wird er NICHT automatisch angelegt.
    // Normalerweise kann das ohnehin nicht passieren, da "Durchsuchen..."
    // (QFileDialog::getExistingDirectory) nur existierende Verzeichnisse
    // zur Auswahl anbietet — die Prüfung fängt aber z. B. den Fall ab, dass
    // der Ordner zwischen Auswahl und Klick auf OK gelöscht wurde.
    if (!QDir(newRoot).exists()) {
        OwnMessageBox::critical(this, tr("Fehler"),
            tr("Das gewählte Verzeichnis existiert nicht:\n%1\n\n"
               "Bitte wählen Sie ein vorhandenes Verzeichnis aus.").arg(newRoot));
        return;
    }

    if (oldRoot.isEmpty()) {
        // Kein alter Pfad angegeben — nichts zum Umschreiben, nur den neuen
        // Root-Pfad übernehmen (z. B. frisches Portfolio ohne Dokumente).
        AppSettings::instance().setDocumentsRootPath(newRoot);
        accept();
        return;
    }

    QString normalizedOldRoot = oldRoot;
    normalizedOldRoot.replace(QLatin1Char('\\'), QLatin1Char('/'));
    normalizedOldRoot = QDir::cleanPath(normalizedOldRoot);

    if (normalizedOldRoot == newRoot) {
        AppSettings::instance().setDocumentsRootPath(newRoot);
        accept();
        return;
    }

    const bool confirmed = OwnMessageBox::question(this, tr("Bestätigung"),
        tr("Alle Dokumentpfade von\n%1\nauf\n%2\numstellen?").arg(oldRoot, newRoot));
    if (!confirmed)
        return;

    const DocumentRootMigrator::Result result =
        DocumentRootMigrator::changeRoot(oldRoot, newRoot);

    AppSettings::instance().setDocumentsRootPath(newRoot);

    qInfo() << "[DocumentsSettingsForm] Root-Pfad umgestellt von" << oldRoot
             << "auf" << newRoot
             << "— angepasst:" << result.rewritten
             << "bereits korrekt:" << result.alreadyInRoot
             << "außerhalb (unverändert):" << result.outsideRoot
             << "fehlgeschlagen:" << result.updateFailed;

    QString summary = tr("Neuer Root-Pfad: %1\n\n"
                         "%2 Dokumentpfad(e) angepasst.\n"
                         "%3 waren bereits korrekt.")
                          .arg(newRoot)
                          .arg(result.rewritten)
                          .arg(result.alreadyInRoot);
    if (result.outsideRoot > 0) {
        summary += tr("\n%1 Dokument(e) begannen nicht mit dem alten "
                      "Root-Pfad und wurden NICHT angepasst.")
                       .arg(result.outsideRoot);
    }
    if (result.updateFailed > 0) {
        summary += tr("\n%1 Datenbank-Update(s) sind fehlgeschlagen — siehe Log.")
                       .arg(result.updateFailed);
    }
    OwnMessageBox::information(this, tr("Dokumente"), summary);

    accept();
}

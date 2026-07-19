// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>

/**
 * @brief Dialog for moving all document paths from an old root directory to
 *        a new one in one go (e.g. after switching from Windows to Linux,
 *        or reorganizing where belegs/documents are kept).
 *
 * Two plain fields:
 * - "Alter Root-Pfad": pre-filled with a best-guess detection (see
 *   DocumentRootMigrator::detectCommonRoot()) — either the currently
 *   configured AppSettings::documentsRootPath() if one exists, or an
 *   automatic, OS-independent guess derived from the documents already in
 *   the database. Freely editable; does NOT need to exist on this machine
 *   (e.g. a Windows path while running on Linux) — it's only used as a
 *   literal string prefix to match against stored document paths.
 * - "Neuer Root-Pfad": chosen via "Durchsuchen..." (must be a real, existing
 *   or creatable directory on this machine).
 *
 * OK rewrites every document path in the database that starts with the old
 * root to start with the new root instead (DocumentRootMigrator::changeRoot()),
 * then saves the new root to AppSettings. Cancel does nothing at all — no
 * database writes, no settings change. Unlike an earlier version of this
 * dialog, there is no mandatory/first-run mode: MainWindow offers it at
 * startup if no root is configured yet, but the user can simply cancel and
 * be asked again next time (see ARCHITECTURE.md, "Dokument-Root-Verzeichnis").
 */
class DocumentsSettingsForm : public QDialog
{
    Q_OBJECT

public:
    explicit DocumentsSettingsForm(QWidget* parent = nullptr);

private slots:
    void onOk();
    void onBrowseNewRoot();

private:
    void setupUi();
    void loadSettings();

    QLabel*      m_lblInfo       = nullptr;
    QLabel*      m_lblHint       = nullptr;

    QLineEdit*   m_editOldRoot   = nullptr;
    QLineEdit*   m_editNewRoot   = nullptr;
    QPushButton* m_btnBrowseNew  = nullptr;

    QPushButton* m_btnOk         = nullptr;
    QPushButton* m_btnCancel     = nullptr;
};

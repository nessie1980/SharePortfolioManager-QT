// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// Eigene Test-Executable statt Erweiterung von tst_mainwindow.cpp — analog zu
// tst_backupsettingsform (siehe ARCHITECTURE.md, "Neue Forms bekommen ihre
// eigene Test-Executable"). Anders als BackupSettingsForm braucht
// DocumentRootMigrator aber eine echte Datenbank (Repositories), daher hier
// dieselben openMemoryDb()/insertTestShare()-Helfer wie in tst_mainwindow.cpp.
#include <QtTest>
#include <QApplication>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QTemporaryDir>
#include <QUuid>
#include <QDir>

#include "DocumentsSettingsForm.h"
#include "../../core/DocumentRootMigrator.h"
#include "../../config/AppSettings.h"
#include "../../core/Database.h"
#include "../../repositories/ShareRepository.h"
#include "../../repositories/BuyRepository.h"
#include "../../models/ShareObject.h"
#include "../../models/BuyObject.h"

/**
 * @brief Tests für DocumentsSettingsForm, DocumentRootMigrator und die
 *        Documents-Sektion von AppSettings.
 *
 * AppSettings ist ein Singleton — jeder Test, der documentsRootPath ändert,
 * stellt am Ende den ursprünglichen Wert wieder her (gleiches Muster wie
 * tst_backupsettingsform).
 *
 * @note Nur BuyObject/BuyRepository werden für die Migrator-Tests verwendet,
 * nicht zusätzlich Sale/Brokerage/Dividend — DocumentRootMigrator behandelt
 * alle vier Tabellen strukturell identisch (derselbe Switch über
 * DocumentEntry::Table in rewrite()/collectAllDocuments()), ein Fehler in
 * der Buy-Behandlung würde sich genauso in den anderen drei zeigen.
 *
 * @note Die Fehler-/Bestätigungs-Zweige von DocumentsSettingsForm::onOk()
 * (leerer oder nicht existierender neuer Pfad, abweichender alter Pfad)
 * rufen OwnMessageBox::critical()/question() auf, die intern exec()
 * aufrufen und damit einen headless Testlauf blockieren würden — dieselbe
 * Konvention wie an den übrigen OwnMessageBox-Aufrufstellen im Projekt
 * (siehe TESTING.md, Abschnitt "Konventionen", sowie die Begründung bei
 * onPortfolioRowDoubleClicked in tst_mainwindow.cpp). Getestet werden daher
 * nur die beiden Zweige, die ohne jeden Dialog direkt akzeptieren.
 */
class TestDocumentsSettingsForm : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;

    // Eigene Sandbox-INI, unabhängig von der echten settings.ini — dieselbe
    // Absicherung wie in tst_mainwindow.cpp/tst_appstartup.cpp. Ohne diesen
    // Aufruf bliebe AppSettings::instance() bei einem leeren m_settingsPath
    // (Default eines frisch gestarteten Testprozesses), und jeder
    // setDocumentsRootPath()-Aufruf in den Tests unten würde mit leerem
    // Pfad "gespeichert" — unklares Verhalten statt einer sauberen Sandbox.
    void loadSandboxedSettings()
    {
        const QString sandboxIni = m_tempDir.path() + QStringLiteral("/test_settings.ini");
        AppSettings::instance().load(sandboxIni);
    }

    void openMemoryDb()
    {
        if (!Database::instance().isOpen())
            Database::instance().open(QStringLiteral(":memory:"));
    }

    QString insertTestShare()
    {
        openMemoryDb();
        ShareRepository repo;
        const QString guid = QStringLiteral("share-docroot-1");
        repo.insert(ShareObject(guid, QStringLiteral("DOC01"),
                                QStringLiteral("DE000DOC0001"), QStringLiteral("DocRoot AG")));
        return guid;
    }

    BuyObject insertTestBuy(const QString& shareGuid, const QString& document)
    {
        BuyRepository repo;
        const QString guid = QUuid::createUuid().toString(QUuid::WithoutBraces);
        BuyObject b(guid, shareGuid, QString(), QStringLiteral("ord-") + guid,
                   QStringLiteral("2024-01-15T10:00:00"), 10.0, 0.0, 100.0,
                   QString(), document);
        repo.insert(b);
        return b;
    }

    static QPushButton* findButtonByText(QWidget* parent, const QString& text)
    {
        const auto buttons = parent->findChildren<QPushButton*>();
        for (auto* b : buttons)
            if (b->text() == text) return b;
        return nullptr;
    }

private slots:

    void initTestCase()
    {
        QVERIFY(m_tempDir.isValid());
        loadSandboxedSettings();
    }

    void init()
    {
        // Jeder Test startet mit einer frischen, leeren In-Memory-DB und
        // erneut sandboxten AppSettings (dasselbe Muster wie in
        // tst_mainwindow.cpp — verhindert, dass irgendein Test hier
        // versehentlich die echte settings.ini berührt).
        loadSandboxedSettings();
        if (Database::instance().isOpen())
            Database::instance().close();
        openMemoryDb();
    }

    void cleanup()
    {
        if (Database::instance().isOpen())
            Database::instance().close();
    }

    // ─────────────────────────────────────────────────────────────────────
    // AppSettings — Documents-Sektion: reiner Speichern/Laden-Roundtrip
    // ─────────────────────────────────────────────────────────────────────

    void test_documentsSettings_saveRootPath()
    {
        const QString original = AppSettings::instance().documentsRootPath();
        AppSettings::instance().setDocumentsRootPath(QStringLiteral("/tmp/my-documents"));
        QCOMPARE(AppSettings::instance().documentsRootPath(), QStringLiteral("/tmp/my-documents"));
        AppSettings::instance().setDocumentsRootPath(original);
    }

    void test_documentsSettings_defaultIsEmpty()
    {
        const QString original = AppSettings::instance().documentsRootPath();
        AppSettings::instance().setDocumentsRootPath(QString());
        QVERIFY(AppSettings::instance().documentsRootPath().isEmpty());
        AppSettings::instance().setDocumentsRootPath(original);
    }

    // ─────────────────────────────────────────────────────────────────────
    // DocumentsSettingsForm — Konstruktion & Vorbefüllung
    // ─────────────────────────────────────────────────────────────────────

    void test_dialog_constructsWithoutCrash()
    {
        DocumentsSettingsForm dialog;
        QVERIFY(true); // reaching this line means construction didn't crash
    }

    void test_dialog_alwaysHasCancelButton()
    {
        DocumentsSettingsForm dialog;
        QVERIFY(findButtonByText(&dialog, tr("Abbrechen")) != nullptr);
    }

    void test_dialog_cancel_doesNotChangeSettings()
    {
        const QString original = AppSettings::instance().documentsRootPath();
        AppSettings::instance().setDocumentsRootPath(QStringLiteral("/keep/me"));

        DocumentsSettingsForm dialog;
        auto* cancelBtn = findButtonByText(&dialog, tr("Abbrechen"));
        if (!cancelBtn) QFAIL("Abbrechen button not found");
        QMetaObject::invokeMethod(cancelBtn, "clicked", Qt::DirectConnection);

        QCOMPARE(dialog.result(), static_cast<int>(QDialog::Rejected));
        QCOMPARE(AppSettings::instance().documentsRootPath(), QStringLiteral("/keep/me"));

        AppSettings::instance().setDocumentsRootPath(original);
    }

    void test_dialog_loadSettings_prefillsOldRootFromConfigured()
    {
        const QString original = AppSettings::instance().documentsRootPath();
        AppSettings::instance().setDocumentsRootPath(QStringLiteral("/configured/root"));

        DocumentsSettingsForm dialog;
        auto* editOld = dialog.findChild<QLineEdit*>(QStringLiteral("editOldRoot"));
        if (!editOld) QFAIL("editOldRoot not found");
        QCOMPARE(editOld->text(), QStringLiteral("/configured/root"));

        AppSettings::instance().setDocumentsRootPath(original);
    }

    void test_dialog_loadSettings_prefillsOldRootFromDetection()
    {
        const QString original = AppSettings::instance().documentsRootPath();
        AppSettings::instance().setDocumentsRootPath(QString());

        const QString shareGuid = insertTestShare();
        insertTestBuy(shareGuid, QStringLiteral("/data/Belege/a.pdf"));
        insertTestBuy(shareGuid, QStringLiteral("/data/Belege/b.pdf"));

        DocumentsSettingsForm dialog;
        auto* editOld = dialog.findChild<QLineEdit*>(QStringLiteral("editOldRoot"));
        if (!editOld) QFAIL("editOldRoot not found");
        QCOMPARE(editOld->text(), QStringLiteral("/data/Belege"));

        AppSettings::instance().setDocumentsRootPath(original);
    }

    void test_dialog_loadSettings_ambiguousShowsHintNoAutofill()
    {
        const QString original = AppSettings::instance().documentsRootPath();
        AppSettings::instance().setDocumentsRootPath(QString());

        const QString shareGuid = insertTestShare();
        insertTestBuy(shareGuid, QStringLiteral("/data/Belege/a.pdf"));
        insertTestBuy(shareGuid, QStringLiteral("/other/Ordner/b.pdf"));

        DocumentsSettingsForm dialog;
        auto* editOld = dialog.findChild<QLineEdit*>(QStringLiteral("editOldRoot"));
        if (!editOld) QFAIL("editOldRoot not found");
        QVERIFY(editOld->text().isEmpty());

        AppSettings::instance().setDocumentsRootPath(original);
    }

    // ─────────────────────────────────────────────────────────────────────
    // DocumentsSettingsForm — onOk(), nur die dialogfreien Zweige
    // ─────────────────────────────────────────────────────────────────────

    void test_dialog_onOk_emptyOldRoot_savesWithoutRewrite()
    {
        const QString original = AppSettings::instance().documentsRootPath();
        AppSettings::instance().setDocumentsRootPath(QString());

        DocumentsSettingsForm dialog;
        auto* editNew = dialog.findChild<QLineEdit*>(QStringLiteral("editNewRoot"));
        if (!editNew) QFAIL("editNewRoot not found");
        editNew->setText(m_tempDir.path());

        auto* okBtn = findButtonByText(&dialog, tr("OK"));
        if (!okBtn) QFAIL("OK button not found");
        QMetaObject::invokeMethod(okBtn, "clicked", Qt::DirectConnection);

        QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));
        QCOMPARE(AppSettings::instance().documentsRootPath(), QDir::cleanPath(m_tempDir.path()));

        AppSettings::instance().setDocumentsRootPath(original);
    }

    void test_dialog_onOk_sameOldAndNewRoot_savesWithoutConfirmation()
    {
        const QString original = AppSettings::instance().documentsRootPath();
        const QString root = QDir::cleanPath(m_tempDir.path());
        AppSettings::instance().setDocumentsRootPath(root);

        DocumentsSettingsForm dialog;
        auto* editNew = dialog.findChild<QLineEdit*>(QStringLiteral("editNewRoot"));
        if (!editNew) QFAIL("editNewRoot not found");
        editNew->setText(root);

        auto* okBtn = findButtonByText(&dialog, tr("OK"));
        if (!okBtn) QFAIL("OK button not found");
        QMetaObject::invokeMethod(okBtn, "clicked", Qt::DirectConnection);

        QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));
        QCOMPARE(AppSettings::instance().documentsRootPath(), root);

        AppSettings::instance().setDocumentsRootPath(original);
    }

    // ─────────────────────────────────────────────────────────────────────
    // DocumentRootMigrator — changeRoot()
    // ─────────────────────────────────────────────────────────────────────

    void test_migrator_changeRoot_rewritesMatchingPaths()
    {
        const QString shareGuid = insertTestShare();
        const BuyObject buy = insertTestBuy(shareGuid, QStringLiteral("/old/root/a.pdf"));

        const auto result = DocumentRootMigrator::changeRoot(
            QStringLiteral("/old/root"), QStringLiteral("/new/root"));

        QCOMPARE(result.rewritten, 1);
        QCOMPARE(result.outsideRoot, 0);

        BuyRepository repo;
        QCOMPARE(repo.findByGuid(buy.guid()).document(), QStringLiteral("/new/root/a.pdf"));
    }

    void test_migrator_changeRoot_leavesOutsidePathsUntouched()
    {
        const QString shareGuid = insertTestShare();
        const BuyObject buy = insertTestBuy(shareGuid, QStringLiteral("/somewhere/else/a.pdf"));

        const auto result = DocumentRootMigrator::changeRoot(
            QStringLiteral("/old/root"), QStringLiteral("/new/root"));

        QCOMPARE(result.rewritten, 0);
        QCOMPARE(result.outsideRoot, 1);
        QCOMPARE(result.outsidePaths.size(), 1);

        BuyRepository repo;
        QCOMPARE(repo.findByGuid(buy.guid()).document(), QStringLiteral("/somewhere/else/a.pdf"));
    }

    void test_migrator_changeRoot_alreadyCorrectPath_notCounted()
    {
        const QString shareGuid = insertTestShare();
        insertTestBuy(shareGuid, QStringLiteral("/old/root/a.pdf"));

        // oldRoot == newRoot -> der umgeschriebene Pfad entspricht dem
        // Original, also "bereits korrekt" statt "umgeschrieben".
        const auto result = DocumentRootMigrator::changeRoot(
            QStringLiteral("/old/root"), QStringLiteral("/old/root"));

        QCOMPARE(result.alreadyInRoot, 1);
        QCOMPARE(result.rewritten, 0);
    }

    void test_migrator_changeRoot_oldRootNeedNotExistOnDisk()
    {
        // Alter Root ist ein Windows-Pfad, der auf diesem (Linux/macOS)
        // Testrechner sicher nicht existiert — changeRoot() muss trotzdem
        // matchen, da es ein reiner String-Vergleich ist, keine
        // Dateisystem-Prüfung.
        const QString shareGuid = insertTestShare();
        const BuyObject buy = insertTestBuy(shareGuid, QStringLiteral("B:\\Depot\\Belege\\a.pdf"));

        const auto result = DocumentRootMigrator::changeRoot(
            QStringLiteral("B:\\Depot\\Belege"), m_tempDir.path());

        QCOMPARE(result.rewritten, 1);
        BuyRepository repo;
        QCOMPARE(repo.findByGuid(buy.guid()).document(),
                 QDir::cleanPath(m_tempDir.path() + QStringLiteral("/a.pdf")));
    }

    void test_migrator_changeRoot_windowsBackslashPaths_matchedCorrectly()
    {
        const QString shareGuid = insertTestShare();
        const BuyObject buy1 = insertTestBuy(shareGuid, QStringLiteral("B:\\Depot\\Belege\\a.pdf"));
        const BuyObject buy2 = insertTestBuy(shareGuid,
            QStringLiteral("B:\\Depot\\Belege\\Unterordner\\b.pdf"));

        const auto result = DocumentRootMigrator::changeRoot(
            QStringLiteral("B:\\Depot\\Belege"), QStringLiteral("/new/root"));

        QCOMPARE(result.rewritten, 2);
        BuyRepository repo;
        QCOMPARE(repo.findByGuid(buy1.guid()).document(), QStringLiteral("/new/root/a.pdf"));
        QCOMPARE(repo.findByGuid(buy2.guid()).document(),
                 QStringLiteral("/new/root/Unterordner/b.pdf"));
    }

    void test_migrator_changeRoot_noDocuments_returnsZeroResult()
    {
        insertTestShare();
        const auto result = DocumentRootMigrator::changeRoot(
            QStringLiteral("/old"), QStringLiteral("/new"));
        QCOMPARE(result.total(), 0);
        QCOMPARE(result.rewritten, 0);
        QCOMPARE(result.outsideRoot, 0);
    }

    // ─────────────────────────────────────────────────────────────────────
    // DocumentRootMigrator — detectCommonRoot()
    // ─────────────────────────────────────────────────────────────────────

    void test_migrator_detect_commonParentDetected()
    {
        const QString shareGuid = insertTestShare();
        insertTestBuy(shareGuid, QStringLiteral("/data/Belege/a.pdf"));
        insertTestBuy(shareGuid, QStringLiteral("/data/Belege/b.pdf"));

        const auto detection = DocumentRootMigrator::detectCommonRoot();
        QCOMPARE(detection.suggestedRoot, QStringLiteral("/data/Belege"));
        QVERIFY(!detection.ambiguous);
        QCOMPARE(detection.absoluteCount, 2);
    }

    void test_migrator_detect_windowsPathsOnLinuxHost_stillDetected()
    {
        // Regressionstest für den 18.07.2026 gemeldeten Bug: Windows-Pfade
        // (Laufwerksbuchstabe + Backslash) wurden unter Linux fälschlich
        // als "nicht absolut" eingestuft.
        const QString shareGuid = insertTestShare();
        insertTestBuy(shareGuid, QStringLiteral("B:\\IngDiba\\Depot\\a.pdf"));
        insertTestBuy(shareGuid, QStringLiteral("B:\\IngDiba\\Depot\\b.pdf"));

        const auto detection = DocumentRootMigrator::detectCommonRoot();
        QCOMPARE(detection.absoluteCount, 2);
        QCOMPARE(detection.relativeCount, 0);
        QCOMPARE(detection.suggestedRoot, QStringLiteral("B:/IngDiba/Depot"));
    }

    void test_migrator_detect_noCommonParent_setsAmbiguous()
    {
        const QString shareGuid = insertTestShare();
        insertTestBuy(shareGuid, QStringLiteral("/data/Belege/a.pdf"));
        insertTestBuy(shareGuid, QStringLiteral("/other/Ordner/b.pdf"));

        const auto detection = DocumentRootMigrator::detectCommonRoot();
        QVERIFY(detection.ambiguous);
        QVERIFY(detection.suggestedRoot.isEmpty());
    }

    void test_migrator_detect_relativePaths_excludedFromDetection()
    {
        const QString shareGuid = insertTestShare();
        insertTestBuy(shareGuid, QStringLiteral("/data/Belege/a.pdf"));
        insertTestBuy(shareGuid, QStringLiteral("just_a_filename.pdf"));

        const auto detection = DocumentRootMigrator::detectCommonRoot();
        QCOMPARE(detection.absoluteCount, 1);
        QCOMPARE(detection.relativeCount, 1);
    }

    void test_migrator_detect_emptyDatabase_returnsZeroResult()
    {
        insertTestShare();
        const auto detection = DocumentRootMigrator::detectCommonRoot();
        QCOMPARE(detection.total(), 0);
        QVERIFY(detection.suggestedRoot.isEmpty());
        QVERIFY(!detection.ambiguous);
    }

    // ─────────────────────────────────────────────────────────────────────
    // DocumentRootMigrator — isPathWithinRoot() (19.07.2026, Durchsetzung
    // "nur noch Dokumente aus dem Root auswählbar" in den fünf
    // Editier-Dialogen — reine Logik, kein Dialog beteiligt.)
    // ─────────────────────────────────────────────────────────────────────

    void test_isPathWithinRoot_emptyRoot_alwaysTrue()
    {
        QVERIFY(DocumentRootMigrator::isPathWithinRoot(
            QStringLiteral("/anywhere/a.pdf"), QString()));
    }

    void test_isPathWithinRoot_exactRootPath_true()
    {
        // Kein realistischer Fall (Root wäre selbst eine Datei), aber die
        // Grenzfall-Logik ("== Root" zählt als innerhalb) soll stimmen.
        QVERIFY(DocumentRootMigrator::isPathWithinRoot(
            QStringLiteral("/data/Belege"), QStringLiteral("/data/Belege")));
    }

    void test_isPathWithinRoot_directChild_true()
    {
        QVERIFY(DocumentRootMigrator::isPathWithinRoot(
            QStringLiteral("/data/Belege/a.pdf"), QStringLiteral("/data/Belege")));
    }

    void test_isPathWithinRoot_nestedSubdirectory_true()
    {
        QVERIFY(DocumentRootMigrator::isPathWithinRoot(
            QStringLiteral("/data/Belege/2024/Depot1/a.pdf"),
            QStringLiteral("/data/Belege")));
    }

    void test_isPathWithinRoot_outsideRoot_false()
    {
        QVERIFY(!DocumentRootMigrator::isPathWithinRoot(
            QStringLiteral("/other/Ordner/a.pdf"), QStringLiteral("/data/Belege")));
    }

    void test_isPathWithinRoot_similarPrefixNotSubdirectory_false()
    {
        // "/data/Belege2" ist KEIN Unterordner von "/data/Belege" — ein
        // reiner startsWith() ohne Trennzeichen-Prüfung würde das fälschlich
        // als "innerhalb" werten.
        QVERIFY(!DocumentRootMigrator::isPathWithinRoot(
            QStringLiteral("/data/Belege2/a.pdf"), QStringLiteral("/data/Belege")));
    }

    void test_isPathWithinRoot_windowsBackslashPath_crossPlatform_true()
    {
        QVERIFY(DocumentRootMigrator::isPathWithinRoot(
            QStringLiteral("B:\\Depot\\Belege\\a.pdf"), QStringLiteral("B:/Depot/Belege")));
    }
};

QTEST_MAIN(TestDocumentsSettingsForm)
#include "tst_documentssettingsform.moc"

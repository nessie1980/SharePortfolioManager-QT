// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
//
// tst_ownmessagebox.cpp — Unit tests for OwnMessageBox (Critical/Information/
// Question-Dialoge).
//
// Aus tst_mainwindow.cpp herausgelöst (22.08.2026) — analog tst_buysform,
// tst_dividendform und tst_salesform. TestOwnMessageBox brauchte weder
// Datenbank noch MainWindow und keinen der Stubs aus tst_mainwindow.cpp;
// reine Auslagerung, die Testmethoden sind unverändert übernommen.
//
// Kein eigenes main() mit QLocale::setDefault(QLocale::German) nötig — die
// Tests vergleichen keine formatierten Beträge, nur feste deutsche Strings
// ("Ja", "Nein", "Ok") und tr()-Ausgaben. QTEST_MAIN reicht (gleiches Muster
// wie tst_backupsettingsform.cpp/tst_traysettingsform.cpp).

#include <QtTest>
#include <QApplication>
#include <QDialog>
#include <QPushButton>
#include <QLabel>

#include "../../app/forms/OwnMessageBoxForm/OwnMessageBox.h"
#include "../../app/forms/UiConstants.h"

// ─────────────────────────────────────────────────────────────────────────────
// TestOwnMessageBox
// ─────────────────────────────────────────────────────────────────────────────
class TestOwnMessageBox : public QObject
{
    Q_OBJECT

private slots:

    // ── Critical ─────────────────────────────────────────────────────────────

    void test_critical_canBeConstructed()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Critical,
                          QStringLiteral("Fehler"),
                          QStringLiteral("Ein Fehler ist aufgetreten."));
        QVERIFY(dlg.windowTitle() == QStringLiteral("Fehler"));
    }

    void test_critical_hasSingleOkButton()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Critical,
                          QStringLiteral("Fehler"),
                          QStringLiteral("Fehlermeldung"));
        auto* btn = dlg.findChild<QPushButton*>("", Qt::FindChildrenRecursively);
        if (!btn) QFAIL("No button found in Critical dialog");
        // There must be exactly one button
        const auto buttons = dlg.findChildren<QPushButton*>();
        QCOMPARE(buttons.size(), 1);
        QCOMPARE(buttons.first()->text(), tr("Ok"));
    }

    void test_critical_hasNoYesNoButtons()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Critical,
                          QStringLiteral("Fehler"),
                          QStringLiteral("Fehlermeldung"));
        const auto buttons = dlg.findChildren<QPushButton*>();
        for (auto* b : buttons) {
            QVERIFY(b->text() != tr("Ja"));
            QVERIFY(b->text() != tr("Nein"));
        }
    }

    void test_critical_okButtonAcceptsDialog()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Critical,
                          QStringLiteral("Fehler"),
                          QStringLiteral("Fehlermeldung"));
        const auto buttons = dlg.findChildren<QPushButton*>();
        if (buttons.isEmpty()) QFAIL("No button found");
        // Simulate click — dialog must not crash and result must be Accepted
        QMetaObject::invokeMethod(buttons.first(), "clicked", Qt::DirectConnection);
        QCOMPARE(dlg.result(), static_cast<int>(QDialog::Accepted));
    }

    void test_critical_hasIconLabel()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Critical,
                          QStringLiteral("Fehler"),
                          QStringLiteral("Fehlermeldung"));
        const auto labels = dlg.findChildren<QLabel*>();
        // At least one label must have a pixmap (the icon label)
        bool hasIcon = false;
        for (auto* l : labels) {
            if (!l->pixmap().isNull()) {
                hasIcon = true;
                break;
            }
        }
        QVERIFY(hasIcon);
    }

    void test_critical_okButtonHasNoIcon()
    {
        // 14.08.2026, Nessies Vorgabe: das bisherige ButtonSave-Icon
        // (Diskette) suggerierte fälschlich ein Speichern, obwohl Ok hier
        // nur den Dialog schließt.
        OwnMessageBox dlg(OwnMessageBox::Type::Critical,
                          QStringLiteral("Fehler"),
                          QStringLiteral("Fehlermeldung"));
        const auto buttons = dlg.findChildren<QPushButton*>();
        if (buttons.isEmpty()) QFAIL("No button found");
        QVERIFY(buttons.first()->icon().isNull());
    }

    void test_critical_messageTextVisible()
    {
        const QString msg = QStringLiteral("Datenbankfehler aufgetreten.");
        OwnMessageBox dlg(OwnMessageBox::Type::Critical,
                          QStringLiteral("Fehler"), msg);
        const auto labels = dlg.findChildren<QLabel*>();
        bool found = false;
        for (auto* l : labels) {
            if (l->text() == msg) { found = true; break; }
        }
        QVERIFY(found);
    }

    // ── Information ───────────────────────────────────────────────────────────

    void test_information_canBeConstructed()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Information,
                          QStringLiteral("Info"),
                          QStringLiteral("Vorgang abgeschlossen."));
        QVERIFY(dlg.windowTitle() == QStringLiteral("Info"));
    }

    void test_information_hasSingleOkButton()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Information,
                          QStringLiteral("Info"),
                          QStringLiteral("Hinweistext"));
        const auto buttons = dlg.findChildren<QPushButton*>();
        QCOMPARE(buttons.size(), 1);
        QCOMPARE(buttons.first()->text(), tr("Ok"));
    }

    void test_information_hasIconLabel()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Information,
                          QStringLiteral("Info"),
                          QStringLiteral("Hinweistext"));
        const auto labels = dlg.findChildren<QLabel*>();
        bool hasIcon = false;
        for (auto* l : labels) {
            if (!l->pixmap().isNull()) { hasIcon = true; break; }
        }
        QVERIFY(hasIcon);
    }

    void test_information_okButtonHasNoIcon()
    {
        // 14.08.2026, Nessies Vorgabe — siehe test_critical_okButtonHasNoIcon().
        OwnMessageBox dlg(OwnMessageBox::Type::Information,
                          QStringLiteral("Info"),
                          QStringLiteral("Hinweistext"));
        const auto buttons = dlg.findChildren<QPushButton*>();
        if (buttons.isEmpty()) QFAIL("No button found");
        QVERIFY(buttons.first()->icon().isNull());
    }

    // ── Question ──────────────────────────────────────────────────────────────

    void test_question_canBeConstructed()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Question,
                          QStringLiteral("Bestätigung"),
                          QStringLiteral("Wirklich löschen?"));
        QVERIFY(dlg.windowTitle() == QStringLiteral("Bestätigung"));
    }

    void test_question_hasTwoButtons()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Question,
                          QStringLiteral("Bestätigung"),
                          QStringLiteral("Wirklich löschen?"));
        const auto buttons = dlg.findChildren<QPushButton*>();
        QCOMPARE(buttons.size(), 2);
    }

    void test_question_hasYesAndNoButtons()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Question,
                          QStringLiteral("Bestätigung"),
                          QStringLiteral("Wirklich löschen?"));
        const auto buttons = dlg.findChildren<QPushButton*>();
        QStringList labels;
        for (auto* b : buttons) labels << b->text();
        QVERIFY(labels.contains(tr("Ja")));
        QVERIFY(labels.contains(tr("Nein")));
    }

    void test_question_hasNoOkButton()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Question,
                          QStringLiteral("Bestätigung"),
                          QStringLiteral("Wirklich löschen?"));
        const auto buttons = dlg.findChildren<QPushButton*>();
        for (auto* b : buttons)
            QVERIFY(b->text() != tr("Ok"));
    }

    void test_question_yesButtonAcceptsDialog()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Question,
                          QStringLiteral("Bestätigung"),
                          QStringLiteral("Wirklich löschen?"));
        const auto buttons = dlg.findChildren<QPushButton*>();
        QPushButton* yesBtn = nullptr;
        for (auto* b : buttons) {
            if (b->text() == tr("Ja")) { yesBtn = b; break; }
        }
        if (!yesBtn) QFAIL("Yes button not found");
        QMetaObject::invokeMethod(yesBtn, "clicked", Qt::DirectConnection);
        QCOMPARE(dlg.result(), static_cast<int>(QDialog::Accepted));
    }

    void test_question_noButtonRejectsDialog()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Question,
                          QStringLiteral("Bestätigung"),
                          QStringLiteral("Wirklich löschen?"));
        const auto buttons = dlg.findChildren<QPushButton*>();
        QPushButton* noBtn = nullptr;
        for (auto* b : buttons) {
            if (b->text() == tr("Nein")) { noBtn = b; break; }
        }
        if (!noBtn) QFAIL("No button not found");
        QMetaObject::invokeMethod(noBtn, "clicked", Qt::DirectConnection);
        QCOMPARE(dlg.result(), static_cast<int>(QDialog::Rejected));
    }

    void test_question_hasIconLabel()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Question,
                          QStringLiteral("Bestätigung"),
                          QStringLiteral("Wirklich löschen?"));
        const auto labels = dlg.findChildren<QLabel*>();
        bool hasIcon = false;
        for (auto* l : labels) {
            if (!l->pixmap().isNull()) { hasIcon = true; break; }
        }
        QVERIFY(hasIcon);
    }

    // ── Static convenience methods ────────────────────────────────────────────

    void test_staticCritical_doesNotCrash()
    {
        // Can't exec() in a unit test — construct directly and verify it compiles
        OwnMessageBox dlg(OwnMessageBox::Type::Critical,
                          QStringLiteral("Fehler"),
                          QStringLiteral("Statischer Aufruf."));
        QVERIFY(dlg.windowTitle() == QStringLiteral("Fehler"));
    }

    void test_staticInformation_doesNotCrash()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Information,
                          QStringLiteral("Info"),
                          QStringLiteral("Statischer Aufruf."));
        QVERIFY(dlg.windowTitle() == QStringLiteral("Info"));
    }

    void test_staticQuestion_doesNotCrash()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Question,
                          QStringLiteral("Bestätigung"),
                          QStringLiteral("Statischer Aufruf."));
        QVERIFY(dlg.windowTitle() == QStringLiteral("Bestätigung"));
    }

    // ── Layout & sizing ───────────────────────────────────────────────────────

    void test_minimumWidth_isAtLeast360()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Critical,
                          QStringLiteral("Fehler"),
                          QStringLiteral("Test"));
        QVERIFY(dlg.minimumWidth() >= 360);
    }

    void test_buttonHeight_matchesUiConstants()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Critical,
                          QStringLiteral("Fehler"),
                          QStringLiteral("Test"));
        const auto buttons = dlg.findChildren<QPushButton*>();
        if (buttons.isEmpty()) QFAIL("No buttons found");
        QCOMPARE(buttons.first()->height(), UiConstants::kButtonHeight);
    }

    void test_isModal()
    {
        OwnMessageBox dlg(OwnMessageBox::Type::Information,
                          QStringLiteral("Info"),
                          QStringLiteral("Test"));
        QVERIFY(dlg.isModal());
    }

    void test_longMessageText_doesNotCrash()
    {
        const QString longMsg = QString(500, QChar('A'));
        OwnMessageBox dlg(OwnMessageBox::Type::Critical,
                          QStringLiteral("Fehler"), longMsg);
        const auto labels = dlg.findChildren<QLabel*>();
        bool found = false;
        for (auto* l : labels) {
            if (l->text() == longMsg) { found = true; break; }
        }
        QVERIFY(found);
    }

    void test_multilineMessage_doesNotCrash()
    {
        const QString msg = QStringLiteral("Zeile 1\nZeile 2\nZeile 3");
        OwnMessageBox dlg(OwnMessageBox::Type::Information,
                          QStringLiteral("Info"), msg);
        QVERIFY(dlg.minimumWidth() >= 360);
    }
};

QTEST_MAIN(TestOwnMessageBox)
#include "tst_ownmessagebox.moc"

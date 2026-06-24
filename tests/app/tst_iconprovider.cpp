// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include <QtTest>
#include "../../app/IconProvider.h"

class TestIconProvider : public QObject
{
    Q_OBJECT

private slots:

    void init()
    {
        // Reset to default set before each test
        IconProvider::setIconSet(QStringLiteral("default"));
    }

    // ── setIconSet / iconSet ──────────────────────────────────────────────

    void test_defaultSet_isDefault()
    {
        QCOMPARE(IconProvider::iconSet(), QStringLiteral("default"));
    }

    void test_setIconSet_validSet_setsName()
    {
        IconProvider::setIconSet(QStringLiteral("default"));
        QCOMPARE(IconProvider::iconSet(), QStringLiteral("default"));
    }

    void test_setIconSet_invalidSet_fallsBackToDefault()
    {
        // "nonexistent" has no icons in resources — should fall back to default
        IconProvider::setIconSet(QStringLiteral("nonexistent"));
        QCOMPARE(IconProvider::iconSet(), QStringLiteral("default"));
    }

    // ── iconPath ──────────────────────────────────────────────────────────

    void test_iconPath_defaultSet()
    {
        const QString path = IconProvider::iconPath(IconProvider::ButtonAdd);
        QCOMPARE(path, QStringLiteral(":/icons/default/button_add_24.png"));
    }

    void test_iconPath_explicitSet()
    {
        const QString path = IconProvider::iconPath(
            IconProvider::ButtonEdit, QStringLiteral("default"));
        QCOMPARE(path, QStringLiteral(":/icons/default/button_pencil_24.png"));
    }

    void test_iconPath_allIconNames_haveNonEmptyPath()
    {
        // Every IconName must map to a non-empty file name
        const QList<IconProvider::IconName> allIcons = {
            IconProvider::ButtonAdd,       IconProvider::ButtonBack,
            IconProvider::ButtonCancel,    IconProvider::ButtonClipboard,
            IconProvider::ButtonExit,      IconProvider::ButtonEdit,
            IconProvider::ButtonDelete,    IconProvider::ButtonReset,
            IconProvider::ButtonSave,      IconProvider::ButtonSaveAs,
            IconProvider::ButtonUpdate,    IconProvider::ButtonUpdateAll,
            IconProvider::DocExcel,        IconProvider::DocExcelImage,
            IconProvider::DocPdf,          IconProvider::DocPdfImage,
            IconProvider::DocWord,         IconProvider::DocWordImage,
            IconProvider::MenuAbout,       IconProvider::MenuEventLog,
            IconProvider::MenuFileAdd,     IconProvider::MenuFileAdd2,
            IconProvider::MenuFileAdd3,    IconProvider::MenuFlagGerman,
            IconProvider::MenuFlagUsa,     IconProvider::MenuFolderAdd,
            IconProvider::MenuFolderOpen,  IconProvider::MenuKey,
            IconProvider::MenuSettings,    IconProvider::MenuSound,
            IconProvider::NegativNormal,   IconProvider::NegativStrong,
            IconProvider::Neutral,         IconProvider::PositivNormal,
            IconProvider::PositivStrong,   IconProvider::SearchFailed,
            IconProvider::SearchFailed2,   IconProvider::SearchInfo,
            IconProvider::SearchOk,        IconProvider::ShowWindow
        };

        for (const auto iconName : allIcons) {
            const QString path = IconProvider::iconPath(iconName);
            QVERIFY2(!path.isEmpty(),
                     qPrintable(QStringLiteral("Empty path for icon index %1")
                                    .arg(static_cast<int>(iconName))));
        }
    }

    // ── icon() ────────────────────────────────────────────────────────────

    void test_icon_defaultSet_returnsValidIcon()
    {
        const QIcon icon = IconProvider::icon(IconProvider::ButtonAdd);
        QVERIFY(!icon.isNull());
    }

    void test_icon_allIcons_loadSuccessfully()
    {
        // All icons in the default set must load without returning a null icon
        const QList<IconProvider::IconName> allIcons = {
            IconProvider::ButtonAdd,       IconProvider::ButtonBack,
            IconProvider::ButtonCancel,    IconProvider::ButtonClipboard,
            IconProvider::ButtonExit,      IconProvider::ButtonEdit,
            IconProvider::ButtonDelete,    IconProvider::ButtonReset,
            IconProvider::ButtonSave,      IconProvider::ButtonSaveAs,
            IconProvider::ButtonUpdate,    IconProvider::ButtonUpdateAll,
            IconProvider::DocExcel,        IconProvider::DocExcelImage,
            IconProvider::DocPdf,          IconProvider::DocPdfImage,
            IconProvider::DocWord,         IconProvider::DocWordImage,
            IconProvider::MenuAbout,       IconProvider::MenuEventLog,
            IconProvider::MenuFileAdd,     IconProvider::MenuFileAdd2,
            IconProvider::MenuFileAdd3,    IconProvider::MenuFlagGerman,
            IconProvider::MenuFlagUsa,     IconProvider::MenuFolderAdd,
            IconProvider::MenuFolderOpen,  IconProvider::MenuKey,
            IconProvider::MenuSettings,    IconProvider::MenuSound,
            IconProvider::NegativNormal,   IconProvider::NegativStrong,
            IconProvider::Neutral,         IconProvider::PositivNormal,
            IconProvider::PositivStrong,   IconProvider::SearchFailed,
            IconProvider::SearchFailed2,   IconProvider::SearchInfo,
            IconProvider::SearchOk,        IconProvider::ShowWindow
        };

        for (const auto iconName : allIcons) {
            const QIcon icon = IconProvider::icon(iconName);
            QVERIFY2(!icon.isNull(),
                     qPrintable(QStringLiteral("Null icon for index %1 (path: %2)")
                                    .arg(static_cast<int>(iconName))
                                    .arg(IconProvider::iconPath(iconName))));
        }
    }

    void test_icon_invalidSet_fallsBackToDefault()
    {
        // Setting an invalid set falls back to default — icons must still load
        IconProvider::setIconSet(QStringLiteral("nonexistent"));
        const QIcon icon = IconProvider::icon(IconProvider::ButtonAdd);
        QVERIFY(!icon.isNull());
    }
};

QTEST_MAIN(TestIconProvider)
#include "tst_iconprovider.moc"

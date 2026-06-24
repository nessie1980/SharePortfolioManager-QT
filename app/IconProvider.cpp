// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "IconProvider.h"

#include <QPixmap>
#include <QDebug>

// ── Static member ─────────────────────────────────────────────────────────────

QString IconProvider::s_iconSet = QStringLiteral("default");

// ── setIconSet / iconSet ──────────────────────────────────────────────────────

void IconProvider::setIconSet(const QString& setName)
{
    // Verify the set exists by trying to load a known icon as a pixmap.
    // QFile::exists() is unreliable for Qt resources on some platforms.
    const QString testPath = QStringLiteral(":/icons/%1/button_add_24.png")
                                 .arg(setName);
    const QPixmap testPixmap(testPath);
    if (testPixmap.isNull()) {
        qWarning() << "[IconProvider] Icon set not found:" << setName
                   << "— falling back to 'default'";
        s_iconSet = QStringLiteral("default");
        return;
    }
    s_iconSet = setName;
    qInfo() << "[IconProvider] Active icon set:" << s_iconSet;
}

QString IconProvider::iconSet()
{
    return s_iconSet;
}

// ── fileName ──────────────────────────────────────────────────────────────────

QString IconProvider::fileName(IconName name)
{
    switch (name) {
    case ButtonAdd:       return QStringLiteral("button_add_24.png");
    case ButtonBack:      return QStringLiteral("button_back_24.png");
    case ButtonCancel:    return QStringLiteral("button_cancel_24.png");
    case ButtonClipboard:   return QStringLiteral("button_clipboard_24.png");
    case ButtonExit:        return QStringLiteral("button_exit_24.png");
    case ButtonEdit:        return QStringLiteral("button_pencil_24.png");
    case ButtonEdit16:      return QStringLiteral("button_pencil_16.png");
    case ButtonDelete:      return QStringLiteral("button_recycle_bin_24.png");
    case ButtonReset:       return QStringLiteral("button_reset_24.png");
    case ButtonSave:        return QStringLiteral("button_save_24.png");
    case ButtonSaveAs:      return QStringLiteral("button_save_as_24.png");
    case ButtonUpdate:      return QStringLiteral("button_update_24.png");
    case ButtonUpdateAll:   return QStringLiteral("button_update_all_24.png");
    case DocExcel:          return QStringLiteral("doc_excel_24.png");
    case DocExcel16:        return QStringLiteral("doc_excel_16.png");
    case DocExcelImage:     return QStringLiteral("doc_excel_image_24.png");
    case DocExcelImage16:   return QStringLiteral("doc_excel_image_16.png");
    case DocPdf:            return QStringLiteral("doc_pdf_24.png");
    case DocPdf16:          return QStringLiteral("doc_pdf_16.png");
    case DocPdfImage:       return QStringLiteral("doc_pdf_image_24.png");
    case DocPdfImage16:     return QStringLiteral("doc_pdf_image_16.png");
    case DocWord:           return QStringLiteral("doc_word_24.png");
    case DocWord16:         return QStringLiteral("doc_word_16.png");
    case DocWordImage:      return QStringLiteral("doc_word_image_24.png");
    case DocWordImage16:    return QStringLiteral("doc_word_image_16.png");
    case MenuAbout:         return QStringLiteral("menu_about_24.png");
    case MenuEventLog:      return QStringLiteral("menu_eventlog_24.png");
    case MenuFileAdd:       return QStringLiteral("menu_file_add_24.png");
    case MenuFileAdd2:      return QStringLiteral("menu_file_add2_24.png");
    case MenuFileAdd3:      return QStringLiteral("menu_file_add3_24.png");
    case MenuFlagGerman:    return QStringLiteral("menu_flag_german_24.png");
    case MenuFlagUsa:       return QStringLiteral("menu_flag_usa_24.png");
    case MenuFolderAdd:     return QStringLiteral("menu_folder_add_24.png");
    case MenuFolderOpen:    return QStringLiteral("menu_folder_open_24.png");
    case MenuFolderOpen16:  return QStringLiteral("menu_folder_open_16.png");
    case MenuKey:           return QStringLiteral("menu_key_24.png");
    case MenuSettings:      return QStringLiteral("menu_settings_24.png");
    case MenuSound:         return QStringLiteral("menu_sound_24.png");
    case NegativNormal:     return QStringLiteral("negativ_normal_development_24.png");
    case NegativStrong:     return QStringLiteral("negativ_strong_development_24.png");
    case Negativ16:         return QStringLiteral("negativ_development_16.png");
    case Neutral:           return QStringLiteral("neutral_development_24.png");
    case Neutral16:         return QStringLiteral("neutral_development_16.png");
    case PositivNormal:     return QStringLiteral("positiv_normal_development_24.png");
    case PositivStrong:     return QStringLiteral("positiv_strong_development_24.png");
    case Positiv16:         return QStringLiteral("positiv_development_16.png");
    case SearchFailed:      return QStringLiteral("search_failed_24.png");
    case SearchFailed2:     return QStringLiteral("search_failed_2_24.png");
    case SearchInfo:        return QStringLiteral("search_info_24.png");
    case SearchOk:          return QStringLiteral("search_ok_24.png");
    case ShowWindow:        return QStringLiteral("show_window_24.png");
    case StateUpdateBoth:   return QStringLiteral("state_update_16.png");
    case StateUpdateMarket: return QStringLiteral("state_update_blue_16.png");
    case StateUpdateDaily:  return QStringLiteral("state_update_yellow_16.png");
    case StateUpdateOrange: return QStringLiteral("state_update_orange_16.png");
    case StateNoUpdate:     return QStringLiteral("state_no_update_16.png");
    }
    return QString();
}

// ── iconPath ──────────────────────────────────────────────────────────────────

QString IconProvider::iconPath(IconName name, const QString& setName)
{
    const QString resolvedSet = setName.isEmpty() ? s_iconSet : setName;
    return QStringLiteral(":/icons/%1/%2").arg(resolvedSet, fileName(name));
}

// ── icon ──────────────────────────────────────────────────────────────────────

QIcon IconProvider::icon(IconName name)
{
    // Try active set first
    const QString path = iconPath(name);
    const QPixmap pixmap(path);
    if (!pixmap.isNull())
        return QIcon(pixmap);

    // Fall back to default set
    if (s_iconSet != QStringLiteral("default")) {
        const QString fallbackPath = iconPath(name, QStringLiteral("default"));
        const QPixmap fallbackPixmap(fallbackPath);
        if (!fallbackPixmap.isNull()) {
            qWarning() << "[IconProvider] Icon not found in set" << s_iconSet
                       << "— using default:" << fileName(name);
            return QIcon(fallbackPixmap);
        }
    }

    qWarning() << "[IconProvider] Icon not found:" << fileName(name);
    return QIcon();
}

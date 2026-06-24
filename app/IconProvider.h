// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QIcon>
#include <QString>

/**
 * @brief Provides application icons from the configured icon set.
 *
 * Icons are loaded from Qt resource files. The prefix structure is:
 * `:/icons/<setName>/<iconName>`, e.g. `:/icons/default/button_add_24.png`.
 *
 * ### Adding a new icon set
 * 1. Add a new `<qresource prefix="/icons/<setName>">` block to `resources.qrc`
 *    with the same file names as the default set.
 * 2. Call `IconProvider::setIconSet("<setName>")` at startup or when switching.
 *
 * ### Usage
 * @code
 * // At startup (default set is used automatically):
 * IconProvider::setIconSet("default");
 *
 * // Anywhere in the UI:
 * m_btnAdd->setIcon(IconProvider::icon(IconProvider::ButtonAdd));
 * @endcode
 */
class IconProvider
{
public:
    IconProvider() = delete;

    /**
     * @brief Enumeration of all named icons used in the application.
     */
    enum IconName {
        // ── Buttons ───────────────────────────────────────────────────────
        ButtonAdd,          ///< button_add_24.png
        ButtonBack,         ///< button_back_24.png
        ButtonCancel,       ///< button_cancel_24.png
        ButtonClipboard,    ///< button_clipboard_24.png
        ButtonExit,         ///< button_exit_24.png
        ButtonEdit,         ///< button_pencil_24.png
        ButtonEdit16,       ///< button_pencil_16.png
        ButtonDelete,       ///< button_recycle_bin_24.png
        ButtonReset,        ///< button_reset_24.png
        ButtonSave,         ///< button_save_24.png
        ButtonSaveAs,       ///< button_save_as_24.png
        ButtonUpdate,       ///< button_update_24.png
        ButtonUpdateAll,    ///< button_update_all_24.png

        // ── Documents ─────────────────────────────────────────────────────
        DocExcel,           ///< doc_excel_24.png
        DocExcel16,         ///< doc_excel_16.png
        DocExcelImage,      ///< doc_excel_image_24.png
        DocExcelImage16,    ///< doc_excel_image_16.png
        DocPdf,             ///< doc_pdf_24.png
        DocPdf16,           ///< doc_pdf_16.png
        DocPdfImage,        ///< doc_pdf_image_24.png
        DocPdfImage16,      ///< doc_pdf_image_16.png
        DocWord,            ///< doc_word_24.png
        DocWord16,          ///< doc_word_16.png
        DocWordImage,       ///< doc_word_image_24.png
        DocWordImage16,     ///< doc_word_image_16.png

        // ── Menu / toolbar ────────────────────────────────────────────────
        MenuAbout,          ///< menu_about_24.png
        MenuEventLog,       ///< menu_eventlog_24.png
        MenuFileAdd,        ///< menu_file_add_24.png
        MenuFileAdd2,       ///< menu_file_add2_24.png
        MenuFileAdd3,       ///< menu_file_add3_24.png
        MenuFlagGerman,     ///< menu_flag_german_24.png
        MenuFlagUsa,        ///< menu_flag_usa_24.png
        MenuFolderAdd,      ///< menu_folder_add_24.png
        MenuFolderOpen,     ///< menu_folder_open_24.png
        MenuFolderOpen16,   ///< menu_folder_open_16.png
        MenuKey,            ///< menu_key_24.png
        MenuSettings,       ///< menu_settings_24.png
        MenuSound,          ///< menu_sound_24.png

        // ── Status / development indicators ──────────────────────────────
        NegativNormal,      ///< negativ_normal_development_24.png
        NegativStrong,      ///< negativ_strong_development_24.png
        Negativ16,          ///< negativ_development_16.png
        Neutral,            ///< neutral_development_24.png
        Neutral16,          ///< neutral_development_16.png
        PositivNormal,      ///< positiv_normal_development_24.png
        PositivStrong,      ///< positiv_strong_development_24.png
        Positiv16,          ///< positiv_development_16.png

        // ── Search state ──────────────────────────────────────────────────
        SearchFailed,       ///< search_failed_24.png
        SearchFailed2,      ///< search_failed_2_24.png
        SearchInfo,         ///< search_info_24.png
        SearchOk,           ///< search_ok_24.png

        // ── Misc ──────────────────────────────────────────────────────────
        ShowWindow,         ///< show_window_24.png

        // ── Update state (16px) ───────────────────────────────────────────
        StateUpdateBoth,    ///< state_update_16.png        (green)
        StateUpdateMarket,  ///< state_update_blue_16.png   (blue)
        StateUpdateDaily,   ///< state_update_yellow_16.png (yellow)
        StateUpdateOrange,  ///< state_update_orange_16.png (orange)
        StateNoUpdate       ///< state_no_update_16.png     (red)
    };

    /**
     * @brief Set the active icon set by name.
     *
     * The name must match a `<qresource prefix="/icons/<name>">` entry
     * in `resources.qrc`. Falls back to "default" if not found.
     * @param setName  Icon set name (e.g. "default").
     */
    static void setIconSet(const QString& setName);

    /**
     * @brief Returns the name of the currently active icon set.
     * @return Icon set name string.
     */
    static QString iconSet();

    /**
     * @brief Returns a QIcon for the given named icon from the active set.
     *
     * If the icon is not found in the active set, falls back to the
     * "default" set. If not found there either, returns an empty QIcon.
     * @param name  The icon to retrieve.
     * @return QIcon loaded from the Qt resource system.
     */
    static QIcon icon(IconName name);

    /**
     * @brief Returns the resource path for a given icon name and set.
     * @param name     The icon to retrieve.
     * @param setName  The icon set to use (defaults to current set).
     * @return Full resource path string (e.g. ":/icons/default/button_add_24.png").
     */
    static QString iconPath(IconName name, const QString& setName = {});

private:
    /**
     * @brief Maps an IconName to its filename (without path prefix).
     * @param name  The icon to look up.
     * @return Filename string (e.g. "button_add_24.png").
     */
    static QString fileName(IconName name);

    static QString s_iconSet; ///< Currently active icon set name
};

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QString>

/**
 * @brief A platform-independent message dialog replacing QMessageBox.
 *
 * Renders consistently on Windows and Linux using the application's own
 * style — no system-theme dependency. Icons come from IconProvider.
 *
 * Three dialog types:
 *  - Critical     : error icon  + OK button
 *  - Information  : info icon   + OK button
 *  - Question     : info icon   + Yes/No buttons
 *
 * Static convenience methods mirror the QMessageBox API:
 * @code
 *   OwnMessageBox::critical(parent, tr("Fehler"), tr("Etwas lief schief."));
 *
 *   bool yes = OwnMessageBox::question(
 *       parent,
 *       tr("Bestätigung"),
 *       tr("Wirklich löschen?"));
 * @endcode
 */
class OwnMessageBox : public QDialog
{
    Q_OBJECT

public:
    /** Dialog type — controls icon and available buttons. */
    enum class Type {
        Critical,     ///< Error icon, single OK button
        Information,  ///< Info  icon, single OK button
        Question      ///< Info  icon, Yes / No buttons
    };

    /**
     * @brief Construct the dialog explicitly.
     *
     * Prefer the static convenience methods for typical use.
     *
     * @param type    Dialog type (Critical / Information / Question).
     * @param title   Window title.
     * @param message Message text (may contain newlines).
     * @param parent  Parent widget.
     */
    explicit OwnMessageBox(Type           type,
                           const QString& title,
                           const QString& message,
                           QWidget*       parent = nullptr);

    // ── Static convenience methods ────────────────────────────────────────

    /**
     * @brief Show a critical error dialog with a single OK button.
     * @param parent  Parent widget.
     * @param title   Window title.
     * @param message Error message.
     */
    static void critical(QWidget*       parent,
                         const QString& title,
                         const QString& message);

    /**
     * @brief Show an information dialog with a single OK button.
     * @param parent  Parent widget.
     * @param title   Window title.
     * @param message Informational message.
     */
    static void information(QWidget*       parent,
                            const QString& title,
                            const QString& message);

    /**
     * @brief Show a question dialog with Yes / No buttons.
     * @param parent  Parent widget.
     * @param title   Window title.
     * @param message Question text.
     * @return @c true if the user clicked Yes, @c false for No / close.
     */
    static bool question(QWidget*       parent,
                         const QString& title,
                         const QString& message);

private:
    void setupUi(const QString& message);

    Type          m_type;
    QLabel*       m_iconLabel = nullptr;
    QLabel*       m_msgLabel  = nullptr;
    QPushButton*  m_btnOk     = nullptr;   ///< Critical / Information
    QPushButton*  m_btnYes    = nullptr;   ///< Question
    QPushButton*  m_btnNo     = nullptr;   ///< Question
};

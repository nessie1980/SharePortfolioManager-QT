// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>

/**
 * @brief Dialog for configuring an API key.
 *
 * Generic dialog that allows the user to enter and save an API key
 * for a named service (e.g. "ApiYahoo").
 *
 * The service name is passed via the constructor and shown in the
 * window title and group box label.
 */
class ApiSettingsForm : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Construct the API key dialog.
     * @param serviceName  Name of the API service (e.g. "ApiYahoo").
     * @param currentKey   The currently configured API key (pre-filled).
     * @param parent       Parent widget.
     */
    explicit ApiSettingsForm(const QString& serviceName,
                             const QString& currentKey,
                             QWidget* parent = nullptr);

    /**
     * @brief Returns the API key entered by the user.
     * @return API key string.
     */
    QString apiKey() const;

private slots:
    void onSave();

private:
    void setupUi(const QString& serviceName, const QString& currentKey);

    QLineEdit*   m_editApiKey  = nullptr;
    QPushButton* m_btnSave     = nullptr;
    QPushButton* m_btnCancel   = nullptr;
};

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IViewShareEdit.h"
#include "IModelShareEdit.h"

#include <QObject>
#include <QString>

/**
 * @brief Presenter for the "Aktie editieren" dialog (MVP pattern).
 *
 * Responsibilities:
 * - Load the ShareObject from the model and populate the view on open.
 * - Compute and push all "Einnahmen / Ausgabe" aggregate values to the view.
 * - Validate input and persist changes on save.
 * - Emit signals when the user wants to navigate to sub-dialogs
 *   (Käufe, Verkäufe, Dividenden, Kosten).
 */
class PresenterShareEdit : public QObject
{
    Q_OBJECT

public:
    explicit PresenterShareEdit(IViewShareEdit*  view,
                                IModelShareEdit* model,
                                const QString&   shareGuid,
                                QObject*         parent = nullptr);

public slots:
    /** Called when the user clicks "Speichern". */
    void onSave();

    /** Called when the user clicks "Schließen" / "Abbrechen". */
    void onCancel();

    /** Called when the user clicks the pencil button next to "Käufe". */
    void onEditBuys();

    /** Called when the user clicks the pencil button next to "Verkäufe". */
    void onEditSales();

    /** Called when the user clicks the pencil button next to "Dividenden". */
    void onEditDividends();

    /** Called when the user clicks the pencil button next to "Kosten". */
    void onEditBrokerages();

    /**
     * @brief Refresh only the "Einnahmen / Ausgabe" aggregate values.
     *
     * Called by ViewShareEdit after the Käufe sub-dialog reports a data change
     * so the summary stays up to date without reopening this dialog.
     */
    void refreshSummary();

signals:
    /**
     * @brief Emitted when the user wants to open the buys sub-dialog.
     * @param shareGuid  GUID of the share whose buys should be shown.
     */
    void openBuysRequested(const QString& shareGuid);

    /**
     * @brief Emitted when the user wants to open the sales sub-dialog.
     */
    void openSalesRequested(const QString& shareGuid);

    /**
     * @brief Emitted when the user wants to open the dividends sub-dialog.
     */
    void openDividendsRequested(const QString& shareGuid);

    /**
     * @brief Emitted when the user wants to open the brokerages sub-dialog.
     */
    void openBrokeragesRequested(const QString& shareGuid);

private:
    /** Load share data and push all values to the view. */
    void loadAndPopulate();

    /** Push all aggregate figures to the "Einnahmen / Ausgabe" section. */
    void populateSummary();

    /** Validate all required fields; returns an error string or empty. */
    QString validateInput() const;

    IViewShareEdit*  m_view  = nullptr;
    IModelShareEdit* m_model = nullptr;
    QString          m_shareGuid;
};

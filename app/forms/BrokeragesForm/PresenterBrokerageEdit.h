// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IViewBrokerageEdit.h"
#include "IModelBrokerageEdit.h"

#include <QObject>
#include <QString>
#include <QList>

/**
 * @brief Presenter for the "Kosten hinzufügen / editieren" dialog (MVP pattern).
 *
 * Like PresenterDividendEdit, every record is fully editable and deletable at
 * any time — no latest-entry restriction. Records linked to a buy or sale
 * transaction are read-only (only the document path may be changed).
 *
 * No parse pipeline — brokerage data is entered manually.
 */
class PresenterBrokerageEdit : public QObject
{
    Q_OBJECT

public:
    explicit PresenterBrokerageEdit(IViewBrokerageEdit*  view,
                                    IModelBrokerageEdit* model,
                                    const QString&       shareGuid,
                                    QObject*             parent = nullptr);

public slots:
    void onSave();
    void onRemove();
    void onReset();
    void onClose();
    void onRowSelected(const QString& brokerageGuid);
    void onValuesChanged();
    void onDocumentSelected(const QString& path);
    void onDocumentPathEdited();

    // ── Live field validation ─────────────────────────────────────────────
    void onDateEdited();
    void onFeeEdited(const QString& fieldKey, double value);

signals:
    void dataChanged();

private:
    void    reloadOverview();
    void    refreshDerivedValues();
    QString validateInput() const;

    /** Returns true when the currently loaded record belongs to a buy/sale
     *  and is therefore read-only (only document path may change). */
    bool isLinkedRecord() const;

    IViewBrokerageEdit*    m_view;
    IModelBrokerageEdit*   m_model;
    QString                m_shareGuid;

    QList<BrokerageObject> m_brokerages;
    QString                m_currentGuid;   ///< GUID of currently loaded record, empty = new
};

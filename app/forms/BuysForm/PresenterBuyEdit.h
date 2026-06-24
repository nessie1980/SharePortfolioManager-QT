// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IViewBuyEdit.h"
#include "IModelBuyEdit.h"
#include "../../config/DocumentsConfig.h"
#include "../../libs/parser/src/Parser.h"

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>

/**
 * @brief Presenter for the "Käufe hinzufügen / editieren" dialog (MVP pattern).
 *
 * Parse pipeline is identical to PresenterShareAdd:
 * 1. onDocumentSelected() → pdftotext (QProcess) → onPdfConversionFinished()
 * 2. startParserForText() → BankIdentifier match → DocumentType detect → ParserLib
 * 3. onParserUpdated() → populateFromResult() → setFieldOk/Error + onParseFinished
 */
class PresenterBuyEdit : public QObject
{
    Q_OBJECT

public:
    explicit PresenterBuyEdit(IViewBuyEdit*    view,
                              IModelBuyEdit*   model,
                              const QString&   shareGuid,
                              DocumentsConfig* config,
                              QObject*         parent = nullptr);

public slots:
    void onSave();
    void onRemove();
    void onReset();
    void onClose();
    void onRowSelected(const QString& buyGuid);
    void onValuesChanged();
    void onDocumentSelected(const QString& path);

    // ── Live field validation ─────────────────────────────────────────────
    /** Called when the user leaves the date field. */
    void onDateEdited();
    /** Called when depot number selection changes. */
    void onDepotNumberEdited();
    /** Called when the user leaves the order number field. */
    void onOrderNumberEdited();
    /** Called when the user leaves volume or price. */
    void onVolumeOrPriceEdited();
    /** Called when the user leaves any optional fee field. */
    void onFeeEdited(const QString& fieldKey, double value);
    /** Called when the user selects a new document path. */
    void onDocumentPathEdited();

signals:
    void dataChanged();

private slots:
    /** Called when pdftotext (QProcess) finishes. */
    void onPdfConversionFinished(int exitCode, int exitStatus);

    /** Called by ParserLib::Parser on every state change. */
    void onParserUpdated(const ParserLib::ParserInfoState& state);

private:
    void    reloadOverview();
    void    refreshDerivedValues();
    QString validateInput() const;

    /** Returns true if @p buyGuid has the most recent dateTime in m_buys. */
    bool    isLatestBuy(const QString& buyGuid) const;

    /** Start ParserLib after pdftext was extracted — identical to PresenterShareAdd. */
    void startParserForText(const QString& pdfText);

    /** Distribute ParserLib results to the view — identical to PresenterShareAdd. */
    void populateFromResult(const QMap<QString, QList<QString>>& result);

    /** Map XML field name → view input widget key. */
    static QString xmlNameToViewField(const QString& xmlName);

    IViewBuyEdit*    m_view;
    IModelBuyEdit*   m_model;
    DocumentsConfig* m_config = nullptr;
    QString          m_shareGuid;

    QList<BuyObject> m_buys;
    QString          m_currentBuyGuid;
    bool             m_isLastBuy    = false;  ///< true when selected buy is most recent

    ParserLib::Parser m_parser;
    QString           m_pendingPdfPath;
    QString           m_pdfText;
};

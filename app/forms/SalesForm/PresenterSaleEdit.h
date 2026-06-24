// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IViewSaleEdit.h"
#include "IModelSaleEdit.h"
#include "../../config/DocumentsConfig.h"
#include "../../libs/parser/src/Parser.h"

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>

/**
 * @brief Presenter for the "Verkäufe hinzufügen / editieren" dialog (MVP pattern).
 *
 * Parse pipeline is identical to PresenterBuyEdit:
 * 1. onDocumentSelected() → pdftotext (QProcess) → onPdfConversionFinished()
 * 2. startParserForText() → BankIdentifier match → DocumentType detect → ParserLib
 * 3. onParserUpdated() → populateFromResult() → setFieldOk/Error + onParseFinished
 *
 * Sale-specific logic:
 * - Latest-sale detection via lexicographic ISO 8601 string comparison.
 * - canRemove = isLastSale (no sold-volume guard needed for sales).
 * - Derived values: saleValue, kaufwert, gewinnVerlust, gesGebühren, taxSum, auszahlung.
 */
class PresenterSaleEdit : public QObject
{
    Q_OBJECT

public:
    explicit PresenterSaleEdit(IViewSaleEdit*   view,
                               IModelSaleEdit*  model,
                               const QString&   shareGuid,
                               DocumentsConfig* config,
                               QObject*         parent = nullptr);

public slots:
    void onSave();
    void onRemove();
    void onReset();
    void onClose();
    void onRowSelected(const QString& saleGuid);
    void onValuesChanged();
    void onDocumentSelected(const QString& path);

    // ── Live field validation ─────────────────────────────────────────────
    void onDateEdited();
    void onDepotNumberEdited();
    void onOrderNumberEdited();
    void onVolumeOrPriceEdited();
    void onFeeEdited(const QString& fieldKey, double value);
    void onTaxEdited(const QString& fieldKey, double value);
    void onDocumentPathEdited();

signals:
    void dataChanged();

private slots:
    void onPdfConversionFinished(int exitCode, int exitStatus);
    void onParserUpdated(const ParserLib::ParserInfoState& state);

private:
    void    reloadOverview();
    void    refreshDerivedValues();
    QString validateInput() const;
    bool    isLatestSale(const QString& saleGuid) const;

    void startParserForText(const QString& pdfText);
    void populateFromResult(const QMap<QString, QList<QString>>& result);
    static QString xmlNameToViewField(const QString& xmlName);

    IViewSaleEdit*   m_view;
    IModelSaleEdit*  m_model;
    DocumentsConfig* m_config = nullptr;
    QString          m_shareGuid;

    QList<SaleObject> m_sales;
    QString           m_currentSaleGuid;
    bool              m_isLastSale = false;

    ParserLib::Parser m_parser;
    QString           m_pendingPdfPath;
    QString           m_pdfText;
};

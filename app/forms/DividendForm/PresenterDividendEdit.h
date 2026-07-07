// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IViewDividendEdit.h"
#include "IModelDividendEdit.h"
#include "../../libs/parser/src/Parser.h"
#include "../../config/DocumentsConfig.h"

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include <QDate>

/**
 * @brief Presenter for the "Dividenden hinzufügen / editieren" dialog (MVP pattern).
 *
 * Parse pipeline is identical to PresenterBuyEdit:
 * 1. onDocumentSelected() → pdftotext (QProcess) → onPdfConversionFinished()
 * 2. startParserForText() → BankIdentifier match → DocumentType detect → ParserLib
 * 3. onParserUpdated() → populateFromResult() → setFieldOk/Error + onParseFinished
 *
 * Dividend-specific logic:
 * - Any dividend may be edited or deleted at any time — no latest-entry restriction.
 * - Derived values: dividendPayout, dividendPayoutFc, taxSum,
 *                   dividendPayoutWithTaxes, yield.
 * - Foreign currency mode: enableForeignCurrency checkbox toggles extra fields.
 */
class PresenterDividendEdit : public QObject
{
    Q_OBJECT

public:
    explicit PresenterDividendEdit(IViewDividendEdit* view,
                                   IModelDividendEdit* model,
                                   const QString&      shareGuid,
                                   DocumentsConfig*    config,
                                   QObject*            parent = nullptr);

public slots:
    void onSave();
    void onRemove();
    void onReset();
    void onClose();
    void onRowSelected(const QString& dividendGuid);
    void onValuesChanged();
    void onDocumentSelected(const QString& path);
    void onForeignCurrencyToggled(bool enabled);

    // ── Live field validation ─────────────────────────────────────────────
    void onDateEdited();
    void onRateEdited();
    void onVolumeEdited();
    void onPriceAtPaydayEdited();
    void onExchangeRatioEdited();
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

    /**
     * @brief Look up the closing price for @p date via IModelDividendEdit and,
     *        on a hit, overwrite "priceAtPayday" with it. No-op on a miss.
     *        Shared by onDateEdited() (manual date edit) and
     *        populateFromResult() (date parsed from a PDF document) so both
     *        paths use the actually-current payout date rather than a
     *        possibly stale one (see ARCHITECTURE.md, DividendForm-Details).
     */
    void applyDailyValuePriceAtPayday(const QDate& date);

    void startParserForText(const QString& pdfText);
    void populateFromResult(const QMap<QString, QList<QString>>& result);
    static QString xmlNameToViewField(const QString& xmlName);

    IViewDividendEdit*  m_view;
    IModelDividendEdit* m_model;
    DocumentsConfig*    m_config    = nullptr;
    QString             m_shareGuid;

    QList<DividendObject> m_dividends;
    QString               m_currentDividendGuid;

    ParserLib::Parser m_parser;
    QString           m_pendingPdfPath;
    QString           m_pdfText;
};

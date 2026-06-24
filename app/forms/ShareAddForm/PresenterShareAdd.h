// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "IViewShareAdd.h"
#include "IModelShareAdd.h"
#include "../../config/DocumentsConfig.h"
#include "../../libs/parser/src/Parser.h"
#include "../../libs/parser/src/ParsingValues.h"
#include "../../libs/parser/src/DataTypes.h"

#include <QObject>
#include <QString>
#include <QMap>

/**
 * @brief Presenter for the "Aktie hinzufügen" dialog (MVP pattern).
 *
 * Coordinates the View and the Model:
 * - Receives UI events forwarded by ViewShareAdd (onSave, onCancel, onDocumentSelected).
 * - Runs the PDF → text → regex parse pipeline when a document is chosen.
 * - Populates view fields with parsed values and marks them ok/error.
 * - Validates input and delegates the final save to IModelShareAdd.
 *
 * ### PDF parse pipeline
 * 1. `pdftotext` converts the PDF to a plain-text string (QProcess).
 * 2. DocumentsConfig provides the bank-specific regex rules from Documents.xml.
 * 3. ParserLib::Parser applies the regex rules and emits a result map.
 * 4. onParseFinished() distributes the results back to the view.
 */
class PresenterShareAdd : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Construct the Presenter and wire it to its View and Model.
     * @param view    Non-owning pointer to the IViewShareAdd implementation.
     * @param model   Non-owning pointer to the IModelShareAdd implementation.
     * @param config  Loaded DocumentsConfig (Documents.xml already parsed).
     * @param parent  Optional Qt parent for memory management.
     */
    explicit PresenterShareAdd(IViewShareAdd*    view,
                               IModelShareAdd*   model,
                               DocumentsConfig*  config,
                               QObject*          parent = nullptr);

    // ── Slots called by the View ──────────────────────────────────────────

public slots:
    /**
     * @brief Called when the user clicks "…" to select a PDF document.
     * @param filePath  Full path to the selected PDF file.
     *
     * Converts the PDF to text via pdftotext, then starts the regex
     * parser to populate form fields automatically.
     */
    void onDocumentSelected(const QString& filePath);

    /**
     * @brief Called when the user clicks "Speichern".
     *
     * Validates all required fields, builds ShareObject + BuyObject,
     * and calls IModelShareAdd::saveShareWithBuy(). On success the
     * view is closed; on error the view shows the error message.
     */
    void onSave();

    /**
     * @brief Called when the user clicks "Abbrechen".
     *
     * Closes the view without saving.
     */
    void onCancel();

private slots:
    /// Called when pdftotext (QProcess) finishes — kicks off the ParserLib parse.
    void onPdfConversionFinished(int exitCode, int exitStatus);

    /// Called by ParserLib::Parser on every state change.
    void onParserUpdated(const ParserLib::ParserInfoState& state);

private:
    // ── Helpers ───────────────────────────────────────────────────────────

    /**
     * @brief Determine bank + document type from the PDF text, then start
     *        the ParserLib::Parser with the matching RegExList.
     * @param pdfText  Plain text extracted from the PDF by pdftotext.
     */
    void startParserForText(const QString& pdfText);

    /**
     * @brief Distribute ParserLib results to the view after Finished.
     * @param result  searchResult map from ParserInfoState.
     */
    void populateFromResult(const QMap<QString, QList<QString>>& result);

    /**
     * @brief Map a Documents.xml field name to the IViewShareAdd field key.
     */
    static QString xmlNameToViewField(const QString& xmlName);

    /**
     * @brief Validate that the minimum required fields are filled.
     * @return Empty string on success; a human-readable error message otherwise.
     */
    QString validateInput() const;

    // ── Members ───────────────────────────────────────────────────────────
    IViewShareAdd*      m_view   = nullptr;
    IModelShareAdd*     m_model  = nullptr;
    DocumentsConfig*    m_config = nullptr;

    ParserLib::Parser   m_parser;
    QString             m_pendingPdfPath;
    QString             m_pdfText;
};

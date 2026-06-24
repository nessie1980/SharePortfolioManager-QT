// MIT License
// Copyright (c) 2021 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "DataTypes.h"
#include <QUrl>
#include <QString>

namespace ParserLib {

/**
 * @brief Configuration for a single parse operation.
 *
 * `ParsingValues` encapsulates everything the Parser needs to know before
 * starting a parse operation. Three construction modes are supported:
 *
 * | Mode | Constructor | Description |
 * |------|-------------|-------------|
 * | Text + Regex | `ParsingValues(text, encoding, regexList)` | Parse a given string directly |
 * | URL + Regex  | `ParsingValues(url, encoding, regexList)`  | Download URL, then apply regex |
 * | URL + JSON   | `ParsingValues(url, apiKey, encoding, type)` | Download URL, parse as JSON |
 *
 * ### Example — Regex on text
 * @code
 * RegExList rules;
 * rules["Price"] = RegExElement{ R"((\d+[.,]\d+))", 0, false, {} };
 * ParsingValues pv("Kurs: 142,50 EUR", "UTF-8", rules);
 * @endcode
 *
 * ### Example — OnVista JSON
 * @code
 * ParsingValues pv(
 *     QUrl("https://api.onvista.de/api/v1/instruments/STOCK/..."),
 *     apiKey, "UTF-8",
 *     ParsingType::OnVistaRealTime
 * );
 * @endcode
 */
class ParsingValues
{
public:
    /// Default constructor — creates an empty/invalid ParsingValues.
    ParsingValues() = default;

    /**
     * @brief Parse a given text string with a regex rule list.
     * @param parsingText  The text to parse (e.g. extracted PDF content)
     * @param encoding     Text encoding (e.g. "UTF-8")
     * @param regexList    Named regex rules to apply
     */
    ParsingValues(const QString& parsingText,
                  const QString& encoding,
                  const RegExList& regexList)
        : m_loadingType(LoadType::Text)
        , m_parsingType(ParsingType::Regex)
        , m_encodingType(encoding)
        , m_parsingText(parsingText)
        , m_regexList(regexList)
    {}

    /**
     * @brief Download URL content and parse it with a regex rule list.
     * @param webSiteUrl   URL to download
     * @param encoding     Expected content encoding (e.g. "UTF-8")
     * @param regexList    Named regex rules to apply to the downloaded content
     */
    ParsingValues(const QUrl& webSiteUrl,
                  const QString& encoding,
                  const RegExList& regexList)
        : m_loadingType(LoadType::Web)
        , m_parsingType(ParsingType::Regex)
        , m_webSiteUrl(webSiteUrl)
        , m_encodingType(encoding)
        , m_regexList(regexList)
    {}

    /**
     * @brief Download URL content and parse it as a JSON API response.
     * @param webSiteUrl   URL of the JSON API endpoint
     * @param apiKey       API key sent as `X-API-KEY` header (empty if not required)
     * @param encoding     Expected content encoding
     * @param parsingType  JSON parsing strategy (OnVistaRealTime, YahooHistoryData, etc.)
     */
    ParsingValues(const QUrl& webSiteUrl,
                  const QString& apiKey,
                  const QString& encoding,
                  ParsingType parsingType)
        : m_loadingType(LoadType::Web)
        , m_parsingType(parsingType)
        , m_webSiteUrl(webSiteUrl)
        , m_apiKey(apiKey)
        , m_encodingType(encoding)
    {}

    // ── Accessors ────────────────────────────────────────────────────────
    LoadType    loadingType()  const { return m_loadingType; }   ///< How content is loaded
    ParsingType parsingType()  const { return m_parsingType; }   ///< How content is parsed
    QUrl        webSiteUrl()   const { return m_webSiteUrl; }    ///< URL to download
    QString     apiKey()       const { return m_apiKey; }        ///< API authentication key
    QString     encodingType() const { return m_encodingType; }  ///< Content encoding
    QString     parsingText()  const { return m_parsingText; }   ///< Direct text to parse
    RegExList   regexList()    const { return m_regexList; }     ///< Regex rule set

    /**
     * @brief Returns true if this ParsingValues is usable for a parse operation.
     *
     * For Web mode: URL must be valid and non-empty.
     * For Text mode: text must be non-empty.
     */
    bool isValid() const {
        if (m_loadingType == LoadType::Web)
            return m_webSiteUrl.isValid() && !m_webSiteUrl.isEmpty();
        return !m_parsingText.isEmpty();
    }

private:
    LoadType    m_loadingType  = LoadType::Text;
    ParsingType m_parsingType  = ParsingType::Regex;
    QUrl        m_webSiteUrl;
    QString     m_apiKey;
    QString     m_encodingType = QStringLiteral("UTF-8");
    QString     m_parsingText;
    RegExList   m_regexList;
};

} // namespace ParserLib

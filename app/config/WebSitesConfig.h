// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include "../../libs/parser/src/DataTypes.h"

#include <QString>
#include <QList>
#include <QMap>

/**
 * @brief Holds the parsing configuration for a single website.
 *
 * Each entry maps to one `<WebSite>` element in `WebSites.xml`.
 * The `regexList` contains the named regex rules used by the Parser
 * (keys: "LastDate", "LastTime", "Price", "PriceBefore", "Currency").
 */
struct WebSiteEntry
{
    QString            id;          ///< Website identifier (e.g. "www.finanzen.net/aktien")
    QString            encoding;    ///< HTTP response encoding (e.g. "UTF-8")
    ParserLib::RegExList regexList; ///< Named regex rules for this website
};

/**
 * @brief Loads and provides access to the website parsing configuration.
 *
 * Reads `WebSites.xml` and exposes the entries as a list of `WebSiteEntry`
 * objects. Each entry is used by the Parser to extract market data
 * (price, date, time, currency) from a specific website via regex.
 *
 * ### XML structure
 * ```xml
 * <WebSites>
 *   <WebSite Id="www.finanzen.net/aktien" Encoding="UTF-8">
 *     <WebSitePrice     Name="Price"      FoundIndex="0" ResultEmpty="false"
 *                       RegexOptions="Multiline">..regex..</WebSitePrice>
 *     <WebSiteLastDate  Name="LastDate"   FoundIndex="0" ResultEmpty="false"
 *                       RegexOptions="None">..regex..</WebSiteLastDate>
 *     ...
 *   </WebSite>
 * </WebSites>
 * ```
 *
 * ### Usage
 * @code
 * WebSitesConfig config;
 * if (config.load("/path/to/WebSites.xml")) {
 *     const auto entry = config.findById("www.finanzen.net/aktien");
 *     if (entry) {
 *         parser.setParsingValues(
 *             ParsingValues(url, entry->encoding, entry->regexList));
 *     }
 * }
 * @endcode
 */
class WebSitesConfig
{
public:
    /**
     * @brief Load error codes returned by load().
     */
    enum class LoadResult {
        Success          =  0, ///< File loaded successfully
        FileNotFound     = -1, ///< XML file does not exist
        EmptyConfig      = -2, ///< XML file contains no WebSite entries
        XmlParseError    = -3, ///< XML is malformed
        AttributeError   = -4, ///< A required XML attribute is missing
        LoadFailed       = -5  ///< Unspecified load failure
    };

    WebSitesConfig() = default;

    /**
     * @brief Load the website configuration from an XML file.
     * @param filePath  Full path to the WebSites.xml file.
     * @return LoadResult::Success on success, error code otherwise.
     */
    LoadResult load(const QString& filePath);

    /**
     * @brief Returns all loaded website entries.
     * @return List of WebSiteEntry objects, empty if not loaded.
     */
    QList<WebSiteEntry> entries() const { return m_entries; }

    /**
     * @brief Find a website entry by its ID.
     * @param id  Website identifier (e.g. "www.finanzen.net/aktien").
     * @return Pointer to the matching entry, or nullptr if not found.
     */
    const WebSiteEntry* findById(const QString& id) const;

    /**
     * @brief Returns true if the config was successfully loaded and is non-empty.
     * @return true if at least one entry is available.
     */
    bool isValid() const { return !m_entries.isEmpty(); }

    /**
     * @brief Returns the number of loaded website entries.
     * @return Entry count.
     */
    int count() const { return m_entries.size(); }

    /**
     * @brief Returns the last error message, if any.
     * @return Human-readable error description, empty if no error.
     */
    QString lastError() const { return m_lastError; }

private:
    /**
     * @brief Parse a regex options string into a list of PatternOption flags.
     *
     * Supported option names (case-insensitive, space- or comma-separated):
     * "None", "Multiline", "Singleline", "CaseInsensitive".
     * @param optionsStr  Options string from XML attribute.
     * @return List of PatternOption flags (empty for "None").
     */
    static QList<QRegularExpression::PatternOption> parseRegexOptions(
        const QString& optionsStr);

    QList<WebSiteEntry> m_entries;
    QString             m_lastError;
};

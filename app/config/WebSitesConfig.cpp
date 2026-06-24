// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#include "WebSitesConfig.h"

#include <QFile>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QDebug>

// ── load ──────────────────────────────────────────────────────────────────────

WebSitesConfig::LoadResult WebSitesConfig::load(const QString& filePath)
{
    m_entries.clear();
    m_lastError.clear();

    if (!QFile::exists(filePath)) {
        m_lastError = QStringLiteral("File not found: %1").arg(filePath);
        qWarning() << "[WebSitesConfig]" << m_lastError;
        return LoadResult::FileNotFound;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = QStringLiteral("Cannot open file: %1").arg(filePath);
        qWarning() << "[WebSitesConfig]" << m_lastError;
        return LoadResult::LoadFailed;
    }

    QXmlStreamReader xml(&file);

    while (!xml.atEnd() && !xml.hasError()) {
        if (xml.readNext() != QXmlStreamReader::StartElement)
            continue;

        if (xml.name() != QStringLiteral("WebSite"))
            continue;

        // ── Read WebSite attributes ───────────────────────────────────────
        const auto attributes = xml.attributes();
        const QString siteId       = attributes.value(QStringLiteral("Id")).toString();
        const QString siteEncoding = attributes.value(QStringLiteral("Encoding")).toString();

        if (siteId.isEmpty() || siteEncoding.isEmpty()) {
            m_lastError = QStringLiteral("Missing Id or Encoding attribute in <WebSite>");
            qWarning() << "[WebSitesConfig]" << m_lastError;
            return LoadResult::AttributeError;
        }

        // ── Read child regex elements ─────────────────────────────────────
        ParserLib::RegExList regexList;

        while (!xml.atEnd()) {
            xml.readNext();

            if (xml.isEndElement() && xml.name() == QStringLiteral("WebSite"))
                break;

            if (!xml.isStartElement())
                continue;

            const auto childAttrs       = xml.attributes();
            const QString regexName     = childAttrs.value(QStringLiteral("Name")).toString();
            const QString foundIndexStr = childAttrs.value(QStringLiteral("FoundIndex")).toString();
            const QString resultEmpty   = childAttrs.value(QStringLiteral("ResultEmpty")).toString();
            const QString regexOptions  = childAttrs.value(QStringLiteral("RegexOptions")).toString();
            const QString regexExpr     = xml.readElementText();

            if (regexName.isEmpty()) {
                m_lastError = QStringLiteral("Missing Name attribute in regex element inside <WebSite Id=\"%1\">")
                                  .arg(siteId);
                qWarning() << "[WebSitesConfig]" << m_lastError;
                return LoadResult::AttributeError;
            }

            ParserLib::RegExElement element;
            element.regexExpression  = regexExpr;
            element.regexFoundPosition = foundIndexStr.toInt();
            element.resultEmpty      = (resultEmpty.toLower() == QStringLiteral("true"));
            element.regexOptions = parseRegexOptions(regexOptions);

            regexList.insert(regexName, element);
        }

        WebSiteEntry entry;
        entry.id        = siteId;
        entry.encoding  = siteEncoding;
        entry.regexList = regexList;
        m_entries.append(entry);
    }

    if (xml.hasError()) {
        m_lastError = QStringLiteral("XML parse error in %1: %2")
                          .arg(filePath, xml.errorString());
        qWarning() << "[WebSitesConfig]" << m_lastError;
        return LoadResult::XmlParseError;
    }

    if (m_entries.isEmpty()) {
        m_lastError = QStringLiteral("No <WebSite> entries found in %1").arg(filePath);
        qWarning() << "[WebSitesConfig]" << m_lastError;
        return LoadResult::EmptyConfig;
    }

    qInfo() << "[WebSitesConfig] Loaded" << m_entries.size()
            << "entries from" << filePath;
    return LoadResult::Success;
}

// ── findById ──────────────────────────────────────────────────────────────────

const WebSiteEntry* WebSitesConfig::findById(const QString& id) const
{
    for (const auto& entry : std::as_const(m_entries)) {
        if (entry.id == id)
            return &entry;
    }
    return nullptr;
}

// ── parseRegexOptions ─────────────────────────────────────────────────────────

QList<QRegularExpression::PatternOption> WebSitesConfig::parseRegexOptions(
    const QString& optionsStr)
{
    QList<QRegularExpression::PatternOption> options;

    // Options may be comma- or space-separated, case-insensitive
    const QStringList tokens = optionsStr.split(
        QRegularExpression(QStringLiteral("[,\\s]+")),
        Qt::SkipEmptyParts);

    for (const QString& token : tokens) {
        const QString lower = token.toLower();
        if (lower == QStringLiteral("multiline"))
            options.append(QRegularExpression::MultilineOption);
        else if (lower == QStringLiteral("singleline"))
            options.append(QRegularExpression::DotMatchesEverythingOption);
        else if (lower == QStringLiteral("caseinsensitive"))
            options.append(QRegularExpression::CaseInsensitiveOption);
        // "None" and unknown values → no option added
    }

    return options;
}

// MIT License
// Copyright (c) 2017 nessie1980 (nessie1980@gmx.de)
#pragma once

#include <QString>
#include <QStringList>
#include <QList>

/**
 * @brief Rewrites document paths stored in the database when the documents
 *        root directory changes (e.g. moving from a Windows machine to
 *        Linux, or reorganizing where belongs/documents are kept).
 *
 * Touches only the `document` columns in `buys`, `sales`, `brokerage` and
 * `dividends` (via the respective repositories' `updateDocument()`) — no
 * file-system operations (no copy/move) are performed. The physical files
 * are expected to already live at their new location before the rewrite
 * runs (see ARCHITECTURE.md, "Dokument-Root-Verzeichnis").
 *
 * ### Usage
 * @code
 * const auto result = DocumentRootMigrator::changeRoot(oldRoot, newRoot);
 * // show result.rewritten / result.alreadyInRoot / result.outsideRoot to the user
 * @endcode
 *
 * ### Cross-platform paths
 * A document path may have been written on a different OS than the one the
 * application is currently running on (typical case: paths were saved on
 * Windows, e.g. "B:\Depot\...", and the application now runs on Linux).
 * Both the old- and new-root comparison and the common-root detection
 * (detectCommonRoot()) work independently of the host OS: backslashes are
 * normalized to forward slashes, and a Windows drive letter (e.g. "B:") is
 * recognized as an absolute path prefix even when Qt itself was built for
 * Linux/macOS. The old root does NOT need to exist on this machine — it is
 * only used as a literal string prefix to match against stored paths.
 */
class DocumentRootMigrator
{
public:
    /**
     * @brief Summary of a changeRoot() run.
     */
    struct Result {
        int rewritten     = 0;  ///< Paths rewritten and persisted successfully
        int alreadyInRoot = 0;  ///< Paths already identical to their target — untouched
        int outsideRoot   = 0;  ///< Paths that didn't start with oldRoot — untouched
        int updateFailed  = 0;  ///< DB update calls that failed (see qWarning() log)

        /// The untouched (outsideRoot) paths, for a detailed report to the user.
        QStringList outsidePaths;

        /// Total number of documents inspected.
        int total() const { return rewritten + alreadyInRoot + outsideRoot; }
    };

    /**
     * @brief Rewrite all document paths that start with oldRoot to start
     *        with newRoot instead (plain string prefix replacement).
     * @param oldRoot  Old root directory prefix to match against. Does NOT
     *                 need to exist on this machine (e.g. a Windows path
     *                 while running on Linux) — it's purely a string match.
     * @param newRoot  New root directory to replace it with.
     * @return Summary counts.
     */
    static Result changeRoot(const QString& oldRoot, const QString& newRoot);

    /**
     * @brief Result of a read-only root-directory detection run.
     */
    struct DetectionResult {
        /// Detected common parent directory of all absolute document paths,
        /// empty if none could be determined (see the fields below for why).
        QString suggestedRoot;

        /// true if two or more documents have absolute paths that do NOT
        /// share a common parent directory (scattered across unrelated
        /// folders/drives) — suggestedRoot is empty whenever this is true.
        bool ambiguous = false;

        /// Number of documents whose stored path is absolute (Unix- or
        /// Windows-style, independent of the host OS — see class docs).
        int absoluteCount = 0;

        /// Number of documents whose stored path is not absolute (bare
        /// filename or relative path) — cannot contribute to detection.
        int relativeCount = 0;

        /// Total number of documents inspected.
        int total() const { return absoluteCount + relativeCount; }
    };

    /**
     * @brief Read-only detection of a shared parent directory among all
     *        existing document paths — no database writes.
     *
     * Used by DocumentsSettingsForm to pre-fill the "alter Root-Pfad" field
     * with a best guess; the user reviews and can freely edit it before
     * confirming — the suggestion does not need to exist on this machine.
     * @return Detection outcome — see DetectionResult.
     */
    static DetectionResult detectCommonRoot();

private:
    /// One document path together with its origin table/row for the update call.
    struct DocumentEntry {
        enum class Table { Buy, Sale, Brokerage, Dividend };
        Table   table;
        QString guid;
        QString path;
    };

    /// Collects every non-empty document path across all shares/tables.
    static QList<DocumentEntry> collectAllDocuments();

    /// Replaces every backslash with a forward slash so path comparisons
    /// work the same regardless of which OS wrote (or is reading) the path.
    static QString normalizePathSeparators(const QString& path);

    /// Returns true if the ALREADY NORMALIZED (forward-slash) path is
    /// absolute — Unix-style ("/...") or Windows-style with a drive letter
    /// ("B:/..."), independent of the host OS.
    static bool isAbsoluteDocumentPath(const QString& normalizedPath);

    /// Returns the longest common parent directory of the given paths, or
    /// an empty string if none exists. Non-absolute paths are ignored.
    static QString longestCommonDirectory(const QStringList& paths);

    /// Dispatches to the correct repository's updateDocument() by table.
    static bool updateDocument(const DocumentEntry& entry, const QString& newPath);
};

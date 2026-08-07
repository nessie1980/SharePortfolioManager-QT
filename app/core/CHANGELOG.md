# Changelog — Database

Eigenständige Datenbank-Bibliothek (`app/core`, SQLite-Wrapper). Eigene
Versionsnummer, unabhängig von der SharePortfolioManager-App-Version — siehe
`app/core/CMakeLists.txt` (`project(Database VERSION x.y.z ...)`) und
ARCHITECTURE.md, Abschnitt "Versionierung".

Format angelehnt an [Keep a Changelog](https://keepachangelog.com/de/1.0.0/),
Versionierung nach [Semantic Versioning](https://semver.org/lang/de/).

## [Unreleased]

## [1.1.0] - 2026-08-07
### Hinzugefügt
- Neue Tabelle `share_splits` für Aktiensplits (`guid`, `share_guid`,
  `date`, `ratio_new`, `ratio_old`, `prices_adjusted`, `comment`,
  `UNIQUE(share_guid, date)`) inkl. Index `idx_splits_share`. Grundlage für
  die Aktiensplit-Behandlung der Anwendung — siehe deren CHANGELOG.md,
  `[1.9.0]`, sowie `docs/architecture/ARCHITECTURE.md`, "Offene Punkte",
  "Aktiensplits werden nicht behandelt".

## [1.0.0] - 2026-07-28
### Hinzugefügt
- Zentrale SQLite-Verbindungsverwaltung (Singleton) über `QSqlDatabase`.
- Automatische Schema-Erstellung (`createSchema()`), WAL-Modus und
  Foreign-Key-Erzwingung.
- Transaktionsunterstützung (`beginTransaction()`/`commitTransaction()`/
  `rollbackTransaction()`).

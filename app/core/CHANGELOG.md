# Changelog — Database

Eigenständige Datenbank-Bibliothek (`app/core`, SQLite-Wrapper). Eigene
Versionsnummer, unabhängig von der SharePortfolioManager-App-Version — siehe
`app/core/CMakeLists.txt` (`project(Database VERSION x.y.z ...)`) und
ARCHITECTURE.md, Abschnitt "Versionierung".

Format angelehnt an [Keep a Changelog](https://keepachangelog.com/de/1.0.0/),
Versionierung nach [Semantic Versioning](https://semver.org/lang/de/).

## [Unreleased]

## [1.0.0] - 2026-07-28
### Hinzugefügt
- Zentrale SQLite-Verbindungsverwaltung (Singleton) über `QSqlDatabase`.
- Automatische Schema-Erstellung (`createSchema()`), WAL-Modus und
  Foreign-Key-Erzwingung.
- Transaktionsunterstützung (`beginTransaction()`/`commitTransaction()`/
  `rollbackTransaction()`).

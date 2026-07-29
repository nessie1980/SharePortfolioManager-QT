# Changelog — Parser

Eigenständige, wiederverwendbare Parsing-Bibliothek (`libs/parser`). Eigene
Versionsnummer, unabhängig von der SharePortfolioManager-App-Version — siehe
`libs/parser/CMakeLists.txt` (`project(Parser VERSION x.y.z ...)`) und
ARCHITECTURE.md, Abschnitt "Versionierung".

Format angelehnt an [Keep a Changelog](https://keepachangelog.com/de/1.0.0/),
Versionierung nach [Semantic Versioning](https://semver.org/lang/de/).

## [Unreleased]

## [1.0.0] - 2026-07-28
### Hinzugefügt
- Asynchrones Parsing über `QNetworkAccessManager`, nicht-blockierend.
- Drei Parsing-Modi: Regex auf Text, OnVista-JSON, Yahoo-JSON.
- Test-Seam über zweiten Konstruktor (`Parser(QNetworkAccessManager*, ...)`)
  für Netzwerk-Mocking in Unit-Tests.

# Changelog — Logger

Eigenständige, wiederverwendbare Logging-Bibliothek (`libs/logger`). Eigene
Versionsnummer, unabhängig von der SharePortfolioManager-App-Version — siehe
`libs/logger/CMakeLists.txt` (`project(Logger VERSION x.y.z ...)`) und
ARCHITECTURE.md, Abschnitt "Versionierung".

Format angelehnt an [Keep a Changelog](https://keepachangelog.com/de/1.0.0/),
Versionierung nach [Semantic Versioning](https://semver.org/lang/de/).

## [Unreleased]

## [1.0.0] - 2026-07-28
### Hinzugefügt
- Level-basiertes Logging mit konfigurierbaren State- und Component-
  Bit-Flags (bis zu 16 Zustände/Komponenten).
- Datei-Logging mit Rotation (`cleanUpLogFiles()`).
- `entryAdded()`-Signal für automatische GUI-Anbindung.
- Frei definierbare Zustandsnamen und -farben je Level.

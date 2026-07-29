# Changelog

Alle nennenswerten Änderungen an diesem Projekt werden in dieser Datei
dokumentiert.

Format angelehnt an [Keep a Changelog](https://keepachangelog.com/de/1.0.0/),
Versionierung nach [SemVer](https://semver.org/lang/de/).

## [1.0.1] - 2026-07-29

### Fixed

- `settings.ini` wurde im Linux-AppImage nie dauerhaft gespeichert: nach
  jedem Neustart musste u. a. das Dokumente-Root-Verzeichnis erneut gewählt
  werden. Ursache war `AppStartup::settingsPath()`, das auf
  `QCoreApplication::applicationDirPath()` basierte — im AppImage der bei
  jedem Start neu und zufällig gemountete FUSE-Pfad. Betraf strukturell auch
  Windows-Standardinstallationen unter `Program Files`, wo dieses
  Verzeichnis für normale Benutzerkonten nicht ohne Weiteres beschreibbar
  ist. Umgestellt auf `QStandardPaths::AppConfigLocation`, stabil und
  nutzerseitig beschreibbar über alle Paketierungsformen hinweg (AppImage,
  Windows-Installer, portabler Build). Siehe
  `docs/architecture/ARCHITECTURE.md`, "settings.ini nicht persistent im
  AppImage".

[1.0.1]: https://github.com/nessie1980/SharePortfolioManager-QT/compare/v1.0.0...v1.0.1

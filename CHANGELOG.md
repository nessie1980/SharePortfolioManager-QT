# Changelog

Alle nennenswerten Änderungen an diesem Projekt werden in dieser Datei
dokumentiert.

Format angelehnt an [Keep a Changelog](https://keepachangelog.com/de/1.0.0/),
Versionierung nach [SemVer](https://semver.org/lang/de/).

## [1.1.0] - 2026-07-30

### Added

- Einheitliche Selektionsfarbe (blauer Hintergrund, gelbe Schrift) für die
  selektierte Zeile in allen Grids der Anwendung, analog zur
  C#-Referenzanwendung — bisher nutzten alle Tabellen stattdessen die
  Standard-Highlight-Farbe der Qt-Palette bzw. des Systemthemes,
  uneinheitlich je nach Plattform. Neuer zentraler Helper `GridStyle`
  (`app/widgets/GridStyle.h`), angewandt auf beide MainWindow-Haupttabellen
  (Depotwert, Marktwert) sowie über `OverviewTabWidget` automatisch auf alle
  fünf Edit-Dialoge und die drei Tabs in ShareDetailsForm. Siehe
  `docs/architecture/ARCHITECTURE.md`, "Grid-Selektionsfarbe (Blau/Gelb) in
  allen Grids".

### Fixed

- Die von `TwoLineDelegate` gerenderten (zweizeiligen) Spalten der beiden
  MainWindow-Haupttabellen (Kosten/Dividenden, Preis, Vortag, Aktuelle
  Entwicklung, Einzahlung/Marktwert, Komplette Entwicklung, Kpl.
  Einzahlung/Kpl. Marktwert) übernahmen die neue Grid-Selektionsfarbe
  zunächst nicht, da Qt eine per Stylesheet gesetzte `item:selected`-Regel
  nicht in eine über `QPalette` abfragbare Farbe zurückspiegelt. Siehe
  `docs/architecture/ARCHITECTURE.md`, "TwoLineDelegate".

## [1.0.2] - 2026-07-29

### Changed

- `OrganizationName` (fließt über `QStandardPaths::AppConfigLocation` in den
  Konfigurationspfad unter Linux ein, siehe `1.0.1`) von `"nessie1980"` auf
  den neutralen Bezeichner `"BT"` geändert.
- Die Programmversion wird jetzt aus einer zentralen, per CMake generierten
  `Version.h` bezogen (`app/Version.h.in` → `configure_file()`), statt aus
  einem separaten Hardcode in `main.cpp`. Verhindert künftig, dass ein
  Versionsbump an einer zweiten Stelle vergessen wird — genau das war beim
  Bump auf `1.0.1` passiert. Siehe `docs/architecture/ARCHITECTURE.md`,
  "Versionierung".

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

[1.1.0]: https://github.com/nessie1980/SharePortfolioManager-QT/compare/v1.0.2...v1.1.0
[1.0.2]: https://github.com/nessie1980/SharePortfolioManager-QT/compare/v1.0.1...v1.0.2
[1.0.1]: https://github.com/nessie1980/SharePortfolioManager-QT/compare/v1.0.0...v1.0.1

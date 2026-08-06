# Changelog

Alle nennenswerten Änderungen an diesem Projekt werden in dieser Datei
dokumentiert.

Format angelehnt an [Keep a Changelog](https://keepachangelog.com/de/1.0.0/),
Versionierung nach [SemVer](https://semver.org/lang/de/).

## [1.7.0] - 2026-08-06

### Added

- Neuer Tab "Depotwert-Chart" im Hauptfenster (zwischen "Kompletter
  Depotwert" und "Kompletter Marktwert"): stellt die tatsächliche
  Wertentwicklung des gesamten Portfolios über die Zeit dar. Die Linie
  bewegt sich ausschliesslich durch Kursänderungen, Dividenden, realisierte
  Verkaufsgewinne und Kosten — Ein- und Auszahlungen verschieben sie nicht,
  ein Kauf über 5.000 Euro lässt sie also unverändert. Zeitraumsteuerung
  (Start-Datum / Interval / Anzahl) und Mausrad-Zoom wie im Aktien-Chart,
  Kurve abschnittsweise grün/rot nach Vorzeichen mit waagerechter
  Null-Linie, Hover-Tooltip mit Datum sowie Entwicklung in Euro und Prozent.
  Aktien ohne Tageswert-Historie werden ausgeschlossen und in einer
  Warnzeile unter dem Chart benannt. Siehe
  `docs/architecture/ARCHITECTURE.md`, "PortfolioChartForm-Details".
- Neuer, datenbankfreier Rechenkern `PortfolioSeriesCalculator`
  (`app/utils/`) mit eigenem Testziel `tst_portfolioseriescalculator`.
- Diagnose-Export im Depotwert-Chart: der Knopf "Diagnose speichern…"
  schreibt je Aktie die Anzahl geladener Datensätze und je Stichtag alle
  Bestandteile der Berechnung als CSV.

### Fixed

- Depotwert-Chart: der Hover-Tooltip zeigte die Cursorposition statt des
  Datenpunkts, weil `QLineSeries::hovered()` Achsenkoordinaten liefert. Bei
  gleichem Datum erschienen dadurch je nach Zeigerhöhe unterschiedliche
  Eurobeträge, und der Prozentwert blieb konstant bei 0,00 %. Der Tooltip
  rastet jetzt auf den nächstgelegenen Datenpunkt ein.
- Depotwert-Chart: Einträge ohne gültiges Datum wurden am ersten Stichtag
  verbucht statt ignoriert, weil ein ungültiges `QDate` in Qt kleiner ist als
  jedes gültige. Die Kurve zeigte dadurch Kosten Jahre vor dem ersten Kauf.
  Solche Einträge fliessen jetzt nicht mehr ein und werden im Diagnose-Export
  gezählt.
- Depotwert-Chart: die Obergrenze für "Anzahl" richtet sich nach dem ersten
  Kauf statt nach dem ältesten Tageswert — vor dem ersten Kauf gibt es kein
  Portfolio, die Kurve läge dort zwangsläufig auf null.
- Das Testziel `tst_sharecalculator` war in keiner `CMakeLists.txt`
  eingetragen und wurde daher nie gebaut, obwohl `TESTING.md` es unter den
  ausführbaren Tests aufführt. Nachgetragen in `tests/utils/CMakeLists.txt`.

## [1.6.0] - 2026-08-03

### Added

- Neue Option "Beim Minimieren in den Infobereich (Tray) legen" (Einstellungen
  → &Tray..., `TraySettingsForm`, Standard: aus): ist sie aktiviert, wird das
  Hauptfenster beim Minimieren nicht mehr in die Taskleiste, sondern
  vollständig versteckt und über ein Symbol im Infobereich wieder erreichbar
  (einfacher Klick auf das Symbol oder "Anzeigen" im Kontextmenü stellt das
  Fenster wieder her). Steht auf dem System kein Infobereich zur Verfügung,
  bleibt das bisherige Minimieren-Verhalten unverändert. Siehe
  `docs/architecture/ARCHITECTURE.md`, "Minimieren wahlweise in Taskleiste
  oder Tray".
- Neues, mehrstufiges Anwendungs-Icon (`IconProvider::appIcon()`,
  16/32/48/256px) — verwendet für das Tray-Icon selbst sowie neu für
  Fenster-/Taskleisten-Titel (`QApplication::setWindowIcon()` in
  `main.cpp`), die zuvor nur das generische Qt-Standardsymbol zeigten.
- Die Anwendung lässt sich nur noch einmal gleichzeitig starten
  (`SingleInstanceGuard`, `app/core/`): ein zweiter Startversuch holt die
  bereits laufende Instanz in den Vordergrund (auch aus dem Tray) und zeigt
  in der zweiten, sich sofort wieder beendenden Instanz einen kurzen
  Hinweis. Siehe `docs/architecture/ARCHITECTURE.md`, "Die Anwendung darf
  nur einmal gestartet werden".

## [1.5.0] - 2026-08-02

### Added

- Tooltip beim Hovern über die "Vortag"-Spalte und die davorliegende
  Entwicklungs-Pfeil-Icon-Spalte zeigt die Gesamtänderung der Position
  (Anteile × Kurswert-Entw.) statt der reinen Pro-Aktie-Kursänderung, inkl.
  Rechenweg und unabhängig eingefärbtem Pro-Stück-/Gesamtwert. Der Footer
  zeigt zusätzlich die Gesamtänderung des kompletten Portfolios. Siehe
  `docs/architecture/ARCHITECTURE.md`, "Vortag-Spalte + Piktogramm-Spalte:
  Tooltip mit Gesamtänderung".

## [1.4.2] - 2026-08-02

### Fixed

- Im Aktien-Chart (ShareDetailsForm-Tab und Rechtsklick-`ChartPopup`) zeigte
  der Hover-Tooltip der Kauf-/Verkauf-Markerlinien sowie der beiden
  Stück-Serien ("Anteile"/"Gehandelte Anteile") die Stückzahl auf ganze
  Stück gerundet an, obwohl Käufe/Verkäufe bis zu 4 Nachkommastellen haben
  können. `ViewChart::onReferenceLineHovered()` und `ViewChart::
  onSeriesHovered()` formatieren die Stückzahl jetzt mit 4 statt 0
  Nachkommastellen, konsistent zur restlichen Anwendung. Siehe
  `docs/architecture/ARCHITECTURE.md`, "ChartForm-Details".

## [1.4.1] - 2026-08-01

### Fixed

- Fenstertitel zeigte bei geöffnetem Portfolio zusätzlich dessen Dateinamen
  an ("Share Portfolio Manager - portfolio.db") — redundant zur bereits
  vorhandenen, live aktualisierten Anzeige des vollen Pfads unten rechts in
  der Statusleiste. `MainWindow::updateWindowTitle()` wurde entfernt; ihre
  Aufrufer aktualisieren jetzt nur noch die Statusleiste. Siehe
  `docs/architecture/ARCHITECTURE.md`, "Fenstertitel — Version statt
  Dateiname".

### Added

- Fenstertitel zeigt stattdessen die aktuelle Applikationsversion an
  ("Share Portfolio Manager (Version X.Y.Z)"), dynamisch über
  `QCoreApplication::applicationVersion()` — dieselbe Quelle, die
  `AboutForm` bereits verwendet. Siehe `docs/architecture/ARCHITECTURE.md`,
  "Fenstertitel — Version statt Dateiname".

## [1.4.0] - 2026-07-31

### Added

- Rahmenloses Popup-Fenster (`ChartPopup`) mit nur Überschrift (Aktienname +
  Zeitraum/Entwicklung) + Graph + Legende, portiert vom C#-Referenz-Popup
  `FrmChart`. Öffnet sich per einfachem Rechtsklick auf eine Zeile in einer
  der beiden Portfolio-Tabellen im Hauptfenster (Depotwert-/Marktwert-Tab)
  und schließt sich automatisch, sobald die Maus den Fensterbereich
  verlässt. Mausrad über dem Chart ändert weiterhin den dargestellten
  Zeitraum, da die bestehende `ViewChart`-Steuerung wiederverwendet wird
  (neuer Compact-Modus blendet lediglich die Selektion-/Zeitraum-
  Steuerelemente aus dem Layout aus, ohne ihre Funktionalität abzuschalten).
  Popup-Breite ist die Hauptfensterbreite minus 50px, horizontal zum
  Hauptfenster zentriert. Siehe `docs/architecture/ARCHITECTURE.md`,
  "ChartPopup — Rechtsklick-Popup-Chart".

## [1.3.0] - 2026-07-31

### Added

- Neue form-weite Warnzeile im ShareDetailsForm-Dialog ("Aktie sollte
  aktualisiert werden! Daten sind evtl. nicht auf dem aktuellen Stand."),
  unterhalb des Tab-Widgets — portiert von `toolStripStatusLabelUpdate` in
  der C#-Referenz (`ShareDetailsForm_Shown()`). Wird einmalig beim Öffnen
  des Dialogs geprüft: kein Warnhinweis, wenn für die Aktie ohnehin keine
  Tageswerte abgerufen werden (Update-Typ "Nur Kurs"/"Kein Update" —
  bewusste Einstellung), sonst Warnhinweis, wenn entweder gar keine
  Tageswerte vorhanden sind oder der neueste vorhandene Tageswert älter als
  der letzte Werktag ist. Siehe `docs/architecture/ARCHITECTURE.md`,
  "ShareDetailsForm-Details".

## [1.2.0] - 2026-07-30

### Added

- Legende im Aktien-Chart-Tab (ShareDetailsForm) um zwei Einträge "Ältere
  Käufe" (Türkis) und "Ältere Verkäufe" (Orange) erweitert — erscheinen
  sobald mindestens ein Kauf bzw. Verkauf im aktuell angezeigten Zeitraum
  liegt, der nicht der jeweils global letzte ist, z. B. weil der Nutzer die
  Zeitspanne vergrößert hat und dadurch ältere Käufe/Verkäufe erstmals im
  Graphen auftauchen. Dieselbe Farbe wie die bereits vorhandenen türkisen/
  orangen Markerlinien im Chart, portiert vom C#-Referenz-Verhalten. Siehe
  `docs/architecture/ARCHITECTURE.md`, "ChartForm-Details".

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

[1.4.2]: https://github.com/nessie1980/SharePortfolioManager-QT/compare/v1.4.1...v1.4.2
[1.4.1]: https://github.com/nessie1980/SharePortfolioManager-QT/compare/v1.4.0...v1.4.1
[1.4.0]: https://github.com/nessie1980/SharePortfolioManager-QT/compare/v1.3.0...v1.4.0
[1.3.0]: https://github.com/nessie1980/SharePortfolioManager-QT/compare/v1.2.0...v1.3.0
[1.2.0]: https://github.com/nessie1980/SharePortfolioManager-QT/compare/v1.1.0...v1.2.0
[1.1.0]: https://github.com/nessie1980/SharePortfolioManager-QT/compare/v1.0.2...v1.1.0
[1.0.2]: https://github.com/nessie1980/SharePortfolioManager-QT/compare/v1.0.1...v1.0.2
[1.0.1]: https://github.com/nessie1980/SharePortfolioManager-QT/compare/v1.0.0...v1.0.1

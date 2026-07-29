# Changelog

Alle nennenswerten Änderungen an der SharePortfolioManager-App werden hier
dokumentiert. Format angelehnt an [Keep a Changelog](https://keepachangelog.com/de/1.0.0/),
Versionierung nach [Semantic Versioning](https://semver.org/lang/de/).

Für die drei mitgelieferten Bibliotheken (jeweils eigene, unabhängige
Versionsnummer) siehe:
- [libs/logger/CHANGELOG.md](libs/logger/CHANGELOG.md)
- [libs/parser/CHANGELOG.md](libs/parser/CHANGELOG.md)
- [app/core/CHANGELOG.md](app/core/CHANGELOG.md) (Database)

## [Unreleased]

## [1.0.0] - 2026-07-28
### Hinzugefügt
- Erster funktionsvollständiger Stand der Anwendung: Verwaltung von
  Depotwerten/Marktwerten, Erfassung von Käufen, Verkäufen, Dividenden und
  Brokeragegebühren, automatischer Kursabruf (OnVista/Yahoo/Regex-Parser).
- ShareDetailsForm mit Jahres-Übersichten (Gewinne/Verluste, Dividenden,
  Kosten) und Aktien-Chart (ChartForm, Qt Charts) inkl. Kauf-/Verkaufs-
  Markerlinien.
- Direkte Dokumentenerfassung per Drag+Drop (PDF) mit automatischer
  Kauf-/Verkaufs-/Dividenden-Erkennung.
- Konfigurierbares Dokumenten-Root-Verzeichnis mit Migrationswerkzeug
  (DocumentRootMigrator) und Durchsetzung "nur Dokumente aus dem Root
  auswählbar".
- Linux-AppImage- und Windows-Installer-Packaging über GitHub Actions
  (manuell auslösbar).
- Deutschsprachige Oberfläche, SQLite-Persistenz, MVP-Architektur mit
  umfangreicher automatisierter Testabdeckung.

# Changelog — Database

Eigenständige Datenbank-Bibliothek (`app/core`, SQLite-Wrapper). Eigene
Versionsnummer, unabhängig von der SharePortfolioManager-App-Version — siehe
`app/core/CMakeLists.txt` (`project(Database VERSION x.y.z ...)`) und
ARCHITECTURE.md, Abschnitt "Versionierung".

Format angelehnt an [Keep a Changelog](https://keepachangelog.com/de/1.0.0/),
Versionierung nach [Semantic Versioning](https://semver.org/lang/de/).

## [1.3.0] - 2026-08-21

### Hinzugefügt

- Spalten `ex_date` und `depot_number` (beide `TEXT`, nullable) in `dividends`
  — Datengrundlage für die Plausibilitätsprüfung der Dividenden-Stückzahl,
  Phase 1 von fünf. Siehe CHANGELOG.md der Anwendung sowie
  `docs/architecture/ARCHITECTURE.md`, "Plausibilitätsprüfung der
  Dividenden-Stückzahl". Bestehende Portfolios bekommen beide Spalten über
  den bestehenden `ensureColumn()`-Migrationsschritt beim nächsten Öffnen,
  ohne Datenverlust — gleiches Vorgehen wie bei `share_splits.document`
  (`[1.2.0]`). Bewusst nullable statt `NOT NULL`: die "Muss"-Eigenschaft für
  neue/bearbeitete Dividenden ist eine Sache der Formularvalidierung in der
  Anwendung, nicht des Datenbankschemas — ein per Migration untergeschobener
  Platzhalterwert für bestehende Zeilen wäre falsche statt fehlender Angabe.

## [1.2.0] - 2026-08-08
### Hinzugefügt
- Schema-Migration für bestehende Portfolios: `migrateSchema()` läuft in
  `open()` unmittelbar nach `createSchema()` und ergänzt über
  `ensureColumn(table, column, definition)` fehlende Spalten in bereits
  vorhandenen Tabellen. Die Prüfung erfolgt über `PRAGMA table_info(<table>)`,
  ergänzt wird per `ALTER TABLE … ADD COLUMN`.

  Hintergrund: `createSchema()` arbeitet durchgehend mit
  `CREATE TABLE IF NOT EXISTS`. Eine komplett neue Tabelle kommt dadurch von
  selbst in bestehende Portfolios — so entstand `share_splits` beim ersten
  Öffnen nach Version 1.1.0. Eine neue Spalte in einer bereits vorhandenen
  Tabelle wird auf diesem Weg jedoch nicht nachgezogen: SQLite sieht die
  Tabelle, vergleicht die Spaltenliste nicht und tut nichts. Bis hierher fiel
  das nie auf, weil jede Tabelle ihre Spalten von Beginn an hatte.

  Bewusst ohne Versionszähler in der Datenbank: die Prüfung "existiert die
  Spalte?" ist idempotent, braucht keinen zusätzlichen Zustand und bleibt auch
  dann verlässlich, wenn ein Portfolio eine oder mehrere Versionen
  übersprungen hat.
- Spalte `document` in `share_splits` — ein Split trägt jetzt einen Beleg wie
  Käufe, Verkäufe, Dividenden und Kosten auch. Bestehende Portfolios bekommen
  die Spalte über den oben beschriebenen Migrationsschritt beim nächsten
  Öffnen, ohne Datenverlust. Siehe CHANGELOG.md der Anwendung, `[1.12.0]`.

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

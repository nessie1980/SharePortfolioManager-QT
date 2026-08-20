# Test-Dokumentation {#testing}

## Übersicht

Das Projekt verwendet **Qt Test** als Unit-Test-Framework. Tests werden als separate
Executables gebaut und über CMake's `ctest` oder Qt Creators Test-Panel ausgeführt.

---

## Test-Ausführung

### Qt Creator (empfohlen)
1. Tools → Tests → Test-Ergebnisse öffnen
2. Play-Button klicken — alle Tests laufen automatisch
3. Ergebnisse erscheinen farbig (grün ✅ / rot ❌)

### Terminal
```bash
cd build/Desktop_Qt_6_11_1-Debug
ctest --output-on-failure
```

### Einzelnen Test direkt starten
```bash
./bin/tst_logger
./bin/tst_parser
./bin/tst_buyrepository
./bin/tst_salerepository
./bin/tst_dividendrepository
./bin/tst_sharerepository
./bin/tst_brokeragerepository
./bin/tst_dailyvaluesrepository
./bin/tst_sharesplitrepository
./bin/tst_database
./bin/tst_appstartup
./bin/tst_iconprovider
./bin/tst_singleinstanceguard
./bin/tst_websitesconfig
./bin/tst_documentsconfig
./bin/tst_documentclassifier
./bin/tst_sharecalculator
./bin/tst_portfolioseriescalculator
./bin/tst_sharesplitadjuster
./bin/tst_salefifoallocator
./bin/tst_sharesplithint
./bin/tst_splitpricejumpdetector
./bin/tst_splitadjustmentaudit
./bin/tst_shareupdaterules
./bin/tst_mainwindow
./bin/tst_shareeditform
./bin/tst_sharesplitsform
./bin/tst_sharedetailsform
./bin/tst_chartform
./bin/tst_portfoliochartform
./bin/tst_overviewtabwidget
./bin/tst_buysform
./bin/tst_backupsettingsform
./bin/tst_traysettingsform
./bin/tst_documentssettingsform
./bin/tst_xmlportfolioparser
./bin/tst_portfoliovalidator
./bin/tst_portfolioimporter
```

@note Diese Liste wurde am 06.08.2026 vollständig gegen die neun
`add_subdirectory(tests/...)`-Aufrufe der Root-`CMakeLists.txt` und die
darin definierten `qt_add_executable()`-Ziele abgeglichen — in beide
Richtungen — und umfasste zu diesem Zeitpunkt alle 31 Testziele des
Projekts; seit `tst_sharesplitrepository`/`tst_sharesplitadjuster`
(07.08.2026, Phase 1 der Aktiensplit-Behandlung) waren es 33, seit
`tst_salefifoallocator` (07.08.2026, Phase 2c) waren es 34, seit
`tst_sharesplitsform` (08.08.2026, Phase 3a) waren es 35, seit
`tst_sharesplithint` (09.08.2026, Phase 3b) waren es 36, seit
`tst_splitpricejumpdetector` (13.08.2026, "Prüfen"-Knopf im Split-Dialog)
waren es 37, seit `tst_splitadjustmentaudit` (20.08.2026, Phase 4b —
automatische Nachprüfung nach Tageswert-Abruf) sind es 38. Anlass für den
ursprünglichen Abgleich war der Vorfall vom 05.08.2026, bei dem
`tst_sharecalculator` hier aufgeführt war, aber in keiner `CMakeLists.txt`
stand und deshalb nie gebaut wurde und nie mitlief. Wer ein Testziel
hinzufügt, trägt es bitte auch hier nach; ein Eintrag ohne zugehöriges Ziel
ist der gefährlichere der beiden Fehler, weil er Abdeckung vortäuscht, die
es nicht gibt.

---

## Implementierte Test-Module

### tests/logger/ — Logger Unit-Tests

Executable: `tst_logger`  
Klasse unter Test: `Logging::Logger`

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_init_success` | Erfolgreiche Initialisierung | `InitState::Initialized`, `LoggerState::Initialized` |
| `test_init_logging_disabled_when_levels_zero` | Beide Level-Masks = 0 → Logging deaktiviert | `LoggerState::LoggingDisabled` |
| `test_addEntry_success` | Eintrag hinzufügen, State/Component/Message prüfen | Ring-Buffer, Feldwerte |
| `test_addEntry_filtered_by_state_level` | Eintrag durch State-Filter herausgefiltert | Kein neuer Eintrag, kein Fehler |
| `test_ring_buffer` | Buffer-Größe wird eingehalten | Älteste Einträge werden entfernt |
| `test_entry_added_signal` | Qt-Signal wird bei neuem Eintrag emittiert | Signal-Count = 1 |
| `test_get_color_of_state_level` | Farbe für State-Level korrekt zurückgegeben | `QColor`-Vergleich |
| `test_file_logging` | Eintrag wird in Datei geschrieben | Datei existiert, Größe > 0 |

---

### tests/parser/ — Parser Unit-Tests

Executable: `tst_parser`

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_parsingValues_text_mode` | Konfiguration im Text-Modus | `LoadType::Text`, `isValid()` |
| `test_parsingValues_web_mode` | Konfiguration im Web-Modus | `LoadType::Web`, `isValid()` |
| `test_parsingValues_invalid_url` | Leere URL ist ungültig | `isValid()` = false |
| `test_regex_parsing_text_mode` | Regex auf direktem Text | Erster Treffer korrekt extrahiert |
| `test_regex_parsing_all_matches` | Alle Treffer sammeln | `foundPosition = -1` liefert alle Matches |
| `test_regex_no_match_result_empty_false` | Pflichtfeld fehlt | `ParserErrorCode::ParsingFailed` |
| `test_regex_no_match_result_empty_true` | Optionales Feld fehlt | `ParserErrorCode::Finished` |
| `test_start_fails_when_noRegexListGiven` | Guard: keine RegExList (bis 06.07.2026 fälschlich `test_start_fails_when_busy` genannt, siehe Hinweis unten) | `ParserErrorCode::NoRegexListGiven` |
| `test_reentrant_start_from_finished_signal_succeeds` | **Regressionstest Bugfix 05.07.2026 (Text-Modus):** reentranter `startParsing()`-Aufruf auf demselben Parser-Objekt aus dem `parserUpdated(Finished)`-Handler heraus (simuliert die Verkettung von "Alle aktualisieren" zur nächsten Aktie) | Zweiter `startParsing()`-Aufruf liefert `true` statt `BusyFailed` (-2); finaler Zustand `Finished` mit den Werten des zweiten Aufrufs; `isBusy() == false` danach |
| `test_onvista_realtime_json_parsing` | OnVista JSON Deserialisierung (direkt, ohne Parser) | Preis, Währung, Vortagskurs korrekt |
| `test_onvista_history_json_parsing` | OnVista Historie JSON (direkt, ohne Parser) | Anzahl Einträge, Eröffnungskurs korrekt |
| `test_yahoo_history_json_parsing` | Yahoo Finance Historie JSON (direkt, ohne Parser) | Timestamps, Schlusskurs korrekt |
| `test_webMode_regexParsing_viaFakeNetwork` | Web-Modus Ende-zu-Ende über `FakeNetworkAccessManager` (neu 07.07.2026) | `createRequest()` wird 1× aufgerufen, korrekte URL, `Finished`, Regex-Ergebnis korrekt |
| `test_webMode_onVistaRealTime_viaFakeNetwork` | Web-Modus, `ParsingType::OnVistaRealTime` über Fake-Netzwerk | `Finished`, `searchResult["Currency"]`/`["Price"]` korrekt |
| `test_webMode_yahooHistory_viaFakeNetwork` | Web-Modus, `ParsingType::YahooHistoryData` über Fake-Netzwerk | `Finished`, `dailyValuesList` korrekt befüllt |
| `test_webMode_networkError_viaFakeNetwork` | Simulierter Netzwerkfehler (`FakeNetworkAccessManager::setError()`) | `ParserErrorCode::NetworkError`, `isBusy() == false` danach |
| `test_start_fails_when_busy_viaFakeNetwork` | **Echter Busy-Guard-Test (neu 07.07.2026):** zweiter `startParsing()`-Aufruf während ein Fake-Download noch aussteht | Zweiter Aufruf liefert `false` mit `ParserErrorCode::BusyFailed` |
| `test_reentrant_start_from_finished_signal_succeeds_viaFakeNetwork` | Wie `test_reentrant_start_from_finished_signal_succeeds`, aber über den echten Web-Modus-Codepfad (`createRequest()` → `onDownloadFinished()` → `doRegexParsing()` → `finish()`) statt der Text-Modus-Abkürzung | Wie oben, zusätzlich `fakeNam.requestCount() == 2` |

@note Umbenennung `test_start_fails_when_busy` → `test_start_fails_when_noRegexListGiven`
(07.07.2026): Der ursprüngliche Test prüfte trotz seines Namens nie echtes
Busy-Verhalten, sondern ausschließlich den `NoRegexListGiven`-Guard — der
Kommentar im Code ("Can't easily test without a real network — just verify
guard works...") war der ehrliche Grund dafür, aber Name und Testinhalt
liefen dauerhaft auseinander. Mit der Parser-Mocking-Infrastruktur (siehe
ARCHITECTURE.md, "Erledigt / Archiv") ist ein echter Busy-Test jetzt
möglich und wurde als `test_start_fails_when_busy_viaFakeNetwork` ergänzt.

Regressionstest `test_reentrant_start_from_finished_signal_succeeds`
(tst_parser.cpp): Deckt den Bugfix vom 05.07.2026 ab, bei dem
`Parser::finish()` `m_busy` erst *nach* dem synchronen Emit des
`Finished`-Zustands zurücksetzte. Bei "Alle aktualisieren" führte das dazu,
dass eine Aktie ohne Kurswert-Update (nur `ShareUpdateType::DailyValues`)
direkt aus dem `onDailyValuesUpdated()`-Callback heraus in
`startRefreshForShare()` für die nächste Aktie verkettete und
`m_parserDailyValues.startParsing()` auf dem noch als "busy" markierten
Parser-Objekt fehlschlug (`BusyFailed`, -2) — sichtbar als
`"Tageswerte: Fehler beim Abruf von ... (-2)"` direkt nach einer schnell
abgeschlossenen vorherigen Aktie. Der ursprüngliche Test ruft `startParsing()`
reentrant aus dem `parserUpdated`-Signal-Handler heraus auf (Text-Modus, daher
synchron und ohne Netzwerk-Mocking testbar); die Variante
`..._viaFakeNetwork` (07.07.2026) reproduziert dieselbe Verkettung zusätzlich
über den tatsächlichen Web-Modus-Codepfad, den MainWindow in Produktion nutzt.

### tests/parser/FakeNetworkAccessManager — Test-Utility (neu 07.07.2026)

Keine eigene Executable — `FakeNetworkAccessManager.h/.cpp` ist eine
`QNetworkAccessManager`-Subklasse, die über die neue
`Parser(QNetworkAccessManager*, QObject*)`-Konstruktor-Injection
(siehe ARCHITECTURE.md) in `tst_parser` eingebunden wird. Details zur
Funktionsweise (Ownership, `FakeNetworkReply`, `QTimer::singleShot(0, ...)`)
stehen als Doxygen-Kommentare direkt im Header. Gedacht zur Wiederverwendung
durch zukünftige `tst_mainwindow`-Refresh-Flow-Tests (siehe Abschnitt
"Refresh-Flow" weiter unten) — dafür ist noch die Aufnahme der Datei in
`tests/forms/CMakeLists.txt` nötig, was Teil der eigentlichen
Refresh-Flow-Test-Implementierung ist, nicht dieser Vorstufe.

---

### tests/repositories/ — Repository Unit-Tests

@note Alle Repository-Tests legen in `initTestCase()` einen Test-Share an.
`Database.cpp` wird **nicht** direkt eingebunden — nur gegen die `Database`-Library
gelinkt (verhindert MOC-Konflikte).

Die Repository-Tests decken `BuyRepository`, `SaleRepository`, `DividendRepository`,
`ShareRepository`, `BrokerageRepository`, `DailyValuesRepository` und `ShareSplitRepository`
ab — CRUD-Operationen, Filterung, Sortierung und Transaktionsverhalten je Repository.

@note **Bugfix: `brokerage` fehlte in `tst_salerepository::init()` (16.07.2026,
siehe ARCHITECTURE.md "SalesForm-Details"):** `test_updateBrokerageGuid` legt
einen `brokerage`-Datensatz an, der per Rückwärts-Link auf die `sales`-Zeile
des Tests verweist. Da `init()` `brokerage` nicht mit aufräumte, scheiterte
`DELETE FROM sales` in jedem folgenden Testlauf an der FK-Constraint, und die
Zeile blieb liegen — `test_totalVolume` summierte in der Folge 13,0 statt
8,0. Fix: `init()` löscht jetzt auch `brokerage`, vor `sales`.

**`ShareRepository::maxLastInternetUpdate()` (Feature 21.07.2026, `tst_sharerepository.cpp`):**
Vier neue Tests decken die Grundlage für das Portfolio-Label "Letzte
Aktualisierung" ab:

| Test | Prüft |
| ---- | ----- |
| `test_maxLastInternetUpdate_emptyPortfolio_returnsEmpty` | Leeres Portfolio → leerer String |
| `test_maxLastInternetUpdate_noShareEverUpdated_returnsEmpty` | Aktie ohne jemals gesetztes `last_internet_update` → leerer String (nicht `NULL`-als-Text o.ä.) |
| `test_maxLastInternetUpdate_returnsLatestAcrossShares` | Zwei Aktien mit unterschiedlichen Zeitstempeln → jüngerer Wert wird geliefert (ISO-8601-String-Vergleich) |
| `test_maxLastInternetUpdate_ignoresSharesNeverUpdated` | Eine aktualisierte + eine nie aktualisierte Aktie → Ergebnis ist der Wert der aktualisierten Aktie, nicht leer |

Regressionstest `test_totalPayoutWithTaxes_matchesDoubleRoundedDividendObjectSum`
(tst_dividendrepository.cpp): Verifiziert, dass `DividendRepository::totalPayoutWithTaxes()`
bei Fremdwährungs-Dividenden dieselbe zweistufige Rundung anwendet wie
`DividendObject::calculateValues()`. Nutzt die realen Wechselkurse (1,07907 / 1,10526) aus
dem Fall, der die 0,02€-Differenz zwischen Dividenden-Tab-Summe und Depotwert-Tab am
02.07.2026 aufdeckte.

`DailyValuesRepository::UpsertStats`-Tests (tst_dailyvaluesrepository.cpp, ab 05.07.2026):
Verifizieren das Change-Tracking von `upsertList()`, das seit dem 05.07.2026 bei jedem
Refresh unnötig wiederholte "Tageswerte aktualisiert"-Meldungen mit identischen Werten
auflösen soll (Hintergrund: `buildDailyValuesUrl()` fragt bei jedem Refresh stets ein
ganzes Zeitfenster ab, nicht nur neue Tage — die Meldung soll daher zwischen tatsächlich
neuen/geänderten und unveränderten Zeilen unterscheiden):

- `test_upsertList_stats_allInserted` — leere DB, alle Zeilen landen in `inserted`.
- `test_upsertList_stats_updatedAndUnchanged` — Mischfall: ein bereits vorhandener Eintrag
  bleibt unverändert (`unchanged`), einer wird mit geänderten Werten überschrieben
  (`updated`), einer ist neu (`inserted`); prüft zusätzlich, dass der unveränderte Eintrag
  in der DB tatsächlich unangetastet bleibt und der geänderte die neuen Werte trägt.
- `test_upsertList_stats_toleratesFloatingPointNoise` — Differenz von `1e-10` im Kurswert
  (reines Fließkomma-Rauschen) muss als `unchanged` gewertet werden.
- `test_upsertList_stats_detectsFifthDecimalChange` — echte Änderung in der 5. Nachkommastelle
  (Auflösung der Kursdaten-APIs) muss zuverlässig als `updated` erkannt werden, darf also
  nicht von der Toleranz (`kValueEpsilon = 1e-9`) verschluckt werden.
- `test_upsertList_backwardCompatible_withoutStats` — Aufruf ohne `stats`-Parameter verhält
  sich unverändert wie vor der Erweiterung.

`test_earliestDate` (tst_dailyvaluesrepository.cpp, ergänzt 12.07.2026):
Gegenstück zu `test_latestDate` — `DailyValuesRepository::earliestDate()` (`MIN(date)`)
liefert bei leerer Tabelle eine ungültige `QDate`, sonst das älteste Datum unabhängig von
der Einfügereihenfolge. Grundlage für die Anzahl-Kappung im Chart-Tab, siehe
`tst_chartform.cpp` unten und ARCHITECTURE.md, "ChartForm-Details".

`ShareSplitRepository`-Tests (tst_sharesplitrepository.cpp, neu 07.08.2026, Phase 1
der Aktiensplit-Behandlung, siehe ARCHITECTURE.md "Offene Punkte"): CRUD plus die
beiden Besonderheiten des Schemas — eigene GUID je Zeile (anders als bei
`DailyValuesRepository`) und `UNIQUE(share_guid, date)`.

| Test | Prüft |
| ---- | ----- |
| `test_insert_andFindByShare_returnsSplit` | Grundfall: einfügen, laden, alle Felder korrekt |
| `test_findByShare_orderedByDateAscending` | Zwei Splits → aufsteigend nach Datum |
| `test_findByShare_noSplits_returnsEmpty` | Aktie ohne Splits → leere Liste |
| `test_findByGuid_found` / `test_findByGuid_notFound_returnsInvalid` | Einzelabruf per GUID |
| `test_existsForDate_true` / `test_existsForDate_false` | Duplikat-Prüfung der Erfassungsmaske (Phase 3a) |
| `test_insert_duplicateDate_fails` | `UNIQUE(share_guid, date)` — zweiter Split am selben Tag scheitert |
| `test_update_changesFields` | Datum, `prices_adjusted` und Kommentar änderbar |
| `test_remove_deletesSplit` / `test_removeByShare_deletesAllSplitsOfShare` | Löschpfade |
| `test_deletingShare_cascadesToSplits` | `ON DELETE CASCADE` über `share_guid` — läuft bewusst als letzter Test, da er die Test-Aktie löscht und danach neu anlegt |

Dokumentspalte (ergänzt 08.08.2026 mit Phase 3a — ein Split trägt jetzt einen
Beleg wie Kauf, Verkauf, Dividende und Kosten auch):

| Test | Prüft |
| ---- | ----- |
| `test_insert_storesDocumentPath` | Pfad wird gespeichert und kommt unverändert zurück |
| `test_insert_withoutDocument_returnsEmptyString` | Kein Beleg → leerer String, kein NULL-Artefakt |
| `test_update_changesDocumentPath` | Pfad über `update()` änderbar |
| `test_updateDocument_changesOnlyDocument` | Nur der Pfad wird angefasst — Verhältnis, Kommentar und `prices_adjusted` bleiben stehen |

@note `documentExists()` wird hier bewusst NICHT geprüft — die Abfrage sitzt in
`ModelShareSplitEdit`, nicht im Repository (dieselbe Platzierung wie bei
`ModelBuyEdit`/`ModelSaleEdit`/`ModelDividendEdit`/`ModelBrokerageEdit`). Die
zugehörigen Tests stehen in `tst_sharesplitsform.cpp`.

@note `test_updateDocument_changesOnlyDocument` ist der Regressionstest für den
Aufruf aus `DocumentRootMigrator`: würde dort versehentlich `update()` statt
`updateDocument()` verwendet, überschriebe ein Root-Wechsel die fachlichen
Felder mit dem, was gerade im Speicher liegt.

---

### tests/database/ — Database Unit-Tests

Tabellen-Existenz, Indizes, Foreign Keys, Default-Werte, WAL-Modus und Transaktionen.

Schema-Migration (ergänzt 08.08.2026, siehe ARCHITECTURE.md,
"Schema-Migration bestehender Portfolios"):

| Test | Prüft |
| ---- | ----- |
| `test_share_splits_table_exists` | Tabelle wird von `createSchema()` angelegt |
| `test_share_splits_has_document_column` | Spalte `document` ist nach `open()` vorhanden |
| `test_migration_addsMissingDocumentColumn` | Alte Tabelle ohne `document` → Spalte wird beim nächsten `open()` nachgezogen, bestehende Zeilen bleiben erhalten |
| `test_migration_isIdempotent` | Zweites und drittes `open()` legen die Spalte nicht erneut an |

@note Die beiden Migrationstests arbeiten als einzige in dieser Datei mit einer
DATEI-Datenbank in einem `QTemporaryDir`, nicht mit `:memory:`. Der Grund ist
zwingend: beim Schliessen einer In-Memory-Datenbank verschwindet ihr gesamter
Inhalt. Der von Hand hergestellte Altzustand (Tabelle ohne `document`) wäre
beim erneuten Öffnen also gar nicht mehr da, und der Test bewiese nichts — er
liefe grün, egal ob `migrateSchema()` existiert oder nicht. Beide Tests stellen
am Ende mit `open(":memory:")` den Ausgangszustand für `cleanupTestCase()`
wieder her.

@note `test_migration_isIdempotent` deckt den Fall ab, der in der Praxis am
häufigsten eintritt: jedes weitere Öffnen eines bereits migrierten Portfolios.
Ein zweites `ALTER TABLE` mit demselben Spaltennamen wäre ein SQL-Fehler und
liesse `open()` scheitern — die Anwendung würde also ab dem zweiten Start nicht
mehr hochkommen.

---

### tests/app/ — AppStartup + IconProvider + SingleInstanceGuard Unit-Tests

Startverhalten der Applikation (fehlende DB, leerer Pfad, erstes Öffnen), Icon-Verfügbarkeit
aller definierten `IconProvider::IconName`-Werte, sowie die Bezeichner-Logik der
Single-Instance-Sperre — `tst_appstartup`, `tst_iconprovider` und `tst_singleinstanceguard`.

@note `AppStartup::settingsPath()` liefert seit dem Bugfix vom 29.07.2026
(siehe ARCHITECTURE.md, "settings.ini nicht persistent im AppImage") einen
Pfad unter `QStandardPaths::AppConfigLocation` statt neben der Executable —
letzteres brach unter einem Linux-AppImage, dessen FUSE-Mountpunkt bei jedem
Start ein neues, zufälliges Verzeichnis ist. Der bisherige Test
`test_settingsPath_containsAppDir` (prüfte `startsWith(applicationDirPath())`)
wurde durch `test_settingsPath_isInStandardConfigLocation` ersetzt (prüft
`startsWith(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))`).
Neu hinzugekommen: `test_settingsPath_directoryIsCreated`, da `settingsPath()`
jetzt aktiv `QDir().mkpath(...)` auf das Zielverzeichnis aufruft — bei einem
brandneuen Config-Verzeichnis existiert es sonst noch nicht, und `QSettings`
legt darin ohne existierendes Elternverzeichnis keine Datei an.

@note `AppStartup::openDatabase()` hat seit 19.07.2026 einen zweiten Parameter
`showErrorDialog = true` — bei `false` wird bei einem Öffnungsfehler nur
`qCritical()` geloggt statt eines blockierenden `QMessageBox::critical()`.
`test_openDatabase_invalidPath_returnsFalse` ruft explizit mit `false` auf,
da dieser Test bewusst den Fehlerfall (Pfad zeigt auf ein Verzeichnis statt
eine Datei) auslöst und sonst am Dialog hängen bliebe. Alle anderen
`openDatabase()`-Tests lösen keinen Fehler aus und sind vom Parameter nicht
betroffen. Produktivaufruf in `main.cpp` bleibt unverändert (`showErrorDialog`
defaultet auf `true`).

@note `AppStartup::loadSettings(path)` (neu, Bugfix 24.07.2026 — siehe
ARCHITECTURE.md, "Erstlauf ohne settings.ini") ersetzt den bis dahin direkten
`AppSettings::instance().load(...)`-Aufruf in `main()`. Persistiert die
In-Memory-Defaults sofort per `AppSettings::save()`, falls die Datei am
übergebenen Pfad vor dem Laden noch nicht existierte, und gibt zurück, ob sie
vorher existierte. `test_loadSettings_missingFile_createsFileWithDefaults`
prüft die Neuanlage in einem `QTemporaryDir`-Sandbox-Pfad (nie ein echter
Installations- oder Testbinary-Pfad). `test_loadSettings_existingFile_
returnsTrueAndPreservesValues` ruft `loadSettings()` zweimal auf denselben
Pfad auf und ändert dazwischen `language` — der zweite Aufruf darf die schon
vorhandene Datei nicht mit frischen Defaults überschreiben, was der Test über
den erhalten gebliebenen Wert verifiziert.

@note `SingleInstanceGuard::buildServerName()` (neu, Feature 03.08.2026, "Die
Anwendung darf nur einmal gestartet werden") ist reine, seiteneffektfreie
String-Logik ohne Datei-/Socket-I/O — deswegen `public static` und direkt
testbar, gleiches Muster wie `MainWindow::buildDailyValuesUrl()`.
`tryAcquire()`/`activationRequested()` selbst (echtes `QLockFile` +
`QLocalServer`/`QLocalSocket` über mehrere Prozesse) bleiben bewusst
ungetestet — kein sauberer Weg, zwei echte, unabhängige Prozessinstanzen
deterministisch in einem einzelnen QTest-Lauf zu simulieren. Siehe
ARCHITECTURE.md für Details.

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_buildServerName_containsOrgAndAppName` | Org- und App-Name im Ergebnis | Beide Teilstrings sowie "SingleInstance" enthalten |
| `test_buildServerName_differentAppNames_produceDifferentNames` | Zwei unterschiedliche App-Namen | Ergebnisse unterscheiden sich |
| `test_buildServerName_sameInputs_areDeterministic` | Zweimaliger Aufruf mit identischen Argumenten | Identisches Ergebnis |
| `test_buildServerName_containsNoSpaces` | Org-/App-Name mit Leerzeichen | Leerzeichen im Ergebnis durch `_` ersetzt |

---

### tests/config/ — Konfiguration Unit-Tests

Laden und Parsen von `WebSites.xml` und `Documents.xml` — `tst_websitesconfig` und `tst_documentsconfig`.

---

### tests/utils/ — Utility Unit-Tests (neu 27.07.2026)

#### tst_documentclassifier — DocumentClassifier

Executable: `tst_documentclassifier`
Klasse unter Test: `DocumentClassifier` (app/utils/)

Reine Logik-Tests, kein GUI, kein `pdftotext`, keine Datenbank — baut sich
je Testfall ein kleines `Documents.xml`-Fixture per `QTemporaryDir`, exakt
nach demselben Muster wie `tst_documentsconfig` (`writeXml()`-Helper).

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_classify_buyDocument_matched` | BankIdentifier + BuyIdentifier treffen | `matched = true`, `type = Buy` |
| `test_classify_saleDocument_matched` | BankIdentifier + SaleIdentifier treffen | `matched = true`, `type = Sale` |
| `test_classify_dividendDocument_matched` | BankIdentifier + DividendIdentifier treffen | `matched = true`, `type = Dividend` |
| `test_classify_unknownBank_notMatched` | Kein BankIdentifier trifft | `matched = false` |
| `test_classify_knownBank_unknownType_notMatched` | Bank erkannt, aber kein Dokumenttyp-Identifier trifft | `matched = false` — bewusst kein Fallback (anders als in den vier Presentern) |
| `test_classify_emptyConfig_notMatched` | `DocumentsConfig` nie geladen | `matched = false`, kein Absturz |
| `test_extractWkn_found` / `test_extractIsin_found` | Regel mit Capture-Gruppe im Text vorhanden | Getrimmter Wert der ersten Capture-Gruppe |
| `test_extractWkn_notPresentInDocType_returnsEmpty` | Dokumenttyp (hier: Sale) hat keine `Wkn`-Regel | Leerer String, kein Absturz |
| `test_extractFieldValue_noMatch_returnsEmpty` | Regel vorhanden, aber Text enthält keinen Treffer | Leerer String |
| `test_matchBankIndex_found` / `test_matchBankIndex_notFound_leavesIndexUnchanged` | Bank-Erkennung isoliert (ohne Typ-Erkennung) | Index gesetzt bzw. unverändert |
| `test_detectDocumentType_matches_buyIdentifier` | Identifier-Treffer gewinnt gegen einen absichtlich "falschen" Fallback | Erkannter Typ, nicht der Fallback |
| `test_detectDocumentType_noIdentifierMatch_returnsFallback` | Bank erkannt, kein Identifier trifft | Übergebener Fallback-Typ (spiegelt z. B. `PresenterSaleEdit`s Default auf `DocumentType::Sale`) |

@note **`matchBankIndex()`/`detectDocumentType()` (ergänzt 27.07.2026):**
Diese beiden Bausteine wurden zusätzlich zu `classify()` eingeführt, damit
`PresenterBuyEdit`/`PresenterSaleEdit`/`PresenterDividendEdit`/
`PresenterShareAdd` nach ihrem Refactoring (s. ARCHITECTURE.md, "Schritt 2")
weiterhin ihren jeweils eigenen Dokumenttyp-Fallback verwenden können, wenn
die Bank erkannt wurde, aber kein Identifier eindeutig trifft — anders als
`classify()`, das für die Direkterfassung bewusst nie rät.

@note **Kein Test für `PdfTextExtractor`:** Die Klasse kapselt nur den
`QProcess`-Aufruf von `pdftotext` — konsistent mit der bestehenden
Projektkonvention, `QProcess`-getriebene `pdftotext`-Codepfade nicht direkt
zu automatisieren (siehe z. B. die `onBrowseDocument()`-Methoden der fünf
Editier-Dialoge, ebenfalls ungetestet aus demselben Grund). Ein Test müsste
entweder einen echten `pdftotext`-Aufruf gegen eine mitgelieferte Test-PDF
voraussetzen (Umgebungsabhängigkeit in CI) oder `QProcess` selbst mocken,
was für einen so simplen Wrapper unverhältnismäßigen Aufwand bedeuten würde.

#### tst_shareupdaterules — ShareUpdateRules (neu 06.08.2026)

Executable: `tst_shareupdaterules`
Klasse unter Test: `ShareUpdateRules` (app/utils/ShareUpdateRules.h)

Der schlankeste Testlauf im Projekt: keine Datenbank, keine Widgets, kein
`QApplication` (`QTEST_APPLESS_MAIN`), im CMake-Ziel nur `Qt6::Test` und
`ShareObject.cpp` wegen der Enum-Definitionen. Genau darum liegt die Regel in
einem eigenen Modul statt in einem der beiden Presenter — sie wird an drei
Stellen gebraucht (`ViewShareEdit`, `PresenterShareEdit`, `MainWindow`) und
muss losgelöst von allen dreien prüfbar bleiben. Siehe ARCHITECTURE.md,
"Erledigt / Archiv", "Tageswert-Historie bei Bestand > 0 erzwingen".

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_requiresDailyValues_zeroVolume_false` | Bestand exakt 0 | `false` |
| `test_requiresDailyValues_positiveVolume_true` | Bestand 1,0 und 0,0001 | `true` in beiden Fällen |
| `test_requiresDailyValues_floatingPointNoise_false` | Bestand `1e-12` sowie exakt `kVolumeEpsilon` | `false` — Grenzfall `>`, nicht `>=` |
| `test_requiresDailyValues_justAboveEpsilon_true` | Bestand `kVolumeEpsilon * 10` | `true` |
| `test_requiresDailyValues_negativeVolume_false` | Negativer Bestand (mehr verkauft als gekauft) | `false` — darf keinesfalls als Bestand durchgehen |
| `test_updateTypeIncludesDailyValues_allFourTypes` | Alle vier `ShareUpdateType`-Werte | `Both`/`DailyValues` = true, `MarketPrice`/`None` = false |
| `test_isUpdateTypeAllowed_withHolding_onlyDailyVariants` | Bestand 12,5 gegen alle vier Typen | Nur `Both` und `DailyValues` zulässig |
| `test_isUpdateTypeAllowed_withoutHolding_everythingAllowed` | Bestand 0 gegen alle vier Typen | Alle vier zulässig — ohne Bestand kostet die fehlende Historie nichts |
| `test_isUpdateTypeAllowed_boundary_epsilonCountsAsNoHolding` | `None` bei `kVolumeEpsilon` bzw. dem Zehnfachen | Zulässig bzw. unzulässig |
| `test_sharesNeedingDailyValues_emptyList_returnsEmpty` | Leere Eingabeliste | Leeres Ergebnis |
| `test_sharesNeedingDailyValues_allCompliant_returnsEmpty` | Vier korrekte Aktien, darunter zwei verkaufte mit `None`/`MarketPrice` | Leeres Ergebnis |
| `test_sharesNeedingDailyValues_mixed_returnsOnlyOffenders` | Vier Aktien, zwei davon Verstöße | Genau 2 Treffer, Eingangsreihenfolge erhalten |
| `test_sharesNeedingDailyValues_keepsAllFields` | Ein Verstoß mit allen Feldern gesetzt | `guid`, `wkn`, `name`, `updateType`, `currentVolume` unverändert im Ergebnis |

@note Die Reihenfolge wird bewusst mitgeprüft: die Startmeldung im
`MainWindow` listet die Aktien in derselben Reihenfolge wie das Grid, weil
`sharesNeedingDailyValues()` die Eingangsreihenfolge von
`ShareRepository::findAll()` (nach Name sortiert) durchreicht.

---

### tests/forms/ — Forms Unit-Tests

#### tst_mainwindow — MainWindow + ShareAddForm + ShareEditForm + SalesForm + DividendForm + BrokeragesForm + OwnMessageBox + BackupProgressForm

Executable: `tst_mainwindow`  
Klassen unter Test: `MainWindow`, `Database`, `ModelShareAdd`, `PresenterShareAdd`,
`ViewShareAdd`, `ModelShareEdit`, `PresenterShareEdit`,
`ModelSaleEdit`, `PresenterSaleEdit`, `ViewSaleEdit`,
`ModelDividendEdit`, `PresenterDividendEdit`, `ViewDividendEdit`,
`ModelBrokerageEdit`, `PresenterBrokerageEdit`, `ViewBrokerageEdit`,
`OwnMessageBox`, `BackupWorker`, `BackupProgressDialog`

@note `ModelBuyEdit`/`PresenterBuyEdit`/`ViewBuyEdit` sind weiterhin als
Produktionsquellen Teil von `tst_mainwindow` (Compile-Abhängigkeit über
`ViewShareEdit`), werden dort aber nicht mehr getestet — siehe `tst_buysform`.
`ViewShareEdit` wurde in `tst_shareeditform` ausgelagert.

@note Stub-Pattern: Für Presenter-Tests werden `StubView*` und `StubModel*`
verwendet — leichtgewichtige Implementierungen der Interfaces ohne echte UI
oder Datenbank.

CMake-Hinweis: `tst_mainwindow` benötigt `AUTOMOC ON` sowie alle
`.cpp`-Quelldateien neuer Forms in `target_sources` und deren Verzeichnisse
in `target_include_directories`.

MainWindow:
| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_construction_windowTitleSet` | Fenstertitel nach Konstruktion gesetzt | Enthält "Share Portfolio Manager" |
| `test_construction_windowTitleContainsVersion` | Feature 01.08.2026: Fenstertitel zeigt App-Version | Titel matcht `\(Version \d+\.\d+\.\d+\)` |
| `test_construction_actionsDisabledAtStart` | Menüaktionen ohne Portfolio deaktiviert | `isEnabled()` = false |
| `test_clearPortfolioTables_removesAllRows` | 2 Datentabellen starten leer, 2 Footer behalten ihre 3 Summenzeilen | `emptyCount` = 2, `footerCount` = 2 |
| `test_finalValueTable_showsFinalFields` | Regression Depotwert-Anzeige: Tab zeigt die `…Final`-Felder (mit Brokerage), nicht die brokeragefreien Marktwerte | "Aktuelle Entwicklung" = `profitLossFinal` (-1009,90), "Einzahlung" = `purchaseValueFinal` (1009,90) statt 1000,00 |
| `test_finalValueTable_priceAndCostDividendBottomColorIsNeutral` | Regression Bugfix 03.07.2026: Unterzeile von "Kosten/Dividenden" und "Preis" (Depotwert) nutzt `neutral` statt `muted` | `BottomColor.alpha()` = `neutral.alpha()` für beide Zellen |
| `test_marketValueTable_priceBottomColorIsNeutral` | Regression Bugfix 03.07.2026: Unterzeile von "Preis" (Marktwert) nutzt `neutral` statt `muted` | `BottomColor.alpha()` = `neutral.alpha()` |
| `test_finalValueFooter_costDividendCell` | Depotwert-Footer: Kosten/Dividenden als 2-zeiliger Wert in der Mittelzeile | Zelle (Zeile 1, Spalte Kosten/Dividenden) `TwoLineRole::Top` = `totalBrokerage` (9,90), `Bottom` = `totalDividend` (0,00) |
| `test_updatePortfolioLabel_defaultValues` | Portfolio-Label existiert | `findChild<QLabel*>()` nicht null |
| `test_newPortfolio_databaseCreated` | Neue DB-Datei wird angelegt | `QFileInfo::exists()` = true |
| `test_newPortfolio_schemaCreated` | Schema-Tabellen vorhanden nach Anlage | Tabellen existieren in DB |
| `test_newPortfolio_closePreviousBeforeOpening` | Bestehende DB wird vor Neuanlage geschlossen | Kein Verbindungsleck |
| `test_newPortfolio_settingsPathUpdated` | Pfad in AppSettings gespeichert | `AppSettings::portfolioPath()` = neuer Pfad |
| `test_openPortfolio_existingDatabase_opens` | Bestehende DB wird geöffnet | Kein Fehler |
| `test_openPortfolio_sharesLoadedFromDatabase` | Shares werden aus DB geladen | `findAll().size()` = 2 |
| `test_openPortfolio_emptyDatabase_noShares` | Leere DB → keine Shares | `rowCount()` = 0 |
| `test_startup_missingPortfolioFile_showsWarning` | DB-Datei fehlt → Warnung | Text enthält "Portfolio nicht gefunden" |
| `test_startup_emptyPortfolioPath_showsHint` | Kein Pfad gesetzt → Hinweis | Statusmeldung nicht leer |
| `test_addStatusMessage_appearsInTextEdit` | Statusmeldung erscheint im TextEdit | Text vorhanden |
| `test_addStatusMessage_containsTimestamp` | Zeitstempel im Format HH:MM:SS | Regex `\d{2}:\d{2}:\d{2}` matcht |
| `test_addStatusMessage_startupMessagePresent` | Startmeldung nach Konstruktion vorhanden | TextEdit nicht leer |
| `test_disableAllControls_onConfigError` | Konfigurationsfehler → alle Controls deaktiviert | `isEnabled()` = false |
| `test_shareAddDialog_canBeConstructed` | ShareAdd-Dialog öffnet ohne Absturz | Kein Absturz |
| `test_loggerSettings_saveColors` | Logger-Farben werden gespeichert und geladen | Gespeicherter Wert = gelesener Wert |
| `test_loggerSettings_saveLevels` | Logger-Level werden gespeichert | Level korrekt geladen |
| `test_loggerSettings_saveComponents` | Logger-Komponenten werden gespeichert | Komponenten korrekt geladen |

@note Bugfix (24.07.2026, siehe ARCHITECTURE.md "Log-Meldungsfarben
theme-neutral"): `AppSettings::m_logColors`-Defaults wurden von
Dark-Theme-optimierten auf theme-neutrale Farben umgestellt (u. a. Start/Info
von `#e0e0e0` auf `#808080`), da sie auf hellem Theme (reproduzierbar im
Linux-AppImage mangels Platform-Theme-Plugin) praktisch unlesbar waren.
`test_loggerSettings_saveColors` bleibt unverändert gültig, da er nur einen
expliziten Farbwert rundtestet, nicht den Default. Kein neuer Test für die
konkreten Default-Hex-Werte selbst — Farben sind bewusst frei über den
Logger-Dialog konfigurierbar, ein hartes `QCOMPARE` auf exakte Hex-Strings
wäre bei der nächsten Farbanpassung nur Reibung ohne echten
Regressionsschutz. Verifikation erfolgte visuell (Screenshot vor/nach dem
Fix). `LoggerSettingsForm::k_colorNames` wurde um die neuen Hex-Werte
ergänzt, die alten bleiben zur Abwärtskompatibilität mit bereits
gespeicherten `settings.ini`-Dateien erhalten.

| `test_soundSettings_saveUpdateEnabled` | Sound-Einstellung (Update) gespeichert | `soundUpdateEnabled()` = true |
| `test_soundSettings_saveErrorEnabled` | Sound-Einstellung (Fehler) gespeichert | `soundErrorEnabled()` = true |
| `test_soundSettings_saveUpdateFile` | Sound-Datei (Update) gespeichert | Pfad korrekt geladen |
| `test_soundSettings_saveErrorFile` | Sound-Datei (Fehler) gespeichert | Pfad korrekt geladen |
| `test_soundSettings_scanFallback` | Kein Sound-Gerät → Fallback | Kein Absturz |
| `test_soundFile_missingDisablesSound` | Fehlende Sound-Datei deaktiviert Sound | Sound disabled |
| `test_onRefreshShare_success_playsUpdateSoundOnce_viaFakeNetwork` | Erfolgreicher Einzel-Refresh | `SoundCountingMainWindow::soundPlayCount` = 1 |
| `test_onRefreshShare_error_doesNotPlayUpdateSound_viaFakeNetwork` | Fehlgeschlagener Einzel-Refresh | `soundPlayCount` = 0 |
| `test_onRefreshAll_success_playsUpdateSoundExactlyOnce_viaFakeNetwork` | Erfolgreiches "Alle aktualisieren" (2 Aktien) | `soundPlayCount` = 1 (nicht pro Aktie) |
| `test_onRefreshAll_error_doesNotPlayUpdateSound_viaFakeNetwork` | "Alle aktualisieren" bricht mit Fehler ab | `soundPlayCount` = 0 |

@note **Sound bei erfolgreicher Aktualisierung (implementiert 21.07.2026):**
`MainWindow::playUpdateFinishedSound()` ist `private virtual`, damit die
Testklasse `SoundCountingMainWindow` (definiert direkt vor `TestMainWindow`
in `tst_mainwindow.cpp`) sie per `override` abfangen und Aufrufzeitpunkt/
-anzahl zählen kann, statt von echter `QSoundEffect`-Wiedergabe (benötigt
ein Audio-Gerät, das in CI/Testumgebungen ggf. fehlt) abhängig zu sein.
Siehe ARCHITECTURE.md, Abschnitt "Erledigt / Archiv".

@note **BackupSettingsForm (implementiert 08.07.2026):** eigene Fälle in
`tests/forms/tst_backupsettingsform.cpp` (siehe eigener Abschnitt weiter
unten), nicht in `tst_mainwindow.cpp` — analog `tst_buysform`/
`tst_shareeditform`. Regressionstests für `createBackup()` selbst (Rotation,
Präfix-Änderung, `mkpath()`, Enable/Disable) bleiben dagegen in
`TestBackupForm` (unten in dieser Datei), da `createBackup()` eine private
Methode von `MainWindow` ist und dessen volle Konstruktion braucht.

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_aboutForm_appVersionSet` | About-Dialog zeigt App-Version | Version-Label nicht leer |
| `test_aboutForm_pdfConverterDetected` | About-Dialog zeigt PDF-Converter-Status | Label nicht leer |
| `test_deleteShare_removesShareFromDatabase` | Share + Remove → DB leer | `findAll().size()` = 0 |
| `test_deleteShare_nonExistentGuid_returnsFalse` | Nicht-existente GUID → kein Absturz | DB bleibt leer |
| `test_deleteShare_actionDeleteDisabledAtStart` | Entfernen-Aktion ohne Selektion deaktiviert | `isEnabled()` = false |
| `test_deleteShare_actionDeleteEnabledAfterSelection` | Entfernen-Aktion nach Zeilenauswahl aktiv (Bugfix 21./22.07.2026: `seedDepotwertPortfolio()` + `findFinalTable(window, 1)` statt manuellem `ShareRepository::insert()` ohne Buy + `findChildren<QTableWidget*>().first()` — vorher fehlten Buy-Transaktion und `AppSettings::portfolioPath`, wodurch die Aktie nicht zuverlässig mit 1 Zeile erschien und der Test in `QSKIP` lief) | `isEnabled()` = true |
| `test_onPortfolioRowDoubleClicked_nullItem_doesNotCrash` | Doppelklick-Slot mit `item == nullptr` | Kein Absturz |
| `test_onPortfolioRowDoubleClicked_emptyGuid_doesNotCrash` | Zeile mit geleerter GUID (Qt::UserRole) | Kein Absturz, kein modaler Dialog |
| `test_shareDetailsDialog_validShare_constructsAndShowsCloseButtonText` | `ViewShareDetails` direkt konstruiert | `hasValidShare()` = true, Fenstertitel = Aktienname, Close-Button = "Schließen" |
| `test_onPortfolioRowRightClicked_noItemAtPos_doesNotCrash` | `customContextMenuRequested()` mit Position ohne Zeile (leere Datentabelle) | Kein Absturz, kein Popup |
| `test_onPortfolioRowRightClicked_emptyGuid_doesNotCrash` | Zeile mit geleerter GUID (Qt::UserRole), Rechtsklick-Signal genuinely emittiert (nicht per `invokeMethod` direkt auf den Slot, siehe TESTING.md-Detailabschnitt) | Kein Absturz, kein Popup |
| `test_chartPopup_validShare_constructsWithChartChild` | `ChartPopup` direkt konstruiert (kein `show()`/`showAt()`) | Enthält ein `ViewChart`-Kindwidget; dessen `selektionBox` ist `isHidden() == true` (Compact-Modus); `chartPopupHeader`-Label enthält den Aktiennamen |
| `test_onPortfolioRowRightClicked_validGuid_popupCenteredAndNarrowerThanMainWindow` (Bugfix 02.08.2026, siehe ARCHITECTURE.md) | Echter Rechtsklick auf gültige Zeile, `MainWindow` bildschirmgeometrie-bewusst positioniert/dimensioniert | `width() == window.width() - 50`; ist der Bildschirm breit genug für das Popup: horizontal zentriert; ist er das nicht (Popup breiter als verfügbarer Bildschirm, z. B. schmale Offscreen-CI-Umgebung): linksbündig an `avail.left()` geklemmt |
| `test_chartWheel_overCountSpinAndChartView_changesIntervalCountAndRefreshes` | Mausrad-Events (`QWheelEvent`) auf `countSpin` (ohne Fokus) und auf `chartView`-Viewport | Beide erhöhen/verringern `intervalCount()`; löst jeweils einen Refresh aus (siehe ARCHITECTURE.md, "ChartForm-Details") |
| `test_chartCheckboxes_heldAndTradedVolumeAreMutuallyExclusive` | `seriesCheckBox_HeldVolume` per `findChild()` angehakt (ergänzt 12.07.2026, siehe ARCHITECTURE.md "ChartForm-Details") | `seriesCheckBox_TradedVolume` wird `setDisabled(true)` und bekommt einen Tooltip; nach dem Abhaken wieder `isEnabled() == true` und Tooltip leer — Prüfung erfolgt symmetrisch in beide Richtungen |
| `test_resolveShareGuidForDocument_matchesByWkn` | WKN im Dokumenttext vorhanden, Aktie mit dieser WKN in DB | GUID der gefundenen Aktie |
| `test_resolveShareGuidForDocument_matchesByIsin` | Nur ISIN im Dokumenttext, Aktie mit dieser ISIN in DB | GUID der gefundenen Aktie |
| `test_resolveShareGuidForDocument_wknTakesPrecedenceOverIsin` | WKN und ISIN vorhanden, gehören zu unterschiedlichen Aktien | GUID der über WKN gefundenen Aktie |
| `test_resolveShareGuidForDocument_noMatch_returnsEmpty` | WKN/ISIN im Text, aber keine passende Aktie in DB | Leerer String |
| `test_resolveShareGuidForDocument_noWknIsinRuleInDocEntry_returnsEmpty` | `DocumentEntry` ohne Wkn-/Isin-Regel (z. B. Sale/Dividend, falls in `Documents.xml` nicht vorhanden) | Leerer String, kein Absturz |

@note **"Direkte Dokumentenerfassung" (Drag+Drop, Feature 27.07.2026) —
Testabdeckung:** `DocumentClassifier::classify()`/`extractWkn()`/
`extractIsin()` sind in `tst_documentclassifier` abgedeckt (siehe
`tests/utils/`). `MainWindow::resolveShareGuidForDocument()` ist seit
27.07.2026 `public static` (korrigiert von einer ursprünglich privaten,
fälschlich als "direkt testbar" dokumentierten Fassung — eine private
Methode ist von außen schlicht nicht erreichbar) und braucht nur
`ShareRepository` gegen eine echte Test-DB, kein `pdftotext` und keinen
`MainWindow`-Instanz — Tests siehe oben.

**Bewusst weiterhin ungetestet:**
- `MainWindow::handleDroppedDocument()` — jetzt `private slot` (analog
  `selectShareRow()`/`selectFirstShareRow()`, testbar per
  `QMetaObject::invokeMethod`), aber ein Test bräuchte einen echten
  `pdftotext`-Aufruf (kein Mock) — konsistent mit der bestehenden
  Projektkonvention, `pdftotext`-`QProcess`-Codepfade nicht direkt zu
  automatisieren.
- `MainWindow::eventFilter()` — würde einen echten Qt-Drag&Drop-Vorgang
  simulieren müssen (`QTest` bietet dafür keine einfache Unterstützung);
  die eigentliche Filter-Logik (Einzeldatei-, PDF-Only-Prüfung) ist bewusst
  simpel gehalten, um das Risiko unentdeckter Bugs dort gering zu halten.
- `MainWindow::openCaptureDialog()` — öffnet echte, modale `QDialog`s
  (`dlg.exec()`); wie der Rest der Codebase testet dieses Projekt keine
  echten `exec()`-Abläufe direkt (vgl. `onBrowseDocument()`-Methoden).

Anzufügender Test-Ausschnitt für `tst_mainwindow.cpp` (private slots-Sektion
der Testklasse; DB-Setup ggf. an das bereits vorhandene Muster in
`initTestCase()`/`init()` dieser Datei anpassen). Nutzt `QUuid::createUuid()`
direkt statt eines evtl. vorhandenen `newGuid()`-Helpers — falls die Datei
bereits einen solchen Helper hat, gerne dafür austauschen, rein kosmetisch:

```cpp
void test_resolveShareGuidForDocument_matchesByWkn()
{
    ShareRepository repo;
    const QString guid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    repo.insert(ShareObject(guid, "BASF11", "DE000BASF111", "BASF SE"));

    DocumentEntry entry;
    entry.regexList.insert("Wkn",
        ParserLib::RegExElement{ "WKN:\\s+([A-Z0-9]{6})", 0, false, {} });

    const QString text = "WKN: BASF11";
    QCOMPARE(MainWindow::resolveShareGuidForDocument(text, entry), guid);
}

void test_resolveShareGuidForDocument_matchesByIsin()
{
    ShareRepository repo;
    const QString guid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    repo.insert(ShareObject(guid, "BASF11", "DE000BASF111", "BASF SE"));

    // Bewusst nur eine Isin-Regel, keine Wkn-Regel — testet den
    // ISIN-only-Pfad (extractWkn() liefert dann "" zurück, kein Absturz).
    DocumentEntry entry;
    entry.regexList.insert("Isin",
        ParserLib::RegExElement{ "ISIN:\\s+([A-Z0-9]{12})", 0, false, {} });

    const QString text = "ISIN: DE000BASF111";
    QCOMPARE(MainWindow::resolveShareGuidForDocument(text, entry), guid);
}

void test_resolveShareGuidForDocument_wknTakesPrecedenceOverIsin()
{
    ShareRepository repo;
    const QString wknGuid  = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString isinGuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    repo.insert(ShareObject(wknGuid,  "BASF11", "DE000BASF111", "BASF SE"));
    repo.insert(ShareObject(isinGuid, "SIE111", "DE0007236101", "Siemens AG"));

    DocumentEntry entry;
    entry.regexList.insert("Wkn",
        ParserLib::RegExElement{ "WKN:\\s+([A-Z0-9]{6})", 0, false, {} });
    entry.regexList.insert("Isin",
        ParserLib::RegExElement{ "ISIN:\\s+([A-Z0-9]{12})", 0, false, {} });

    // WKN gehört zu BASF, ISIN (absichtlich widersprüchlich) zu Siemens —
    // die WKN muss gewinnen, resolveShareGuidForDocument() darf die ISIN
    // in diesem Fall gar nicht erst nachschlagen.
    const QString text = "WKN: BASF11\nISIN: DE0007236101";
    QCOMPARE(MainWindow::resolveShareGuidForDocument(text, entry), wknGuid);
}

void test_resolveShareGuidForDocument_noMatch_returnsEmpty()
{
    DocumentEntry entry;
    entry.regexList.insert("Wkn",
        ParserLib::RegExElement{ "WKN:\\s+([A-Z0-9]{6})", 0, false, {} });

    const QString text = "WKN: UNKNWN"; // keine Aktie mit dieser WKN in der DB
    QVERIFY(MainWindow::resolveShareGuidForDocument(text, entry).isEmpty());
}

void test_resolveShareGuidForDocument_noWknIsinRuleInDocEntry_returnsEmpty()
{
    DocumentEntry entry; // regexList bewusst leer — simuliert einen
                         // Sale-/Dividend-DocumentEntry ohne Wkn/Isin-Regel
    QVERIFY(MainWindow::resolveShareGuidForDocument("beliebiger Text", entry).isEmpty());
}
```

ModelShareAdd:
| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_modelShareAdd_saveShareWithBuy_success` | Share + Buy + Brokerage in Transaktion | Share per WKN findbar, Buy-Count = 1 |
| `test_modelShareAdd_saveShareWithBuy_rollsBackOnDuplicateWkn` | Doppelte WKN → Rollback | Nur 1 Share in DB |
| `test_modelShareAdd_wknExists_true` | Vorhandene / nicht vorhandene WKN | `wknExists()` = true / false |
| `test_modelShareAdd_isinExists_true` | Vorhandene / nicht vorhandene ISIN | `isinExists()` = true / false |

PresenterShareAdd (via StubView + StubModel):
| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_presenterShareAdd_onSave_success_closesView` | Gültige Eingaben → View wird geschlossen | `view.closed` = true |
| `test_presenterShareAdd_onSave_emptyWkn_showsError` | Leere WKN → Fehler | `view.lastError` nicht leer |
| `test_presenterShareAdd_onSave_emptyName_showsError` | Leerer Name → Fehler | `view.lastError` nicht leer |
| `test_presenterShareAdd_onSave_zeroVolume_showsError` | Anteile = 0 → Fehler | `view.lastError` nicht leer |
| `test_presenterShareAdd_onSave_zeroPrice_showsError` | Kurs = 0 → Fehler | `view.lastError` nicht leer |
| `test_presenterShareAdd_onSave_duplicateWkn_showsError` | Doppelte WKN → Fehler | `view.lastError` nicht leer |
| `test_presenterShareAdd_onSave_duplicateIsin_showsError` | Doppelte ISIN → Fehler | `view.lastError` nicht leer |
| `test_presenterShareAdd_onSave_modelError_showsError` | DB-Fehler → Fehler | `view.lastError` nicht leer |
| `test_presenterShareAdd_onSave_invalidDateTime_showsError` | Ungültiges Datum → Fehler | `view.lastError` nicht leer |

ViewShareAdd:
| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_viewShareAdd_initialValues` | Alle Felder starten leer / 0 | WKN/ISIN/Name leer, numerische Felder = "0,0000" / "0,00" |
| `test_viewShareAdd_setFieldOk_updatesLineEdit` | `setFieldOk("wkn", ...)` schreibt in QLineEdit | `wkn()` = gesetzter Wert |
| `test_viewShareAdd_setFieldOk_updatesSpinBox_volume` | `setFieldOk("volume", ...)` → SpinBox | `volume()` = gesetzter Wert |
| `test_viewShareAdd_setFieldOk_handlesGermanDecimal` | "245,60" → 245.60 (Komma→Punkt) | `price()` = 245.60 |
| `test_viewShareAdd_setFieldError_doesNotCrash` | Fehler-Icon auf gültigem + unbekanntem Feld | Kein Absturz |
| `test_viewShareAdd_shareType_defaultIsShare` | Standard-Aktientyp ist "Share" | `shareType()` = Share |
| `test_viewShareAdd_parsingType_defaultIsRegex` | Standard-Parsingtyp ist Regex | `parsingType()` = Regex |
| `test_viewShareAdd_buyDateTime_isValid` | Kaufdatum nach Konstruktion gültig | `buyDateTime()` nicht null/leer |
| `test_viewShareAdd_recalc_kurswert` | Kurswert = Anteile × Kurs via textChanged | `volume()` × `price()` = Kurswert |
| `test_viewShareAdd_recalc_gesGebuehren` | Ges. Gebühren = Provision + Courtage + Handelsplatz | Summe korrekt |
| `test_viewShareAdd_recalc_endbetrag` | Endbetrag = Kurswert + Ges. Gebühren − Rabatt | Korrekte Endformel |
| `test_viewShareAdd_marketApiKey_disabledForRegex` | API-Key-Feld deaktiviert im Regex-Modus | `isEnabled()` = false |
| `test_viewShareAdd_dailyApiKey_enabledForApiYahoo` | Tages-API-Key aktiv bei Yahoo-Modus | `isEnabled()` = true |
| `test_viewShareAdd_hasMissingRequiredFields_initiallyTrue` | Direkt nach Konstruktion fehlen Pflichtfelder | `hasMissingRequiredFields()` = true |
| `test_viewShareAdd_hasMissingRequiredFields_falseAfterAllOk` | Nach Setzen aller Pflichtfelder | `hasMissingRequiredFields()` = false |
| `test_viewShareAdd_onParseFinished_setsInfoOnUntouched` | Parse-Ergebnis befüllt unberührte Felder | Felder enthalten geparste Werte |
| `test_viewShareAdd_markMissingFieldsAsFailed_doesNotCrash` | `markMissingFieldsAsFailed()` auf leerem Form | Kein Absturz |
| `test_viewShareAdd_documentPreviewPanel_nonExistentFile_doesNotCrash` | `DocumentPreviewPanel::showDocument()` mit ungültigem Pfad, via `findChild()` | Kein Absturz |
| `test_viewShareAdd_docTypeIcon_existsAndInitiallyEmpty` | Neues Fallback-Icon `m_docTypeIcon`, via `findChild<QLabel*>("docTypeIcon")` | Widget existiert, `pixmap()` vor Dokumentauswahl null |

@note Anders als bei `ViewBuyEdit`/`ViewSaleEdit`/`ViewDividendEdit`/
`ViewBrokerageEdit` ist `openPdfPreview()` bei `ViewShareAdd` kein Teil von
`IViewShareAdd` (nie vom Presenter aufgerufen, siehe ARCHITECTURE.md,
"ViewShareAdd auf DocumentPreviewPanel umgestellt") und existiert seit der
Migration auf `DocumentPreviewPanel` (19.07.2026) gar nicht mehr auf
`ViewShareAdd` selbst. Der Test greift daher über
`dlg.findChild<DocumentPreviewPanel*>()` auf das eingebettete Panel zu und
ruft dessen öffentliches `showDocument()` direkt auf — testet denselben Pfad
wie `test_viewBrokerageEdit_openPdfPreview_nonExistentFile_doesNotCrash`,
ohne den privaten `onBrowseDocument()`-Slot (echter `QFileDialog`) anfassen
zu müssen.

@note **Dokument-Typ-Icon-Fallback ergänzt (20.07.2026):** Analog zu
`ViewBuyEdit`/`ViewSaleEdit`/`ViewDividendEdit`/`ViewBrokerageEdit` zeigt
`ViewShareAdd` jetzt ebenfalls ein Icon je Dateiendung neben dem
Dokumentpfad (`m_docTypeIcon`, siehe ARCHITECTURE.md, "Erledigt / Archiv")
— eine rein defensive Anzeige, aktuell ohne aktiven Auswahlweg, da
alle fünf Dialoge auf PDF-only reduziert sind. Die Icon-Auswahl selbst
sitzt in `onBrowseDocument()` und wird daher wie der übrige Inhalt dieser
Methode nicht direkt getestet (kein automatisiertes Auslösen des echten
`QFileDialog`, gleiche Einschränkung wie bei den anderen vier Dialogen,
siehe ARCHITECTURE.md, "Durchsetzung 'nur Dokumente aus dem Root
auswählbar'"). `test_viewShareAdd_docTypeIcon_existsAndInitiallyEmpty`
deckt daher nur ab, dass das Widget existiert (`findChild<QLabel*>
("docTypeIcon")`) und vor einer Dokumentauswahl korrekt leer ist —
konsistent mit der bewusst unveränderten Testlücke bei
`DocumentPreviewPanel` selbst.

ModelShareEdit (Datenbanktests):

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_modelShareEdit_loadShare_returnsValidShare` | Share per GUID geladen | `isValid()` = true, WKN/Name korrekt |
| `test_modelShareEdit_loadShare_notFound_returnsInvalid` | Unbekannte GUID | `isValid()` = false, `lastError()` nicht leer |
| `test_modelShareEdit_saveShare_success` | Geänderter Name wird persistiert | Neu geladener Share zeigt neuen Namen |
| `test_modelShareEdit_currentVolume_sumsBuyMinusSold` | Zwei Käufe, einer teilverkauft | Summe `volume − volumeSold` über alle Käufe korrekt |
| `test_modelShareEdit_currentVolume_noBuys_returnsZero` | Keine Käufe vorhanden | `currentVolume()` = 0.0 |
| `test_modelShareEdit_firstBuyDate_returnsEarliestBuyDate` | Zwei Käufe in unterschiedlichen Jahren | Rückgabe = `dateAsStr()` des ältesten Kaufs |
| `test_modelShareEdit_firstBuyDate_noBuys_returnsEmpty` | Keine Käufe vorhanden | `firstBuyDate()` leer |
| `test_modelShareEdit_totalBuyValue_delegatesToRepository` | Reine Delegations-Smoke-Test | Wert identisch zu `BuyRepository::totalBuyValueBrokerageReduction()` |

@note `currentVolume()` und `firstBuyDate()` enthalten eigene Aggregationslogik
(Summenbildung bzw. Auswahl des ältesten Eintrags) und sind daher trotz Delegation an
`BuyRepository` separat getestet. Die übrigen Aggregat-Methoden (`totalSaleValue`,
`totalProfitLoss`, `totalDividendValue`, `totalBrokerageValue`, …) sind reine
1:1-Weiterleitungen ohne eigene Logik und werden über die bereits bestehenden
Repository-Tests in `tests/repositories/` abgedeckt.

PresenterShareEdit (via StubView + StubModel):
| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_presenterShareEdit_loadsShareOnConstruction` | Share wird beim Öffnen geladen | `view.loadShareCalled` = true |
| `test_presenterShareEdit_populatesSummaryOnConstruction` | Aggregate werden befüllt | `setTotalBuys/Sales/...` aufgerufen |
| `test_presenterShareEdit_onSave_success_closesView` | Gültige Eingaben → View akzeptiert | `view.closed` = true |
| `test_presenterShareEdit_refreshSummary_callsPopulate` | `refreshSummary()` aktualisiert Aggregate | `setTotalBuys` erneut aufgerufen |
| `test_presenterShareEdit_onEditBuys_emitsSignal` | Pencil-Button Käufe → Signal | `openBuysRequested` emittiert |
| `test_presenterShareEdit_onEditSales_emitsSignal` | Pencil-Button Verkäufe → Signal | `openSalesRequested` emittiert |
| `test_presenterShareEdit_onEditDividends_emitsSignal` | Pencil-Button Dividenden → Signal | `openDividendsRequested` emittiert |
| `test_presenterShareEdit_onEditBrokerages_emitsSignal` | Pencil-Button Kosten → Signal | `openBrokeragesRequested` emittiert |
| `test_presenterShareEdit_withHolding_requiresDailyValues` | Bestand 12,5 | `setDailyValuesRequired(true)` an die View gereicht |
| `test_presenterShareEdit_withoutHolding_doesNotRequireDailyValues` | Bestand 0 | Aufruf findet statt, Argument `false` |
| `test_presenterShareEdit_onSave_changingToForbiddenType_showsErrorAndDoesNotSave` | Aktiv von "Beide" auf "Keine", Bestand 10,0 | View nicht geschlossen, `saveShare()` NICHT erreicht, Fehlermeldung gesetzt |
| `test_presenterShareEdit_onSave_marketPriceWithoutHolding_saves` | "Markt-Preis", Bestand 0 | Gespeichert und geschlossen — die Sperre greift nicht pauschal |
| `test_presenterShareEdit_onSave_dailyValuesWithHolding_saves` | "Tages-Werte", Bestand 10,0 | Gespeichert und geschlossen |
| `test_presenterShareEdit_onSave_unchangedLegacyType_stillSaves` | Gespeichert "Keine", unverändert gelassen, Bestand 10,0 | Gespeichert und geschlossen, keine Fehlermeldung |
| `test_presenterShareEdit_onSave_legacyTypeChangedToOtherForbidden_blocked` | Gespeichert "Keine", gewählt "Markt-Preis", Bestand 10,0 | Abgewiesen — die Ausnahme gilt nur für den unveränderten Wert |

Text der Start-Meldung (`MainWindow`, statische Helfer):

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_updateTypeLabel_allFourValues` | Alle vier `ShareUpdateType`-Werte | Beschriftungen wortgleich zu den Radios in `ViewShareEdit` |
| `test_buildDailyValuesWarningMessage_emptyList_returnsEmpty` | Leere Liste | Leerer String — belegt den Frühausstieg, ohne Verstösse geht kein Dialog auf |
| `test_buildDailyValuesWarningMessage_containsNameWknAndType` | Eine Aktie | Name, WKN und Update-Typ stehen im Text |
| `test_buildDailyValuesWarningMessage_listsAllSharesInOrder` | Zwei Aktien | Beide genannt, Reihenfolge des Grids erhalten |
| `test_buildDailyValuesWarningMessage_explainsConsequenceAndUrgency` | Eine Aktie | Text nennt "Depotwert-Chart" und "dauerhaft verloren" |
| `test_buildSplitAdjustmentWarningMessage_emptyList_returnsEmpty` | Leere Liste | Leerer String — belegt den Frühausstieg, ohne Widerspruch geht kein Dialog auf (Phase 4b, 20.08.2026) |
| `test_buildSplitAdjustmentWarningMessage_containsNameWknAndSplitDescription` | Ein Widerspruch | Aktienname, WKN und Split-Beschreibung (`ShareSplitHint::describeSplit()`) stehen im Text |
| `test_buildSplitAdjustmentWarningMessage_listsAllWarningsInOrder` | Zwei Widersprüche | Beide genannt, Reihenfolge der Eingabeliste erhalten |
| `test_buildSplitAdjustmentWarningMessage_explainsNoAutomaticChange` | Ein Widerspruch | Text stellt klar, dass nichts automatisch geändert wird, und verweist auf den "Prüfen"-Knopf |

@note Der letzte Test der ersten Gruppe wirkt zunächst wie eine Prüfung auf
Wortlaut, ist aber der eigentliche Zweck der Meldung: ohne die Folge
(Ausschluss aus dem Chart) und ohne die Dringlichkeit (rückwirkend nicht mehr
abrufbare Historie) wäre sie eine folgenlose Notiz, die der Nutzer wegklickt.
Fiele einer der beiden Teile bei einer späteren Textänderung heraus, bliebe
das sonst unbemerkt — eine Meldung erscheint ja weiterhin. Bei
`buildSplitAdjustmentWarningMessage()` gilt dieselbe Überlegung für
`test_buildSplitAdjustmentWarningMessage_explainsNoAutomaticChange`: die
Zusicherung "automatisch geändert wird hier nichts" ist der Kern von Phase
4b (siehe ARCHITECTURE.md, "Automatische Nachprüfung nach
Tageswert-Abruf") — ohne sie könnte der Nutzer die Meldung für eine bereits
erfolgte Korrektur halten.

@note Zwei Anpassungen an den Stubs waren dafür nötig (06.08.2026).
`StubViewShareEdit::updateType()` lieferte fest `ShareUpdateType::None` und
`StubModelShareEdit::currentVolume()` fest `10,0` — seit
`PresenterShareEdit::validateInput()` beides gegeneinander prüft, ist diese
Kombination unzulässig, und `test_presenterShareEdit_onSave_success_closesView`
wäre am Validierungsfehler gescheitert statt am eigentlichen Prüfgegenstand.
Beide Werte sind jetzt je Test setzbar, die Vorgabe des Update-Typs ist
`Both`. Zusätzlich zeichnet `StubModelShareEdit::saveShare()` über
`saveShareCalled` auf, ob es überhaupt erreicht wurde — nur so lässt sich
belegen, dass die Validierung wirklich vorher abbricht und nicht bloss
nachträglich eine Meldung anzeigt.

ViewShareEdit: Tests wurden in `tst_shareeditform` ausgelagert — siehe Abschnitt unten.

---

#### tst_buysform — BuysForm

Executable: `tst_buysform`
Klassen unter Test: `ModelBuyEdit`, `PresenterBuyEdit`, `ViewBuyEdit`

@note Zur Auslagerung: `tst_buysform` wurde aus `tst_mainwindow` herausgelöst,
nachdem die Testzahl der BuysForm groß genug geworden war, um eine eigene
Executable zu rechtfertigen. Die Produktionsklassen
`ModelBuyEdit`/`PresenterBuyEdit`/`ViewBuyEdit` bleiben weiterhin auch Teil der
`tst_mainwindow`-Quellen, da `ViewShareEdit` zur Compile-/Link-Zeit von
`ViewBuyEdit` abhängt (Pencil-Button "Käufe" öffnet `ViewBuyEdit` direkt) —
dort existieren dafür aber keine eigenen Tests mehr.

@note Stub-Pattern: `StubViewBuyEdit` und `StubModelBuyEdit` implementieren die
jeweiligen Interfaces ohne echte UI oder Datenbank — identisches Muster wie in
`tst_mainwindow`.

ModelBuyEdit:
| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_modelBuyEdit_documentExists_notFound_returnsFalse` | Pfad nicht in DB | `documentExists()` = false |
| `test_modelBuyEdit_documentExists_emptyPath_returnsFalse` | Leerer Pfad | `documentExists()` = false (Early Return) |
| `test_modelBuyEdit_addBuy_success` | Buy + Brokerage in Transaktion gespeichert | `loadBuys()` gibt 1 Eintrag zurück, `loadBrokerage()` gültig |
| `test_modelBuyEdit_addBuy_rollsBackOnError` | Fehler bei Brokerage-Insert → Rollback | Kein Buy in DB |
| `test_modelBuyEdit_updateBuy_success` | Buy + Brokerage aktualisiert | Aktualisierte Werte in DB |
| `test_modelBuyEdit_updateBuy_createsBrokerageIfMissing` (erweitert 20.07.2026) | Kein Brokerage vorhanden → wird erstellt | `loadBrokerage()` danach gültig; zusätzlich `BuyRepository::totalBuyValueBrokerageReduction()` enthält die Provision — Regression für den Brokerage-Vorwärts-Link-Bugfix (s.u.) |
| `test_modelBuyEdit_removeBuy_deletesBrokerageFirst` | Delete in richtiger Reihenfolge (FK) | Buy + Brokerage entfernt |
| `test_modelBuyEdit_removeBuy_rollsBackOnError` | `removeBuy` auf nicht-existenter GUID → kein Absturz (SQLite DELETE gibt bei 0 Treffern kein Fehler zurück) | Kein Absturz |
| `test_modelBuyEdit_orderNumberExists_true` | Vorhandene Ordernummer erkannt | `orderNumberExists()` = true |
| `test_modelBuyEdit_orderNumberExists_excludeGuid` | Eigene Ordernummer wird ausgeschlossen | `orderNumberExists()` = false beim Edit |
| `test_modelBuyEdit_loadBuys_orderedByDate` | Käufe nach Datum aufsteigend | Datums-Reihenfolge korrekt |
| `test_modelBuyEdit_loadBrokerage_notFound_returnsInvalid` | Kein Brokerage → ungültiges Objekt | `isValid()` = false |

`BuyRepository::updateBrokerageGuid()` selbst hat zusätzlich einen isolierten
Repository-Unit-Test, `test_updateBrokerageGuid` in
`tests/repositories/tst_buyrepository.cpp` (ergänzt 20.07.2026, analog zu
`SaleRepository::test_updateBrokerageGuid`): Brokerage wird absichtlich
zunächst nur über den Rückwärts-Link (`buy_guid`) angelegt —
`totalBuyValueBrokerageReduction()` liefert davor nur den reinen Kaufwert
(1000,0, ohne Provision), nach `updateBrokerageGuid()` korrekt 1007,5
(inkl. 7,5 Provision).

---

PresenterBuyEdit (via StubView + StubModel):
| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_presenterBuyEdit_construction_loadsOverview` | Konstruktor lädt Käufe und befüllt Übersicht | `view.populateOverviewCalled` = true |
| `test_presenterBuyEdit_construction_clearsForm` | Formular nach Konstruktion leer | `view.clearFormCalled` = true |
| `test_presenterBuyEdit_construction_setsButtonStates_noSelection` | Initialer Button-Zustand: keine Selektion | `canRemove=false`, `isLastBuy=false` |
| `test_presenterBuyEdit_onRowSelected_singleBuy_isLastBuy` | Einzelner Kauf ist immer der jüngste | `isLastBuy=true` |
| `test_presenterBuyEdit_onRowSelected_olderBuy_isNotLastBuy` | Älterer Kauf → `isLastBuy=false` | `canRemove=false` (da `isLastBuy=false`) |
| `test_presenterBuyEdit_onRowSelected_newerBuy_isLastBuy` | Jüngster Kauf → `isLastBuy=true` | `canRemove=true` wenn `volumeSold=0` |
| `test_presenterBuyEdit_onRowSelected_latestBuyWithSoldShares_canRemoveFalse` | Jüngster Kauf mit verkauften Anteilen | `canRemove=false` |
| `test_presenterBuyEdit_onRowSelected_latestBuyNoSoldShares_canRemoveTrue` | Jüngster Kauf ohne verkaufte Anteile | `canRemove=true` |
| `test_presenterBuyEdit_onRowSelected_emptyGuid_resetsForm` | Leere GUID → Reset | `view.clearFormCalled` = true |
| `test_presenterBuyEdit_onReset_setsButtonStates_noSelection` | Reset → Button-Zustand zurückgesetzt | `canRemove=false`, `isLastBuy=false` |
| `test_presenterBuyEdit_onReset_jumpsToOverviewTab` | Reset → Übersicht-Tab | `showOverviewTab()` aufgerufen |
| `test_presenterBuyEdit_onSave_newBuy_callsAddBuy` | Neuer Kauf → `addBuy()` | `model.addBuyCalled` = true |
| `test_presenterBuyEdit_onSave_newBuy_emitsDataChanged` | Neuer Kauf → Signal | `dataChanged` emittiert |
| `test_presenterBuyEdit_onSave_newBuy_jumpsToOverviewTab` | Neuer Kauf → Übersicht-Tab | `showOverviewTab()` aufgerufen |
| `test_presenterBuyEdit_onSave_latestBuy_callsUpdateBuy` | Jüngster Kauf edit → `updateBuy()` | `model.updateBuyCalled` = true |
| `test_presenterBuyEdit_onSave_latestBuy_jumpsToOverviewTab` | Jüngster Kauf edit → Übersicht-Tab | `showOverviewTab()` aufgerufen |
| `test_presenterBuyEdit_onSave_nonLatestBuy_callsUpdateBuyDocOnly` | Älterer Kauf → `updateBuy()` (nur Dokument) | `model.updateBuyCalled` = true, kein `addBuy` |
| `test_presenterBuyEdit_onSave_nonLatestBuy_emitsDataChanged` | Älterer Kauf gespeichert → Signal | `dataChanged` emittiert |
| `test_presenterBuyEdit_onSave_nonLatestBuy_jumpsToOverviewTab` | Älterer Kauf gespeichert → Übersicht-Tab | `showOverviewTab()` aufgerufen |
| `test_presenterBuyEdit_onSave_nonLatestBuy_resetsButtonLabel` | Älterer Kauf gespeichert → Button zurückgesetzt | `canRemove=false`, `isEdit=false` |
| `test_presenterBuyEdit_onSave_missingFields_showsError` | Pflichtfelder fehlen → Fehler | `view.lastError` nicht leer, kein `addBuy` |
| `test_presenterBuyEdit_onSave_documentDuplicate_showsError` | Dokument bereits vergeben → Fehler | `view.lastError` nicht leer, kein `addBuy` |
| `test_presenterBuyEdit_onRemove_latestBuyNoSoldShares_callsModel` | Löschen erlaubt → `removeBuy()` | `model.removeBuyCalled` = true |
| `test_presenterBuyEdit_onRemove_latestBuyNoSoldShares_emitsDataChanged` | Löschen → Signal | `dataChanged` emittiert |
| `test_presenterBuyEdit_onRemove_olderBuy_showsError` | Nicht-letzter Kauf → Fehler | `model.removeBuyCalled` = false |
| `test_presenterBuyEdit_onRemove_latestBuyWithSoldShares_showsError` | Verkaufte Anteile → Fehler | `model.removeBuyCalled` = false |
| `test_presenterBuyEdit_onRemove_noSelection_doesNothing` | Kein Buy ausgewählt → kein Aufruf | `model.removeBuyCalled` = false |
| `test_presenterBuyEdit_onOrderNumberEdited_empty_setsError` | Leere Ordernummer → Fehler-Icon | `setFieldError("orderNumber")` aufgerufen |
| `test_presenterBuyEdit_onOrderNumberEdited_valid_setsOk` | Gültige Ordernummer → Ok-Icon | `setFieldOk("orderNumber")` aufgerufen |
| `test_presenterBuyEdit_onOrderNumberEdited_duplicate_setsError` | Doppelte Ordernummer → Fehler-Icon | `setFieldError("orderNumber")` aufgerufen |
| `test_presenterBuyEdit_onDocumentPathEdited_duplicate_setsError` | Duplikat-Dokument → Fehler-Icon | `setFieldError("document")` aufgerufen |
| `test_presenterBuyEdit_onDocumentPathEdited_unique_setsOk` | Eindeutiges Dokument → Ok-Icon | `setFieldOk("document")` aufgerufen |
| `test_presenterBuyEdit_onDocumentSelected_newMode_doesNotEarlyReturn` | Neu-Modus → Vorschau wird geöffnet | `openPdfPreview()` aufgerufen |
| `test_presenterBuyEdit_onDocumentSelected_nonLatestBuy_earlyReturn` | Nicht-letzter Kauf → kein Parse | `setUiBusy(true)` nicht aufgerufen (pdftotext wird nicht gestartet) |

---

ViewBuyEdit:
| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_viewBuyEdit_canBeConstructed` | Dialog öffnet ohne Absturz | Fenstertitel enthält "Käufe" |
| `test_viewBuyEdit_initialValues` | Alle Felder starten mit Standardwerten | volume/price = "0,0000", provision/etc. = "0,00" |
| `test_viewBuyEdit_depotNumberCombo_populatedFromConfig` | ComboBox aus Documents.xml befüllt | `count()` > 1 (Placeholder + Einträge) |
| `test_viewBuyEdit_depotNumberCombo_itemDataIsIdentifier` | `itemData` enthält rohen Identifier | `itemData(1)` nicht leer |
| `test_viewBuyEdit_setFieldOk_date_parsesDotFormat` | `setFieldOk("date", "4.2.2026")` → QDateEdit | `date()` = 04.02.2026 |
| `test_viewBuyEdit_setFieldOk_date_parsesISOFormat` | `setFieldOk("date", "2026-02-04")` → QDateEdit | `date()` gültig |
| `test_viewBuyEdit_setFieldOk_time_parsesHMS` | `setFieldOk("time", "19:51:45")` → QTimeEdit | `time()` = 19:51:45 |
| `test_viewBuyEdit_setFieldOk_depotNumber_matchesByItemData` | `setFieldOk("depotNumber", identifier)` wählt korrekten Eintrag | `currentIndex()` > 0 |
| `test_viewBuyEdit_setFieldOk_volume_handlesGermanDecimal` | "15,00" → 15.0 | `volume()` = 15.0 |
| `test_viewBuyEdit_setFieldOk_doesNotOverwriteWithEmptyValue` | `setFieldOk("orderNumber", "")` → Widget-Text unverändert | Bestehender Wert bleibt erhalten |
| `test_viewBuyEdit_setFieldOk_writesValueWhenNonEmpty` | `setFieldOk("orderNumber", "ORD-456")` → Widget aktualisiert | `text()` = "ORD-456" |
| `test_viewBuyEdit_setFieldError_doesNotCrash` | Fehler-Icon auf gültigem + unbekanntem Feld | Kein Absturz |
| `test_viewBuyEdit_hasMissingRequiredFields_initiallyTrue` | Direkt nach Konstruktion fehlen Pflichtfelder | Liste enthält date, depotNumber, orderNumber, volume, price |
| `test_viewBuyEdit_hasMissingRequiredFields_depotNumber_checkedByItemData` | Placeholder hat kein itemData → fehlend | `hasMissingRequiredFields()` = true |
| `test_viewBuyEdit_hasMissingRequiredFields_falseAfterAllSet` | Nach Setzen aller Pflichtfelder | `hasMissingRequiredFields()` = false |
| `test_viewBuyEdit_markMissingFieldsAsFailed_doesNotCrash` | Auf leerem Formular | Kein Absturz |
| `test_viewBuyEdit_clearForm_resetsAllFields` | clearForm() → Standardwerte | Alle Felder auf 0/leer/heute |
| `test_viewBuyEdit_clearForm_resetsStatusIcons` | clearForm() → Icons geleert | `m_fieldStates` leer |
| `test_viewBuyEdit_clearForm_resetsParseStatusBar` | clearForm() → Statuszeile transparent | `m_parseProgress` value = 0 |
| `test_viewBuyEdit_clearForm_restoresEditableFields` | Nach `setButtonStates(true,false,true)` + clearForm() | Felder wieder enabled |
| `test_viewBuyEdit_setParseProgress_showsValues` | `setParseProgress(50, "...")` | Progress = 50, Text gesetzt |
| `test_viewBuyEdit_setButtonStates_noSelection_addLabelHinzufuegen` | `setButtonStates(false,false,false)` | Button-Text = "Hinzufügen" |
| `test_viewBuyEdit_setButtonStates_canRemoveTrue_saveLabelSpeichern` | `setButtonStates(true,true,true)` | Button-Text = "Speichern" |
| `test_viewBuyEdit_setButtonStates_nonLatestBuy_saveLabelSpeichern` | `setButtonStates(false,false,true)` | Button-Text = "Speichern" (kein canRemove nötig) |
| `test_viewBuyEdit_setButtonStates_canRemoveFalse_removeDisabled` | `canRemove=false` | Entfernen-Button deaktiviert |
| `test_viewBuyEdit_setButtonStates_canRemoveTrue_removeEnabled` | `canRemove=true` | Entfernen-Button aktiv |
| `test_viewBuyEdit_setButtonStates_notLastBuy_fieldsDisabled` | `isLastBuy=false, isEdit=true` | Ordernummer-Feld deaktiviert |
| `test_viewBuyEdit_setButtonStates_isLastBuy_fieldsEnabled` | `isLastBuy=true` | Ordernummer-Feld aktiv |

---

ViewBuyEdit — populateOverview:

@note Migration auf OverviewTabWidget (16.07.2026, siehe ARCHITECTURE.md):
alle Tests in diesem Abschnitt und in "ViewBuyEdit — Tab-Klick-Logik" wurden
von `findChild<QTabWidget*>()` auf `findChild<OverviewTabWidget*>()`
umgestellt — reiner Typ-Austausch, da `count()/widget()/tabText()/
currentIndex()/setCurrentIndex()` bewusst identisch zur bisherigen
`QTabWidget`-API benannt sind.

Die Kauf-Uebersicht verwendet ein Frozen-Footer-Layout: pro Tab ein Container mit
scrollbarem `dataTable` und fixiertem `footerTable` (Gesamt-Zeile). Die Spaltenbreiten
beider Tables werden ueber `QHeaderView::sectionResized` synchron gehalten.
Alle Zellwerte sind zentriert. Icons in der Dokument-Spalte werden per `setCellWidget`
mit einem zentrierten `QLabel` gesetzt — `QTableWidgetItem::setIcon` ignoriert AlignCenter.
Die Data-Table wird per `container->property("dataTable")` abgerufen (zuverlässiger als
`findChild`, da jeder Container zwei QTableWidgets enthält).
Tab-Reihenfolge: Übersicht-Tab (Index 0), dann Jahres-Tabs **absteigend nach Jahr** (neuestes zuerst).

| Test | Beschreibung | Prueft |
|------|--------------|--------|
| `test_viewBuyEdit_populateOverview_emptyBuys_noTabs` | Leere Liste — kein Tab | `tabs->count()` = 0 |
| `test_viewBuyEdit_populateOverview_singleYear_twoTabs` | 1 Kauf in 2024 — 2 Tabs | Tab 0 = "Uebersicht", Tab 1 enthaelt "2024" |
| `test_viewBuyEdit_populateOverview_twoYears_threeTabs` | Kaeufe in 2023 + 2024 — 3 Tabs | 3 Tabs vorhanden |
| `test_viewBuyEdit_populateOverview_jahresTabsDescendingByYear` | Neuestes Jahr zuerst | Tab 1 = 2024, Tab 2 = 2022 |
| `test_viewBuyEdit_populateOverview_uebersichtTabHasTable` | Uebersicht-Tab enthaelt QTableWidget | `findChild<QTableWidget*>()` nicht null, 3 Spalten, 1 Zeile |
| `test_viewBuyEdit_populateOverview_jahresTabHasSixColumns` | Jahres-Tab hat 6 Spalten | `columnCount()` = 6 (Datum, Anteile, Kurswert, Gebuehren, Einzahlung, Dokument) |
| `test_viewBuyEdit_populateOverview_jahresTabRowCount` | Zeilenanzahl = Anzahl Kaeufe (Gesamt nicht in Tabelle) | 3 Kaeufe — `rowCount()` = 3 |
| `test_viewBuyEdit_populateOverview_guidStoredInDateColumn` | GUID in Spalte 0 (Datum), `Qt::UserRole` | `item(0,0)->data(UserRole)` = buy.guid() |
| `test_viewBuyEdit_populateOverview_kurswertIsPrice` | Kurswert-Spalte zeigt Kurs je Aktie, nicht Gesamtwert | `item(0,2)->text()` enthaelt `b.price()`, nicht `b.buyValue()` |
| `test_viewBuyEdit_populateOverview_footerKurswertIsDash` | Gesamt-Zeile Kurswert = "-" | Footer `item(0,2)->text()` = "-" |
| `test_viewBuyEdit_populateOverview_docIconWhenPathSet` | Dokument-Pfad gesetzt — `QLabel` als CellWidget in Spalte 5 | `tbl->cellWidget(0,5)` nicht null |
| `test_viewBuyEdit_populateOverview_docPdfIcon` | Endung ".pdf" — DocPdfImage16-Icon | `cellWidget` hat Pixmap gesetzt |
| `test_viewBuyEdit_populateOverview_docDashWhenNoPath` | Kein Dokument — "-" in Spalte 5 als Item-Text | `item(0,5)->text()` = "-", kein CellWidget |
| `test_viewBuyEdit_populateOverview_tabTitleContainsTotal` | Tab-Titel enthaelt Gesamtbetrag | Titel enthaelt "Euro-Zeichen" |
| `test_viewBuyEdit_populateOverview_repopulateReplacesOldTabs` | Zweiter Aufruf ersetzt alle Tabs | Alte Tabs verschwunden, neue korrekt |

---

ViewBuyEdit — Tab-Klick-Logik:
Klick-Verhalten der Kauf-Uebersicht: Uebersicht-Tab springt bei Zeilenklick zum
Jahres-Tab. Jahres-Tab delegiert an den Presenter. Tab-Wechsel zum Jahres-Tab
selektiert erste Zeile und ruft `onRowSelected()` auf. Tab-Wechsel zu Übersicht
ruft `onReset()` auf → Formular geleert, Button "Hinzufügen".

| Test | Beschreibung | Prueft |
|------|--------------|--------|
| `test_viewBuyEdit_uebersichtClick_jumpsToYearTab` | Klick auf 2024-Zeile im Uebersicht-Tab | `tabs->currentIndex()` = Index des 2024-Tabs |
| `test_viewBuyEdit_uebersichtRowSelection_isEnabled` | Uebersicht-Tab hat SelectRows | `selectionBehavior()` = `SelectRows` |
| `test_viewBuyEdit_jahresTab_hasSelectRows` | Jahres-Tab hat SelectRows | `selectionBehavior()` = `SelectRows` |
| `test_viewBuyEdit_tabChange_clearsOldSelection` | Tab-Wechsel leert Selektion im verlassenen Tab | `selectedItems()` leer nach Wechsel |
| `test_viewBuyEdit_tabChange_selectsFirstRowInJahresTab` | Wechsel zu Jahres-Tab selektiert Zeile 0 | `currentRow()` = 0 |
| `test_viewBuyEdit_tabChange_noAutoSelectInUebersicht` | Wechsel zu Uebersicht-Tab (Index 0) — keine Autoauswahl | `selectedItems()` leer |
| `test_viewBuyEdit_tabChange_toJahresTab_selectsFirstRow` | Wechsel zu Jahres-Tab selektiert Zeile 0 | Selektion nicht leer, `currentRow()` = 0 |
| `test_viewBuyEdit_tabChange_backToUebersicht_clearsJahresSelection` | Zurück zu Übersicht → Jahres-Tab-Selektion geleert | `selectedItems()` leer |

Split-Hinweis (ergänzt 09.08.2026, Phase 3b) — die Formatierung selbst prüft
`tst_sharesplithint`, hier geht es ausschliesslich um die Verdrahtung:

| Test | Prüft |
| ---- | ----- |
| `test_presenterBuyEdit_setsSplitHintOnConstruction` | Hinweis wird beim Öffnen gesetzt |
| `test_presenterBuyEdit_noSplits_hintSaysNoSplit` | `hasSplit` = false, Tooltip leer |
| `test_presenterBuyEdit_splitAfterBuyDate_hintIsActive` | `hasSplit` = true, Text enthält "20:1" |
| `test_presenterBuyEdit_splitBeforeBuyDate_hintIsInactive` | Kauf nach dem Split → `hasSplit` = false |
| `test_presenterBuyEdit_onDateEdited_refreshesHint` | Datumswechsel schaltet den Hinweis um |
| `test_presenterBuyEdit_onValuesChanged_refreshesHint` | Stückzahländerung rechnet neu (10 × 20 = 200) |
| `test_viewBuyEdit_hasSplitHintLabel` | Label `splitHint` existiert |
| `test_viewBuyEdit_setSplitHint_setsTextAndTooltip` | Text und Tooltip landen im Label |
| `test_viewBuyEdit_setSplitHint_labelStaysVisibleWithoutSplit` | Zeile bleibt auch ohne Split stehen |

@note `test_..._onDateEdited_refreshesHint` und `test_..._onValuesChanged_refreshesHint`
sind bewusst getrennt. Der Hinweis hängt an Datum, Stückzahl und Preis, aber
`refreshDerivedValues()` läuft beim Datumswechsel nicht und `onDateEdited()`
nicht beim Ändern von Stückzahl oder Preis. Wer später einen der beiden
Aufrufe entfernt, weil er redundant aussieht, bekommt von genau einem der
beiden Tests Widerspruch.

@note `test_..._labelStaysVisibleWithoutSplit` hält die Platzierungsentscheidung
vom 08.08.2026 fest: die Zeile darf NICHT ausgeblendet werden, wenn kein Split
vorliegt — sonst springt beim Tippen im Datumsfeld das halbe Formular. Ein
naheliegendes `setVisible(hasSplit)` würde hier auffliegen.

---

#### tst_shareeditform — ShareEditForm

Executable: `tst_shareeditform`
Klassen unter Test: `ViewShareEdit`

@note `ViewShareEdit.cpp` zieht alle fünf Sub-Form-Trios (`BuysForm`,
`SalesForm`, `DividendForm`, `BrokeragesForm` und seit 08.08.2026
`ShareSplitsForm`) als Compile-Abhängigkeit rein — diese werden in
`tst_shareeditform` nur kompiliert und gelinkt, aber nicht getestet. `ModelShareEdit` und `PresenterShareEdit` sind ebenfalls Compile-
Abhängigkeiten; ihre Tests verbleiben in `tst_mainwindow`.

ViewShareEdit:

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_viewShareEdit_canBeConstructed` | Dialog öffnet ohne Absturz | Fenstertitel enthält "Aktie" |
| `test_viewShareEdit_hasPencilButtons` | Fünf Pencil-Buttons vorhanden (Käufe, Verkäufe, Dividenden, Kosten, Splits) | Anzahl Buttons mit Icon und leerem Text = 5 |
| `test_viewShareEdit_hasSaveAndCloseButtons` | Speichern- und Schließen-Button vorhanden | Beide Texte gefunden |
| `test_viewShareEdit_loadShare_setsWknAndIsin` | `loadShare()` schreibt WKN + ISIN | `wkn()` = "840400", `isin()` = "DE0008404005" |
| `test_viewShareEdit_loadShare_setsName` | Name wird korrekt gesetzt | `name()` = "Allianz SE" |
| `test_viewShareEdit_loadShare_setsUpdateType` | UpdateType-RadioButton gewählt | `updateType()` = `Both` |
| `test_viewShareEdit_loadShare_setsMarketUrl` | Markt-URL korrekt gesetzt | `marketPriceUrl()` = erwartete URL |
| `test_viewShareEdit_loadShare_setsDailyUrl` | Tages-URL korrekt gesetzt | `dailyValuesUrl()` = erwartete URL |
| `test_viewShareEdit_loadShare_setsMarketParsingType` | Markt-Parsing-Combo gesetzt | `marketPriceParsingType()` = `ApiYahoo` |
| `test_viewShareEdit_loadShare_setsDailyParsingType` | Tages-Parsing-Combo gesetzt | `dailyValuesParsingType()` = `ApiOnVista` |
| `test_viewShareEdit_loadShare_setsShareType` | Typ-Combo gesetzt | `shareType()` = `Etf` |
| `test_viewShareEdit_loadShare_setsDetailsWebsite` | Details-Webseite gesetzt | `detailsWebsite()` = erwartete URL |
| `test_viewShareEdit_setFirstBuyDate_setsText` | Datum-Feld gesetzt | Mindestens ein QLineEdit enthält den Datumsstring |
| `test_viewShareEdit_setFirstBuyDate_emptyShowsDash` | Leerer String → "-" | QLineEdit mit Text "-" vorhanden |
| `test_viewShareEdit_setCurrentVolume_formatsWithFourDecimals` | Anteile mit 4 Dezimalstellen | QLocale-formatierter Wert in einem QLineEdit |
| `test_viewShareEdit_setTotalBuys_setsEinzahlungAndKaeufe` | `setTotalBuys()` → beide Felder | 2 QLineEdits mit gleichem Geldwert (m_totalBuys + m_einzahlung) |
| `test_viewShareEdit_setTotalSales_setsField` | Verkäufe-Feld gesetzt | Formatierter Wert in einem QLineEdit |
| `test_viewShareEdit_setTotalProfitLoss_positiveIsGreen` | Positiver Wert → grüne Farbe | StyleSheet eines QLineEdit enthält "green" |
| `test_viewShareEdit_setTotalProfitLoss_negativeIsRed` | Negativer Wert → rote Farbe | StyleSheet enthält "red" |
| `test_viewShareEdit_setTotalProfitLoss_zeroNoColor` | Wechsel zu 0 → kein StyleSheet | Kein "red"/"green" nach Reset auf 0 |
| `test_viewShareEdit_setTotalDividends_setsField` | Dividenden-Feld gesetzt | Formatierter Wert gefunden |
| `test_viewShareEdit_setTotalBrokerages_setsField` | Kosten-Feld gesetzt | Formatierter Wert gefunden |
| `test_viewShareEdit_marketApiKey_disabledForRegex` | Regex-Modus → API-Key-Feld leer | `marketPriceApiKey()` leer |
| `test_viewShareEdit_marketApiKey_setFromSettingsForYahoo` | Yahoo-Modus → Key aus AppSettings | `marketPriceApiKey()` = gesetzter Key |
| `test_viewShareEdit_dailyApiKey_setFromSettingsForOnVista` | OnVista-Modus → Key aus AppSettings | `dailyValuesApiKey()` = gesetzter Key |
| `test_viewShareEdit_refreshSummary_doesNotCrash` | `refreshSummary()` ohne Absturz | Kein Absturz |

Split-Zeile in "Allgemein" (ergänzt 08.08.2026, Phase 3a der
Aktiensplit-Behandlung) — sechs Tests zu `setSplitInfo()`. Anders als bei den
Update-Radios wird hier gezielt über `objectName` gesucht (`splitsField`,
`btnEditSplits`) statt über alle `QLineEdit` des Dialogs: der Hinweis soll
nachweislich in genau diesem einen Feld neben dem Stift-Button landen, nicht
irgendwo:

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_viewShareEdit_hasSplitsFieldAndButton` | Feld und Button existieren | `splitsField` und `btnEditSplits` per `objectName` gefunden |
| `test_viewShareEdit_setSplitInfo_emptyShowsKeine` | Aktie ohne Splits | Feldtext = "keine" |
| `test_viewShareEdit_setSplitInfo_singleSplitShowsRatioAndDate` | Genau ein Split | Text beginnt mit "20:1", enthält das Datum |
| `test_viewShareEdit_setSplitInfo_multipleShowsCountAndLatest` | Zwei Splits | Text enthält "2 Splits" und das jüngste Verhältnis |
| `test_viewShareEdit_setSplitInfo_tooltipListsAllSplits` | Zwei Splits | Tooltip enthält beide Verhältnisse |
| `test_viewShareEdit_setSplitInfo_reverseSplitKeepsRatioOrder` | Reverse-Split 1:10 | Text beginnt mit "1:10", nicht "10:1" |

@note `test_..._singleSplitShowsRatioAndDate` prüft mit `startsWith("20:1")`
bewusst auch die Formatierung ohne Nachkommastellen. Ein Verhältnis als
"20,00:1,00" darzustellen wäre nicht falsch, aber schwer lesbar — und der Test
schlägt an, sobald jemand die Sonderbehandlung ganzer Zahlen entfernt.

@note `test_..._reverseSplitKeepsRatioOrder` ist der Regressionstest gegen eine
vertauschte Verhältnis-Darstellung. Aus einer Zusammenlegung (1:10) würde
optisch eine Teilung (10:1), was die Bedeutung ins Gegenteil verkehrt — ein
Fehler, der ohne Test lange unbemerkt bliebe, weil beide Formen plausibel
aussehen.

Update-Typ-Sperre bei Bestand (ergänzt 06.08.2026) — sieben Tests zu
`setDailyValuesRequired()`. Die vier Radiobuttons haben bewusst keine
`objectName`-Vergabe (sie entstehen in einer Schleife über eine
Beschriftungstabelle in `createGeneralGroup()`), der Helfer `updateRadio()`
sucht sie daher über `findChildren<QRadioButton*>()` anhand ihrer Beschriftung:

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_viewShareEdit_updateRadios_allPresentAndEnabledByDefault` | Aktie ohne Käufe, Bestand also 0 | Alle vier Radios vorhanden und wählbar |
| `test_viewShareEdit_setDailyValuesRequired_disablesMarketPriceAndNone` | `setDailyValuesRequired(true)` | "Markt-Preis" und "Keine" gesperrt |
| `test_viewShareEdit_setDailyValuesRequired_keepsDailyVariantsSelectable` | Gleicher Aufruf | "Beide" und "Tages-Werte" bleiben wählbar |
| `test_viewShareEdit_setDailyValuesRequired_falseReEnablesAll` | `true` gefolgt von `false` | Sperre wird vollständig zurückgenommen |
| `test_viewShareEdit_setDailyValuesRequired_keepsStoredSelectionChecked` | Gespeichertes `None` geladen, danach Sperre gesetzt | Radio "Keine" bleibt angehakt und ist gesperrt, `updateType()` = `None` |
| `test_viewShareEdit_updateHint_hiddenUntilRequired` | Frisch geöffneter Dialog | `updateHint` existiert, unsichtbar, Text leer |
| `test_viewShareEdit_updateHint_shownWhenRequired` | Nach `setDailyValuesRequired(true)` | Sichtbar, Text nicht leer |

@note **`isVisibleTo()` statt `isVisible()`:** Der Dialog wird in den Tests nie
gezeigt — `isVisible()` wäre für jedes Kind-Widget `false`, unabhängig davon,
ob `setVisible()` korrekt aufgerufen wurde. `isVisibleTo(&dlg)` prüft die
Sichtbarkeit relativ zum Elternwidget und ist damit die einzige Variante, die
hier überhaupt etwas aussagt.

@note `test_..._keepsStoredSelectionChecked` ist der eigentliche
Regressionstest der Altbestands-Entscheidung: eine gespeicherte, jetzt
unzulässige Auswahl darf beim Öffnen NICHT still umgestellt werden (siehe
ARCHITECTURE.md). Er scheitert also bewusst, sobald jemand die View dazu
bringt, den Update-Typ selbstständig zu korrigieren. Die Gegenprobe — dass
das Speichern eines solchen Werts blockiert wird — braucht ein Stub-Paar und
liegt deshalb in `tst_mainwindow.cpp`, siehe dort
`test_presenterShareEdit_onSave_forbiddenUpdateType_showsErrorAndDoesNotSave`.
`tst_shareeditform` selbst arbeitet durchgehend gegen den echten Dialog.

---

#### tst_sharesplitsform — ShareSplitsForm (neu, 08.08.2026)

Executable: `tst_sharesplitsform`
Klassen unter Test: `ViewShareSplitEdit`, `ModelShareSplitEdit`, `PresenterShareSplitEdit`

@note Stub-Pattern: `StubViewShareSplitEdit` und `StubModelShareSplitEdit` für
alle Presenter-Tests. `StubViewShareSplitEdit::confirm()` liefert einen je Test
setzbaren Wert (`confirmResult`) zurück, statt einen modalen Dialog zu öffnen —
genau dafür sitzt die Löschabfrage im View-Interface und nicht als direkter
`OwnMessageBox::question()`-Aufruf im Presenter. Ohne diesen Umweg wäre der
gesamte Löschpfad nicht testbar.

@note Schlanker als `tst_shareeditform`: die Split-Maske hat keine
Parse-Pipeline und braucht deshalb weder `PdfTextExtractor` noch
`DocumentClassifier` als Compile-Abhängigkeit. `DocumentPreviewPanel` und
`DocumentRootMigrator` kamen am 08.08.2026 mit der Dokumentspalte dazu, ebenso
der `SPM_HAVE_QTPDF`-Zweig im CMake-Ziel.

@note `main()` setzt `QLocale::setDefault(QLocale::German)`. Presenter und View
formatieren Zahlen und Datumsangaben über `QLocale()`; CI-Runner laufen nicht
mit deutschem Locale, ohne diese Zeile schlügen die Vergleiche der
Löschabfrage-Texte dort fehl.

PresenterShareSplitEdit — Konstruktion und Vorschau:

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_presenter_populatesOverviewOnConstruction` | Übersicht wird beim Öffnen gefüllt | `populateOverview()` erhält die Modell-Liste |
| `test_presenter_startsInAddModeWithRemoveDisabled` | Frisch geöffnet | `canRemove` = false, `isEdit` = false |
| `test_presenter_setsFactorPreviewOnConstruction` | Umrechnungs-Vorschau initial gesetzt | Text nicht leer, enthält "20" |
| `test_presenter_factorPreview_reverseSplitUsesSingular` | Reverse-Split 1:10 | Text enthält "wird 1", nicht "werden" |
| `test_presenter_factorPreview_invalidRatioShowsDash` | Verhältnis-Seite = 0 | Vorschau = "-" |

PresenterShareSplitEdit — Validierung beim Speichern:

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_presenter_onSave_validSplit_callsAdd` | Gültige Eingabe | `addSplit()` aufgerufen, Werte korrekt übernommen |
| `test_presenter_onSave_emitsDataChanged` | Gültige Eingabe | `dataChanged()` genau einmal gesendet |
| `test_presenter_onSave_sentinelDate_showsErrorAndDoesNotSave` | Ex-Tag = 01.01.2000 | Kein Speichern, Fehlermeldung |
| `test_presenter_onSave_futureDate_isAllowed` | Ex-Tag ein Jahr in der Zukunft | Speichern erfolgt, keine Meldung |
| `test_presenter_onSave_zeroRatio_showsErrorAndDoesNotSave` | Verhältnis-Seite = 0 | Kein Speichern, Fehlermeldung |
| `test_presenter_onSave_ratioOneToOne_isRejected` | Verhältnis 1:1 | Kein Speichern, Fehlermeldung |
| `test_presenter_onSave_equivalentRatio_isAlsoRejected` | Verhältnis 2:2 | Kein Speichern — geprüft wird der Quotient, nicht der Wortlaut |
| `test_presenter_onSave_duplicateDate_showsErrorAndDoesNotSave` | Tag bereits belegt | Kein Speichern, Fehlermeldung |
| `test_presenter_onSave_modelFails_showsError` | Modell meldet Fehler | `showError()` mit der Modell-Meldung |

PresenterShareSplitEdit — Bearbeiten:

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_presenter_onRowSelected_loadsSplitAndEnablesRemove` | Zeile ausgewählt | `loadSplit()` aufgerufen, `canRemove` und `isEdit` = true |
| `test_presenter_onRowSelected_olderSplitIsAlsoRemovable` | Ältere von zwei Zeilen | `canRemove` = true — keine Letzter-Eintrag-Sperre |
| `test_presenter_onRowSelected_emptyGuid_resetsForm` | Auswahl aufgehoben | `clearForm()` aufgerufen |
| `test_presenter_onSave_existingSplit_callsUpdate` | Geladener Split gespeichert | `updateSplit()` statt `addSplit()`, GUID erhalten |
| `test_presenter_onSave_existingSplit_unchangedDateIsNoDuplicate` | Nur Verhältnis geändert | Speichern erlaubt trotz `existsForDate()` = true |
| `test_presenter_onSave_existingSplit_changedDateToOccupiedDay_isRejected` | Auf belegten Tag verschoben | Kein Speichern, Fehlermeldung |

Dokument (ergänzt 08.08.2026):

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_presenter_onSave_storesDocumentPath` | Pfad im Feld | Landet unverändert im gespeicherten Objekt |
| `test_presenter_onSave_trimsDocumentPath` | Pfad mit Leerzeichen | Wird getrimmt gespeichert |
| `test_presenter_onDocumentSelected_setsPathAndPreview` | Auswahl im Dateidialog | Feld gesetzt UND Vorschau geladen |
| `test_presenter_onDocumentPathEdited_duplicateShowsHint` | Pfad schon vergeben | Hinweis über `showError()` |
| `test_presenter_onDocumentPathEdited_duplicateDoesNotBlockSaving` | Gleicher Fall, danach speichern | Speichern läuft trotzdem durch |
| `test_presenter_onDocumentPathEdited_emptyPath_noCheck` | Leeres Feld | Keine Prüfung, keine Meldung |
| `test_presenter_onDocumentPathEdited_excludesLoadedSplit` | Geladener Split | Eigene GUID wird als `excludeGuid` durchgereicht |
| `test_presenter_onRowSelected_withDocument_opensPreview` | Zeile mit Beleg | Vorschau wird mit dem Pfad geladen |
| `test_presenter_onRowSelected_withoutDocument_clearsPreview` | Zeile ohne Beleg | Vorschau wird geleert |
| `test_presenter_onReset_clearsPreview` | Reset | Vorschau wird geleert |

@note `test_..._duplicateDoesNotBlockSaving` hält die Entscheidung fest, dass
die Doppelbelegung nur ein Hinweis ist. Zwei Splits können legitim auf
derselben Bankmitteilung stehen; würde jemand die Prüfung später zu einer
Blockade in `validateInput()` hochziehen, schlägt dieser Test an und erzwingt
eine bewusste Entscheidung.

@note `test_..._excludesLoadedSplit` prüft nicht das Ergebnis, sondern das
ÜBERGEBENE ARGUMENT — der Stub merkt sich den `excludeGuid`. Ohne diesen Weg
liesse sich nicht unterscheiden, ob die Ausnahme wirklich gesetzt wurde oder
ob das Modell zufällig `false` geliefert hat.

PresenterShareSplitEdit — Löschen:

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_presenter_onRemove_withoutSelection_doesNothing` | Keine Auswahl | Weder `confirm()` noch `removeSplit()` |
| `test_presenter_onRemove_asksForConfirmation` | Auswahl vorhanden | `confirm()` aufgerufen, danach gelöscht |
| `test_presenter_onRemove_declinedConfirmation_doesNotRemove` | Rückfrage verneint | `removeSplit()` NICHT aufgerufen |
| `test_presenter_onRemove_confirmationNamesTheSplit` | Meldungstext | Enthält "20:1" |
| `test_presenter_onRemove_confirmationShowsVolumeChange` | 100 Stk. vor 20:1-Split | Meldung enthält 2.000,0000 und 100,0000 |
| `test_presenter_onRemove_lotAfterSplitIsUnaffected` | Kauf nach dem Splittag | Bestand vorher = nachher |
| `test_presenter_onRemove_emitsDataChanged` | Erfolgreiches Löschen | `dataChanged()` genau einmal |
| `test_presenter_onRemove_modelFails_showsError` | Modell meldet Fehler | `showError()` mit der Modell-Meldung |

PresenterShareSplitEdit — Reset und Schliessen:

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_presenter_onReset_clearsFormAndButtonStates` | Nach Auswahl zurückgesetzt | `clearForm()`, `canRemove` und `isEdit` = false |
| `test_presenter_onReset_forgetsSelection` | Speichern nach Reset | `addSplit()` statt `updateSplit()` |
| `test_presenter_onClose_closesView` | Schliessen | `acceptAndClose()` aufgerufen |

PresenterShareSplitEdit — Kurssprung-Prüfung ("Prüfen"-Knopf,
`SplitPriceJumpDetector`, 13.08.2026, siehe ARCHITECTURE.md, "Automatische
Erkennung split-bereinigter Kurshistorie"):

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_presenter_onCheckPriceJump_missingDate_showsErrorAndDoesNotDetect` | Ex-Tag = Sentinel (01.01.2000) | Fehlermeldung, kein `setPricesAdjusted()`, kein Hinweistext |
| `test_presenter_onCheckPriceJump_invalidRatio_showsErrorAndDoesNotDetect` | Verhältnis-Seite = 0 | Fehlermeldung, kein `setPricesAdjusted()` |
| `test_presenter_onCheckPriceJump_clearJump_setsCheckedAndHint` | 20:1-Split, Kurs springt von ~1.000 auf ~50 | Haken automatisch AUS, Hinweistext gesetzt, `PriceJumpTone::Adopted` |
| `test_presenter_onCheckPriceJump_noJump_setsCheckedTrueAndHint` | Kurs bleibt über den Ex-Tag hinweg gleich | Haken automatisch AN, `PriceJumpTone::Adopted` |
| `test_presenter_onCheckPriceJump_ambiguous_doesNotTouchCheckbox` | Verhältnis zwischen den Toleranzbändern | Haken UNVERÄNDERT, Hinweistext gesetzt, `PriceJumpTone::ManualDecisionNeeded` |
| `test_presenter_onCheckPriceJump_insufficientData_showsHintNotError` | Keine Kursdaten vorhanden | Kein `showError()` (kein Bedienfehler), sondern Hinweistext, `PriceJumpTone::ManualDecisionNeeded` |
| `test_presenter_onCheckPriceJump_passesLookbackWindowToModel` | Ex-Tag 18.07.2022 | Modell erhält Zeitraum ± `SplitPriceJumpDetector::kDefaultMaxLookbackDays` |
| `test_presenter_onCheckPriceJump_excludesOwnSplitAsNeighbor` | Der gerade bearbeitete Split selbst als einziger vorhandener Split | Grenzt das Suchfenster nicht auf das eigene Datum ein |

@note `test_presenter_onCheckPriceJump_insufficientData_showsHintNotError`
hält eine bewusste Unterscheidung fest: fehlende Kursdaten sind kein
Bedienfehler des Nutzers und laufen deshalb über das Ergebnisfeld, nicht
über `showError()`.

@note `test_presenter_onCheckPriceJump_excludesOwnSplitAsNeighbor` ist ein
Regressionstest für einen Bearbeiten-Sonderfall: ohne den Ausschluss würde
das Suchfenster beim Editieren eines bestehenden Splits auf dessen eigenes
Datum kollabieren, weil `PresenterShareSplitEdit` denselben Split auch als
"Nachbar-Split" an `SplitPriceJumpDetector::detect()` übergeben würde.

ModelShareSplitEdit — gegen die echte In-Memory-Datenbank:

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_model_addAndLoadSplit_roundTrip` | Anlegen und Lesen | Verhältnis und Kommentar unverändert zurück |
| `test_model_loadSplits_orderedByDateAscending` | Umgekehrt eingefügt | Rückgabe aufsteigend nach Datum |
| `test_model_existsForDate_findsInsertedSplit` | Belegter und freier Tag | true bzw. false |
| `test_model_updateSplit_changesStoredValues` | Verhältnis und Flag geändert | Werte in der Datenbank aktualisiert |
| `test_model_removeSplit_deletesRow` | Löschen | Liste danach leer |
| `test_model_removeSplit_leavesBuysUntouched` | Löschen bei vorhandenem Kauf | `buys.volume` und `buys.price` unverändert |
| `test_model_openLots_skipsFullySoldBuys` | Ein Kauf voll verkauft, einer teilweise | Nur der offene Posten, Restmenge = 30 |
| `test_model_documentExists_findsAssignedDocument` | Vergebener Beleg, ohne excludeGuid | true — der Fall, der den NULL-Fallstrick aufdeckte |
| `test_model_documentExists_unknownDocument_returnsFalse` | Unbekannter Pfad | false |
| `test_model_documentExists_excludesOwnGuid` | Eigene GUID ausgenommen | false |
| `test_model_documentExists_otherSplitStillCounts` | Fremde GUID ausgenommen | true — die Ausnahme darf nicht zu weit greifen |
| `test_model_documentExists_emptyPath_returnsFalse` | Leerer Pfad | false |
| `test_model_documentExists_trimsPath` | Pfad mit Leerzeichen | true — es wird auch beim Binden getrimmt |

@note `test_..._findsAssignedDocument` und `test_..._otherSplitStillCounts`
sind die Lehre aus dem Bugfix vom 08.08.2026. Die erste Fassung hatte nur den
Ausschluss-Fall getestet — und ein Test, der belegt, dass etwas NICHT gefunden
wird, kann einen Fehler nicht entdecken, bei dem nie etwas gefunden wird. Wo
eine Prüfung beide Antworten liefern muss, braucht es beide Richtungen als
Test, sonst ist der Negativtest wertlos.

@note `test_model_removeSplit_leavesBuysUntouched` ist der Regressionstest der
Grundentscheidung vom 07.08.2026: die Datenbank behält die Beleg-Wahrheit, ein
Split ist nur eine Rechenvorschrift. Sobald jemand beim Löschen anfinge,
`buys` mit umzuschreiben, wäre der Vorgang nicht mehr umkehrbar — und der Test
schlägt an.

ViewShareSplitEdit — Widget-Ebene:

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_view_canBeConstructed` | Dialog öffnet ohne Absturz | Fenstertitel enthält "Split" |
| `test_view_hasAllFormFields` | Alle Eingabefelder vorhanden | Sieben `objectName`-Treffer inkl. Tabelle |
| `test_view_hasNoOverviewTabWidget` | Kein `OverviewTabWidget` | Kein `QTabWidget` im Dialog |
| `test_view_loadSplit_populatesFields` | Split geladen | Alle fünf Felder korrekt gefüllt |
| `test_view_clearForm_resetsRatioToOne` | Nach `clearForm()` | Verhältnis 1:1, Flag aus, Kommentar leer |
| `test_view_populateOverview_fillsTable` | Zwei Splits | `rowCount()` = 2, Verhältnis-Zelle = "20:1" |
| `test_view_populateOverview_storesGuidPerRow` | Ein Split | GUID an erster UND letzter Zelle der Zeile |
| `test_view_populateOverview_clearsPreviousRows` | Zweiter Aufruf mit leerer Liste | `rowCount()` = 0 |
| `test_view_setButtonStates_editModeRenamesAddButton` | `isEdit` = true | Button heisst "Speichern" |
| `test_view_setButtonStates_removeDisabledWithoutSelection` | `canRemove` = false | Entfernen-Button gesperrt |
| `test_view_setFactorPreview_setsField` | Vorschautext gesetzt | Feldtext exakt übernommen |
| `test_view_futureDateIsAccepted` | Datum ein Jahr in der Zukunft | `splitDate()` liefert es unverändert |
| `test_view_ratioFieldsAcceptGermanDecimalComma` | Eingabe "1,5" | `ratioNew()` = 1.5 |
| `test_view_presenterIsAccessible` | `presenter()`-Getter | Nicht `nullptr` |

Dokument und Vorschau auf Widget-Ebene (ergänzt 08.08.2026):

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_view_hasDocumentFieldAndBrowseButton` | Pfadfeld und "…"-Button | Beide per `objectName` gefunden |
| `test_view_loadSplit_populatesDocumentPath` | Split mit Beleg geladen | `documentPath()` = gespeicherter Pfad |
| `test_view_clearForm_clearsDocumentPath` | Nach `clearForm()` | Feld leer |
| `test_view_hasPreviewPanel` | Vorschau eingebettet | `DocumentPreviewPanel` als Kind vorhanden |
| `test_view_clearPdfPreview_doesNotCrash` | Leeren der Vorschau | Kein Absturz, kein modaler Dialog |
| `test_view_overviewTable_hasDocumentColumn` | Tabellenaufbau | 6 Spalten, letzte ohne Überschrift, 36 px breit |
| `test_view_populateOverview_showsDocumentIcon` | Zeile mit und ohne Beleg | Icon plus Tooltip nur bei vorhandenem Beleg |

@note `test_view_clearPdfPreview_doesNotCrash` ist unbedenklich, obwohl er ein
Anzeige-Widget anfasst: `DocumentPreviewPanel` meldet eine fehlende Datei seit
19.07.2026 inline statt über `OwnMessageBox::critical()` — genau damit ein
solcher Aufruf den Testlauf nicht blockiert.

@note Der `onBrowseDocument()`-Pfad selbst bleibt ungetestet — er öffnet einen
`QFileDialog` und meldet den Fehlerfall über `OwnMessageBox::critical()`.
Dieselbe Konvention wie bei den fünf anderen Dialogen; die zugrundeliegende
`isPathWithinRoot()`-Logik ist in `tst_documentssettingsform.cpp` abgedeckt.

@note `test_view_hasNoOverviewTabWidget` hält die bewusste Abweichung von
BuysForm/SalesForm/DividendForm fest. Er ist kein Selbstzweck: er schlägt an,
sobald jemand die Übersicht "der Einheitlichkeit halber" auf
`OverviewTabWidget` umstellt, und zwingt damit zu einer erneuten Entscheidung
statt zu einer stillen Angleichung (siehe ARCHITECTURE.md,
"ShareSplitsForm-Details").

@note `test_view_populateOverview_storesGuidPerRow` prüft die erste UND die
letzte Spalte. Die GUID hängt absichtlich an jeder Zelle, damit die Auswahl
unabhängig davon auflösbar ist, welche Spalte der Benutzer angeklickt hat —
läge sie nur an Spalte 0, wäre ein Klick auf den Kommentar wirkungslos.

ViewShareSplitEdit — Kurssprung-Prüfung und Reverse-Split-Hinweis
(13./14.08.2026):

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_view_hasPriceJumpCheckButtonAndResultField` | "Prüfen"-Knopf und Ergebnisfeld | Beide per `objectName` gefunden (`btnCheckPriceJump`, `priceJumpResult`) |
| `test_view_hasReverseSplitHintButton` | "Hinweis Reverse-Split"-Knopf | Per `objectName` gefunden (`btnReverseSplitHint`), Text exakt, dauerhaft aktiv und nicht ausgeblendet |
| `test_view_pricesAdjustedCheckbox_isFindable` | "Kurshistorie"-Haken | Per `objectName` gefunden (`pricesAdjusted`) |
| `test_view_pricesAdjustedCheckbox_hasSameRowHeightAsOtherFields` | Feste Höhe wie alle anderen einzeiligen Felder | `minimumHeight() == maximumHeight() == 24` |
| `test_view_setPriceJumpHint_setsResultFieldText` | `setPriceJumpHint()` mit Text | Ergebnisfeld übernimmt den Text unverändert |
| `test_view_setPriceJumpHint_adoptedTone_usesGreenText` | `PriceJumpTone::Adopted` | Textfarbe `#388e3c` |
| `test_view_setPriceJumpHint_manualDecisionTone_usesRedText` | `PriceJumpTone::ManualDecisionNeeded` | Textfarbe `#d32f2f` |
| `test_view_setPricesAdjusted_checksCheckbox` | `setPricesAdjusted(true)` | `pricesAdjusted()` liefert true |
| `test_view_clearForm_clearsPriceJumpResult` | Nach `clearForm()` | Ergebnisfeld leer |
| `test_view_clearForm_resetsPriceJumpResultColor` | Eingefärbtes Ergebnis, danach `clearForm()` | Textfarbe wieder auf Ausgangswert |
| `test_view_loadSplit_clearsPriceJumpResult` | Anderen Split geladen | Ergebnis eines vorherigen Prüflaufs bleibt nicht stehen |
| `test_view_loadSplit_resetsPriceJumpResultColor` | Eingefärbtes Ergebnis, danach `loadSplit()` | Textfarbe wieder auf Ausgangswert |
| `test_view_priceJumpResult_hasFixedHeightRegardlessOfTextLength` | Kurzer und langer Ergebnistext | `minimumHeight() == maximumHeight()`, unverändert vor/nach `setPriceJumpHint()` |
| `test_view_priceJumpLabel_isTopAligned` | Label "Prüfung:" | Ausrichtung enthält `Qt::AlignTop` |
| `test_view_priceJumpButton_isTopAlignedInRow` | "Prüfen"-Knopf in seiner Zeile | Layout-Item-Ausrichtung enthält `Qt::AlignTop` |

@note `test_view_priceJumpResult_hasFixedHeightRegardlessOfTextLength`
prüft bewusst `minimumHeight()`/`maximumHeight()` statt `height()` — ohne
`show()` hinge Letzteres vom Zeitpunkt der Layout-Aktivierung ab, der Test
könnte bei einer versehentlich wieder textabhängigen Höhe zufällig grün
bleiben. Regressionstest für die ursprüngliche `QLabel`-Fassung, deren Höhe
je nach Textlänge wechselte und dadurch beim Prüfen/Reset alles darunter im
Dialog nach unten bzw. wieder nach oben springen liess.

@note `test_view_hasReverseSplitHintButton` prüft bewusst `isHidden()`
statt `isVisible()`: der Dialog wird in diesen headless Tests nie `show()`n,
`isVisible()` wäre also unabhängig vom Knopf-Code immer false.

---

#### tst_backupsettingsform — BackupSettingsForm (neu, 08.07.2026)

Executable: `tst_backupsettingsform`
Klasse unter Test: `BackupSettingsForm`, sowie die Backup-Sektion von `AppSettings`

@note Eigene Executable statt Erweiterung von `tst_mainwindow.cpp` — analog
`tst_buysform`/`tst_shareeditform` (siehe ARCHITECTURE.md, "Neue Forms
bekommen ihre eigene Test-Executable"). Deutlich schlanker als die anderen
Form-Tests: `BackupSettingsForm` braucht weder Datenbank noch `MainWindow`,
Compile-Abhängigkeiten sind nur `AppSettings.cpp` und `IconProvider.cpp`.
Alle Widgets tragen zu Testzwecken feste `objectName()`s (keine visuelle
Auswirkung), damit `findChild<T>(name)` eindeutig statt über Konstruktions-
Reihenfolge sucht — bei drei `QLineEdit`s und mehreren `QLabel`s im Dialog
wäre Positions-Suche fehleranfällig gewesen.

@note `AppSettings` ist ein Singleton — jeder Test, der Werte ändert, stellt
am Ende den ursprünglichen Wert wieder her (gleiches Muster wie
`test_loggerSettings_*`/`test_soundSettings_*` in `tst_mainwindow.cpp`).
Regressionstests für `createBackup()` selbst (Rotation, Präfix-Änderung,
`mkpath()`, Enable/Disable) bleiben in `TestBackupForm` (`tst_mainwindow.cpp`),
da `createBackup()` eine private Methode von `MainWindow` ist.

AppSettings — Backup-Sektion (reiner Speichern/Laden-Roundtrip):

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_backupSettings_saveEnabled` | `backupEnabled` gespeichert und gelesen | Wert korrekt geladen |
| `test_backupSettings_saveMaxCount` | `backupMaxCount` gespeichert und gelesen | Wert korrekt geladen |
| `test_backupSettings_saveNamePrefix` | `backupNamePrefix` gespeichert und gelesen | Wert korrekt geladen |
| `test_backupSettings_saveDateFormat` | `backupDateFormat` gespeichert und gelesen | Wert korrekt geladen |
| `test_backupSettings_saveDirectory` | `backupDirectory` gespeichert und gelesen | Wert korrekt geladen |

Dialog — Konstruktion & Laden:

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_dialog_constructsWithoutCrash` | Dialog öffnet ohne Absturz | Kein Absturz |
| `test_dialog_loadSettings_populatesEnabledCheckbox` | Checkbox nach `loadSettings()` | Checkbox-Zustand = `backupEnabled()` |
| `test_dialog_loadSettings_populatesPrefixAndDateFormat` | Präfix- und Datumsformat-Feld nach `loadSettings()` | Beide Felder = konfigurierte Werte |
| `test_dialog_loadSettings_populatesMaxCountFromKnownValue` | Anzahl-Combobox bei Wert aus der vordefinierten Liste (10) | `currentText()` = "10" |
| `test_dialog_loadSettings_populatesMaxCountFromCustomValue` | Anzahl-Combobox bei freiem Wert außerhalb der Liste (7) | `setCurrentText()`-Fallback greift, `currentText()` = "7" |

Dialog — Speichern:

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_dialog_save_persistsAllFieldsToAppSettings` | Alle fünf Felder geändert + Speichern-Klick | `AppSettings` enthält alle fünf neuen Werte |
| `test_dialog_save_emptyPrefixFallsBackToDefault` | Präfix-Feld nur Leerzeichen + Speichern | `backupNamePrefix()` = "Backup" |
| `test_dialog_save_emptyDateFormatFallsBackToDefault` | Datumsformat-Feld leer + Speichern | `backupDateFormat()` = "yyyy_MM_dd_HH_mm_ss" |
| `test_dialog_save_invalidMaxCountFallsBackToFive` | Anzahl-Feld nicht-numerischer Text ("abc") + Speichern | `backupMaxCount()` = 5, nicht 0 |
| `test_dialog_cancel_doesNotPersistChanges` | Checkbox im Dialog geändert, dann Abbrechen-Klick | `AppSettings` unverändert |

Dateinamen-Vorschau:

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_preview_updatesOnPrefixChange` | Vorschau reagiert live auf Präfix-Eingabe | Vorschautext beginnt mit neuem Präfix |
| `test_preview_containsPortfolioPlaceholder` | Vorschau enthält erkennbaren Platzhalter | Text enthält `<Portfolioname>` |
| `test_preview_emptyPrefixShowsDefaultInPreview` | Leeres Präfix-Feld → Vorschau zeigt Default | Vorschautext beginnt mit "Backup_" |

---

#### tst_traysettingsform — TraySettingsForm (neu, 03.08.2026)

Executable: `tst_traysettingsform`
Klasse unter Test: `TraySettingsForm`, sowie die Tray-Sektion von `AppSettings`

@note Eigene Executable statt Erweiterung von `tst_mainwindow.cpp`, analog
`tst_backupsettingsform` — braucht weder Datenbank noch `MainWindow`,
Compile-Abhängigkeiten sind nur `AppSettings.cpp` und `IconProvider.cpp`.
Die Checkbox trägt zu Testzwecken eine feste `objectName()`
(`chkTrayOnMinimizeEnabled`), keine visuelle Auswirkung.

@note `AppSettings` ist ein Singleton — `init()` setzt vor jedem Test
`trayOnMinimizeEnabled` explizit auf `false` zurück, gleiches Muster wie in
`tst_backupsettingsform`. Die Entscheidungslogik selbst
(`MainWindow::shouldMinimizeToTray()`) wird nicht hier, sondern in
`tst_mainwindow.cpp` getestet (siehe dort) — dieser Dialog testet nur
Laden/Speichern/Abbrechen der Einstellung.

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_loadSettings_disabled_checkboxUnchecked` | Dialog öffnet bei deaktivierter Option | Checkbox unchecked |
| `test_loadSettings_enabled_checkboxChecked` | Dialog öffnet bei aktivierter Option | Checkbox checked |
| `test_save_checkedThenSave_persistsEnabled` | Checkbox aktiviert + Speichern-Klick | `AppSettings::trayOnMinimizeEnabled()` = true |
| `test_save_uncheckedThenSave_persistsDisabled` | Checkbox deaktiviert + Speichern-Klick | `AppSettings::trayOnMinimizeEnabled()` = false |
| `test_cancel_doesNotPersistChange` | Checkbox geändert, dann Abbrechen-Klick | `AppSettings` unverändert |

---

#### tst_sharedetailsform — ShareDetailsForm (Depotwert 09.07., Marktwert 10.07., Gewinne/Verluste-/Dividenden-/Kosten-Tabs 13.07.2026)

Executable: `tst_sharedetailsform`
Klasse unter Test: `PresenterShareDetails`

@note Deckt "Komplette Depotbewertung" **und** "Komplette Marktbewertung" ab
(siehe ARCHITECTURE.md, "ShareDetailsForm-Details"). Der Aktien-Chart-Tab
selbst ist seit 12.07.2026 implementiert — seine Tests liegen in einer
eigenen Executable, `tst_chartform` (siehe eigener Abschnitt unten), analog
zur Trennung von `tst_sharedetailsform`/`tst_shareeditform`. Die
Gewinne/Verluste-, Dividenden- und Kosten-Tabs sind seit 13.07.2026
implementiert (siehe ARCHITECTURE.md, "OverviewTabWidget-Details") — auf
Presenter-Ebene (reines Durchreichen der drei neuen
`IModelShareDetails::load*()`-Methoden an `IViewShareDetails::populate*()`)
durch zwei Tests unten abgedeckt. **Nicht** abgedeckt: `DocumentPreviewPanel`
selbst (kein eigenes Test-Target, siehe ARCHITECTURE.md,
"Erledigt / Archiv") — insbesondere die Existenzprüfung in
`DocumentPreviewPanel::showDocument()` ist bislang durch nichts abgesichert.
`OverviewTabWidget` hat seit 14.07.2026 einen fixierten Übersicht-Tab (zwei
`QTabBar`s + `QStackedWidget` statt einem `QTabWidget`, siehe
ARCHITECTURE.md, "OverviewTabWidget-Details") sowie zwei nachgezogene
Bugfixes (Klick-Erkennung über `tabBarClicked` statt `currentChanged`,
dauerhaft fette Spaltenköpfe) — alles abgedeckt durch das eigene
Test-Target `tst_overviewtabwidget` (siehe eigener Abschnitt unten).
`ViewShareDetails::onMainTabChanged()` (Reset auf Jahresübersicht bei
äußerem Tab-Wechsel) ist durch `test_mainTabChanged_resetsOverviewTabsToUebersicht`
in `tst_mainwindow.cpp` abgedeckt (siehe "tests/forms/ — MainWindow" unten).
Der Gewinne/Verluste-Tab existiert seit 14.07.2026 in beiden Modi
(brokeragefrei im Marktwert-Modus, siehe ARCHITECTURE.md, "Marktwert- vs.
Depotwert-Modus") — auf Presenter-Ebene durch
`test_loadAndDisplay_marketMode_populatesOnlyGewinneVerluste` abgedeckt, auf
View-Ebene (Tab-Struktur je Modus) durch `test_marketMode_
hasOnlyGewinneVerlusteOverviewTab`/`test_depotwertMode_
hasAllThreeOverviewTabs` und (tatsächliche brokeragefreie vs. Final-Werte,
über echte DB-Daten statt Fakes) durch `test_marketMode_
gewinneVerlusteTab_usesBrokerageFreeValues`, alle drei in
`tst_mainwindow.cpp`.

@note **Entdeckter und behobener Folgefehler (15.07.2026):** Der letztgenannte
Test schlug beim ersten Durchlauf fehl (490,00 € erwartet, 500,00 € erhalten)
— nicht wegen der Marktwert/Depotwert-Umschaltung selbst, sondern wegen eines
unabhängigen, vorbestehenden Bugs in `ModelSaleEdit::addSale()` (fehlender
Brokerage-Vorwärts-Link `sales.brokerage_guid`, siehe ARCHITECTURE.md,
"SalesForm-Details"). Drei Tests decken den Fix ab: `test_modelSaleEdit_
addSale_success` (erweitert), `test_modelSaleEdit_addSale_
linksBrokerageForwardReference` (neu) und `test_modelSaleEdit_updateSale_
createsBrokerageIfMissing` (erweitert) — alle in `tst_mainwindow.cpp`, siehe
Tabelle unten.

@note Wie schon beim vorherigen Anlauf: **weder** Datenbank **noch**
`QWidget` **noch** `ShareCalculator` werden instanziiert.
`PresenterShareDetails` kennt View und Model ausschließlich über
`IViewShareDetails`/`IModelShareDetails` — `FakeModelShareDetails::
computeShareValues()` liefert ein von Hand befülltes `ShareValues`
zurück, `FakeViewShareDetails` speichert die drei `CalculationRows`-Listen
(`gesamtRows`/`vortagRows`/`aktuelleRows`) nur zwischen. Ein
`findRow(rows, label)`-Helfer sucht Zeilen über ihr (übersetztes) Label,
statt sich auf feste Index-Positionen zu verlassen. Seit 13.07.2026 zusätzlich:
`FakeModelShareDetails` hat `sales`/`dividends`/`brokerages`-Member (je
`QList<SaleObject>`/`QList<DividendObject>`/`QList<BrokerageObject>`,
manuell befüllbar), `FakeViewShareDetails` speichert die drei per
`populateGewinneVerluste()`/`populateDividenden()`/`populateKosten()`
übergebenen Listen in `saleRows`/`dividendRows`/`brokerageRows` sowie je ein
`gewinneVerlusteCalled`/`dividendenCalled`/`kostenCalled`-Flag — getrennt von
den Listen selbst, damit Tests "gar nicht aufgerufen" (Marktwert-Modus) von
"mit leerer Liste aufgerufen" unterscheiden können.

Seit 11.08.2026 (Aktiensplit-Behandlung, Phase 3c) hat `FakeModelShareDetails`
zusätzlich einen `splits`-Member und implementiert `loadSplits()`;
`FakeViewShareDetails` legt die per `populateGewinneVerluste()`/
`populateDividenden()` übergebenen Splits getrennt je Tab in
`gewinneVerlusteSplits`/`dividendenSplits` ab. Damit prüfbar, dass der
Presenter die Splits an beide Tabs durchreicht und im Marktwert-Modus nur an
Gewinne/Verluste (dort existiert der Dividenden-Tab nicht).

@note `tst_sharedetailsform` brauchte dafür `ShareSplitObject.cpp` neu in
seiner Quellenliste in `tests/forms/CMakeLists.txt` — die Fakes konstruieren
echte `ShareSplitObject`-Instanzen. `ShareSplitAdjuster.cpp`/
`ShareSplitHint.cpp` bleiben bewusst draussen: Umrechnung und Marker-Texte
liegen in der View, die hier Fake ist. Das Target bleibt damit das
schlankste im Projekt (keine Widgets, keine Datenbank).

PresenterShareDetails — Split-Übergabe (Phase 3c, 11.08.2026):
| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_loadAndDisplay_depotwertMode_passesSplitsToGewinneVerlusteAndDividenden` | Depotwert-Modus mit einem 20:1-Split | Beide Tabs erhalten die Splitliste |
| `test_loadAndDisplay_marketValueMode_passesSplitsToGewinneVerluste` | Marktwert-Modus | Gewinne/Verluste erhält die Splits, Dividenden-Tab wird gar nicht aufgerufen |
| `test_loadAndDisplay_withoutSplits_passesEmptySplitList` | Aktie ohne Splits | Leere Liste statt Sonderbehandlung |

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_loadAndDisplay_shareNotFound_showsErrorAndCloses` | Unbekannte GUID | `loadAndDisplay()` liefert `false`, `showError()`/`closeDialog()` aufgerufen, keine Boxen befüllt |
| `test_loadAndDisplay_validShare_setsHeaderAndStatusLine` | Gültige Aktie, kein Internet-Update | Fenstertitel = Aktienname, StatusLine enthält "noch nicht aktualisiert" und "Aktie" |
| `test_loadAndDisplay_lastInternetUpdateSet_appearsInStatusLine` | ISO-8601-Eingabe (`"2026-07-09T20:34:00"`, wie in der DB gespeichert) | StatusLine enthält den über `QLocale::ShortFormat` formatierten Wert, **nicht** den rohen ISO-String; enthält "Fonds" |
| `test_loadAndDisplay_malformedInternetUpdate_fallsBackToRawString` | Nicht als ISO 8601 parsbarer String | StatusLine zeigt den Rohwert (Fallback statt Verschwinden) |
| `test_loadAndDisplay_lastPriceUpdateSet_appearsInWebsiteUpdateLine` | ISO-8601-Eingabe (`"2026-07-10T11:53:00"`) | "Letzte Website-Aktualisierung"-Zeile enthält den lokal formatierten Wert, nicht den rohen ISO-String |
| `test_loadAndDisplay_noPriceUpdate_websiteUpdateLineShowsPlaceholder` | `lastPriceUpdate()` leer | "Letzte Website-Aktualisierung"-Zeile zeigt "noch nicht aktualisiert" |
| `test_loadAndDisplay_gesamtBox_mapsShareValuesFieldsDirectly` | `ShareValues` mit den Depotwert-Screenshot-Werten (40 Stk., 484,40€, ...) | "Aktueller Bestandswert"/"Verkäufe"/"Alle Einzahlungen"/"Gewinn / Verlust (gesamt)"/"Entwicklung" exakt wie im Screenshot, Farbe grün, `emphasize` auf "="-Zeilen |
| `test_loadAndDisplay_gesamtBox_negativeProfitLoss_setsRedColor` | `completeProfitLoss < 0` | Farbe = `Qt::red` |
| `test_loadAndDisplay_vortagBox_computesProfitLossFromVolumeTimesDiff` | `volume=40`, `prevDayDiff=41,90` | "Gewinn / Verlust" = 1676,00€ (40×41,90, exakt wie Screenshot), Farbe grün |
| `test_loadAndDisplay_vortagBox_negativeDiff_setsRedColor` | `prevDayDiff < 0` | Farbe = `Qt::red`, Produkt korrekt negativ |
| `test_loadAndDisplay_aktuelleBox_sumAddsCurValueDividendAndSaleProfitLoss` | `curValue=19376`, `totalDividend=0`, `saleProfitLossFinal=-252,20` | "Summe" = 19123,80€ (Presenter-Arithmetik, kein `ShareCalculator`-Feld) |
| `test_loadAndDisplay_marketMode_setsTabTitle` | `marketValueMode = true` | Tab-Titel = "Komplette Marktbewertung" |
| `test_loadAndDisplay_depotwertMode_setsTabTitle` | `marketValueMode = false` (Default) — Regression | Tab-Titel = "Komplette Depotbewertung" |
| `test_loadAndDisplay_marketMode_gesamtBox_matchesScreenshotValues` | `ShareValues` mit den echten Marktwert-Screenshot-Werten (AGIF-Allianz, 168,50796 Stk.) | "Verkäufe"/"Summe"/"Alle Einzahlungen"/"Gewinn / Verlust (gesamt)"/"Entwicklung" exakt wie im Screenshot; "Dividenden" = "-", Farbe `Qt::gray` |
| `test_loadAndDisplay_marketMode_aktuelleBox_matchesScreenshotValues` | Gleiche Fixture | "Gewinn / Verlust (Verkäufe)" = `saleProfitLoss`, "Summe" = `marketValue`, beide exakt wie im Screenshot; "Dividenden" deaktiviert |
| `test_loadAndDisplay_marketMode_vortagBox_unaffectedByMode` | Gleiche Fixture, Marktwert-Modus | "Gewinn / Verlust" = 53,59€ (168,50796×0,318, exakt wie im Screenshot) — bestätigt, dass die Vortag-Box modus-unabhängig ist |
| `test_loadAndDisplay_marketMode_populatesOnlyGewinneVerluste` (ergänzt 14.07.2026, ersetzt `..._doesNotPopulateNewTabs`) | `marketValueMode = true`, Model liefert 2 Sales/1 Dividend/3 Brokerages | `gewinneVerlusteCalled = true`, `saleRows.size() == 2`; `dividendenCalled`/`kostenCalled` bleiben `false`, `dividendRows`/`brokerageRows` bleiben leer — Gewinne/Verluste existiert seit 14.07.2026 in beiden Modi, Dividenden/Kosten bleiben Depotwert-only (siehe ARCHITECTURE.md, "Marktwert- vs. Depotwert-Modus") |
| `test_loadAndDisplay_depotwertMode_populatesGewinneVerlusteDividendenKosten` | `marketValueMode = false` (Default), gleiche Fixture | Alle drei `*Called`-Flags `true`, `saleRows.size() == 2`/`dividendRows.size() == 1`/`brokerageRows.size() == 3` — reines Durchreichen der Model-Listen, keine Presenter-Logik |

**"Aktie sollte aktualisiert werden!"-Warnzeile (ergänzt 30.07.2026)** —
`previousBusinessDay()`/`needsUpdateWarning()` sind `public static` und
werden direkt mit festen Datums-/Enum-Kombinationen getestet (feste
Referenzwoche 03.–09.08.2026, Montag–Sonntag), unabhängig von
`QDate::currentDate()`:

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_previousBusinessDay_monday_returnsPreviousFriday` | Montag 03.08.2026 | Ergebnis = Freitag 31.07.2026 (überspringt das ganze Wochenende) |
| `test_previousBusinessDay_tuesday_returnsMonday` | Dienstag 04.08.2026 | Ergebnis = Montag 03.08.2026 |
| `test_previousBusinessDay_wednesday_returnsTuesday` | Mittwoch 05.08.2026 | Ergebnis = Dienstag 04.08.2026 |
| `test_previousBusinessDay_thursday_returnsWednesday` | Donnerstag 06.08.2026 | Ergebnis = Mittwoch 05.08.2026 |
| `test_previousBusinessDay_friday_returnsThursday` | Freitag 07.08.2026 | Ergebnis = Donnerstag 06.08.2026 |
| `test_previousBusinessDay_saturday_returnsFriday` | Samstag 08.08.2026 | Ergebnis = Freitag 07.08.2026 |
| `test_previousBusinessDay_sunday_returnsFriday` | Sonntag 09.08.2026 | Ergebnis = Freitag 07.08.2026 (überspringt Samstag) |
| `test_needsUpdateWarning_marketPriceOnly_neverWarns` | `ShareUpdateType::MarketPrice`, sowohl ungültiges als auch weit zurückliegendes Datum | `false` in beiden Fällen — bewusste Einstellung, kein Datenproblem |
| `test_needsUpdateWarning_none_neverWarns` | `ShareUpdateType::None`, ungültiges Datum | `false` |
| `test_needsUpdateWarning_dailyValues_noData_warns` | `ShareUpdateType::DailyValues`, ungültiges Datum (keine Tageswerte vorhanden) | `true` |
| `test_needsUpdateWarning_both_noData_warns` | `ShareUpdateType::Both`, ungültiges Datum | `true` |
| `test_needsUpdateWarning_dataExactlyOnPreviousBusinessDay_noWarning` | Heute = Dienstag 04.08.2026, Tageswert genau vom Montag 03.08.2026 (letzter Werktag) | `false` — Grenzfall `>=`, nicht `>` |
| `test_needsUpdateWarning_dataOneBusinessDayOlderThanThreshold_warns` | Gleiches "heute", Tageswert vom Freitag 31.07.2026 (einen Werktag zu alt) | `true` |
| `test_needsUpdateWarning_dataFromToday_noWarning` | Tageswert vom selben Tag wie "heute" | `false` |
| `test_loadAndDisplay_dailyValuesUpdateType_noData_setsUpdateWarningText` | `updateType = DailyValues`, keine Tageswerte (`latestDailyValueDateResult` bleibt ungültig) | `view.updateWarning` enthält exakt "Aktie sollte aktualisiert werden! Daten sind evtl. nicht auf dem aktuellen Stand." |
| `test_loadAndDisplay_marketPriceOnlyUpdateType_noWarningRegardlessOfData` | `updateType = MarketPrice`, keine Tageswerte | `view.updateWarning` bleibt leer |
| `test_loadAndDisplay_dailyValuesUpdateType_freshData_noWarning` | `updateType = Both`, `latestDailyValueDateResult = QDate::currentDate()` | `view.updateWarning` bleibt leer — deterministisch unabhängig vom tatsächlichen Testdatum, da "heute" nie älter als der letzte Werktag vor "heute" sein kann |

@note **`lastInternetUpdate()`-Zweig (erledigt 09.07.2026):** `ShareObject`
besitzt `setLastInternetUpdate(const QString&)` — der zuvor als offen
markierte Test ist jetzt oben in der Tabelle enthalten.

@note **`setWebsiteUpdateLine()`/`lastPriceUpdate()` (erledigt 10.07.2026):**
Von Nessie bestätigt — "Letzte Website-Aktualisierung" ist der Zeitpunkt der
letzten Marktwert-/Kurs-Aktualisierung, `lastPriceUpdate()` ist damit das
richtige Feld. Zwei Tests oben decken den gesetzten und den leeren Fall ab.

@note **Länderschema-Bugfix (11.07.2026):** `lastInternetUpdate()`/
`lastPriceUpdate()` lieferten den rohen ISO-8601-String statt eines
länderschema-formatierten Werts (z. B. `"2026-07-11T00:45:00"` statt
`"11.07.2026 00:45"`) — Nessie ist das im Dialog aufgefallen. Behoben über
einen `formatDateTime()`-Helfer (`QLocale().toString(dt, QLocale::ShortFormat)`,
gleiche Konvention wie `BuyObject::dateAsStr()` usw.). Drei Tests oben decken
den formatierten Normalfall sowie den Fallback bei nicht-ISO-Strings ab.

@note **`tst_mainwindow.cpp` (erledigt 09.07.2026):** Drei Tests decken
`ViewShareDetails`/`onPortfolioRowDoubleClicked()` auf MainWindow-Seite ab —
siehe eigener Abschnitt weiter unten unter "tests/forms/ — MainWindow" bzw.
direkt in `tst_mainwindow.cpp` (Suchbegriff `onPortfolioRowDoubleClicked`/
`shareDetailsDialog`):

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_onPortfolioRowDoubleClicked_nullItem_doesNotCrash` | Doppelklick-Slot mit `item == nullptr` | Kein Absturz |
| `test_onPortfolioRowDoubleClicked_emptyGuid_doesNotCrash` | Zeile mit geleerter GUID (Qt::UserRole) | Kein Absturz, kein modaler Dialog |
| `test_shareDetailsDialog_validShare_constructsAndShowsCloseButtonText` | `ViewShareDetails` direkt konstruiert (kein `exec()`, analog `test_shareAddDialog_canBeConstructed`) | `hasValidShare()` = true, Fenstertitel = Aktienname, Close-Button-Text = "Schließen" (Regressionstest für den qtbase-Übersetzungs-Bugfix vom 09.07.2026) |
| `test_mainTabChanged_resetsOverviewTabsToUebersicht` (ergänzt 14.07.2026) | Zwei `insertTestBuy()`-Aufrufe in verschiedenen Jahren (erzeugen je einen Brokerage-Eintrag, siehe `insertTestBuy()`) befüllen den Kosten-Tab mit zwei Jahres-Tabs; ein Jahres-Tab wird ausgewählt, dann das äußere `m_tabs` gewechselt | Das per `findChildren<OverviewTabWidget*>()` (über `count() > 1` identifizierte) Kosten-`OverviewTabWidget` springt bei jedem Wechsel des äußeren Tabs zurück auf `currentIndex() == 0` (Übersicht) — Regressionstest für `ViewShareDetails::onMainTabChanged()`, siehe ARCHITECTURE.md, "OverviewTabWidget-Details" |
| `test_shareDetailsGewinneVerluste_tabChange_selectsFirstRowInJahresTab` (ergänzt 19.07.2026) | Verkauf mit einem Jahr (2024) angelegt, Gewinne/Verluste-`OverviewTabWidget` per `overviewTabByGroupTitle()` gefunden, `setCurrentIndex(1)` (2024-Jahres-Tab) | `ViewShareDetails::wireOverviewTab()` selektiert automatisch Zeile 0 — `tbl->selectedItems()` nicht leer, `tbl->currentRow()` = 0 |
| `test_shareDetailsGewinneVerluste_rowClick_emitsRowActivatedWithDocumentPath` (ergänzt 19.07.2026) | Verkauf mit Dokumentpfad angelegt, Klick auf eine Spalte (nicht die Dokument-Spalte) der Jahres-Tab-Zeile | `OverviewTabWidget::rowActivatedWithDocument` feuert 1× mit dem korrekten Dokumentpfad — Regressionstest für den ersetzten Doppelklick-Mechanismus |
| `test_shareDetailsGewinneVerluste_tabChange_backToUebersicht_clearsSelection` (ergänzt 19.07.2026) | Kosten-`OverviewTabWidget`: Jahres-Tab ausgewählt (Zeile automatisch selektiert), dann zurück zur Übersicht (`setCurrentIndex(0)`) | Jahres-Tab-Selektion ist danach leer (`tbl->selectedItems().isEmpty()`) |
| `test_marketMode_hasOnlyGewinneVerlusteOverviewTab` (ergänzt 14.07.2026) | `ViewShareDetails` im Marktwert-Modus konstruiert | Äußerer Tab-Titel "Gewinne/Verluste" vorhanden, "Dividenden"/"Kosten" nicht; genau eine `OverviewTabWidget`-Instanz statt drei |
| `test_depotwertMode_hasAllThreeOverviewTabs` (ergänzt 14.07.2026, Regression) | `ViewShareDetails` im Depotwert-Modus (Default) konstruiert | Alle drei äußeren Tab-Titel vorhanden, genau drei `OverviewTabWidget`-Instanzen |
| `test_marketMode_gewinneVerlusteTab_usesBrokerageFreeValues` (ergänzt 14.07.2026, deckte am 15.07.2026 einen unabhängigen `ModelSaleEdit::addSale()`-Bug auf, s.u.) | Ein Verkauf (5 Stk. à 100,00 €) mit eigener Provision (10,00 €), über `ModelSaleEdit::addSale()` real in die DB eingefügt; Gewinne/Verluste-`OverviewTabWidget` per `overviewTabByGroupTitle()`-Helfer (QGroupBox-Titel statt Index) gefunden | Depotwert-Modus zeigt "Auszahlung" = 490,00 € (500,00 € − 10,00 € Provision, über `payoutBrokerageReduction()`); Marktwert-Modus zeigt 500,00 € (brokeragefrei, über `payout()`) — für **dieselben** DB-Daten, direkter Beleg für die `market ? ... : ...`-Umschaltung in `ViewShareDetails::populateGewinneVerluste()` |
| `test_modelSaleEdit_addSale_success` (erweitert 15.07.2026) | Verkauf mit Provision 9,90 € über `addSale()` gespeichert | Zusätzlich zu `orderNumber()`: `loaded.first().provision() == 9.90` nach `loadSales()` — Regression für den Brokerage-Vorwärts-Link-Bugfix (s.u.) |
| `test_modelSaleEdit_addSale_linksBrokerageForwardReference` (ergänzt 15.07.2026) | Verkauf ohne Kauf-Anteil, Provision 10,00 € | `payoutBrokerageReduction()` nach `loadSales()` = 490,00 € (nicht 500,00 €, wie vor dem Fix); `loadBrokerage()` (Rückwärts-Link) weiterhin unverändert korrekt |
| `test_modelSaleEdit_updateSale_createsBrokerageIfMissing` (erweitert 15.07.2026) | Provision 15,00 € beim `updateSale()`-Aufruf, der einen fehlenden Brokerage-Eintrag neu anlegt | Zusätzlich zu `loadBrokerage().provision() == 15.0`: `loadSales()` liefert dieselbe Provision — Regression für denselben Bugfix im `updateSale()`-"Brokerage neu anlegen"-Zweig |

`SaleRepository::updateBrokerageGuid()` selbst hat zusätzlich einen isolierten
Repository-Unit-Test, `test_updateBrokerageGuid` in
`tests/repositories/tst_salerepository.cpp` (ergänzt 15.07.2026): Brokerage
wird absichtlich zunächst nur über den Rückwärts-Link (`sale_guid`) angelegt
— `findByGuid(guid).provision()` liefert davor 0, nach
`updateBrokerageGuid()` korrekt 7,5.

@note **Mausrad-Steuerung der "Anzahl" (ergänzt 12.07.2026):** Da
`ViewChart` (anders als `PresenterChart`) echte `QWidget`s und Qt-Charts
instanziiert, lebt der Regressionstest dafür in `tst_mainwindow.cpp`
(kompiliert `ViewShareDetails.cpp` → `ViewChart.h` ohnehin bereits, siehe
"Abhängigkeiten" unten), nicht in `tst_chartform` — analog zur Trennung
zwischen View- und Presenter-Tests bei den übrigen Formularen.

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_chartWheel_overCountSpinAndChartView_changesIntervalCountAndRefreshes` | Ein `QWheelEvent` (positives `angleDelta().y()`) wird per `QCoreApplication::sendEvent()` erst an `countSpin` (ohne vorherigen Fokus-Aufbau) und dann an `chartView`-Viewport geschickt | Beide Wege erhöhen `intervalCount()` (`m_countSpin->value()`) um 1 und lösen über die bestehende `valueChanged()`-Verbindung einen Refresh aus (Chart-Daten ändern sich sichtbar, z. B. via `findChild<QSpinBox*>("countSpin")->value()`) |

Bewusst weiterhin **nicht** getestet: der "gültige GUID → `dlg.exec()`"-Pfad
in `onPortfolioRowDoubleClicked()` selbst — ein echter modaler `QDialog::exec()`
würde den (headless) Testlauf blockieren, exakt dieselbe Konvention wie bei
`onEditShare()`/`onDeleteShare()` in derselben Datei.

@note **`onPortfolioRowRightClicked()`/`ChartPopup` (ergänzt 31.07.2026,
Feature "ChartPopup — Rechtsklick-Popup-Chart", siehe ARCHITECTURE.md):**
Gleiche Guard-only-Konvention wie bei `onPortfolioRowDoubleClicked()` oben,
mit einer wichtigen Abweichung: `ChartPopup::showAt()` ruft `show()`, nicht
`exec()` — nicht-blockierend. Der "gültige GUID"-Pfad ist deshalb **nicht**
über ein blockierendes Dialogproblem ausgeschlossen; er wird stattdessen
bewusst über eine direkte `ChartPopup`-Konstruktion (ohne `show()`/`showAt()`)
abgedeckt, um von echtem On-Screen-Fenster-/Cursor-Verhalten im headless
Testlauf unabhängig zu bleiben.

Wichtig für die beiden Guard-Tests: `onPortfolioRowRightClicked()` ermittelt
die auslösende Tabelle über `sender()` (siehe ARCHITECTURE.md,
"MainWindow-Verdrahtung" im ChartPopup-Abschnitt) — ein direkter
`QMetaObject::invokeMethod()`-Aufruf auf den Slot (wie bei den
`onPortfolioRowDoubleClicked()`-Tests oben) würde `sender() == nullptr`
liefern und jeden Guard trivial bestehen lassen, ohne die
`itemAt()`/GUID-Logik tatsächlich zu prüfen. Die Tests rufen daher das
generierte Signal direkt auf (`tbl->customContextMenuRequested(pos)`), was
eine echte Emission mit korrekt gesetztem `sender()` auslöst.

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_onPortfolioRowRightClicked_noItemAtPos_doesNotCrash` | `customContextMenuRequested(QPoint(5,5))` auf der leeren Datentabelle (0 Zeilen) | Kein Absturz, kein Popup |
| `test_onPortfolioRowRightClicked_emptyGuid_doesNotCrash` | Zeile mit geleerter GUID (Qt::UserRole), Signal auf die reale Zeilenposition (`visualItemRect(item).center()`) emittiert | Kein Absturz, kein Popup |
| `test_chartPopup_validShare_constructsWithChartChild` (erweitert 31.07.2026 um die Überschriften-Prüfung) | `ChartPopup` direkt konstruiert (kein `show()`/`showAt()`) | `findChild<ViewChart*>("ViewChart")` nicht null; dessen `findChild<QGroupBox*>("selektionBox")` ist `isHidden() == true` (Compact-Modus, siehe ARCHITECTURE.md); `findChild<QLabel*>("chartPopupHeader")` enthält den übergebenen Aktiennamen |
| `test_onPortfolioRowRightClicked_validGuid_popupCenteredAndNarrowerThanMainWindow` (ergänzt 31.07.2026, überarbeitet nach mehreren Rückmeldungen — zuletzt "horizontal zentriert ... Hauptfensterbreite − 50px, also auf jeder Seite 25px schmäler"; **Bugfix 02.08.2026**, siehe ARCHITECTURE.md) | Echter Rechtsklick (`customContextMenuRequested`) auf eine gültige Zeile — `MainWindow` wird dafür bewusst relativ zur verfügbaren Bildschirmgeometrie dimensioniert/positioniert (`QGuiApplication::primaryScreen()->availableGeometry()`), nicht fest auf 900×600. Das reicht auf einem schmalen Bildschirm (< Fenster-Mindestbreite 900px + 50px, z. B. der 800px breite CI-Offscreen-Runner) aber nicht aus: `MainWindow` kann wegen `setMinimumSize(900, 600)` nicht darunter schrumpfen, das Popup (`window.width() − 50`) wird dadurch breiter als der verfügbare Bildschirm — eine exakte Zentrierung ist dann unmöglich. Der Test berechnet deshalb dieselbe `avail`-Geometrie wie `ChartPopup::showAt()` und unterscheidet explizit beide Fälle, statt die komplette Klemm-Formel zu duplizieren | `ChartPopup` wird über `QApplication::topLevelWidgets()` gefunden (kein Kind-Widget von `MainWindow`, da ownerlos erzeugt); `width() == window.width() - 50`; passt das Popup auf den verfügbaren Bildschirm (`popup->width() <= avail.width()`, jeder reale Desktop): Popup-Mittelpunkt (`x() + width()/2`) == Hauptfenster-Mittelpunkt in globalen Koordinaten; passt es nicht (z. B. schmaler CI-Runner): `popup->x() == avail.left()` (Linksklemmung) — Regressionstest für `MainWindow::onPortfolioRowRightClicked()`'s Breiten-/Positionsberechnung vor `showAt()` |
| `test_onReferenceLineHovered_fractionalVolume_showsFourDecimals` (**Bugfix 02.08.2026**, siehe ARCHITECTURE.md) | `ChartPopup` direkt konstruiert, `ViewChart`-Kindwidget per `findChild()` geholt, `onReferenceLineHovered()` (seit diesem Bugfix `private slots:`) per `QMetaObject::invokeMethod()` direkt mit einer `ChartReferenceLine` aufgerufen (`volume = 1.5`, bewusst eine Bruchstückzahl — genau der Fall aus Nessies Screenshot) | `QToolTip::text()` enthält `"1,5000 Stk."`, nicht mehr `"1 Stk."` (0 Nachkommastellen) |
| `test_onSeriesHovered_heldVolumeSeries_fractionalValue_showsFourDecimals` (**Bugfix 02.08.2026**, siehe ARCHITECTURE.md) | Gleiches Vorgehen, `onSeriesHovered()` mit `SeriesKind::HeldVolume` und `QPointF(0.0, 12.3456)` aufgerufen | `QToolTip::text()` enthält `"12,3456"` |

@note **Warum diese beiden Tests in `tst_mainwindow.cpp` statt in
`tst_chartform.cpp` (ergänzt 02.08.2026):** `tst_chartform.cpp` testet
bewusst ausschließlich `PresenterChart` über ein Fake-View/Fake-Model-Paar
(kein echtes `ViewChart`/`QChartView`, siehe dortiger Datei-Header-Kommentar)
— die Tooltip-Formatierung sitzt aber ausschließlich in der echten
`ViewChart`-Klasse. `onSeriesHovered()`/`onReferenceLineHovered()` sind
deshalb seit diesem Bugfix als `private slots:` deklariert (`ViewChart.h`,
reine Testbarkeits-Maßnahme, kein Verhaltensunterschied — die Verbindung
selbst läuft weiterhin über eine Lambda in `setChartData()`/
`setReferenceLines()`), damit sie hier per `QMetaObject::invokeMethod()`
direkt aufgerufen werden können, statt ein reales Maus-Hover über die im
headless Testlauf nicht verlässlich vermessbare Chart-Zeichenfläche zu
simulieren.


Nessies Rückmeldung "Dialog geht zu, auch wenn die Maus noch auf dem Dialog
ist" — siehe ARCHITECTURE.md, "ChartPopup"):** Bewusst **nicht** durch einen
automatisierten Test abgedeckt. Der Fix prüft `QCursor::pos()` gegen die
tatsächliche Bildschirmgeometrie des Popups — eine verlässliche
automatisierte Prüfung bräuchte eine echte, plattformabhängige
Cursor-Bewegungssimulation (`QCursor::setPos()` verhält sich auf der
Offscreen-QPA-Plattform, mit der die Testsuite läuft, nicht zuverlässig
gleich wie auf einer echten Anzeige), die keine belastbare Aussage liefern
würde. Manuell durch Nessie verifiziert.

---

#### tst_overviewtabwidget — OverviewTabWidget (implementiert 14.07.2026)

Executable: `tst_overviewtabwidget`
Klasse unter Test: `OverviewTabWidget` (echtes `QWidget`, kein Presenter)

@note Bewusste Ausnahme vom Fake-View/Fake-Model-Muster der übrigen
Form-Tests (siehe `tst_sharedetailsform`/`tst_chartform`): `OverviewTabWidget`
hat keinen eigenen Presenter, es ist ein wiederverwendbares, in sich
geschlossenes Anzeige-Widget (Spaltendefinitionen + Populate-Callbacks als
reine `std::function`-Argumente, siehe ARCHITECTURE.md,
"OverviewTabWidget-Details"). Getestet wird daher die echte `QWidget`-
Instanz direkt, analog zu `tst_backupsettingsform` (dort ebenfalls ein
echter `QDialog` statt einer Fake-Schicht). Klick-Signale (`QTabBar::
tabBarClicked`, `QTableWidget::cellClicked`) werden direkt als
Funktionsaufruf ausgelöst (z.B. `yearsBar->tabBarClicked(1)`) statt über
echte Maus-Events — beide sind öffentliche Qt-Signale, ein direkter Aufruf
verhält sich identisch zu einem realen Klick, ohne Tab-Rect-Berechnungen im
Test. Keine Datenbank nötig.

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_populateOverview_countWidgetTabText` | 2 Jahre (2025, 2024) | `count()` = 3, `widget(0..2)` nicht null, `widget(3)` null, `tabText(0)` = "Übersicht", `tabText(1/2)` = "2025"/"2024" |
| `test_populateOverview_emptyYears_leavesNoTabs` | Leere Jahres-Liste | `count()` = 0 |
| `test_setCurrentIndex_switchesStackAndBothBars` | `setCurrentIndex(2)` dann `setCurrentIndex(0)` | `currentIndex()`, `yearsBar->currentIndex()` und `pinnedBar->currentIndex()` (immer 0) synchron |
| `test_setCurrentIndex_outOfRange_isIgnored` | `setCurrentIndex(99)` bzw. `(-1)` | Aktueller Index bleibt unverändert |
| `test_pinnedTabClick_afterYearTabClick_returnsToOverview` | Regressionstest Bugfix 1: Klick auf Jahres-Tab, dann Klick auf Übersicht-Tab | `currentIndex()` wechselt zurück auf 0 — vor dem Fix (`currentChanged` statt `tabBarClicked`) blieb der Übersicht-Tab unanwählbar, da `m_pinnedBar` nur einen, nie wechselnden Index hat |
| `test_yearsBarClick_sameYearAsBefore_stillSwitchesBack` | Jahres-Tab anwählen, zurück zur Übersicht, denselben Jahres-Tab erneut anklicken | Wechsel funktioniert trotz unverändertem `yearsBar`-eigenem Index |
| `test_headerColumns_alwaysBold_regardlessOfSelection` | Regressionstest Bugfix 2 | Spaltenköpfe fett **vor und nach** `selectRow()`, `highlightSections() == false` |
| `test_uebersichtRowClick_jumpsToMatchingYearTab` | Klick auf Übersicht-Zeile mit Jahr 2024 | `currentIndex()` wechselt zum passenden Jahres-Tab (Index 2) |
| `test_uebersichtRowClick_unknownYear_doesNothing` | Klick auf eine künstliche Zeile mit einem nicht vorhandenen Jahr | `currentIndex()` bleibt unverändert |
| `test_clear_removesAllTabsAndResetsCount` | `clear()` nach `populateOverview()` | `count()` = 0, beide `QTabBar`s leer |
| `test_populateOverview_calledTwice_replacesOldTabs` | Zweiter `populateOverview()`-Aufruf mit nur einem Jahr | `count()` = 2 (nicht 3+2) — alte Tabs werden vollständig ersetzt, nicht angehängt |
| `test_populateOverview_dataTablesHaveGridSelectionStyle` | Grid-Selektionsfarbe (Feature 29.07.2026) | `dataTable->styleSheet()` jedes Tabs (Übersicht + Jahre) enthält `GridStyle::kSelectionBackground` und `kSelectionForeground` |
| `test_populateOverview_footerTableHasNoGridSelectionStyle` | footerTable ist `NoSelection` | `footerTable->styleSheet()` enthält die Selektionsfarbe NICHT |

**Grid-Selektionsfarbe (ergänzt 29.07.2026, siehe ARCHITECTURE.md,
"GridStyle — App-weite Grid-Selektionsfarbe"):** `OverviewTabWidget` deckt
über `buildFrozenTable()` automatisch alle fünf Edit-Dialoge sowie
`ViewShareDetails` ab, daher genügen die beiden Tests oben für den gesamten
Dialog-Bereich der App. Für die zwei Haupttabellen in `MainWindow`
(Depotwert/Marktwert) existieren die analogen Tests
`test_mainWindow_portfolioTables_haveGridSelectionStyle` und
`test_mainWindow_portfolioFooters_haveNoGridSelectionStyle` in
`tst_mainwindow.cpp` (prüfen `m_finalValueTable`/`m_marketValueTable` auf
dieselben `GridStyle`-Konstanten bzw. die beiden Footer-Tabellen explizit auf
deren Abwesenheit, da nicht selektierbar — kein Seeding nötig, der Stil wird
unabhängig von Daten bereits in `setupCentralWidget()` gesetzt).

`rowActivatedWithDocument()` / `documentActivated()` (ergänzt 19.07.2026,
siehe ARCHITECTURE.md, "ShareDetailsForm: Dokument-Vorschau per Zeilenauswahl
statt Doppelklick"): eigene `populateSampleWithDoc()`-Variante mit einer
dritten Spalte als konfigurierter Dokument-Spalte (`jahresDocColumn = 2`).

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_jahresRowClick_withDocColumn_emitsRowActivatedWithDocumentAndPath` | Klick auf Spalte 1 (nicht die Dokument-Spalte) einer Zeile mit Dokument | `rowActivatedWithDocument` feuert 1×, `userData` = GUID der Zeile, Pfad = Dokumentpfad aus Spalte 2 |
| `test_jahresRowClick_rowWithoutDocument_emitsEmptyPath` | Klick auf eine Zeile ohne Dokument (leerer Pfad in der Dokument-Spalte) | Signal feuert trotzdem, Pfad ist leer |
| `test_jahresRowClick_noDocColumnConfigured_emitsEmptyPath` | Klick in einem Tab ohne konfigurierte Dokument-Spalte (`populateSample()`, `jahresDocColumn` = Default -1) | Signal feuert, Pfad ist leer |
| `test_jahresRowClick_stillEmitsPlainRowActivated` | Derselbe Klick löst weiterhin auch `rowActivated()` aus | Beide Signale sind unabhängig voneinander aktiv |
| `test_documentColumnDoubleClick_stillEmitsDocumentActivated` | Regression: Doppelklick auf die Dokument-Spalte | `documentActivated(path)` feuert weiterhin 1× mit korrektem Pfad — dieser Mechanismus wurde beim Nachziehen von `rowActivatedWithDocument()` versehentlich kurz entfernt (brach den Build von `ViewBuyEdit` & Co.), seither bewusst als Ergänzung statt als Ersatz umgesetzt |
| `test_documentColumnDoubleClick_emptyPath_doesNotEmitDocumentActivated` | Doppelklick auf die Dokument-Spalte einer Zeile ohne Dokument | Signal feuert nicht |
| `test_documentColumnDoubleClick_wrongColumn_doesNotEmitDocumentActivated` | Doppelklick außerhalb der Dokument-Spalte | Signal feuert nicht |

`ViewShareDetails::onMainTabChanged()` ist kein `OverviewTabWidget`-Verhalten mehr, sondern lebt
in `ViewShareDetails` und braucht eine echte Dialog-Instanz mit Mehrjahres-
Testdaten — dafür `test_mainTabChanged_resetsOverviewTabsToUebersicht` in
`tst_mainwindow.cpp` statt dieses schlanken Test-Targets (siehe
"tests/forms/ — MainWindow" oben).

---

#### tst_chartform — ChartForm (implementiert 12.07.2026)

Executable: `tst_chartform`
Klasse unter Test: `PresenterChart`

@note Gleiches Fake-View/Fake-Model-Muster wie `tst_sharedetailsform`: kein
`QWidget`, keine Qt-Charts-Instanziierung, keine Datenbank.
`FakeModelChart::latestBuy()`/`latestSale()` geben direkt ein von Hand
befülltes `ChartReferenceInfo` zurück, ohne echte `BuyObject`/`SaleObject`-
Instanzen zu benötigen. `initTestCase()` setzt `QLocale::setDefault(QLocale::
German)` explizit, da dieses Test-Target (anders als die volle App)
`AppStartup.cpp` nicht mitkompiliert und die Zahlenformat-Assertions sonst
vom System-Locale der Baumaschine abhängen würden.

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_loadAndDisplay_noData_showsEmptyAndClearsRangeInfo` | `latestDailyValueDate()` liefert ungültiges `QDate` | `showEmptyChart()` aufgerufen, `setDefaultStartDate()` **nicht** aufgerufen, `setRangeInfo("")` |
| `test_loadAndDisplay_withData_setsDefaultStartDateToLatest` | Ein Tageswert vorhanden | `setDefaultStartDate()` erhält das späteste Datum aus `daily_values` |
| `test_refresh_defaultSelection_onlyClosingPriceSeries` | Default-Selektion der Fake-View (nur `ClosingPrice`) | `setChartData()` enthält genau eine Serie mit den `closingPrice()`-Werten, `axis == ChartAxis::Price` |
| `test_refresh_dayInterval_computesCorrectRangeStart` | `Interval=Day`, `Anzahl=5`, Start-Datum 10.07.2026 | Nur Tageswerte ab (inkl.) 05.07.2026 landen in der Serie — bestätigt `computeRangeStart()` |
| `test_refresh_heldVolumeSeries_usesModelValuesAndVolumeAxis` | `HeldVolume` selektiert | Serie übernimmt `ModelChart::heldVolumeSeries()`-Werte 1:1, `axis == ChartAxis::Volume` |
| `test_refresh_tradedVolumeSeries_usesDailyValuesVolumeAndVolumeAxis` | `TradedVolume` selektiert (ergänzt 12.07.2026) | Serie übernimmt `DailyValuesObject::volume()` direkt (kein Model-Aufruf nötig), `axis == ChartAxis::Volume` |
| `test_refresh_heldAndTradedVolume_shareSameVolumeAxis` | `HeldVolume` **und** `TradedVolume` gleichzeitig selektiert (in der echten UI durch Checkbox-Exklusivität unmöglich, siehe unten) | Beide Serien tragen `ChartAxis::Volume` — Presenter-Ebene kennt keine Exklusivität, das ist bewusst reine View-Logik |
| `test_refresh_noSeriesSelected_showsEmptyMessage` | Alle Selektions-Checkboxen deaktiviert | `showEmptyChart()` statt `setChartData()`, trotz vorhandener Tageswerte |
| `test_refresh_legendEntries_minMaxForClosingPrice` | Zwei Tageswerte (379,70€/422,40€) | Legende-Eintrag "Schluss-Kurs" enthält beide Werte in der Min/Max-Zeile |
| `test_refresh_legendEntries_lastBuyAndSaleReference` | `latestBuy`/`latestSale` gesetzt (12.05.2022, 198,36€ bzw. 27.02.2020, 205,25€) | Legende enthält "Letzter Kauf"/"Letzter Verkauf" mit Datum, Preis und Entwicklung (224,04€) relativ zum Range-Max-Schlusskurs |
| `test_refresh_noReferenceEntries_whenModelReturnsInvalid` | `latestBuy`/`latestSale` bleiben `ChartReferenceInfo{}` (ungültig) | Keine "Letzter Kauf"/"Letzter Verkauf"-Zeilen in der Legende |
| `test_refresh_referenceLines_latestBuyIsBlueOlderIsTurquoise` | Zwei Käufe im Zeitraum (`buysInRange`, mit Preis+Stückzahl), einer davon der global letzte (ergänzt 12.07.2026) | Global letzter Kauf → `Qt::blue`, älterer Kauf → `QColor(0, 170, 170)` (Türkis); `kind`/`price`/`volume` korrekt aus `ChartReferenceInfo` übernommen |
| `test_refresh_referenceLines_latestSaleIsRedOlderIsOrange` | Zwei Verkäufe im Zeitraum (`salesInRange`), einer davon der global letzte | Global letzter Verkauf → `Qt::red`, älterer Verkauf → `QColor(255, 140, 0)` (Orange); `kind == Sale`, `price` korrekt |
| `test_refresh_referenceLines_onlyDatesWithinComputedRange` | `Interval=Day`, `Anzahl=5`; zwei Käufe, nur einer im berechneten Fenster; zweiter, deutlich älterer Tageswert (01.05.2026) in der Fixture, damit die seit 12.07.2026 bestehende Anzahl-Kappung das Fenster nicht auf 1 zurückstutzt (siehe unten) | Nur das im Fenster liegende Datum erscheint in `setReferenceLines()` |
| `test_loadAndDisplay_noData_clearsReferenceLines` | Keine Tageswerte vorhanden | `setReferenceLines({})` wird trotzdem aufgerufen (leere Liste, kein veralteter Zustand) |
| `test_onControlsChanged_beforeAnyData_doesNotCrashOrRefresh` | `onControlsChanged()` ohne vorheriges `loadAndDisplay()` | Kein `setChartData()`/`showEmptyChart()`-Aufruf (internes `m_hasData`-Guard) |
| `test_onControlsChanged_afterLoad_reflectsNewIntervalCount` | Interval/Anzahl nach dem ersten Laden geändert | Serie wird bei erneutem `onControlsChanged()` mit dem neuen Zeitfenster neu berechnet |

Anzahl-Kappung (ergänzt 12.07.2026 auf Nessies Vorgabe, siehe
ARCHITECTURE.md "ChartForm-Details"): `FakeModelChart::earliestDailyValueDate()`
liefert das kleinste Datum über **alle** `m_dailyValues` hinweg (nicht auf
das gerade abgefragte Fenster beschränkt) — spiegelt damit exakt
`DailyValuesRepository::earliestDate()` gegen die volle Historie in der DB.
`FakeModelChart::lastQueryFrom`/`lastQueryTo` erfassen zusätzlich die zuletzt
an `loadDailyValues()` übergebene Spanne, um zu beweisen, dass wirklich die
*gekappte* Anfrage abgesetzt wird — nicht nur, dass das Ergebnis zufällig
gleich aussieht. `FakeViewChart::setMaxIntervalCount()` klemmt bewusst
**nicht** automatisch (anders als das echte `QSpinBox::setMaximum()`), damit
die Tests wirklich die Presenter-seitige `std::min()`-Klemmung prüfen, nicht
ein zufälliges Zusammenspiel mit View-Verhalten.

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_refresh_setsMaxIntervalCount_basedOnEarliestDailyValue` | Ältester Wert 01.07.2026, Start-Datum 10.07.2026, `Interval=Day` | `setMaxIntervalCount(9)` — 9 Tage zwischen ältestem Wert und Start-Datum |
| `test_refresh_intervalCountBeyondMax_clampsQueryToEarliestDate` | `Anzahl=50` angefordert, aber nur 9 Tage Historie vorhanden | `lastMaxIntervalCount == 9`; `FakeModelChart::lastQueryFrom == 01.07.2026` (nicht 22.05.2026, wie die ungekappte Anzahl 50 ergäbe) |
| `test_refresh_singleValueAtRangeEnd_maxIntervalCountStaysAtOne` | Einziger Tageswert exakt am Start-Datum selbst | `setMaxIntervalCount(1)` — nichts Älteres zu erreichen |
| `test_onControlsChanged_countAboveMax_clampsEffectiveQueryRange` | Nach `loadAndDisplay()` wird `Anzahl` per `onControlsChanged()` auf 50 gesetzt (simuliert Mausrad-/Spinbox-Event über die Grenze hinaus) | `lastMaxIntervalCount == 9`, `lastQueryFrom` bleibt auf die gekappte Spanne begrenzt |
| `test_refresh_monthIntervalNotLandingOnEarliestDate_maxIntervalCountStillReachesIt` | Bugfix-Regressionstest (12.07.2026): `Interval=Monat`, ältester Wert am 15. eines Monats, Start-Datum am 10. — die Monatsgrenze trifft den ältesten Wert NICHT exakt | `lastMaxIntervalCount == 6` (nicht 5, der alte Off-by-one-Wert), `lastQueryFrom == 10.01.2026`, ältester Wert (15.01.2026) ist tatsächlich in der `ClosingPrice`-Serie enthalten |
| `test_refresh_weekIntervalNotLandingOnEarliestDate_maxIntervalCountStillReachesIt` | Wie oben, für `Interval=Woche`: ältester Wert (03.06.2026) liegt nicht auf einer vollen 7-Tage-Grenze zum Start-Datum (10.07.2026) | `lastMaxIntervalCount == 6` (nicht 5), `lastQueryFrom == 29.05.2026`, ältester Wert enthalten |
| `test_refresh_yearIntervalNotLandingOnEarliestDate_maxIntervalCountStillReachesIt` | Wie oben, für `Interval=Jahr`: ältester Wert (15.03.2023) liegt nicht auf einer vollen Jahres-Grenze zum Start-Datum (10.07.2026, immer 10.07.) | `lastMaxIntervalCount == 4` (nicht 3), `lastQueryFrom == 10.07.2022`, ältester Wert enthalten |
| `test_refresh_monthIntervalLandingExactlyOnEarliestDate_maxIntervalCountDoesNotOvershoot` | Pendant zum Tag-Exakttreffer-Test, für `Interval=Monat`: ältester Wert (10.01.2026) trifft die Fenstergrenze bei Anzahl=6 exakt | `lastMaxIntervalCount == 6` (NICHT 7) — belegt, dass die korrigierte Schleife im Gleichheitsfall korrekt stoppt und nicht überschießt |
| `test_refresh_dayIntervalWithLongHistory_maxIntervalCountExceedsOldFixedCeiling` | Zweiter Bugfix-Regressionstest (12.07.2026): `Interval=Tag`, ältester Wert und Start-Datum liegen 3843 Tage auseinander (01.01.2016–10.07.2026) — deutlich mehr als die alte feste Grenze von 999 | `lastMaxIntervalCount == 3843` (NICHT 999), `lastQueryFrom == 01.01.2016`, ältester Wert enthalten — belegt die neue dynamische Grenze (`earliestDate.daysTo(rangeEnd)`, zusätzlich `kAbsoluteSafetyCeiling`-abgesichert) |
| `test_refresh_corruptEarliestDate_maxIntervalCountClampedByAbsoluteSafetyCeiling` | Korruptes/unplausibles Datum (Jahr -1000) als ältester Wert — `daysTo(rangeEnd)` läge bei ~1,1 Mio. Tagen | `lastMaxIntervalCount == 1000000` (exakt `kAbsoluteSafetyCeiling`, nicht die volle Tagesspanne) — belegt, dass die absolute Notbremse tatsächlich greift, nicht nur die dynamische Grenze |

Bewusst weiterhin **nicht** getestet: `ViewChart` selbst (QtCharts-Rendering,
Achsen-Rebuild, Legende-Layout, Hover-Tooltip via `onSeriesHovered()`,
Rendering der Kauf-/Verkauf-Markerlinien via `setReferenceLines()`) —
analog zur bestehenden Konvention, dass reine Qt-Widgets-Views ohne eigene
Logik nicht isoliert unit-getestet werden, solange der Presenter (der die
eigentliche Logik trägt) abgedeckt ist.

---

### tests/forms/ — SalesForm

@note Zur Teststruktur: Da `QTEST_MAIN` nur eine Testklasse unterstützt,
läuft `TestSalesForm` in einer eigenen `QObject`-Unterklasse. Ein gemeinsamer
`main()`-Einstiegspunkt ruft `QTest::qExec` für alle Klassen nacheinander auf
(aktuell: `TestMainWindow`, `TestSalesForm`, `TestDividendForm`, `TestOwnMessageBox`, `TestBackupForm`).

@note Stub-Pattern: `StubViewSaleEdit` und `StubModelSaleEdit` implementieren
die jeweiligen Interfaces ohne echte UI oder Datenbank.

`StubModelSaleEdit` implementiert alle Methoden von `IModelSaleEdit` inkl.
`loadAllBuys()` (gibt `availableBuys` zurück) und `loadBrokerageForBuy()`
(gibt `brokerage` zurück) — beide delegieren auf die konfigurierbaren
Stub-Member ohne DB-Zugriff. Seit Phase 2c der Aktiensplit-Behandlung
(07.08.2026) zusätzlich `loadAvailableBuysForDepotExcludingSale()` (gibt
ebenfalls `availableBuys` zurück, erfasst aber zusätzlich Aufruf und
übergebene GUID in `excludingSaleCalled`/`lastExcludeSaleGuid` — mutable,
da die Methode selbst `const` ist) und `loadSplits()` (gibt `splits`
zurück). `addSale()`/`updateSale()` erfassen das übergebene `SaleObject`
zusätzlich in `lastAddedSale`/`lastUpdatedSale`, damit Tests die tatsächlich
berechneten `SaleBuyDetails` prüfen können.

`StubViewSaleEdit` implementiert `setAllBuys()` als No-op — der Aufruf
durch den Presenter im Konstruktor wird damit ohne Seiteneffekt absorbiert. Seit
dem Bugfix zu den anteiligen Kauf-Nebenkosten (11.08.2026) implementiert er
zusaetzlich `showBuyDetails()` und legt den uebergebenen
`SaleBuyDetailSummary` in `lastBuyDetails` ab; `showBuyDetailsCallCount`
zaehlt die Aufrufe. Damit sind Zeilen und Summen des Details-Dialogs ohne
echte UI pruefbar — die Aufbereitung liegt seither im Presenter.
`setSplits()` speichert die übergebenen Splits in `m_splits`. `setKaufwert()`/
`setGewinnVerlust()` erfassen den zuletzt übergebenen Wert in
`lastKaufwert`/`lastGewinnVerlust` (Phase 2c, für Tests der Live-FIFO-Vorschau).

---

PresenterSaleEdit — anteilige Kauf-Nebenkosten (Bugfix 11.08.2026):
| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_presenterSaleEdit_onSave_fullyConsumedBuy_carriesCompleteBrokerage` | Kauf vollstaendig verbraucht | `brokeragePart` = 30,95 statt 0,0 |
| `test_presenterSaleEdit_onSave_partialSale_splitsBrokerageProportionally` | Ein Drittel des Kaufs verkauft | `brokeragePart` und `reductionPart` je ein Drittel |
| `test_presenterSaleEdit_onSave_brokerageIsNotScaledBySplit` | Kauf 10 Stk. vor 20:1, Verkauf 200 heutige | `volume` = 10 (Beleg-Skala), `brokeragePart` = 30,95 und NICHT 619,00 |
| `test_presenterSaleEdit_onShowDetails_liveBranch_reportsBuyCosts` | Details-Dialog im Live-FIFO-Zweig | Zeile und Summe tragen 30,95 statt 0,00 |
| `test_presenterSaleEdit_onShowDetails_profitLossSubtractsBuyCosts` | G/V-Summe des Dialogs | Kosten abgezogen, Rabatt gegengerechnet |
| `test_presenterSaleEdit_livePreview_gewinnVerlustIncludesBuyCosts` | Live-Vorschau im Formular | `lastGewinnVerlust` inkl. Kaufkosten, `lastKaufwert` weiterhin OHNE |

@note Der dritte Test ist der eigentliche Regressionsschutz. Ein Geldbetrag
darf nicht mit dem Split-Faktor skaliert werden; der Anteil
`detailVolume / buy.volume()` ist skaleninvariant, weil beide Werte in der
Beleg-Skala desselben Kaufs liegen (siehe ARCHITECTURE.md, "Anteilige
Kauf-Nebenkosten der FIFO-Zuteilung").

@note Der erste Test verwendet bewusst die Zahlen des Feldfalls
(Provision 29,20 + Handelsplatzgebuehr 1,75 = 30,95). Der Fehler war ueber
Unit-Tests nicht aufgefallen, sondern erst beim Vergleich zweier Datenbanken
— 48 historisch erfasste Verkaeufe trugen ihre Kosten korrekt, ein frisch
erfasster nicht.

@note Kein Testziel deckt `ViewSaleEdit::showBuyDetails()` selbst ab. Die
Methode rendert seit der Verlagerung nur noch; die Logik liegt im Presenter
und ist dort abgedeckt. Das entspricht der bestehenden Konvention, reine
Qt-Widgets-Views ohne eigene Logik nicht isoliert zu testen.

---

ViewSaleEdit / PresenterSaleEdit — Split-Marker und Summen (Phase 3c, 11.08.2026):
| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_viewSaleEdit_populateOverview_belegRowKeepsBelegVolumeWithMarker` | Verkauf 2021, Split 20:1 danach | Belegzeile bleibt bei 5,0000, trägt Marker und Tooltip |
| `test_viewSaleEdit_populateOverview_belegRowWithoutSplitHasNoMarker` | Aktie ohne Splits | Kein Marker, kein Tooltip |
| `test_viewSaleEdit_populateOverview_uebersichtRowUsesTodayScale` | Übersicht-Tab | Jahreszeile zeigt 100,0000 (heutige Skala) mit Marker |
| `test_viewSaleEdit_populateOverview_splitMidYearSumsOnOneScale` | Zwei Verkäufe 2022, Split dazwischen | Jahres-Fusszeile zeigt 200,0000 (5 × 20 + 100), nicht 105 |
| `test_presenterSaleEdit_populateOverview_passesSplitsAsParameter` | Presenter-Verdrahtung | Splits kommen als Parameter an |

@note Der vierte Test ist der eigentliche Punkt der Änderung. Die frühere rohe
Summe mischte zwei Stückelungen und war damit bedeutungslos — sichtbar wird
das nur, wenn ein Split MITTEN in ein Jahr fällt.

@note Die View-Tests greifen über die dynamischen Eigenschaften `dataTable`
und `footerTable` des Tab-Containers auf die Tabellen zu (Helfer
`dataTableOf()`/`footerTableOf()`). `OverviewTabWidget` erbt von `QWidget`,
nicht von `QTabWidget` — `currentWidget()` gibt es dort nicht.

---

ModelSaleEdit (Datenbanktests):
| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_modelSaleEdit_addSale_success` | Sale + Brokerage in Transaktion gespeichert | `loadSales()` gibt 1 Eintrag zurück |
| `test_modelSaleEdit_addSale_updatesVolumeSoldOnBuy` | `addSale()` erhöht `volumeSold` auf dem Kauf | `volumeSold` = 10.0 nach Verkauf von 10 Anteilen |
| `test_modelSaleEdit_addSale_rollsBackOnError` | Doppelte GUID → Rollback | Kein zweiter Sale in DB |
| `test_modelSaleEdit_removeSale_revertsVolumeSold` | `removeSale()` setzt `volumeSold` zurück | `volumeSold` = 0 nach Löschen, DB leer |
| `test_modelSaleEdit_orderNumberExists_true` | Vorhandene / nicht vorhandene Ordernummer | `orderNumberExists()` = true / false |
| `test_modelSaleEdit_orderNumberExists_excludeGuid` | Eigene Ordernummer beim Editieren ausgeschlossen | `orderNumberExists()` = false |
| `test_modelSaleEdit_documentExists_notFound_returnsFalse` | Pfad nicht in DB | `documentExists()` = false |
| `test_modelSaleEdit_documentExists_emptyPath_returnsFalse` | Leerer Pfad | Early Return = false |
| `test_modelSaleEdit_loadAvailableBuys_excludesSoldOut` | Vollständig verkaufte Käufe werden ausgeblendet | Nur 1 Kauf mit Restvolumen zurückgegeben |
| `test_modelSaleEdit_loadAvailableBuysForDepot_filtersDepot` | Nur Käufe des gewählten Depots | Je 1 Kauf pro Depot zurückgegeben |
| `test_modelSaleEdit_loadAvailableBuysForDepot_emptyDepot_returnsAll` | Leeres Depot = kein Filter | Alle 2 verfügbaren Käufe zurückgegeben |
| `test_modelSaleEdit_loadAvailableBuysForDepot_oldestFirst` | FIFO-Reihenfolge: ältester Kauf zuerst | `dateTime[0]` < `dateTime[1]` |
| `test_modelSaleEdit_updateSale_success` | Sale + Brokerage aktualisiert | Aktualisierte Werte in DB |
| `test_modelSaleEdit_updateSale_createsBrokerageIfMissing` | Kein Brokerage vorhanden → wird erstellt | `loadBrokerage()` danach gültig |
| `test_modelSaleEdit_loadSales_orderedByDate` | Verkäufe nach Datum aufsteigend | `dateTime[0]` < `dateTime[1]` |
| `test_modelSaleEdit_loadAllBuys_includesSoldOut` | `loadAllBuys()` gibt auch vollst. verkaufte Käufe zurück | Alle Käufe inkl. `volumeSold == volume` enthalten |
| `test_modelSaleEdit_loadBrokerageForBuy_returnsBrokerage` | `loadBrokerageForBuy()` gibt das Brokerage des Kaufs zurück | `brokerageGuid` korrekt |
| `test_modelSaleEdit_loadAvailableBuysForDepotExcludingSale_creditsBackPartialBuy` | Phase 2c, 07.08.2026: Kauf mit 20 Stück, 8 davon durch den zu bearbeitenden Verkauf verkauft | normal: 12 verfügbar; mit Ausschluss: wieder 20 |
| `test_modelSaleEdit_loadAvailableBuysForDepotExcludingSale_restoresFullyConsumedBuy` | Kauf durch GENAU diesen Verkauf vollständig aufgebraucht (fehlt in der normalen Liste) | mit Ausschluss: Kauf erscheint wieder, volle Menge verfügbar |
| `test_modelSaleEdit_loadAvailableBuysForDepotExcludingSale_emptyGuid_behavesLikeNormal` | Leere `excludeSaleGuid` | Ergebnis identisch zu `loadAvailableBuysForDepot()` |
| `test_modelSaleEdit_loadSplits_returnsInsertedSplit` | `loadSplits()` liest über `ShareSplitRepository` | Eingefügter Split wird zurückgegeben |

@note `test_modelSaleEdit_loadAllBuys_includesSoldOut` und
`test_modelSaleEdit_loadBrokerageForBuy_returnsBrokerage` sind dokumentiert
aber noch nicht implementiert.

---

PresenterSaleEdit (via StubView + StubModel):
| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_presenterSaleEdit_construction_loadsOverview` | Konstruktor lädt Verkäufe und befüllt Übersicht | `view.populateOverviewCalled` = true |
| `test_presenterSaleEdit_construction_clearsForm` | Formular nach Konstruktion leer | `view.clearFormCalled` = true |
| `test_presenterSaleEdit_construction_setsButtonStates_noSelection` | Initialer Button-Zustand | `canRemove=false`, `isLastSale=false` |
| `test_presenterSaleEdit_onRowSelected_singleSale_isLastSale` | Einzelner Verkauf ist immer der jüngste | `isLastSale=true`, `canRemove=true` |
| `test_presenterSaleEdit_onRowSelected_olderSale_isNotLastSale` | Älterer Verkauf → `isLastSale=false` | `canRemove=false` |
| `test_presenterSaleEdit_onRowSelected_newerSale_isLastSale` | Jüngster Verkauf → `isLastSale=true` | `canRemove=true` |
| `test_presenterSaleEdit_onRowSelected_emptyGuid_resetsForm` | Leere GUID → Reset | `view.clearFormCalled` = true |
| `test_presenterSaleEdit_onReset_setsButtonStates_noSelection` | Reset → Button-Zustand zurückgesetzt | `canRemove=false`, `isLastSale=false` |
| `test_presenterSaleEdit_onReset_jumpsToOverviewTab` | Reset → Übersicht-Tab | `showOverviewTab()` aufgerufen |
| `test_presenterSaleEdit_onSave_newSale_callsAddSale` | Neuer Verkauf → `addSale()` | `model.addSaleCalled` = true |
| `test_presenterSaleEdit_onSave_newSale_emitsDataChanged` | Neuer Verkauf → Signal | `dataChanged` emittiert |
| `test_presenterSaleEdit_onSave_newSale_jumpsToOverviewTab` | Neuer Verkauf → Übersicht-Tab | `showOverviewTab()` aufgerufen |
| `test_presenterSaleEdit_onSave_missingFields_showsError` | Pflichtfelder fehlen → Fehler | `view.lastError` nicht leer, kein `addSale` |
| `test_presenterSaleEdit_onSave_latestSale_callsUpdateSale` | Jüngster Verkauf edit → `updateSale()` | `model.updateSaleCalled` = true |
| `test_presenterSaleEdit_onSave_latestSale_recomputesFifoAllocation` | Phase 2c, 07.08.2026: gespeicherte `SaleBuyDetails` zeigen absichtlich auf einen nicht mehr verfügbaren Kauf, Verkaufsmenge im Formular geändert | `model.lastUpdatedSale.saleBuyDetails()` zeigt auf den frisch berechneten, tatsächlich verfügbaren Kauf |
| `test_presenterSaleEdit_onSave_latestSale_usesExcludingSaleVariant` | `onSave()` muss beim Bearbeiten `loadAvailableBuysForDepotExcludingSale()` mit der GUID des bearbeiteten Verkaufs aufrufen | `model.excludingSaleCalled` = true, `lastExcludeSaleGuid` korrekt |
| `test_presenterSaleEdit_onSave_newSale_doesNotUseExcludingSaleVariant` | Neuer Verkauf hat nichts zurückzubuchen | `model.excludingSaleCalled` = false |
| `test_presenterSaleEdit_onRowSelected_latestSale_livePreviewMatchesFifo` | Live-Vorschau (`refreshDerivedValues()`) muss beim jüngsten Verkauf dieselbe FIFO-Zuteilung zeigen wie `onSave()` später tatsächlich berechnet | `view.lastKaufwert` = Anteile × Kaufkurs des zugeteilten Kaufs |
| `test_presenterSaleEdit_onSave_nonLatestSale_callsUpdateSaleDocOnly` | Älterer Verkauf → `updateSale()` (nur Dokument) | `model.updateSaleCalled` = true, kein `addSale` |
| `test_presenterSaleEdit_onSave_nonLatestSale_jumpsToOverviewTab` | Älterer Verkauf gespeichert → Übersicht-Tab | `showOverviewTab()` aufgerufen |
| `test_presenterSaleEdit_onSave_nonLatestSale_emitsDataChanged` | Älterer Verkauf gespeichert → Signal | `dataChanged` emittiert |
| `test_presenterSaleEdit_onSave_duplicateOrderNumber_showsError` | Doppelte Ordernummer → Fehler | `view.lastError` nicht leer, kein `addSale` |
| `test_presenterSaleEdit_onSave_documentDuplicate_showsError` | Dokument bereits vergeben → Fehler | `view.lastError` nicht leer, kein `addSale` |
| `test_presenterSaleEdit_onRemove_latestSale_callsModel` | Löschen erlaubt → `removeSale()` | `model.removeSaleCalled` = true |
| `test_presenterSaleEdit_onRemove_latestSale_emitsDataChanged` | Löschen → Signal | `dataChanged` emittiert |
| `test_presenterSaleEdit_onRemove_olderSale_showsError` | Nicht-letzter Verkauf → Fehler | `model.removeSaleCalled` = false |
| `test_presenterSaleEdit_onRemove_noSelection_doesNothing` | Kein Verkauf ausgewählt → kein Aufruf | `model.removeSaleCalled` = false |
| `test_presenterSaleEdit_onOrderNumberEdited_empty_setsError` | Leere Ordernummer → Fehler-Icon | `setFieldError("orderNumber")` aufgerufen |
| `test_presenterSaleEdit_onOrderNumberEdited_duplicate_setsError` | Doppelte Ordernummer → Fehler-Icon | `setFieldError("orderNumber")` aufgerufen |
| `test_presenterSaleEdit_onDocumentPathEdited_duplicate_setsError` | Duplikat-Dokument → Fehler-Icon | `setFieldError("document")` aufgerufen |
| `test_presenterSaleEdit_onDocumentPathEdited_unique_setsOk` | Eindeutiges Dokument → Ok-Icon | `setFieldOk("document")` aufgerufen |
| `test_presenterSaleEdit_onOrderNumberEdited_valid_setsOk` | Gültige Ordernummer → Ok-Icon | `setFieldOk("orderNumber")` aufgerufen |
| `test_presenterSaleEdit_onDocumentSelected_newMode_doesNotEarlyReturn` | Neu-Modus → Vorschau wird geöffnet | `openPdfPreview()` aufgerufen |
| `test_presenterSaleEdit_onDocumentSelected_nonLatestSale_earlyReturn` | Nicht-letzter Verkauf → kein Parse | `setUiBusy(true)` nicht aufgerufen |
| `test_presenterSaleEdit_onSave_nonLatestSale_resetsButtonLabel` | Nicht-letzter Verkauf gespeichert → Button zurückgesetzt | `canRemove=false`, `isLastSale=false` |

---

ViewSaleEdit:
| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_viewSaleEdit_canBeConstructed` | Dialog öffnet ohne Absturz | Fenstertitel enthält "Verkäufe" |
| `test_viewSaleEdit_initialValues` | Alle Felder starten mit Standardwerten | Alle numerischen Felder = 0.0, Texte leer |
| `test_viewSaleEdit_depotNumberCombo_populatedFromConfig` | ComboBox aus Documents.xml befüllt | `count()` ≥ 1 |
| `test_viewSaleEdit_hasMissingRequiredFields_initiallyTrue` | Direkt nach Konstruktion fehlen Pflichtfelder | Liste enthält depotNumber, orderNumber, volume, salePrice |
| `test_viewSaleEdit_hasMissingRequiredFields_falseAfterAllSet` | Nach Setzen aller Pflichtfelder | `hasMissingRequiredFields()` = false |
| `test_viewSaleEdit_markMissingFieldsAsFailed_doesNotCrash` | Auf leerem Formular | Kein Absturz |
| `test_viewSaleEdit_clearForm_resetsAllFields` | `clearForm()` → Standardwerte | Alle Eingabefelder auf 0 / leer |
| `test_viewSaleEdit_clearForm_restoresEditableFields` | Nach `setButtonStates(false,false,true)` + `clearForm()` | Keine deaktivierten Eingabefelder mehr |
| `test_viewSaleEdit_setFieldOk_doesNotOverwriteWithEmptyValue` | `setFieldOk("orderNumber", "")` → Widget-Text unverändert | Bestehender Wert bleibt erhalten |
| `test_viewSaleEdit_setFieldOk_writesValueWhenNonEmpty` | `setFieldOk("orderNumber", "ORD-S-456")` → Widget aktualisiert | `orderNumber()` = "ORD-S-456" |
| `test_viewSaleEdit_setFieldError_doesNotCrash` | Fehler-Icon auf gültigem + unbekanntem Feld | Kein Absturz |
| `test_viewSaleEdit_setButtonStates_noSelection_addLabelHinzufuegen` | `setButtonStates(false,false,false)` | Button-Text = "Hinzufügen" |
| `test_viewSaleEdit_setButtonStates_isEdit_saveLabelSpeichern` | `setButtonStates(true,true,true)` | Button-Text = "Speichern" |
| `test_viewSaleEdit_setButtonStates_canRemoveFalse_removeDisabled` | `canRemove=false` | Entfernen-Button deaktiviert |
| `test_viewSaleEdit_setButtonStates_canRemoveTrue_removeEnabled` | `canRemove=true` | Entfernen-Button aktiv |
| `test_viewSaleEdit_setButtonStates_notLastSale_fieldsDisabled` | `isLastSale=false, isEdit=true` | Ordernummer-Feld deaktiviert |
| `test_viewSaleEdit_setButtonStates_isLastSale_fieldsEnabled` | `isLastSale=true` | Ordernummer-Feld aktiv |

---

ViewSaleEdit — populateOverview:

@note Migration auf OverviewTabWidget (16.07.2026, siehe ARCHITECTURE.md):
alle Tests in diesem Abschnitt und in "ViewSaleEdit — Tab-Klick-Logik" wurden
von `findChild<QTabWidget*>()` auf `findChild<OverviewTabWidget*>()`
umgestellt — reiner Typ-Austausch, da `count()/widget()/tabText()/
currentIndex()/setCurrentIndex()` bewusst identisch zur bisherigen
`QTabWidget`-API benannt sind. Die beiden vorherigen Klick-Slots
`onOverviewRowActivated()`/`onUebersichtRowActivated()` entfielen ersatzlos;
das beobachtbare Verhalten (siehe "ViewSaleEdit — Tab-Klick-Logik" unten)
bleibt unveraendert, da es jetzt `OverviewTabWidget` intern uebernimmt.

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_viewSaleEdit_populateOverview_emptyList_noTabs` | Leere Liste → kein Tab | `tabs->count()` = 0 |
| `test_viewSaleEdit_populateOverview_singleYear_twoTabs` | 1 Verkauf in 2024 → 2 Tabs | Tab 0 = "Übersicht", Tab 1 enthält "2024" |
| `test_viewSaleEdit_populateOverview_twoYears_threeTabs` | Verkäufe in 2023 + 2024 → 3 Tabs | `count()` = 3 |
| `test_viewSaleEdit_populateOverview_jahresTabsDescendingByYear` | Neuestes Jahr zuerst | Tab 1 = 2024, Tab 2 = 2022 |
| `test_viewSaleEdit_populateOverview_jahresTabHasFiveColumns` | Jahres-Tab hat 5 Spalten | `columnCount()` = 5 (Datum, Anteile, Auszahlung, G/V, Dokument) |
| `test_viewSaleEdit_populateOverview_guidStoredInDateColumn` | GUID in Spalte 0, `Qt::UserRole` | `item(0,0)->data(UserRole)` = sale.guid() |
| `test_viewSaleEdit_populateOverview_repopulateReplacesOldTabs` | Zweiter Aufruf ersetzt alle Tabs | Alte Tabs verschwunden, neue korrekt |
| `test_viewSaleEdit_populateOverview_docIconWhenPathSet` | Dokument-Pfad gesetzt → `QLabel` als CellWidget | `tbl->cellWidget(0,4)` nicht null |
| `test_viewSaleEdit_populateOverview_docDashWhenNoPath` | Kein Dokument → "-" in Spalte 4 | `item(0,4)->text()` = "-", kein CellWidget |
| `test_viewSaleEdit_populateOverview_uebersichtTabHasTable` | Übersicht-Tab enthält QTableWidget | `dataTable` nicht null, 4 Spalten, 1 Zeile |
| `test_viewSaleEdit_populateOverview_jahresTabRowCount` | Zeilenanzahl = Anzahl Verkäufe | 3 Verkäufe → `rowCount()` = 3 |
| `test_viewSaleEdit_populateOverview_tabTitleContainsTotal` | Tab-Titel enthält Gesamtbetrag und "€" | Titel enthält "€" |

---

ViewSaleEdit — Tab-Klick-Logik:
| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_viewSaleEdit_uebersichtClick_jumpsToYearTab` | Klick auf 2024-Zeile im Übersicht-Tab | `tabs->currentIndex()` = Index des 2024-Tabs |
| `test_viewSaleEdit_uebersichtRowSelection_isEnabled` | Übersicht-Tab hat SelectRows | `selectionBehavior()` = `SelectRows` |
| `test_viewSaleEdit_jahresTab_hasSelectRows` | Jahres-Tab hat SelectRows | `selectionBehavior()` = `SelectRows` |
| `test_viewSaleEdit_tabChange_clearsOldSelection` | Tab-Wechsel leert Selektion im verlassenen Tab | `selectedItems()` leer nach Wechsel |
| `test_viewSaleEdit_tabChange_selectsFirstRowInJahresTab` | Wechsel zu Jahres-Tab selektiert Zeile 0 | `currentRow()` = 0 |
| `test_viewSaleEdit_tabChange_noAutoSelectInUebersicht` | Wechsel zu Übersicht-Tab — keine Autoauswahl | `selectedItems()` leer |
| `test_viewSaleEdit_tabChange_toJahresTab_selectsFirstRow` | Wechsel zu Jahres-Tab selektiert Zeile 0 | Selektion nicht leer, `currentRow()` = 0 |
| `test_viewSaleEdit_tabChange_backToUebersicht_clearsJahresSelection` | Zurück zu Übersicht → Jahres-Tab-Selektion geleert | `selectedItems()` leer |

---

ViewSaleEdit — Details-Button:
| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_viewSaleEdit_detailsButton_enabledInNewMode` | Neu-Modus → Details-Button aktiv | `btn->isEnabled()` = true |
| `test_viewSaleEdit_detailsButton_enabledInEditMode` | Edit-Modus → Details-Button aktiv | `btn->isEnabled()` = true |
| `test_viewSaleEdit_detailsButton_enabledForNonLatestSale` | Älterer Verkauf (Read-only) → Details-Button aktiv | `btn->isEnabled()` = true |
| `test_viewSaleEdit_loadSale_clearedByReset` | `loadSale()` befüllt Felder; `clearForm()` setzt zurück | volume/salePrice = 0, orderNumber leer |

Nicht unit-testbar (bewusste Entscheidung):
`onShowDetails()` öffnet `QDialog::exec()` und blockiert die Ereignisschleife —
der Dialog selbst ist daher nicht direkt unit-testbar. Dies betrifft:

- Die 13-spaltige "Verwendete Käufe"-Tabelle
  (Datum | Anteile | x | Kaufkurs | = | Kaufsumme | + | Kosten | - | Rabatt | = | Gesamt | Dokument-Icon, seit 17.07.2026 ohne Spaltenkopf-Text)
- Den Doppelklick-Dokument-Vorschau-Dialog
- Die 5-gliedrige G/V-Zusammenfassung
  (Ges. Anteile . Ges. Verkauf - Ges. Kauf inkl. Kosten - Verkaufsgebuehren/Steuern = G/V)

Die Dok-Icon-Logik (`setCellWidget` + Icon-Auswahl nach Dateiendung) ist identisch
zum bereits abgedeckten `populateOverview`-Muster
(`test_viewSaleEdit_populateOverview_docIconWhenPathSet` /
`test_viewSaleEdit_populateOverview_docDashWhenNoPath`). Neue Tests wuerden
keinen zusaetzlichen Mehrwert bringen.

Der Dokumentpfad-Lookup für ältere, nicht editierbare Verkäufe verwendet
`m_allBuys` (alle Käufe inkl. vollständig verkaufter) — befüllt durch
`setAllBuys()` im Presenter-Konstruktor. Dadurch können auch Käufe mit
`volumeSold == volume` korrekt nachgeschlagen werden. Für neue Verkäufe und
für das Bearbeiten des jüngsten Verkaufs (seit Phase 2c der
Aktiensplit-Behandlung, 07.08.2026, siehe ARCHITECTURE.md "SalesForm-
Details") läuft der Lookup stattdessen über `m_availableBuys`, befüllt über
`loadAvailableBuysForDepotExcludingSale()`.

---

Konfiguration & Settings:
| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_configurations_webSitesLoaded` | WebSites.xml ladbar | `Success`, `count()` > 0 |
| `test_configurations_documentsLoaded` | Documents.xml ladbar | `Success`, `count()` > 0 |
| `test_apiSettings_saveYahooKey` | Yahoo API-Key gespeichert | `apiKeyYahoo()` = gesetzter Wert |

Split-Hinweis (ergänzt 09.08.2026, Phase 3b) — wortgleiche Verdrahtung wie in
BuysForm:

| Test | Prüft |
| ---- | ----- |
| `test_presenterSaleEdit_setsSplitHintOnConstruction` | Hinweis wird beim Öffnen gesetzt |
| `test_presenterSaleEdit_noSplits_hintSaysNoSplit` | `hasSplit` = false, Tooltip leer |
| `test_presenterSaleEdit_splitAfterSaleDate_hintIsActive` | `hasSplit` = true, Text enthält "20:1" |
| `test_presenterSaleEdit_splitBeforeSaleDate_hintIsInactive` | Verkauf nach dem Split → false |
| `test_presenterSaleEdit_onDateEdited_refreshesHint` | Datumswechsel schaltet den Hinweis um |
| `test_presenterSaleEdit_usesSalePriceNotBuyPrice` | Verkaufspreis geht ein (2.000 / 20 = 100) |

@note `test_..._usesSalePriceNotBuyPrice` fängt den wahrscheinlichsten Fehler
beim Übertragen aus der Buy-Variante ab. Der Test prüft die konkrete Zahl
statt nur die Anwesenheit eines Textes — sonst liefe er auch dann grün, wenn
versehentlich die Stückzahl statt des Preises umgerechnet würde.

---

### tests/forms/ — DividendForm

@note Stub-Pattern: `StubViewDividendEdit` und `StubModelDividendEdit` implementieren
die jeweiligen Interfaces ohne echte UI oder Datenbank.
`StubModelDividendEdit::loadShare()` gibt ein ungültiges `ShareObject{}` zurück —
die WKN/ISIN-Prüfung im Presenter wird damit übersprungen (korrekt für Unit-Tests).
`StubModelDividendEdit::findClosingPriceForDate()` ist über `hasClosingPrice` /
`closingPriceToReturn` konfigurierbar und zeichnet den letzten Aufruf auf
(`findClosingPriceForDateCalled`, `lastClosingPriceShareGuid`, `lastClosingPriceDate`).
`StubViewDividendEdit::setFieldOk()` zeichnet Feld, Wert und Tooltip des letzten
Aufrufs auf (`lastFieldOkField/-Value/-Tooltip`) und schreibt den Wert für
`priceAtPayday` zusätzlich zurück in `m_priceAtPayday`, analog zum echten
`ViewDividendEdit`-Verhalten.

Seit 11.08.2026 (Phase 3c) hat `StubModelDividendEdit` einen `splits`-Member
und implementiert `loadSplits()`; `StubViewDividendEdit::populateOverview()`
legt die übergebenen Splits in `lastOverviewSplits` ab.

ViewDividendEdit / PresenterDividendEdit — Split-Marker (Phase 3c, 11.08.2026):
| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_viewDividendEdit_populateOverview_belegRowKeepsBelegVolumeWithMarker` | Ausschüttung 2021, Split 20:1 danach | Belegzeile bleibt in Beleg-Skala, trägt Marker und Tooltip |
| `test_viewDividendEdit_populateOverview_withoutSplitHasNoMarker` | Aktie ohne Splits | Kein Marker, kein Tooltip |
| `test_viewDividendEdit_populateOverview_footerVolumeIsDash` | Zwei Ausschüttungen im selben Jahr, kein Split | Anteile-Summe ist "-" mit Tooltip, Dividenden-Summe wird weiterhin gebildet |
| `test_presenterDividendEdit_populateOverview_passesSplitsAsParameter` | Presenter-Verdrahtung | Splits kommen als Parameter an, nicht über einen Setter |

@note Der Strich in der Fusszeile steht unabhängig von Splits — die Summe war
schon vorher bedeutungslos (Anteile verschiedener Auszahlungstage). Genau das
prüft der dritte Test bewusst OHNE Split.

ModelDividendEdit (Datenbanktests):
| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_modelDividendEdit_addDividend_success` | Dividende gespeichert | `loadDividends()` gibt 1 Eintrag zurück |
| `test_modelDividendEdit_updateDividend_success` | Dividende aktualisiert | Neuer Rate-Wert in DB |
| `test_modelDividendEdit_removeDividend_success` | Dividende gelöscht | `loadDividends()` leer danach |
| `test_modelDividendEdit_documentExists_notFound_returnsFalse` | Pfad nicht in DB | `documentExists()` = false |
| `test_modelDividendEdit_documentExists_emptyPath_returnsFalse` | Leerer Pfad | Early Return = false |
| `test_modelDividendEdit_loadDividends_orderedByDate` | Dividenden nach Datum aufsteigend | `dateTime[0]` < `dateTime[1]` |
| `test_modelDividendEdit_findClosingPriceForDate_found_returnsTrue` | Schlusskurs für Datum in `daily_values` vorhanden | Rückgabe `true`, `outPrice` = gespeicherter Closing-Wert |
| `test_modelDividendEdit_findClosingPriceForDate_notFound_returnsFalse` | Kein Eintrag für Datum in `daily_values` | Rückgabe `false` |
| `test_modelDividendEdit_findClosingPriceForDate_zeroClosing_returnsFalse` | Eintrag vorhanden, aber `closing` = 0 | Rückgabe `false` (kein sinnvoller Kurs) |

---

PresenterDividendEdit (via StubView + StubModel):
| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_presenterDividendEdit_construction_loadsOverview` | Konstruktor lädt Dividenden und befüllt Übersicht | `view.populateOverviewCalled` = true |
| `test_presenterDividendEdit_construction_clearsForm` | Formular nach Konstruktion leer | `view.clearFormCalled` = true |
| `test_presenterDividendEdit_construction_setsButtonStates_noSelection` | Initialer Button-Zustand | `canRemove=false`, `isEdit=false` |
| `test_presenterDividendEdit_onSave_newDividend_callsAddDividend` | Neue Dividende → `addDividend()` | `model.addDividendCalled` = true |
| `test_presenterDividendEdit_onSave_newDividend_emitsDataChanged` | Neue Dividende → Signal | `dataChanged` emittiert |
| `test_presenterDividendEdit_onSave_newDividend_jumpsToOverviewTab` | Neue Dividende → Übersicht-Tab | `showOverviewTab()` aufgerufen |
| `test_presenterDividendEdit_onSave_missingFields_showsError` | Pflichtfelder fehlen → Fehler | `view.lastError` nicht leer, kein `addDividend` |
| `test_presenterDividendEdit_onSave_documentDuplicate_showsError` | Dokument bereits vergeben → Fehler | `view.lastError` nicht leer, kein `addDividend` |
| `test_presenterDividendEdit_onSave_modelError_showsError` | DB-Fehler beim Speichern → Fehler | `view.lastError` nicht leer, kein `addDividend` |
| `test_presenterDividendEdit_onRowSelected_loadsAndSetsButtonStates` | Dividende laden → Felder befüllt, Button-Zustand gesetzt | `loadDividendCalled` = true, `canRemove=true`, `isEdit=true` |
| `test_presenterDividendEdit_onRowSelected_emptyGuid_resetsForm` | Leere GUID → Reset | `view.clearFormCalled` = true |
| `test_presenterDividendEdit_onSave_existingDividend_callsUpdateDividend` | Bestehende Dividende → `updateDividend()` | `model.updateDividendCalled` = true, kein `addDividend` |
| `test_presenterDividendEdit_onSave_existingDividend_emitsDataChanged` | Bestehende Dividende gespeichert → Signal | `dataChanged` emittiert |
| `test_presenterDividendEdit_onSave_existingDividend_jumpsToOverviewTab` | Bestehende Dividende → Übersicht-Tab | `showOverviewTab()` aufgerufen |
| `test_presenterDividendEdit_onRemove_callsModel` | Löschen → `removeDividend()` | `model.removeDividendCalled` = true |
| `test_presenterDividendEdit_onRemove_emitsDataChanged` | Löschen → Signal | `dataChanged` emittiert |
| `test_presenterDividendEdit_onRemove_anyDividend_canBeRemoved` | **Jede** Dividende löschbar (auch ältere) | `canRemove=true` für älteren Eintrag; kein Fehler |
| `test_presenterDividendEdit_onRemove_noSelection_doesNothing` | Kein Eintrag ausgewählt → kein Aufruf | `model.removeDividendCalled` = false |
| `test_presenterDividendEdit_onReset_setsButtonStates_noSelection` | Reset → Button-Zustand zurückgesetzt | `canRemove=false`, `isEdit=false` |
| `test_presenterDividendEdit_onReset_jumpsToOverviewTab` | Reset → Übersicht-Tab | `showOverviewTab()` aufgerufen |
| `test_presenterDividendEdit_onDocumentPathEdited_duplicate_setsError` | Duplikat-Dokument → Fehler-Icon | kein Absturz, Error-Pfad ausgeführt |
| `test_presenterDividendEdit_onDocumentPathEdited_unique_setsOk` | Eindeutiges Dokument → Ok-Icon | kein Absturz, Ok-Pfad ausgeführt |
| `test_presenterDividendEdit_onClose_closesView` | `onClose()` → View geschlossen | `view.closed` = true |
| `test_presenterDividendEdit_onForeignCurrencyToggled_callsView` | FC-Toggle → kein Absturz | true/false beide ohne Fehler |

---

ViewDividendEdit — Fremdwaehrungs-Modus:
| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_viewDividendEdit_fcFieldsDisabledByDefault` | FC-Felder initial deaktiviert | `enableForeignCurrency()` = false, `exchangeRatio()` = 1.0 |
| `test_viewDividendEdit_setForeignCurrencyEnabled_true_enablesFields` | `setForeignCurrencyEnabled(true)` → kein Absturz | `exchangeRatio()` lesbar |
| `test_viewDividendEdit_setForeignCurrencyEnabled_false_disablesFields` | true → false → kein Absturz | Felder deaktiviert |
| `test_viewDividendEdit_clearForm_resetsFcCheckbox` | `clearForm()` nach FC-Dividende laden | `enableForeignCurrency()` = false |
| `test_viewDividendEdit_loadDividend_withFC_setsCheckbox` | Dividende mit FC laden | Checkbox angehakt, `exchangeRatio()` korrekt |
| `test_viewDividendEdit_loadDividend_withoutFC_checkboxUnchecked` | Dividende ohne FC laden | Checkbox nicht angehakt |
| `test_presenterDividendEdit_onDateEdited_validDate_setsOk` | Gültiges Datum → Ok-Icon | kein Fehler-Dialog |
| `test_presenterDividendEdit_onDateEdited_sentinelDate_setsError` | Sentinel 2000-01-01 → Error-Icon | Icon-Only, kein Dialog |
| `test_presenterDividendEdit_onDateEdited_dailyValueFound_fillsPriceAtPayday` | Datum geändert, Model liefert Schlusskurs | `findClosingPriceForDate` aufgerufen mit korrektem shareGuid/Datum, `priceAtPayday()` = gelieferter Preis, Tooltip gesetzt |
| `test_presenterDividendEdit_onDateEdited_noDailyValue_leavesPriceAtPaydayUnchanged` | Datum geändert, kein Treffer in `daily_values` | `priceAtPayday()` bleibt unverändert (bereits manuell gesetzter Wert bleibt erhalten) |
| `test_presenterDividendEdit_onDateEdited_invalidDate_doesNotQueryDailyValue` | Sentinel-Datum → ungültig | `findClosingPriceForDate` wird **nicht** aufgerufen |

@note Die Lookup-Logik sitzt in der gemeinsamen privaten Hilfsmethode
`applyDailyValuePriceAtPayday()`, die sowohl von `onDateEdited()` als auch von
`populateFromResult()` (direkt nach dem Parsen, mit dem tatsächlich geparsten Datum)
aufgerufen wird. Der zweite Aufrufpfad ist nicht separat unit-getestet — die
Parse-Pipeline hängt an `ParserLib::Parser`/`QProcess` (pdftotext) und ist wie beim
Refresh-Flow (siehe "Erledigt / Archiv") noch nicht gemockt. Manuell verifiziert
am 07.07.2026: Preis wird nach dem Einlesen eines Dividenden-Dokuments korrekt anhand
des geparsten Auszahlungsdatums gesetzt, auch wenn zuvor durch einen beiläufigen
Fokuswechsel auf das Datumsfeld (noch mit Default "heute") ein abweichender
Zwischenwert gesetzt wurde.

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_presenterDividendEdit_onRateEdited_valid_setsOk` | Rate > 0 → Ok-Icon | kein Fehler-Dialog |
| `test_presenterDividendEdit_onRateEdited_zero_setsError` | Rate = 0 → Error-Icon | Icon-Only, kein Dialog |
| `test_presenterDividendEdit_onVolumeEdited_valid_setsOk` | Volume > 0 → Ok-Icon | kein Fehler-Dialog |
| `test_presenterDividendEdit_onPriceAtPaydayEdited_valid_setsOk` | Preis > 0 → Ok-Icon | kein Fehler-Dialog |
| `test_presenterDividendEdit_onPriceAtPaydayEdited_zero_setsError` | Preis = 0 → Error-Icon | Icon-Only, kein Dialog |
| `test_presenterDividendEdit_onTaxEdited_negative_setsError` | Negativer Steuerwert → Error-Icon | Icon-Only, kein Dialog |
| `test_presenterDividendEdit_onTaxEdited_zero_setsOk` | Steuerwert = 0 → Ok-Icon (optional, valide) | kein Fehler-Dialog |
| `test_presenterDividendEdit_onExchangeRatioEdited_valid_setsOk` | FC aktiv + Kurs > 0 → Ok-Icon | kein Fehler-Dialog |
| `test_presenterDividendEdit_onExchangeRatioEdited_zero_setsError` | FC aktiv + Kurs = 0 → Error-Icon | Icon-Only, kein Dialog |
| `test_presenterDividendEdit_onExchangeRatioEdited_fcDisabled_noValidation` | FC nicht aktiv → keine Validierung | kein Fehler trotz Kurs = 0 |

---

ViewDividendEdit:
| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_viewDividendEdit_canBeConstructed` | Dialog öffnet ohne Absturz | Fenstertitel enthält "Dividende" |
| `test_viewDividendEdit_initialValues` | Alle Felder starten mit Standardwerten | rate/volume/taxAtSource/priceAtPayday = 0.0, documentPath leer |
| `test_viewDividendEdit_hasMissingRequiredFields_initiallyTrue` | Direkt nach Konstruktion fehlen Pflichtfelder | Liste enthält Dividendensatz, Anteile, Preis der Aktie |
| `test_viewDividendEdit_hasMissingRequiredFields_falseAfterAllSet` | Nach Setzen aller Pflichtfelder | `hasMissingRequiredFields()` = false |
| `test_viewDividendEdit_clearForm_resetsAllFields` | `clearForm()` → Standardwerte | rate = 0.0, volume = 0.0, documentPath leer |
| `test_viewDividendEdit_setButtonStates_noSelection_addLabelHinzufuegen` | `setButtonStates(false, false)` | Button-Text = "Hinzufügen" |
| `test_viewDividendEdit_setButtonStates_isEdit_saveLabelSpeichern` | `setButtonStates(true, true)` | Button-Text = "Speichern" |
| `test_viewDividendEdit_setButtonStates_canRemoveFalse_removeDisabled` | `canRemove=false` | Entfernen-Button deaktiviert |
| `test_viewDividendEdit_setButtonStates_canRemoveTrue_removeEnabled` | `canRemove=true` | Entfernen-Button aktiv |
| `test_viewDividendEdit_allFieldsAlwaysEnabled` | `setButtonStates(true, true)` — kein readOnlyMode | Alle editierbaren Felder aktiviert |
| `test_viewDividendEdit_markMissingFieldsAsFailed_doesNotCrash` | Auf leerem Formular | Kein Absturz |
| `test_viewDividendEdit_setFieldOk_doesNotOverwriteWithEmptyValue` | `setFieldOk("rate", "")` → Widget-Text unverändert | Bestehender Wert bleibt erhalten |
| `test_viewDividendEdit_setFieldOk_writesValueWhenNonEmpty` | `setFieldOk("rate", "2,00")` → Widget aktualisiert | `rate()` = 2.0 |
| `test_viewDividendEdit_setFieldOk_date_parsesISOFormat` | `setFieldOk("date", "2024-06-15")` → QDateEdit | `date()` = 2024-06-15 |
| `test_viewDividendEdit_setFieldOk_volume_handlesGermanDecimal` | "165,0000" → 165.0 | `volume()` = 165.0 |
| `test_viewDividendEdit_setFieldOk_rate_handlesGermanDecimal` | "1,50" → 1.5 | `rate()` = 1.5 |
| `test_viewDividendEdit_setFieldError_doesNotCrash` | Fehler-Icon auf gültigem + unbekanntem Feld | Kein Absturz |
| `test_viewDividendEdit_clearForm_resetsStatusIcons` | `clearForm()` → alle Felder Untouched | `hasMissingRequiredFields()` = true danach |
| `test_viewDividendEdit_clearForm_resetsDerivedFields` | `clearForm()` nach gesetzten Werten | alle read-only-Felder zeigen 0,00 |
| `test_viewDividendEdit_clearForm_resetsParseStatusBar` | `clearForm()` nach `setParseProgress()` | Kein Absturz |
| `test_viewDividendEdit_setParseProgress_showsValues` | `setParseProgress(50, "...")` | `QProgressBar::value()` = 50 |
| `test_viewDividendEdit_populateOverview_emptyList_noTabs` | Leere Liste → kein Tab | `tabs->count()` = 0 |
| `test_viewDividendEdit_populateOverview_singleYear_twoTabs` | 1 Dividende in 2024 → 2 Tabs | Tab 0 = "Übersicht", Tab 1 enthält "2024" |
| `test_viewDividendEdit_populateOverview_twoYears_threeTabs` | Dividenden in 2023 + 2024 → 3 Tabs | `count()` = 3 |
| `test_viewDividendEdit_populateOverview_jahresTabsDescendingByYear` | Neuestes Jahr zuerst | Tab 1 = 2024, Tab 2 = 2022 |
| `test_viewDividendEdit_populateOverview_jahresTabHasFiveColumns` | Jahres-Tab hat 5 Spalten | `columnCount()` = 5 (Datum, Rate, Anteile, Dividende, Dokument-Icon ohne Spaltenkopf-Text seit 17.07.2026) |
| `test_viewDividendEdit_populateOverview_jahresTabRowCount` | Zeilenanzahl = Anzahl Dividenden | 3 Dividenden → `rowCount()` = 3 |
| `test_viewDividendEdit_populateOverview_guidStoredInDateColumn` | GUID in Spalte 0, `Qt::UserRole` | `item(0,0)->data(UserRole)` = dividend.guid() |
| `test_viewDividendEdit_populateOverview_docDashWhenNoPath` | Kein Dokument → "-" in Spalte 4 | `item(0,4)->text()` = "-", kein CellWidget |
| `test_viewDividendEdit_populateOverview_tabTitleContainsTotal` | Tab-Titel enthält "€" | Titel enthält "€" |
| `test_viewDividendEdit_populateOverview_repopulateReplacesOldTabs` | Zweiter Aufruf ersetzt alle Tabs | Alte Tabs verschwunden, neue korrekt |
| `test_viewDividendEdit_uebersichtTab_hasTable` | Übersicht-Tab enthält QTableWidget | `dataTable` nicht null, 2 Spalten, 1 Zeile |
| `test_viewDividendEdit_uebersichtClick_jumpsToYearTab` | Klick auf Zeile im Übersicht-Tab | `tabs->currentIndex()` > 0 |
| `test_viewDividendEdit_tabChange_selectsFirstRowInJahresTab` | Wechsel zu Jahres-Tab selektiert Zeile 0 | `currentRow()` = 0 |

Keine Letzter-Eintrag-Einschränkung:
`test_viewDividendEdit_allFieldsAlwaysEnabled` prüft explizit dass nach
`setButtonStates(true, true)` alle editierbaren Felder aktiviert bleiben.
In BuysForm/SalesForm würde dieselbe Konstellation (`isLastBuy=false, isEdit=true`)
den `readOnlyMode` auslösen — im DividendForm gibt es diesen Modus nicht.

---

### tests/forms/ — BrokeragesForm

@note Stub-Pattern: `StubViewBrokerageEdit` und `StubModelBrokerageEdit` implementieren
die jeweiligen Interfaces ohne echte UI oder Datenbank.

Linked-Record-Besonderheit: `StubModelBrokerageEdit` kann so konfiguriert werden,
dass `loadBrokerages()` Einträge mit gesetztem `buyGuid` oder `saleGuid` zurückgibt —
damit lässt sich die `isLinkedRecord()`-Logik des Presenters testen.

---

ModelBrokerageEdit (Datenbanktests):
| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_modelBrokerageEdit_addBrokerage_success` | Standalone-Eintrag gespeichert | `loadBrokerages()` gibt 1 Eintrag zurück |
| `test_modelBrokerageEdit_updateBrokerage_success` | Eintrag aktualisiert | Aktualisierte Werte in DB |
| `test_modelBrokerageEdit_updateDocument_success` | Nur Dokumentpfad aktualisiert | `document()` enthält neuen Pfad |
| `test_modelBrokerageEdit_removeBrokerage_success` | Eintrag gelöscht | `loadBrokerages()` leer danach |
| `test_modelBrokerageEdit_documentExists_notFound_returnsFalse` | Pfad nicht in DB | `documentExists()` = false |
| `test_modelBrokerageEdit_documentExists_emptyPath_returnsFalse` | Leerer Pfad | Early Return = false |
| `test_modelBrokerageEdit_documentExists_excludeGuid` | Eigene GUID beim Editieren ausgeschlossen | `documentExists()` = false |
| `test_modelBrokerageEdit_loadBrokerages_orderedByDate` | Einträge nach Datum aufsteigend | `dateTime[0]` < `dateTime[1]` |

---

PresenterBrokerageEdit (via StubView + StubModel):
| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_presenterBrokerageEdit_construction_loadsOverview` | Konstruktor lädt Einträge und befüllt Übersicht | `view.populateOverviewCalled` = true |
| `test_presenterBrokerageEdit_construction_clearsForm` | Formular nach Konstruktion leer | `view.clearFormCalled` = true |
| `test_presenterBrokerageEdit_construction_setsButtonStates_noSelection` | Initialer Button-Zustand | `canRemove=false`, `isEdit=false`, `readOnly=false` |
| `test_presenterBrokerageEdit_onSave_newBrokerage_callsAddBrokerage` | Neuer Eintrag → `addBrokerage()` | `model.addBrokerageCalled` = true |
| `test_presenterBrokerageEdit_onSave_newBrokerage_emitsDataChanged` | Neuer Eintrag → Signal | `dataChanged` emittiert |
| `test_presenterBrokerageEdit_onSave_newBrokerage_jumpsToOverviewTab` | Neuer Eintrag → Übersicht-Tab | `showOverviewTab()` aufgerufen |
| `test_presenterBrokerageEdit_onSave_missingFields_showsError` | Pflichtfelder fehlen → Fehler | `view.lastError` nicht leer, kein `addBrokerage` |
| `test_presenterBrokerageEdit_onSave_allFieldsZero_showsError` | Alle vier Felder = 0 → Fehler | `view.lastError` nicht leer, kein `addBrokerage` |
| `test_presenterBrokerageEdit_onSave_onlyRabattSet_success` | Nur Rabatt > 0 (100% Rabatt) → erfolgreich | `model.addBrokerageCalled` = true |
| `test_presenterBrokerageEdit_onSave_onlyProvisionSet_success` | Nur Provision > 0 → erfolgreich | `model.addBrokerageCalled` = true |
| `test_presenterBrokerageEdit_onSave_documentDuplicate_showsError` | Dokument bereits vergeben → Fehler | `view.lastError` nicht leer, kein `addBrokerage` |
| `test_presenterBrokerageEdit_onSave_existingStandalone_callsUpdateBrokerage` | Standalone-Edit → `updateBrokerage()` | `model.updateBrokerageCalled` = true, kein `addBrokerage` |
| `test_presenterBrokerageEdit_onSave_linkedRecord_showsError` | Linked-Edit → vollständig read-only, `showError()` | kein `updateBrokerage`, kein `updateDocument` |
| `test_presenterBrokerageEdit_onRowSelected_standaloneRecord_canRemoveTrue` | Standalone → `canRemove=true` | `readOnly=false` |
| `test_presenterBrokerageEdit_onRowSelected_linkedRecord_canRemoveFalse` | Linked Record → `canRemove=false` | `readOnly=true`, `isEdit=true` |
| `test_presenterBrokerageEdit_onRowSelected_withDocument_opensPdfPreview` | Eintrag mit Dokument → Vorschau geöffnet | `view.openPdfPreviewCalled` = true |
| `test_presenterBrokerageEdit_onRowSelected_withoutDocument_clearsPdfPreview` | Eintrag ohne Dokument → Vorschau geleert | `view.clearPdfPreviewCalled` = true |
| `test_presenterBrokerageEdit_onRowSelected_emptyGuid_resetsForm` | Leere GUID → Reset | `view.clearFormCalled` = true |
| `test_presenterBrokerageEdit_onRemove_standalone_callsModel` | Standalone löschen → `removeBrokerage()` | `model.removeBrokerageCalled` = true |
| `test_presenterBrokerageEdit_onRemove_standalone_emitsDataChanged` | Standalone löschen → Signal | `dataChanged` emittiert |
| `test_presenterBrokerageEdit_onRemove_linkedRecord_showsError` | Linked Record löschen → Fehler | `model.removeBrokerageCalled` = false |
| `test_presenterBrokerageEdit_onRemove_noSelection_doesNothing` | Kein Eintrag ausgewählt → kein Aufruf | `model.removeBrokerageCalled` = false |
| `test_presenterBrokerageEdit_onReset_clearsForm` | Reset → Formular geleert | `view.clearFormCalled` = true |
| `test_presenterBrokerageEdit_onReset_clearsPdfPreview` | Reset → PDF-Vorschau geleert | `view.clearPdfPreviewCalled` = true |
| `test_presenterBrokerageEdit_onReset_jumpsToOverviewTab` | Reset → Übersicht-Tab | `showOverviewTab()` aufgerufen |
| `test_presenterBrokerageEdit_onReset_setsButtonStates_noSelection` | Reset → Button-Zustand zurückgesetzt | `canRemove=false`, `isEdit=false`, `readOnly=false` |
| `test_presenterBrokerageEdit_onValuesChanged_updatesGesamtGebuehren` | Fee-Änderung → `setGesamtGebuehren()` | Wert = provision + brokerFee + traderFee |
| `test_presenterBrokerageEdit_onDocumentPathEdited_duplicate_showsError` | Duplikat-Dokument → Fehler | `view.lastError` nicht leer |
| `test_presenterBrokerageEdit_onDocumentPathEdited_unique_noError` | Eindeutiges Dokument → kein Fehler | `view.lastError` leer |
| `test_presenterBrokerageEdit_onClose_closesView` | `onClose()` → View geschlossen | `view.closed` = true |

---

ViewBrokerageEdit:
| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_viewBrokerageEdit_canBeConstructed` | Dialog öffnet ohne Absturz | Fenstertitel enthält "Kosten" |
| `test_viewBrokerageEdit_initialValues` | Alle Felder starten mit Standardwerten | provision/brokerFee/traderFee/reduction = 0.0, documentPath leer |
| `test_viewBrokerageEdit_hasMissingRequiredFields_initiallyTrue` | Datum defaultet auf "heute" (kein Sentinel) → direkt nach Konstruktion fehlt nichts; Laden eines Datensatzes mit Sentinel-Datum löst die Pflichtfeld-Prüfung aus | `hasMissingRequiredFields()` = false nach Konstruktion, = true nach Laden des Sentinel-Datums, Liste enthält "date" |
| `test_viewBrokerageEdit_hasMissingRequiredFields_falseAfterDateSet` | Nach Setzen des Datums | `hasMissingRequiredFields()` = false |
| `test_viewBrokerageEdit_clearForm_resetsAllFields` | `clearForm()` → Standardwerte | provision = 0.0, documentPath leer |
| `test_viewBrokerageEdit_clearForm_restoresEditableFields` | Nach `setButtonStates(false,true,true)` + `clearForm()` | Alle Felder wieder enabled |
| `test_viewBrokerageEdit_setButtonStates_noSelection_addLabelHinzufuegen` | `setButtonStates(false,false,false)` | Button-Text = "Hinzufügen" |
| `test_viewBrokerageEdit_setButtonStates_isEdit_saveLabelSpeichern` | `setButtonStates(true,true,false)` | Button-Text = "Speichern" |
| `test_viewBrokerageEdit_setButtonStates_canRemoveFalse_removeDisabled` | `canRemove=false` | Entfernen-Button deaktiviert |
| `test_viewBrokerageEdit_setButtonStates_canRemoveTrue_removeEnabled` | `canRemove=true` | Entfernen-Button aktiv |
| `test_viewBrokerageEdit_setButtonStates_readOnly_feeFieldsDisabled` | `readOnly=true` | Provision/Courtage/Handelsplatz/Rabatt/Datum, Browse-Button **und Speichern-Button** deaktiviert |
| `test_viewBrokerageEdit_setButtonStates_notReadOnly_feeFieldsEnabled` | `readOnly=false` | Alle Felder enabled |
| `test_viewBrokerageEdit_setGesamtGebuehren_updatesField` | `setGesamtGebuehren(12.50)` | read-only Feld zeigt "12,50" |
| `test_viewBrokerageEdit_setBrokerageReduction_positiveGreen` | Positiver Wert → grüner Hintergrund | StyleSheet enthält grüne Farbe |
| `test_viewBrokerageEdit_setBrokerageReduction_negativeRed` | Negativer Wert → roter Hintergrund | StyleSheet enthält rote Farbe |
| `test_viewBrokerageEdit_clearPdfPreview_doesNotCrash` | `clearPdfPreview()` ohne geladenes Dokument | Kein Absturz |
| `test_viewBrokerageEdit_openPdfPreview_nonExistentFile_doesNotCrash` | `openPdfPreview()` mit ungültigem Pfad | Kein Absturz |
| `test_viewBrokerageEdit_markMissingFieldsAsFailed_doesNotCrash` | Auf leerem Formular | Kein Absturz |

---

ViewBrokerageEdit — populateOverview:

@note Migration auf OverviewTabWidget (16.07.2026, siehe ARCHITECTURE.md):
alle Tests in diesem Abschnitt (inkl. "Linked-Record / readOnly-Besonderheit"
unten) wurden von `findChild<QTabWidget*>()` auf
`findChild<OverviewTabWidget*>()` umgestellt — reiner Typ-Austausch, da
`count()/widget()/tabText()/currentIndex()/setCurrentIndex()` bewusst
identisch zur bisherigen `QTabWidget`-API benannt sind. Die beiden
vorherigen Klick-Slots `onOverviewRowActivated()`/`onUebersichtRowActivated()`
entfielen ersatzlos; das beobachtbare Verhalten (Klick auf Jahres-Zeile lädt
Kosteneintrag, Klick auf Übersicht-Zeile springt zum Jahres-Tab) bleibt
unverändert, da es jetzt `OverviewTabWidget` intern übernimmt. Die
Dokument-Spalte (Index 5) war zunächst fest auf `120`px gesetzt statt
Stretch, analog zum Bugfix in `ViewSaleEdit`; seit der globalen
Vereinheitlichung am 17.07.2026 (siehe ARCHITECTURE.md, "Dokument-Spalten:
Breite auf 36px vereinheitlicht") fix `36`px, ohne Spaltenkopf-Text.

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_viewBrokerageEdit_populateOverview_emptyList_noTabs` | Leere Liste → kein Tab | `tabs->count()` = 0 |
| `test_viewBrokerageEdit_populateOverview_singleYear_twoTabs` | 1 Eintrag in 2024 → 2 Tabs | Tab 0 = "Übersicht", Tab 1 enthält "2024" |
| `test_viewBrokerageEdit_populateOverview_twoYears_threeTabs` | Einträge in 2023 + 2024 → 3 Tabs | `count()` = 3 |
| `test_viewBrokerageEdit_populateOverview_jahresTabsDescendingByYear` | Neuestes Jahr zuerst | Tab 1 = 2024, Tab 2 = 2022 |
| `test_viewBrokerageEdit_populateOverview_uebersichtTabHasTable` | Übersicht-Tab enthält QTableWidget | `dataTable` nicht null, 2 Spalten |
| `test_viewBrokerageEdit_populateOverview_jahresTabHasSixColumns` | Jahres-Tab hat 6 Spalten | Datum, Typ, Ges. Gebühren, Rabatt, Netto-Kosten, Dokument-Icon ohne Spaltenkopf-Text seit 17.07.2026 |
| `test_viewBrokerageEdit_populateOverview_guidStoredInDateColumn` | GUID in Spalte 0, `Qt::UserRole` | `item(0,0)->data(UserRole)` = brokerage.guid() |
| `test_viewBrokerageEdit_populateOverview_typColumnStandaloneIsSonstig` | Standalone-Eintrag → "Sonstig" | `item(0,1)->text()` = "Sonstig" |
| `test_viewBrokerageEdit_populateOverview_typColumnLinkedBuyIsKauf` | `buyGuid` gesetzt → "Kauf" | `item(0,1)->text()` = "Kauf" |
| `test_viewBrokerageEdit_populateOverview_typColumnLinkedSaleIsVerkauf` | `saleGuid` gesetzt → "Verkauf" | `item(0,1)->text()` = "Verkauf" |
| `test_viewBrokerageEdit_populateOverview_docIconWhenPathSet` | Dokument-Pfad gesetzt → `QLabel` als CellWidget | `tbl->cellWidget(0,5)` nicht null |
| `test_viewBrokerageEdit_populateOverview_docDashWhenNoPath` | Kein Dokument → "-" in Spalte 5 | `item(0,5)->text()` = "-", kein CellWidget |
| `test_viewBrokerageEdit_populateOverview_tabTitleContainsTotal` | Tab-Titel enthält "€" | Titel enthält "€" |
| `test_viewBrokerageEdit_populateOverview_repopulateReplacesOldTabs` | Zweiter Aufruf ersetzt alle Tabs | Alte Tabs verschwunden, neue korrekt |
| `test_viewBrokerageEdit_uebersichtClick_jumpsToYearTab` | Klick auf Zeile im Übersicht-Tab | `tabs->currentIndex()` > 0 |
| `test_viewBrokerageEdit_tabChange_selectsFirstRowInJahresTab` | Wechsel zu Jahres-Tab selektiert Zeile 0 | `currentRow()` = 0 |

Linked-Record / readOnly-Besonderheit:
`test_viewBrokerageEdit_setButtonStates_readOnly_feeFieldsDisabled` prüft explizit
dass bei `readOnly=true` alle Gebührenfelder und Datum/Uhrzeit deaktiviert werden.
Im Gegensatz zu BuysForm/SalesForm (wo `readOnlyMode` über `isLastBuy` gesteuert wird)
kommt der `readOnly`-Parameter hier direkt von der Linked-Record-Erkennung im Presenter.

@note Zur Testklasse: Anders als SalesForm/DividendForm (eigene `TestSalesForm` /
`TestDividendForm`) laufen alle BrokeragesForm-Tests in `TestMainWindow`, da
BrokeragesForm zusammen mit BuysForm im selben `tst_mainwindow`-Abschnitt geführt wird.
`StubViewBrokerageEdit` und `StubModelBrokerageEdit` sind entsprechend dort als
file-globale Klassen vor `TestMainWindow` definiert.

---

### tests/forms/ — OwnMessageBox

@note Stub-Pattern: Kein Stub nötig — `OwnMessageBox` hat keine externe
Abhängigkeit zu Datenbank oder komplexen Interfaces. Alle Tests arbeiten
direkt mit dem Widget.

@note Zu statischen Methoden: `critical()`, `information()` und
`question()` rufen intern `exec()` auf und blockieren die Ereignisschleife —
sie sind daher nicht direkt unit-testbar. Stattdessen wird der Konstruktor
direkt verwendet und das Ergebnis von Button-Klicks über
`QMetaObject::invokeMethod` mit `Qt::DirectConnection` geprüft.

TestOwnMessageBox:
| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_critical_canBeConstructed` | Dialog öffnet ohne Absturz | Fenstertitel korrekt |
| `test_critical_hasSingleOkButton` | Genau ein Button vorhanden | `buttons.size()` = 1, Text = "Ok" |
| `test_critical_hasNoYesNoButtons` | Kein Ja/Nein-Button | Text ≠ "Ja", Text ≠ "Nein" |
| `test_critical_okButtonAcceptsDialog` | OK-Klick → `Accepted` | `result()` = `QDialog::Accepted` |
| `test_critical_hasIconLabel` | Icon-Label mit Pixmap vorhanden | `pixmap().isNull()` = false |
| `test_critical_okButtonHasNoIcon` (14.08.2026, Nessies Vorgabe) | Ok-Knopf ohne Icon | `icon().isNull()` = true |
| `test_critical_messageTextVisible` | Meldungstext in Label sichtbar | Label-Text = gesetzter Text |
| `test_information_canBeConstructed` | Dialog öffnet ohne Absturz | Fenstertitel korrekt |
| `test_information_hasSingleOkButton` | Genau ein OK-Button | `buttons.size()` = 1 |
| `test_information_hasIconLabel` | Icon-Label mit Pixmap vorhanden | `pixmap().isNull()` = false |
| `test_information_okButtonHasNoIcon` (14.08.2026, Nessies Vorgabe) | Ok-Knopf ohne Icon | `icon().isNull()` = true |
| `test_question_canBeConstructed` | Dialog öffnet ohne Absturz | Fenstertitel korrekt |
| `test_question_hasTwoButtons` | Genau zwei Buttons vorhanden | `buttons.size()` = 2 |
| `test_question_hasYesAndNoButtons` | Ja- und Nein-Button vorhanden | Labels enthalten "Ja" und "Nein" |
| `test_question_hasNoOkButton` | Kein OK-Button | Text ≠ "Ok" |
| `test_question_yesButtonAcceptsDialog` | Ja-Klick → `Accepted` | `result()` = `QDialog::Accepted` |
| `test_question_noButtonRejectsDialog` | Nein-Klick → `Rejected` | `result()` = `QDialog::Rejected` |
| `test_question_hasIconLabel` | Icon-Label mit Pixmap vorhanden | `pixmap().isNull()` = false |
| `test_staticCritical_doesNotCrash` | Konstruktion des Critical-Typs | Kein Absturz |
| `test_staticInformation_doesNotCrash` | Konstruktion des Information-Typs | Kein Absturz |
| `test_staticQuestion_doesNotCrash` | Konstruktion des Question-Typs | Kein Absturz |
| `test_minimumWidth_isAtLeast360` | Mindestbreite eingehalten | `minimumWidth()` ≥ 360 |
| `test_buttonHeight_matchesUiConstants` | Button-Höhe = `kButtonHeight` | `height()` = 24 |
| `test_isModal` | Dialog ist modal | `isModal()` = true |
| `test_longMessageText_doesNotCrash` | 500 Zeichen Meldungstext | Kein Absturz, Text korrekt |
| `test_multilineMessage_doesNotCrash` | Mehrzeiliger Meldungstext | Kein Absturz |

@note `test_critical_okButtonHasNoIcon`/`test_information_okButtonHasNoIcon`
(14.08.2026, Nessies Vorgabe): der Ok-Knopf trug zuvor das
`ButtonSave`-Icon (Diskette) — irreführend, da der Knopf nur den Dialog
schließt und nichts speichert. Betrifft nur Critical/Information; Ja/Nein
im Question-Typ behalten ihre Icons, ungetestet blieb das schon vorher.

---

### tests/forms/ — BackupProgressForm

@note Zu BackupWorker-Tests: `BackupWorker::run()` wird in den Tests synchron direkt
aufgerufen — kein Thread nötig. Signals werden via `QSignalSpy` geprüft.

@note Zu BackupProgressDialog-Tests: Der Dialog startet einen `QThread` im Konstruktor.
`%BackupProgressDialog::~BackupProgressDialog()` wartet seit dem Destruktor-Race-Fix
selbst aktiv (`quit()` + `wait()`) auf das tatsächliche Thread-Ende, bevor `~QObject()`
das `m_thread`-Kindobjekt zerstört — ein Test darf den Dialog daher inzwischen auch ohne
`waitForDialog()` gefahrlos zerstören (siehe `test_backupProgressDialog_destroyedImmediately_doesNotCrash`).
Die übrigen Tests rufen `waitForDialog()` trotzdem weiterhin auf — nicht zur Crash-Vermeidung,
sondern weil sie danach `wasSuccessful()` bzw. die kopierte Zieldatei prüfen wollen, was ohne
Warten auf den Abschluss des Kopiervorgangs nicht deterministisch wäre.
`waitForDialog()` ist eine statische Hilfsmethode in `TestBackupForm` die Events verarbeitet
bis `wasSuccessful()` true ist (max. 5 Sekunden).

TestBackupForm — BackupWorker:

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_backupWorker_copiesFileSuccessfully` | Datei wird vollständig kopiert | Zieldatei existiert, Größe identisch |
| `test_backupWorker_emitsProgressSignal` | progress-Signal wird emittiert | `spy.count()` ≥ 1, letzter bytesWritten = totalBytes |
| `test_backupWorker_emitsFinishedWithSuccess` | finished-Signal mit success=true | `spy.at(0).at(0).toBool()` = true |
| `test_backupWorker_missingSource_emitsFailure` | Fehlende Quelldatei → finished(false) | success = false, Zieldatei nicht erstellt |
| `test_backupWorker_cancel_removesPartialFile` | cancel() vor run() → Abbruch | success = false, Zieldatei gelöscht |

TestBackupForm — BackupProgressDialog:

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_backupProgressDialog_canBeConstructed` | Dialog öffnet ohne Absturz | Titel enthält "Backup" |
| `test_backupProgressDialog_isModal` | Dialog ist modal | `isModal()` = true |
| `test_backupProgressDialog_hasCancelButton` | Abbrechen-Button vorhanden | Text enthält "Abbrechen" |
| `test_backupProgressDialog_hasProgressBar` | Fortschrittsbalken vorhanden | Range 0–100 |
| `test_backupProgressDialog_successfulCopy_wasSuccessfulTrue` | Erfolgreiche Kopie | `wasSuccessful()` = true, Zieldatei existiert |
| `test_backupProgressDialog_destroyedImmediately_doesNotCrash` | Regression: Dialog wird zerstört bevor der Worker-Thread sein `finished()`/`quit()` durchlaufen hat (`wasSuccessful()` ggf. noch `false`) — Destruktor muss aktiv auf Thread-Ende warten statt einen laufenden `QThread` zu zerstören | Destruktor kehrt zurück ohne Absturz/Warnung; kein `waitForDialog()`-Aufruf |

TestBackupForm — createBackup via MainWindow:

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_createBackup_createsBackupFile` | Backup-Datei wird angelegt | `Backup_MyPortfolio_*.db` existiert |
| `test_createBackup_filenameContainsOriginalName` | Dateiname enthält Original-Namen | Dateiname beginnt mit `Backup_ShareList_` |
| `test_createBackup_keepsMaxFiveBackups` | Rotation auf max. 5 Backups | `backups.size()` ≤ 5 nach 6. Backup |

---

### PortfolioSeriesCalculator (tests/utils/tst_portfolioseriescalculator.cpp)

Executable: `tst_portfolioseriescalculator`
Klasse unter Test: `PortfolioSeriesCalculator` — der Rechenkern des
Depotwert-Charts (Feature 05.08.2026).

Anders als `tst_sharecalculator` braucht dieser Test **keine** Datenbank: der
Rechenkern ist bewusst datenbankfrei, alle Eingangsdaten kommen als einfache
Structs herein. Genau dafür wurde er aus dem Model herausgelöst. Die
Repository- und Model-Quellen hängen nur als Link-Abhängigkeit mit dran, weil
der Kern für die Rundung `ShareCalculator::roundAway()` verwendet und
`ShareCalculator.cpp` die Repositories einbindet.

Der zentrale Test ist `test_referenceScenario_twoSharesWithPartialSale` — das
mit Nessie Schritt für Schritt durchgerechnete Zwei-Aktien-Beispiel, dessen
Sollwerte er bestätigt hat (siehe ARCHITECTURE.md,
"PortfolioChartForm-Details"). Schlägt er fehl, hat sich die Formel geändert,
nicht der Test.

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_buildDateGrid_unionOfPriceAndTransactionDates` | Datumsraster über zwei Aktien | Vereinigungsmenge, sortiert, duplikatfrei |
| `test_buildDateGrid_includesTransactionDateWithoutPrice` | Kosteneintrag an einem Tag ohne Kurs | Der Tag wird trotzdem Rasterpunkt |
| `test_buildDateGrid_respectsWindow` | Fensterbegrenzung | Nur Daten in [from, to] |
| `test_closingPriceAt_forwardFillsLastKnownPrice` | Vorwärts-Fortschreibung | Letzter bekannter Kurs gilt weiter |
| `test_closingPriceAt_zeroBeforeFirstEntry` | Vor dem ersten Kurs | 0,00 |
| `test_forwardFill_gapDayDoesNotDentThePortfolioSum` | Aktie ohne Eintrag am Stichtag | Summe bricht nicht ein (150,00) |
| `test_referenceScenario_twoSharesWithPartialSale` | Referenzbeispiel | -10 / +82 / +114 / +160 / +259 |
| `test_referenceScenario_componentsOfLastPoint` | Bestandteile des letzten Punkts | Bestandswert 1794, realis. 65, Div 22, Kosten 22, gehalten 1600, gesamt 2000 |
| `test_referenceScenario_percentUsesTotalPurchaseValue` | Prozent-Nenner | 259 / 2000 = 12,95 % |
| `test_buyDoesNotMoveTheLine` | Nachkauf über 5.000 Euro | Linie unverändert (+100,00 nur aus dem Kursgewinn) |
| `test_completeSale_lineStaysFlatAfterwards` | Komplettverkauf | -10 / +90 / +105 / +205 / +170 / +170; gehalten 0, gesamt 1000, 17,00 % |
| `test_fifo_usesOldestLotFirst` | Zwei Lots zu 100 und 200 | Realis. 200,00 (FIFO), nicht 100,00 und kein Mittelwert |
| `test_shareWithoutHistory_isExcludedAndReported` | Aktie ohne Tageswerte | Vollständig ausgeschlossen, Name gemeldet, ihre Kosten fliessen nicht ein |
| `test_shareContributesNothingBeforeItsFirstPriceDate` | Kauf vor Historienbeginn | Kein Phantom-Verlust am linken Rand |
| `test_emptyInput_yieldsEmptyResult` | Leeres Portfolio | Keine Punkte, keine Meldung |
| `test_percentIsZeroWhenNothingWasEverBought` | Kaufwert 0 | Guard greift, 0,00 % statt Division durch null |
| `test_unsortedInputIsSortedInternally` | Unsortierte Eingangslisten | Werden intern sortiert, FIFO bleibt korrekt |
| `test_windowLimitsPointsButNotAccumulatedState` | Fenster ab März | Drei Punkte, aufgelaufener Zustand bleibt erhalten |

---

### ShareCalculator (tests/utils/tst_sharecalculator.cpp)

`ShareCalculator::compute()` ist eine reine Berechnungsfunktion ohne UI oder
Netzwerk. Da sie Käufe, Verkäufe, Brokerage und Dividenden frisch aus den
Repositories liest, läuft der Test gegen eine echte In-Memory-SQLite-Datenbank
(`Database::instance().open(":memory:")`) — dasselbe Muster wie die
Repository-Tests. Die Helfer `addBuy()`/`addSale()` legen Käufe samt verknüpfter
Kauf-Brokerage (`brokerage.buy_guid`) bzw. Verkäufe samt Verkaufs-Brokerage
(geladen via JOIN über `brokerage_guid`) an; `addDividend()` legt eine Dividende
an (`rate * volume` abzüglich Steuer). `init()` räumt FK-sicher auf
(`sale_buy_details`, `sales`, `brokerage`, `buys`, `dividends`).

Geprüft wird vor allem der Marktwert-Tab inklusive der beim Port korrigierten
Logik. Die Sollwerte sind gegen die C#-Referenz abgeglichen.

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_roundAway_halfAwayFromZero` | Cent-Rundung | half-away-from-zero, positiv/negativ |
| `test_marktwert_coreScenario` | Kernbeispiel (2 Käufe, 1 Verkauf) | `purchaseValue` (1200,00), `curValue`, `profitLoss` (175,00), `completeProfitLossMarket` (281,04), `completeCurValueMarket` (1881,04), `salePayoutMarket` (517,00) + Depotwert-Basis |
| `test_depotwert_finalFields` | Depotwert-Tab (mit Brokerage), per-Lot-Zuordnung — gleiche Fixture | `profitLossFinal` (166,06), `profitLossPctFinal`, `purchaseValueFinal` (1208,94), `completeCurValue` (1885,00), `completeProfitLoss` (272,10), `completeProfitPct` |
| `test_depotwert_partialLotBrokerageAndReduction` | Brokerage UND Rabatt anteilig auf teilverkauftem Lot (6/10 gehalten) | `purchaseValueFinal` (602,65, nicht 604,43), `purchaseValue` (600,00 — Marktwert schließt jetzt auch Rabatt aus), `profitLossFinal` (-2,65) — Voll-Zuordnung explizit ausgeschlossen |
| `test_depotwert_dividendInCompleteValue` | Netto-Dividende fliesst in die Komplett-Spalten | `completeCurValue` (1012,00 statt 1000,00), `completeProfitLoss` (12,00) |
| `test_marktwert_emptyDetails_sameResult` | Regression "viel zu hoch" | Ergebnis identisch trotz **leerer** `SaleBuyDetails` (Aggregat-basiert); `completeProfitLossMarket` (281,04) |
| `test_marktwert_columnIdentity` | Spalten-Identität | `Kpl. Marktwert = Kpl. Einzahlung + Kpl. Entwicklung` |
| `test_marktwert_fullySold` | Position komplett verkauft | `volume = 0`, `purchaseValue = 0`, realisierte G/V mit Gebühren |
| `test_depotwert_saleProfitLossFinal_matchesRealizedWhenFullySold` | Wie `test_marktwert_fullySold` (gleiche Fixture) | `saleProfitLossFinal == completeProfitLossMarket` (Algebra-Invariante, held = 0), `salePayoutFinal` (1292,00) |
| `test_marktwert_noSales` | keine Verkäufe | reine unrealisierte Entwicklung, `Kpl. Marktwert == curValue` |
| `test_prevDay_diffAndPct` | Vortagswerte | `prevDayDiff`, `prevDayPct` |
| `test_split_heldVolumeAndCurValueUseTodayScale` | Aktiensplit Phase 2a, 07.08.2026: Kauf vor einem 20:1-Split (Alphabet-Fixture, 5 Stück à 1.003,00 €), keine Verkäufe | `volume` (100,0), `curValue`/`purchaseValue` (5.015,00), `profitLoss` (0,00) |
| `test_split_reverseSplit_scalesDownHeldVolume` | Reverse-Split 1:10 (100 Stück à 5,00 € → 10 Stück à 50,00 €) | `volume` (10,0), `curValue`/`purchaseValue` (500,00) |
| `test_split_realizedAndHeldValuesUseTodayScale` | Kauf vor dem Split (teilverkauft in Beleg-Skala), Verkauf danach — Bestands- **und** realisierte Seite in einer split-übergreifenden Position | `volume` (60,0), `curValue` (3.300,00), `purchaseValue` (3.000,00), `profitLoss` (300,00), `saleProfitLoss` (400,00), `completePurchaseMarket` (5.000,00), `completeProfitLossMarket` (700,00), `completeCurValueMarket` (5.700,00) |
| `test_split_brokerageStaysUnscaled` | Brokerage ist ein Geldbetrag, nicht stückbezogen — nur die Pro-Lot-Fraktion nutzt die split-bereinigten Stückzahlen | `purchaseValueFinal` (3.012,00 = heldBuyValue 3.000,00 + heldBrokerage round(20,00×0,6)=12,00) |

@note **Rückwärtskompatibilität:** Alle Tests oberhalb dieser Zeile legen
keine Splits an und decken damit ab, dass `ShareCalculator::compute()` ohne
gespeicherte Splits bitgenau dasselbe Ergebnis liefert wie vor der
Split-Umrechnung (Faktor 1,0 überall, Division/Multiplikation mit 1,0 ist in
IEEE 754 exakt) — kein einziger bestehender Test musste für Phase 2a
angepasst werden.

`TwoLineDelegate` und `CenterIconDelegate` sind Header-only ohne `Q_OBJECT` —
kein eigenständiger Test nötig.

@note **Bugfix Grid-Selektionsfarbe (29.07.2026, siehe ARCHITECTURE.md,
"TwoLineDelegate"):** Ein erster Fixversuch in `TwoLineDelegate::paint()`
(`opt.widget->style()` statt `QApplication::style()`) reichte nicht aus, da
Qt eine per Stylesheet gesetzte `item:selected`-Farbe nicht in eine über
`QPalette` abfragbare Farbe zurückspiegelt. Endgültiger Fix verwendet bei
Selektion direkt `GridStyle::kSelectionBackground`/`kSelectionForeground`
statt Style/Palette-Abfragen. Kein eigener automatisierter Test ergänzt —
reine `QPainter`-Zeichenlogik ohne öffentliches Zustands-API, Verifikation
weiterhin visuell durch Nessie, analog zu den übrigen Farb-Defaults dieser
Delegates.

@note **Bugfix (10.07.2026):** Rabatt (`reduction`) wurde in den Marktwert-
Feldern (`purchaseValueMarket`, `completePurchaseMarket`, `salePayoutMarket`)
weiterhin verrechnet, obwohl Brokerage dort schon ausgeschlossen war —
inkonsistent, da Rabatt fachlich zur Brokerage gehört (Kosten-Rabatt-Paar in
der C#-Referenz). Von Nessie bestätigt und behoben. Betrifft `test_marktwert_
coreScenario`, `test_depotwert_partialLotBrokerageAndReduction` und
`test_marktwert_emptyDetails_sameResult` — Sollwerte oben entsprechend neu
durchgerechnet (`completeCurValueMarket` bleibt bei 100%-gehaltenen Buys
algebraisch unverändert, `completePurchaseMarket`/`completeProfitLossMarket`
einzeln ändern sich). Alle anderen Tests nutzen Fixtures mit `reduction = 0`
und sind unverändert.

@note **`salePayoutFinal`/`saleProfitLossFinal` (erledigt 09.07.2026),
`salePayoutMarket` (erledigt 10.07.2026):** `test_depotwert_finalFields`
prüft `salePayoutFinal` (510,00€) und `saleProfitLossFinal` (106,04€) anhand
der Kernfixture, mit Cross-Check gegen `completeProfitLoss`.
`test_marktwert_coreScenario` prüft zusätzlich `salePayoutMarket` (517,00€,
gleiche Fixture, ohne Brokerage und ohne Rabatt). Ergänzend prüft
`test_depotwert_saleProfitLossFinal_matchesRealizedWhenFullySold` die reine
Algebra-Invarianz `saleProfitLossFinal == completeProfitLossMarket` im
Fully-Sold-Fall (held = 0), unabhängig von Hand nachgerechneten Zahlen.

@note **Offen:** `saleProfitLoss` und `marketValue` (beide bereits vor dieser
Iteration vorhanden) werden jetzt auch von `PresenterShareDetails` für die
Marktwert-"Aktuelle Bestandsberechnung"-Box verwendet, haben in
`tst_sharecalculator.cpp` aber weiterhin keine direkte, isolierte Prüfung —
nur indirekt über `completeProfitLossMarket`/`completeCurValueMarket`, die
denselben `saleProfitLossMarket`-Rohwert anders verrechnen. Vorbestehende
Lücke, keine Regression durch diese Iteration, aber jetzt mit höherer
praktischer Relevanz.

---

### ShareSplitAdjuster (tests/utils/tst_sharesplitadjuster.cpp)

Executable: `tst_sharesplitadjuster`
Klasse unter Test: `ShareSplitAdjuster` — der Rechenkern der Aktiensplit-
Behandlung, Phase 1 (siehe ARCHITECTURE.md, "Offene Punkte", "Aktiensplits
werden nicht behandelt").

Anders als `tst_sharecalculator` braucht dieser Test **keine** Datenbank:
`ShareSplitAdjuster` ist bewusst zustandslos und datenbankfrei, alle
Eingangsdaten kommen als `QList<ShareSplitObject>` herein — gleicher Ansatz
wie `PortfolioSeriesCalculator`. Die Fixture-Werte spiegeln den Alphabet-
Fall aus der Architektur-Doku: ein Kauf von 5 Stück zu 1.003,00 € am
18.03.2020, 20:1-Split zum Ex-Tag 18.07.2022.

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_volumeFactor_noSplits_returnsOne` | Keine Splits | Faktor 1,0 |
| `test_volumeFactor_splitAfterDate_applies` | Split nach dem Stichtag | Faktor 20,0 |
| `test_volumeFactor_splitOnOrBeforeDate_doesNotApply` | Split am/vor dem Stichtag | Faktor bleibt 1,0 — der Beleg des Splittags selbst liegt fachlich vor dem Split |
| `test_volumeFactor_multipleSplits_cumulate` | Zwei Splits (4:1, dann 20:1), Stichtag vor beiden | Faktor 80,0 (kumuliert) |
| `test_volumeFactor_dateBetweenTwoSplits_onlyLaterApplies` | Stichtag zwischen den beiden Splits | Faktor 20,0, nicht 80,0 |
| `test_volumeFactor_reverseSplit_isFractional` | Reverse-Split 1:10 | Faktor 0,1 |
| `test_volumeFactor_unsortedInput_stillCumulatesCorrectly` | Splits in beliebiger Reihenfolge übergeben | Ergebnis unverändert (80,0) |
| `test_volumeFactor_dateAfterAllSplits_returnsOne` | Stichtag nach allen Splits | Faktor 1,0 |
| `test_priceFactorForHistory_unadjustedSplit_applies` | `pricesAdjusted = false` | Faktor 20,0 |
| `test_priceFactorForHistory_adjustedSplit_doesNotApply` | `pricesAdjusted = true` | Faktor 1,0 — Gegenstück zum Alphabet-Fall |
| `test_priceFactorForHistory_mixedSplits_onlyUnadjustedCumulate` | Ein bereinigter + ein unbereinigter Split | Nur der unbereinigte trägt bei (20,0, nicht 80,0) |
| `test_adjustedVolume_scalesUp` | 5 Stück, Faktor 20 | 100,0 |
| `test_adjustedTransactionPrice_scalesDown` | Alphabet-Fall: 1.003,00 € Beleg-Kurs | 50,15 € |
| `test_adjustedTransactionPrice_valueInvariant` | Stückzahl × Preis vor/nach Umrechnung | Exakt gleich (Algebra-Invariante) |
| `test_adjustedTransactionPrice_noSplits_isUnchanged` | Keine Splits | Preis unverändert |
| `test_adjustedHistoryPrice_unadjustedHistory_isScaledDown` | Alphabet-Fall, Tageswert | 1.003,00 € → 50,15 € |
| `test_adjustedHistoryPrice_alreadyAdjustedHistory_isUnchanged` | Bereits bereinigte Historie | Kurs unverändert |
| `test_dateAfterSplit_pricesAndVolumesUnchanged` | Stichtag nach dem Split | Stückzahl und beide Preis-Umrechnungen unverändert |

---

### ShareSplitHint (tests/utils/tst_sharesplithint.cpp)

Executable: `tst_sharesplithint`
Klasse unter Test: `ShareSplitHint`

Formatierung der Split-Hinweise unter den Kauf- und Verkaufsdaten, Phase 3b
der Aktiensplit-Behandlung (09.08.2026, siehe ARCHITECTURE.md,
"Split-Hinweis in den Editier-Dialogen").

@note Kein `QCoreApplication` in `main()` — der Helfer ist zustandslos, greift
nicht auf Qt SQL zu und instanziiert keine Widgets, gleiche Bauweise wie
`tst_sharesplitadjuster`. `QLocale::setDefault(QLocale::German)` wird
trotzdem gesetzt: der Helfer formatiert über `QLocale()`, und CI-Runner laufen
nicht mit deutschem Locale.

hasSplitAfter():

| Test | Prüft |
| ---- | ----- |
| `test_hasSplitAfter_emptyList_false` | Keine Splits → false |
| `test_hasSplitAfter_splitLater_true` | Split nach dem Belegdatum → true |
| `test_hasSplitAfter_splitEarlier_false` | Split vor dem Belegdatum → false |
| `test_hasSplitAfter_splitOnSameDay_false` | Split AM Belegdatum → false |
| `test_hasSplitAfter_invalidDate_false` | Ungültiges Datum → false, kein Absturz |

footerText():

| Test | Prüft |
| ---- | ----- |
| `test_footerText_noSplits_mentionsCurrentState` | Text ist auch ohne Split belegt |
| `test_footerText_onlyEarlierSplits_mentionsCurrentState` | Frühere Splits zählen nicht |
| `test_footerText_singleSplit_showsRatioDateAndConversion` | Verhältnis, Datum, umgerechnete Stückzahl und Preis |
| `test_footerText_singleSplit_productStaysEqual` | 5 × 1.003,00 € = 100 × 50,15 € |
| `test_footerText_reverseSplit_scalesDown` | 1:10 → aus 100 à 5,00 € werden 10 à 50,00 € |
| `test_footerText_multipleSplits_showsCountAndLatest` | "2 Splits", jüngstes Verhältnis, kumulierter Faktor 80 |
| `test_footerText_multipleSplits_onlyCountsThoseAfterTheDate` | Beleg zwischen zwei Splits sieht nur den späteren |
| `test_footerText_unsortedSplits_stillNamesTheLatest` | Unsortierte Eingabe liefert dasselbe Ergebnis |
| `test_footerText_pricesAdjustedFlag_isIrrelevantHere` | `prices_adjusted` verändert den Hinweis nicht |
| `test_footerText_zeroVolume_doesNotCrash` | Frisch geöffnetes Formular (0 Stück, 0 €) |
| `test_footerText_fractionalRatio_keepsDecimals` | 3:2 wird nicht auf ganze Zahlen gerundet |

tooltipText() und Formatierung:

| Test | Prüft |
| ---- | ----- |
| `test_tooltipText_noSplits_isEmpty` | Leerer Tooltip ohne Splits |
| `test_tooltipText_listsAllSplitsAfterTheDate` | Beide Splits, eine Zeile je Split |
| `test_tooltipText_skipsSplitsBeforeTheDate` | Frühere Splits fehlen |
| `test_describeSplit_wholeRatioHasNoDecimals` | "20:1", nicht "20,00:1,00" |
| `test_describeSplit_reverseSplitKeepsOrder` | "1:10", nicht "10:1" |
| `test_formatRatioPart_fractionalKeepsTwoDecimals` | 1,5 bleibt 1,50 |
| `test_formatRatioPart_wholeNumberHasNoDecimals` | 20,0 wird "20" |

@note `test_footerText_pricesAdjustedFlag_isIrrelevantHere` sichert eine
Unterscheidung, die leicht verrutscht: `prices_adjusted` betrifft
ausschliesslich die Tageswert-Historie (`priceFactorForHistory()`). Belege
liegen IMMER in Beleg-Skala vor. Würde der Hinweis versehentlich den
History-Faktor verwenden, zeigte er bei einem bereits bereinigten Split gar
keine Umrechnung mehr an — obwohl die Stückzahl auf dem Beleg sehr wohl eine
alte ist.

@note `test_footerText_unsortedSplits_stillNamesTheLatest` prüft eine
Eigenschaft, die im laufenden Betrieb nie gebraucht wird —
`ShareSplitRepository::findByShare()` liefert stets sortiert. Der Helfer ist
aber öffentlich und wird von zwei Presentern aufgerufen; verlässt er sich
stillschweigend auf die Sortierung, nennt er beim ersten Aufrufer mit anderer
Reihenfolge den falschen Splittag.

@note `test_footerText_singleSplit_productStaysEqual` ist der Test, der den
eigentlichen Zweck des Hinweises festhält. Stückzahl mal Preis muss über einen
Split hinweg gleich bleiben; würde nur die Stückzahl umgerechnet, sähe der
Nutzer eine scheinbare Wertvervielfachung.

---

### SplitPriceJumpDetector (tests/utils/tst_splitpricejumpdetector.cpp)

Executable: `tst_splitpricejumpdetector`
Klasse unter Test: `SplitPriceJumpDetector` — zustandsloser, DB-freier
Heuristik-Helfer hinter dem "Prüfen"-Knopf im Split-Dialog (13.08.2026,
siehe ARCHITECTURE.md, "Automatische Erkennung split-bereinigter
Kurshistorie"). Fixture-Werte lehnen sich lose an den Alphabet-Fall an
(20:1-Split, Ex-Tag 18.07.2022).

@note Kein `QCoreApplication` in `main()` — gleiche Bauweise wie
`tst_sharesplitadjuster`/`tst_sharesplithint`: der Helfer ist zustandslos
und greift nicht auf Qt SQL zu.

Sprung erkannt (`Result::NotAdjusted`):

| Test | Prüft |
| ---- | ----- |
| `test_detect_clearJump_returnsNotAdjusted` | Kurs springt vor/nach dem Ex-Tag um ~Faktor 20 |
| `test_detect_reverseSplit_smallFactor_stillDetectsJump` | Reverse-Split 1:10 (Faktor 0,1): Kurs steigt statt zu fallen |

Kein Sprung (`Result::Adjusted`):

| Test | Prüft |
| ---- | ----- |
| `test_detect_noJump_returnsAdjusted` | Kurs bleibt über den Ex-Tag hinweg im selben Bereich |

Uneindeutig (`Result::Ambiguous`):

| Test | Prüft |
| ---- | ----- |
| `test_detect_ratioBetweenBands_returnsAmbiguous` | Verhältnis 5,0 liegt weder nah bei 1,0 noch nah beim Faktor 20 |
| `test_detect_smallFactor_overlappingBands_returnsAmbiguous` | Faktor 1,25 (5:4): Toleranzbänder um 1,0 (±15 %) und um 1,25 (±20 %) überlappen sich |

Nicht genug Daten (`Result::InsufficientData`):

| Test | Prüft |
| ---- | ----- |
| `test_detect_noDataAtAll_returnsInsufficientData` | Leere Kurshistorie |
| `test_detect_onlyDataBefore_returnsInsufficientData` | Nur ein Kurs vor dem Ex-Tag, keiner danach |
| `test_detect_onlyDataAfter_returnsInsufficientData` | Nur ein Kurs nach dem Ex-Tag, keiner davor |
| `test_detect_dataOutsideLookbackWindow_ignored` | Kurs weit außerhalb des Standardfensters (15 Tage) zählt nicht |
| `test_detect_invalidExDate_returnsInsufficientData` | Ungültiges Ex-Tag-Datum |
| `test_detect_zeroFactor_returnsInsufficientData` | Faktor 0,0 |

Ex-Tag und Nachbar-Splits als Fenstergrenzen:

| Test | Prüft |
| ---- | ----- |
| `test_detect_priceOnExDateItself_countsAsBefore` | Ein Kurs GENAU am Ex-Tag zählt als "davor" — dieselbe Konvention wie `ShareSplitAdjuster::volumeFactor()` |
| `test_detect_previousSplitDate_boundsWindowStart` | Ein früherer Nachbar-Split begrenzt das Suchfenster nach hinten |
| `test_detect_nextSplitDate_boundsWindowEnd_inclusive` | Ein Kurs genau am späteren Nachbar-Split zählt noch mit (inklusive) |
| `test_detect_dataAfterNextSplitDate_excluded` | Ein Kurs nach dem späteren Nachbar-Split zählt nicht mehr |

Nächstgelegener Kurs:

| Test | Prüft |
| ---- | ----- |
| `test_detect_picksNearestPriceOnEachSide` | Wählt den jeweils nächstgelegenen Kurs vor/nach dem Ex-Tag, nicht irgendeinen aus dem Fenster |

@note `test_detect_priceOnExDateItself_countsAsBefore` sichert dieselbe
Randregel wie `ShareSplitAdjuster::volumeFactor()` — siehe auch
"Bruchstücke bei Reverse-Splits nicht abgedeckt" in ARCHITECTURE.md, wo
genau diese Regel die Grundlage für das dortige Vorgehen ist.

---

### SplitAdjustmentAudit (tests/utils/tst_splitadjustmentaudit.cpp)

Executable: `tst_splitadjustmentaudit`
Klasse unter Test: `SplitAdjustmentAudit` — zustandsloser, DB-freier
Helfer hinter Phase 4b der Aktiensplit-Behandlung (20.08.2026, siehe
ARCHITECTURE.md, "Automatische Nachprüfung nach Tageswert-Abruf"). Baut auf
`SplitPriceJumpDetector` auf: `check(splits, dailyValues)` vergleicht dessen
Ergebnis je Split gegen das gespeicherte `ShareSplitObject::pricesAdjusted()`
und meldet Widersprüche, schreibt selbst nichts. Fixture-Werte teils
identisch mit `tst_splitpricejumpdetector` (Alphabet-Fall, 20:1-Split,
Ex-Tag 18.07.2022).

@note Kein `QCoreApplication` in `main()`, gleiche Bauweise wie
`tst_splitpricejumpdetector`.

| Test | Prüft |
| ---- | ----- |
| `test_check_noSplits_returnsEmpty` | Leere Split-Liste -> leeres Ergebnis |
| `test_check_storedNotAdjusted_detectedAdjusted_reportsDiscrepancy` | Gespeichert unbereinigt, aber kein Kurssprung erkannt -> Widerspruch |
| `test_check_storedAdjusted_detectedNotAdjusted_reportsDiscrepancy` | Gespeichert bereinigt, aber Kurssprung um den Faktor erkannt -> Widerspruch |
| `test_check_storedNotAdjusted_detectedNotAdjusted_noDiscrepancy` | Gespeicherter Zustand passt zur Historie -> kein Widerspruch |
| `test_check_storedAdjusted_detectedAdjusted_noDiscrepancy` | Gespeicherter Zustand passt zur Historie -> kein Widerspruch |
| `test_check_ambiguousResult_neverReportsDiscrepancy` | `Ambiguous`-Ergebnis zählt nie als Widerspruch, unabhängig vom gespeicherten Zustand |
| `test_check_insufficientData_neverReportsDiscrepancy` | `InsufficientData`-Ergebnis zählt nie als Widerspruch, unabhängig vom gespeicherten Zustand |
| `test_check_multipleSplits_onlyContradictingOneReported` | Von zwei Splits landet nur der tatsächlich widersprechende im Ergebnis |
| `test_check_neighborSplit_boundsWindow_perSplit` | Nachbar-Splits begrenzen das Suchfenster je geprüftem Split — dieselbe Logik wie `PresenterShareSplitEdit::onCheckPriceJump()` |
| `test_check_resultOrder_matchesInputOrder` | Mehrere Widersprüche erscheinen in der Reihenfolge der Eingabeliste |

@note `test_check_ambiguousResult_neverReportsDiscrepancy` und
`test_check_insufficientData_neverReportsDiscrepancy` sind der eigentliche
Kern der Klasse: falscher Alarm bei unsicherer Datenlage wäre schädlicher
als ein übersehener echter Widerspruch, weil er das Vertrauen in die
Startmeldung untergräbt (dieselbe Vorsicht wie beim "Prüfen"-Knopf, siehe
`tst_splitpricejumpdetector` oben).

---

### SaleFifoAllocator (tests/utils/tst_salefifoallocator.cpp)

Executable: `tst_salefifoallocator`
Klasse unter Test: `SaleFifoAllocator` — die gemeinsame, split-bewusste
FIFO-Verkaufszuteilung, Phase 2c der Aktiensplit-Behandlung (07.08.2026,
siehe ARCHITECTURE.md "Offene Punkte"). Ersetzt die vormals dreifach
duplizierte FIFO-Schleife in `PresenterSaleEdit`/`ViewSaleEdit`.

Wie `tst_sharesplitadjuster` zustandslos und datenbankfrei — `BuyObject.cpp`
wird nur wegen dessen Konstruktor gebraucht. Die split-übergreifenden
Fixture-Werte sind gegen eine unabhängige Python-Simulation der
Zuteilungslogik gegengerechnet.

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_allocate_singleBuy_fullyCovers` | Ein Kauf deckt die Verkaufsmenge vollständig | 1 Zeile, Volumen = Verkaufsmenge |
| `test_allocate_multipleBuys_fifoOrder` | Zwei Käufe, älterer zuerst voll, Rest vom jüngeren | 2 Zeilen, `10,0` dann `5,0` |
| `test_allocate_insufficientVolume_stopsWhenExhausted` | Weniger verfügbar als nachgefragt | 1 Zeile, Rest bleibt offen (unverändertes Altverhalten) |
| `test_allocate_fullyConsumedBuy_isSkipped` | Ein Kauf mit `volumeSold == volume` | Wird übersprungen, nur der offene Kauf erscheint |
| `test_allocate_zeroSaleVolume_returnsEmpty` | Verkaufsmenge 0 | Leeres Ergebnis |
| `test_allocate_emptyAvailableBuys_returnsEmpty` | Keine verfügbaren Käufe | Leeres Ergebnis |
| `test_allocate_splitBetweenBuyAndSale_scalesToBuysBelegSkala` | Kauf vor, Verkauf nach einem 20:1-Split | Zugeteiltes Volumen in der Beleg-Skala des Kaufs (2,0 statt 40,0) |
| `test_allocate_splitBetweenBuyAndSale_valueInvariant` | Derselbe Fall | Euro-Wert (Volumen × Kaufkurs) bleibt exakt 2.000,00 € |
| `test_allocate_reverseSplitBetweenBuyAndSale` | Reverse-Split 1:10 zwischen Kauf und Verkauf | Zugeteiltes Volumen 90,0 (Beleg-Skala) |
| `test_allocate_multipleBuysAcrossSplitBoundary` | Ein Kauf vor, einer nach demselben Split | Beide Zeilen korrekt in ihrer jeweils eigenen Beleg-Skala |
| `test_allocate_noSplits_matchesLegacyBehavior` | Ohne Splits | Bitgenau wie die ursprüngliche, unskalierte FIFO-Schleife |

---

### Refresh-Flow (Kursdaten-Abruf) — teilweise umgesetzt (07.07.2026)

Der Kursdaten-Abruf (`onRefreshShare`, `onRefreshAll`, `buildDailyValuesUrl`,
`onMarketValuesUpdated`, `onDailyValuesUpdated`) ist direkt in `MainWindow`
implementiert und erfordert echte Netzwerkzugriffe. Der bisherige Blocker —
kein Weg, `ParserLib::Parser` in Tests von echtem Netzwerk zu entkoppeln — ist
seit 07.07.2026 vollständig behoben:

1. `Parser` besitzt einen Konstruktor zur `QNetworkAccessManager`-Injection;
   `ParserTestUtils::FakeNetworkAccessManager` (`tests/parser/FakeNetworkAccessManager.h/.cpp`)
   liefert vorab definierte Antworten ohne echten Netzwerkzugriff (siehe
   ARCHITECTURE.md, "Erledigt / Archiv", sowie den `tst_parser`-Testblock
   weiter oben).
2. `MainWindow` besitzt jetzt ebenfalls einen Test-Konstruktor:
   `MainWindow(QNetworkAccessManager* networkManagerForTesting, QWidget* parent = nullptr)`
   reicht den injizierten (Fake-)`QNetworkAccessManager` an **beide** internen
   Parser (`m_parserMarketValues`, `m_parserDailyValues`) durch. Umgesetzt als
   verhaltensneutraler Refaktor: der komplette bisherige Konstruktor-Body
   wurde in eine private `initialize()`-Methode ausgelagert, die von beiden
   Konstruktoren aufgerufen wird — einzig die Parser-Member-Initialisierung
   unterscheidet sich zwischen den beiden Konstruktoren (Member-Initializer-
   Liste, vor `initialize()`). Der Produktions-Konstruktor
   `MainWindow(QWidget*)` ist dadurch unverändert im Verhalten.
   `tests/forms/CMakeLists.txt` bindet `FakeNetworkAccessManager.h/.cpp` aus
   `tests/parser/` sowie `Qt6::Network` für `tst_mainwindow` ein.

Erste Tests, die den kompletten Pfad `startRefreshForShare()` →
`onMarketValuesUpdated()` → Grid-Update über die echte Produktionslogik (kein
Fake der Geschäftslogik, nur der Netzwerkantwort) abdecken:

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_onRefreshShare_iconRegression_updatesChartIconsViaFakeNetwork` | **Regressionstest Bugfix 06.07.2026:** Aktie mit initial negativem `prevDayPct` (Icon `NegativStrong`), Fake-Netzwerk liefert Kurs mit `prevDayPct = +20%` | Nach `onRefreshShare()` zeigen `PrevDayChart` **und** `CompleteChart` in **beiden** Tabellen (Depotwert + Marktwert) das Icon `PositivStrong`; `fakeNam.requestCount() == 1` |
| `test_onRefreshShare_busyGuard_selectionDuringRefreshDoesNotReenableActions` | `enableShareActions`-Busy-Guard (`setupCentralWidget()`): `onRefreshShare()` deaktiviert die Aktionen, `startRefreshForShare()` → `selectShareRow()` selektiert `m_marketValueTable` zum ersten Mal (echtes `selectionChanged()`), **bevor** ein Parser `startParsing()` aufruft | `m_actionEdit` bleibt disabled — deckte beim ersten Lauf einen echten Bugfix-07.07.2026-Regressionsfall auf, siehe unten |

Icon-Vergleich: `QIcon` hat kein sinnvolles `operator==` (vergleicht
Engine-Pointer-Identität, nicht Pixelinhalt) — `IconProvider::icon()` baut bei
jedem Aufruf ein frisches `QIcon` aus demselben Ressourcenpfad, daher sind
zwei "gleiche" Icons nie `==`. Der Testhelper `iconsEqual()` vergleicht
stattdessen `icon.pixmap(24,24).toImage()`.

### onDailyValuesUpdated()-Pfad — erledigt (08.07.2026)

Bislang war über `FakeNetworkAccessManager` nur der `MarketPrice`-Zweig
(`onMarketValuesUpdated()`) end-to-end abgedeckt. Analog dazu jetzt auch der
`DailyValues`-Zweig: Yahoo-History-JSON über Fake-Netzwerk, volle
Produktionslogik (`buildDailyValuesUrl()` → `ParserLib::Parser` →
`DailyValuesRepository::upsertList()`), keine eigene Attrappe der
Geschäftslogik.

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_onRefreshShare_dailyValuesOnly_upsertsIntoDailyValuesRepository_viaFakeNetwork` | Einzel-Aktie, `ShareUpdateType::DailyValues`, Yahoo-History mit 2 Handelstagen | `DailyValuesRepository::findByShare()` liefert 2 Einträge (aufsteigend nach Datum, closingPrice 141.5 / 143.0); Statusmeldung "Tageswerte aktualisiert: ... 2 Einträge geholt (Eingefügt: 2 / Aktualisiert: 0 / Unverändert: 0)" erscheint im Status-Log |
| `test_onRefreshAll_dailyValuesQueue_chainsAcrossTwoShares_viaFakeNetwork` | 2-Aktien-Queue, beide `DailyValues`-only | Reentrante Verkettung analog zu den `MarketPrice`-Queue-Tests (`m_marketDone` ist bei `DailyValues`-only von vornherein `true`, sodass `onDailyValuesUpdated()` allein `onRefreshShareFinished()` auslöst); `requestCount() == 2`; Selektion springt danach via `selectFirstShareRow()` auf Zeile 0; beide Aktien haben je 2 Tageswerte-Einträge |
| `test_onRefreshShare_bothUpdateType_updatesMarketPriceAndDailyValues_viaFakeNetwork` | Einzel-Aktie, `ShareUpdateType::Both` (OnVista-Realtime + Yahoo-History gleichzeitig) | Beide Parser laufen unabhängig; `onRefreshShareFinished()` (sichtbar über `finaliseRefresh()`/Re-Enable der Action) feuert erst nachdem **beide** `m_marketDone` und `m_dailyDone` `true` sind; `ShareObject::curPrice()` UND `DailyValuesRepository`-Einträge sind beide aktualisiert |
| `test_onRefreshShare_dailyValuesOnly_splitAdjustmentDiscrepancy_addsStatusMessage_viaFakeNetwork` | Einzel-Aktie mit einem `ShareSplitObject` (Ex-Tag 15.01.2024, 20:1, `pricesAdjusted=false`), dieselbe Yahoo-History-Fixture wie oben (141.5 am 15.01. / 143.0 am 16.01.) | `refreshSplitAdjustmentWarningsForShare()` (Phase 4b, siehe ARCHITECTURE.md "Automatische Nachprüfung nach Tageswert-Abruf") erkennt den Widerspruch — kein Kurssprung, aber als unbereinigt gespeichert — direkt nach dem Abruf; Statusmeldung "... — 1 Split(s) mit abweichendem Bereinigungs-Zustand erkannt" erscheint im Status-Log |

Da für frisch angelegte Aktien noch keine `daily_values` existieren, löst
`buildDailyValuesUrl()` für `ApiYahoo` deterministisch immer den
"noch keine Daten"-Zweig auf (`tpl.arg("20y")` → `...?range=20y`) — die
finale Request-URL ist damit ohne Sonderfall pro Aktie vorhersagbar. Das GUID
wird dazu bewusst NICHT als `%`-Platzhalter ins URL-Template eingebaut
(`QString::arg()` würde bei mehrfachem `%1` alle Vorkommen ersetzen, was mit
dem einzigen von `buildDailyValuesUrl()` selbst gefüllten `%1` = Periodencode
kollidieren würde), sondern per einfacher String-Konkatenation vor dem
verbleibenden `%1`.

### Grid-Selektion während "Alle aktualisieren" — erledigt (07.07.2026)

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_onRefreshAll_gridSelectionFollowsQueueProgress_viaFakeNetwork` | 2-Aktien-Queue über Fake-Netzwerk | Selektion folgt in **beiden** Tabellen jedem Queue-Schritt (Aktie A → Aktie B); nach erfolgreichem Abschluss springt die Selektion via `selectFirstShareRow()` auf Zeile 0 |
| `test_onRefreshAll_errorMidQueue_selectionStaysOnFailedShare_viaFakeNetwork` | 4-Aktien-Queue, Aktie B (Index 1) liefert `QNetworkReply::HostNotFoundError` | Aktien C/D werden nie angefragt (`fakeNam.requestCount() == 2`, Queue wird bei Fehler geleert statt pausiert); Selektion bleibt auf der fehlgeschlagenen Aktie B stehen, `selectFirstShareRow()` wird **nicht** aufgerufen |

@note Aktienanzahl bewusst **nie 3**: sowohl die Datentabelle als auch die
Footer-Tabelle (feste 3 Summenzeilen) haben bei 3 Zeilen dieselbe Spalten-
**und** Zeilenanzahl — `findFinalTable(window, 3)`/`findMarketTable(window, 3)`
wären dadurch zwischen Daten- und Footer-Tabelle mehrdeutig. Der Helper
`seedRefreshQueuePortfolio(shareCount, dbPath)` erzwingt das über einen
`Q_ASSERT`.

@note Reentrancy (Bugfix 05.07.2026) bedeutet, dass eine abgeschlossene Aktie
direkt aus demselben Callback heraus in `startParsing()` der nächsten Aktie
verkettet — ein Zwischenzustand mitten in der Queue lässt sich daher nicht
über einen festen `QTest::qWait()` zuverlässig einfangen (Race). Stattdessen
dient `fakeNam.requestCount()` als deterministischer Checkpoint:
`createRequest()` zählt synchron genau in dem Moment hoch, in dem
`startParsing()` aufgerufen wird — und das passiert in `startRefreshForShare()`
unmittelbar **nach** `selectShareRow()`. Sobald `requestCount()` auf den
erwarteten Wert gestiegen ist, steht die Selektion also bereits fest.

### Footer-Update bei Refresh — erledigt (07.07.2026)

`refreshPortfolioFooters()` wird aus `onRefreshShareFinished()` nur im
Erfolgsfall aufgerufen (vor dem Verketten zur nächsten Aktie in der Queue,
falls vorhanden) — im Fehlerfall (`m_errorOccurred`) kehrt die Methode vorher
zurück.

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_onRefreshShare_footerUpdatesImmediately_viaFakeNetwork` | Einzel-Refresh (keine Queue), Kurs springt von 0 auf 300 | Depotwert-Footer "Aktueller Depotstand" (Zeile 2) ändert sich gegenüber dem Vorher-Zustand |
| `test_onRefreshAll_footerUpdatesBetweenEachShare_viaFakeNetwork` | 2-Aktien-Queue, unterschiedliche neue Kurse pro Aktie | Footer ändert sich bereits zum Zeitpunkt, an dem Aktie B ihre Anfrage stellt (`fakeNam.requestCount() == 2`, Aktie B selbst also noch nicht fertig) — `refreshPortfolioFooters()` läuft für Aktie A nachweislich **vor** dem Verketten zu Aktie B, nicht erst am Ende der Queue; ändert sich danach ein weiteres Mal nach Abschluss von Aktie B |
| `test_onRefreshShare_footerNotUpdated_onNetworkError_viaFakeNetwork` | Einzel-Refresh liefert `QNetworkReply::HostNotFoundError` | Footer "Aktueller Depotstand" bleibt exakt unverändert |

@note Bewusst keine hartkodierten Erwarteten-Summen: Der Footer-Gesamtwert
wird von `ShareCalculator::portfolioTotalsFinal()` über Brokerage-/Dividenden-/
FIFO-Logik berechnet, die bereits an anderer Stelle eigenständig getestet ist
(siehe `tests/utils/tst_sharecalculator.cpp`). Eine zweite, von Hand
hergeleitete Erwartungssumme hier hätte primär das Risiko, die eigene
(möglicherweise falsche) Testarithmetik statt der eigentlichen Verdrahtungs-
frage zu prüfen — nämlich schlicht: läuft `refreshPortfolioFooters()`
überhaupt, und zu welchem Zeitpunkt. Die Tests vergleichen daher den
Footer-Text vor/nach Refresh auf Änderung/Gleichheit statt auf einen
bestimmten Zahlenwert.

### Vortag-Tooltip (Gesamtänderung) — erledigt (02.08.2026)

Feature (siehe ARCHITECTURE.md, "Vortag-Spalte + Piktogramm-Spalte: Tooltip
mit Gesamtänderung"): Hovern über die "Vortag"-Spalte **und** über die
Entwicklungs-Pfeil-Icon-Spalte davor (`PrevDayChart`) zeigt einen
zweizeiligen Tooltip — Zeile 1 nur die Beschriftung "Gesamtänderung Aktie:",
Zeile 2 der Rechenweg (`Anteile × Kurswert-Entw. = Ergebnis`, Anteile mit 4
statt 2 Nachkommastellen). Pro-Stück-Wert **und** Gesamtergebnis sind jeweils
**unabhängig voneinander** nach ihrem eigenen Vorzeichen eingefärbt
(grün/rot; HTML-`<span>` via `colorizeToolTip()`). Bei exakt 0 wird weder
Farbe noch führendes "+" angezeigt (`formatSignedMoney()` zeigt "+" nur bei
`value > 0.0`; `formatSignedMoneyMaybeColored()` lässt bei
`qFuzzyIsNull(value)` den Farb-Span ganz weg, sonst rendert `QToolTip`
aufgrund seiner eigenen `ToolTipText`-Palette sichtbar grau statt schwarz).
Zeile 2 steckt in einem `white-space:nowrap`-`<div>`, damit sie nicht
umbricht. Gesetzt in `populatePortfolioTables()`, `onMarketValuesUpdated()`
und (als Portfolio-Gesamtsumme, einzeilig — Beschriftung + farbiger Wert in
einer Zeile, ohne Rechenweg) `updatePortfolioFooters()`.

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_populatePortfolioTables_prevDayTooltip_showsVolumeTimesDiff` | Aktie mit 40 Stk., Kurs +12,30 € zum Vortag | `item(row, PrevDay)->toolTip()` **und** `item(row, PrevDayChart)->toolTip()` sind byte-für-byte identisch zum vollständig konstruierten erwarteten HTML-String (Beschriftung, 4-Nachkommastellen-Anteile, beide Werte grün eingefärbt, Ergebnis 492,00 €) — in beiden Tabellen (Depotwert + Marktwert) |
| `test_populatePortfolioTables_prevDayTooltip_colorsIndependently` | Aktie mit `prevDayDiff = +10,00 €`, aber ohne jeden Kauf (`volume == 0`, Gesamtergebnis daher 0) | Pro-Stück-Wert trägt den grünen Hex-Farbcode aus `AppSettings::instance().logColorAt(5)`, das Gesamtergebnis dagegen `"0,00 €"` ganz ohne Farb-Span und ohne "+" — exakter `QCOMPARE()` gegen den vollständigen erwarteten Tooltip-String, belegt die Unabhängigkeit beider Farben |
| `test_populatePortfolioTables_prevDayTooltip_neutralWhenPriceUnchanged` | Aktie mit 20 Stk., aber `curPrice == prevDayPrice` (`prevDayDiff == 0`) | Beide Werte (Pro-Stück **und** Gesamtergebnis) erscheinen als `"0,00 €"` ohne "+" und ohne `color:`-Span, exakter `QCOMPARE()` |
| `test_onRefreshShare_prevDayTooltip_updatesAfterRefresh_viaFakeNetwork` | Aktie startet flach (0,00 €/0,00 €), Einzel-Refresh liefert `prevDayDiff = +30,00 €` (10 Stk. → +300,00 €) | Tooltip vor und nach dem Refresh je exakt gegen den erwarteten HTML-String geprüft (`QCOMPARE`) — Rechenweg, beide Farben **und** die `PrevDayChart`-Icon-Spalte ändern sich korrekt, nicht nur die Text-/Farb-Rollen der Zelle |
| `test_updatePortfolioFooters_prevDayTooltip_sumsAllShares` | Zwei Aktien (+50,00 € / −10,00 €, Summe +40,00 €) | Footer-Tooltip (Span-Anker `FC::Price` im Depotwert-, `MC::Icon` im Marktwert-Footer, alle drei Zeilen je Footer) exakt `"Gesamtänderung Portfolio: <farbig +40,00 €>"`, einzeilig — Summe der pro Aktie gerundeten Einzelwerte |

@note Alle fünf Tests nutzen bewusst runde, FIFO-/Brokerage-freie Testwerte
und prüfen daher per exaktem `QCOMPARE()` gegen den vollständig
konstruierten erwarteten HTML-Tooltip-String, statt nur auf einzelne
Text-Fragmente zu prüfen (anders als z. B. die Footer-Summen-Tests unter
"Footer-Update bei Refresh" oben, die aus Komplexitätsgründen bewusst nur auf
Änderung statt auf einen bestimmten Zahlenwert prüfen).

### Portfolio-Label "Letzte Aktualisierung" — erledigt (21.07.2026)

`updatePortfolioLabel(entryCount, formatLastPortfolioUpdate())` wird an
derselben Stelle in `onRefreshShareFinished()` aufgerufen wie
`refreshPortfolioFooters()` (siehe Abschnitt "Footer-Update bei Refresh"
oben) — direkt danach, vor dem Verketten zur nächsten Aktie bzw. vor
`finaliseRefresh()`. Grundlage ist `ShareRepository::maxLastInternetUpdate()`
(siehe `tests/repositories/tst_sharerepository.cpp`), nicht ein eigener
Persistenz-Mechanismus — der Zeitstempel lebt also in `shares.
last_internet_update` und übersteht dadurch auch einen Neustart der
Anwendung (dieselbe Portfolio-Datei erneut geöffnet).

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_populatePortfolioTables_neverUpdated_labelShowsDash` | Frisches Portfolio, Aktie ohne jemals gesetztes `last_internet_update` | Label enthält "Letzte Aktualisierung: -" |
| `test_onRefreshShare_marketPriceSuccess_labelShowsCurrentTimestamp_viaFakeNetwork` | Erfolgreicher Einzel-Refresh (MarketPrice) | Label wechselt von "-" zu einem echten Zeitstempel |
| `test_onRefreshShare_dailyValuesOnlySuccess_labelShowsCurrentTimestamp_viaFakeNetwork` | Erfolgreicher Einzel-Refresh, `ShareUpdateType::DailyValues`-only | Regressionstest für die geschlossene Lücke in `onDailyValuesUpdated()` (siehe ARCHITECTURE.md) — Label wechselt ebenfalls von "-" zu einem Zeitstempel |
| `test_onRefreshShare_networkError_labelStaysAtDash_viaFakeNetwork` | Einzel-Refresh liefert `QNetworkReply::HostNotFoundError` | Label bleibt exakt bei "Letzte Aktualisierung: -" |
| `test_populatePortfolioTables_afterRefresh_timestampPersistsAcrossReload_viaFakeNetwork` | Erfolgreicher Refresh gegen eine echte Datei-DB, Fenster geschlossen, DB erneut geöffnet, neues `MainWindow` konstruiert | Zeitstempel bleibt nach dem simulierten Neustart erhalten (nicht zurück auf "-") |

@note Wie bei den Sound-Tests wird `window.findChild<QLabel*>()` ohne
Namensfilter verwendet — `m_portfolioLabel` ist das erste `QLabel`, das
`setupCentralWidget()` erzeugt, dasselbe Muster wie bereits in
`test_updatePortfolioLabel_defaultValues` etabliert.

### buildDailyValuesUrl() — erledigt (07.07.2026)

Reine, seiteneffektfreie Funktion ihrer drei Parameter — kein Parser, kein
Netzwerk, keine `MainWindow`-Instanzzustände. Deswegen `public static`
gemacht (statt wie `selectShareRow()`/`selectFirstShareRow()` ein `private
slot` zu werden): der `ShareParsingType`-Enum-Parameter hat kein
`Q_DECLARE_METATYPE`/`Q_ENUM`, was `Q_ARG()` für `QMetaObject::invokeMethod`
bräuchte — eine direkte `static`-Methode umgeht das komplett und ist zugleich
technisch korrekter, da die Methode ohnehin nie `this` verwendet hat. Folgt
damit demselben Muster wie das bereits bestehende
`XmlPortfolioParser::normalizeWebSiteUrl()` (öffentliche, pur-statische
Utility-Methode, direkt getestet).

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_buildDailyValuesUrl_normalizesPlaceholdersAndAmpersand` | `{0}`/`{1}`-Platzhalter und `&amp;` im Template | Keine `{0}`/`{1}`/`&amp;` mehr in der resultierenden URL |
| `test_buildDailyValuesUrl_noExistingData_onVista_returns5YearWindow` | Ungültiges `latestExistingDate`, OnVista | Periodencode `Y5`, Startdatum = heute − 5 Jahre |
| `test_buildDailyValuesUrl_noExistingData_yahoo_returns20yPeriod` | Ungültiges `latestExistingDate`, Yahoo | `range=20y` |
| `test_buildDailyValuesUrl_recentData_selectsM1` | Letzter Datenpunkt vor 5 Tagen | Periodencode `M1` |
| `test_buildDailyValuesUrl_dataThreeWeeksOld_selectsM3` | Letzter Datenpunkt vor 2 Monaten | `range=3mo` |
| `test_buildDailyValuesUrl_dataFourMonthsOld_selectsM6` | Letzter Datenpunkt vor 4 Monaten | `range=6mo` |
| `test_buildDailyValuesUrl_dataNineMonthsOld_selectsY1` | Letzter Datenpunkt vor 9 Monaten | `range=1y` |
| `test_buildDailyValuesUrl_dataTwentyMonthsOld_selectsY3` | Letzter Datenpunkt vor 20 Monaten | `range=3y` |
| `test_buildDailyValuesUrl_dataFortyMonthsOld_selectsY5` | Letzter Datenpunkt vor 40 Monaten | `range=5y` |
| `test_buildDailyValuesUrl_dataOverFiveYearsOld_fallsBackToY5` | Letzter Datenpunkt vor 70 Monaten (kein Bracket in der Schleife matcht) | Fallback-Zweig liefert identisches Ergebnis wie der Y5-Treffer in der Schleife |
| `test_buildDailyValuesUrl_regexParsingType_returnsEmptyString` | `ShareParsingType::Regex` (keine gültige Strategie für den History-Endpunkt) | Leerer String, sowohl mit gültigem als auch ungültigem `latestExistingDate` |

@note Testdaten sind relativ zu `QDate::currentDate()` formuliert (`addMonths()`/
`addDays()`), nicht auf feste Kalenderdaten fixiert, da `buildDailyValuesUrl()`
selbst `QDate::currentDate()` intern liest und kein injizierbares "heute"
kennt. `addMonths()` erhält den Tag-im-Monat wo möglich, was die
Monatsdifferenz-Berechnung exakt hält — außer in seltenen Monatsende-
Randfällen (z. B. Tag 31 ohne Entsprechung im Zielmonat). Minimales,
akzeptiertes Restrisiko für Testflakiness an bestimmten Kalendertagen.

`refreshPortfolioFooters()` (aufgerufen aus `onRefreshShareFinished()` bei
Erfolg, siehe ARCHITECTURE.md) lädt alle Aktien neu und ruft
`updatePortfolioFooters()` auf — dieser Pfad sollte zusammen mit den
Refresh-Flow-Tests (Parser-Mocking) mitgetestet werden: nach Abschluss einer
einzelnen Aktie (`onRefreshShare`) ebenso wie zwischen den einzelnen Aktien
während `onRefreshAll()` müssen beide Footer (Depotwert- und Marktwert-Tab)
bereits die aktualisierten Summen zeigen, nicht erst nach dem letzten Element
der Queue. Im Fehlerfall (`m_errorOccurred`) darf der Footer für die
betroffene Aktie nicht aktualisiert werden.

Grid-Selektion folgt dem Refresh (Feature vom 05.07.2026) — vollständig
umgesetzt und getestet (siehe "Grid-Selektions-Testplan vollständig" unten).

### shouldMinimizeToTray() — erledigt (03.08.2026)

Reine, seiteneffektfreie Entscheidungsfunktion ihrer zwei `bool`-Parameter —
kein `QSystemTrayIcon`, keine `MainWindow`-Instanzzustände. Deswegen
`public static` gemacht, gleiches Muster wie `buildDailyValuesUrl()`/
`resolveShareGuidForDocument()`: direkt testbar ohne echtes Tray-Icon und
unabhängig davon, ob in der Test-/CI-Umgebung tatsächlich ein Infobereich
verfügbar ist (z. B. offscreen QPA in der CI meldet üblicherweise keinen
verfügbaren Tray).

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_shouldMinimizeToTray_settingEnabledAndTrayAvailable_returnsTrue` | Option aktiv + Tray verfügbar | `true` |
| `test_shouldMinimizeToTray_settingDisabled_returnsFalse` | Option inaktiv, Tray verfügbar | `false` |
| `test_shouldMinimizeToTray_trayNotAvailable_returnsFalse` | Option aktiv, Tray nicht verfügbar | `false` |
| `test_shouldMinimizeToTray_settingDisabledAndTrayNotAvailable_returnsFalse` | Option inaktiv, Tray nicht verfügbar | `false` |

Die Dialog-Tests für `TraySettingsForm` selbst (Laden/Speichern/Abbrechen der
Einstellung) laufen in `tst_traysettingsform` (eigene Executable, siehe dort).


@note Korrektur (07.07.2026): Diese Datei behauptete bis dahin fälschlich,
`selectShareRow()`/`selectFirstShareRow()` seien `private` (keine
`private slots`) und ungetestet. Tatsächlich wurden beide Methoden bereits
am 05.07.2026 zu `private slots` refaktoriert — verhaltensneutral, einzig um
`QMetaObject::invokeMethod()`-Aufrufe aus Tests zu ermöglichen, unabhängig
vom restlichen (Parser-abhängigen) Refresh-Flow — und dafür existieren
bereits zwei Tests in `tst_mainwindow.cpp`:

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_selectShareRow_selectsMatchingGuidInBothTables` | `selectShareRow(guid)` per `QMetaObject::invokeMethod` aufgerufen | `currentRow()` in beiden Tabellen (`m_finalValueTable`, `m_marketValueTable`) zeigt auf die Zeile mit passender GUID |
| `test_selectShareRow_switchingGuid_movesSelectionToOtherShare` | Zweiter `selectShareRow()`-Aufruf mit anderer GUID (simuliert Queue-Fortschritt bei "Alle aktualisieren") | Selektion wandert zur neuen Aktie |

Beide Tests kommen komplett ohne Parser/Netzwerk aus — die Methoden selbst
haben keine solche Abhängigkeit, nur ihre *Aufrufer* (`startRefreshForShare()`,
`onRefreshShareFinished()`) schon. Das war der Grund für den Slot-Refaktor:
die Selektionslogik isoliert testbar zu machen, ohne auf die
Parser-Mocking-Infrastruktur warten zu müssen.

Erledigt (07.07.2026) — mit `MainWindow(QNetworkAccessManager*, ...)` +
`FakeNetworkAccessManager` (siehe oben):

- Regressionstest für Icon-Update bei Einzel-Refresh (Bugfix 06.07.2026):
  `test_onRefreshShare_iconRegression_updatesChartIconsViaFakeNetwork` —
  Aktie mit zuvor negativem Vortagswert (Icon `NegativStrong`), Refresh
  liefert einen positiven `prevDayPct` (+20 %) → Icon wechselt in **beiden**
  Tabellen (`PrevDayChart` und `CompleteChart`) auf `PositivStrong`. Dieser
  Fall wurde vor dem Fix nicht abgedeckt, weil `onMarketValuesUpdated()` die
  Icon-Zellen schlicht nie anfasste (nur `setTwoLine()` für die
  Text-Spalten) — Text und Icon liefen dadurch nach einem Einzel-Refresh
  dauerhaft auseinander, bis der nächste volle `populatePortfolioTables()`-
  Aufbau (z. B. Neustart) das Icon wieder korrigierte.
- `enableShareActions`-Lambda-Busy-Guard — Regressionstest, deckte Bugfix
  07.07.2026 auf: `test_onRefreshShare_busyGuard_selectionDuringRefreshDoesNotReenableActions`.
  `startRefreshForShare()` ruft `selectShareRow()` auf, **bevor** einer der
  beiden Parser `startParsing()` aufruft. Wurde eine Tabelle (typischerweise
  `m_marketValueTable`) zuvor noch nie selektiert, löst `selectShareRow()`
  dort ein echtes `selectionChanged()` aus — zu diesem Zeitpunkt waren
  `m_parserMarketValues.isBusy()`/`m_parserDailyValues.isBusy()` noch beide
  `false`, der reine Busy-Guard griff also nicht und `enableShareActions`
  schaltete Edit/Delete/Refresh genau in dem Moment wieder frei, in dem
  `onRefreshShare()` sie eben erst deaktiviert hatte. Behoben durch ein
  zusätzliches Flag `m_refreshInProgress`, gesetzt in `startRefreshForShare()`
  vor `selectShareRow()`, zurückgesetzt in `finaliseRefresh()` (siehe
  ARCHITECTURE.md für Details).
- Grid-Selektion während `onRefreshAll()` folgt jeder Aktie in der Queue, und
  springt nach erfolgreichem Abschluss (Queue leer, kein Fehler) in beiden
  Tabellen auf Zeile 0:
  `test_onRefreshAll_gridSelectionFollowsQueueProgress_viaFakeNetwork`.
- Fehlerfall während `onRefreshAll()`: Selektion bleibt unverändert auf der
  Aktie stehen, bei der der Fehler auftrat — `selectFirstShareRow()` wird
  nicht aufgerufen, unabhängig davon ob noch weitere Aktien in der Queue
  standen:
  `test_onRefreshAll_errorMidQueue_selectionStaysOnFailedShare_viaFakeNetwork`.
- Abgeschlossener Einzel-Refresh (`onRefreshShare()`, kein "Alle
  aktualisieren"): Selektion bleibt auf der aktualisierten Aktie stehen,
  `selectFirstShareRow()` wird nicht aufgerufen (ergänzt 20.07.2026, bis
  dahin als einziger der vier Grid-Selektions-Punkte noch offen):
  `test_onRefreshShare_completed_selectionStaysOnUpdatedShare_viaFakeNetwork`.

@note **Grid-Selektions-Testplan vollständig (20.07.2026):** Mit dem letzten
der vier oben genannten Tests ist der ursprünglich zurückgestellte
Refresh-Flow-Selektions-Testplan (siehe "Grid-Selektion folgt dem Refresh"
oben) komplett abgedeckt. Es gibt aktuell keine offenen Punkte mehr zur
Grid-Selektion beim Refresh.

---

## Konventionen

### Testmethoden-Benennung
```
test_<was>_<erwartetes Ergebnis>

Beispiele:
  test_modelBuyEdit_addBuy_success
  test_viewBuyEdit_setFieldOk_date_parsesDotFormat
  test_presenterBuyEdit_onSave_emptyDepotNumber_showsError
```

### Assertions
| Makro | Verwendung |
|-------|------------|
| `QCOMPARE(actual, expected)` | Wertvergleich |
| `QVERIFY(condition)` | Boolesche Bedingung |

### Häufige Fehlerquellen

`AppSettings::instance().load(...)` mit leerem Pfad oder dem echten
`AppStartup::settingsPath()` in `cleanupTestCase()`, "um die echte
settings.ini für den nächsten Lauf wiederherzustellen" — **falsche
Absicherung, tut das Gegenteil** (gemeldet und behoben 19.07.2026, siehe
`tst_mainwindow.cpp`/`tst_appstartup.cpp`). `AppSettings` ist ein
prozesslokaler Singleton; er stirbt mit dem Testprozess, ein "Zurücksetzen"
ist dafür nicht nötig. Enthält ein Testbinary aber — wie `tst_mainwindow` —
mehrere `QObject`-Testklassen, die im selben Prozess sequenziell laufen
(eigene `main()` mit mehreren `QTest::qExec()`-Aufrufen statt `QTEST_MAIN`),
leitet ein solcher Reset den Singleton auf die **echte** `settings.ini` um —
jeder `setXxx()`-Aufruf einer später laufenden Testklasse landet dann direkt
in der echten Konfigurationsdatei des Benutzers (Portfolio-Pfad,
Dokument-Root, ...) statt in einer Sandbox. Regel: `cleanupTestCase()`
schließt höchstens die Datenbank — AppSettings nie mit einem "echten" Pfad
neu laden. Jede Testklasse, die `AppSettings`-Werte setzt, muss außerdem
ihr **eigenes** `loadSandboxedSettings()` (eigene `QTemporaryDir`-INI) in
`initTestCase()`/`init()` aufrufen, statt sich auf den Zustand einer
vorher gelaufenen Klasse im selben Prozess zu verlassen.

`QSqlDatabase::database()` ohne Argument — gibt die Default-Verbindung zurück, die im
Projekt nicht existiert. Immer `QSqlDatabase::database(Database::connectionName())` verwenden.

Presenter vor `setupUi()` erstellt — führt zu Absturz. Immer erst `setupUi()`, dann
Presenter anlegen.

Fehlende Quelldateien in `tst_mainwindow` — jede neue Form muss in
`tests/forms/CMakeLists.txt` mit allen `.cpp`-Dateien und ihrem Verzeichnis in
`target_include_directories` eingetragen werden. Symptom: vtable-Linker-Fehler.

`AUTOMOC ON` fehlt — `Q_OBJECT`-Klassen in Unterverzeichnissen werden nicht
verarbeitet. Symptom: vtable-Linker-Fehler.

@note In der Praxis tritt dieses Symptom im Projekt aktuell nicht auf: Die
Root-`CMakeLists.txt` ruft `qt_standard_project_setup()` auf, bevor irgendein
`add_subdirectory()` folgt. Das setzt `CMAKE_AUTOMOC` (und `CMAKE_AUTOUIC`)
projektweit auf `ON` und vererbt sich an alle danach definierten Targets —
auch ohne expliziten Eintrag pro Target. Trotzdem setzt jedes Target im
Projekt (Stand 07.07.2026) zusätzlich `set_target_properties(<target>
PROPERTIES AUTOMOC ON)` explizit, rein defensiv/dokumentierend: falls die
Reihenfolge in der Root-`CMakeLists.txt` sich künftig ändert oder ein Target
isoliert in ein anderes Projekt übernommen wird, bleibt das Verhalten
nachvollziehbar statt implizit vom globalen Default abhängig.

`ViewBuyEdit`/`ViewShareEdit` brauchen `DocumentsConfig*` — Konstruktoren verlangen
einen gültigen (oder `nullptr`-) Zeiger. In Tests kann `nullptr` übergeben werden
wenn keine Parsing-Funktionalität getestet wird.

`populateOverview`-Tests: Data-Table abrufen — per `dataTableFromContainer(container)`
(Hilfsmethode in `TestMainWindow`), nicht per `findChild<QTableWidget*>()`. Jeder Container
enthält zwei QTableWidgets (data + footer); `findChild` liefert das erste, was zufällig
korrekt ist, aber `property("dataTable")` ist zuverlässiger.

`populateOverview`-Tests: Dokument-Icon — das Icon wird per `setCellWidget` als
`QLabel` gesetzt, nicht per `QTableWidgetItem::setIcon`. Deshalb muss in Tests
`tbl->cellWidget(row, 5)` geprueft werden, nicht `tbl->item(row, 5)->icon()`.

`QVERIFY(ptr != nullptr)` reicht dem Clang Static Analyzer nicht — er modelliert
`QVERIFY` als normalen Funktionsaufruf, der zurückkehren kann. Der Analyzer hält den Zeiger
danach weiterhin für potenziell null und meldet `core.CallAndMessage`. Abhilfe:
`if (!ptr) QFAIL("...")` — das ist für den Analyzer transparent als echter Return-Guard.
Beispiel: `if (!tabs) QFAIL("QTabWidget not found");` statt `QVERIFY(tabs != nullptr);`.

Live-Validierungsaufrufe
übergeben immer einen leeren Value. Das ist gewollt: nur der Parser darf Widget-Inhalte
per `setFieldOk` setzen. Tests die prüfen ob Widget-Text nach Validierung erhalten bleibt,
müssen den Text vor dem `setFieldOk`-Aufruf setzen.

---

## Abhängigkeiten

| Paket | Zweck |
|-------|-------|
| `Qt6::Test` | Qt Test Framework |
| `Qt6::Sql` | SQLite-Datenbankzugriff |
| `Qt6::Widgets` | Widget-Tests |
| `Qt6::Charts` | `tst_mainwindow` (kompiliert `ViewShareDetails.cpp` → `ViewChart.h`) |
| `Logger` (statisch) | Logger-Lib |
| `Parser` (statisch) | Parser-Lib |
| `Database` (statisch) | Database-Lib — wird gelinkt, nicht direkt eingebunden |

CMake-Integration (`tests/forms/CMakeLists.txt`):
```cmake
find_package(Qt6 REQUIRED COMPONENTS Test Sql Widgets Multimedia)

qt_add_executable(tst_mainwindow tst_mainwindow.cpp
    ../../app/forms/MainForm/MainWindow.cpp
    ../../app/forms/ShareAddForm/ModelShareAdd.cpp
    ../../app/forms/ShareAddForm/PresenterShareAdd.cpp
    ../../app/forms/ShareAddForm/ViewShareAdd.cpp
    ../../app/forms/ShareEditForm/ModelShareEdit.cpp
    ../../app/forms/ShareEditForm/PresenterShareEdit.cpp
    ../../app/forms/ShareEditForm/ViewShareEdit.cpp
    ../../app/forms/BuysForm/ModelBuyEdit.cpp
    ../../app/forms/BuysForm/PresenterBuyEdit.cpp
    ../../app/forms/BuysForm/ViewBuyEdit.cpp
    ../../app/forms/SalesForm/ModelSaleEdit.cpp
    ../../app/forms/SalesForm/PresenterSaleEdit.cpp
    ../../app/forms/SalesForm/ViewSaleEdit.cpp
    ../../app/forms/DividendForm/ModelDividendEdit.cpp
    ../../app/forms/DividendForm/PresenterDividendEdit.cpp
    ../../app/forms/DividendForm/ViewDividendEdit.cpp
    ../../app/forms/BrokeragesForm/ModelBrokerageEdit.cpp
    ../../app/forms/BrokeragesForm/PresenterBrokerageEdit.cpp
    ../../app/forms/BrokeragesForm/ViewBrokerageEdit.cpp
    ../../app/forms/OwnMessageBoxForm/OwnMessageBox.cpp
    ../../app/forms/BackupProgressForm/BackupWorker.cpp
    ../../app/forms/BackupProgressForm/BackupProgressDialog.cpp
    ../../app/utils/ShareCalculator.cpp
    # ... Repositories, Models, Config, AppStartup, IconProvider
)

target_include_directories(tst_mainwindow
    PRIVATE ../../app
            ../../app/forms/ShareEditForm
            ../../app/forms/BuysForm
            ../../app/forms/SalesForm
            ../../app/forms/DividendForm
            ../../app/forms/BrokeragesForm
            ../../app/forms/OwnMessageBoxForm
            ../../app/forms/BackupProgressForm
            # ...
)

set_target_properties(tst_mainwindow PROPERTIES
    AUTOMOC ON
    AUTORCC ON
)

target_link_libraries(tst_mainwindow
    PRIVATE Qt6::Test Qt6::Sql Qt6::Widgets Qt6::Multimedia Database Logger Parser
)

add_test(NAME tst_mainwindow COMMAND tst_mainwindow)

# BuysForm-Quellen bleiben im tst_mainwindow-Target (Compile-Dep. via ViewShareEdit).
# Die eigentlichen BuysForm-Tests laufen in tst_buysform:

qt_add_executable(tst_buysform tst_buysform.cpp
    ../../app/forms/BuysForm/ModelBuyEdit.cpp
    ../../app/forms/BuysForm/PresenterBuyEdit.cpp
    ../../app/forms/BuysForm/ViewBuyEdit.cpp
    ../../app/forms/OwnMessageBoxForm/OwnMessageBox.cpp
    ../../app/config/AppSettings.cpp
    ../../app/config/DocumentsConfig.cpp
    ../../app/IconProvider.cpp
    ../../app/models/BuyObject.cpp
    ../../app/models/BrokerageObject.cpp
    ../../app/repositories/BuyRepository.cpp
    ../../app/repositories/BrokerageRepository.cpp
    ../../app/repositories/ShareRepository.cpp
    # ...
)

target_link_libraries(tst_buysform
    PRIVATE Qt6::Test Qt6::Sql Qt6::Widgets Database Logger Parser
)

add_test(NAME tst_buysform COMMAND tst_buysform)

# ViewShareEdit zieht alle vier Sub-Form-Trios als Compile-Abhängigkeit rein:

qt_add_executable(tst_shareeditform tst_shareeditform.cpp
    ../../app/forms/ShareEditForm/ModelShareEdit.cpp
    ../../app/forms/ShareEditForm/PresenterShareEdit.cpp
    ../../app/forms/ShareEditForm/ViewShareEdit.cpp
    ../../app/forms/BuysForm/ModelBuyEdit.cpp
    ../../app/forms/BuysForm/PresenterBuyEdit.cpp
    ../../app/forms/BuysForm/ViewBuyEdit.cpp
    ../../app/forms/SalesForm/ModelSaleEdit.cpp
    ../../app/forms/SalesForm/PresenterSaleEdit.cpp
    ../../app/forms/SalesForm/ViewSaleEdit.cpp
    ../../app/forms/DividendForm/ModelDividendEdit.cpp
    ../../app/forms/DividendForm/PresenterDividendEdit.cpp
    ../../app/forms/DividendForm/ViewDividendEdit.cpp
    ../../app/forms/BrokeragesForm/ModelBrokerageEdit.cpp
    ../../app/forms/BrokeragesForm/PresenterBrokerageEdit.cpp
    ../../app/forms/BrokeragesForm/ViewBrokerageEdit.cpp
    ../../app/forms/OwnMessageBoxForm/OwnMessageBox.cpp
    # ... Repositories, Models, Config, IconProvider
)

target_link_libraries(tst_shareeditform
    PRIVATE Qt6::Test Qt6::Sql Qt6::Widgets Qt6::Multimedia Database Logger Parser
)

add_test(NAME tst_shareeditform COMMAND tst_shareeditform)
```

---

### tests/xml-importer/ — XML-Importer Unit-/Integrationstests

@note `tools/xml-importer` ist ein eigenständiges Console-Tool (kein Teil des
`SharePortfolioManager`-Targets), das dieselben Models/Repositories der
Hauptanwendung wiederverwendet. Die Tests spiegeln das: `tst_xmlportfolioparser`
prüft den reinen XML-Parser ohne DB, `tst_portfoliovalidator` (neu, 05.07.2026)
prüft die Vorab-Validierung isoliert vom eigentlichen Import, `tst_portfolioimporter`
folgt exakt dem Muster aus `tests/repositories/` (In-Memory-SQLite via `:memory:`).

#### tst_xmlportfolioparser

Executable: `tst_xmlportfolioparser`
Klasse unter Test: `XmlPortfolioParser` (reines XML → Struct-Mapping, keine DB)

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_parse_minimalShare_readsBasicAttributesAndFields` | Ein `<Share>` mit allen Basis-Feldern | WKN/ISIN/Name/Update, Datumsfelder, `MarketValue`/`DailyValues`-Parsing-Attribute korrekt extrahiert, `parseWarnings` leer |
| `test_parse_marketValueWebSite_doubleEscapedAmpersand_isAutoCorrected` | `MarketValue@WebSite` mit `&amp;amp;` in der Quelle (Regressionstest Nvidia, gemeldet 05.07.2026) | Literales `&amp;` im Rohwert wird zu `&` korrigiert, genau eine Warnung mit Feldbezeichner "MarketValue.WebSite" |
| `test_parse_dailyValuesWebSite_doubleEscapedAmpersand_isAutoCorrected` | `DailyValues@WebSite` mit `&amp;amp;` in der Quelle (Regressionstest Wacker Chemie, gemeldet 05.07.2026) | Analog korrigiert, Warnung mit Feldbezeichner "DailyValues.WebSite" |
| `test_parse_detailsWebSite_doubleEscapedAmpersand_isAutoCorrected` | `<DetailsWebSite>` mit `&amp;amp;` in der Quelle | Analog korrigiert, Warnung mit Feldbezeichner "DetailsWebSite" (vorsorglich abgedeckt, kein bekannter Realfall) |
| `test_parse_marketValuePluralTag_isReportedAsErrorNotImported` | `<MarketValues>` (Plural) statt `<MarketValue>` (Singular) in der Quelle (Regressionstest Wacker Chemie, gemeldet 05.07.2026) | Wird NICHT übernommen (`marketValueWebSite`/`marketValueParsing` bleiben leer), stattdessen genau ein Eintrag in `parseErrors` |
| `test_parse_marketValuePluralTag_doesNotSuppressOtherFieldWarnings` | Plural-Tag bei `MarketValue` (→ `parseErrors`) **und** doppelt-escaptes Ampersand bei `DailyValues` (→ `parseWarnings`) gleichzeitig (Regressionstest Nvidia, Originalwerte aus dem Report vom 05.07.2026) | Beide Erkennungen laufen unabhängig voneinander an unterschiedlichen Feldern, ohne sich gegenseitig zu beeinflussen |
| `test_parse_singleEscapedAmpersand_isLeftUnchanged_noWarning` | Korrekt einfach escapte URL, Kontrollfall (analog BMW.DE aus demselben Bestand) | Wert unverändert nach dem Unescape, `parseWarnings` bleibt leer — kein Fehlalarm |
| `test_parse_buysSalesBrokerages_mapsAttributesCorrectly` | Ein Buy + eine Sale mit `UsedBuys` + zwei Brokerages | Anzahl je Kategorie, `UsedBuy.buyGuid`/`brokerage`, `Brokerage.buyPart`/`salePart`/`guidBuySale` |
| `test_parse_dividendWithForeignCurrency_setsFcFields` | Eine Dividende mit `<ForeignCurrency>`, eine ohne | `hasForeignCurrency`, `fc.enabled`, `fc.exchangeRatio`, `fc.currency` nur bei vorhandenem Element gesetzt |
| `test_parse_dailyValuesEntries_mapsAttributesCorrectly` | Zwei `<Entry D/C/O/T/B/V>` | Korrektes Mapping der Kurzattribute auf `date/close/open/top/bottom/volume` |
| `test_parse_multipleShares_areAllCollected` | Zwei `<Share>`-Blöcke in einem `<Portfolio>` | `portfolio.shares.size() == 2`, Reihenfolge erhalten |
| `test_parse_missingRootElement_fails` | XML ohne `<Portfolio>`-Wurzel | `parse()` liefert `false`, Fehlermeldung nicht leer |
| `test_parse_fileNotFound_fails` | Nicht existierender Pfad | `parse()` liefert `false`, Fehlermeldung nicht leer |
| `test_parse_malformedXml_fails` | Nicht geschlossenes `<Share>`-Tag | `parse()` liefert `false` (QXmlStreamReader-Fehler) |

#### tst_portfolioimporter

Executable: `tst_portfolioimporter`
Klasse unter Test: `PortfolioImporter` (gegen In-Memory-SQLite `:memory:`)

@note Da `importBuys()`/`importSales()`/`importBrokerages()`/... privat sind,
laufen alle Tests über den einzigen öffentlichen Einstiegspunkt
`importPortfolio()` — das sind bewusst Integrations- statt Einzelmethodentests.
Seit 05.07.2026 gibt `importPortfolio()` `bool` zurück (siehe
`PortfolioValidator` weiter unten); alle Tests prüfen diesen Rückgabewert
explizit per `QVERIFY`/`QVERIFY(!...)`.

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_importShare_insertsNewShare` | Neue Aktie importieren | Genau eine Zeile in `shares`, Felder korrekt |
| `test_importShare_reusesExistingShareByWkn_masterDataUntouched` | Zweiter Import derselben WKN mit geändertem Namen | Keine Dublette, GUID wiederverwendet, Stammdaten **nicht** überschrieben |
| `test_importBuy_isIdempotentOnRerun` | Identischer Import zweimal ausgeführt | Kein doppelter Buy-Datensatz (GUID-Dedupe), zweiter Lauf liefert weiterhin `true` (Regressionstest 05.07.2026 — `PortfolioValidator` darf einen Re-Import derselben GUID/OrderNumber nicht als Kollision werten) |
| `test_importBuy_orderNumberCollision_abortsEntireImport` | Zwei Buys mit gleicher `OrderNumber`, unterschiedlicher GUID | `importPortfolio()` liefert `false`, **gar nichts** wird importiert — auch nicht die Aktie selbst (umbenannt von `..._skipsSecondButContinues`: Verhalten seit 05.07.2026 komplett geändert, ursprünglicher Regressionstest AGIF/Facebook-Fall vom 01.07.2026) |
| `test_importPortfolio_oneShareInvalid_abortsWholeImportIncludingValidShares` | Eine valide Aktie + eine mit nicht parsbarem Datum, in derselben Datei (ergänzt 05.07.2026) | `importPortfolio()` liefert `false`, **keine** der beiden Aktien landet in der DB — Kernverhalten der Validate-then-Import-Architektur |
| `test_importBrokerage_correctsWrongBuyPartFlag` | `BuyPart="True"`, `GuidBuySale` zeigt aber auf eine Sale | `sale_guid` korrekt gesetzt, `buy_guid` leer (Regressionstest Mensch u. Maschine/Procter & Gamble, 01.07.2026) |
| `test_importBrokerage_correctsWrongSalePartFlag` | Spiegelfall: `SalePart="True"`, zeigt auf einen Buy | `buy_guid` korrekt gesetzt, `sale_guid` leer |
| `test_importBrokerage_correctFlags_areAcceptedUnchanged` | Korrekte Flags (Kontrollfall) | Zuordnung wie erwartet, kein falscher Fehlalarm |
| `test_importBrokerage_guidBuySaleNotFoundInEitherTable_abortsEntireImport` | `GuidBuySale` existiert weder als Buy noch als Sale | `importPortfolio()` liefert `false`, weder Brokerage noch die Aktie selbst landen in der DB (umbenannt von `..._isSkipped`, gleicher Grund wie oben) |
| `test_importDividend_foreignCurrencyFieldsAreStored` | Dividende mit `ForeignCurrency` | `enable_fc`, `exchange_ratio`, `currency` korrekt in der DB |
| `test_dryRun_writesNothing` | Import mit `dryRun=true` | Alle Zieltabellen bleiben leer, `importPortfolio()` liefert trotzdem `true` (Validierung läuft unabhängig von `dryRun` und findet hier nichts) |
| `test_dailyValues_upsertReplacesExistingValueOnRerun` | Gleicher Tag zweimal mit unterschiedlichem Schlusskurs importiert | Keine Dublette, Wert wurde aktualisiert (`INSERT OR REPLACE`) |
| `test_importDailyValues_logsInsertedUpdatedUnchangedBreakdown` | Zweiter Import mit einem unveränderten, einem geänderten und einem neuen Tageswert-Eintrag (ergänzt 05.07.2026) | Log enthält "3 Tageswert(e) geholt (Eingefügt: 1 / Aktualisiert: 1 / Unverändert: 1)" |

#### tst_portfoliovalidator (neu, 05.07.2026)

Executable: `tst_portfoliovalidator`
Klasse unter Test: `PortfolioValidator` (reine Vorab-Prüfung gegen In-Memory-SQLite `:memory:`, kein Import)

Isoliert von `tst_portfolioimporter`, damit jeder Validierungsfall einzeln und
ohne den Umweg über einen vollständigen Import-Lauf geprüft werden kann.

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_validate_completelyValidPortfolio_noIssues` | Vollständig valide Aktie mit Buy/Brokerage/Dividend/DailyValue (Kontrollfall) | `validate()` liefert `true`, `issues` leer |
| `test_validate_missingWkn_isReported` | Aktie ohne WKN | Problem gemeldet, Meldung erwähnt "WKN" |
| `test_validate_unknownUpdateValue_isReported` | `Update="Sometimes"` (Tippfehler-Simulation) | Problem mit `recordId == "Update"` |
| `test_validate_unknownShareType_isReported` | `ShareType="9"` | Problem mit `recordId == "ShareType"` |
| `test_validate_unknownParsingValue_isReported` | `Parsing="ApiYaho"` (Tippfehler-Simulation) | Problem mit `recordId == "MarketValue.Parsing"` |
| `test_validate_emptyParsingValue_isAccepted` | Leerer `Parsing`-Wert | **Kein** Problem — leer ist ein legitimer "nicht konfiguriert"-Zustand |
| `test_validate_regexParsingValue_isAccepted` | `Parsing="Regex"` | **Kein** Problem — laut `ARCHITECTURE.md` ein regulärer dritter Parsing-Typ, kein Tippfehler |
| `test_validate_unparsableShareDate_isReported` | `StockMarketLaunchDate="32.13.2024"` | Problem mit `recordId == "StockMarketLaunchDate"` |
| `test_validate_parseErrorsFromXmlParser_areIncluded` | Simulierter `RawShare::parseErrors`-Eintrag (z. B. `<MarketValues>`-Tag) | Fließt 1:1 in die Validierungsprobleme ein |
| `test_validate_buyMissingGuid_isReported` | Buy ohne GUID | Problem mit `category == "Buy"`, erwähnt "GUID" |
| `test_validate_buyUnparsableDate_isReported` | Buy mit `Date="31.02.2024"` (31. Februar existiert nicht) | Problem mit `category == "Buy"`, erwähnt "Datum" |
| `test_validate_duplicateOrderNumberAmongBuysInSameFile_isReported` | Zwei Buys derselben Aktie, gleiche `OrderNumber`, unterschiedliche GUID, in derselben Datei | Problem mit `category == "Buy"`, `recordId` = die doppelte OrderNumber |
| `test_validate_duplicateGuidAcrossCategoriesInSameShare_isReported` | Ein Buy und eine Dividende derselben Aktie mit identischer GUID | Problem erwähnt "mehrfach", `recordId` = die kollidierende GUID |
| `test_validate_brokerageGuidBuySaleNotFound_isReported` | `GuidBuySale` zeigt auf keine existierende Buy/Sale-GUID | Problem erwähnt "weder als Buy noch als Sale" |
| `test_validate_brokerageGuidBuySaleAmbiguous_isReported` | Eine GUID wird absichtlich sowohl als Buy- als auch als Sale-GUID verwendet | Problem erwähnt "sowohl als Buy als auch als Sale" |
| `test_validate_dailyValueUnparsableDate_isReported` | `DailyValue` mit nicht parsbarem Datum | Problem mit `category == "DailyValue"` |
| `test_validate_shareUnparsableSharePrice_isReported` | `SharePrice="nicht-eine-zahl"` (ergänzt 08.07.2026) | Problem mit `category == "Share"`, `recordId == "SharePrice"` |
| `test_validate_buyUnparsableVolume_isReported` | Buy mit `Volume="zehn"` (ergänzt 08.07.2026) | Problem mit `category == "Buy"`, erwähnt "Volume" |
| `test_validate_saleUnparsableSalePrice_isReported` | Sale mit `SalePrice="11,00,00"` (doppeltes Komma, ergänzt 08.07.2026) | Problem mit `category == "Sale"`, erwähnt "SalePrice" |
| `test_validate_usedBuyUnparsableBuyPrice_isReported` | `<UsedBuy BuyPrice="??">` (ergänzt 08.07.2026) | Problem mit `category == "Sale"`, erwähnt "UsedBuy" und "BuyPrice" |
| `test_validate_brokerageUnparsableProvision_isReported` | Brokerage mit `Provision="neun-neunzig"` (ergänzt 08.07.2026) | Problem mit `category == "Brokerage"`, erwähnt "Provision" |
| `test_validate_dividendUnparsableRate_isReported` | Dividende mit `Rate="fünfzig-cent"` (ergänzt 08.07.2026) | Problem mit `category == "Dividend"`, erwähnt "Rate" |
| `test_validate_dividendForeignCurrencyUnparsableExchangeRatio_isReported` | Fremdwährungs-Dividende mit `ExchangeRatio="ein Euro achtzig"` (ergänzt 08.07.2026) | Problem mit `category == "Dividend"`, erwähnt "ExchangeRatio" |
| `test_validate_dailyValueUnparsableClose_isReported` | `DailyValue` mit `C="hundert"` (ergänzt 08.07.2026) | Problem mit `category == "DailyValue"`, erwähnt "C" |
| `test_validate_emptyNumericFields_areAccepted` | Diverse numerische Felder (Share/Buy/Brokerage/Dividend) leer statt gesetzt (ergänzt 08.07.2026) | **Kein** Problem — leer ist laut `Documents.xml` (`ResultEmpty="true"`) ein legitimer "nicht gesetzt"-Zustand, kein Datenfehler |
| `test_validate_orderNumberAlreadyExistsInDb_isReported` | Neuer Buy mit `OrderNumber`, die bereits unter einer **anderen** GUID in der DB existiert | Problem mit `category == "Buy"`, erwähnt "Datenbank" |
| `test_validate_brokerageResolvesAgainstExistingDbBuy_noIssue` | Brokerage referenziert einen Buy, der bereits aus einem früheren Import in der DB steht (nicht in der aktuellen Datei) | **Kein** Problem — `GuidBuySale`-Auflösung berücksichtigt auch bereits importierte Daten |
| `test_validate_sameGuidReimportedWithSameOrderNumber_noIssue` | Derselbe Buy (gleiche GUID, gleiche `OrderNumber`) wird erneut importiert | **Kein** Problem — Idempotenz-Regressionstest (Bug gefunden 05.07.2026 durch `test_importBuy_isIdempotentOnRerun`: DB-Abgleich unterschied ursprünglich nicht zwischen "andere GUID, gleiche OrderNumber" und "dieselbe GUID nochmal") |

---

### tests/forms/ — DocumentsSettingsForm & DocumentRootMigrator

@note `AppSettings` ist ein Singleton — jeder Test, der `documentsRootPath`
ändert, stellt am Ende den ursprünglichen Wert wieder her. `DocumentRootMigrator`-
Tests brauchen eine echte (temporäre) SQLite-DB mit ein paar Shares/
Buys/Sales, da die Repositories direkt gegen `Database::instance()` arbeiten.

AppSettings — Documents-Sektion (reiner Speichern/Laden-Roundtrip):

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_documentsSettings_saveRootPath` | `documentsRootPath` gespeichert und gelesen | Wert korrekt geladen |
| `test_documentsSettings_defaultIsEmpty` | Frisch geladene INI ohne Eintrag | `documentsRootPath()` = "" |

DocumentsSettingsForm — Konstruktion & Vorbefüllung:

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_dialog_constructsWithoutCrash` | Dialog öffnet ohne Absturz | Kein Absturz |
| `test_dialog_alwaysHasCancelButton` | Abbrechen-Button immer vorhanden (kein Zwangsmodus mehr) | `findChild<QPushButton*>` mit Text "Abbrechen" ≠ nullptr |
| `test_dialog_cancel_doesNotChangeSettings` | Abbrechen-Klick | `AppSettings::documentsRootPath()` unverändert, kein DB-Write |
| `test_dialog_loadSettings_prefillsOldRootFromConfigured` | Root bereits gesetzt | "Alter Root-Pfad"-Feld = `AppSettings::documentsRootPath()` |
| `test_dialog_loadSettings_prefillsOldRootFromDetection` | Kein Root gesetzt, Dokumente mit gemeinsamem Ordner in DB | "Alter Root-Pfad"-Feld = erkannter Ordner |
| `test_dialog_loadSettings_ambiguousShowsHintNoAutofill` | Dokumente mit unterschiedlichen Ordnern | Hinweistext sichtbar, Feld bleibt leer |
| `test_dialog_onOk_emptyOldRoot_savesWithoutRewrite` | Altes Feld leer, neues gesetzt (existierendes Verzeichnis) | `AppSettings::documentsRootPath()` = neuer Pfad, `accept()`, keine `changeRoot()`-Aufrufe/DB-Änderungen |
| `test_dialog_onOk_sameOldAndNewRoot_savesWithoutConfirmation` | Altes und neues Feld identisch | `accept()` ohne Bestätigungsdialog, `documentsRootPath()` = Pfad |

@note Bewusst NICHT implementiert: `onOk()`s Fehler-/Bestätigungs-Zweige
(leerer oder nicht existierender neuer Pfad → `OwnMessageBox::critical()`;
abweichender alter Pfad → `OwnMessageBox::question()`) — beide rufen intern
`exec()` auf und würden den headless Testlauf blockieren, exakt dieselbe
Konvention wie bei `onPortfolioRowDoubleClicked()` in `tst_mainwindow.cpp`.
Die zugrundeliegende Validierungslogik (leerer/nicht-existierender Pfad,
Präfix-Vergleich) ist über die `DocumentRootMigrator`-Tests unten trotzdem
abgedeckt — nur der UI-Dialog-Aufruf selbst bleibt ungetestet.

DocumentRootMigrator — `changeRoot()`:

@note Das VERHALTEN von `changeRoot()` wird ausschließlich über
`BuyObject`/`BuyRepository` geprüft, nicht zusätzlich über
Sale/Brokerage/Dividend/ShareSplit — `DocumentRootMigrator` behandelt alle fünf
Tabellen strukturell identisch (derselbe Switch über `DocumentEntry::Table` in
`rewrite()`/`collectAllDocuments()`).

@note Davon zu unterscheiden ist der ANSCHLUSS einer Tabelle, und hier wurde
die Begründung oben am 08.08.2026 nachgeschärft: dass eine Tabelle sich wie die
anderen verhält, sagt nichts darüber, ob sie überhaupt angeschlossen IST. Wer
eine Tabelle ergänzt und dabei nur den Switch in `updateDocument()` anfasst,
aber die Sammelschleife in `collectAllDocuments()` vergisst, bekommt von den
Verhaltenstests keinen Widerspruch — die Dokumente werden dann schlicht nie
eingesammelt, und der Fehler fällt erst Monate später an einem toten Pfad auf.
Für jede neu hinzukommende Tabelle gibt es deshalb einen eigenen
Anschlusstest.

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_migrator_changeRoot_rewritesMatchingPaths` | Buy-Dokument unter altem Root | Neuer Pfad in DB, `Result::rewritten` = 1 |
| `test_migrator_changeRoot_rewritesShareSplitDocuments` | Split-Dokument unter altem Root (Anschlusstest, 08.08.2026) | `rewritten` = 1, `updateFailed` = 0, neuer Pfad in `share_splits` |
| `test_migrator_changeRoot_leavesOutsidePathsUntouched` | Dokument außerhalb des alten Root | `Result::outsideRoot` = 1, Pfad unverändert in DB |
| `test_migrator_changeRoot_alreadyCorrectPath_notCounted` | Pfad bereits identisch zum Zielpfad | `Result::alreadyInRoot` erhöht, kein DB-Write |
| `test_migrator_changeRoot_oldRootNeedNotExistOnDisk` | Alter Root ist ein Windows-Pfad, der auf dem Testrechner nicht existiert | Umschreibung funktioniert trotzdem (reiner String-Vergleich) |
| `test_migrator_changeRoot_windowsBackslashPaths_matchedCorrectly` | Gespeicherte Pfade mit `\` (z. B. `B:\Depot\...`) | Werden korrekt erkannt und umgeschrieben, unabhängig vom Test-Betriebssystem |
| `test_migrator_changeRoot_noDocuments_returnsZeroResult` | Leere Datenbank | Alle Zähler = 0 |

DocumentRootMigrator — `detectCommonRoot()`:

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_migrator_detect_commonParentDetected` | Alle Dokumente unter einem gemeinsamen Ordner | `suggestedRoot` korrekt, `ambiguous` = false |
| `test_migrator_detect_windowsPathsOnLinuxHost_stillDetected` | Nur `B:\...`-Pfade, Test läuft auf Linux | `absoluteCount` = Anzahl Dokumente, `suggestedRoot` korrekt ermittelt (Regressionstest für den 18.07.2026 gemeldeten Bug) |
| `test_migrator_detect_noCommonParent_setsAmbiguous` | Dokumente in unabhängigen Ordnern | `ambiguous` = true, `suggestedRoot` leer |
| `test_migrator_detect_relativePaths_excludedFromDetection` | Gemischt: absolute und rein relative Pfade (bloße Dateinamen) | `relativeCount` korrekt, diese fließen nicht in `suggestedRoot` ein |
| `test_migrator_detect_emptyDatabase_returnsZeroResult` | Keine Aktien/Dokumente vorhanden | Alle Zähler = 0 |

DocumentRootMigrator — `isPathWithinRoot()` (19.07.2026, Durchsetzung "nur
Root auswählbar" in ViewBuyEdit/ViewSaleEdit/ViewDividendEdit/
ViewBrokerageEdit/ViewShareAdd):

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_isPathWithinRoot_emptyRoot_alwaysTrue` | Kein Root konfiguriert | `true`, unabhängig vom Pfad |
| `test_isPathWithinRoot_exactRootPath_true` | Pfad == Root | `true` |
| `test_isPathWithinRoot_directChild_true` | Datei direkt im Root | `true` |
| `test_isPathWithinRoot_nestedSubdirectory_true` | Datei mehrere Ebenen unter Root | `true` |
| `test_isPathWithinRoot_outsideRoot_false` | Datei in unabhängigem Ordner | `false` |
| `test_isPathWithinRoot_similarPrefixNotSubdirectory_false` | Ordner mit ähnlichem Namen ohne Trennzeichen (z. B. "Belege2" vs. "Belege") | `false` — kein reiner `startsWith()`-Bug |
| `test_isPathWithinRoot_windowsBackslashPath_crossPlatform_true` | Windows-Pfad mit `\`, Root mit `/` | `true`, unabhängig vom Test-Betriebssystem |

@note Die `onBrowseDocument()`-Methoden der fünf Editier-Dialoge selbst
sind NICHT unit-getestet — der Fehlerfall (Datei außerhalb des Root) ruft
`OwnMessageBox::critical()` auf und würde einen headless Testlauf
blockieren (bekannte Konvention, siehe `TestDocumentsSettingsForm`-Klassendoku
oben). Die zugrundeliegende Logik ist über `isPathWithinRoot()` vollständig
abgedeckt — nur der UI-Dialog-Aufruf selbst bleibt ungetestet.

### ShareDetailsForm: Dokument-Vorschau per Zeilenauswahl (19.07.2026) — Testabdeckung nachgezogen

`rowActivatedWithDocument(userData, documentPath)` (`OverviewTabWidget`,
additiv neben dem unverändert bestehenden `documentActivated()`, siehe
ARCHITECTURE.md, "ShareDetailsForm: Dokument-Vorschau per Zeilenauswahl statt
Doppelklick") und `ViewShareDetails::wireOverviewTab()` (Erst-Zeilen-Auswahl
+ Dokument-Laden bei Tab-Wechsel, Vorschau-Leeren bei Rückkehr zur Übersicht)
sind jetzt abgedeckt:

- `tst_overviewtabwidget.cpp`: sieben neue Tests für
  `rowActivatedWithDocument()`/`documentActivated()` (Doppelklick-Regression),
  siehe eigener Tabellenabschnitt oben im `tst_overviewtabwidget`-Kapitel.
- `tst_mainwindow.cpp`: drei neue Tests direkt an
  `test_mainTabChanged_resetsOverviewTabsToUebersicht` angehängt (siehe
  unten, "tests/forms/ — MainWindow"):
  `test_shareDetailsGewinneVerluste_tabChange_selectsFirstRowInJahresTab`,
  `test_shareDetailsGewinneVerluste_rowClick_emitsRowActivatedWithDocumentPath`,
  `test_shareDetailsGewinneVerluste_tabChange_backToUebersicht_clearsSelection`.

Bewusst weiterhin **nicht** abgedeckt: `DocumentPreviewPanel` selbst zeigt
tatsächlich das richtige Dokument an (`showDocument()`/`clearDocument()`
haben kein öffentliches, testbares Zustands-API, siehe ARCHITECTURE.md,
"DocumentPreviewPanel") — die drei `tst_mainwindow.cpp`-Tests prüfen daher
die Zeilenauswahl-/Signal-Ebene (Tabellen-Selektion, `rowActivatedWithDocument`-
Payload), nicht die tatsächliche Panel-Anzeige. Bekannte, bewusste Lücke,
identisch zur bereits bestehenden Einschränkung bei den Editier-Dialogen.

---

### PortfolioChartForm (tests/forms/tst_portfoliochartform.cpp)

Executable: `tst_portfoliochartform`
Klasse unter Test: `PresenterPortfolioChart`

Fake-View/Fake-Model-Paar wie `tst_chartform` — keine echte Datenbank, kein
`QWidget`, keine QtCharts-Instanziierung. Die eigentliche Rechenlogik ist in
`tst_portfolioseriescalculator` abgedeckt; hier geht es um das Zusammenspiel:
welche Setter der Presenter in welcher Reihenfolge bedient, wie er den
Zeitraum umrechnet und wie er die Texte aufbaut. `FakeViewPortfolioChart`
schreibt dafür zusätzlich die Aufrufreihenfolge in ein `callLog` mit.

Wie in `tst_chartform` klemmt `setMaxIntervalCount()` im Fake bewusst **nicht**
automatisch den Zählwert — anders als das echte `QSpinBox::setMaximum()`.
Die presenter-seitige Begrenzung muss unabhängig davon greifen.

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_loadAndDisplay_setsTodayAsDefaultStartDate` | Vorgabe-Start-Datum | `QDate::currentDate()` |
| `test_loadAndDisplay_drawsCurveAndRangeInfo` | Erster Aufbau | Zwei Punkte (0,00 / 100,00), Kopfzeile mit Zeitraum und Entwicklung |
| `test_loadAndDisplay_showsCalculatingBeforeChartData` | Reihenfolge | `showCalculating()` vor `setChartData()` |
| `test_loadAndDisplay_noSharesAtAll_showsEmptyChart` | Leeres Portfolio | Hinweis statt Chart, keine Warnzeile |
| `test_loadAndDisplay_onlySharesWithoutHistory_warnsAndShowsEmpty` | Nur Aktien ohne Historie | Hinweis plus Warnzeile mit Namen |
| `test_warningNamesExcludedShares` | Gemischtes Portfolio | Nur die ausgeschlossene Aktie steht in der Warnzeile |
| `test_warningIsClearedWhenAllSharesHaveHistory` | Alle Aktien mit Historie | Warnzeile leer |
| `test_onControlsChanged_recomputesWithNarrowerWindow` | Fenster auf einen Tag | Hinweis statt Chart |
| `test_reload_readsTheModelAgain` | `reload()` | Model wird ein zweites Mal gelesen |
| `test_onControlsChanged_doesNotReadTheModelAgain` | Datencache | Model wird genau einmal gelesen |
| `test_computeRangeStart_allUnits` | Zeitraumbeginn | Tag/Woche/Monat/Jahr rückwärts vom Start-Datum |
| `test_computeRangeStart_countBelowOneIsTreatedAsOne` | Anzahl 0 | Wird als 1 behandelt |
| `test_computeMaxIntervalCount_stopsAtOldestValue` | Obergrenze | 10 bei zehn Tagen Historie |
| `test_computeMaxIntervalCount_withoutHistoryReturnsOne` | Keine Historie | 1 |
| `test_computeMaxIntervalCount_oldestNotBeforeRangeEndReturnsOne` | Ältester Wert = Start-Datum | 1 |
| `test_computeMaxIntervalCount_yearsAreCounted` | Interval Jahr | 3 bei drei Jahren Historie |
| `test_buildWarningText_emptyForEmptyList` | Keine ausgeschlossenen Aktien | Leerer String |
| `test_buildWarningText_joinsNames` | Mehrere Namen | Kommagetrennt |
| `test_buildRangeInfo_withoutPointsShowsOnlyRange` | Ohne Punkte | Nur Zeitraum, kein Entwicklungsteil |
| `test_buildRangeInfo_usesLastPoint` | Mit Punkten | Werte des letzten Punkts, lokalisiert formatiert |

---

### ModelPortfolioChart (tests/forms/tst_portfoliochartform.cpp, Klasse TestModelPortfolioChart)

Executable weiterhin `tst_portfoliochartform` — Aktiensplit-Behandlung,
Phase 2b (07.08.2026, siehe ARCHITECTURE.md "Offene Punkte"). Anders als
`TestPortfolioChartForm` oben läuft diese Klasse gegen eine echte
In-Memory-SQLite-Datenbank; die Datei hat deshalb seit dieser Phase einen
eigenen, mehrklassigen `main()` (Muster wie `tst_mainwindow.cpp`) statt
`QTEST_MAIN`. Geprüft wird, dass `ModelPortfolioChart::loadPortfolioInput()`
Splits beim Laden tatsächlich anwendet — die Rechenlogik selbst ist bereits
eigenständig in `tst_portfolioseriescalculator.cpp` und
`tst_sharesplitadjuster.cpp` getestet.

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_loadPortfolioInput_noSplits_matchesRawValues` | Keine Splits | Kauf und Tageswert unverändert |
| `test_loadPortfolioInput_split_scalesBuyAndPriceToTodayScale` | Alphabet-Fixture: Kauf und Tageswert vor einem 20:1-Split, beide unbereinigt gespeichert | Beide landen auf derselben heutigen Skala (100 Stück à 50,15 €) |
| `test_loadPortfolioInput_split_saleAfterSplitStaysInTodayScale` | Verkauf nach dem Split | Werte unverändert, keine weitere Umrechnung nötig |
| `test_loadPortfolioInput_split_costsStayUnscaled` | Brokerage-Kosten trotz Split | Betrag bleibt exakt 9,90 € |
| `test_loadPortfolioInput_reverseSplit_scalesDown` | Reverse-Split 1:10 | 100 Stück à 5,00 € → 10 Stück à 50,00 € |

@note Bugfix 08.08.2026 — dieser Block war von seiner Einführung an nie
lauffähig. Der von Hand geschriebene `main()` legte keine `QCoreApplication`
an; ohne die verweigert `QSqlDatabase::addDatabase()` die Arbeit, warnt nur
("QSqlDatabase requires a QCoreApplication") und gibt ein ungültiges
`QSqlDatabase` zurück, dessen `open()` dann in einen SIGSEGV läuft. Das
`QVERIFY` in `initTestCase()` kam gar nicht erst zum Zug.

Unbemerkt blieb das, weil `TestPortfolioChartForm` davor mit 23 Tests grün
durchläuft — die Klasse arbeitet ausschliesslich mit Fake-View und Fake-Model
und fasst Qt SQL nie an. Wer nur auf die Zusammenfassung schaut, sieht erst
Erfolge und dann einen Absturz, der nach einem Umgebungsproblem aussieht.

Lehre für künftige mehrklassige `main()`: Wenn eine Testklasse mit
Datenbankzugriff zu einer bis dahin reinen Fake-Datei dazukommt, muss die
`QCoreApplication` mitkommen. `QTEST_MAIN` erzeugt sie automatisch — genau
diese Absicherung entfällt beim Handschreiben von `main()`. Betroffen sind
davon `tst_mainwindow.cpp`, `tst_shareeditform.cpp`, `tst_sharesplitsform.cpp`,
`tst_documentssettingsform.cpp` und `tst_portfoliochartform.cpp`; alle ausser
der letzten hatten ihre `QApplication` von Anfang an.

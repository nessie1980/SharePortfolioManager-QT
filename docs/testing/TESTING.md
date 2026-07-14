# Test-Dokumentation {#testing}

## Übersicht

Das Projekt verwendet **Qt Test** als Unit-Test-Framework. Tests werden als separate
Executables gebaut und über CMake's `ctest` oder Qt Creators Test-Panel ausgeführt.

---

## Test-Ausführung

### Qt Creator (empfohlen)
1. **Tools → Tests → Test-Ergebnisse** öffnen
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
./bin/tst_database
./bin/tst_appstartup
./bin/tst_iconprovider
./bin/tst_websitesconfig
./bin/tst_documentsconfig
./bin/tst_sharecalculator
./bin/tst_mainwindow
./bin/tst_shareeditform
./bin/tst_buysform
./bin/tst_backupsettingsform
./bin/tst_xmlportfolioparser
./bin/tst_portfoliovalidator
./bin/tst_portfolioimporter
```

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
ARCHITECTURE.md, "Offene Punkte / TODO") ist ein echter Busy-Test jetzt
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
`ShareRepository`, `BrokerageRepository` und `DailyValuesRepository` ab — CRUD-Operationen,
Filterung, Sortierung und Transaktionsverhalten je Repository.

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

---

### tests/database/ — Database Unit-Tests

Tabellen-Existenz, Indizes, Foreign Keys, Default-Werte, WAL-Modus und Transaktionen.

---

### tests/app/ — AppStartup + IconProvider Unit-Tests

Startverhalten der Applikation (fehlende DB, leerer Pfad, erstes Öffnen) und Icon-Verfügbarkeit
aller definierten `IconProvider::IconName`-Werte — `tst_appstartup` und `tst_iconprovider`.

---

### tests/config/ — Konfiguration Unit-Tests

Laden und Parsen von `WebSites.xml` und `Documents.xml` — `tst_websitesconfig` und `tst_documentsconfig`.

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
| `test_construction_actionsDisabledAtStart` | Menüaktionen ohne Portfolio deaktiviert | `isEnabled()` = false |
| `test_clearPortfolioTables_removesAllRows` | 2 Datentabellen starten leer, 2 Footer behalten ihre 3 Summenzeilen | `emptyCount` = 2, `footerCount` = 2 |
| `test_finalValueTable_showsFinalFields` | Regression Depotwert-Anzeige: Tab zeigt die `…Final`-Felder (mit Brokerage), nicht die brokeragefreien Marktwerte | "Aktuelle Entwicklung" = `profitLossFinal` (-1009,90), "Einzahlung" = `purchaseValueFinal` (1009,90) statt 1000,00 |
| `test_finalValueTable_priceAndCostDividendBottomColorIsNeutral` | Regression Bugfix 03.07.2026: Unterzeile von "Kosten/Dividenden" und "Preis" (Depotwert) nutzt `neutral` statt `muted` | `BottomColor.alpha()` = `neutral.alpha()` für beide Zellen |
| `test_marketValueTable_priceBottomColorIsNeutral` | Regression Bugfix 03.07.2026: Unterzeile von "Preis" (Marktwert) nutzt `neutral` statt `muted` | `BottomColor.alpha()` = `neutral.alpha()` |
| `test_finalValueFooter_costDividendCell` | Depotwert-Footer: Kosten/Dividenden als 2-zeiliger Wert in der Mittelzeile | Zelle (Zeile 1, Spalte Kosten/Dividenden) `TwoLineRole::Top` = `totalBrokerage` (9,90), `Bottom` = `totalDividend` (0,00) |
| `test_updatePortfolioLabel_defaultValues` | Portfolio-Label existiert | `findChild<QLabel*>()` nicht null |
| `test_updateWindowTitle_showsFileName` | Fenstertitel enthält Dateinamen nach Öffnen | Titel enthält Dateinamen |
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
| `test_soundSettings_saveUpdateEnabled` | Sound-Einstellung (Update) gespeichert | `soundUpdateEnabled()` = true |
| `test_soundSettings_saveErrorEnabled` | Sound-Einstellung (Fehler) gespeichert | `soundErrorEnabled()` = true |
| `test_soundSettings_saveUpdateFile` | Sound-Datei (Update) gespeichert | Pfad korrekt geladen |
| `test_soundSettings_saveErrorFile` | Sound-Datei (Fehler) gespeichert | Pfad korrekt geladen |
| `test_soundSettings_scanFallback` | Kein Sound-Gerät → Fallback | Kein Absturz |
| `test_soundFile_missingDisablesSound` | Fehlende Sound-Datei deaktiviert Sound | Sound disabled |

@note **BackupSettingsForm (implementiert 08.07.2026):** eigene Fälle in
`tests/forms/tst_backupsettingsform.cpp` (siehe eigener Abschnitt weiter
unten), nicht in `tst_mainwindow.cpp` — analog `tst_buysform`/
`tst_shareeditform`. Regressionstests für `createBackup()` selbst (Rotation,
Präfix-Änderung, `mkpath()`, Enable/Disable) bleiben dagegen in
`TestBackupForm` (unten in dieser Datei), da `createBackup()` eine private
Methode von `MainWindow` ist und dessen volle Konstruktion braucht.

| `test_aboutForm_appVersionSet` | About-Dialog zeigt App-Version | Version-Label nicht leer |
| `test_aboutForm_pdfConverterDetected` | About-Dialog zeigt PDF-Converter-Status | Label nicht leer |
| `test_deleteShare_removesShareFromDatabase` | Share + Remove → DB leer | `findAll().size()` = 0 |
| `test_deleteShare_nonExistentGuid_returnsFalse` | Nicht-existente GUID → kein Absturz | DB bleibt leer |
| `test_deleteShare_actionDeleteDisabledAtStart` | Entfernen-Aktion ohne Selektion deaktiviert | `isEnabled()` = false |
| `test_deleteShare_actionDeleteEnabledAfterSelection` | Entfernen-Aktion nach Zeilenauswahl aktiv | `isEnabled()` = true |
| `test_onPortfolioRowDoubleClicked_nullItem_doesNotCrash` | Doppelklick-Slot mit `item == nullptr` | Kein Absturz |
| `test_onPortfolioRowDoubleClicked_emptyGuid_doesNotCrash` | Zeile mit geleerter GUID (Qt::UserRole) | Kein Absturz, kein modaler Dialog |
| `test_shareDetailsDialog_validShare_constructsAndShowsCloseButtonText` | `ViewShareDetails` direkt konstruiert | `hasValidShare()` = true, Fenstertitel = Aktienname, Close-Button = "Schließen" |
| `test_chartWheel_overCountSpinAndChartView_changesIntervalCountAndRefreshes` | Mausrad-Events (`QWheelEvent`) auf `countSpin` (ohne Fokus) und auf `chartView`-Viewport | Beide erhöhen/verringern `intervalCount()`; löst jeweils einen Refresh aus (siehe ARCHITECTURE.md, "ChartForm-Details") |
| `test_chartCheckboxes_heldAndTradedVolumeAreMutuallyExclusive` | `seriesCheckBox_HeldVolume` per `findChild()` angehakt (ergänzt 12.07.2026, siehe ARCHITECTURE.md "ChartForm-Details") | `seriesCheckBox_TradedVolume` wird `setDisabled(true)` und bekommt einen Tooltip; nach dem Abhaken wieder `isEnabled() == true` und Tooltip leer — Prüfung erfolgt symmetrisch in beide Richtungen |

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
| `test_modelBuyEdit_updateBuy_createsBrokerageIfMissing` | Kein Brokerage vorhanden → wird erstellt | `loadBrokerage()` danach gültig |
| `test_modelBuyEdit_removeBuy_deletesBrokerageFirst` | Delete in richtiger Reihenfolge (FK) | Buy + Brokerage entfernt |
| `test_modelBuyEdit_removeBuy_rollsBackOnError` | `removeBuy` auf nicht-existenter GUID → kein Absturz (SQLite DELETE gibt bei 0 Treffern kein Fehler zurück) | Kein Absturz |
| `test_modelBuyEdit_orderNumberExists_true` | Vorhandene Ordernummer erkannt | `orderNumberExists()` = true |
| `test_modelBuyEdit_orderNumberExists_excludeGuid` | Eigene Ordernummer wird ausgeschlossen | `orderNumberExists()` = false beim Edit |
| `test_modelBuyEdit_loadBuys_orderedByDate` | Käufe nach Datum aufsteigend | Datums-Reihenfolge korrekt |
| `test_modelBuyEdit_loadBrokerage_notFound_returnsInvalid` | Kein Brokerage → ungültiges Objekt | `isValid()` = false |

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

---

#### tst_shareeditform — ShareEditForm

Executable: `tst_shareeditform`
Klassen unter Test: `ViewShareEdit`

@note `ViewShareEdit.cpp` zieht alle vier Sub-Form-Trios (`BuysForm`,
`SalesForm`, `DividendForm`, `BrokeragesForm`) als Compile-Abhängigkeit rein —
diese werden in `tst_shareeditform` nur kompiliert und gelinkt, aber nicht
getestet. `ModelShareEdit` und `PresenterShareEdit` sind ebenfalls Compile-
Abhängigkeiten; ihre Tests verbleiben in `tst_mainwindow`.

ViewShareEdit:

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_viewShareEdit_canBeConstructed` | Dialog öffnet ohne Absturz | Fenstertitel enthält "Aktie" |
| `test_viewShareEdit_hasPencilButtons` | Vier Pencil-Buttons vorhanden | Anzahl Buttons mit Icon und leerem Text = 4 |
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

#### tst_sharedetailsform — ShareDetailsForm (Depotwert 09.07., Marktwert 10.07., Gewinne/Verluste-/Dividenden-/Kosten-Tabs 13.07.2026)

Executable: `tst_sharedetailsform`
Klasse unter Test: `PresenterShareDetails`

@note Deckt "Komplette Depotbewertung" **und** "Komplette Marktbewertung" ab
(siehe ARCHITECTURE.md, "ShareDetailsForm-Details"). Der Aktien-Chart-Tab
selbst ist seit 12.07.2026 implementiert — seine Tests liegen in einer
eigenen Executable, `tst_chartform` (siehe eigener Abschnitt unten), analog
zur Trennung von `tst_sharedetailsform`/`tst_shareeditform`. Die
Gewinne/Verluste-, Dividenden- und Kosten-Tabs sind seit 13.07.2026
implementiert (siehe ARCHITECTURE.md, "Gewinne/Verluste-, Dividenden-,
Kosten-Tabs") — auf Presenter-Ebene (reines Durchreichen der drei neuen
`IModelShareDetails::load*()`-Methoden an `IViewShareDetails::populate*()`)
durch zwei Tests unten abgedeckt. **Nicht** abgedeckt: `OverviewTabWidget`
und `DocumentPreviewPanel` selbst (kein eigenes Test-Target, siehe
ARCHITECTURE.md, "Offene Punkte / TODO") — insbesondere die Existenzprüfung in
`DocumentPreviewPanel::showDocument()` ist bislang durch nichts abgesichert.

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
| `test_loadAndDisplay_marketMode_doesNotPopulateNewTabs` | `marketValueMode = true`, Model liefert 2 Sales/1 Dividend/3 Brokerages | `gewinneVerlusteCalled`/`dividendenCalled`/`kostenCalled` bleiben `false`, `saleRows`/`dividendRows`/`brokerageRows` bleiben leer — `ViewShareDetails` legt die drei Tabs im Marktwert-Modus gar nicht erst an, der Presenter darf sie folglich auch nicht befüllen |
| `test_loadAndDisplay_depotwertMode_populatesGewinneVerlusteDividendenKosten` | `marketValueMode = false` (Default), gleiche Fixture | Alle drei `*Called`-Flags `true`, `saleRows.size() == 2`/`dividendRows.size() == 1`/`brokerageRows.size() == 3` — reines Durchreichen der Model-Listen, keine Presenter-Logik |

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

**Anzahl-Kappung (ergänzt 12.07.2026 auf Nessies Vorgabe, siehe
ARCHITECTURE.md "ChartForm-Details"):** `FakeModelChart::earliestDailyValueDate()`
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
Stub-Member ohne DB-Zugriff.

`StubViewSaleEdit` implementiert `setAllBuys()` als No-op — der Aufruf
durch den Presenter im Konstruktor wird damit ohne Seiteneffekt absorbiert.

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
  (Datum | Anteile | x | Kaufkurs | = | Kaufsumme | + | Kosten | - | Rabatt | = | Gesamt | Dok.)
- Den Doppelklick-Dokument-Vorschau-Dialog
- Die 5-gliedrige G/V-Zusammenfassung
  (Ges. Anteile . Ges. Verkauf - Ges. Kauf inkl. Kosten - Verkaufsgebuehren/Steuern = G/V)

Die Dok-Icon-Logik (`setCellWidget` + Icon-Auswahl nach Dateiendung) ist identisch
zum bereits abgedeckten `populateOverview`-Muster
(`test_viewSaleEdit_populateOverview_docIconWhenPathSet` /
`test_viewSaleEdit_populateOverview_docDashWhenNoPath`). Neue Tests wuerden
keinen zusaetzlichen Mehrwert bringen.

Der Dokumentpfad-Lookup im Edit-Modus verwendet `m_allBuys` (alle Kaeufe inkl.
vollstaendig verkaufter) — befuellt durch `setAllBuys()` im Presenter-Konstruktor.
Dadurch koennen auch Kaeufe mit `volumeSold == volume` korrekt nachgeschlagen werden.

---

Konfiguration & Settings:
| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_configurations_webSitesLoaded` | WebSites.xml ladbar | `Success`, `count()` > 0 |
| `test_configurations_documentsLoaded` | Documents.xml ladbar | `Success`, `count()` > 0 |
| `test_apiSettings_saveYahooKey` | Yahoo API-Key gespeichert | `apiKeyYahoo()` = gesetzter Wert |

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

ViewDividendEdit — Fremdwährungs-Modus:
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
Refresh-Flow (siehe "Offene Punkte / TODO") noch nicht gemockt. Manuell verifiziert
am 07.07.2026: Preis wird nach dem Einlesen eines Dividenden-Dokuments korrekt anhand
des geparsten Auszahlungsdatums gesetzt, auch wenn zuvor durch einen beiläufigen
Fokuswechsel auf das Datumsfeld (noch mit Default "heute") ein abweichender
Zwischenwert gesetzt wurde.

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
| `test_viewDividendEdit_populateOverview_jahresTabHasFiveColumns` | Jahres-Tab hat 5 Spalten | `columnCount()` = 5 (Datum, Rate, Anteile, Dividende, Dok.) |
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
| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_viewBrokerageEdit_populateOverview_emptyList_noTabs` | Leere Liste → kein Tab | `tabs->count()` = 0 |
| `test_viewBrokerageEdit_populateOverview_singleYear_twoTabs` | 1 Eintrag in 2024 → 2 Tabs | Tab 0 = "Übersicht", Tab 1 enthält "2024" |
| `test_viewBrokerageEdit_populateOverview_twoYears_threeTabs` | Einträge in 2023 + 2024 → 3 Tabs | `count()` = 3 |
| `test_viewBrokerageEdit_populateOverview_jahresTabsDescendingByYear` | Neuestes Jahr zuerst | Tab 1 = 2024, Tab 2 = 2022 |
| `test_viewBrokerageEdit_populateOverview_uebersichtTabHasTable` | Übersicht-Tab enthält QTableWidget | `dataTable` nicht null, 2 Spalten |
| `test_viewBrokerageEdit_populateOverview_jahresTabHasSixColumns` | Jahres-Tab hat 6 Spalten | Datum, Typ, Ges. Gebühren, Rabatt, Netto-Kosten, Dok. |
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
| `test_critical_messageTextVisible` | Meldungstext in Label sichtbar | Label-Text = gesetzter Text |
| `test_information_canBeConstructed` | Dialog öffnet ohne Absturz | Fenstertitel korrekt |
| `test_information_hasSingleOkButton` | Genau ein OK-Button | `buttons.size()` = 1 |
| `test_information_hasIconLabel` | Icon-Label mit Pixmap vorhanden | `pixmap().isNull()` = false |
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

`TwoLineDelegate` und `CenterIconDelegate` sind Header-only ohne `Q_OBJECT` —
kein eigenständiger Test nötig.

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

### Refresh-Flow (Kursdaten-Abruf) — teilweise umgesetzt (07.07.2026)

Der Kursdaten-Abruf (`onRefreshShare`, `onRefreshAll`, `buildDailyValuesUrl`,
`onMarketValuesUpdated`, `onDailyValuesUpdated`) ist direkt in `MainWindow`
implementiert und erfordert echte Netzwerkzugriffe. Der bisherige Blocker —
kein Weg, `ParserLib::Parser` in Tests von echtem Netzwerk zu entkoppeln — ist
seit 07.07.2026 vollständig behoben:

1. `Parser` besitzt einen Konstruktor zur `QNetworkAccessManager`-Injection;
   `ParserTestUtils::FakeNetworkAccessManager` (`tests/parser/FakeNetworkAccessManager.h/.cpp`)
   liefert vorab definierte Antworten ohne echten Netzwerkzugriff (siehe
   ARCHITECTURE.md, "Offene Punkte / TODO", sowie den `tst_parser`-Testblock
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

### `onDailyValuesUpdated()`-Pfad — erledigt (08.07.2026)

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

### `buildDailyValuesUrl()` — erledigt (07.07.2026)

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

**Grid-Selektion folgt dem Refresh (Feature vom 05.07.2026)** — teilweise bereits
umgesetzt und getestet, teilweise weiterhin Teil des zurückgestellten
Refresh-Flow-Testplans.

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

**Erledigt (07.07.2026)** — mit `MainWindow(QNetworkAccessManager*, ...)` +
`FakeNetworkAccessManager` (siehe oben):

- **Regressionstest für Icon-Update bei Einzel-Refresh (Bugfix 06.07.2026):**
  `test_onRefreshShare_iconRegression_updatesChartIconsViaFakeNetwork` —
  Aktie mit zuvor negativem Vortagswert (Icon `NegativStrong`), Refresh
  liefert einen positiven `prevDayPct` (+20 %) → Icon wechselt in **beiden**
  Tabellen (`PrevDayChart` und `CompleteChart`) auf `PositivStrong`. Dieser
  Fall wurde vor dem Fix nicht abgedeckt, weil `onMarketValuesUpdated()` die
  Icon-Zellen schlicht nie anfasste (nur `setTwoLine()` für die
  Text-Spalten) — Text und Icon liefen dadurch nach einem Einzel-Refresh
  dauerhaft auseinander, bis der nächste volle `populatePortfolioTables()`-
  Aufbau (z. B. Neustart) das Icon wieder korrigierte.
- **`enableShareActions`-Lambda-Busy-Guard — Regressionstest, deckte Bugfix
  07.07.2026 auf:** `test_onRefreshShare_busyGuard_selectionDuringRefreshDoesNotReenableActions`.
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

**Weiterhin offen** — die folgenden Aspekte hängen ebenfalls am Refresh-Flow,
sind mit der jetzt vorhandenen Infrastruktur umsetzbar, aber noch nicht
geschrieben:

- Während `onRefreshAll()` läuft: nach dem Start jeder Aktie in der Queue ist
  in **beiden** Tabellen (`m_finalValueTable`, `m_marketValueTable`) die Zeile
  mit `item(row, 0)->data(Qt::UserRole) == share.guid()` selektiert
  (`currentRow()` entsprechend gesetzt), unabhängig vom aktiven Tab.
- Erfolgreicher Abschluss von "Alle aktualisieren" (Queue leer, kein Fehler):
  nach `onRefreshShareFinished()` ist in beiden Tabellen Zeile 0 selektiert.
- Abgeschlossener Einzel-Refresh (`onRefreshShare()`, kein "Alle
  aktualisieren"): Selektion bleibt auf der aktualisierten Aktie stehen,
  `selectFirstShareRow()` wird nicht aufgerufen.
- Fehlerfall (`m_errorOccurred == true`) während `onRefreshAll()`: Selektion
  bleibt unverändert auf der Aktie stehen, bei der der Fehler auftrat —
  `selectFirstShareRow()` wird nicht aufgerufen, unabhängig davon ob noch
  weitere Aktien in der Queue standen.

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

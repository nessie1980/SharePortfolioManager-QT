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
| `test_start_fails_when_busy` | Guard: keine RegExList | `ParserErrorCode::NoRegexListGiven` |
| `test_onvista_realtime_json_parsing` | OnVista JSON Deserialisierung | Preis, Währung, Vortagskurs korrekt |
| `test_onvista_history_json_parsing` | OnVista Historie JSON | Anzahl Einträge, Eröffnungskurs korrekt |
| `test_yahoo_history_json_parsing` | Yahoo Finance Historie JSON | Timestamps, Schlusskurs korrekt |

---

### tests/repositories/ — Repository Unit-Tests

Hinweis: Alle Repository-Tests legen in `initTestCase()` einen Test-Share an.
`Database.cpp` wird **nicht** direkt eingebunden — nur gegen die `Database`-Library
gelinkt (verhindert MOC-Konflikte).

Die Repository-Tests decken `BuyRepository`, `SaleRepository`, `DividendRepository`,
`ShareRepository`, `BrokerageRepository` und `DailyValuesRepository` ab — CRUD-Operationen,
Filterung, Sortierung und Transaktionsverhalten je Repository.

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

> Hinweis: `ModelBuyEdit`/`PresenterBuyEdit`/`ViewBuyEdit` sind weiterhin als
> Produktionsquellen Teil von `tst_mainwindow` (Compile-Abhängigkeit über
> `ViewShareEdit`), werden dort aber nicht mehr getestet — siehe `tst_buysform`.
> `ViewShareEdit` wurde in `tst_shareeditform` ausgelagert.

Stub-Pattern: Für Presenter-Tests werden `StubView*` und `StubModel*`
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
| `test_aboutForm_appVersionSet` | About-Dialog zeigt App-Version | Version-Label nicht leer |
| `test_aboutForm_pdfConverterDetected` | About-Dialog zeigt PDF-Converter-Status | Label nicht leer |
| `test_deleteShare_removesShareFromDatabase` | Share + Remove → DB leer | `findAll().size()` = 0 |
| `test_deleteShare_nonExistentGuid_returnsFalse` | Nicht-existente GUID → kein Absturz | DB bleibt leer |
| `test_deleteShare_actionDeleteDisabledAtStart` | Entfernen-Aktion ohne Selektion deaktiviert | `isEnabled()` = false |
| `test_deleteShare_actionDeleteEnabledAfterSelection` | Entfernen-Aktion nach Zeilenauswahl aktiv | `isEnabled()` = true |

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

**ModelShareEdit (Datenbanktests):**

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

> **Hinweis:** `currentVolume()` und `firstBuyDate()` enthalten eigene Aggregationslogik
> (Summenbildung bzw. Auswahl des ältesten Eintrags) und sind daher trotz Delegation an
> `BuyRepository` separat getestet. Die übrigen Aggregat-Methoden (`totalSaleValue`,
> `totalProfitLoss`, `totalDividendValue`, `totalBrokerageValue`, …) sind reine
> 1:1-Weiterleitungen ohne eigene Logik und werden über die bereits bestehenden
> Repository-Tests in `tests/repositories/` abgedeckt.

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

**Executable:** `tst_buysform`  
**Klassen unter Test:** `ModelBuyEdit`, `PresenterBuyEdit`, `ViewBuyEdit`

> **Hinweis zur Auslagerung:** `tst_buysform` wurde aus `tst_mainwindow` herausgelöst,
> nachdem die Testzahl der BuysForm groß genug geworden war, um eine eigene
> Executable zu rechtfertigen. Die Produktionsklassen
> `ModelBuyEdit`/`PresenterBuyEdit`/`ViewBuyEdit` bleiben weiterhin auch Teil der
> `tst_mainwindow`-Quellen, da `ViewShareEdit` zur Compile-/Link-Zeit von
> `ViewBuyEdit` abhängt (Pencil-Button "Käufe" öffnet `ViewBuyEdit` direkt) —
> dort existieren dafür aber keine eigenen Tests mehr.

> **Stub-Pattern:** `StubViewBuyEdit` und `StubModelBuyEdit` implementieren die
> jeweiligen Interfaces ohne echte UI oder Datenbank — identisches Muster wie in
> `tst_mainwindow`.

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

**Executable:** `tst_shareeditform`  
**Klassen unter Test:** `ViewShareEdit`

> **Hinweis:** `ViewShareEdit.cpp` zieht alle vier Sub-Form-Trios (`BuysForm`,
> `SalesForm`, `DividendForm`, `BrokeragesForm`) als Compile-Abhängigkeit rein —
> diese werden in `tst_shareeditform` nur kompiliert und gelinkt, aber nicht
> getestet. `ModelShareEdit` und `PresenterShareEdit` sind ebenfalls Compile-
> Abhängigkeiten; ihre Tests verbleiben in `tst_mainwindow`.

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

### tests/forms/ — SalesForm

Hinweis zur Teststruktur: Da `QTEST_MAIN` nur eine Testklasse unterstützt,
läuft `TestSalesForm` in einer eigenen `QObject`-Unterklasse. Ein gemeinsamer
`main()`-Einstiegspunkt ruft `QTest::qExec` für alle Klassen nacheinander auf
(aktuell: `TestMainWindow`, `TestSalesForm`, `TestDividendForm`, `TestOwnMessageBox`, `TestBackupForm`).

Stub-Pattern: `StubViewSaleEdit` und `StubModelSaleEdit` implementieren
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

> **Hinweis:** `test_modelSaleEdit_loadAllBuys_includesSoldOut` und
> `test_modelSaleEdit_loadBrokerageForBuy_returnsBrokerage` sind dokumentiert
> aber noch nicht implementiert.

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

Stub-Pattern: `StubViewDividendEdit` und `StubModelDividendEdit` implementieren
die jeweiligen Interfaces ohne echte UI oder Datenbank.
`StubModelDividendEdit::loadShare()` gibt ein ungültiges `ShareObject{}` zurück —
die WKN/ISIN-Prüfung im Presenter wird damit übersprungen (korrekt für Unit-Tests).

ModelDividendEdit (Datenbanktests):
| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_modelDividendEdit_addDividend_success` | Dividende gespeichert | `loadDividends()` gibt 1 Eintrag zurück |
| `test_modelDividendEdit_updateDividend_success` | Dividende aktualisiert | Neuer Rate-Wert in DB |
| `test_modelDividendEdit_removeDividend_success` | Dividende gelöscht | `loadDividends()` leer danach |
| `test_modelDividendEdit_documentExists_notFound_returnsFalse` | Pfad nicht in DB | `documentExists()` = false |
| `test_modelDividendEdit_documentExists_emptyPath_returnsFalse` | Leerer Pfad | Early Return = false |
| `test_modelDividendEdit_loadDividends_orderedByDate` | Dividenden nach Datum aufsteigend | `dateTime[0]` < `dateTime[1]` |

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

Stub-Pattern: `StubViewBrokerageEdit` und `StubModelBrokerageEdit` implementieren
die jeweiligen Interfaces ohne echte UI oder Datenbank.
>
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

Hinweis zur Testklasse: Anders als SalesForm/DividendForm (eigene `TestSalesForm` /
`TestDividendForm`) laufen alle BrokeragesForm-Tests in `TestMainWindow`, da
BrokeragesForm zusammen mit BuysForm im selben `tst_mainwindow`-Abschnitt geführt wird.
`StubViewBrokerageEdit` und `StubModelBrokerageEdit` sind entsprechend dort als
file-globale Klassen vor `TestMainWindow` definiert.

---

### tests/forms/ — OwnMessageBox

Stub-Pattern: Kein Stub nötig — `OwnMessageBox` hat keine externe
Abhängigkeit zu Datenbank oder komplexen Interfaces. Alle Tests arbeiten
direkt mit dem Widget.
>
Hinweis zu statischen Methoden: `critical()`, `information()` und
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

Hinweis zu BackupWorker-Tests: `BackupWorker::run()` wird in den Tests synchron direkt
aufgerufen — kein Thread nötig. Signals werden via `QSignalSpy` geprüft.

Hinweis zu BackupProgressDialog-Tests: Der Dialog startet einen `QThread` im Konstruktor.
Alle Tests rufen `waitForDialog()` am Ende auf bevor der Dialog den Scope verlässt —
sonst tritt `QThread: Destroyed while thread is still running` auf.
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
(geladen via JOIN über `brokerage_guid`) an. `init()` räumt FK-sicher auf
(`sale_buy_details`, `sales`, `brokerage`, dann `buys`).

Geprüft wird vor allem der Marktwert-Tab inklusive der beim Port korrigierten
Logik. Die Sollwerte sind gegen die C#-Referenz abgeglichen.

| Test | Beschreibung | Prüft |
|------|--------------|-------|
| `test_roundAway_halfAwayFromZero` | Cent-Rundung | half-away-from-zero, positiv/negativ |
| `test_marktwert_coreScenario` | Kernbeispiel (2 Käufe, 1 Verkauf) | `purchaseValue`, `curValue`, `profitLoss`, `completeProfitLossMarket`, `completeCurValueMarket` + Depotwert-Basis |
| `test_marktwert_emptyDetails_sameResult` | Regression „viel zu hoch" | Ergebnis identisch trotz **leerer** `SaleBuyDetails` (Aggregat-basiert) |
| `test_marktwert_columnIdentity` | Spalten-Identität | `Kpl. Marktwert = Kpl. Einzahlung + Kpl. Entwicklung` |
| `test_marktwert_fullySold` | Position komplett verkauft | `volume = 0`, `purchaseValue = 0`, realisierte G/V mit Gebühren |
| `test_marktwert_noSales` | keine Verkäufe | reine unrealisierte Entwicklung, `Kpl. Marktwert == curValue` |
| `test_prevDay_diffAndPct` | Vortagswerte | `prevDayDiff`, `prevDayPct` |

`TwoLineDelegate` und `CenterIconDelegate` sind Header-only ohne `Q_OBJECT` —
kein eigenständiger Test nötig.

---

### Refresh-Flow (Kursdaten-Abruf) — noch nicht getestet

Der Kursdaten-Abruf (`onRefreshShare`, `onRefreshAll`, `buildDailyValuesUrl`,
`onMarketValuesUpdated`, `onDailyValuesUpdated`) ist direkt in `MainWindow`
implementiert und erfordert echte Netzwerkzugriffe — Unit-Tests mit dem
Qt-Test-Framework sind daher nur mit Mocking der `ParserLib::Parser`-Klasse
oder via Stub-URLs sinnvoll. Tests sind zurückgestellt.

`buildDailyValuesUrl()` als reine Berechnungsfunktion (kein Netzwerk, kein UI)
wäre eigenständig testbar und kann bei Bedarf in `tst_mainwindow` ergänzt werden.
Testbare Aspekte wären u.a.: URL-Normalisierung (`{0}`→`%1`, `&amp;`→`&`),
korrekte Periodenauswahl nach Monatsdifferenz, und Verhalten bei ungültigem
`latestExistingDate`.

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

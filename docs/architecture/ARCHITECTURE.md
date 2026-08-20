# Architektur {#architecture}

## Überblick

Der **Share Portfolio Manager** ist eine Desktop-Anwendung zur Verwaltung von Aktienportfolios. Sie
ermöglicht das Erfassen von Käufen, Verkäufen, Dividenden und Brokeragegebühren sowie das
automatische Abrufen und Parsen von Kursdaten aus dem Internet.

Entwickelt mit Qt/C++ für Windows und Linux.

---

## Technologie-Entscheidungen

| Bereich | Technologie | Begründung |
| ------ | ------ | ------ |
| UI-Framework | Qt Widgets | Plattformunabhängig, ausgereift |
| Charting | Qt Charts (Qt6::Charts) | Offizielles Qt-Modul, keine eigene QPainter-Chart-Implementierung nötig (Aktien-Chart-Tab in ShareDetailsForm) |
| Datenspeicherung | SQLite (QSqlDatabase) | Filterung per SQL, skalierbar, kein Server nötig |
| Mehrsprachigkeit | Qt i18n (.ts / .qm) | Eingebaut, kein Neubuild bei Sprachwechsel |
| HTTP / REST | QNetworkAccessManager | Eingebaut in Qt, async, plattformübergreifend |
| JSON-Parsing | QJsonDocument | Eingebaut in Qt |
| PDF-Parsing | pdftotext + QProcess | Bewährt, sauber gewrappt — wandelt PDF in Text für den Regex-Parser |
| PDF-Vorschau | Qt PDF (`QPdfView`) | Qt-Viewer seit 6.4, kein externer Prozess. Fallback: pdftoppm. |
| Logging | Logger-Lib | Wiederverwendbar, Bit-Flag-Levels |
| Einstellungen | QSettings (INI) | Einfach, menschenlesbar, kein XML-Overhead |
| Build-System | CMake 3.21+ | Standard für C++, IDE-unabhängig |

---

## Modulstruktur

@code{.unparsed}
spm-qt/
├── libs/
│   ├── logger/          # Wiederverwendbare Logger-Bibliothek (statisch)
│   │   └── src/         # LogEntry.h/.cpp, Logger.h/.cpp
│   └── parser/          # Wiederverwendbare Parser-Bibliothek (statisch)
│       └── src/         # DataTypes.h, ParsingValues.h/.cpp, Parser.h/.cpp
│           └── JsonObjects/  # OnVistaObjects.h, YahooObjects.h
├── app/                 # Hauptanwendung (Qt Widgets)
│   ├── core/            # Database-Singleton (eigene statische Library)
│   ├── models/          # Domänenobjekte (ShareObject, BuyObject, ...)
│   ├── repositories/    # Datenbankzugriff pro Entität
│   ├── config/          # Konfigurationsklassen (AppSettings, WebSitesConfig, ...)
│   ├── utils/           # Statische Hilfsklassen (ShareCalculator, ShareUpdateRules, ...)
│   ├── forms/           # UI-Schicht — je Fenster ein MVP-Triad
│   │   └── UiConstants.h   # Gemeinsame UI-Größenkonstanten (kFieldHeight, kButtonHeight)
│   ├── AppStartup.h/.cpp   # Startup-Helfer (testbar, verwendet in main())
│   ├── IconProvider.h/.cpp # Icon-Set-Verwaltung (wechselbare Icon-Sets)
│   └── resources/       # Icons (PNG), Sounds (WAV), resources.qrc
├── docs/
│   ├── architecture/    # Diese Dokumente
│   └── doxygen/         # Doxygen-Konfiguration + Theme
├── translations/        # .ts-Quelldateien (spm_de.ts, spm_en.ts)
└── tests/              # Neun Verzeichnisse, alle direkt aus der Root-CMakeLists.txt
    ├── logger/          # Unit-Tests für Logger
    ├── parser/          # Unit-Tests für Parser + FakeNetworkAccessManager
    ├── repositories/    # Unit-Tests für alle sieben Repositories
    ├── database/        # Unit-Tests für Database
    ├── app/             # AppStartup, IconProvider, SingleInstanceGuard
    ├── config/          # WebSitesConfig, DocumentsConfig
    ├── utils/           # ShareCalculator, PortfolioSeriesCalculator, ShareUpdateRules, ShareSplitAdjuster, DocumentClassifier
    ├── xml-importer/    # XmlPortfolioParser, PortfolioValidator, PortfolioImporter
    └── forms/           # Unit-Tests für Forms (MainWindow, ShareAddForm, ShareEditForm, BuysForm, SalesForm, DividendForm, BrokeragesForm, OwnMessageBox, BackupProgressForm, ShareDetailsForm, ChartForm, PortfolioChartForm, OverviewTabWidget, BackupSettingsForm, TraySettingsForm, DocumentsSettingsForm)
@endcode

@note Nicht enthalten: `tests/widgets/`. Das Verzeichnis liegt im
Repository, wird aber von keinem `add_subdirectory()` erfasst und ist damit
tot — siehe "Offene Punkte", "Verwaistes Verzeichnis tests/widgets
entfernen".

---

## Schichtenarchitektur

@code{.unparsed}

┌─────────────────────────────────────────────────────────┐
│                    Presentation Layer                   │
│            Qt Widgets  (QWidget / QDialog)              │
│              View — passiv, keine Logik                 │
├─────────────────────────────────────────────────────────┤
│                    Presenter Layer                      │
│         QObject-Subklassen mit Signals & Slots          │
│      Verbindet View (über IView-Interface) mit Model    │
├─────────────────────────────────────────────────────────┤
│                     Model Layer                         │
│         Validierung, Geschäftslogik, Zustand            │
├─────────────────────────────────────────────────────────┤
│                   Repository Layer                      │
│       Abstrakte Interfaces + SQLite-Implementierung     │
│                  (QSqlDatabase)                         │
├─────────────────────────────────────────────────────────┤
│                    Domain Models                        │
│    ShareObject, BuyObject, SaleObject, DividendObject   │
│              BrokerageObject (POCOs)                    │
├────────────────┬────────────────┬───────────────────────┤
│  Logger-Lib    │  Parser-Lib    │  Config-Schicht        │
│  (statisch)    │  (statisch)    │  AppSettings (INI)    │
│                │  REST + Regex  │  WebSitesConfig       │
│                │                │  DocumentsConfig      │
└────────────────┴────────────────┴───────────────────────┘
@endcode

---

## Build-Struktur (CMake)

Das Projekt ist in vier statische Libraries und eine ausführbare Datei aufgeteilt:

@code{.unparsed}
Logger    (libs/logger)          ← keine internen Abhängigkeiten
Parser    (libs/parser)          ← linkt Logger
Database  (app/core)             ← linkt Qt6::Sql
SharePortfolioManager (app)      ← linkt Logger, Parser, Database
@endcode

Tests linken direkt gegen die jeweiligen Libraries:

@code{.unparsed}
tst_logger              ← Logger
tst_parser              ← Parser, Logger
tst_buyrepository  \
tst_salerepository  |
tst_dividendrepository| ← Database, Qt6::Sql
tst_sharerepository |
tst_brokeragerepository|
tst_dailyvaluesrepository|
tst_sharesplitrepository/
tst_database            ← Database, Qt6::Sql
tst_mainwindow          ← alle ShareEditForm-, ShareAddForm-, BuysForm- (Compile-Dep.), SalesForm-, DividendForm-, BrokeragesForm-, OwnMessageBox-, BackupProgressForm-Quelldateien + alle Repositories + ShareCalculator
tst_buysform            ← BuysForm (ModelBuyEdit, PresenterBuyEdit, ViewBuyEdit) + BuyRepository, BrokerageRepository, ShareRepository
tst_shareeditform       ← ViewShareEdit + alle vier Sub-Form-Trios als Compile-Dep. (BuysForm, SalesForm, DividendForm, BrokeragesForm) + alle Repositories
tst_backupsettingsform  ← BackupSettingsForm + AppSettings, IconProvider (kein DB-/MainWindow-Bezug)
tst_sharedetailsform    ← PresenterShareDetails über Fake-View/Fake-Model (IViewShareDetails, IModelShareDetails) — keine DB, keine Qt-Widgets, kein ShareCalculator
tst_chartform           ← PresenterChart über Fake-View/Fake-Model (IViewChart, IModelChart) — keine DB, keine Qt-Widgets, keine Qt-Charts-Instanziierung
tst_shareupdaterules    ← nur Qt6::Test + ShareObject (Enums) — header-only Regelmodul, keine DB, keine Widgets, kein QApplication
tst_documentclassifier  ← DocumentsConfig + DocumentClassifier, linkt Parser
tst_sharecalculator     \
tst_portfolioseriescalculator| ← alle Repositories + ShareCalculator, Database, Qt6::Sql
tst_sharesplitadjuster  ← nur Qt6::Test + ShareSplitObject — zustandslos, datenbankfrei, kein QApplication
tst_salefifoallocator   ← nur Qt6::Test + BuyObject, ShareSplitObject — zustandslos, datenbankfrei, kein QApplication
tst_overviewtabwidget   ← nur OverviewTabWidget, keine DB, kein ShareCalculator (korrigiert 07.08.2026)
tst_portfoliochartform  ← PortfolioChartForm-Trio + ModelPortfolioChart + PortfolioSeriesCalculator + alle Repositories (zwei Testklassen seit Phase 2b, 07.08.2026 — TestPortfolioChartForm + TestModelPortfolioChart, eigener main() statt QTEST_MAIN)
tst_documentssettingsform ← DocumentsSettingsForm + DocumentRootMigrator + AppSettings
tst_traysettingsform    ← TraySettingsForm + AppSettings, IconProvider
tst_appstartup / tst_iconprovider / tst_singleinstanceguard  ← AppStartup, IconProvider bzw. SingleInstanceGuard
tst_websitesconfig / tst_documentsconfig  ← WebSitesConfig bzw. DocumentsConfig, linken Parser
tst_xmlportfolioparser / tst_portfoliovalidator / tst_portfolioimporter  ← tools/xml-importer + Repositories
@endcode

@note Diese Aufstellung wurde am 06.08.2026 vollständig gegen alle neun
`tests/`-Unterverzeichnisse abgeglichen und umfasste zu diesem Zeitpunkt alle
31 Testziele; seit `tst_sharesplitrepository`/`tst_sharesplitadjuster`
(07.08.2026, Phase 1 der Aktiensplit-Behandlung) waren es 33, seit
`tst_salefifoallocator` (07.08.2026, Phase 2c) waren es 34, seit
`tst_sharesplitsform` (08.08.2026, Phase 3a) waren es 35, seit
`tst_sharesplithint` (09.08.2026, Phase 3b) sind es 36. Die vollständige
Startbefehl-Liste steht in TESTING.md, "Einzelnen Test direkt starten".

---

## UI-Größenkonstanten (forms/UiConstants.h)

Alle Eingabe-Widgets und Buttons in allen Form-Dialogen verwenden gemeinsame Konstanten
aus `app/forms/UiConstants.h`:

```cpp
namespace UiConstants {
    constexpr int kFieldHeight  = 24;  // QLineEdit, QComboBox, QDateEdit, QTimeEdit
    constexpr int kButtonHeight = 24;  // QPushButton
}
```

Diese werden in den `addRow()`-Implementierungen aller Views automatisch per
`field->setFixedHeight(UiConstants::kFieldHeight)` gesetzt — ein einziger Wert
steuert damit alle ~80 Eingabe-Widgets über alle Dialoge.

---

## MVP-Pattern

Das **Model-View-Presenter**-Pattern wird konsequent für alle Fenster verwendet.

### Struktur pro Fenster

@code{.unparsed}
forms/BuysForm/
├── IViewBuyEdit.h          ← reines Interface (pure virtual)
├── IModelBuyEdit.h         ← reines Interface (pure virtual)
├── ModelBuyEdit.h/.cpp     ← Zustand + Validierung
├── PresenterBuyEdit.h/.cpp ← Logik, verbindet View + Model
└── ViewBuyEdit.h/.cpp      ← QWidget, implementiert IViewBuyEdit
@endcode

### Kommunikation

@code{.unparsed}
View  ──(Signal)──▶  Presenter  ──(Methode)──▶  Model
View  ◀─(Methode)──  Presenter  ◀─(Methode)──  Model
@endcode

Der Presenter kennt die View **nur über das Interface** — dadurch ist die View vollständig
austauschbar und isoliert testbar.

### Implementierte Forms

| Form | Verzeichnis | Status |
| ------ | ------ | ------ |
| MainWindow | `forms/MainForm/` | ✅ implementiert |
| ShareAddForm | `forms/ShareAddForm/` | ✅ implementiert |
| ShareEditForm | `forms/ShareEditForm/` | ✅ implementiert |
| BuysForm | `forms/BuysForm/` | ✅ implementiert |
| LoggerSettingsForm | `forms/LoggerSettingsForm/` | ✅ implementiert |
| SoundSettingsForm | `forms/SoundSettingsForm/` | ✅ implementiert |
| ApiSettingsForm | `forms/ApiSettingsForm/` | ✅ implementiert |
| AboutForm | `forms/AboutForm/` | ✅ implementiert |
| SalesForm | `forms/SalesForm/` | ✅ implementiert |
| DividendForm | `forms/DividendForm/` | ✅ implementiert |
| BrokeragesForm | `forms/BrokeragesForm/` | ✅ implementiert |
| ShareSplitsForm | `forms/ShareSplitsForm/` | ✅ implementiert (08.08.2026) — siehe "ShareSplitsForm-Details" |
| OwnMessageBox | `forms/OwnMessageBoxForm/` | ✅ implementiert |
| BackupProgressForm | `forms/BackupProgressForm/` | ✅ implementiert |
| BackupSettingsForm | `forms/BackupSettingsForm/` | ✅ implementiert (08.07.2026) |
| ShareDetailsForm | `forms/ShareDetailsForm/` | 🟨 MVP-Struktur steht, Depotwert-/Marktwert-Box und Aktien-Chart-Tab implementiert (12.07.2026) — siehe Detailabschnitt |
| ChartForm | `forms/ChartForm/` | ✅ implementiert (12.07.2026), eingebettet als Tab 1 von ShareDetailsForm — siehe "ChartForm-Details"; zusätzlich ChartPopup (31.07.2026), rahmenloses Rechtsklick-Popup — siehe "ChartPopup — Rechtsklick-Popup-Chart" |

@note **ShareDetailsForm — Umfang dieser Iteration (09.07.2026):** Nach
Abgleich mit der C#-Referenzimplementierung (`FrmShareDetails` +
`TabControl.cs`/`DividendDetails.cs`/`ProfitLossDetails.cs`/
`BrokerageDetails.cs`) stellte sich heraus, dass ein früherer MVP-Umbau auf
Basis von `ShareDetailsForm.h/.cpp` (Stammdaten/Käufe/Verkäufe/Dividenden/
Brokerages-Tabs) strukturell nicht der Referenz entsprach — diese Tabs
existieren im C# gar nicht. Details siehe Abschnitt "ShareDetailsForm-Details"
unten.

---

### BuysForm-Details

Vollstaendig nach MVP-Pattern implementiert, geoeffnet via Pencil-Button Kaeufe in ViewShareEdit.

`IViewBuyEdit` — Interface mit Lese-Accessoren für alle Eingabefelder:
`loadBuy()`, `clearForm()`, `setVolumeSold()`, `setKurswert()`, `setGesGebuehren()`,
`setEndbetrag()`, `setFieldOk()` / `setFieldError()`, Parse-Statuszeile,
`populateOverview()`, `showOverviewTab()`, `openPdfPreview()`, `clearPdfPreview()`,
`setButtonStates(canRemove, isLastBuy, isEdit)`, `showError()`, `acceptAndClose()`,
`markMissingFieldsAsFailed()`, `hasMissingRequiredFields()`.

`IModelBuyEdit` — Interface: `loadBuys()`, `loadBrokerage()`, `addBuy()`,
`updateBuy()`, `removeBuy()`, `orderNumberExists()`, `documentExists()`, `lastError()`.

`ModelBuyEdit` — Delegiert an `BuyRepository` und `BrokerageRepository`.
`addBuy` und `updateBuy` in SQLite-Transaktion.
`documentExists()` prüft aktienübergreifend per direkter SQL-Abfrage.

@note **Bugfix: fehlender Brokerage-Vorwärts-Link (20.07.2026).** Nachprüfung
des am 15.07.2026 in `ModelSaleEdit` gefundenen Bugs (siehe "SalesForm-
Details" oben, "Erledigt / Archiv", "Brokerage-Vorwärts-Link:
ModelBuyEdit/ModelBrokerageEdit ungeprüft") ergab: `ModelBuyEdit::addBuy()`
war nicht betroffen — die `brokerageGuid` wird dort bereits vor dem Insert
erzeugt und direkt ins `BuyObject` eingebaut, der Vorwärts-Link
(`buys.brokerage_guid`) steht also von Anfang an korrekt.

`ModelBuyEdit::updateBuy()` hatte jedoch denselben Bug wie
`ModelSaleEdit::updateSale()`: im Zweig "kein Brokerage vorhanden — neu
anlegen" bekam die neue Brokerage-Zeile korrekt den Rückwärts-Link
(`brokerage.buy_guid = buy.guid()`), aber `buys.brokerage_guid` wurde nie
auf die neue `brokerageGuid` aktualisiert — es blieb beim Wert, den
`m_buyRepo.update(buy)` bereits zu Beginn der Methode geschrieben hatte.
Betroffen ist `BuyRepository::totalBuyValueBrokerageReduction()`
(`LEFT JOIN brokerage br ON br.guid = b.brokerage_guid`), die für
Depotwert-Berechnungen genutzt wird: ohne den Vorwärts-Link lieferte der
JOIN für Provision/Courtage/Handelsplatzgebühr/Rabatt dieses Kaufs
weiterhin `NULL`/0, obwohl der Brokerage-Datensatz selbst korrekt in der DB
stand. Der bereits vorhandene Test
`test_modelBuyEdit_updateBuy_createsBrokerageIfMissing` deckte das nicht
auf, da er nur den Rückwärts-Link prüfte (`loadBrokerage()` →
`BrokerageRepository::findByBuyGuid()`), nicht den Vorwärts-Link.

Fix: neue `BuyRepository::updateBrokerageGuid(guid, brokerageGuid)`-Methode
(analog zu `SaleRepository::updateBrokerageGuid()`), aufgerufen von
`updateBuy()` in **beiden** Zweigen — auch im "Brokerage existiert
bereits"-Zweig, zur Absicherung gegen einen vom Aufrufer übergebenen
veralteten/leeren `buy.brokerageGuid()`-Wert (derselbe defensive Ansatz wie
bei `ModelSaleEdit::updateSale()`).

`ModelBrokerageEdit` war von vornherein nicht betroffen — es verwaltet
ausschließlich standalone Brokerage-Einträge ohne `buy_guid`/`sale_guid` und
hat mit `buys.brokerage_guid`/`sales.brokerage_guid` nichts zu tun.

`PresenterBuyEdit` — Vollständige Kauf-Logik inkl. Letzter-Kauf-Erkennung,
Live-Validierung, Parse-Pipeline und Dokument-Duplikat-Check.

`ViewBuyEdit` — Linkes Formular (Kaufdaten + Dokument + Button-Bar +
Kauf-Übersicht) + rechte PDF-Vorschau. Feste Größe 1300 × 790 px.

#### Edit-Modi:

| Modus | Bedingung | Verhalten |
| ------ | ------ | ------ |
| Neu-Modus | Kein Kauf ausgewählt | Alle Felder editierbar, Button "Hinzufügen", Re-Parse erlaubt |
| Letzter Kauf | `isLastBuy = true` | Alle Felder editierbar, "Speichern", Re-Parse, Entfernen wenn `volumeSold == 0` |
| Älterer Kauf | `isLastBuy = false` | Nur Dokumentpfad editierbar, Button "Speichern", kein Re-Parse |

Letzter-Kauf-Erkennung:
Lexikographischer Vergleich der ISO-8601-`dateTime`-Strings in `isLatestBuy()` — korrekt da ISO 8601
lexikographisch sortierbar ist.

Entfernen-Bedingung:
`canRemove = isLastBuy && volumeSold == 0` — nur der jüngste Kauf ohne bereits verkaufte Anteile
darf gelöscht werden. Schutz auch in `onRemove()` als Guard implementiert.

#### Methode setButtonStates(canRemove, isLastBuy, isEdit)

- `canRemove`: Entfernen-Button aktiv/inaktiv
- `isLastBuy`: bestimmt `readOnlyMode = !isLastBuy && isEdit` → alle Eingabefelder außer
  Dokumentpfad werden per `setEnabled(false)` deaktiviert
- `isEdit`: Button-Label "Speichern" wenn ein Kauf geladen ist, sonst "Hinzufügen"

`IModelBuyEdit::documentExists(path, excludeGuid)`:
Aktienübergreifende SQL-Abfrage ob ein Dokumentpfad bereits bei einem anderen Kauf verwendet wird.
Verhindert versehentliches doppeltes Zuordnen desselben PDFs.

#### Live-Validierung (Presenter-Slots, alle via editingFinished / activated):

| Slot | Signal-Quelle | Prüft |
| ------ | ------ | ------ |
| `onDateEdited()` | `QDateEdit::editingFinished` | Datum nach Sentinel 2000-01-01 |
| `onDepotNumberEdited()` | `QComboBox::activated` | `currentData()` nicht leer |
| `onOrderNumberEdited()` | `QLineEdit::editingFinished` | Nicht leer + kein Duplikat in DB |
| `onVolumeOrPriceEdited()` | `QLineEdit::editingFinished` | > 0 |
| `onFeeEdited(key, value)` | `QLineEdit::editingFinished` | ≥ 0 (negative ungültig) |
| `onDocumentPathEdited()` | explizit aus `onBrowseDocument()` | Kein Duplikat aktienübergreifend |

`activated()` statt `currentIndexChanged()` für ComboBox — feuert nur bei echtem Benutzer-Klick,
nicht bei programmatischem `setCurrentIndex()`.
`editingFinished` statt `dateChanged` für QDateEdit — feuert nicht bei `setDate()`.

`setFieldOk(field, value)`-Konvention:
Setzt nur das Status-Icon (grüner Haken). Widget-Text wird **nur überschrieben wenn `value` nicht
leer ist** — Live-Validierungsaufrufe übergeben immer einen leeren Value und dürfen bestehende
Formularwerte nicht löschen.

#### Tab-Navigations-Logik (populateOverview, showOverviewTab, currentChanged)

- `m_suppressTabSignal` unterdrückt den `currentChanged`-Lambda während `populateOverview`
  (removeTab + addTab feuern sonst `onReset`)
- `populateOverview` setzt `currentIndex(0)` noch innerhalb des supprimierten Bereichs
- `showOverviewTab()` springt zu Tab 0 (supprimiert) und ruft `clearForm()` auf
- Jahres-Tab-Wechsel: `currentChanged`-Lambda → `tbl->selectRow(0)` → `onRowSelected(guid)`
- Übersicht-Tab-Wechsel: `currentChanged`-Lambda → `onReset()`
- Data-Table wird per `container->property("dataTable")` zuverlässig abgerufen (zwei QTableWidgets
  pro Container: data + footer)

#### Laden eines bestehenden Kaufs (onRowSelected)

`loadBuy(b, brokerage)` befüllt alle Felder (depotNumber-Signale blockiert),
`openPdfPreview(b.document())` zeigt die PDF-Vorschau ohne Re-Parse,
`setButtonStates(canRemove, isLastBuy, true)` setzt den Modus,
`refreshDerivedValues()` berechnet Kurswert/Gebühren/Endbetrag neu,
danach werden Validierungsslots direkt aufgerufen.

Nach Speichern (neu oder edit) und nach Entfernen wird `m_currentBuyGuid.clear()` +
`showOverviewTab()` aufgerufen — Formular zurückgesetzt, Button wieder "Hinzufügen".

#### Dialog-Layout

Der Dialog gliedert sich in zwei Hälften: links das Formular (Kaufdaten, Dokument,
Button-Bar), rechts die PDF-Vorschau. Das Formular enthält die Felder Datum, Uhrzeit,
Depotnummer (ComboBox), Ordernummer, Gekaufte Anteile, Ber. verk. Anteile (readonly),
Kurs, Kurswert (readonly), Provision, Courtage, Handelsplatz, Ges. Gebühren (readonly),
Rabatt, Endbetrag (readonly, grün), Dokumentpfad mit Browse-Button sowie die
Parse-Statuszeile. Darunter folgen die vier Buttons Hinzufügen/Speichern, Entfernen,
Reset und Schließen.

#### Kauf-Übersicht — Frozen-Footer-Layout

Jeder Tab enthält einen Container (`QVBoxLayout`) mit zwei `QTableWidget`s:
scrollbares `dataTable`, einem `QFrame::HLine`-Separator und einem `footerTable`
(1 Zeile, fixedHeight, kein Scrollbar) — die Gesamt-Zeile bleibt damit immer sichtbar.

Spaltenbreiten: Übersicht-Tab: Jahr, Anteile, Einzahlung (100px, stretch, stretch).
Jahres-Tab: Datum, Anteile, Kurswert, Gebühren, Einzahlung, Dok. (100px, je stretch).

Die Spaltenbreiten von `footerTable` werden ueber `QHeaderView::sectionResized` mit
`dataTable` synchronisiert. Alle Werte sind zentriert (`Qt::AlignCenter`). Icons in
der Dokument-Spalte werden per `setCellWidget` mit `QLabel` dargestellt.

#### Dokument-Icon-Auswahl (Jahres-Tab, Spalte "Dokument")

| Dateiendung | Icon |
| ------ | ------ |
| `.pdf` | `DocPdfImage16` |
| `.doc` / `.docx` | `DocWordImage16` |
| `.xls` / `.xlsx` | `DocExcelImage16` |
| unbekannt | `SearchFailed2` (Warnsymbol) |
| kein Dokument | Text "-" |

#### Tab-Klick-Logik (Kauf-Uebersicht):

| Signal | Slot | Verhalten |
| ------ | ------ | ------ |
| `cellClicked` Übersicht | `onUebersichtRowActivated` | Jahr aus `Qt::UserRole` Sp.0 → Jahres-Tab |
| `cellClicked` Jahres-Tab | `onOverviewRowActivated` | GUID aus `Qt::UserRole` Sp.0 → `onRowSelected()` |
| `currentChanged` | Lambda | Selektion leeren; Jahres-Tab: Zeile 0 + `onRowSelected()`; Übersicht: `onReset()` |

Tab-Reihenfolge: Übersicht-Tab zuerst (Index 0), dann Jahres-Tabs **absteigend nach Jahr** (neuestes Jahr = Tab 1).

Alle Tabs verwenden `SelectRows + SingleSelection` — keine Einzelzellen-Selektion moeglich.
Die Gesamt-Zeile (`footerTable`) ist von der Selektion ausgeschlossen, da sie ein eigenes
`QTableWidget` ohne Selektion ist.

Layout-Besonderheit Statuszeile:
Die Statuszeile (Progress + Icon + Label) hat immer `setFixedHeight(22)` auf dem
`statusRow`-Widget — der Platz ist dauerhaft reserviert. Vor dem Parsen sind die
Widgets nicht via `setVisible` versteckt, sondern der `QProgressBar` hat
`stylesheet: "background: transparent; border: none"` und Icon/Label haben keinen Inhalt.
Beim ersten `setParseProgress()`-Aufruf wird der StyleSheet zurückgesetzt → kein
Layout-Shift.

Parse-Pipeline (identisch zu PresenterShareAdd):

@code{.unparsed}
PDF-Datei
    │
    ▼  QProcess("pdftotext -enc UTF-8 -layout file.pdf -")
Klartext (stdout)
    │
    ▼  BankIdentifier-Regex  → Bank erkannt (z.B. ING diba, DKB)
    │  BuyIdentifier-Regex   → Dokumenttyp erkannt
    │
    ▼  ParserLib::Parser::startParsing()
    │  ParsingValues(text, encoding, docEntry->regexList)
    │
    ▼  onParserUpdated() → populateFromResult()
    │  └── setFieldOk("date"|"depotNumber"|"orderNumber"|"volume"|"price"|
    │                  "provision"|"brokerFee"|"traderFee"|"reduction", value)
    │
    ▼  QTimer::singleShot(0) → setUiBusy(false) + Statuszeile
@endcode

#### XML-Feld-Mapping (xmlNameToViewField):

| XML-Feldname | View-Widget-Key | Pflichtfeld |
| ------ | ------ | ------ |
| `Date` | `date` | ✅ |
| `Time` | `time` | — |
| `DepotNumber` | `depotNumber` | ✅ |
| `OrderNumber` | `orderNumber` | ✅ |
| `Volume` | `volume` | ✅ |
| `Price` | `price` | ✅ |
| `Provision` | `provision` | — |
| `BrokerFee` | `brokerFee` | — |
| `TraderPlaceFee` | `traderFee` | — |
| `Reduction` | `reduction` | — |

#### Depotnummer-ComboBox

- `setEditable(false)` — nur bekannte Werte aus `Documents.xml`
- Befüllung beim Konstruktor aus `DocumentsConfig::entries()`:
  Anzeige `"BankName (Identifier)"`, `itemData` = roher Identifier-String
- Beim Parsing: `setFieldOk("depotNumber", value)` sucht per `itemData`-Match
- Validierung: `currentData().toString().isEmpty()` → Pflichtfeld nicht erfüllt

#### Reset-Verhalten (clearForm)

- Alle Eingabefelder auf Standardwerte (0 / leer / heute)
- Alle `m_statusLabels`: `setPixmap(QPixmap())` + `setToolTip("")`
- `m_fieldStates.clear()` → alle Felder auf `Untouched`
- Statuszeile: `progress = 0`, StyleSheet transparent, Icon geleert, Label geleert

#### Integration in ViewShareEdit

- `ViewShareEdit` nimmt `DocumentsConfig*` als Konstruktorparameter und leitet ihn
  an `ViewBuyEdit` weiter
- `onEditBuys()` in `ViewShareEdit` öffnet `ViewBuyEdit` und verbindet
  `PresenterBuyEdit::dataChanged()` mit `ViewShareEdit::refreshSummary()`
- `MainWindow` übergibt `&m_documentsConfig` beim Öffnen von `ViewShareEdit`

---

### SalesForm-Details

Vollständig nach MVP-Pattern implementiert, geöffnet via Pencil-Button Verkäufe in ViewShareEdit.

`IViewSaleEdit` — Interface mit Lese-Accessoren für alle Eingabefelder:
`loadSale()`, `clearForm()`, `populateAvailableBuys()`, `setAllBuys()`,
`setSaleValue()`, `setKaufwert()`, `setGewinnVerlust()`, `setGesGebuehren()`,
`setTaxSum()`, `setAuszahlung()`, `setFieldOk()` / `setFieldError()`, Parse-Statuszeile,
`populateOverview()`, `showOverviewTab()`, `openPdfPreview()`, `clearPdfPreview()`,
`setButtonStates(canRemove, isLastSale, isEdit)`, `showError()`, `acceptAndClose()`,
`markMissingFieldsAsFailed()`, `hasMissingRequiredFields()`.

`setAllBuys(buys)` befüllt `m_allBuys` (alle Käufe inkl. vollständig verkaufter) —
wird vom Presenter im Konstruktor via `loadAllBuys()` gesetzt und im Details-Dialog
für den Dokumentpfad-Lookup im Edit-Modus verwendet.
`populateAvailableBuys(buys)` befüllt `m_availableBuys` (nur Käufe mit verbleibendem
Volumen) — wird für die FIFO-Vorschau im Neu-Modus und die Depot-Filterung verwendet.

`showBuyDetails(summary)` (11.08.2026) nimmt den fertig aufbereiteten Inhalt des
Details-Dialogs entgegen — siehe "Anteilige Kauf-Nebenkosten der FIFO-Zuteilung"
weiter unten. Seither wertet `ViewSaleEdit` weder `m_availableBuys` noch
`m_allBuys` oder `m_splits` selbst aus; die Listen werden aber weiterhin ueber
die Setter befuellt (Interface-Vertrag, Depot-Filterung).

`IModelSaleEdit` — Interface: `loadSales()`, `loadShare()`, `loadAvailableBuys()`,
`loadAllBuys()`, `loadAvailableBuysForDepot()`, `loadBrokerage()`,
`loadBrokerageForBuy()`, `addSale()`, `updateSale()`, `removeSale()`,
`orderNumberExists()`, `documentExists()`, `lastError()`.

`loadAllBuys(shareGuid)` gibt alle Käufe einer Aktie zurück (unabhängig vom
verbleibenden Volumen) — für den Dokumentpfad-Lookup im Details-Dialog.
`loadBrokerageForBuy(buyGuid)` gibt das mit einem Kauf verknüpfte `BrokerageObject`
zurück — delegiert an `BrokerageRepository::findByBuyGuid()`.

`ModelSaleEdit` — Delegiert an `SaleRepository`, `BuyRepository` und
`BrokerageRepository`. `addSale` / `updateSale` / `removeSale` in SQLite-Transaktion.
`addSale` aktualisiert `volumeSold` auf den beteiligten Käufen (FIFO).
`removeSale` macht `volumeSold`-Änderungen rückgängig.
`documentExists()` prüft aktienübergreifend per direkter SQL-Abfrage.

@note **Bugfix: fehlender Brokerage-Vorwärts-Link (15.07.2026).** `SaleRepository`
kennt zwei FK-Richtungen zwischen `sales` und `brokerage`: den Vorwärts-Link
`sales.brokerage_guid → brokerage.guid`, den `findByShare()`/`findByGuid()`/
`findByShareAndYear()` für ihren Brokerage-JOIN nutzen (`kSelectWithBrokerage`),
und den Rückwärts-Link `brokerage.sale_guid → sales.guid`, den
`BrokerageRepository::findBySaleGuid()` (und damit `ModelSaleEdit::
loadBrokerage()`) nutzt. `ModelSaleEdit::addSale()` legte den neuen
Brokerage-Eintrag bisher nur mit dem Rückwärts-Link an, ohne
`sales.brokerage_guid` zu setzen — beim erneuten Laden über `loadSales()`
kam die Provision/Brokerage eines frisch gespeicherten Verkaufs dadurch immer
als 0 zurück, obwohl der Brokerage-Datensatz selbst korrekt in der DB stand
(nur eben nicht über den vom SELECT-JOIN genutzten Weg auffindbar). Derselbe
Bug steckte im "Brokerage neu anlegen"-Zweig von `updateSale()`. Gefunden
über einen View-Ebene-Test für den brokeragefreien Gewinne/Verluste-Tab im
Marktwert-Modus (siehe "OverviewTabWidget-Details"), der echte DB-Daten statt
Fakes verwendet — mit Fakes/direkten `BrokerageRepository`-Zugriffen (wie im
zuvor einzigen Test für diesen Pfad) war der Bug unsichtbar. Fix: neue
`SaleRepository::updateBrokerageGuid(guid, brokerageGuid)`-Methode, von
`addSale()` sowie **beiden** Zweigen von `updateSale()` (auch dem
`existing.isValid()`-Zweig, zur Absicherung gegen einen vom Aufrufer
übergebenen veralteten/leeren `sale.brokerageGuid()`-Wert) aufgerufen.

@note **Bugfix: Fremdschlüssel-Verletzung in `tst_salerepository::init()`
(16.07.2026).** Der obige Regressionstest `test_updateBrokerageGuid` legt
einen `brokerage`-Datensatz an, der über den Rückwärts-Link
(`brokerage.sale_guid → sales.guid`) auf die `sales`-Zeile dieses Tests
zurückverweist. `init()` räumte bisher `sale_buy_details`, `sales` und `buys`
vor jedem Test auf, aber **nicht** `brokerage` — dadurch schlug `DELETE FROM
sales` in jedem nachfolgenden `init()`-Aufruf mit `FOREIGN KEY constraint
failed` fehl, und die betroffene Sale-Zeile (Volumen 5,0) blieb dauerhaft
stehen. `test_totalVolume` summierte dadurch über eine fremde, liegen
gebliebene Zeile mit und lieferte 13,0 statt der erwarteten 8,0. Fix: `init()`
löscht jetzt zusätzlich `brokerage` — vor `sales`, wegen der genannten
FK-Abhängigkeit.

`PresenterSaleEdit` — Vollständige Verkauf-Logik inkl. Letzter-Verkauf-Erkennung,
Live-Validierung, Parse-Pipeline und Dokument-Duplikat-Check.
Depot-gefiltertes FIFO für Kaufzuteilung.

G/V-Berechnung in `refreshDerivedValues()`:

- Edit-Modus: `s.profitLossBrokerageReduction()` direkt aus dem gecachten `SaleObject` —
  beinhaltet alle Gebühren (Kauf-Brokerage, Verkaufs-Gebühren, Steuern).
- Neu-Modus (Vorschau): `saleValue - gesGebuehren + reduction - kaufwert - taxSum` —
  Kauf-Brokerage in der Vorschau nicht verfügbar (kein DB-Join pro FIFO-Kauf).
- `setKaufwert()` zeigt immer `s.buyValue()` (reine Kaufsumme ohne Brokerage) —
  das Anzeigefeld ist unabhängig von der G/V-Berechnung.

`ViewSaleEdit` — Linkes Formular (Verkaufsdaten + Dokument + Button-Bar +
Verkaufs-Übersicht) + rechte PDF-Vorschau. Feste Größe 1300 × 820 px.

#### Pflichtfelder:

| Feld | Widget | Prüfung |
| ------ | ------ | ------ |
| Datum / Uhrzeit | `QDateEdit` + `QTimeEdit` | Datum nach Sentinel 2000-01-01 |
| Depotnummer | `QComboBox` (aus Documents.xml) | `currentData()` nicht leer; steuert FIFO-Filter |
| Ordernummer | `QLineEdit` | Nicht leer + kein Duplikat in DB |
| Verkaufte Anteile | `QLineEdit` (QDoubleValidator) | > 0 |
| Verkaufs-Preis je Aktie | `QLineEdit` (QDoubleValidator) | > 0 |

#### Optionale Felder (alle ≥ 0):

| Feld | Widget | Bemerkung |
| ------ | ------ | ------ |
| Verkaufter Kaufwert | `QLineEdit` read-only | vol × salePrice |
| Gekaufter Kaufwert | `QLineEdit` read-only + Details-Button | FIFO-Buchwert; Details-Dialog zeigt Kaufzuteilung |
| Gewinn / Verlust | `QLineEdit` read-only, farbig | Grün positiv, rot negativ |
| Quellsteuer | `QLineEdit` | ≥ 0 |
| Kapitalertragssteuer | `QLineEdit` | ≥ 0 |
| Solidaritätszuschlag | `QLineEdit` | ≥ 0 |
| Provision | `QLineEdit` | ≥ 0 |
| Courtage | `QLineEdit` | ≥ 0 |
| Handelsplatzgebühr | `QLineEdit` | ≥ 0 |
| Rabatt | `QLineEdit` | ≥ 0 |
| Ges. Gebühren | `QLineEdit` read-only | Provision + Courtage + Handelsplatz |
| Auszahlung | `QLineEdit` read-only, farbig | Grün positiv, rot negativ |
| Dokument | `QLineEdit` read-only + Browse-Button | Duplikat-Check aktienübergreifend |

#### FIFO-Logik (Kaufzuteilung)

- `loadAvailableBuysForDepot(shareGuid, depotNumber)` filtert auf das gewählte Depot und liefert
  verfügbare Käufe ältester → jüngster Kauf (ISO-8601-lexikographische Sortierung via
  `BuyRepository::findByShare` ORDER BY datetime ASC).
- Depotauswahl-Wechsel → `onDepotNumberEdited()` → `populateAvailableBuys()` neu laden.
- Beim Laden eines bestehenden Verkaufs: Depot des Verkaufs → `loadAvailableBuysForDepot` mit
  diesem Depot — bzw. `loadAvailableBuysForDepotExcludingSale` mit diesem Depot UND der GUID
  des geladenen Verkaufs (siehe unten).
- Neuer Verkauf ODER Bearbeitung des jüngsten Verkaufs: FIFO-Zuteilung über `SaleFifoAllocator`
  (Phase 2c der Aktiensplit-Behandlung, 07.08.2026) — wird bei jedem Speichern frisch berechnet,
  auch beim Bearbeiten (Bugfix 07.08.2026: vorher übernahm der Edit-Zweig unverändert die
  gespeicherten `SaleBuyDetails`, selbst bei geänderter Verkaufsmenge). Die Ausgangsliste kommt
  beim Bearbeiten über `loadAvailableBuysForDepotExcludingSale()`, die die eigenen, bereits
  gebuchten Anteile des Verkaufs zunächst virtuell zurückbucht (siehe "SaleFifoAllocator und die
  FIFO-Verkaufszuteilung" oben).
- Details-Button (neben "Gekaufter Kaufwert"): immer aktiv (Neu-Modus, Edit-Modus, älterer Verkauf).
  Öffnet `onShowDetails()` — einen modalen Read-only-Dialog mit zwei GroupBoxen.

GroupBox "Verwendete Käufe" — Frozen-Footer-Tabelle mit 13 Spalten:
Datum | Anteile | `×` | Kaufkurs | `=` | Kaufsumme | `+` | Kosten | `−` | Rabatt | `=` | Gesamt | Dok.
Operator-Spalten 24 px breit, grau. Datum 100 px fix (identisch zur Verkaufsübersicht).
Dok.-Spalte 36 px fix. Alle Wertspalten gestreckt. Footer-Zeile (fett) mit Summen für
Anteile, Kaufsumme (= totBuyVal), Kosten, Rabatt und Gesamt (= totBuyVal + totFees)
(Dok.-Zelle im Footer leer). Spaltenbreiten per `sectionResized` synchronisiert
(initiale Übertragung per `QTimer::singleShot(0)` nach erstem Layout-Durchlauf).

Spaltenwerte pro Zeile (seit Phase 2c, 07.08.2026, durchgängig auf heutiger/
split-bereinigter Skala — dieser Dialog ist eine berechnete Übersicht über
ggf. mehrere Lots, keine Beleg-Abschrift; nur so bleiben Summen über mehrere
Lots hinweg sinnvoll, auch wenn ein Split zwischen zwei Lots liegt):

| Spalte | Wert |
| ------ | ------ |
| Anteile | `ShareSplitAdjuster::adjustedVolume(...)` des zugeteilten Anteils |
| Kaufkurs | `ShareSplitAdjuster::adjustedTransactionPrice(...)` des Kaufkurses |
| Kaufsumme | Anteile × Kaufkurs (beide bereits heutige Skala) |
| Kosten | anteilige Brokerage des Kaufs (`brokeragePart`) — Geldbetrag, unskaliert |
| Gesamt | Kaufsumme + Kosten − Rabatt |

Dok.-Spalte: zeigt das Icon des Kauf-Dokuments (PDF/Word/Excel, identische
Icon-Logik wie Jahres-Tab der Verkaufsübersicht). Kein Dokument → `"-"`.
Tooltip zeigt Dateipfad + "Doppelklick: Dokument anzeigen".
Doppelklick → modaler Vorschau-Dialog (Kind des Details-Dialogs, Titel = Dateiname,
700 × 900 px) mit `QPdfView` (wenn `SPM_HAVE_QTPDF`) oder `pdftoppm`-Fallback —
identische Rendering-Logik wie die rechte PDF-Vorschau im Hauptformular.

Dokument-Pfad-Lookup:

| Modus | Quelle |
| ------ | ------ |
| Älterer, nicht editierbarer Verkauf | `SaleBuyDetail::buyGuid()` → Suche in `m_allBuys` → `BuyObject::document()` |
| Neuer Verkauf / Bearbeitung des jüngsten Verkaufs | `FifoAllocationRow::buyGuid` → Suche in `m_availableBuys` → `BuyObject::document()` |

Im ersten Fall wird `m_allBuys` verwendet (nicht `m_availableBuys`), da ein
Kauf vollständig verbraucht sein kann (`volumeSold == volume`) und damit
nicht mehr in `m_availableBuys` erscheint.

GroupBox "Übersicht Gewinn/Verlust-Rechnung" — einzeilige Kästchen-Anzeige
mit je einem normalen Label (Bezeichner) und einem fetten Label (Zahlenwert):

Ges. Anteile · Ges. Verkauf − Ges. Kauf (inkl. Kosten) − Verkaufsgebühren / Steuern = Gewinn / Verlust

Berechnung:

| Kästchen | Wert |
| ------ | ------ |
| Ges. Kauf (inkl. Kosten) | `totBuyVal + totFees` |
| Verkaufsgebühren / Steuern | `saleBrokerage + saleTaxSum` |
| Gewinn / Verlust | `totSaleVal - totBuyValWithFees - saleFees` |

Das G/V-Kästchen erhält einen farbigen Hintergrund (grün = Gewinn, rot = Verlust);
Bezeichner-Label und Wert-Label sind beide in der entsprechenden Farbe eingefärbt.

Zwei Modi des Dialogs (seit Phase 2c, 07.08.2026, um den jüngsten Verkauf
erweitert — vorher war "Edit" gleichbedeutend mit "ein Verkauf ist geladen",
jetzt zusätzlich davon abhängig, ob dieser Verkauf noch editierbar ist):

| Modus | Bedingung | Datenquelle |
| ------ | ------ | ------ |
| Gespeichert | `m_loadedSale.isValid() && !m_isLastSale` (älterer, gesperrter Verkauf) | gespeicherte `SaleBuyDetails` aus gecachtem `SaleObject` — exakte DB-Daten |
| Live-FIFO | kein Verkauf geladen ODER `m_isLastSale` (neuer Verkauf bzw. jüngster, editierbarer Verkauf) | `SaleFifoAllocator::allocate()` über `m_availableBuys` |

`m_loadedSale` (privates `SaleObject`-Member in `ViewSaleEdit`) wird in `loadSale()` gesetzt
und in `clearForm()` auf einen leeren Standardwert zurückgesetzt. `m_isLastSale` wird in
`setButtonStates()` mitgeführt (Presenter übergibt es dort ohnehin für die Feld-Sperrung).

#### Edit-Modi:

| Modus | Bedingung | Verhalten |
| ------ | ------ | ------ |
| Neu-Modus | Kein Verkauf ausgewählt | Alle Felder editierbar, Button "Hinzufügen" |
| Letzter Verkauf | `isLastSale = true` | Alle Felder editierbar, "Speichern", Entfernen aktiv |
| Älterer Verkauf | `isLastSale = false` | Nur Dokumentpfad editierbar, Button "Speichern", kein Entfernen |

Letzter-Verkauf-Erkennung:
Lexikographischer Vergleich der ISO-8601-`dateTime`-Strings in `isLatestSale()` — identisch zu
`isLatestBuy()` in `PresenterBuyEdit`.

`canRemove`-Bedingung:
`canRemove = isLastSale` — kein zusätzlicher `volumeSold`-Guard nötig (Verkäufe haben keine
verkauften Anteile die noch schützen müssten).

Verkaufs-Übersicht — Frozen-Footer-Layout:

Identisches Muster wie BuysForm. Übersicht-Tab (Index 0) dann Jahres-Tabs absteigend nach Jahr.

| Tab | Spalten |
| ------ | ------ |
| Übersicht | Jahr \| Anteile \| Auszahlung \| Gewinn/Verlust |
| Jahres-Tab | Datum \| Anteile \| Auszahlung \| Gewinn/Verlust \| Dokument |

Layout des Dialogs:

@code{.unparsed}

┌── Verkaufsdaten ──────────────────────┬── Dokumenten-Vorschau ──────────────┐
│  Datum:          [date]  [time]       │  QPdfView (MultiPage + Zoom)        │
│  Depotnummer:    [combo, aus Docs.xml]│  oder pdftoppm-Fallback             │
│  Ordernummer:    [edit]               │                                     │
│  Verk. Anteile:  [edit]   stk.        │                                     │
│  Verk. Preis:    [edit]   €           │                                     │
│  Verk. Kaufwert: [readonly] €         │                                     │
│  Gek. Kaufwert:  [readonly] € [Details]                                     │
│  Gewinn/Verlust: [readonly, farbig] € │                                     │
│  Quellsteuer:    [edit]   €           │                                     │
│  KapErtrSteuer:  [edit]   €           │                                     │
│  Solidaritätsz.: [edit]   €           │                                     │
│  Provision:      [edit]   €           │                                     │
│  Courtage:       [edit]   €           │                                     │
│  Handelsplatz:   [edit]   €           │                                     │
│  Rabatt:         [edit]   €           │                                     │
│  Ges. Gebühren:  [readonly] €         │                                     │
│  Auszahlung:     [readonly, farbig] € │                                     │
├── Dokument ─────────────────────────  │                                     │
│  Dokument: [readonly path]  [📁]     │                                     │
│  [progress 200px] [icon] [status]     │  ← statusRow fixedHeight=22         │
├─────────────────────────────────────  │                                     │
│  [Hinzufügen] [Entfernen] [Reset] [Schließen]                              │
└──────────────────────────────────────┴─────────────────────────────────────┘
┌── Verkaufs-Übersicht ──────────────────────────────────────────────────────┐
│  [Tab: Übersicht (X.XXX,XX €)] [Tab: 2024 (X.XXX,XX €)] …                │
│  (Übersicht-Tab)  Jahr ❘ Anteile ❘ Auszahlung ❘ Gewinn/Verlust             │
│  (Jahres-Tab)     Datum ❘ Anteile ❘ Auszahlung ❘ G/V ❘ Dok.                │
│  ── Separator ─────────────────────────────────────────────────────────── │
│  Gesamt: XX,XXXX stk.   X.XXX,XX €  (fett, immer sichtbar)               │
└────────────────────────────────────────────────────────────────────────────┘
@endcode

#### Integration in ViewShareEdit (SalesForm)

- `ViewShareEdit::onEditSales()` öffnet `ViewSaleEdit` direkt (analog zu `onEditBuys()`).
- `PresenterSaleEdit::dataChanged()` → `ViewShareEdit::refreshSummary()`.

#### XML-Feld-Mapping (xmlNameToViewField):

| XML-Feldname | View-Widget-Key | Pflichtfeld |
| ------ | ------ | ------ |
| `Date` | `date` | ✅ |
| `Time` | `time` | — |
| `DepotNumber` | `depotNumber` | ✅ |
| `OrderNumber` | `orderNumber` | ✅ |
| `Volume` | `volume` | ✅ |
| `Price` | `salePrice` | ✅ |
| `TaxAtSource` | `taxAtSource` | — |
| `CapitalGainsTax` | `capitalGainsTax` | — |
| `SolidarityTax` | `solidarityTax` | — |
| `Provision` | `provision` | — |
| `BrokerFee` | `brokerFee` | — |
| `TraderPlaceFee` | `traderFee` | — |
| `Reduction` | `reduction` | — |

---

### DividendForm-Details

Vollständig nach MVP-Pattern implementiert, geöffnet via Pencil-Button Dividenden in ViewShareEdit.

`IViewDividendEdit` — Interface: `loadDividend()`, `clearForm()`, `setDividendPayout()`,
`setDividendPayoutFc()`, `setTaxSum()`, `setDividendPayoutWithTaxes()`, `setYield()`,
`setForeignCurrencyEnabled()`, `setFieldOk()` / `setFieldError()`, Parse-Statuszeile,
`populateOverview()`, `showOverviewTab()`, `openPdfPreview()`, `clearPdfPreview()`,
`setButtonStates(canRemove, isEdit)`, `showError()`, `acceptAndClose()`,
`markMissingFieldsAsFailed()`, `hasMissingRequiredFields()`.

`IModelDividendEdit` — Interface: `loadDividends()`, `loadShare()`, `addDividend()`,
`updateDividend()`, `removeDividend()`, `documentExists()`, `lastError()`.

`ModelDividendEdit` — Delegiert an `DividendRepository` und `ShareRepository`.
`loadShare()` wird für die WKN/ISIN-Prüfung beim Parsen verwendet.

`PresenterDividendEdit` — Vollständige Dividenden-Logik inkl. Live-Validierung,
Parse-Pipeline und Dokument-Duplikat-Check.

`ViewDividendEdit` — Linkes Formular (Dividendendaten + Dokument + Button-Bar +
Dividenden-Übersicht) + rechte PDF-Vorschau. Feste Größe 1200 × 760 px.

#### Besonderheit: Keine Letzter-Eintrag-Beschränkung

Im Gegensatz zu BuysForm und SalesForm gilt für DividendForm:

- Jede Dividende ist jederzeit vollständig editierbar — kein `readOnlyMode`.
- Jede Dividende darf jederzeit gelöscht werden — kein "nur letzter Eintrag"-Guard.
- `setButtonStates(canRemove, isEdit)` hat nur zwei Parameter (kein `isLastDividend`).
- `PresenterDividendEdit` enthält keine `isLatestDividend()`-Methode und kein `m_isLastDividend`-Member.
- GroupBox-Titel wechselt dynamisch: "Dividende hinzufügen" im Neu-Modus, "Dividende editieren" wenn ein bestehender Eintrag geladen ist — gesteuert über `m_dividenddatenGroup->setTitle()` in `setButtonStates()`.
- `clearForm()` setzt auch abgeleitete Felder zurück — `m_payout`, `m_payoutFc`, `m_taxSum`, `m_payoutWithTaxes`, `m_yield` werden explizit auf `0` gesetzt (kein Umweg über `refreshDerivedValues`).

Begründung: Dividendenzahlungen haben keinen direkten Zusammenhang zu Käufen und Verkäufen
und beeinflussen keine `volumeSold`-Felder oder FIFO-Berechnungen.

#### Fremdwährungs-Modus:

- Alle FC-Felder (Devisenkurs, Währungsauswahl, Auszahlung FC) sind **immer sichtbar** — das Layout verschiebt sich nie.
- `QCheckBox "Fremdwährungseingabe aktivieren"` steuert nur `setEnabled()` auf Devisenkurs und Währung.
- Deaktivierte Felder erhalten `background: palette(midlight)` als visuelles Feedback.
- `setForeignCurrencyEnabled(bool)` ist direkt mit `QCheckBox::toggled` verbunden (nicht über den Presenter) damit `setEnabled`/`setStyleSheet` sofort und synchron wirken.
- `QComboBox::activated` aktualisiert das Währungssymbol in `m_rateUnit` und `m_payoutFcUnit` bei jeder Benutzerauswahl.
- `QLineEdit::editingFinished` auf `m_exchangeRatio` → `onExchangeRatioEdited()` — validiert nur wenn FC aktiviert ist; 0 oder negativ → Fehler-Icon.
- Devisenkurs und Währungsauswahl befinden sich in einem `fcRowWidget`-Container; `setEnabled` muss **nach** `addWidget()` aufgerufen werden damit Qt die Parent-Child-Hierarchie für `enabled`-Vererbung korrekt auflöst.

#### Pflichtfelder (Validierung beim Speichern):

| Feld | Widget | Prüfung |
| ------ | ------ | ------ |
| Datum der Auszahlung | `QDateEdit` | Datum nach Sentinel 2000-01-01 |
| Dividendensatz | `QLineEdit` (QDoubleValidator) | > 0 |
| Anteile am Auszahlungstag | `QLineEdit` (QDoubleValidator) | > 0 |
| Preis der Aktie am Auszahlungstag | `QLineEdit` (QDoubleValidator) | > 0, **automatisch aus `daily_values` befüllt, sonst manuell** (siehe unten) |

#### Optionale Felder:

| Feld | Widget | Bemerkung |
| ------ | ------ | ------ |
| Uhrzeit | `QTimeEdit` | Default 00:00:00 |
| Fremdwährung aktivieren | `QCheckBox` | Aktiviert/deaktiviert Devisenkurs + Währung |
| Devisenkurs | `QLineEdit` | Immer sichtbar; editierbar nur wenn FC aktiviert |
| Währungsauswahl | `QComboBox` | Immer sichtbar; wählbar nur wenn FC aktiviert; en-US/$, en-GB/£, ja-JP/¥, de-CH/CHF, ... |
| Quellsteuer | `QLineEdit` | ≥ 0 |
| Kapitalertragssteuer | `QLineEdit` | ≥ 0 |
| Solidaritätszuschlag | `QLineEdit` | ≥ 0 |
| Dokument | `QLineEdit` read-only + Browse-Button | Duplikat-Check aktienübergreifend |

#### Auto-Fill: Preis der Aktie am Auszahlungstag (ergänzt 06.07.2026, erweitert 07.07.2026)

`priceAtPayday` wird automatisch aus den gespeicherten Tageswerten (`daily_values`)
befüllt, sofern für das jeweilige Datum ein Schlusskurs vorliegt. Die Lookup-Logik
sitzt in der privaten Hilfsmethode `PresenterDividendEdit::applyDailyValuePriceAtPayday(date)`
und wird von zwei Stellen aus aufgerufen:

```
①  QDateEdit::editingFinished                    ②  Nach erfolgreichem PDF-Parsen
       ↓                                                ↓ (Date-Feld aus Parser-Ergebnis geparst)
   onDateEdited()                                  populateFromResult()
       ↓ (Datum gültig, > Sentinel 2000-01-01)          ↓ (parsedDate gültig)
       └──────────────────┬───────────────────────────────┘
                           ↓
        applyDailyValuePriceAtPayday(date)
                           ↓
        IModelDividendEdit::findClosingPriceForDate(shareGuid, date, outPrice)
                           ↓ (ModelDividendEdit delegiert an DailyValuesRepository::findByShareAndDate())
        Treffer mit closing > 0?
            Ja  → setFieldOk("priceAtPayday", closingPrice, "Aus Tageswerten übernommen (Kurs vom TT.MM.JJJJ)")
                  + refreshDerivedValues() (Dividenden-Rendite aktualisiert sich mit)
            Nein → Feld bleibt unverändert — manuelle Eingabe weiterhin nötig/möglich
```

Wichtige Designentscheidungen:
- Überschreibt einen bereits vorhandenen Wert bei jeder Datumsänderung mit Treffer —
  auch einen zuvor manuell eingegebenen. Der Nutzer kann den Wert danach jederzeit
  wieder von Hand korrigieren (kein Sperren des Felds).
- Kein Treffer → keine Änderung.
- Pfad ② (Preis-Abgleich direkt nach dem Parsen) ist notwendig, nicht nur bequem.
  Ursprünglich war angenommen, für das Default-Datum "heute" (beim frischen Öffnen
  des Dialogs) läge ohnehin noch kein Schlusskurs vor, sodass Pfad ① allein reicht.
  In der Praxis bestätigt (07.07.2026): Wenn beim Öffnen des Datei-Auswahldialogs
  ("Dokument auswählen") das Datumsfeld kurz den Fokus verliert — noch mit dem
  Default-Datum "heute" befüllt — feuert `editingFinished` und Pfad ① schlägt ggf.
  **erfolgreich** zu, weil für den aktuellen Tag durchaus schon ein Tageswert
  vorliegen kann (z. B. durch einen zuvor gelaufenen Refresh). Das Ergebnis wäre ein
  falscher, an "heute" hängender Preis, der stehen bliebe, weil das anschließende
  Setzen des tatsächlichen Dokumentdatums per `setDate()` **kein** `editingFinished`
  auslöst. Pfad ② behebt das: Nach dem Parsen wird der Abgleich zwangsläufig
  nochmal mit dem tatsächlichen Dokumentdatum ausgeführt und überschreibt einen
  eventuellen "heute"-Fehltreffer.
- Tooltip als Unterscheidungsmerkmal: `IViewDividendEdit::setFieldOk()` erhält einen
  optionalen dritten Parameter `tooltip`, damit automatisch befüllte Werte
  ("Aus Tageswerten übernommen (Kurs vom ...)") sich vom Standard-Tooltip
  ("Eingabe gültig") bei manueller Eingabe unterscheiden lassen. Leerer String → Standard-Tooltip.
- Keine Währungsumrechnung nötig: `daily_values.closing` liegt ebenso wie `rate` immer
  in EUR vor, unabhängig vom Fremdwährungs-Modus der Dividende — kein Äpfel-mit-Birnen-Vergleich
  bei der Dividenden-Rendite.

#### Abgeleitete Werte (read-only, automatisch berechnet):

| Feld | Formel |
| ------ | ------ |
| Auszahlung (€) | `rate × volume` (oder `rate × volume / exchangeRatio` bei FC) |
| Auszahlung (FC) | `rate × volume` — beide Werte stehen in **einer Zeile** nebeneinander |
| Gezahlte Steuern | `taxAtSource + capitalGainsTax + solidarityTax` |
| Auszahlung nach Steuern | `dividendPayout − taxSum` (grün/rot eingefärbt) |
| Dividenden-Rendite | `rate / priceAtPayday × 100 %` |

#### WKN/ISIN-Prüfung beim Parsen:

Alle drei Presenter (BuyEdit, SaleEdit, DividendEdit) prüfen nach dem Parsen ob
das Dokument zur aktuell geöffneten Aktie gehört. Die Prüfung erfolgt in
`populateFromResult()`:

```
parsedWkn / parsedIsin aus Parse-Ergebnis
    ↓
loadShare(m_shareGuid) → ShareObject
    ↓
wknMatch || isinMatch?
    Nein → showError("Dokument gehört nicht zu dieser Aktie") + return
    Ja  → Felder befüllen
```

Wenn weder WKN noch ISIN im Dokument gefunden wurden (`anyParsed = false`),
wird die Prüfung übersprungen — kein False-Positive für Dokumente ohne Identifier.

#### XML-Feld-Mapping (xmlNameToViewField):

| XML-Feldname | View-Widget-Key | Pflichtfeld (Parser) |
| ------ | ------ | ------ |
| `Date` | `date` | ✅ |
| `Time` | `time` | — |
| `Volume` | `volume` | ✅ |
| `DividendRate` | `rate` | ✅ |
| `TaxAtSource` | `taxAtSource` | — |
| `CapitalGainTax` | `capitalGainsTax` | — |
| `SolidarityTax` | `solidarityTax` | — |
| `ExchangeRate` | `exchangeRatio` | — |
| `Currency` | `currency` | — |

@note **PriceAtPayday-Mapping entfernt (08.07.2026):** Das frühere
`PriceAtPayday`-Mapping wurde entfernt — kein `Document Type="Dividend"` in
`Documents.xml` definierte je ein `PriceAtPayday`-Feld, der Eintrag war
totes Gewicht. Details siehe "Totes Mapping: PriceAtPayday in
xmlNameToViewField()" unter "Erledigt / Archiv". `knownXmlNames` in
`populateFromResult()` enthält den Namen weiterhin (bewusst unangetastet,
siehe dort).

#### Dividenden-Übersicht — Frozen-Footer-Layout:

Identisches Muster wie BuysForm/SalesForm. Übersicht-Tab (Index 0), dann Jahres-Tabs
absteigend nach Jahr.

| Tab | Spalten |
| ------ | ------ |
| Übersicht | Jahr \| Dividende (netto) |
| Jahres-Tab | Datum \| Dividendensatz \| Anteile \| Dividende (netto) \| Dokument |

#### Integration in ViewShareEdit (DividendForm)

- `ViewShareEdit::onEditDividends()` öffnet `ViewDividendEdit` direkt.
- `DocumentsConfig*` wird von `ViewShareEdit` an `ViewDividendEdit` weitergereicht.
- `PresenterDividendEdit::dataChanged()` → `ViewShareEdit::refreshSummary()`.

---

### BrokeragesForm-Details

Vollständig nach MVP-Pattern implementiert, geöffnet via Pencil-Button Kosten in ViewShareEdit.

`IViewBrokerageEdit` — Interface mit Lese-Accessoren für alle Eingabefelder:
`loadBrokerage()`, `clearForm()`, `setGesamtGebuehren()`, `setBrokerageReduction()`,
`setDocumentPreview()`, `openPdfPreview()`, `clearPdfPreview()`,
`populateOverview()`, `showOverviewTab()`,
`setButtonStates(canRemove, isEdit, readOnly)`, `showError()`, `acceptAndClose()`,
`markMissingFieldsAsFailed()`, `hasMissingRequiredFields()`.

`IModelBrokerageEdit` — Interface: `loadBrokerages()`, `addBrokerage()`,
`updateBrokerage()`, `updateDocument()`, `removeBrokerage()`, `documentExists()`, `lastError()`.

`ModelBrokerageEdit` — Delegiert an `BrokerageRepository`.
`documentExists()` prüft aktienübergreifend per direkter SQL-Abfrage.
`updateDocument()` ist im Interface vorhanden, wird vom Presenter aber nicht aufgerufen —
Linked Records sind vollständig read-only.

`PresenterBrokerageEdit` — Vollständige Kosten-Logik inkl. Linked-Record-Erkennung,
Live-Validierung und Dokument-Duplikat-Check. Keine Parse-Pipeline — Daten werden
manuell eingegeben.

`ViewBrokerageEdit` — Linkes Formular (Kostendaten + Dokument + Button-Bar +
Kosten-Übersicht) + rechte PDF-Vorschau. Feste Größe 1100 × 680 px.
Identische Layout-Struktur wie BuysForm/SalesForm/DividendForm: Top-Level `QHBoxLayout`,
linkes Panel (Formular + Übersicht) stretch 3, rechtes Vorschau-Panel stretch 2.

#### Besonderheit: Keine Letzter-Eintrag-Beschränkung, aber Linked-Record-Schutz

Im Gegensatz zu BuysForm und SalesForm gilt für BrokeragesForm:

- Jeder standalone Kosteneintrag ist jederzeit vollständig editierbar und löschbar —
  kein `isLastBuy`-Guard, analog zu DividendForm.
- Linked Records (Einträge mit gesetztem `buyGuid` oder `saleGuid`) sind vollständig
  read-only: kein Feld darf geändert werden, da Datum, Gebühren und Dokument aus dem
  verknüpften Kauf oder Verkauf stammen. Speichern, Entfernen und Browse-Button sind
  alle deaktiviert. Ein versehentlicher `onSave()`-Aufruf wird im Presenter abgefangen
  und zeigt eine Fehlermeldung.
- `setButtonStates(canRemove, isEdit, readOnly)` hat drei Parameter:
  `readOnly = true` deaktiviert alle Gebührenfelder, Datum/Uhrzeit, Browse-Button
  und den Speichern-Button.
- GroupBox-Titel wechselt dynamisch: "Kosten hinzufügen" im Neu-Modus,
  "Kosten editieren" wenn ein Eintrag geladen ist.

Linked-Record-Erkennung (`isLinkedRecord()`):
Prüft ob `buyGuid` oder `saleGuid` des aktuell geladenen Eintrags nicht leer sind.
Linked Records stammen aus Käufen/Verkäufen und werden dort verwaltet — im BrokeragesForm
sind sie vollständig read-only und können weder bearbeitet noch gelöscht werden.
Sie sind in der Übersicht sichtbar, damit der Nutzer einen vollständigen Überblick
über alle Kosten einer Aktie hat.

#### Pflichtfelder:

| Feld | Widget | Prüfung |
| ------ | ------ | ------ |
| Datum / Uhrzeit | `QDateEdit` + `QTimeEdit` | Datum nach Sentinel 2000-01-01 |

#### Optionale Felder (alle ≥ 0):

| Feld | Widget | Bemerkung |
| ------ | ------ | ------ |
| Provision | `QLineEdit` (QDoubleValidator) | ≥ 0 |
| Courtage | `QLineEdit` (QDoubleValidator) | ≥ 0 |
| Handelsplatzgebühr | `QLineEdit` (QDoubleValidator) | ≥ 0 |
| Ges. Gebühren | `QLineEdit` read-only | Provision + Courtage + Handelsplatz |
| Rabatt | `QLineEdit` (QDoubleValidator) | ≥ 0 |
| Netto-Kosten | `QLineEdit` read-only, farbig | Ges. Gebühren − Rabatt; grün positiv, rot negativ |
| Dokument | `QLineEdit` read-only + Browse-Button | Duplikat-Check aktienübergreifend |

#### Edit-Modi:

| Modus | Bedingung | Verhalten |
| ------ | ------ | ------ |
| Neu-Modus | Kein Eintrag ausgewählt | Alle Felder editierbar, Button "Hinzufügen" |
| Standalone-Edit | `isLinkedRecord() = false` | Alle Felder editierbar, Button "Speichern", Entfernen aktiv |
| Linked-Edit | `isLinkedRecord() = true` | Alle Felder und Buttons deaktiviert (readOnly); nur Reset und Schließen bleiben aktiv |

#### Kosten-Übersicht — Frozen-Footer-Layout:

Identisches Muster wie BuysForm/SalesForm/DividendForm. Übersicht-Tab (Index 0),
dann Jahres-Tabs absteigend nach Jahr.

| Tab | Spalten |
| ------ | ------ |
| Übersicht | Jahr \| Netto-Kosten |
| Jahres-Tab | Datum \| Typ \| Ges. Gebühren \| Rabatt \| Netto-Kosten \| Dokument |

Die **Typ-Spalte** unterscheidet die Herkunft eines Eintrags:

| Wert | Bedeutung |
| ------ | ------ |
| `Kauf` | `buyGuid` gesetzt — stammt aus einer Kauftransaktion |
| `Verkauf` | `saleGuid` gesetzt — stammt aus einer Verkaufstransaktion |
| `Sonstig` | Standalone-Eintrag, direkt im BrokeragesForm angelegt |

#### Dialog-Layout:

@code{.unparsed}
┌── Kosten hinzufügen ──────────────────────────┬── Dokumenten-Vorschau ──────┐
│  Datum / Uhrzeit:    [date]  [time]           │  QPdfView (MultiPage + Zoom)│
│  Provision:          [edit]  €                │  oder pdftoppm-Fallback     │
│  Courtage:           [edit]  €                │                             │
│  Handelsplatzgebühr: [edit]  €                │                             │
│  Ges. Gebühren:      [readonly] €             │                             │
│  Rabatt:             [edit]  €                │                             │
│  Netto-Kosten:       [readonly, farbig] €     │                             │
├── Dokument ────────────────────────────────── │                             │
│  [path readonly]  [📁]                        │                             │
├───────────────────────────────────────────────│                             │
│         [Hinzufügen] [Entfernen] [Reset] [Schließen]                       │
└───────────────────────────────────────────────┴─────────────────────────────┘
┌── Kosten-Übersicht ────────────────────────────────────────────────────────┐
│  [Tab: Übersicht (X,XX €)] [Tab: 2024 (X,XX €)] ...                      │
│  (Übersicht-Tab)  Jahr ❘ Netto-Kosten                                      │
│  (Jahres-Tab)     Datum ❘ Typ ❘ Ges. Gebühren ❘ Rabatt ❘ Netto-Kosten ❘ Dok. │
│  ── Separator ─────────────────────────────────────────────────────────── │
│  Gesamt: X,XX €  (fett, immer sichtbar)                                   │
└────────────────────────────────────────────────────────────────────────────┘
@endcode

#### Integration in ViewShareEdit (BrokeragesForm)

- `ViewShareEdit::onEditBrokerages()` öffnet `ViewBrokerageEdit` direkt.
- `PresenterBrokerageEdit::dataChanged()` → `ViewShareEdit::refreshSummary()`.
- Kein `DocumentsConfig*` nötig — BrokeragesForm hat keine Parse-Pipeline.

---

### ShareDetailsForm-Details (Depotwert-Box implementiert 09.07.2026, Aktien-Chart-Tab implementiert 12.07.2026)

Reine Anzeige-Form (kein Speichern, keine Bearbeitung), Portierung von
`FrmShareDetails` aus der C#-Referenz. Geöffnet per Doppelklick auf eine Zeile
in `m_finalValueTable` oder `m_marketValueTable` (MainWindow).

Wichtiger Hinweis zum Umfang dieser Iteration: Ein früherer Anlauf hatte
diese Form ohne Abgleich mit der C#-Referenz gebaut (Tabs für Stammdaten,
Käufe, Verkäufe, Dividenden, Brokerages) — die Referenz sieht davon nichts vor.
Die tatsächliche Struktur von `FrmShareDetails` (`ShareDetailsForm.cs` +
`TabControl.cs`/`DividendDetails.cs`/`ProfitLossDetails.cs`/
`BrokerageDetails.cs`, C#) ist:

@code{.unparsed}
FrmShareDetails
├── Tab "Aktien-Chart"                — Chart der Tagesdaten, Zeitraum-/Intervall-Auswahl
├── Tab "Komplette Depotbewertung"    — 3 Bestandsberechnungs-Boxen (nur bei Depotwert-Aufruf)
│   ODER "Komplette Marktbewertung"   — (nur bei Marktwert-Aufruf)
├── Tab "Gewinne / Verluste (X€)"     — Übersicht + Jahres-Tabs (beide Modi, Marktwert brokeragefrei)
├── Tab "Dividenden (X€)"             — Übersicht + Jahres-Tabs (nur Depotwert-Modus)
└── Tab "Kosten (X€)"                 — Übersicht + Jahres-Tabs (nur Depotwert-Modus)
@endcode

`FrmShareDetails` hat zwei Modi (`marketValueOverviewTabSelected`-Flag im
Konstruktor, je nachdem ob der Aufruf vom Depotwert- oder Marktwert-Tab in
MainWindow kam): Marktwert-Modus zeigt nur Chart + Marktbewertung; Depotwert-
Modus zeigt Chart + Depotbewertung + die drei Jahres-Tab-Bereiche.

Umfang dieser Iteration (auf Nessies Vorgabe eingegrenzt):

| Teil | Status |
| ------ | ------ |
| Chart-Tab | ✅ implementiert (12.07.2026) — eigene MVP-Triad (ChartForm/), eingebettet als Tab 1; siehe "ChartForm-Details" unten |
| Komplette Depotbewertung | ✅ implementiert (09.07.2026) |
| Komplette Marktbewertung | ✅ implementiert (10.07.2026), gegen echten Screenshot der C#-Referenz abgeglichen |
| Formel-Validierung gegen echte DB | 🟨 begonnen (10.07.2026) mit einer Ein-Aktien-Datenbank (Allianz SE, 5 Käufe/1 Verkauf/10 Dividenden/reale Historie 2016–2026) — dabei den Rabatt-Bugfix (siehe unten) sowie eine irreführende Label-Benennung gefunden und korrigiert. Berechnungslogik selbst (`completePurchase`/`completePurchaseMarket`) gegen `ShareObjectFinalValue.cs` bestätigt. Validierung der tatsächlichen App-Anzeige (Screenshot-Vergleich) steht noch aus. |
| Gewinne/Verluste, Dividenden, Kosten | ✅ implementiert (13.07.2026, fixierter Übersicht-Tab 14.07.2026, Gewinne/Verluste auf Marktwert-Modus erweitert 14.07.2026) — je ein `OverviewTabWidget` + `DocumentPreviewPanel`, siehe "OverviewTabWidget-Details" unten |

#### "Komplette Depotbewertung" — drei Bestandsberechnungs-Boxen

Im C# als mehrspaltiges WinForms-Grid mit Operator-Symbolen zwischen den
Spalten dargestellt (`Anteile × Preis = Einzahlung`, dann `+ Dividenden`,
`+ Verkäufe`, `= Summe` usw., alle in einer Zeile über mehrere Spalten). Im
Qt-Port bewusst vereinfacht zu einer vertikalen Zeilenliste pro Box (Operator |
Label | Wert), inhaltlich aber 1:1 gleiche Werte/Zeilen — siehe Chat-Verlauf
09.07.2026. Revisionswürdig, aber als Zwischenlösung akzeptiert.

Alle drei Boxen (`Gesamt-`, `Vortag-`, `Aktuelle Bestandsberechnung`) mappen
fast vollständig auf bereits vorhandene `ShareValues`-Felder (siehe
`ShareCalculator.h`). Für die Depotwert-Box mussten zwei rein additive Felder
ergänzt werden:

- `ShareValues::salePayoutFinal` — rohe Verkaufserlöse MIT Brokerage (Zeile
  "+ Verkäufe"), bislang nur als lokale Variable in `compute()` vorhanden.
- `ShareValues::saleProfitLossFinal` — realisierter Gewinn/Verlust aus
  Verkäufen MIT Brokerage (Zeile "+ Gewinn / Verlust (Verkäufe)"), Depotwert-
  Pendant zum bereits vorhandenen `saleProfitLoss` (Marktwert-Variante).

Für die Marktwert-Box (10.07.2026, gegen echten Screenshot abgeglichen —
Bild "AGIF-Allianz Glo.Eq.Insights") kam ein drittes additives Feld hinzu:

- `ShareValues::salePayoutMarket` — rohe Verkaufserlöse OHNE Brokerage,
  Marktwert-Pendant zu `salePayoutFinal`.

Wichtig: Für die Marktwert-Box `Gesamt-Bestandsberechnung` werden
bewusst **nicht** `completeCurValueMarket`/`completeProfitLossMarket`/
`completeProfitPctMarket` verwendet — laut eigenem Doc-Kommentar in
`ShareCalculator.h` mischen diese den brokeragehaltigen realisierten
Gewinn/Verlust mit ein ("Komplette Entwicklung = ... + realized P/L WITH
brokerage"), weil sie für die Portfolio-Grid-Fußzeile gedacht sind. Die
Detaildialog-Box braucht die reine, brokeragefreie Variante und berechnet sie
deshalb frisch im Presenter: `curValue + salePayoutMarket −
completePurchaseMarket`. Die Aktuelle-Box im Marktwert-Modus verwendet dagegen
`marketValue` (= `curValue + saleProfitLoss`) direkt — dieses Feld war bereits
vor dieser Iteration vorhanden und mischt keine Brokerage-Anteile ein.

Zwei Zeilen sind reine Presenter-Arithmetik über bereits vorhandene Felder
(keine Repository-Zugriffe, daher bewusst NICHT in `ShareCalculator`):

- Vortag-Box "Gewinn / Verlust" = `volume × prevDayDiff` (beide Modi)
- Aktuelle-Box "Summe" (nur Depotwert-Modus) = `curValue + totalDividend + saleProfitLossFinal`

Beide werden mit einer lokalen, in `PresenterShareDetails.cpp` duplizierten
`roundAway()`-Funktion gerundet (bewusst **nicht** `ShareCalculator::roundAway()`
aufgerufen) — sonst müsste `tst_sharedetailsform` `ShareCalculator.cpp` und
damit transitiv alle vier Repositories + `Qt6::Sql` mitkompilieren, was dem
Ziel eines DB-freien Test-Targets widerspräche. Bei Änderungen an
`ShareCalculator::roundAway()` muss die Kopie manuell synchron gehalten werden
(Kommentar im Code verweist darauf).

#### Marktwert- vs. Depotwert-Modus

`PresenterShareDetails`/`ViewShareDetails` nehmen einen `marketValueMode`-Parameter
(`bool`, Default `false`) entgegen — Pendant zum `marketValueOverviewTabSelected`-
Flag der C#-Referenz. `MainWindow::onPortfolioRowDoubleClicked()` setzt ihn
anhand der auslösenden Tabelle (`table == m_marketValueTable`).

Unterschiede zwischen den Modi:

| Zeile | Depotwert | Marktwert |
| ------ | ------ | ------ |
| Tab-Titel | "Komplette Depotbewertung" | "Komplette Marktbewertung" |
| Gesamt-/Aktuelle-Box "Dividenden" | echter Wert (`totalDividend`) | deaktiviert: Wert `"-"`, Farbe `Qt::gray` (Dividenden sind ein reines Depotwert-Konzept) |
| Vortag-Box | identisch in beiden Modi (keine Brokerage involviert) | identisch |
| Gewinne/Verluste-Tab | Werte inkl. Brokerage (`payoutBrokerageReduction()`/`profitLossBrokerageReduction()`) | seit 14.07.2026: brokeragefrei (`payout()`/`profitLoss()`), Tab existiert in beiden Modi |
| Dividenden-/Kosten-Tab | vorhanden | existiert nicht (reine Depotwert-Konzepte, siehe oben) |

Die deaktivierte Dividenden-Zeile wird über einen kleinen Presenter-Helfer
`disabledRow(operatorSymbol, label)` erzeugt (Wert `"-"`, `QColor(Qt::gray)`) —
matcht die ausgegraute Beschriftung im C#-Referenz-Screenshot.

@note **Gewinne/Verluste-Tab im Marktwert-Modus (14.07.2026, Nessies
Vorgabe):** Ursprünglich (13.07.2026) existierten alle drei neuen Tabs
(Gewinne/Verluste, Dividenden, Kosten) nur im Depotwert-Modus. Auf Nessies
Wunsch legt `ViewShareDetails::setupUi()` den Gewinne/Verluste-Tab jetzt in
beiden Modi an — Dividenden und Kosten bleiben bewusst Depotwert-only, da
beides laut C#-Referenz reine Depotwert-Konzepte sind (dieselbe Begründung,
aus der auch die Dividenden-Zeile oben deaktiviert ist). Im Marktwert-Modus
verwendet `ViewShareDetails::populateGewinneVerluste()` die brokeragefreien
`SaleObject`-Felder `payout()`/`profitLoss()` statt der Depotwert-Felder
`payoutBrokerageReduction()`/`profitLossBrokerageReduction()` — konsistent
zur "Marktwert-Tab (no brokerage, no reduction)"-Formel in
`ShareCalculator.h`. Die Spaltenüberschriften/-Labels bleiben unverändert
(kein "brokeragefrei"-Zusatz), analog dazu, dass auch der Rest der
Marktwert-Box keine solchen Zusätze verwendet. `PresenterShareDetails::
loadAndDisplay()` ruft `populateGewinneVerluste()` entsprechend in beiden
Modi auf, `populateDividenden()`/`populateKosten()` weiterhin nur im
Depotwert-Modus.

@note **Entdeckter Folgefehler (15.07.2026):** Der Regressionstest für obige
Erweiterung (`test_marketMode_gewinneVerlusteTab_usesBrokerageFreeValues` in
`tst_mainwindow.cpp`, echte DB-Daten statt Fakes) deckte einen unabhängigen,
vorbestehenden Bug in `ModelSaleEdit::addSale()` auf (fehlender
Brokerage-Vorwärts-Link) — siehe "SalesForm-Details"/`ModelSaleEdit` oben
für die Details des Fixes.

@note **Bugfix (10.07.2026, betrifft Grid UND ShareDetailsForm):** Beim
Durchgehen der Marktwert-Box fiel auf, dass `ShareCalculator` zwar Brokerage
(Provision/Broker-Gebühr/Händler-Gebühr) aus allen `...Market`-Feldern
ausschloss, `Rabatt` (`reduction`) aber weiterhin verrechnete — sowohl bei
Käufen (`purchaseValueMarket`, `completePurchaseMarket`) als auch bei
Verkäufen (`salePayoutMarket`). Da Rabatt fachlich nur eine Reduktion der
Brokerage-Kosten ist (siehe C#-Referenz: "Kosten"- und "Rabatt"-Spalte gehören
in derselben Tabelle zusammen und sind nur im Depotwert-Modus überhaupt
sichtbar), gehört er konsequent mit ausgeschlossen. Von Nessie bestätigt:
Rabatt fliegt jetzt überall aus den Marktwert-Feldern raus — das betrifft
**auch das Portfolio-Grid selbst** (`m_marketValueTable`), nicht nur diesen
Dialog, da beide dieselben `ShareValues`-Felder verwenden.

Auswirkung auf `ShareCalculator::compute()`:
- `purchaseValueMarket`: `heldBuyValue − heldReduction` → `heldBuyValue`
- `completePurchaseMarket`: `fullBuyValue − buyReduction` → `fullBuyValue`
- `salePayoutMarket`: `saleValue + sale.reduction() − taxSum()` → `saleValue − taxSum()`

`completeCurValueMarket` bleibt bei einem 100%-gehaltenen Buy algebraisch
unverändert (Rabatt kürzt sich in `completePurchaseMarket − purchaseValueMarket`
heraus), `completePurchaseMarket`/`completeProfitLossMarket` einzeln aber
schon — Details und neu durchgerechnete Werte siehe `tst_sharecalculator.cpp`.
Depotwert-Felder (`purchaseValueFinal`, `completePurchase`, `salePayoutFinal`
usw.) sind unverändert — dort gehört Rabatt weiterhin dazu.

Zusätzlich neu (beide Modi, in der C#-Referenz aber übersehen): eine graue
Statuszeile "Letzte Website- Aktualisierung: ..." **innerhalb** des Depotwert-/
Marktwert-Tabs (nicht der äußeren Dialog-Statuszeile). Gemappt auf
`ShareObject::lastPriceUpdate()` — das zweite der beiden Update-Zeitstempel-
Felder auf `ShareObject` (`lastInternetUpdate()` wird bereits für die äußere
Statuszeile verwendet). **Von Nessie bestätigt (10.07.2026):** "Letzte
Website-Aktualisierung" ist der Zeitpunkt der letzten Marktwert-/Kurs-
Aktualisierung — `lastPriceUpdate()` ist damit das richtige Feld, getrennt
von `lastInternetUpdate()` (allgemeines Internet-Update, äußere Statuszeile).

@note **Bugfix (11.07.2026):** `ShareObject::lastInternetUpdate()`/
`lastPriceUpdate()` liefern den in der DB gespeicherten ISO-8601-String roh
zurück (z. B. `"2026-07-11T00:45:00"`, bestätigt anhand der Allianz-SE-
Validierungs-DB) — anders als die Datumsfelder der Transaktionsobjekte
(`BuyObject::dateAsStr()` usw.), die bereits über `QLocale` formatieren.
Beide Zeilen in `ViewShareDetails` zeigten dadurch den rohen ISO-String statt
sich am eingestellten Länderschema zu orientieren. `PresenterShareDetails`
hat jetzt einen `formatDateTime()`-Helfer (parst via `QDateTime::fromString(...,
Qt::ISODate)`, formatiert via `QLocale().toString(dt, QLocale::ShortFormat)` —
dieselbe Konvention wie bei den Transaktionsobjekten), mit Fallback auf den
Rohwert bei nicht parsbaren Strings (Datenintegrität sichtbar statt versteckt).

Vollständiges Feld-Mapping:

| Box | Zeile | Depotwert-Feld | Marktwert-Feld |
| ------ | ------ | ------ | ------ |
| Gesamt | Anteile / Aktueller Kurswert / Aktueller Bestandswert | `volume` / `curPrice` / `curValue` | (identisch) |
| Gesamt | + Dividenden | `totalDividend` | deaktiviert ("-") |
| Gesamt | + Verkäufe | `salePayoutFinal` | `salePayoutMarket` |
| Gesamt | = Summe | `completeCurValue` | `curValue + salePayoutMarket` (Presenter) |
| Gesamt | − Alle Einzahlungen | `completePurchase` | `completePurchaseMarket` |
| Gesamt | = Gewinn / Verlust (gesamt) | `completeProfitLoss` (grün/rot) | `Summe − completePurchaseMarket` (Presenter, grün/rot) |
| Gesamt | Entwicklung | `completeProfitPct` (grün/rot) | `Gewinn/Verlust ÷ completePurchaseMarket × 100` (Presenter, grün/rot) |
| Vortag | Aktueller Kurswert / Vortages-Kurswert / Kurswert-Entw. | `curPrice` / `prevDayPrice` / `prevDayDiff` (grün/rot) | (identisch) |
| Vortag | Entwicklung | `prevDayPct` (grün/rot) | (identisch) |
| Vortag | Anteile × Kurswert-Entw. = Gewinn/Verlust | `volume × prevDayDiff` (Presenter, grün/rot) | (identisch) |
| Aktuelle | Anteile / Aktueller Kurswert / Aktueller Bestandswert | `volume` / `curPrice` / `curValue` | (identisch) |
| Aktuelle | + Dividenden | `totalDividend` | deaktiviert ("-") |
| Aktuelle | + Gewinn / Verlust (Verkäufe) | `saleProfitLossFinal` | `saleProfitLoss` |
| Aktuelle | = Summe | `curValue + totalDividend + saleProfitLossFinal` (Presenter) | `marketValue` |

@note **Label-Korrektur (10.07.2026, gegen echte Datenbank validiert):** Die
Zeile "− Alle Einzahlungen" (`completePurchase`/`completePurchaseMarket`) hieß im
C#-Original "Verkaufte Einzahlungen" — irreführend, denn der Wert ist die
Summe **aller** Käufe (verkauft + noch gehalten), nicht nur der verkauften.
Bestätigt gegen `ShareObjectFinalValue.cs`:
`BuyValueBrokerageReduction => AllBuyEntries.BuyValueBrokerageReductionTotal`,
wobei `AllBuyEntries.AddBuy(...)` bei **jedem** Kauf aufgerufen wird,
unabhängig vom Verkaufsstatus. Von Nessie bestätigt: Wert war schon korrekt,
nur das Label wurde auf "Alle Einzahlungen:" korrigiert (Qt-Port weicht hier bewusst
vom C#-Original-Wortlaut ab).

@note **Label-Korrektur (11.07.2026):** "Aktueller Preis"/"Vortages-Preis"/
"Preis-Entw." wurden fachlich präzisiert auf "Aktueller Kurswert"/
"Vortages-Kurswert"/"Kurswert-Entw." — auf Nessies Vorgabe, konsistent in
allen drei Boxen (Gesamt/Vortag/Aktuelle, beide Modi). Reine Label-Änderung,
`ShareValues`-Felder (`curPrice`, `prevDayPrice`, `prevDayDiff`) und Werte
unverändert.

Farblogik durchgängig `value >= 0.0 ? QColor("green") : QColor("red")` —
matcht `Color.Green`/`Color.Red` aus der C#-Referenz (`PerformanceValue >= 0`).

@note **Label-Korrektur (11.07.2026):** "Einzahlungen" (Zeile `= Einzahlungen:`,
`curValue` = Anteile × Aktueller Kurswert) wurde auf "Aktueller Bestandswert:"
korrigiert — "Einzahlungen" impliziert eingezahltes Geld, tatsächlich ist der
Wert aber der aktuelle Marktwert der gehaltenen Position. Nicht zu verwechseln
mit der bereits umbenannten Zeile "− Alle Einzahlungen"
(`completePurchase`/`completePurchaseMarket`, siehe oben) — die heißt
weiterhin so und ist unverändert.

`IViewShareDetails` — `setHeaderName()`, `setStatusLine()`,
`setWebsiteUpdateLine()`, `setBoxesTabTitle()`,
`populateGesamtBox()`/`populateVortagBox()`/`populateAktuelleBox()` (je
`CalculationRows`), `showError()`, `closeDialog()`. `CalculationRow` = reines
DTO (`operatorSymbol`, `label`, `value`, `color`, `emphasize`) — komplett
vorformatiert vom Presenter, View rendert nur generisch (`populateBox()`).

`IModelShareDetails` — bewusst minimal: nur `loadShare()` und
`computeShareValues()` (dünner Pass-Through zu
`ShareCalculator::compute()`). Keine `loadBuys()`/`loadSales()`/... mehr,
da die entsprechenden Tabs die bestehenden Sub-Dialog-Widgets wiederverwenden
sollen statt eigene Datenlisten zu laden. Ergänzt 30.07.2026 um
`latestDailyValueDate()` (analog `IModelChart::latestDailyValueDate()`,
dünner Pass-Through zu `DailyValuesRepository::latestDate()`) — Grundlage
für die "Aktie sollte aktualisiert werden!"-Warnzeile, siehe Note unten.

`PresenterShareDetails` — Wie bei den übrigen Presentern: bei unbekannter GUID
wird `view->showError()` + `view->closeDialog()` aufgerufen (`loadAndDisplay()`
liefert `false`) statt den Dialog leer offen zu lassen.

`ViewShareDetails` — `QDialog`, implementiert `IViewShareDetails`. Tab 1
("Aktien-Chart") bettet `ViewChart` (eigenes MVP-Triad, siehe
"ChartForm-Details" unten) direkt als Kind-Widget ein — `setupChartTab()`
konstruiert es mit der im Konstruktor gespeicherten `m_shareGuid` und
verbindet dessen `titleInfoChanged`-Signal mit `onChartTitleInfoChanged()`,
das den Fenstertitel aus Aktienname + Zeitraum/Entwicklung zusammensetzt (s.u.).
Tab 2 ("Komplette
Depotbewertung"/"Komplette Marktbewertung", Titel dynamisch per
`setBoxesTabTitle()`) enthält die graue "Letzte Website-Aktualisierung"-Zeile
sowie drei `QGroupBox`en mit je einem generischen Operator/Label/Wert-
`QGridLayout` (`createCalculationBox()`/`populateBox()`).
`hasValidShare()` gibt zurück, ob die im Konstruktor übergebene GUID
aufgelöst werden konnte — der Aufrufer darf `exec()` nur bei `true` aufrufen.

@note **Bugfix (09.07.2026):** Der Close-Button (`QDialogButtonBox::Close`)
zeigte "Close" statt "Schließen" — Qt übersetzt Standard-Button-Texte nur,
wenn Qt's eigene `qtbase_de.qm` per `QTranslator` geladen ist; das Projekt
lädt aber nur `spm_de.ts`/`spm_en.ts`. `setupUi()` setzt den Text seither
explizit (`buttonBox->button(QDialogButtonBox::Close)->setText(tr("Schließen"))`)
statt sich auf die automatische Qt-Übersetzung zu verlassen. Regressionstest:
`test_shareDetailsDialog_validShare_constructsAndShowsCloseButtonText`
(`tst_mainwindow.cpp`, siehe TESTING.md).

@note **"Aktie sollte aktualisiert werden!"-Warnzeile (ergänzt 30.07.2026,
portiert von `ShareDetailsForm_Shown()` in der C#-Referenz,
`ShareDetailsForm.cs`):** Form-weite Statusleiste unterhalb des
`QTabWidget` (nicht Teil von `ViewChart`/dem Aktien-Chart-Tab — die
C#-Referenz nutzt dafür `toolStripStatusLabelUpdate`, eine Statusleiste
auf Form-Ebene, unabhängig vom aktuell aktiven Tab). Roter Text auf
demselben grauen Balken-Look wie `m_websiteUpdateLine`
(`QPalette::Mid`-Hintergrund), standardmäßig versteckt.

Wird — wie in der C#-Referenz — nur **einmal** beim ersten Öffnen des
Dialogs ausgewertet (`PresenterShareDetails::loadAndDisplay()` →
`buildUpdateWarning()`), nicht bei jeder Chart-Zeitraum-Änderung: die
Meldung bewertet die tatsächliche Datenaktualität der Aktie, nicht den
gerade im Chart angezeigten Ausschnitt.

Die C#-Bedingung (`ShareDetailsForm_Shown()`, Zeilen 628–654) ist auf den
ersten Blick kryptisch, wurde aber vollständig durchgerechnet:

- Die verschachtelte `while`-Schleife (Wochentag-Prüfung Sonntag/Samstag/
  Montag, danach ggf. ein weiterer `AddDays(-1)`) berechnet — für alle
  sieben möglichen Wochentage einzeln durchgerechnet — in jedem Fall exakt
  **"den letzten Werktag (Mo–Fr) vor dem übergebenen Datum"**, keine
  Feiertagsprüfung. Portiert als deutlich einfachere, äquivalente Methode
  `PresenterShareDetails::previousBusinessDay(QDate)`.
- Die anschließende Bedingung enthält in der C#-Quelle einen Bug: `if
  (InternetUpdateOption == MarketPrice && InternetUpdateOption == None ||
  ...)` — ein einzelner Enum-Wert kann nie gleichzeitig `MarketPrice` UND
  `None` sein, dieser Teil ist toter Code. Von Nessie bestätigt (30.07.2026):
  gemeint war ein `||`. Fachliche Absicht: **keine Warnung, wenn für die
  Aktie ohnehin keine Tageswerte abgerufen werden** (`ShareUpdateType::
  MarketPrice` oder `::None`) — das ist eine bewusste Einstellung, kein
  Datenproblem, und wird deshalb bewusst NICHT als Fehler angezeigt.
  Andernfalls (`DailyValues`/`Both`): Warnung, wenn entweder gar keine
  Tageswerte vorhanden sind, oder der neueste vorhandene Tageswert älter
  als der letzte Werktag vor heute ist. Portiert als
  `PresenterShareDetails::needsUpdateWarning(ShareUpdateType,
  latestDataDate, today)`.

Beide Methoden sind bewusst `public static` (statt `private`, wie
`PresenterChart::computeRangeStart()`), damit `tst_sharedetailsform.cpp`
sie direkt mit festen Datums-/Enum-Kombinationen testen kann, ohne von der
echten Systemzeit (`QDate::currentDate()`) abzuhängen — dieselbe
Konvention wie bei `XmlPortfolioParser::normalizeWebSiteUrl()` (öffentliche,
pur-statische, direkt getestete Utility-Methode). `buildUpdateWarning()`
selbst ruft `needsUpdateWarning()` mit dem echten `QDate::currentDate()`
auf; die Warntext-Formatierung ("Aktie sollte aktualisiert werden! Daten
sind evtl. nicht auf dem aktuellen Stand.") liegt vollständig im Presenter,
`IViewShareDetails::setUpdateWarning(text)` (leerer String versteckt die
Zeile) layoutet nur — dieselbe Konvention wie `setStatusLine()`/
`setWebsiteUpdateLine()`.

#### Integration in MainWindow

- Doppelklick (`itemDoubleClicked`) auf eine Zeile in `m_finalValueTable`
  **oder** `m_marketValueTable` öffnet `ViewShareDetails` für die GUID aus
  Spalte 0 (`Qt::UserRole`).
- `MainWindow::onPortfolioRowDoubleClicked(QTableWidgetItem*)` liest die GUID,
  bestimmt `marketValueMode` anhand `table == m_marketValueTable`, konstruiert
  `ViewShareDetails`, prüft `hasValidShare()` und ruft nur bei Erfolg `exec()`
  auf.

---

### ChartForm-Details (implementiert 12.07.2026)

Eigene MVP-Triad unter `forms/ChartForm/` (`IViewChart`/`IModelChart`/
`ModelChart`/`PresenterChart`/`ViewChart`), portiert vom Chart-Tab der
C#-Referenz (`FrmShareDetails`, Tab "Aktien-Chart"). Anders als die übrigen
`*Edit`-Dialoge ist `ViewChart` ein `QWidget`, kein `QDialog` — es hat kein
eigenständiges Lebenszyklus-Konzept, sondern wird von `ViewShareDetails::
setupChartTab()` direkt als Tab 1 des dortigen `QTabWidget` eingebettet
(gleiche Beziehung wie in der C#-Referenz zwischen `FrmShareDetails` und
ihrem Chart-Tab).

@code{.unparsed}
┌── Aktien-Chart-Tab ────────────────────────────────────┬── Legende ─────┐
│                                                         │  ● Schluss-... │
│              QChartView (Qt Charts)                    │  ● Letzter...  │
│                                                         ├── Selektion: ──┤
│                                                         │  ☑ Schluss-Kurs│
│                                                         │  ☐ Eröffnungs-…│
│                                                         │  ☐ Höchstwert  │
│                                                         │  ☐ Tiefstwert  │
│                                                         │  ☐ Anteile     │
│                                                         │  Start-Datum:  │
│                                                         │  Interval:     │
│                                                         │  Anzahl:       │
└─────────────────────────────────────────────────────────┴────────────────┘
@endcode

"Start-Datum" ist das Ende des Zeitraums, nicht der Anfang — matcht die
C#-Referenz exakt (Screenshot 12.07.2026: Start-Datum=10.7.2026,
Interval=Month, Anzahl=1 → Zeitraum 10.06.2026–10.07.2026). Der Anfang wird
in `PresenterChart::computeRangeStart()` rückwärts aus
Start-Datum − (Anzahl × Interval-Einheit) berechnet (Tag/Woche/Monat/Jahr).
Anders als der Name vermuten lässt, steuern Interval/Anzahl also nur die
Größe des angezeigten Fensters, keine Aggregation der Datenpunkte — geplottet
werden immer die rohen Tageswerte aus `daily_values` im berechneten Fenster.

Sechs Selektions-Checkboxen (`SeriesKind`: `ClosingPrice`/`OpeningPrice`/
`High`/`Low`/`HeldVolume`/`TradedVolume`) — Default nur `ClosingPrice` aktiv,
wie im C#-Referenz-Screenshot. Jede Serie trägt ein `ChartAxis`-Feld
(`Price`/`Volume`), das bestimmt, an welcher der zwei unabhängigen Y-Achsen
sie hängt:

- `ClosingPrice`/`OpeningPrice`/`High`/`Low` → gemeinsame Preis-Achse links
  (`QValueAxis`, "Preis (€)").
- `HeldVolume` ("Anteile") → Volumen-Achse rechts, Titel "Anteile" — im
  Portfolio gehaltener Bestand, kumuliert aus Käufen/Verkäufen bis zum
  jeweiligen Datum, berechnet in `ModelChart::heldVolumeSeries()` per Sweep
  über sortierte Buy-/SaleRepository-Listen.
- `TradedVolume` ("Gehandelte Anteile", ergänzt 12.07.2026 auf Nessies
  Vorgabe — die C#-Referenz zeigt hier das an der Börse gehandelte
  Tagesvolumen, nicht den Depotbestand) → dieselbe Volumen-Achse rechts,
  Titel "Gehandelte Anteile". Liest direkt `DailyValuesObject::volume()`
  (Spalte `daily_values.volume`), kein eigener Model-Aufruf nötig, da dieser
  Wert schon in den ohnehin geladenen Tageswerten steckt.

@note **Geschichte der Achsen-Lösung (12.07.2026, drei Anläufe):** Erste
Version: `HeldVolume` und `TradedVolume` teilten sich eine gemeinsame
Stück-Achse. Nach Nessies visueller Prüfung ("das sieht nicht schön aus")
zunächst auf eine dritte, eigene Achse pro Serie umgestellt (Depotbestand
meist zweistellig, Börsenvolumen oft fünf-/sechsstellig — zu unterschiedliche
Größenordnungen für eine gemeinsame Achse). Auch das überzeugte optisch noch
nicht. **Finale Lösung:** Statt weiter an der Achsendarstellung zu feilen,
sind die beiden Checkboxen `HeldVolume` und `TradedVolume` seither in
`ViewChart::setupSelektionBox()` gegenseitig exklusiv — sobald eine der
beiden angehakt wird, deaktiviert (`QCheckBox::setDisabled()`) sie die
andere und setzt einen erklärenden Tooltip ("Anteile und Gehandelte Anteile
können nicht gleichzeitig angezeigt werden."), statt sie zu verstecken (die
Selektionsbox soll beim Umschalten nicht in der Höhe springen). Da es damit
nie beide Serien gleichzeitig geben kann, ist die dritte Achse überflüssig —
`ChartAxis` ist wieder auf `Price`/`Volume` reduziert, und beide
Volumen-Serien teilen sich `m_yAxisVolume`, dessen Titeltext
`ViewChart::rebuildAxes()` dynamisch aus der tatsächlich vorhandenen Serie
setzt ("Anteile" bzw. "Gehandelte Anteile"). Die Exklusivität ist bewusst
reine View-Ebene: `PresenterChart::axisForKind()` kennt nur noch
`ChartAxis::Volume` für beide `SeriesKind`-Werte, der Presenter selbst weiß
nichts von der Checkbox-Exklusivität — er fragt über `isSeriesSelected()`
ohnehin nur ab, was die View tatsächlich als angehakt meldet.

Alle Achsen werden bei jedem Refresh komplett neu aufgebaut
(`ViewChart::rebuildAxes()`) statt nur die Range anzupassen, da sich die
Menge der sichtbaren Serien jederzeit ändern kann.

"Legende"-Box statt Qt-Charts-eigener Legende: `m_chart->legend()->hide()`
— die rechte Box wird stattdessen manuell aus `PresenterChart`-formatierten
`LegendEntry`-Zeilen aufgebaut (Farbquadrat + fett Titel + Min/Max-Zeile),
da sie mehr zeigen muss als reine Serien-Namen: "Letzter Kauf:"/"Letzter
Verkauf:" (aus `ModelChart::latestBuy()`/`latestSale()` — jeweils der letzte
Eintrag der nach Datum aufsteigend sortierten `BuyRepository`/
`SaleRepository`-Listen) mit der Entwicklung relativ zum höchsten Schluss-Kurs
im aktuell angezeigten Zeitraum. Diese beiden Referenzzeilen erscheinen nur,
wenn für die Aktie tatsächlich Käufe/Verkäufe existieren.

Fenstertitel: `PresenterChart::refresh()` baut die Zeile "Zeitraum: ... -
... / Entwicklung: X€ (Y %)" (erster/letzter Schluss-Kurs im Fenster) und
gibt sie über `IViewChart::setRangeInfo()` an die View. `ViewChart` emittiert
das unverändert als eigenes Qt-Signal `titleInfoChanged`, das
`ViewShareDetails::onChartTitleInfoChanged()` mit dem gespeicherten
Aktiennamen zum vollständigen C#-Referenz-Fenstertitel kombiniert. Gibt es
für die Aktie gar keine Tageswerte, bleibt `infoText` leer und der Titel
fällt auf den reinen Aktiennamen zurück — hält
`test_shareDetailsDialog_validShare_constructsAndShowsCloseButtonText`
unverändert grün.

Qt6 QComboBox-Interaktionssignal: Die Interval-ComboBox verbindet
`activated(int)`, nicht `currentIndexChanged(int)` — nur echte
Nutzerauswahl soll `PresenterChart::onControlsChanged()` auslösen (gleiches
Prinzip wie an anderer Stelle im Projekt bereits etabliert, siehe
"Key learnings").

@note **Bugfix (12.07.2026):** `clearLegendLayout()` hat die alten
Zeilen-Widgets vor jedem `setLegendEntries()`-Aufruf bisher per
`deleteLater()` entfernt. Da `deleteLater()` erst beim nächsten
Event-Loop-Durchlauf löscht, blieb das alte (schon aus dem Layout entfernte)
Widget bis dahin an seiner eingefrorenen Position sichtbar, während die neuen
Zeilen direkt im selben Aufruf darüber gerendert wurden — sichtbar als
überlappende "Letzter Kauf"/"Letzter Verkauf"-Zeilen, verstärkt durch jede
zusätzlich aktivierte Selektions-Checkbox (Nessies Rückmeldung: "je mehr ich
einblende, umso mehr wird rechts die Ausgabe gestaucht"). Behoben durch
sofortiges `delete` statt `deleteLater()` — exakt dieselbe Konvention wie
`ViewShareDetails::populateBox()`.

@note **Bugfix (12.07.2026, zweiter Anlauf):** Der Fix oben behob die
überlappenden "Letzter Kauf"/"Letzter Verkauf"-Zeilen nicht vollständig —
Nessies nächster Screenshot zeigte weiterhin überlappenden Text zwischen
`line1` und `line2` innerhalb dieser beiden Einträge, unabhängig von
`deleteLater()` vs. `delete`. Statt weiter an der verschachtelten
Konstruktion (QHBoxLayout → eigenes QVBoxLayout → drei einzelne QLabels pro
Zeile) zu debuggen, wurde die Zeilendarstellung strukturell robuster gebaut:
**ein einziges** `QLabel` pro Legende-Eintrag mit Rich-Text
(`<b>Titel</b><br>Zeile1<br>Zeile2`, `setWordWrap(true)`) statt drei
verschachtelter Widgets in einem eigenen Sub-Layout — damit gibt es pro
Zeile nur noch eine einzige Widget-Geometrie zu berechnen.

@note **Bugfix (12.07.2026, dritter und finaler Anlauf):** Auch das
Rich-Text-Label löste das Problem nicht vollständig — Nessies dritter
Screenshot zeigte die zweite Zeile ("422,40€ - 198,36€ = 224,04€
(112,95 %)") bei "Letzter Kauf"/"Letzter Verkauf" komplett fehlend statt
überlappend. Die eigentliche Ursache lag eine Ebene höher: Der rechte
Bereich (Legende + Selektion) wurde bisher direkt als `QWidget` mit fester
`setMaximumWidth(280)` ins `mainLayout` gehängt. Reichte die Dialoghöhe bei
vielen aktiven Serien (bis zu 6 Checkboxen + 2 Referenzzeilen in der
Legende) nicht aus, hat Qt die Labels unter ihre benötigte Höhe gequetscht —
je nach Konstellation als überlappender Text (erster Screenshot) oder als
abgeschnittene letzte Zeile (dieser Screenshot), aber in beiden Fällen
dieselbe Ursache. Behoben durch eine `QScrollArea`
(`setWidgetResizable(true)`, Scrollbar horizontal aus) um den gesamten
rechten Bereich — der Inhalt bekommt jetzt immer seine volle benötigte Höhe
und scrollt bei Bedarf, statt gestaucht zu werden. Die ersten beiden Fixes
(sofortiges `delete`, Rich-Text-Label) bleiben trotzdem sinnvoll und wurden
nicht zurückgebaut.

@note **Dialog verbreitert (12.07.2026):** Auf Nessies Vorgabe zusätzlich der
naheliegendere Weg genommen, statt sich allein auf die `QScrollArea` zu
verlassen — `ViewShareDetails` ist jetzt von 900×600/1100×700 auf
1050×600 (Minimum) / 1400×750 (Startgröße) verbreitert, der rechte Bereich
in `ViewChart::setupUi()` von 280/300px auf 400/420px. Damit passen lange
Legende-Zeilen wie "422,40€ - 198,36€ = 224,04€ (112,95 %)" ohne
Zeilenumbruch, und Scrollen sollte im Normalfall gar nicht mehr nötig sein —
die `QScrollArea` bleibt trotzdem als Absicherung bestehen (z. B. für sehr
kleine Bildschirme oder falls künftig weitere Legende-Zeilen dazukommen).

@note **Nochmals verbreitert (12.07.2026, zweite Anpassung):** Auf Nessies
Vorgabe ("nach Möglichkeit alle Zeilen einzeilig") weiter erhöht —
`ViewShareDetails` jetzt 1150×600 (Minimum) / 1550×780 (Startgröße), rechter
Bereich in `ViewChart::setupUi()` von 400/420px auf 500/520px.

@note **Bugfix (12.07.2026):** Die Verbreiterung des rechten Bereichs kam
zunächst nicht an — `rightScroll` hatte nur `setMaximumWidth(520)`, aber
keine Mindestbreite. Ohne Stretch-Faktor im umgebenden `QHBoxLayout` nimmt
sich ein Widget nur so viel Platz, wie sein `sizeHint()` verlangt; die
zusätzliche Dialogbreite floss komplett an den Chart (`m_stack`, Stretch-
Faktor 1), der rechte Bereich blieb bei seiner alten, schmalen
sizeHint-Breite hängen (Nessies Rückmeldung: "Dialog ist breiter, aber nicht
die Legende"). Behoben durch `setFixedWidth(520)` statt
`setMaximumWidth(520)`.

@note **Zu breit, wieder reduziert (12.07.2026):** 520px war deutlich zu
breit — Nessies Screenshot zeigte viel ungenutzten Leerraum rechts neben den
Legende-Zeilen. Reduziert auf `setFixedWidth(380)` (360px Inhaltsbreite),
ein moderateres Maß, das die längste Zeile weiterhin einzeilig zeigt. Die
Dialoggröße selbst (1150×600 / 1550×780) blieb unverändert — der jetzt
größere Chart-Anteil war nicht Teil der Rückmeldung.

Bugfix (12.07.2026): Die Serienfarbe (`ChartSeriesData::color`, in
`PresenterChart` vergeben — Schluss-Kurs Schwarz, Kauf/Verkauf-Referenzlinien
Blau/Rot usw.) wurde bisher **vor** `m_chart->addSeries(line)` gesetzt.
`QChart` wendet sein Theme aber beim Hinzufügen einer Serie an und
überschreibt dabei eine vorher gesetzte Farbe wieder — Schluss-Kurs erschien
dadurch blau (erste Theme-Farbe) statt schwarz, obwohl die "Legende"-Box
(die unabhängig vom Chart-Theme aus `PresenterChart`-Daten gerendert wird)
korrekt Schwarz zeigte. Nessie ist das an der Diskrepanz Legende ↔ Graph
aufgefallen. Behoben durch `line->setColor(s.color)` **nach**
`m_chart->addSeries(line)` in `ViewChart::setChartData()`.

Hover-Tooltip (ergänzt 12.07.2026): Portiert vom C#-Referenz-Verhalten
("Maus über den Graphen bewegen zeigt Datum + Wert als Tooltip"). Jede
`QLineSeries` wird in `ViewChart::setChartData()` einzeln mit
`onSeriesHovered()` verbunden (`QLineSeries::hovered(QPointF, bool)`) — Qt
Charts hat kein chart-weites Hover-Signal, das zusätzlich verrät, welche
Serie getroffen wurde, daher ein Connect pro Serie statt eines gemeinsamen.
`point.x()` ist `msecsSinceEpoch` (gleiche Kodierung wie beim Befüllen der
Serie), `point.y()` der Wert am nächstgelegenen Datenpunkt — Qt Charts liefert
hier immer den nächsten tatsächlichen Datenpunkt, keine interpolierte
Mausposition. Preis-Serien werden mit "€" und 2 Nachkommastellen formatiert,
die beiden Stück-Serien (`HeldVolume`/`TradedVolume`) mit 4 Nachkommastellen
(bis 02.08.2026 ohne Nachkommastellen, siehe Bugfix-Hinweis unten) —
`QToolTip::showText(QCursor::pos(), ...)` bei `state == true`,
`QToolTip::hideText()` bei `state == false` (Maus verlässt die Linie).

Vertikale Kauf-/Verkauf-Markerlinien (ergänzt 12.07.2026): Portiert vom
C#-Referenz-Verhalten — jeder Kauf/Verkauf, dessen Datum in den aktuell
angezeigten Zeitraum fällt, bekommt eine gestrichelte vertikale Linie über
die volle Höhe der Preis-Achse. Farbcodierung identisch zu den
"Letzter Kauf"/"Letzter Verkauf"-Swatches in der Legende: der global letzte
Kauf ist Blau, ältere im Zeitraum liegende Käufe Türkis (`QColor(0, 170,
170)`); der global letzte Verkauf ist Rot, ältere Verkäufe Orange
(`QColor(255, 140, 0)`, bewusst dunkler/roter als das Tiefstwert-Orange, um
Verwechslungen zu vermeiden). "Global letzter Kauf/Verkauf" ist dieselbe
Definition wie bei den Legende-Referenzzeilen (`IModelChart::latestBuy()`/
`latestSale()`) — fällt der global letzte Kauf/Verkauf außerhalb des
angezeigten Zeitraums, erscheinen im Zeitraum liegende ältere Käufe/Verkäufe
trotzdem, nur eben ohne blaue/rote Linie.

- `IModelChart::buyDatesInRange()`/`saleDatesInRange()` liefern alle
  Kauf-/Verkaufsdaten im Bereich `[rangeStart, rangeEnd]`, `ModelChart`
  filtert dafür `BuyRepository::findByShare()`/`SaleRepository::findByShare()`
  clientseitig (keine neue Repository-Methode nötig).
- `PresenterChart::refresh()` baut daraus `QList<ChartReferenceLine>` und
  reicht sie über die neue `IViewChart::setReferenceLines()` an die View.
- `ViewChart::setReferenceLines()` zeichnet jede Linie als eigene
  `QLineSeries` mit zwei Punkten (`(x, yMin)`–`(x, yMax)` der aktuellen
  `m_yAxisPrice`-Range, `Qt::DashLine`, `setPointsVisible(false)`) — ohne
  Preis-Achse (nur "Anteile"/"Gehandelte Anteile" selektiert) werden keine
  Linien gezeichnet, da es keine sinnvolle Y-Referenz gäbe.
- Dieselbe Farb-nach-`addSeries()`-Reihenfolge wie beim Schluss-Kurs-Bugfix
  oben (`m_chart->addSeries(series)` **vor** `series->setPen(...)`).
- Eigene Widget-Verwaltung (`m_referenceLineSeries`) getrennt von den
  Daten-Serien, damit `setReferenceLines()` gezielt nur die Markerlinien
  austauschen kann. Da `setChartData()` aber `m_chart->removeAllSeries()`
  aufruft (löscht *alle* Serien inkl. der Markerlinien), leert
  `setChartData()` die Liste zusätzlich ohne erneutes `delete` — sonst würde
  `setReferenceLines()` beim nächsten Aufruf auf bereits freigegebenen
  Speicher zeigen. `setReferenceLines()` selbst prüft defensiv
  `m_chart->series().contains(s)`, bevor es entfernt/löscht, für den Fall
  eines Aufrufs ohne vorheriges `setChartData()`.

Hover-Tooltip für die Markerlinien (ergänzt 12.07.2026, zweiter Anlauf):
Auf Nessies Vorgabe nachgezogen — jede Kauf-/Verkauf-Markerlinie bekommt
denselben Hover-Mechanismus wie die Daten-Serien, über einen eigenen Handler
`ViewChart::onReferenceLineHovered()` statt `onSeriesHovered()`, da
Markerlinien kein `SeriesKind` haben und Datum/Preis/Stückzahl schon direkt
in der `ChartReferenceLine` stecken (keine Rückrechnung aus den
Achsen-Koordinaten wie bei den Daten-Serien nötig). Tooltip-Text:
"{Kauf/Verkauf}\n{Datum}: {Preis}€\n{Stückzahl} Stk.". Dafür musste
`ChartReferenceInfo` um ein `volume`-Feld erweitert werden (nur von
`buysInRange()`/`salesInRange()` befüllt, `latestBuy()`/`latestSale()` lassen
es bei 0.0 — für die Legende weiterhin nicht gebraucht), und
`IModelChart::buyDatesInRange()`/`saleDatesInRange()` (reine Datumslisten)
wurden durch `buysInRange()`/`salesInRange()` (liefern `ChartReferenceInfo`
mit Datum, Preis **und** Stückzahl) ersetzt.
`ChartReferenceLine` trägt entsprechend zusätzlich `kind`
(`ChartReferenceLineKind::Buy`/`Sale`, fürs Tooltip-Label), `price` und
`volume` — für das reine Zeichnen der Linie selbst weiterhin nur `date` und
`color` relevant, die Linie geht immer über die volle Preis-Achsen-Höhe.

@note **Bugfix Stückzahl-Rundung im Tooltip (ergänzt 02.08.2026, Nessies
Rückmeldung anhand eines Screenshots — "1 Stk." statt der tatsächlichen
Bruchstückzahl):** Sowohl `onReferenceLineHovered()` (Kauf-/Verkauf-
Markerlinien-Tooltip) als auch `onSeriesHovered()` für die Stück-Serien
(`HeldVolume`/`TradedVolume`) formatierten die Stückzahl mit
`QLocale().toString(value, 'f', 0)` — eine bewusste Vereinfachung der ersten
Iteration (siehe oben), die aber Käufe/Verkäufe mit Nachkommastellen (im
restlichen Projekt durchgängig auf 4 Nachkommastellen normiert, siehe
`QDoubleValidator`-Eingabefelder und `formatVolume()` in den Kauf-/Verkaufs-/
Dividenden-Formularen sowie `ViewShareEdit::setCurrentVolume()`) auf ganze
Stück rundete. Beide Stellen formatieren die Stückzahl jetzt konsistent mit
`'f', 4` statt `'f', 0`. Die Min/Max-Zahlenformatierung in der Legende-Box
(`PresenterChart`, weiterhin 0 Nachkommastellen für Anteile-Min/Max) ist
davon nicht betroffen — das bleibt eine separate, bewusst offene
Vereinfachung.

Da `tst_chartform.cpp` bewusst nur `PresenterChart` über ein Fake-View/
Fake-Model-Paar testet (kein echtes `ViewChart`/`QChartView`, siehe oben),
kann die Tooltip-Formatierung dort nicht abgedeckt werden. Stattdessen sind
`onSeriesHovered()` und `onReferenceLineHovered()` seit diesem Bugfix als
`private slots:` deklariert (`ViewChart.h`) — eine reine Testbarkeits-
Maßnahme ohne Verhaltensänderung (die Verbindung selbst läuft weiterhin über
eine Lambda in `setChartData()`/`setReferenceLines()`). Dadurch kann
`tst_mainwindow.cpp` beide Handler direkt per `QMetaObject::invokeMethod()`
aufrufen (gleiches Muster wie `selectShareRow`/`onRefreshShare`) und den
resultierenden `QToolTip::text()` prüfen, ohne ein reales Maus-Hover über
die im headless Testlauf nicht verlässlich vermessbare Chart-Zeichenfläche
zu simulieren. Regressionstests:
`test_onReferenceLineHovered_fractionalVolume_showsFourDecimals`,
`test_onSeriesHovered_heldVolumeSeries_fractionalValue_showsFourDecimals`
(beide `tst_mainwindow.cpp`, über eine direkt konstruierte `ChartPopup`-
Instanz).

Bewusste Vereinfachungen dieser ersten Iteration (auf Wunsch bei
Bedarf später verfeinerbar):
- Zahlenformatierung in der Legende durchgängig mit 2 (Preis/Anteile) bzw.
  0 Nachkommastellen (Anteile-Min/Max), nicht exakt wie im C#-Screenshot
  (dort teils 1 Nachkommastelle).
- Legende-Titeltext für "Letzter Kauf"/"Letzter Verkauf" ist in der
  Swatch-Farbe (Blau/Rot) statt wie im Referenz-Screenshot grün.
- Keine eigenen Legende-Einträge für "ältere Käufe"/"ältere Verkäufe"
  (Türkis/Orange) — nur die Linien selbst im Chart, die Legende zeigt
  weiterhin nur den jeweils letzten Kauf/Verkauf.

Tests (`tst_chartform`): Fake-View/Fake-Model-Paar (analog
`tst_sharedetailsform`) — kein `QWidget`, keine Qt-Charts-Instanziierung,
keine Datenbank. Deckt ab: leerer/gefüllter Initialzustand, Default-Selektion
(nur Schluss-Kurs), Zeitraum-Berechnung für Tag-/Monat-Intervall,
Anteile-Serie inkl. eigener Achse, gehandelte Anteile (Börsenvolumen)
inkl. eigener dritter Achse (unterschiedlich von Anteile), "keine Serie ausgewählt"-Leerzustand,
Min/Max- sowie Letzter-Kauf/Verkauf-Legendenzeilen (inkl. Fehlen bei
fehlenden Käufen/Verkäufen), und dass `onControlsChanged()` vor dem ersten
`loadAndDisplay()` keinen Effekt hat.

Mausrad-Steuerung der "Anzahl" (ergänzt 12.07.2026 auf Nessies Vorgabe,
portiert vom C#-Referenz-Verhalten): In der C#-Referenz lässt sich der
Zeitraum sowohl über das "Anzahl"-Feld selbst als auch per Mausrad direkt im
Chart ändern. Beide Wege sind über `ViewChart::eventFilter()` auf dieselbe
Logik zurückgeführt:

- `m_countSpin`: `QAbstractSpinBox::wheelEvent()` ignoriert Mausrad-
  Events ohne Fokus — der Event-Filter fängt sie stattdessen direkt ab, damit
  Scrollen auch ohne vorherigen Klick funktioniert.
- `m_chartView->viewport()`, nicht `m_chartView` selbst: `QChartView`
  (als `QGraphicsView`-Ableitung) leitet Mausrad-Events intern an seinen
  Viewport weiter — ein Filter direkt auf dem `QChartView`-Objekt würde sie
  nie sehen. Der Viewport deckt exakt die Zeichenfläche ab (nicht Legende/
  Selektion daneben), erfüllt also von selbst Nessies Vorgabe "nur über der
  Zeichenfläche".

`ViewChart::applyWheelStep()` rechnet `QWheelEvent::angleDelta()` mit der
gleichen Formel wie Qt intern (`angleDelta().y() / 8 / 15`) in "Rasten" um
und ruft `m_countSpin->stepBy(numSteps)` — Rad nach oben = Anzahl erhöhen.
Das löst automatisch die bereits bestehende `valueChanged()`-Verbindung zu
`m_presenter.onControlsChanged()` aus, ein separater Refresh-Aufruf war nicht
nötig. Regressionstest: `test_chartWheel_overCountSpinAndChartView_
changesIntervalCountAndRefreshes` (`tst_mainwindow.cpp`, siehe TESTING.md).

Obergrenze für "Anzahl" (ergänzt 12.07.2026 auf Nessies Vorgabe): Ohne
Begrenzung ließ sich "Anzahl" beliebig weit über den Punkt hinaus erhöhen, an
dem der älteste vorhandene Tageswert bereits im Fenster lag — jede weitere
Vergrößerung zeigte exakt dieselben Daten, ohne dass das für den Nutzer
erkennbar war. Die Begrenzung sitzt komplett im `PresenterChart` (Business-
Logik), nicht im `ViewChart` — reine MVP-Trennung, die View bleibt dumm.

- `DailyValuesRepository::earliestDate()` (Gegenstück zu `latestDate()`,
  `MIN(date)`) → `IModelChart::earliestDailyValueDate()` →
  `ModelChart::earliestDailyValueDate()` reichen das durch.
- `PresenterChart::computeMaxIntervalCount(rangeEnd, unit, earliestDate)`
  berechnet bei jedem `refresh()` die kleinste "Anzahl", für die das Fenster
  den ältesten Wert erstmals vollständig einschließt (`rangeStart(count) <=
  earliestDate`). Kein geschlossener Ausdruck für Monat/Jahr möglich
  (unterschiedliche Monatslängen — `addMonths()` clamped den Tag, z. B. 31.01.
  minus 1 Monat = 28./29.02.), daher eine einfache Schleife über
  `computeRangeStart()` statt einer Formel — robust für alle vier
  `IntervalUnit`-Werte, ohne Sonderfälle. Die Schleife terminiert dabei immer
  von selbst (`computeRangeStart()` bewegt sich bei steigendem `count`
  monoton rückwärts), eine Obergrenze dient nur der Absicherung, nicht der
  Korrektheit.

  @note **Bugfix 12.07.2026 (Nessies Rückmeldung: "nicht alle Werte werden
  angezeigt, wenn ich den Zeitraum größer mache"):** Die Schleife prüfte
  ursprünglich `rangeStart(count + 1)` statt `rangeStart(count)` — ein
  Off-by-one. Bei Intervallen, deren Schrittweite den ältesten Wert nicht
  exakt trifft (v. a. Interval=Monat/Jahr, wo `addMonths()`/`addYears()`
  selten exakt auf das Datum des ältesten Tageswerts fällt), blieb "Anzahl"
  dadurch systematisch einen Schritt zu klein — genau das Fenster, das den
  ältesten Wert korrekt eingeschlossen hätte, war über die Spinbox nie
  erreichbar, die entsprechenden Datenpunkte blieben dauerhaft unsichtbar.
  Bei Interval=Tag fiel der Fehler nicht auf: dort verschiebt jede Stufe
  exakt einen Tag, sodass die Fenstergrenze fast immer exakt auf einen
  vorhandenen Tageswert trifft und der Off-by-one keine sichtbare Wirkung
  hatte (Zufall der bisherigen Testdaten, nicht Beweis der Korrektheit).

  @note **Zweiter Bugfix 12.07.2026 (Nessies Rückmeldung: "beim Intervall
  Tag ist maximal 999 möglich"):** Die Schleife war ursprünglich zusätzlich
  auf eine feste Konstante `kIntervalCountCeiling = 999` gedeckelt, gedacht
  als reine Sicherheitsbremse gegen eine theoretisch endlose Schleife (siehe
  oben: die Schleife terminiert an sich immer von selbst). Für
  Interval=Woche/Monat/Jahr war das großzügig genug (999 Jahre/Monate/Wochen
  sind absurd viel Historie), für Interval=Tag aber eine echte, spürbare
  Grenze: 999 Tage sind nur ~2,7 Jahre, deutlich weniger als real vorhandene
  Kurshistorien (das eigene Allianz-SE-Referenzportfolio allein umfasst
  bereits 2016–2026, ~3843 Tage). Ersetzt durch `std::min(earliestDate.
  daysTo(rangeEnd), kAbsoluteSafetyCeiling)`: die tatsächliche Tagesspanne
  zur ältesten Kurshistorie ist für Interval=Tag die exakt richtige Grenze,
  für Woche/Monat/Jahr automatisch großzügig genug (jede dortige Stufe ist
  mindestens so groß wie ein Tag) — skaliert von selbst mit der tatsächlich
  vorhandenen Historie, ohne erneute Magic Number. `kAbsoluteSafetyCeiling =
  1000000` bleibt zusätzlich als reine Notbremse gegen korrupte Datumsdaten
  (z. B. ein kaputtes Datum wie Jahr 1) bestehen — auf Nessies ausdrücklichen
  Wunsch, obwohl die Schleife dafür rechnerisch nicht nötig wäre (auch
  Millionen einfacher `QDate`-Berechnungen sind in Millisekunden erledigt).
- `IViewChart::setMaxIntervalCount(int)` reicht das Ergebnis an die View
  durch. `ViewChart::setMaxIntervalCount()` ruft `m_countSpin->setMaximum()`
  auf (Signal geblockt — verhindert einen rekursiven `onControlsChanged()`-
  Aufruf, falls der aktuelle Wert dabei automatisch heruntergeklemmt wird).
  Das wirkt automatisch auf **alle** Eingabewege — Pfeiltasten, Tippen und
  die oben beschriebene Mausrad-Steuerung —, da Qt `QSpinBox` intern immer
  an `maximum()` clamped.
- Zusätzlich presenter-seitig geklemmt: `refresh()` begrenzt die
  tatsächlich für die Datenabfrage verwendete Anzahl selbst per
  `std::min(maxCount, ...)`, unabhängig davon, ob die View den Wert schon
  korrekt heruntergeklemmt hat. Macht die Presenter-Logik durch Fakes
  eigenständig testbar (`FakeViewChart` in `tst_chartform.cpp` klemmt
  bewusst NICHT automatisch, siehe TESTING.md) und robust gegen künftige
  View-Implementierungen, die das UI-seitige Clamping anders lösen.

Regressionstests: `test_refresh_setsMaxIntervalCount_basedOnEarliestDailyValue`,
`test_refresh_intervalCountBeyondMax_clampsQueryToEarliestDate`,
`test_refresh_singleValueAtRangeEnd_maxIntervalCountStaysAtOne`,
`test_onControlsChanged_countAboveMax_clampsEffectiveQueryRange`,
`test_refresh_monthIntervalNotLandingOnEarliestDate_maxIntervalCountStillReachesIt`,
`test_refresh_weekIntervalNotLandingOnEarliestDate_maxIntervalCountStillReachesIt`,
`test_refresh_yearIntervalNotLandingOnEarliestDate_maxIntervalCountStillReachesIt`,
`test_refresh_monthIntervalLandingExactlyOnEarliestDate_maxIntervalCountDoesNotOvershoot`,
`test_refresh_dayIntervalWithLongHistory_maxIntervalCountExceedsOldFixedCeiling`,
`test_refresh_corruptEarliestDate_maxIntervalCountClampedByAbsoluteSafetyCeiling`
(alle `tst_chartform.cpp`), sowie `test_earliestDate`
(`tst_dailyvaluesrepository.cpp`) — siehe TESTING.md.

---

### ChartPopup — Rechtsklick-Popup-Chart (implementiert 31.07.2026)

Rahmenloses Popup-Fenster (`forms/ChartForm/ChartPopup.h/.cpp`), das nur
Überschrift + Graph + Legende zeigt — portiert vom C#-Referenz-Popup
`FrmChart`. Öffnet sich per einfachem Rechtsklick auf eine Portfolio-Zeile
in `MainWindow` (`m_finalValueTable`/`m_marketValueTable`), unabhängig von
einer eventuell bereits geöffneten `ShareDetailsForm`. Kein eigenes
MVP-Presenter/Model nötig: `ChartPopup` bettet lediglich eine eigenständige
`ViewChart`-Instanz ein — `ViewChart` bringt bereits ihre eigene
`ModelChart`-/`PresenterChart`-Instanz mit (siehe "ChartForm-Details" oben).

#### ViewChart: Compact-Modus

`ViewChart` bekommt einen neuen optionalen Konstruktor-Parameter
`bool compact = false`. Im Compact-Modus wird die "Selektion:"-Box
(Serien-Checkboxen + Start-Datum/Interval/Anzahl-Formular — technisch eine
einzige `QGroupBox`, siehe `ViewChart::setupSelektionBox()`) zwar wie gewohnt
angelegt, aber nicht ins sichtbare Layout gehängt (`hide()` + `setParent
(this)` statt `rightLayout->addWidget(...)`) — sichtbar bleibt nur die
"Legende"-Box. Dadurch bleiben alle `IViewChart`-Getter sowie die
bestehende Mausrad-Steuerung auf `m_countSpin` (`eventFilter()`/
`applyWheelStep()`, siehe "ChartForm-Details" oben) unverändert
funktionsfähig — Nessies Vorgabe "Mausrad ändert das Intervall" ist damit im
Popup ohne jede Zusatzlogik erfüllt, exakt wie im C#-Referenzverhalten
(`OnChartDailyValues_MouseWheel` in `FrmChart`). Da im Compact-Modus ohnehin
nur die Default-Checkbox (`ClosingPrice`) angehakt ist und nie umgeschaltet
werden kann, zeigt der Chart wie in der C#-Referenz immer nur die
Schluss-Kurs-Serie (+ Kauf-/Verkauf-Markerlinien) — die C#-Referenz baut die
Popup-Grafik ohnehin komplett unabhängig von etwaigen Checkbox-Zuständen.
Die rechte Panel-Breite ist im Compact-Modus schmaler (260px statt 380px),
da nur noch die Legende Platz braucht.

Zusätzlich bekommt `ViewChart` einen rein View-internen Getter
`rangeInfo() const` (kein Teil von `IViewChart`), der den zuletzt per
`setRangeInfo()` gesetzten Text zurückgibt. Grund: `PresenterChart::
loadAndDisplay()` läuft bereits synchron im `ViewChart`-Konstruktor und
feuert `titleInfoChanged()` dabei ein erstes Mal — bevor sich `ChartPopup`
(das `ViewChart` erst nach dessen vollständiger Konstruktion erzeugt und
verbindet) überhaupt verbinden kann. `rangeInfo()` erlaubt `ChartPopup`,
diesen initialen Wert direkt im Anschluss nachträglich abzugreifen, statt
ihn zu verpassen (siehe "ChartPopup" unten).

#### ChartPopup

- Konstruktor nimmt neben der `shareGuid` auch den Aktiennamen entgegen
  (`ChartPopup(shareGuid, shareName, parent)`) — `MainWindow` liest ihn
  direkt aus der Name-Spalte der Portfolio-Zeile (Index 2, bei
  `FinalValueColumn` UND `MarketValueColumn` identisch), statt ihn per
  Repository nachzuschlagen.
- **Überschrift** (ergänzt 31.07.2026, Nessies Rückmeldung "Was auch fehlt
  ist die Überschrift mit Informationen!"): zentriertes `QLabel`
  (`chartPopupHeader`) oberhalb des Charts zeigt `<b>Aktienname</b>` +
  "Zeitraum: ... / Entwicklung: ..." — Pendant zum C#-Referenz-Chart-Titel
  (`FrmChart.Title`, dort direkt auf das WinForms-Chart-Steuerelement
  gezeichnet, hier ein eigenes Qt-Label, da `QChart` keinen vergleichbar
  einfachen mehrzeiligen Titel unterstützt). Der Zeitraum-Teil kommt über
  `ViewChart::titleInfoChanged()`; der initiale Wert wird direkt nach dem
  Verbinden einmalig über `ViewChart::rangeInfo()` nachgeholt (siehe oben) —
  spätere Änderungen (z. B. durch Mausrad-Zoom) aktualisieren die
  Überschrift automatisch über dasselbe Signal.
- Fensterflags: `Qt::Tool | Qt::FramelessWindowHint` — kein Rahmen, keine
  Taskbar-Präsenz. `Qt::WA_DeleteOnClose` gesetzt: `MainWindow` erzeugt das
  Popup per `new ChartPopup(...)` ohne Owner und muss sich um dessen
  Zerstörung nicht weiter kümmern.
- `leaveEvent()` schließt das Fenster automatisch, sobald die Maus den
  Fensterbereich verlässt — Pendant zu
  `OnChartDailyValues_MouseLeave`/`OnLblNoDataMessage_MouseLeave` in der
  C#-Referenz (dort an zwei einzelne Kind-Widgets gebunden; hier genügt ein
  einziger Handler auf dem Popup-Fenster selbst, da es außer Überschrift und
  `ViewChart`-Instanz keine weiteren Geschwister-Widgets enthält).
  **Spurious-Leave-Fix (ergänzt 31.07.2026, Nessies Rückmeldung "Dialog geht
  zu, auch wenn die Maus noch auf dem Dialog ist"):** `QChartView`
  (`QGraphicsView`) hat einen eigenen Viewport, an dessen inneren
  Widget-Grenzen Qt gelegentlich ein Leave auf `ChartPopup` selbst auslöst,
  obwohl die Maus tatsächlich noch innerhalb des Popups steht — ein
  bekannter Qt-Effekt bei `QGraphicsView`-Kindwidgets. `leaveEvent()` prüft
  deshalb zusätzlich `QCursor::pos()` gegen die eigene Bildschirmgeometrie
  (`QRect(mapToGlobal(QPoint(0,0)), size())`) und schließt nur, wenn die
  Maus tatsächlich außerhalb liegt — ein falsches Leave wird ignoriert.
  Bewusst **nicht** automatisiert getestet (siehe TESTING.md): eine
  verlässliche Prüfung bräuchte eine echte, plattformabhängige
  Cursor-Bewegungssimulation, die in einer Offscreen-Testumgebung nicht
  robust nachstellbar ist.
- `showAt(globalPos)` positioniert das Popup so, dass sein oberer linker
  Rand an `globalPos` liegt, geklemmt an die verfügbare Geometrie des
  Bildschirms unter dem Cursor (`QGuiApplication::screenAt()`) — verhindert
  ein Abschneiden am rechten/unteren Bildschirmrand bei einem Rechtsklick
  nahe der Kante.
- Standardgröße ist ein kompaktes Fenstermaß (`kPopupWidth`/`kPopupHeight`
  in `ChartPopup.cpp`, letzteres seit der Überschrift um 40px erhöht).
  `MainWindow::onPortfolioRowRightClicked()` überschreibt Breite und
  horizontale Position jedoch vor `showAt()` (siehe "MainWindow-Verdrahtung"
  unten) — die Höhe bleibt beim kompakten Standardmaß.

#### MainWindow-Verdrahtung

`m_finalValueTable`/`m_marketValueTable` bekommen
`setContextMenuPolicy(Qt::CustomContextMenu)` — bewusst zweckentfremdet, um
Rechtsklicks direkt abzufangen, ohne ein natives Kontextmenü zu zeigen.
Beide Tabellen sind über `customContextMenuRequested()` mit dem neuen Slot
`MainWindow::onPortfolioRowRightClicked(const QPoint&)` verbunden — Pendant
zu `onPortfolioRowDoubleClicked()`, aber:

- GUID-Ermittlung über `table->itemAt(pos)` statt eines direkt gelieferten
  `QTableWidgetItem*` (die auslösende Tabelle liefert `sender()`, da
  `customContextMenuRequested()` anders als `itemDoubleClicked()` keine
  Tabellenreferenz mitliefert).
- Keine `hasValidShare()`-Prüfung: eine leere/unbekannte GUID führt in
  `ChartPopup`/`PresenterChart` lediglich zum bestehenden
  "keine Kursdaten"-Leerzustand (`showEmptyChart()`), keinen modalen
  Fehlerdialog wie bei `ViewShareDetails` — der frühe Return bei leerer GUID
  ist daher nur eine Abkürzung, kein Fehlerfall.
- **Breite/Position** (überarbeitet 31.07.2026, mehrere Rückmeldungen: erst
  "etwas kleiner als das Hauptfenster ... Links und rechts 5px", dann
  "nochmals um 50 px" schmäler, zuletzt "horizontal zentriert ... Du kannst
  hier 'Hauptfensterbreite − 50px' einsetzen, also auf jeder Seite 25px
  schmäler"): Popup-Breite = Hauptfensterbreite − 50px, mindestens 200px
  (`qMax(200, ...)` als Sicherheitsnetz gegen ein sehr schmales
  Hauptfenster). Horizontal **zentriert** zum Hauptfenster ausgerichtet
  (nicht mehr linksbündig) — bei dieser Breite gleichbedeutend mit 25px
  Rand auf jeder Seite, daher die vereinfachte Formel gegenüber der
  vorherigen 2×5px+50px-Rechnung. Nur die vertikale Position folgt weiterhin
  dem Rechtsklick-Punkt (`table->viewport()->mapToGlobal(pos)` liefert die
  Y-Koordinate); die X-Koordinate kommt aus der Mittelpunkt-Berechnung
  relativ zum Hauptfenster (`mapToGlobal(QPoint(width()/2, 0))`).

---


### MainWindow-Details

Das MainWindow ist die zentrale Anwendungsschicht. Es verwaltet Portfolio-Datei,
Toolbar-Aktionen und die zwei Aktienlisten (Depotwert- und Marktwert-Tabelle).

#### Portfolio-Grid (Aktienübersicht)

Zwei `QTabWidget`-Tabs, jeder mit einer Haupttabelle und einer Footer-Tabelle
(3 Summenzeilen). Die Spalten werden über `TwoLineDelegate` mit zwei Textzeilen
pro Zelle gerendert.

Depotwert-Tab (`m_finalValueTable`):

| Spalte | Inhalt oben | Inhalt unten |
| ------ | ----------- | ------------ |
| Icon | Update-Status-Icon | — |
| WKN | WKN-String | — |
| Name | Aktienname | — |
| Anteile | Netto-Volumen | — |
| Kosten / Dividenden | `totalBrokerage` € (oben) | `totalDividend` € (unten) |
| Preis | `curPrice` | `prevDayPrice` |
| (Chart-Icon) | Entwicklungs-Pfeil | — |
| Vortag | Δ€ | Δ% |
| Aktuelle Entwicklung | `profitLossFinal` € | `profitLossPctFinal` % |
| Einzahlung / Marktwert | `purchaseValueFinal` € | `curValue` € |
| (Chart-Icon) | Kpl. Entwicklungs-Pfeil | — |
| Komplette Entwicklung | `completeProfitLoss` € | `completeProfitPct` % |
| Kpl. Einzahlung / Kpl. Marktwert | `completePurchase` € | `completeCurValue` € |

Marktwert-Tab (`m_marketValueTable`) — ohne Kosten/Dividenden; Werte durchgängig
brokeragefrei. Die Komplett-Spalten sind in sich konsistent
(`Kpl. Entwicklung = Kpl. Marktwert − Kpl. Einzahlung`):

| Spalte | Inhalt oben | Inhalt unten |
| ------ | ----------- | ------------ |
| Icon | Update-Status-Icon | — |
| WKN | WKN-String | — |
| Name | Aktienname | — |
| Anteile | Netto-Volumen | — |
| Preis | `curPrice` | `prevDayPrice` |
| (Chart-Icon) | Entwicklungs-Pfeil | — |
| Vortag | Δ€ | Δ% |
| Aktuelle Entwicklung | `profitLoss` € | `profitLossPct` % |
| Einzahlung / Marktwert | `purchaseValue` € | `curValue` € |
| (Chart-Icon) | Kpl. Entwicklungs-Pfeil | — |
| Komplette Entwicklung | `completeProfitLossMarket` € | `completeProfitPctMarket` % |
| Kpl. Einzahlung / Kpl. Marktwert | `completePurchaseMarket` € | `completeCurValueMarket` € |

#### Portfolio-Label — "Letzte Aktualisierung" (Feature 21.07.2026)

`m_portfolioLabel` ("Portfolio-Übersicht ( Einträge: X ) / Letzte
Aktualisierung: Y") zeigte den Zeitstempel-Teil dauerhaft als "-" —
`updatePortfolioLabel()` wurde nie mit einem echten Wert für `lastUpdate`
aufgerufen, obwohl der Parameter dafür bereits vorgesehen war.

Statt eines separaten Persistenz-Mechanismus (z.B. eigene Meta-Tabelle) wird
der bereits vorhandene `shares.last_internet_update`-Wert wiederverwendet:

- `ShareRepository::maxLastInternetUpdate()` liefert per SQL `MAX()` das
  Maximum über alle Aktien. ISO-8601-Strings (`yyyy-MM-ddTHH:mm:ss`) sortieren
  lexikographisch korrekt, eine Konvertierung nach `QDateTime` ist dafür nicht
  nötig. Aktien, deren `last_internet_update` `NULL` oder leer ist (noch nie
  aktualisiert), werden per `WHERE`-Klausel ausgeschlossen, damit eine einzelne
  nie aktualisierte Aktie das Ergebnis nicht auf einen leeren String zieht.
- `MainWindow::formatLastPortfolioUpdate()` ruft `maxLastInternetUpdate()` auf
  und formatiert das Ergebnis mit der App-Locale (`QLocale::ShortFormat`) —
  gleiche Konvention wie `formatDateTime()` in `PresenterShareDetails`. Liefert
  `maxLastInternetUpdate()` einen leeren String (Portfolio leer oder noch nie
  aktualisiert), wird `"-"` zurückgegeben.
- Aufrufstellen: `populatePortfolioTables()` (Laden/Neustart derselben
  Portfolio-Datei) sowie `onRefreshShareFinished()` (live während "Alle
  aktualisieren" — siehe "Methode onRefreshShareFinished()" unten), jeweils
  über `updatePortfolioLabel(entryCount, formatLastPortfolioUpdate())`.

Voraussetzung für ein korrektes `MAX()` über alle `ShareUpdateType`-Varianten:
`onDailyValuesUpdated()` musste um einen `updateLastInternetUpdate()`-Aufruf
ergänzt werden, da bis dahin nur `onMarketValuesUpdated()` diesen Wert
schrieb — siehe @note bei "Callback onDailyValuesUpdated()" oben.

#### Fenstertitel — Version statt Dateiname (Bugfix + Feature, 01.08.2026)

Zwei zusammenhängende Änderungen an `setWindowTitle()`, auf Nessies Vorgabe:

- **Bugfix:** `MainWindow::updateWindowTitle(portfolioPath)` hängte bislang
  zusätzlich den Dateinamen des geöffneten Portfolios an den Fenstertitel
  an ("Share Portfolio Manager - portfolio.db"). Das war redundant zur
  bereits vorhandenen, live aktualisierten Anzeige des vollen Pfads unten
  rechts in der Statusleiste (`updateStatusBarPortfolio()`, siehe
  `m_portfolioPathLabel`). Die Methode `updateWindowTitle()` wurde
  vollständig entfernt; ihre drei Aufrufer (`onNewPortfolio()`,
  `onOpenPortfolio()`, `onSaveAsPortfolio()`) rufen jetzt direkt
  `updateStatusBarPortfolio(filePath)` auf — der einzig verbliebene Teil
  ihrer bisherigen Aufgabe.
- **Feature:** Der Fenstertitel zeigt stattdessen die aktuelle
  Applikationsversion: "Share Portfolio Manager (Version X.Y.Z)". Neue
  private Hilfsmethode `MainWindow::baseWindowTitle()` liest die Version
  über `QCoreApplication::applicationVersion()` — dieselbe App-weite
  Quelle, die `AboutForm` bereits verwendet (siehe Abschnitt
  "Versionierung" oben), damit kein zweiter Ort für den Versionsbump
  entsteht und kein zusätzlicher `Version.h`-Include in `MainWindow.cpp`
  nötig ist.

`baseWindowTitle()` wird sowohl in `initialize()` (Titel beim Start) als
auch — vor der Entfernung — in `updateWindowTitle()` verwendet; da Letztere
komplett entfällt, ist `initialize()` inzwischen die einzige Aufrufstelle.
Der Titel ändert sich damit nach dem Start nicht mehr automatisch mit
geöffnetem/gewechseltem Portfolio — beabsichtigt, siehe Bugfix oben.

Testabdeckung: `test_construction_windowTitleContainsVersion` in
`tst_mainwindow.cpp` prüft per `QRegularExpression` auf ein `(Version
X.Y.Z)`-Muster im Titel, statt den literalen `SPM_VERSION_STRING` zu
vergleichen — bleibt damit bei künftigen Versionsbumps unverändert grün.
Damit die Prüfung eine echte Versionsnummer sieht (nicht nur einen leeren
String), setzt `tst_mainwindow.cpp`s eigene `main()` jetzt ebenfalls
`app.setApplicationVersion(QStringLiteral(SPM_VERSION_STRING))`, analog zu
`main.cpp` — dafür ergänzt `tests/forms/CMakeLists.txt` bei
`target_include_directories(tst_mainwindow)` den Pfad
`${CMAKE_BINARY_DIR}/app`, wo `Version.h` von CMake generiert wird. Der
zuvor vorhandene, aber bereits vor diesem Feature bedeutungslose Test
`test_updateWindowTitle_showsFileName` (konstruierte nur ein `MainWindow`
und prüfte `startsWith("Share Portfolio Manager")`, ohne
`updateWindowTitle()` je aufzurufen) wurde ersatzlos entfernt.

#### Footer-Tabelle (Summenzeilen)

Jeder Tab hat unter der Haupttabelle eine eigene 3-zeilige Footer-Tabelle. Der
Marktwert-Footer (`m_marketValueFooter`) zeigt:

| Zeile | Beschriftung | Werte |
| ----- | ------------ | ----- |
| 0 | Einzahlung (gesamt) | `mPurchase` (Einzahlung/Marktwert), `mcPurchase` (Kpl. Einzahlung) — einzeilig |
| 1 | Entwicklung (gesamt) | `mProfit`/`mProfitPct` (Aktuelle Entwicklung), Entwicklungs-Icon (CompleteChart), `mcProfit`/`mcProfitPct` (Komplette Entwicklung) |
| 2 | Aktueller Depotstand | `mMarketValue` (Einzahlung/Marktwert), `mcCurValue` (Kpl. Einzahlung/Kpl. Marktwert) — einzeilig |

Totale liefern `portfolioTotalsMarket` (laufende Spalten) und
`portfolioCompleteTotalsMarket` (Kpl.-Spalten).

Der Depotwert-Footer (`m_finalValueFooter`) ist analog aufgebaut, weicht aber
wegen der zusaetzlichen Spalte Kosten / Dividenden im Label-Layout ab:

| Zeile | Beschriftung | Werte |
| ----- | ------------ | ----- |
| 0 | Einzahlung (gesamt) | `tPurchase` (Einzahlung/Marktwert), `cPurchase` (Kpl. Einzahlung) — einzeilig |
| 1 | Entwicklung (gesamt) | Label "Kosten (ges.)" / "Dividenden (ges.)" und 2-zeiliger Wert `tBrokerage`/`tDividend` in der Spalte Kosten / Dividenden; `tProfit`/`tProfitPct` (Aktuelle Entwicklung), Entwicklungs-Icon (CompleteChart), `cProfit`/`cProfitPct` (Komplette Entwicklung) |
| 2 | Aktueller Depotstand | `tCurValue` (Einzahlung/Marktwert), `cCurValue` (Kpl. Einzahlung/Kpl. Marktwert) — einzeilig |

Totale liefert `portfolioTotalsFinal`; `tBrokerage` und `tDividend` werden als
Summe von `totalBrokerage` bzw. `totalDividend` ueber alle Aktien gebildet.

Abweichungen zum Marktwert-Footer (bedingt durch die Spalte Kosten / Dividenden):

- Die Zeilenbeschriftung spannt per `setSpan(row, Price, 1, 3)` nur Preis +
  (Chart-)Icon + Vortag (rechtsbuendig, endet an Vortag). Die Spalte Kosten /
  Dividenden links davon bleibt frei fuer ihren eigenen Wert.
- In der Entwicklungs-Zeile steht links der Kosten / Dividenden-Spalte das
  2-zeilige Label per `setSpan(row, Icon, 1, 4)`, rechtsbuendig neben dem Wert.
- Delegat-Sonderfall (nur Footer): die Anker-Spalte des Zeilen-Labels (Preis)
  nutzt den `CenterIconDelegate` (einzeiliger Text), die Anker-Spalte des
  Kosten / Dividenden-Labels (Icon) den `TwoLineDelegate` (zwei Zeilen), damit
  beide Label-Arten korrekt rendern.

Layout-Konventionen (an die C#-Anwendung angelehnt):

- Die Beschriftung jeder Zeile spannt im Marktwert-Footer per
  `setSpan(row, 0, 1, 7)` die Spalten Icon..Vortag und ist rechtsbuendig
  (endet an der Vortag-Spalte). Die Spalten **nach** Vortag werden NICHT
  verbunden — die Werte stehen einzeln unter ihren jeweiligen Ueberschriften.
  Der Depotwert-Footer spannt stattdessen nur Preis..Vortag (siehe oben).
- In der Entwicklungs-Zeile sitzt in der `CompleteChart`-Spalte das
  Entwicklungs-Icon (`devIcon(mcProfitPct)`) — wie im Grid.
- Einzelwert-Zellen (Zeile 0 und 2) werden vom `TwoLineDelegate` vertikal
  zentriert dargestellt (eine saubere Zeile statt obere Hälfte + Leerraum).
- Der Footer scrollt nie: feste Höhe = 3 × 34 px + Rahmen, beide Scrollbalken aus.
- Die Footer-Spaltenbreiten werden laufend über `QHeaderView::sectionResized`
  mit der Haupttabelle synchronisiert. Das ist nötig, weil die gestreckte
  Name-Spalte ihre echte Breite erst beim Layout erhält; eine einmalige
  Spiegelung beim Daten-Update würde die Werte nach links verschieben.

#### Darstellung (Farben, Icons, Zeilen)

Gilt für beide Tabs (Haupttabellen und Footer):

- Farben sind theme-abhängig aus der Palette: `neutral` =
  `palette().color(QPalette::Text)`, `muted` = `neutral` mit Alpha 140
  (nur für leere Platzhalterwerte in Footer-Einzelwertzeilen genutzt — alle
  sichtbaren Zweitzeilen in Haupttabellen und Footer nutzen `neutral` bzw.
  `perfColor(...)`, damit Ober- und Unterzeile optisch gleich dargestellt
  werden; Bugfix 03.07.2026 — zuvor nutzten "Kosten/Dividenden" und "Preis"
  fälschlich `muted` für die Unterzeile, wodurch sie optisch wie eine andere
  Schrift wirkte). Gewinn/Verlust nutzen dieselbe Quelle wie die
  Statusmeldungsbox — `AppSettings::logColorAt(5)` (Erfolg-Grün) bzw.
  `logColorAt(3)` (Fehler-Rot); ein Nullwert wird in Textfarbe gezeichnet.
- Icons: `setIconSize(24×24)`; die Entwicklungs-Pfeile liegen als 24-px-PNGs
  vor. Der `CenterIconDelegate` zentriert die Icon-Dekoration in den
  Icon-Spalten (Icon, PrevDayChart, CompleteChart) von Haupttabelle und Footer.
  Die Icon-Auswahl selbst läuft immer über dieselbe `devIcon(pct)`-Lambda
  (`pct > 2.0` → PositivStrong, `pct > 0.0` → PositivNormal, `pct < -2.0` →
  NegativStrong, `pct < 0.0` → NegativNormal, sonst Neutral) — einmal definiert
  in `populatePortfolioTables()`/`updatePortfolioFooters()`, und seit dem
  Bugfix vom 06.07.2026 zusätzlich lokal in `onMarketValuesUpdated()` (siehe
  dort), damit auch ein Einzel-Refresh die Icon-Zellen mit aktualisiert statt
  nur die Text-Zellen (`setTwoLine`). Zuvor blieb `PrevDayChart` nach einem
  Einzel-Refresh auf dem Icon-Stand des letzten vollständigen Tabellenaufbaus
  stehen — ein gestiegener Tageskurs konnte so trotz korrekt grün angezeigter
  Werte (`+1,20 €`, `+0,26 %`) weiterhin mit einem fallenden Icon dargestellt
  werden, da `it->setIcon(...)` für `PrevDayChart`/`CompleteChart` im
  Einzel-Refresh-Pfad schlicht nie aufgerufen wurde.
- Zeilen: Haupttabellen 38 px, Footer 34 px; alternierende Zeilenfarben und
  Gridlinien in Haupttabellen und Footer.

#### TwoLineDelegate (forms/MainForm/TwoLineDelegate.h)

Header-only `QStyledItemDelegate` der zwei Textzeilen in einer Qt-Zelle rendert.
Daten werden über `TwoLineRole`-Konstanten gesetzt (`Qt::UserRole + 10..13`):
`Top`, `Bottom`, `TopColor`, `BottomColor`. Kein `Q_OBJECT` — keine eigenen Signals.

Beide Zeilen werden in der Zellenschrift (`option.font`) gezeichnet, also **gleich
groß** (die untere Zeile war früher kleiner gerendert). Hat eine Zelle keine zweite
Zeile (`Bottom` leer), wird der Einzelwert **vertikal in der ganzen Zelle zentriert**
statt in der oberen Hälfte — sonst sähen einzeilige Werte wie eine zweizeilige Zelle
mit leerer Unterzeile aus. Standardausrichtung ist rechtsbündig/vertikal zentriert,
überschreibbar via `Qt::TextAlignmentRole`. Die tatsächliche Zeilenhöhe geben die
Tabellen explizit vor (`setFixedHeight`: Haupttabelle 38 px, Footer 34 px); der
`sizeHint` ist damit nicht die maßgebliche Größe.

@note **Bugfix Grid-Selektionsfarbe (29.07.2026, zweiter Anlauf):** Nach
Einführung von `GridStyle::applySelectionStyle()` (siehe "GridStyle" unten)
blieben genau die von `TwoLineDelegate` gerenderten Spalten (Kosten /
Dividenden, Preis, Vortag, Aktuelle Entwicklung, Einzahlung / Marktwert,
Komplette Entwicklung, Kpl. Einzahlung / Kpl. Marktwert) bei Zeilen-Selektion
uneingefärbt, während alle einzeiligen Spalten korrekt blau/gelb erschienen.

Erster Fixversuch (`opt.widget->style()` statt `QApplication::style()` für
`drawControl()`, analog zu Qt's eigenem `QStyledItemDelegate::paint()`) traf
zwar denselben Style-Objekt-Pfad wie die einzeiligen Spalten, löste das
Problem aber nicht: Qt spiegelt eine per Stylesheet gesetzte
`item:selected { color: ...; background-color: ...; }`-Regel nicht in eine
über `QPalette` abfragbare Farbe zurück — `pal.color(QPalette::
HighlightedText)` lieferte weiterhin die alte System-Highlight-Farbe (meist
Weiß) statt Gelb, nur der Hintergrund näherte sich zufällig dem
System-Standard-Blau an, wodurch der Unterschied optisch kaum auffiel
("nichts geändert").

Endgültiger Fix: `TwoLineDelegate::paint()` fragt bei Selektion nicht mehr
Style/Palette ab, sondern verwendet direkt `GridStyle::kSelectionBackground`/
`kSelectionForeground` (`painter->fillRect()` für den Hintergrund, direkte
Stiftfarbe für beide Textzeilen) — dieselben Konstanten, die auch
`table->setStyleSheet()` setzt. Damit ist die Farbe garantiert identisch zum
Rest der Zeile, unabhängig von Qt's QSS-zu-Palette-Übersetzung. Die bei
Selektion bisher Vorrang genießende explizite `TopColor`/`BottomColor`
(Grün/Rot bei Gewinn/Verlust) wird bei Selektion jetzt konsequent von der
Selektionsfarbe überschrieben, nicht umgekehrt.

Betrifft ausschließlich `MainWindow` (Depotwert-/Marktwert-Haupttabellen) —
die Editier-Dialoge/`ViewShareDetails` nutzen `TwoLineDelegate` nicht.

#### CenterIconDelegate (forms/MainForm/CenterIconDelegate.h)

Header-only `QStyledItemDelegate` ohne `Q_OBJECT`. Überschreibt nur
`initStyleOption` und setzt `option->decorationAlignment = Qt::AlignCenter`. Damit
werden die Icons in den reinen Icon-Spalten (Icon/Status, PrevDayChart, CompleteChart)
zentriert statt linksbündig dargestellt — angewandt auf beide Haupttabellen und beide
Footer.

#### GridStyle (widgets/GridStyle.h) — App-weite Grid-Selektionsfarbe (erledigt, 29.07.2026)

Auf Nessies Vorgabe: In der C#-Referenzanwendung wird die selektierte Zeile in
allen Grids mit blauem Hintergrund und gelber Schrift dargestellt (statt der
Standard-Highlight-Farbe der Qt-Palette/des Systemthemes) — bisher galt das
in der Qt-Portierung nirgends explizit, alle Tabellen nutzten die
Palette-Highlight-Farbe des jeweiligen Themes.

Header-only Helper `GridStyle::applySelectionStyle(QTableWidget*)`, analog zu
`CenterIconDelegate`/`TwoLineDelegate` (kein `Q_OBJECT`, keine eigene `.cpp`).
Setzt additiv (hängt an ein evtl. vorhandenes Stylesheet an, statt es zu
überschreiben) `QTableWidget::item:selected { background-color: ...; color: ...; }`.
Farbwerte theme-neutral gewählt, angelehnt an den Log-Farben-Fix vom
24.07.2026 (ausreichender Kontrast auf hellem und dunklem Hintergrund, da das
Linux-AppImage mangels Platform-Theme-Plugin immer auf die helle Palette
zurückfällt):

| Konstante | Wert | Verwendung |
| --- | --- | --- |
| `GridStyle::kSelectionBackground` | `#1c3f8f` | Hintergrund selektierte Zeile |
| `GridStyle::kSelectionForeground` | `#ffd400` | Schrift selektierte Zeile |

Angewandt auf alle selektierbaren Daten-Tabellen der App:
- `MainWindow::setupCentralWidget()` — `setupTable`-Lambda, deckt beide
  Haupttabellen (Depotwert `m_finalValueTable`, Marktwert `m_marketValueTable`) ab.
- `OverviewTabWidget::buildFrozenTable()` — deckt automatisch alle fünf
  Edit-Dialoge (`ViewBuyEdit`, `ViewSaleEdit`, `ViewDividendEdit`,
  `ViewBrokerageEdit`, `ViewShareAdd`) sowie die drei Tabs in
  `ViewShareDetails` (Gewinne/Verluste, Dividenden, Kosten) ab, da alle
  über dieses gemeinsame Widget laufen.

Bewusst NICHT angewandt auf Footer-Tabellen (Gesamt-Zeile in
`OverviewTabWidget` sowie `m_finalValueFooter`/`m_marketValueFooter` in
`MainWindow`) — diese haben durchgängig `QAbstractItemView::NoSelection`,
ein Selektionsstil wäre dort wirkungslos.

#### ShareCalculator (utils/ShareCalculator.h/.cpp)

Statische, zustandslose Hilfsklasse. `compute(guid, curPrice, prevDayPrice)` liest Kaeufe,
Verkaeufe, Brokerage und Dividenden frisch aus den Repositories und gibt ein `ShareValues`
zurueck. Keine laufenden Salden, keine Sentinel-Guards. Die anteilige Brokerage/Rabatt eines
teilverkauften Kaufs wird hier aus der verknuepften Brokerage des Kaufs rekonstruiert,
unabhaengig von den auf `SaleBuyDetail` gespeicherten Anteilen.

Beide Tabs teilen sich die "aktuellen" Spalten (Einzahlung/Marktwert, Aktuelle Entwicklung),
fuellen sie aber mit zwei verschiedenen Wertesaetzen. Beide Basen zaehlen nur die aktuell
GEHALTENEN Anteile (FIFO: jeder Kauf steuert sein Restvolumen bei) und sind damit konsistent
zu `curValue`.

Einheitlicher Rundungs-Vertrag (cent-genau): Jeder Geldbetrag wird per `roundAway(value, 2)`
kaufmaennisch (half-away-from-zero) auf 2 Stellen gerundet -- jeder Kaufwert, jeder
Verkaufswert, jeder anteilige Brokerage/Rabatt-Teil, der aktuelle Marktwert und jede
angezeigte Aggregatzelle. `roundAway` addiert vor dem Runden ein winziges Epsilon in
Betragsrichtung, damit Halb-Cent-Grenzfaelle (z. B. 1,005) identisch zur C#-Referenz
(`MidpointRounding.AwayFromZero`) gerundet werden.

Marktwert-Tab (ohne Brokerage):

- `purchaseValue` = gehaltene Basis: Summe(round(remVol x price) - reductionPart)
- `curValue` = round(heldVolume x curPrice)
- `profitLoss` = curValue - purchaseValue
- `saleProfitLoss` = realisierte G/V ohne Brokerage (mit Rabatt), nur Footer-Aggregat
- `marketValue` = curValue + saleProfitLoss
- `completePurchaseMarket` = alle Kaeufe: Summe(round(vol x price) - reduction) (ohne Brokerage)
- `completeProfitLossMarket` = (curValue - purchaseValue) + realisierte G/V MIT Brokerage
- `completeCurValueMarket` = completePurchaseMarket + completeProfitLossMarket

Die Spalten "Einzahlung/Marktwert" und "Kpl. Einzahlung" sind brokeragefrei (reiner
Marktwert der gehaltenen Anteile bzw. Einzahlung ohne Brokerage). "Komplette Entwicklung"
addiert zur unrealisierten Entwicklung der gehaltenen Anteile die **realisierte** G/V
abgeschlossener Verkaeufe inkl. Verkaufs- und anteiliger Kaufgebuehren (entspricht
`profitLossBrokerageReduction`), da dies der tatsaechlich erzielte Netto-Gewinn/-Verlust ist.
"Kpl. Marktwert" = "Kpl. Einzahlung" + "Komplette Entwicklung", womit
`Kpl. Entwicklung = Kpl. Marktwert - Kpl. Einzahlung` exakt gilt.

Die realisierte G/V wird aus Aggregaten gerechnet (Netto-Verkaufserloes minus Kaufkosten der
verkauften Stuecke = alle Kaeufe minus noch gehaltene Kaeufe), nicht aus den `SaleBuyDetail`-
Records. Footer-Totale liefert `portfolioCompleteTotalsMarket`.

Depotwert-Tab (mit Brokerage + Dividenden):

- `purchaseValueFinal` = gehaltene Basis: Summe(round(remVol x price) + brokeragePart - reductionPart)
- `profitLossFinal` = curValue - purchaseValueFinal
- `totalBrokerage` = `BrokerageRepository::totalBrokerage()`
- `totalDividend` = `DividendRepository::totalPayoutWithTaxes()` (netto nach Steuer) —
  bei Fremdwährungs-Dividenden zweistufig gerundet: erst `rate x volume` auf 2 Nachkomma-
  stellen, dann das Ergebnis der Division durch `exchange_ratio` erneut auf 2 Nachkomma-
  stellen. Identisch zu `DividendObject::calculateValues()`, damit die Summe im Depotwert-
  Tab exakt der Summe der einzeln im Dividenden-Tab angezeigten Zeilen entspricht (Bugfix
  02.07.2026 — die SQL-Aggregation rundete zuvor nur einmal, was bei bestimmten Wechsel-
  kursen zu Differenzen von wenigen Cent führte; siehe Regressionstest
  `test_totalPayoutWithTaxes_matchesDoubleRoundedDividendObjectSum` in
  `tests/repositories/tst_dividendrepository.cpp`).
- `completePurchase` = alle Kaeufe: Summe(round(vol x price) + brokerage - reduction)
- `completeCurValue` = curValue + Summe(Verkaufs-Auszahlung inkl. Brokerage/Rabatt) + totalDividend
- `completeProfitLoss` = completeCurValue - completePurchase

Die Semantik spiegelt das C#-Referenzprojekt (`ShareObjectMarketValue` / `ShareObjectFinalValue`):
gehaltene Kostenbasis, einziger Unterschied der "aktuellen" Spalten ist die Brokerage, und die
Komplett-Sicht des Depotwerts setzt Gesamteinsatz (alle Kaeufe inkl. Gebuehren) gegen
aktuellen Bestand + Netto-Verkaufsauszahlungen + Dividenden. Abweichend von C# wird die
Rundung einheitlich auf 2 Stellen vorgenommen (C# laesst Verkaufswerte und FIFO-Anteile
ungerundet); dadurch ist das Grid cent-genau konsistent zum Qt-Details-Fenster, kann aber in
seltenen Grenzfaellen um einen Cent von den alten C#-Ausgaben abweichen.

#### Toolbar-Aktionen

| Aktion | Signal | Slot | Status |
| ------ | ------ | ------ | ------ |
| Neu | `triggered` | `onNewPortfolio()` | ✅ |
| Öffnen | `triggered` | `onOpenPortfolio()` | ✅ |
| Speichern unter | `triggered` | `onSaveAsPortfolio()` | ✅ |
| Hinzufügen | `triggered` | `onAddShare()` | ✅ |
| Editieren | `triggered` | `onEditShare()` | ✅ |
| Entfernen | `triggered` | `onDeleteShare()` | ✅ |
| Aktualisieren | `triggered` | `onRefreshShare()` | ✅ |
| Alle aktualisieren | `triggered` | `onRefreshAll()` | ✅ |

#### Kursdaten-Abruf (Refresh-Flow)

`MainWindow` besitzt zwei `ParserLib::Parser`-Member-Instanzen:

- `m_parserMarketValues` — ruft den aktuellen Kurs ab (`OnVistaRealTime` oder `YahooRealTime`)
- `m_parserDailyValues` — ruft OHLCV-Tagesdaten ab (`OnVistaHistoryData` oder `YahooHistoryData`)

Beide Parser werden im Konstruktor über `connect()` mit ihren Callbacks verdrahtet.

Für jede Aktie gilt: `ShareObject::updateType()` steuert welche Parser gestartet werden:

| `ShareUpdateType` | MarketValues-Parser | DailyValues-Parser |
| ------ | ------ | ------ |
| `None` | nein (überspringen) | nein |
| `MarketPrice` | ja | nein |
| `DailyValues` | nein | ja |
| `Both` | ja | ja |

Beide Parser starten **parallel** für eine Aktie. Die Koordination erfolgt über
die Flags `m_marketDone` und `m_dailyDone` — erst wenn beide `true` sind, wird
`onRefreshShareFinished()` aufgerufen.

#### Methode selectShareRow() — Grid-Selektion folgt dem Refresh

Wird von `startRefreshForShare()` als erstes aufgerufen, noch bevor ein Parser
gestartet wird. Durchsucht Spalte 0 (`Qt::UserRole`, GUID) beider Tabellen
(`m_finalValueTable` und `m_marketValueTable`) nach der übergebenen GUID und
selektiert die gefundene Zeile per `setCurrentCell()` + `scrollToItem(...,
PositionAtCenter)`. Läuft für **beide** Tabs, unabhängig davon welcher gerade
sichtbar ist, damit ein Tab-Wechsel während des Refreshs stets die korrekte
Zeile zeigt. No-Op bei leerer oder nicht gefundener GUID.

Dadurch folgt die Grid-Selektion sowohl beim Einzel-Refresh (`onRefreshShare()`)
als auch bei jedem Schritt der "Alle aktualisieren"-Queue automatisch der
gerade angefragten Aktie.

@note Der bestehende `enableShareActions`-Lambda (verbunden mit
`selectionChanged` beider `QItemSelectionModel`s in `setupCentralWidget()`,
aktiviert normalerweise Edit/Delete/Refresh sobald eine Zeile selektiert ist)
wurde um einen Busy-Guard erweitert: Solange `m_parserMarketValues.isBusy()`
oder `m_parserDailyValues.isBusy()`, ist der Lambda ein No-Op. Ohne diesen
Guard hätte die programmatische Selektion aus `selectShareRow()` die drei
Aktionen mitten im laufenden Refresh versehentlich wieder freigeschaltet,
obwohl `onRefreshShare()`/`onRefreshAll()` sie explizit deaktiviert hatten.

Bugfix 07.07.2026 — Lücke im Busy-Guard: `startRefreshForShare()` ruft
`selectShareRow()` auf, **bevor** einer der beiden Parser `startParsing()`
aufruft. Wird eine Tabelle zum allerersten Mal selektiert (z. B.
`m_marketValueTable`, wenn zuvor nur im Depotwert-Tab selektiert wurde), löst
`selectShareRow()` dort ein echtes `selectionChanged()` aus — und zu diesem
Zeitpunkt sind `m_parserMarketValues.isBusy()`/`m_parserDailyValues.isBusy()`
noch beide `false`. Der Busy-Guard griff in dieser Lücke nicht, wodurch
`enableShareActions` die drei Aktionen genau in dem Moment wieder freischaltete,
in dem `onRefreshShare()` sie eben erst deaktiviert hatte. Aufgedeckt durch
`test_onRefreshShare_busyGuard_selectionDuringRefreshDoesNotReenableActions`
(siehe TESTING.md). Behoben durch ein zusätzliches Flag `m_refreshInProgress`,
das in `startRefreshForShare()` **vor** `selectShareRow()` gesetzt und in
`finaliseRefresh()` zurückgesetzt wird; der Busy-Guard prüft jetzt
`m_refreshInProgress || m_parserMarketValues.isBusy() || m_parserDailyValues.isBusy()`.

#### Methode selectFirstShareRow() — Grid-Reset nach "Alle aktualisieren"

Selektiert Zeile 0 und ruft `scrollToTop()` in beiden Tabellen auf (No-Op falls
eine Tabelle leer ist). Wird ausschließlich aus `onRefreshShareFinished()`
aufgerufen, und zwar nur dann, wenn der komplette "Alle aktualisieren"-Lauf
**ohne Fehler** abgeschlossen wurde (siehe unten) — nicht nach einem
abgeschlossenen Einzel-Refresh und nicht im Fehlerfall. Im Fehlerfall bleibt
die Selektion bewusst auf der zuletzt von `selectShareRow()` markierten
(fehlgeschlagenen) Aktie stehen, damit sofort ersichtlich ist, welche Aktie
das Problem verursacht hat.

#### Methode buildDailyValuesUrl()

Spiegelt `Helper.BuildDailyValuesUrl()` aus dem C#-Referenzprojekt.
Berechnet die minimale Zeitfenster-URL auf Basis der letzten vorhandenen
`daily_values`-Einträge.

Die URL-Templates werden zunächst normalisiert: C#-Platzhalter `{0}` und `{1}`
werden in Qt-Platzhalter `%1` und `%2` konvertiert, und XML-kodierte `&amp;`
werden zu `&` aufgelöst (beide Formen können in der DB vorkommen, da die
Templates aus dem C#-Referenzprojekt übernommen wurden).

Kein Eintrag vorhanden → 5 Jahre History anfordern.
Einträge vorhanden → minimales Fenster nach Monatsdifferenz zu heute:

| Monatsdifferenz | OnVista-Code | Yahoo-Code |
| ------ | ------ | ------ |
| < 1 | M1 | 1mo |
| < 3 | M3 | 3mo |
| < 6 | M6 | 6mo |
| < 12 | Y1 | 1y |
| < 36 | Y3 | 3y |
| ≥ 36 | Y5 | 5y |

OnVista-Templates erwarten zwei Platzhalter: `%1` = ISO-Datum (`yyyy-MM-dd`),
`%2` = Periodencode. Yahoo-Templates erwarten einen: `%1` = Periodencode.

Dieselbe `&amp;`-Normalisierung wird auch auf `ShareObject::marketPriceUrl()`
angewendet, bevor die URL an den MarketValues-Parser übergeben wird.

#### Callback onMarketValuesUpdated()

Bei `ParserErrorCode::Finished`:

1. Kurs aus `state.searchResult["Price"]` extrahieren und mit `QLocale::c().toDouble()`
   nach `double` konvertieren — der Parser liefert Punkt als Dezimaltrenner (`"274.5000"`),
   unabhängig von der System-Locale.
2. `prevDayPrice` aus `DailyValuesRepository::findByShareAndDateRange()` —
   letzter Eintrag mit Datum < heute → `closingPrice()`.
3. `ShareRepository::updatePrice(guid, curPrice, prevDayPrice, now)` und
   `ShareRepository::updateLastInternetUpdate(guid, now)` in DB speichern.
4. Beide Grid-Zeilen (FinalValueTable + MarketValueTable) aktualisieren —
   Kurs- und Prozent-Performance-Spalten **sowie** die Entwicklungs-Icons
   (`PrevDayChart`, `CompleteChart`) über dieselbe lokale `devIcon`-Lambda,
   die auch `populatePortfolioTables()`/`updatePortfolioFooters()` verwenden
   (Bugfix 06.07.2026 — siehe unten).
5. Statusmeldung: `"Kurswert aktualisiert: <Name> — <Kurs>"`.
6. `m_marketDone = true` → wenn auch `m_dailyDone`, `onRefreshShareFinished()` aufrufen.

Bei Fehler (`lastErrorCode < NoError`): Fehlermeldung, `m_errorOccurred = true`,
`m_marketDone = true` — den DailyValues-Parser **nicht** abbrechen, er läuft
unabhängig weiter. Erst wenn beide fertig sind, wertet `onRefreshShareFinished()`
den Fehler aus und stoppt die Queue.

#### Callback onDailyValuesUpdated()

Bei `ParserErrorCode::Finished`:

1. `ShareRepository::updateLastInternetUpdate(guid, now)` in DB speichern
   (Feature 21.07.2026 — siehe @note unten).
2. `state.dailyValuesList` nach `QList<DailyValuesObject>` konvertieren
   (GUID aus `m_refreshShare.guid()`).
3. `DailyValuesRepository::upsertList(objects, &stats)` — Transaktion; jeder
   Datensatz wird gegen den bestehenden DB-Eintrag verglichen
   (`findByShareAndDate()` + `valuesEqual()`, Toleranz `1e-9`). Neue/geänderte
   Zeilen werden per `INSERT OR REPLACE` geschrieben, unveränderte Zeilen
   übersprungen (kein DB-Write). Rückgabe der Zähler in
   `DailyValuesRepository::UpsertStats` (`fetched`/`inserted`/`updated`/`unchanged`).
4. Statusmeldung: `"Tageswerte aktualisiert: <Name> — <N> Einträge geholt
   (Eingefügt: X / Aktualisiert: Y / Unverändert: Z)"`.
5. `m_dailyDone = true` → wenn auch `m_marketDone`, `onRefreshShareFinished()` aufrufen.

Bei Fehler: analog zu `onMarketValuesUpdated()` — `m_errorOccurred = true`,
`m_dailyDone = true`, MarketValues-Parser läuft unabhängig weiter.

@note **Feature (21.07.2026):** `updateLastInternetUpdate()` wurde bisher nur
von `onMarketValuesUpdated()` aufgerufen — ein reiner
`ShareUpdateType::DailyValues`-Refresh hinterließ dadurch nie eine Spur in
`shares.last_internet_update`, obwohl der Internet-Zugriff erfolgreich war.
`onDailyValuesUpdated()` ruft den Aufruf jetzt ebenfalls unbedingt bei
`Finished` auf — auch wenn `dvList` leer ist oder der nachfolgende Upsert
fehlschlägt, da bereits der erfolgreiche Internet-Abruf selbst zählt (gleiche
Semantik wie bei `onMarketValuesUpdated()`, das `updateLastInternetUpdate()`
ebenfalls unabhängig vom Erfolg der nachfolgenden `updatePrice()`-Persistenz
aufruft). Grund: `ShareRepository::maxLastInternetUpdate()` (siehe Abschnitt
"Portfolio-Label" unten) soll jede Art erfolgreicher Aktualisierung
widerspiegeln, nicht nur Kurswert-Refreshs.

@note Die OnVista-API liefert JSON-Keys in camelCase (`datetimeLast`, `first`,
`last`, `high`, `low`, `volume`). `OnVistaObjects::HistoryData::fromJson()` und
`RealTimeData::fromJson()` verwenden entsprechend camelCase-Keys — nicht PascalCase.

#### Methode onRefreshShareFinished() — Footer-Aktualisierung

Wird aufgerufen sobald sowohl `m_marketDone` als auch `m_dailyDone` für die
aktuelle Aktie `true` sind (siehe oben) — d.h. sobald die Aktie **vollständig**
angefragt wurde (Kurswert- und/oder Tageswerte-Parser, je nach
`ShareUpdateType`, sind durchgelaufen).

Ablauf bei Erfolg (`m_errorOccurred == false`):

1. `refreshPortfolioFooters()` — rekonstruiert `QList<ShareValues>` für
   **alle** Aktien frisch aus der DB (`ShareRepository::findAll()` +
   `ShareCalculator::compute()` je Aktie) und ruft damit
   `updatePortfolioFooters()` auf. Dadurch werden beide Footer (Depotwert-
   und Marktwert-Tab) sofort nach jeder einzelnen Aktie neu berechnet — nicht
   erst am Ende von "Alle aktualisieren". Direkt im Anschluss wird auch
   `updatePortfolioLabel(m_finalValueTable->rowCount(),
   formatLastPortfolioUpdate())` aufgerufen (Feature 21.07.2026), sodass die
   "Letzte Aktualisierung" im Portfolio-Label während "Alle aktualisieren"
   live nach jeder Aktie mitzieht statt erst nach einem Neuladen — siehe
   Abschnitt "Portfolio-Label" unten.
2. Im "Alle aktualisieren"-Modus (`m_updateAllFlag == true`) und solange die
   Queue nicht leer ist: nächste Aktie via `startRefreshForShare()` starten
   (welches wiederum `selectShareRow()` für die neue Aktie aufruft).
3. Andernfalls (Queue leer oder Einzel-Refresh): `m_updateAllFlag` wird
   **vor** dem Aufruf von `finaliseRefresh()` zwischengespeichert, da
   `finaliseRefresh()` es zurücksetzt. War es `true` — d.h. der komplette
   "Alle aktualisieren"-Lauf ist soeben ohne Fehler zu Ende gegangen — wird
   im Anschluss an `finaliseRefresh()` `selectFirstShareRow()` aufgerufen,
   sodass das Grid wieder die erste Aktie zeigt statt der zuletzt
   aktualisierten. War es `false` (abgeschlossener Einzel-Refresh), bleibt
   die Selektion unverändert auf der gerade aktualisierten Aktie stehen.
   Direkt im Anschluss wird `playUpdateFinishedSound()` aufgerufen (Feature
   21.07.2026) — genau an dieser Stelle, also einmal pro abgeschlossenem
   Lauf (Einzel-Refresh oder Ende von "Alle aktualisieren"), nie pro
   einzelner Aktie innerhalb der Queue. Da dieser gesamte Zweig nur bei
   `m_errorOccurred == false` erreicht wird, spielt der Sound ausschließlich
   bei erfolgreichem Abschluss.

Bei Fehler (`m_errorOccurred == true`) wird die Queue geleert und direkt
`finaliseRefresh()` aufgerufen — der Footer wird in diesem Fall **nicht**
aktualisiert, da die Aktie nicht vollständig (beide Parser fehlerfrei)
angefragt wurde. `selectFirstShareRow()` wird in diesem Zweig **nicht**
aufgerufen — die Selektion bleibt auf der Aktie stehen, die zuletzt via
`selectShareRow()` markiert wurde, also der Aktie, bei der der Fehler
auftrat. So ist sofort ersichtlich, welche Aktie das Problem verursacht hat.

`refreshPortfolioFooters()` ist bewusst von `updatePortfolioFooters()`
getrennt: Letztere nimmt eine bereits vorliegende `QList<ShareValues>`
entgegen (z.B. aus `populatePortfolioTables()`), Erstere lädt die Liste
für den Refresh-Flow eigenständig neu, da zum Zeitpunkt von
`onRefreshShareFinished()` keine vollständige Werteliste vorliegt — nur
die einzelne soeben aktualisierte Aktie ist bekannt (`m_refreshShare`), die
Footer-Summen sind aber portfolioweite Aggregate.

#### Methode onRefreshAll() — Alle aktualisieren

Lädt alle Shares via `ShareRepository::findAll()`, filtert `updateType() == None`
heraus und befüllt `m_refreshQueue`. Startet dann die erste Aktie via
`startRefreshForShare()`. Nach jedem vollständigen Abschluss einer Aktie
(beide Parser fertig) ruft `onRefreshShareFinished()` automatisch die nächste
aus der Queue ab.

Fehlerverhalten: Wenn mindestens ein Parser fehlschlug (`m_errorOccurred`),
wird die Queue in `onRefreshShareFinished()` geleert und die Aktualisierung
gestoppt — alle nachfolgenden Aktien werden nicht mehr abgerufen. Der jeweils
andere Parser der aktuellen Aktie wird dabei **nicht** abgebrochen, er darf
noch regulär abschließen.

Grid-Selektion: Jede Aktie der Queue wird beim Start via `selectShareRow()`
im Grid selektiert (siehe oben) — das Grid zeigt also fortlaufend, welche
Aktie gerade angefragt wird. Endet der Lauf erfolgreich (Queue leer, kein
Fehler), springt die Selektion via `selectFirstShareRow()` zurück auf die
erste Zeile. Bricht der Lauf wegen eines Fehlers ab, bleibt die Selektion
auf der fehlgeschlagenen Aktie stehen.

#### Methode onDeleteShare() — Aktie entfernen

Liest GUID und Name aus der selektierten Zeile (GUID via `Qt::UserRole` in Spalte 0).
Zeigt einen `OwnMessageBox::question`-Dialog mit dem Hinweis dass alle zugehörigen
Käufe, Verkäufe, Dividenden und Kosten ebenfalls gelöscht werden.
Bei Bestätigung delegiert der Slot an `ShareRepository::remove(guid)` — der Cascade-Delete
über Foreign Keys löscht alle verknüpften Datensätze automatisch.
Bei Fehler: `OwnMessageBox::critical` mit DB-Fehlermeldung.
Bei Erfolg: Tabellen neu laden, Aktionen deaktivieren, Statusmeldung + `qInfo()`-Log.

#### Methode createBackup(portfolioPath) — Backup-Erstellung

Private Hilfsmethode, aufgerufen an zwei Stellen — beim App-Start direkt nach
erfolgreichem Laden des Portfolios aus `AppSettings`, sowie in `onOpenPortfolio()`
direkt nach erfolgreichem `Database::instance().open()`.

Namensschema: `Backup_<Dateiname>_YYYY_MM_DD_HH_mm_ss.db` —
Beispiel: `ShareList.db` → `Backup_ShareList_2026_06_16_21_59_30.db`

Backup-Rotation: Maximal 5 Backups werden behalten. Der ISO-8601-Timestamp im Dateinamen
garantiert korrekte lexikographische Sortierung — älteste Datei = erster Eintrag.
Überzählige Backups werden nach erfolgreichem Kopieren gelöscht.

Threading: Der Kopiervorgang läuft in einem `QThread` via `BackupWorker` —
der UI-Thread bleibt während des gesamten Kopiervorgangs reaktionsfähig.
Der Benutzer sieht einen `%BackupProgressDialog` mit Fortschrittsbalken und Abbrechen-Button.
Bei Abbruch wird die unfertige Backup-Datei gelöscht.

---

### BackupProgressForm-Details

Besteht aus zwei Klassen: `BackupWorker` und `BackupProgressDialog`.

`%BackupWorker` ist eine `QObject`-Subklasse die via `moveToThread()` in einem `QThread`
läuft. Sie kopiert die Quelldatei chunkweise (512 KB pro Schritt) via `QFile::read/write`,
emittiert `progress(bytesWritten, totalBytes)` nach jedem Chunk sowie
`finished(success, message)` bei Abschluss, Fehler oder Abbruch.
Der `%cancel()`-Slot setzt `m_cancelled = true` — der nächste Chunk-Durchlauf bricht dann
ab und die unfertige Zieldatei wird gelöscht.

`%BackupProgressDialog` ist ein modaler `QDialog` der `%BackupWorker` im Konstruktor startet
und alle Signals/Slots verbindet. Der Fortschrittsbalken zeigt `X MB von Y MB kopiert...`.
Der Abbrechen-Button ruft `BackupWorker::cancel()` via `Qt::QueuedConnection` auf.
Der `×`-Button in der Titelleiste ist deaktiviert — Schließen nur über Abbrechen oder
Fertigstellen möglich. Der Dialog schließt sich automatisch 800 ms nach dem
`finished()`-Signal. `%wasSuccessful()` gibt das Ergebnis zurück an `%createBackup()`.

Thread-Lifecycle:

@code{.unparsed}
BackupProgressDialog::ctor
    → BackupWorker::moveToThread(m_thread)
    → m_thread->start()
    → BackupWorker::run()       (im Thread)
    → finished() Signal
    → QThread::quit()
    → QObject::deleteLater()    (Worker + Thread)
@endcode

Destruktor-Sicherheit (Race-Fix): `onFinished()` (setzt `m_success`) und
`QThread::quit()` hängen am selben `BackupWorker::finished()`-Signal, laufen
im GUI-Thread aber als getrennte, queued Events ab. `wasSuccessful() == true`
bedeutet daher **nicht**, dass der Worker-Thread bereits tatsächlich beendet
ist (`isRunning() == false`) — nur dass `quit()` angefordert wurde. Wird der
Dialog in genau diesem Zwischenfenster zerstört (z.B. unmittelbar nachdem ein
Aufrufer `wasSuccessful()` als `true` sieht), würde `~QObject()` versuchen,
das `m_thread`-Kindobjekt zu zerstören während es noch läuft —
`"QThread: Destroyed while thread is still running"`, im schlimmsten Fall ein
Absturz.

`%BackupProgressDialog::~BackupProgressDialog()` löst das defensiv: falls
`m_thread` noch lebt, wird `quit()` (idempotent) plus ein blockierendes
`wait()` ausgeführt, bevor die Basisklassen-Destruktoren laufen. `m_thread`
ist dafür bewusst als `QPointer<QThread>` statt als roher `QThread*`
deklariert — der Thread entsorgt sich nach `finished()` selbst per
`deleteLater()`, ein roher Zeiger wäre im (häufigen) regulären Fall, in dem
der Thread schon fertig ist, ein Dangling-Pointer. `QPointer` wird dabei
automatisch `nullptr`, sobald das Objekt zerstört wurde, wodurch
`if (m_thread)` im Destruktor immer sicher ist.

Diese Absicherung greift unabhängig vom Aufrufer — sowohl im echten
`createBackup()`-Flow als auch in Tests, die den Dialog vor dem
800-ms-Auto-Close zerstören.

---

### ShareEditForm-Details

Das ShareEditForm ist vollstaendig nach dem MVP-Pattern implementiert und wird ueber den Editieren-
Button in der Toolbar geoeffnet.

`IViewShareEdit` — Accessoren für alle editierbaren Felder, `loadShare()`,
`setFirstBuyDate()`, `setCurrentVolume()`, `setDailyValuesRequired()`,
`setTotalBuys/Sales/ProfitLoss/Dividends/Brokerages()`, `showError()`, `acceptAndClose()`.

`IModelShareEdit` — `loadShare()`, `saveShare()`, alle Aggregate, `currentVolume()`, `firstBuyDate()`.

`ModelShareEdit` — Delegiert an alle fünf Repositories.

`PresenterShareEdit` — Lädt Share + Aggregate im Konstruktor. Emittiert
`openBuysRequested`, `openSalesRequested`, `openDividendsRequested`, `openBrokeragesRequested`.
Slot `refreshSummary()` → `populateSummary()`. Entscheidet zusätzlich über die
Sperre der Update-Typ-Auswahl (`setDailyValuesRequired()`) und blockiert in
`validateInput()` das Speichern eines unzulässigen Update-Typs — siehe
"Erledigt / Archiv", "Tageswert-Historie bei Bestand > 0 erzwingen".

`ViewShareEdit` — Nimmt `DocumentsConfig*` (wird an `ViewBuyEdit` / `ViewSaleEdit`
weitergereicht). Zwei GroupBox-Bereiche: Allgemein + Einnahmen/Ausgabe.

#### Pencil-Buttons (Einnahmen / Ausgabe):

| Button | Aktion |
| ------ | ------ |
| Käufe | `ViewShareEdit::onEditBuys()` → öffnet `ViewBuyEdit` direkt |
| Verkäufe | `ViewShareEdit::onEditSales()` direkt |
| Dividenden | `ViewShareEdit::onEditDividends()` → öffnet `ViewDividendEdit` direkt |
| Kosten | `ViewShareEdit::onEditBrokerages()` → öffnet `ViewBrokerageEdit` direkt |

---

### ShareAddForm-Details

Das ShareAddForm ist vollstaendig nach dem MVP-Pattern implementiert.

`IViewShareAdd` — Eingabefelder als const-Accessoren, `setFieldOk()` / `setFieldError()` /
`onParseFinished()`, `setParseProgress()` / `setParseStatusIcon()` / `setUiBusy()`,
`hasMissingRequiredFields()` / `markMissingFieldsAsFailed()`, `showError()`, `acceptAndClose()`.

`IModelShareAdd` — `saveShareWithBuy()` (6 Parameter: Share, Buy, provision,
brokerFee, traderFee, reduction), `wknExists()`, `isinExists()`, `lastError()`.

`ModelShareAdd` — Speichert Share + Buy + Brokerage in einer SQLite-Transaktion.

`PresenterShareAdd` — Parse-Pipeline identisch zu `PresenterBuyEdit` (Referenzimplementierung).
Setzt `ShareUpdateType::Both` in `onSave()` explizit; der Dialog hat keine
Update-Typ-Auswahl, und eine neu angelegte Aktie hat wegen `volume() > 0` in
`validateInput()` immer Bestand — siehe "Erledigt / Archiv",
"Tageswert-Historie bei Bestand > 0 erzwingen".

`ViewShareAdd` — Linkes Formular in `QScrollArea` (700px) + rechte PDF-Vorschau.
Numerische Felder: `QLineEdit` mit `QDoubleValidator`.
Abgeleitete Werte via `textChanged` → `recalcDerivedValues()`.

Statuszeile in ViewShareAdd:
`setVisible(false/true)` — funktioniert ohne Layout-Shift weil das `formPanel` in einer
`QScrollArea` steckt die `setWidgetResizable(true)` hat.

---

### Gemeinsame Validierungskonvention (alle Forms mit Pflichtfeldern)

```
onSave() im Presenter:
    if (view->hasMissingRequiredFields(missing)) {
        view->markMissingFieldsAsFailed();  // SearchFailed-Icons auf leere Felder
        return tr("Es fehlen noch Pflichtangaben.\n"
                  "Die fehlenden Felder sind in der Maske rot markiert.");
    }
    // dann fachliche Duplikat-Checks …
```

`QMessageBox::critical()` für alle Fehlermeldungen (nicht `warning`).

---

## Datenbankschema (SQLite)

@code{.unparsed}
shares          ← Stammdaten je Aktie (GUID, WKN, ISIN, Name, ...)
  │
  ├── buys              ← Kauftransaktionen
  ├── sales             ← Verkaufstransaktionen
  │   └── sale_buy_details  ← Beteiligte Käufe je Verkauf
  ├── brokerage         ← Gebühren (verknüpft mit buy oder sale)
  ├── dividends         ← Dividendenzahlungen
  ├── daily_values      ← Historische Kursdaten (OHLCV)
  └── share_splits      ← Aktiensplits (Datum, Verhältnis, Beleg), siehe "ShareSplitObject /
                           ShareSplitRepository / ShareSplitAdjuster" unten
@endcode

Alle Tabellen verwenden `TEXT`-GUIDs als Primärschlüssel. Foreign Keys aktiviert
(`PRAGMA foreign_keys = ON`), WAL-Modus aktiv (`PRAGMA journal_mode = WAL`).

### Schema-Migration bestehender Portfolios (08.08.2026)

`Database::open()` ruft nacheinander `createSchema()` und `migrateSchema()` auf.
Die Aufteilung hat einen konkreten Grund.

`createSchema()` arbeitet durchgehend mit `CREATE TABLE IF NOT EXISTS`. Eine
komplett NEUE Tabelle kommt dadurch von selbst in bestehende Portfolios — genau
so ist `share_splits` beim ersten Öffnen nach Phase 1 der Aktiensplit-Behandlung
entstanden, ohne dass jemand etwas dafür tun musste. Bis dahin fiel nie auf,
dass dieselbe Anweisung eine neue SPALTE in einer bereits vorhandenen Tabelle
NICHT nachzieht: SQLite sieht die Tabelle, vergleicht die Spaltenliste nicht
und tut nichts. Jede Tabelle hatte ihre Spalten bis dahin von Geburt an.

Mit `share_splits.document` trat dieser Fall zum ersten Mal auf. `migrateSchema()`
schliesst die Lücke:

```cpp
static const ColumnMigration migrations[] = {
    { "share_splits", "document", "TEXT" },
};
```

`ensureColumn(table, column, definition)` liest `PRAGMA table_info(<table>)` und
setzt bei fehlender Spalte ein `ALTER TABLE … ADD COLUMN` ab. Eine nachgerüstete
Spalte ist damit eine Zeile in dieser Tabelle.

Bewusst OHNE Versionszähler in der Datenbank (etwa `PRAGMA user_version`): die
Prüfung "existiert die Spalte?" ist idempotent, braucht keinen zusätzlichen
Zustand und kann auch dann nicht aus dem Tritt geraten, wenn ein Portfolio eine
oder mehrere Versionen übersprungen hat. Ein Zähler müsste dagegen bei jedem
Sprung korrekt fortgeschrieben werden und wäre bei einem von Hand bearbeiteten
oder aus einem Backup zurückgespielten Portfolio unzuverlässig.

@note Was `ensureColumn()` NICHT kann: Spalten umbenennen, Typen oder
Constraints ändern, Spalten entfernen. SQLite verlangt dafür den Umweg über
eine Ersatztabelle (neu anlegen, kopieren, alte löschen, umbenennen). Das ist
hier bewusst nicht vorgesehen, solange es keinen konkreten Anlass gibt — die
Ersatztabellen-Variante ist deutlich fehleranfälliger und will einzeln
durchdacht sein. Ebenfalls von SQLite vorgegeben: `ADD COLUMN` darf weder
`PRIMARY KEY` noch `UNIQUE` enthalten und bei `NOT NULL` keinen Default
vermissen lassen.

---

## Repository-Schicht

Alle 7 Repositories sind implementiert: `ShareRepository`, `BuyRepository`,
`SaleRepository`, `DividendRepository`, `BrokerageRepository`, `DailyValuesRepository`,
`ShareSplitRepository`.

Verbindungsregel: Immer `QSqlDatabase::database(Database::connectionName())` —
niemals `QSqlDatabase::database()` ohne Argument (gibt ungültige Default-Verbindung).

### ShareSplitObject / ShareSplitRepository / ShareSplitAdjuster (Aktiensplit-Behandlung, Phase 1: 07.08.2026, Phase 2a/2b/2c: 07.08.2026)

Grundlage für den offenen Punkt "Aktiensplits werden nicht behandelt" (siehe
"Offene Punkte" unten). Phase 1 legte reines Datenmodell und Rechenkern ohne
jede sichtbare Änderung an; Phase 2a wendet den Rechenkern in
`ShareCalculator` an (siehe eigenen Absatz unten), Phase 2b in den
Chart-Modellen `ModelPortfolioChart`/`ModelChart` (ebenfalls eigener Absatz
unten), Phase 2c in der FIFO-Verkaufszuteilung (`SaleFifoAllocator`,
ebenfalls eigener Absatz unten) — damit ist die Umrechnung an allen Stellen
angewendet, an denen Käufe, Verkäufe oder Tageswerte in Berechnungen
einfliessen. Phase 3a ergänzte die Erfassungsmaske (`ShareSplitsForm`, siehe
eigenen Abschnitt unten). Offen bleiben Phase 3b (Split-Hinweis in den
Editier-Dialogen für Käufe und Verkäufe) und Phase 4 (automatische
Nachprüfung des `prices_adjusted`-Zustands).

`share_splits` (Schema siehe oben): eigene GUID je Split, wie bei
`BuyObject`/`BrokerageObject` — nicht wie bei `DailyValuesObject` mit
zusammengesetztem Primärschlüssel — plus `UNIQUE(share_guid, date)`, da zwei
Splits derselben Aktie am selben Tag fachlich keinen Sinn ergeben.
`ratio_new`/`ratio_old` bilden das Verhältnis ab (20:1-Split →
`ratio_new=20, ratio_old=1`; Reverse-Split 1:10 → `ratio_new=1,
ratio_old=10`). `prices_adjusted` ist je Split gesetzt (Nessies Entscheidung
07.08.2026), nicht je Aktie — bei mehreren Splits derselben Aktie kann die
Kurshistorie unterschiedlich weit bereinigt vorliegen, je nachdem wann und
mit welchem Anbieter sie zuletzt abgerufen wurde.

`ShareSplitObject::factor()` = `ratioNew / ratioOld`. `ShareSplitRepository`
folgt exakt dem Muster von `BrokerageRepository`/`DailyValuesRepository`
(`findByShare()` sortiert nach Datum aufsteigend, `insert()`/`update()`/
`remove()`/`removeByShare()`, `lastError()`), ergänzt um `existsForDate()`
als Vorgriff auf die künftige Split-Erfassungsmaske (Phase 3).

`ShareSplitAdjuster` (`app/utils`, zustandslos, vollständig datenbankfrei —
gleicher Stil wie `PortfolioSeriesCalculator`) ist die zentrale
Umrechnungslogik:

- `volumeFactor(splits, date)` — Produkt aller `factor()` von Splits mit
  einem Datum ECHT NACH `date`. Gilt für Transaktionen (`buys`/`sales`), die
  immer in der Beleg-Skala vorliegen.
- `priceFactorForHistory(splits, date)` — dieselbe Kumulierung, aber nur über
  Splits mit `pricesAdjusted() == false`. Ein bereits bereinigter Split zeigt
  in der Kurshistorie keinen Sprung mehr und darf daher nicht zusätzlich
  herausgerechnet werden.
- `adjustedVolume()`, `adjustedTransactionPrice()`, `adjustedHistoryPrice()`
  als dünne Hüllen um die beiden Faktor-Funktionen.

@note **Grundinvariante:** `Stückzahl × Preis` bleibt über einen Split hinweg
exakt gleich — ein Split schafft weder Gewinn noch Verlust, er zerlegt oder
bündelt nur die Stückelung. Einzahlung, Kaufwert, Verkaufserlös, Gebühren,
Steuern und Dividendensummen bleiben deshalb von einem Split gänzlich
unberührt; nur Stückzahl und Kurs je Stück werden umgerechnet. Die
Datenbank speichert dabei durchgehend die Beleg-Wahrheit (Nessies
Entscheidung 07.08.2026) — `buys`/`sales`/`daily_values` werden bei einem
erfassten Split NICHT physisch umgeschrieben, die Umrechnung passiert
ausschliesslich zur Laufzeit. Editier-Dialoge zeigen deshalb künftig immer
den Beleg mit einem Hinweis auf den Splittag ("Split 20:1 am 18.07.2022 —
entspricht 100 Stück à 50,15 €"); Grid, Charts und Detailansicht zeigen
bereinigt.

#### Anwendung in ShareCalculator (Phase 2a, 07.08.2026)

`ShareCalculator::compute()` lädt die Splits der Aktie zusätzlich über ein
eigenes `ShareSplitRepository`-Member und rechnet `volume`, `volumeSold` und
`price` jedes Kaufs bzw. jedes Verkaufs vor jeder weiteren Berechnung über
`ShareSplitAdjuster` auf die heutige Skala um — `curPrice`/`prevDayPrice`
sind bereits heutige Skala (Live-Kurs), `curValue` braucht also einen ebenso
umgerechneten Bestand. `volume` und `volumeSold` eines Kaufs werden mit
demselben, vom Kaufdatum abhängigen Faktor skaliert, wodurch der Anteil
`remVol/volume` — und damit die bestehende Pro-Lot-Zuordnung von
Brokerage/Rabatt aus "SalesForm-Details"/"BuysForm-Details" — unverändert
bleibt. Brokerage, Rabatt und Steuern sind reine Geldbeträge und werden
nicht skaliert.

Ohne gespeicherte Splits liefert `ShareSplitAdjuster` überall den Faktor
1,0; Division/Multiplikation mit 1,0 ist in IEEE 754 bitgenau, das Verhalten
ist für alle bestehenden Portfolios also exakt identisch zum Stand vor
dieser Änderung — bestätigt durch die vollständig unveränderten
Bestandstests in `tst_sharecalculator.cpp`.

#### Anwendung in ModelPortfolioChart / ModelChart (Phase 2b, 07.08.2026)

Beide Modelle bekommen ein eigenes `ShareSplitRepository`-Member, exakt wie
`ShareCalculator` in Phase 2a. Die Umrechnung passiert ausschliesslich
innerhalb der Modelle — an `IModelChart`/`IModelPortfolioChart` ändert sich
nichts, der Presenter erwartet ohnehin fertige, korrekte Werte und weiss
nichts von Splits. `PortfolioSeriesCalculator` bleibt dadurch vollständig
unangetastet: er bekommt bereits split-bereinigte `PortfolioBuyEvent`/
`PortfolioSaleEvent`/`PortfolioPriceEvent`-Werte herein und bleibt so
zustandslos und split-unbewusst, wie er ohnehin schon dokumentiert ist
("Das Laden übernimmt ModelPortfolioChart").

`ModelPortfolioChart::loadPortfolioInput()`: pro Aktie werden die Splits
einmal geladen, dann `PortfolioBuyEvent`/`PortfolioSaleEvent` über
`ShareSplitAdjuster::adjustedVolume()`/`adjustedTransactionPrice()`
umgerechnet (Datum = jeweiliges Kauf-/Verkaufsdatum) und
`PortfolioPriceEvent` über `adjustedHistoryPrice()`. Dividenden
(`payoutWithTaxes`) und Kosten (`brokerageReduction`) sind Geldbeträge und
bleiben unangetastet.

`ModelChart`: `heldVolumeSeries()` rechnet jeden Kauf/Verkauf vor der
Summierung um — sonst genau der ursprüngliche Alphabet-Bug (Bestand springt
am Splittag). `latestBuy()`/`latestSale()` und `buysInRange()`/
`salesInRange()` rechnen den Kaufpreis (und bei den Range-Varianten auch die
im Hover-Tooltip gezeigte Stückzahl) um. `loadDailyValues()` gibt für jeden
Tageswert ein neu konstruiertes `DailyValuesObject` mit über
`adjustedHistoryPrice()` umgerechneten OHLC-Werten zurück.

@note **Randentscheidung zum Handelsvolumen:** `daily_values.volume`
("Gehandelte Anteile", eigene Chart-Serie) wird mit demselben
`priceFactorForHistory`-Faktor umgerechnet wie die Kurse — Annahme:
Handelsvolumen kommt vom selben Datenfeed wie die Kurse und hat denselben
Bereinigungszustand. Es gibt dafür kein eigenes `prices_adjusted`-Flag.
Ohne diese Umrechnung stünde am Splittag ein Sprung im Volumen-Chart, der
optisch genau das Symptom wäre, das diese Funktion beheben soll.

Ohne gespeicherte Splits ist das Verhalten beider Modelle bitgenau identisch
zum Stand vor dieser Änderung (Faktor 1,0 überall).

**Testabdeckung bewusst asymmetrisch:** `ModelPortfolioChart` hat eine neue
Testklasse `TestModelPortfolioChart` in `tst_portfoliochartform.cpp`
bekommen, die gegen eine echte In-Memory-SQLite-Datenbank läuft (die
Link-Abhängigkeiten für Qt6::Sql/Database waren dort ohnehin schon vorhanden,
siehe "Build-Struktur"). Für `ModelChart` wurde bewusst darauf verzichtet:
`tst_chartform` ist als DB-freies Ziel angelegt (nur Fakes, kein Qt6::Sql,
keine Database-Bibliothek), und eine echte DB-Testabdeckung hätte diese
Trennung für das gesamte Ziel aufgehoben. Die Split-Umrechnung in
`ModelChart` nutzt ausschliesslich dieselben, bereits unabhängig getesteten
`ShareSplitAdjuster`-Funktionen nach demselben, in `ModelPortfolioChart`
bereits DB-getesteten Muster — eine eigene DB-Testabdeckung für `ModelChart`
wäre ein sinnvoller eigener Testziel-Zuschnitt (analog `tst_sharecalculator`),
aber ein separater Schritt.

#### SaleFifoAllocator und die FIFO-Verkaufszuteilung (Phase 2c, 07.08.2026)

`app/utils/SaleFifoAllocator.h` — zustandslos, vollständig datenbankfrei,
gleicher Stil wie `ShareSplitAdjuster`/`PortfolioSeriesCalculator`. Ersetzt
die vormals dreifach duplizierte FIFO-Zuteilungsschleife in
`PresenterSaleEdit::onSave()`, `PresenterSaleEdit::refreshDerivedValues()`
(Live-Vorschau) und `ViewSaleEdit::onShowDetails()` (Details-Dialog).

`SaleFifoAllocator::allocate(saleVolume, saleDate, availableBuysOldestFirst,
splits)` rechnet Verkaufsmenge und jede Kauf-Restmenge intern über
`ShareSplitAdjuster::adjustedVolume()` auf die heutige Skala um (die
gemeinsame Vergleichsbasis, unabhängig davon, wie viele Splits zwischen
Kauf- und Verkaufsdatum liegen), und rechnet das je Kauf zugeteilte Stück
über die neue Umkehrfunktion `ShareSplitAdjuster::belegVolume()` wieder
zurück auf DESSEN EIGENE Beleg-Skala. `FifoAllocationRow.volume` liegt damit
konsequent in der Beleg-Skala des referenzierten Kaufs — passt direkt zu
`buyPrice` (`buy.price()`, unverändert) und `ModelSaleEdit::addSale()`/
`updateSale()`/`removeSale()` können dadurch **unverändert** bleiben
(`buy.volumeSold() += detail.volume()` bzw. `-=` rechnet weiterhin richtig,
egal ob ein Split zwischen Kauf und Verkauf liegt).

Zwei neue `IModelSaleEdit`-Methoden versorgen die Zuteilung mit ihren
Eingaben:

- `loadSplits(shareGuid)` — reiner Passthrough zu `ShareSplitRepository`.
- `loadAvailableBuysForDepotExcludingSale(shareGuid, depotNumber,
  excludeSaleGuid)` — wie `loadAvailableBuysForDepot()`, bucht aber die
  Anteile von `excludeSaleGuid` vorher virtuell zurück. Notwendig, weil
  `buy.volumeSold()` in der Datenbank bis zum tatsächlichen Speichern noch
  den ALTEN Verkauf widerspiegelt — ohne die Rückbuchung würde eine
  FIFO-Neuberechnung beim Bearbeiten gegen einen künstlich verkleinerten
  Bestand rechnen. Ein Kauf, den der bearbeitete Verkauf bereits
  vollständig aufgebraucht hat und der deshalb NICHT in der
  "verfügbar"-Liste steht, wird mit der zurückgebuchten Restmenge wieder
  aufgenommen — sonst würde er bei der Neuzuteilung fälschlich fehlen.

@note **Bugfix mit erledigt (Nessies Entscheidung 07.08.2026):**
`PresenterSaleEdit::onSave()` übernahm beim Bearbeiten des jüngsten
Verkaufs bisher unverändert die gespeicherten `SaleBuyDetails`, selbst wenn
sich die Verkaufsmenge im Formular geändert hatte — ein von Splits
unabhängiger, vorbestehender Bug. Die FIFO-Zuteilung wird jetzt in jedem
Fall frisch berechnet (`onSave()`, `refreshDerivedValues()` als Live-
Vorschau, `ViewSaleEdit::onShowDetails()`), sobald der bearbeitete Verkauf
der jüngste ist. Für ältere, nicht editierbare Verkäufe bleibt es bei der
gespeicherten Zuteilung, da deren Felder ohnehin gesperrt sind.

`ViewSaleEdit::onShowDetails()` zeigt seither durchgängig auf heutiger
(split-bereinigter) Skala an — dieser Dialog ist eine berechnete Übersicht
über ggf. mehrere Lots, keine Beleg-Abschrift, und nur so bleiben Summen
über mehrere Lots hinweg sinnvoll, auch wenn ein Split zwischen zwei Lots
liegt. `IViewSaleEdit::setSplits()` versorgt die View einmalig (im
Presenter-Konstruktor, analog `setAllBuys()`) mit den Splits der Aktie.

---

## Konfiguration

### AppSettings (app/config/AppSettings.h/.cpp)

INI-Datei neben der Executable. Singleton `AppSettings::instance()`.

### WebSitesConfig / DocumentsConfig

XML-Konfigurationsdateien (`WebSites.xml`, `Documents.xml`) neben der Executable.
`DocumentsConfig` wird beim Start in `MainWindow` geladen (`m_documentsConfig`) und
als Zeiger an `ViewShareEdit` → `ViewBuyEdit` → `PresenterBuyEdit` sowie
`ViewShareEdit` → `ViewSaleEdit` → `PresenterSaleEdit` weitergereicht.
`ViewDividendEdit` nimmt ebenfalls `DocumentsConfig*` entgegen und leitet ihn an
`PresenterDividendEdit` weiter.

---

## Mehrsprachigkeit (i18n)

@code{.unparsed}
tr("Text") im Code → lupdate → .ts-Dateien → Qt Linguist → lrelease → .qm
@endcode

Sprache ohne Neubuild änderbar durch Austausch der `.qm`-Datei.

---

### OverviewTabWidget-Details (Gewinne/Verluste-, Dividenden-, Kosten-Tabs, implementiert 13.07.2026, fixierter Übersicht-Tab 14.07.2026, Gewinne/Verluste auf Marktwert-Modus erweitert 14.07.2026)

Wiederverwendbares "Übersicht + Jahres-Tabs"-Anzeige-Widget (siehe
`app/widgets/OverviewTabWidget.h`), verwendet für die Gewinne/Verluste-,
Dividenden- und Kosten-Tabs in `ViewShareDetails` — je eine reine
Anzeige-Instanz ohne Verbindung zu einem Editier-Formular. Gewinne/Verluste
existiert in beiden Modi (Marktwert-Modus seit 14.07.2026, siehe
"Marktwert- vs. Depotwert-Modus" oben), Dividenden und Kosten bleiben
Depotwert-only. Die Jahres-Gruppierung/-Summierung liegt bewusst in der
View, identisch zum Muster in den Editier-Dialogen
(`ViewBuyEdit`/`ViewSaleEdit`/`ViewDividendEdit`, die weiterhin ihre eigene,
lokale Kopie des Musters haben — die vollständige Umstellung dieser Dialoge
auf `OverviewTabWidget` ist noch offen, siehe unten).

@note **Fixierter Übersicht-Tab (14.07.2026, auf Nessies Vorgabe):** Bei
vielen Jahren an Historie zeigte ein einzelnes `QTabWidget` Scroll-Pfeile an,
sobald die Tab-Leiste zu breit wurde — dabei konnte der Übersicht-Tab
(bisher Index 0) mit aus dem Sichtbereich scrollen. `QTabWidget`/`QTabBar`
unterstützen "angepinnte" Tabs nicht nativ. `OverviewTabWidget` baut daher
intern kein `QTabWidget` mehr auf, sondern zwei nebeneinanderliegende
`QTabBar`s (`m_pinnedBar` mit dem einzelnen, nie scrollenden "Übersicht"-Tab;
`m_yearsBar` mit den Jahres-Tabs, scrollt bei Bedarf) über einem gemeinsamen
`QStackedWidget` (`m_stack`), getrennt durch einen schmalen
`QFrame::VLine`-Separator. Nach außen bildet `count()`/`widget(int)`/
`tabText(int)`/`currentIndex()`/`setCurrentIndex(int)` weiterhin einen
einzigen, durchgehenden Index ab (0 = Übersicht, 1..n = Jahre in
Aufbau-Reihenfolge) — diese Methoden ersetzen die bisherige `tabWidget()`-
Methode, da kein internes `QTabWidget` mehr existiert, auf das sie zeigen
könnte. `m_tabYears` und die Klick-Navigation
(`onUebersichtRowActivated()`/`onJahresRowActivated()`) sind unverändert.
Bewusst vereinfacht: Ist der Übersicht-Tab aktiv, zeigt `m_yearsBar`
weiterhin seinen zuletzt gewählten Jahres-Tab optisch als "selektiert" an
(native `QTabBar` kennt keinen "keiner ausgewählt"-Zustand) — der Separator
macht die beiden Gruppen aber klar erkennbar; bei Bedarf nach visuellem
Review noch verfeinerbar. `OverviewTabWidget` hat seit 14.07.2026 ein eigenes
Test-Target, `tst_overviewtabwidget` (siehe TESTING.md).

@note **Bugfixes nach erstem Build (14.07.2026, Nessies Feedback):**
- Übersicht-Tab nicht mehr anwählbar: `m_pinnedBar` hat nur genau einen
  Tab (Index 0) — dessen `currentIndex` ändert sich also nie, wodurch
  `QTabBar::currentChanged` bei einem Klick auf einen bereits (intern) als
  "aktuell" geltenden Tab nicht feuert. Nach einem Sprung in einen Jahres-Tab
  (z.B. per Klick auf eine Übersicht-Zeile) ließ sich der Übersicht-Tab
  dadurch nicht mehr zurück anwählen, derselbe Effekt drohte spiegelbildlich
  in `m_yearsBar`. Fix: beide `QTabBar`s auf `QTabBar::tabBarClicked(int)`
  statt `currentChanged(int)` umgestellt — feuert bei jedem tatsächlichen
  Klick, unabhängig vom internen Indexstand der jeweiligen Bar. Die Klick-
  Handler (`onPinnedBarClicked()`/`onYearsBarClicked()`) rufen direkt
  `setCurrentIndex()` auf, die Stack und beide Bars synchron hält;
  `setCurrentIndex()` selbst konnte dadurch vereinfacht werden (der
  ursprüngliche `m_suppressTabSignal`-Tanz um die Bar-`setCurrentIndex()`-
  Aufrufe war nur zur Rekursionsvermeidung bei `currentChanged` nötig,
  `tabBarClicked` feuert nicht bei programmatischen Änderungen).
- Spaltenköpfe erst bei Selektion fett: `buildFrozenTable()` setzte
  Fettschrift bisher nur auf die Footer-Zeile — die als "erst bei Selektion
  fett" wahrgenommene Kopfzeile kam von Qt's Style-Standardverhalten
  (`QHeaderView::highlightSections`, hebt die zur Selektion gehörige
  Kopfspalte hervor). Fix: `data->horizontalHeader()->setFont(...)` (fett)
  direkt beim Tabellenaufbau gesetzt sowie `setHighlightSections(false)`,
  sodass die Spaltenköpfe unabhängig von jeder Selektion immer fett
  erscheinen.

@note **Reset auf Jahresübersicht bei äußerem Tab-Wechsel (14.07.2026,
Nessies Vorgabe):** Verließ man z.B. den Gewinne/Verluste-Tab mit einem
gewählten Jahres-Tab und kehrte später zurück, blieb bislang der zuletzt
gewählte Jahres-Tab sichtbar statt der Übersicht. Der Reset gehört bewusst
**nicht** in `OverviewTabWidget` selbst (das kennt seinen Einbettungskontext
nicht), sondern in `ViewShareDetails`: `ViewShareDetails::onMainTabChanged()`
ist mit dem `currentChanged`-Signal des äußeren `m_tabs` (Aktien-Chart/
Depotwert/Gewinne-Verluste/Dividenden/Kosten) verbunden und ruft bei jedem
Wechsel `setCurrentIndex(0)` auf allen drei vorhandenen Instanzen
(`m_gewinneVerlusteTab`/`m_dividendenTab`/`m_kostenTab`) auf — bewusst immer
alle drei statt nur der neu aktiven, das ist einfacher als Index-Tracking pro
Tab und funktional gleichwertig (der Reset passiert je nachdem beim Verlassen
oder beim Betreten, in jedem Fall aber bevor der Tab wieder sichtbar wird).

@note **BuysForm auf OverviewTabWidget/DocumentPreviewPanel umgestellt
(16.07.2026):** `ViewBuyEdit` nutzt für die Kauf-Übersicht `OverviewTabWidget`
statt einer lokalen Kopie des `buildFrozenTable()`-Musters, und für die
PDF-Vorschau `DocumentPreviewPanel` statt eigenem QPdfView/pdftoppm-Code.
`createPreviewPanel()` instanziiert nur noch `DocumentPreviewPanel`,
`openPdfPreview()`/`clearPdfPreview()` sind reine Weiterleitungen an
`showDocument()`/`clearDocument()`. `createOverviewGroup()` verdrahtet drei
Signale: `rowActivated()` (Zeilenklick im Jahres-Tab → Kauf laden),
`currentTabChanged()` (Tab-Wechsel → Übersicht: Formular zurücksetzen,
Jahres-Tab: erste Zeile automatisch laden — ersetzt das bisherige
`QTabWidget::currentChanged`) und `documentActivated()` (Doppelklick
Dokument-Spalte → Vorschau aktualisieren, neu). Die Dokument-Spalte bleibt
bewusst Stretch statt fester Breite (ShareEdit-Dialoge haben ein schmaleres
Grid als ShareDetailsForm, ein fixer 110px-Wert passt hier nicht) — nur der
Spaltenindex wird als `jahresDocColumn` übergeben, damit der Doppelklick
funktioniert.

Dafür wurde `OverviewTabWidget` um das Signal `currentTabChanged(int index)`
erweitert, gefeuert am Ende von `setCurrentIndex()` (außer während
`populateOverview()`/`clear()`). Grund: anzeigende Kontexte
(`ViewShareDetails`) brauchen kein Feedback bei Tab-Wechsel, Editier-Dialoge
aber schon (Formular-Reset bzw. Auto-Laden der ersten Zeile). Das Signal
bleibt in `ViewShareDetails` unverbunden.

@note **Bugfix nach erstem Build (16.07.2026):** `ViewBuyEdit::setupUi()` rief
bisher `createOverviewGroup()` vor `createPreviewPanel()` auf — `m_previewPanel`
war zum Zeitpunkt des `documentActivated`-Connects in `createOverviewGroup()`
noch `nullptr` (`connect(..., Unknown): invalid nullptr parameter`). Fix:
`createPreviewPanel()` wird jetzt zuerst aufgerufen (Widget aber erst am Ende
ins Layout eingefügt, damit die sichtbare Reihenfolge Formular/Vorschau
unverändert bleibt).

@note **SalesForm auf OverviewTabWidget/DocumentPreviewPanel umgestellt
(16.07.2026):** `ViewSaleEdit` 1:1 nach demselben Muster wie `ViewBuyEdit`
umgebaut — inkl. des Bugfixes (`createPreviewPanel()` vor
`createOverviewGroup()`) gleich mit übernommen, da sonst derselbe
Nullptr-Connect-Fehler aufgetreten wäre. `populateOverview()` delegiert jetzt
an `OverviewTabWidget::populateOverview()` statt einer lokalen
`buildFrozenTable()`-Kopie; Spalten/Icon-Logik der Jahres-Tabs
(Datum/Anteile/Auszahlung/Gewinn-Verlust/Dokument) unverändert übernommen.
Die beiden alten Klick-Slots `onOverviewRowActivated()`/
`onUebersichtRowActivated()` entfallen ersatzlos — das Verhalten (Klick auf
Jahres-Zeile lädt Verkauf, Klick auf Übersicht-Zeile springt zum Jahres-Tab)
übernimmt `OverviewTabWidget` intern.

`onShowDetails()` (FIFO-Kaufzuteilungs-Dialog, siehe "SalesForm-Details" oben)
ist bewusst **nicht** Teil dieser Migration — die dort eingebettete,
eigenständige PDF-Vorschau ist lokal auf den Details-Dialog beschränkt und
unabhängig vom rechten Hauptvorschau-Panel. `ViewSaleEdit.h`/`.cpp` behalten
daher weiterhin das `#ifdef SPM_HAVE_QTPDF`-Include für `QPdfView`/
`QPdfDocument`.

@note **Bugfix Dokument-Spaltenbreite (16.07.2026, Nessies Feedback nach
erstem Build):** Die Verkaufs-Übersicht hat nur 4 Stretch-Spalten (Anteile/
Auszahlung/Gewinn-Verlust/Dokument) gegenüber 5 in der Kauf-Übersicht
(Anteile/Kurswert/Gebühren/Einzahlung/Dokument) — bei gleicher Stretch-
Behandlung der Dokument-Spalte (`-1`) verteilt sich dieselbe Restbreite auf
weniger Spalten, wodurch die Dokument-Spalte in `ViewSaleEdit` optisch
breiter ausfiel als in `ViewBuyEdit`. Fix: Dokument-Spalte in
`ViewSaleEdit::populateOverview()` fest auf `kDocColWidth = 120` (statt
`-1`/Stretch) gesetzt — angenähert an die Kauf-Übersicht-Breite bei
gleicher Dialoggröße (1300×820, 3:2-Aufteilung Formular/Vorschau). Der
Wert ist eine Annäherung ohne Live-Rendering geprüft; bei Bedarf einfach
`kDocColWidth` anpassen.

`DividendForm`/`BrokeragesForm` sowie `ViewShareAdd` (nur
`DocumentPreviewPanel`, kein Übersicht-Tab dort) folgen in den nächsten
Schritten mit demselben Muster.

@note **DividendForm auf OverviewTabWidget/DocumentPreviewPanel umgestellt
(16.07.2026):** `ViewDividendEdit` 1:1 nach demselben Muster wie
`ViewBuyEdit`/`ViewSaleEdit` umgebaut, inkl. des Bugfixes
(`createPreviewPanel()` vor `createOverviewGroup()`) von Anfang an.
`populateOverview()` delegiert jetzt an `OverviewTabWidget::populateOverview()`
statt einer lokalen `buildFrozenTable()`-Kopie; Spalten/Icon-Logik der
Jahres-Tabs (Datum/Dividendensatz/Anteile/Dividende/Dokument) unverändert
übernommen. Die beiden alten Klick-Slots `onOverviewRowActivated()`/
`onUebersichtRowActivated()` entfallen ersatzlos — übernimmt
`OverviewTabWidget` intern.

Einzige inhaltliche Änderung: die Dokument-Spalte der Jahres-Tabs war zuvor
fest auf `36`px (reine Icon-Spalte) — auf Nessies Entscheidung jetzt auf
Stretch (`-1`) umgestellt, konsistent zu `ViewBuyEdit`. Das ist ein bewusster
Zwischenstand, kein Zielzustand — siehe "Dokument-Spalten: Breite
verkleinern + Header" unten, dort auch die von Nessie vorgegebene
Reihenfolge (erst DividendForm und BrokeragesForm fertig umbauen, dann
global vereinheitlichen).

Da bisher kein Doppelklick auf die Dokument-Spalte existierte (keine
eingebettete Vorschau, nur das rechte Hauptpanel bei Zeilenauswahl), fehlte
bislang `iDoc->setData(Qt::UserRole, d.document())` auf dem Dokument-Item —
ohne das liest `OverviewTabWidget::documentActivated()` einen leeren Pfad.
Beim Umbau ergänzt, analog zu `ViewSaleEdit`.

@note **BrokeragesForm auf OverviewTabWidget/DocumentPreviewPanel umgestellt
(16.07.2026):** `ViewBrokerageEdit` 1:1 nach demselben Muster wie
`ViewBuyEdit`/`ViewSaleEdit`/`ViewDividendEdit` umgebaut, inkl. des Bugfixes
(`createPreviewPanel()` vor `createOverviewGroup()`) von Anfang an.
`populateOverview()` delegiert jetzt an `OverviewTabWidget::populateOverview()`
statt einer lokalen `QTabWidget`-/`buildFrozenTable()`-Kopie; Spalten-/Icon-
Logik der Jahres-Tabs (Datum/Typ/Ges. Gebühren/Rabatt/Netto-Kosten/Dokument,
inkl. der Typ-Spalte Kauf/Verkauf/Sonstig) unverändert übernommen. Die beiden
alten Klick-Slots `onOverviewRowActivated()`/`onUebersichtRowActivated()`
entfallen ersatzlos — übernimmt `OverviewTabWidget` intern.

Abweichend von `ViewDividendEdit` (dort Stretch als bewusster Zwischenstand)
ist die Dokument-Spalte hier von Anfang an fest auf `kDocColWidth = 120`
gesetzt — analog zum Bugfix in `ViewSaleEdit`, auf Nessies direkte Vorgabe.
Grund: die Kosten-Übersicht hat nur 4 Stretch-Spalten (Typ/Ges. Gebühren/
Rabatt/Netto-Kosten), bei Stretch-Behandlung der Dokument-Spalte würde sie
also automatisch breiter ausfallen als in Buy/Sale. `kColDoc` (Index 5) wird
weiterhin als `jahresDocColumn` übergeben, damit der Doppelklick
`documentActivated()` auslöst.

`onShowDetails()`/eigenständige Sub-Dialoge existieren in `ViewBrokerageEdit`
nicht — die Migration ist damit vollständig, keine Restausnahme wie bei
`ViewSaleEdit`.

@note **ViewShareAdd auf DocumentPreviewPanel umgestellt (19.07.2026):**
`ViewShareAdd` nutzt jetzt `DocumentPreviewPanel` statt einer eigenen Kopie
des QPdfView-/pdftoppm-Codes — der letzte der fünf Editier-Dialoge, der noch
nicht umgestellt war (siehe "Erledigt / Archiv",
"DocumentPreviewPanel: blockierender Dialog durch Inline-Anzeige ersetzt").
Anders als bei BuysForm/SalesForm/DividendForm/BrokeragesForm gibt es hier
kein `OverviewTabWidget` — `ViewShareAdd` ist ein reiner Anlage-Dialog ohne
Übersichts-Tabelle, die Migration betrifft daher ausschließlich die
PDF-Vorschau.

`createPreviewPanel()` instanziiert nur noch `DocumentPreviewPanel`
(vorher ~80 Zeilen eigener QPdfView-/Zoom-/pdftoppm-Aufbau). Die eigene
`openPdfPreview()`-Methode entfällt ersatzlos — `onBrowseDocument()` ruft
direkt `m_previewPanel->showDocument(path)`. Da `openPdfPreview()` bei
`ViewShareAdd` (anders als bei den anderen vier Dialogen) nie Teil von
`IViewShareAdd` war, sondern reines View-internes Detail, ist der Presenter
von der Änderung nicht betroffen — bestätigt durch Durchsicht von
`PresenterShareAdd.cpp`, das `openPdfPreview()` nirgends aufruft.

Damit bringt `ViewShareAdd` jetzt automatisch dieselbe
Existenzprüfung/Inline-"nicht gefunden"-Anzeige mit wie die anderen vier
Dialoge, ohne eigene Parallel-Implementierung.

---

## Versionierung

Die Versionsnummer der Anwendung selbst (`SharePortfolioManager`, Executable
in `app/`) hat genau **eine** Quelle: `project(SharePortfolioManager VERSION
...)` in der Root-`CMakeLists.txt`. Ein Bump erfolgt ausschließlich dort.

Feature (29.07.2026): Bis dahin gab es eine zweite, unabhängig gepflegte
Stelle — `app.setApplicationVersion(QStringLiteral("1.0.0"))` als
Hardcoded-Literal in `main.cpp`. Das führte prompt beim ersten PATCH-Bump
(auf `1.0.1`, siehe CHANGELOG.md) dazu, dass die Literal-Stelle in `main.cpp`
vergessen wurde und weiterhin `"1.0.0"` auslieferte. Behoben durch einen
generierten Versions-Header:

- **`app/Version.h.in`** — Vorlage im Quellbaum, definiert
  `SPM_VERSION_MAJOR`/`_MINOR`/`_PATCH`/`_STRING` als `@...@`-Platzhalter.
- **`app/CMakeLists.txt`** — `configure_file(Version.h.in ... @ONLY)`
  erzeugt daraus `Version.h` im Build-Verzeichnis (nicht im Quellbaum, daher
  kein zusätzlicher `.gitignore`-Eintrag nötig — `build/` ist dort bereits
  ausgeschlossen), mit `${CMAKE_CURRENT_BINARY_DIR}` im Include-Pfad des
  `SharePortfolioManager`-Targets.
- **`main.cpp`** — `#include "Version.h"`,
  `app.setApplicationVersion(QStringLiteral(SPM_VERSION_STRING))` statt des
  Literals.

`AboutForm` und dessen Zwischenablage-Export waren von Anfang an über
`QCoreApplication::applicationVersion()` implementiert und mussten daher
nicht angepasst werden — sie zeigen automatisch den korrekten, jetzt aus
CMake generierten Wert an.

Feature (01.08.2026): `MainWindow::baseWindowTitle()` verwendet denselben
Mechanismus für den Fenstertitel ("Share Portfolio Manager (Version
X.Y.Z)") — siehe "MainWindow-Details", Abschnitt "Fenstertitel", für die
Details und den zugehörigen Bugfix (redundante Dateiname-Anzeige entfernt).

Bewusst außerhalb dieses Mechanismus: `Logging::Logger::version()`,
`ParserLib::Parser::version()` und `Database::version()` — das sind
eigenständige, von der Applikationsversion unabhängige Bibliotheksversionen
(jeweils eigene, von Hand gepflegte Konstante in ihrer Quelldatei), die ein
eigenes Versionierungsschema mit eigenem Bump-Rhythmus haben. Kein
gemeinsamer Mechanismus mit `SPM_VERSION_STRING` vorgesehen, da eine
Kopplung an die Applikationsversion fachlich falsch wäre — die Bibliotheken
können sich unabhängig von der Applikation weiterentwickeln.

## PortfolioChartForm-Details

Der Depotwert-Chart (Feature 05.08.2026) stellt die tatsächliche
Wertentwicklung des gesamten Portfolios über die Zeit dar. Er sitzt als
eigener Tab im Hauptfenster, direkt hinter dem Depotwert-Grid.

### Was die Kurve zeigt

Es ist bewusst KEINE Vermögenskurve, sondern eine kumulierte Gewinn-/
Verlustkurve. Sie startet bei null und bewegt sich ausschliesslich durch
Dinge, die echten Wert schaffen oder vernichten: Kursänderungen, Dividenden,
realisierte Verkaufsgewinne und Kosten. Ein- und Auszahlungen verschieben sie
nicht — ein Nachkauf über 5.000 Euro lässt die Linie unverändert, weil
Einzahlen kein Gewinn ist.

Die Formel je Stichtag t, jeweils über alle Aktien summiert:

Linie(t) = Bestandswert(t) + realisierter Gewinn(t) + Dividenden(t)
           - Kosten(t) - Kaufwert der gehaltenen Anteile(t)

| Term | Definition |
|------|------------|
| Bestandswert(t) | gehaltene Stück(t) x Schlusskurs(t) |
| realisierter Gewinn(t) | Summe über Verkäufe bis t: (Verkaufswert - Steuer) - Kaufwert der verkauften Anteile |
| Dividenden(t) | Summe der Netto-Dividenden bis t, also brutto abzüglich Steuern |
| Kosten(t) | Summe aller Gebühren bis t, abzüglich Rabatt |
| Kaufwert gehalten(t) | Summe über die noch gehaltenen Lots: Restmenge x Kaufkurs |

@note **Steuern und Gebühren wirken an unterschiedlichen Stellen (Nessies
Vorgabe 05.08.2026):** Steuern werden direkt am jeweiligen Zahlungsstrom
abgezogen (Dividende brutto minus Steuern, Verkaufswert minus Steuer),
Gebühren sammeln sich im Kosten-Term. Der Verkaufserlös darf deshalb die
Verkaufsgebühr NICHT zusätzlich abziehen, sonst wäre sie doppelt gerechnet —
`ModelPortfolioChart::loadPortfolioInput()` ignoriert die per JOIN
mitgeladene Brokerage der Sale bewusst. Dividenden haben fachlich keine
Gebühren, entsprechend kennt `DividendObject` auch kein Gebührenfeld.

### Durchgerechnetes Referenzbeispiel

Zwei Aktien, ein Teilverkauf, ein Rabatt. Aktie A: 10.01. Kauf 20 x 50 Euro,
Gebühr 12, Rabatt 2; 10.03. Dividende brutto 30, Steuern 8; 10.04. Verkauf
8 x 60 Euro, Gebühr 4, Steuer 15. Aktie B: 01.02. Kauf 5 x 200 Euro,
Gebühr 8. Kurse A: 50 / 55 / 58 / 60 / 62, Kurse B: - / 200 / 190 / 195 / 210.

| Stichtag | Bestandswert | + realis. Gewinn | + Div. | - Kosten | - Kaufwert gehalten | Linie |
|----------|--------------|------------------|--------|----------|---------------------|-------|
| 10.01. | 1.000,00 | 0,00 | 0,00 | 10,00 | 1.000,00 | -10,00 |
| 01.02. | 2.100,00 | 0,00 | 0,00 | 18,00 | 2.000,00 | +82,00 |
| 10.03. | 2.110,00 | 0,00 | 22,00 | 18,00 | 2.000,00 | +114,00 |
| 10.04. | 1.695,00 | 65,00 | 22,00 | 22,00 | 1.600,00 | +160,00 |
| 30.04. | 1.794,00 | 65,00 | 22,00 | 22,00 | 1.600,00 | +259,00 |

Diese Zahlen sind als Regressionstest hinterlegt
(`test_referenceScenario_twoSharesWithPartialSale` in
`tests/utils/tst_portfolioseriescalculator.cpp`). Schlägt er fehl, hat sich
die Formel geändert, nicht der Test.

### FIFO

Verkäufe werden intern FIFO abgespielt: was zuerst gekauft wurde, wird zuerst
verkauft. Die gespeicherten `SaleBuyDetail`-Zuordnungen werden bewusst NICHT
verwendet. Zwei Gründe: `buys.volume_sold` gibt nur den heutigen Stand her,
für historische Stichtage muss die Zuordnung ohnehin durch Nachspielen der
Verkäufe in Datumsreihenfolge rekonstruiert werden — und `ShareCalculator::
compute()` warnt in seinem Kommentar ausdrücklich davor, sich auf
`SaleBuyDetail` zu stützen, da diese Datensätze ohne ihre Brokerage-Anteile
oder ganz leer gespeichert sein können.

### Datumsraster, Kurs-Fortschreibung, Aktien ohne Historie

Das Datumsraster ist die Vereinigungsmenge aller Kursdaten UND aller
Transaktionsdaten im gewählten Fenster. Die Transaktionsdaten müssen mit
hinein, weil sich die Linie auch an Kauf-, Verkaufs-, Dividenden- und
Kostentagen ändert; fehlten sie, fehlten genau die Sprungstellen.

Hat eine Aktie an einem Stichtag keinen eigenen Kurseintrag (Feiertag, andere
Börse), gilt der letzte bekannte Schlusskurs davor. Ohne diese
Vorwärts-Fortschreibung bräche die Portfoliosumme an einem solchen Tag
künstlich ein.

Eine Aktie ohne einen einzigen Eintrag in `daily_values` (Update-Typ "Nur
Kurs" oder "Kein Update") kann zu keinem Stichtag bewertet werden. Sie wird
vollständig ausgeschlossen — mit allen Käufen, Verkäufen, Dividenden und
Kosten — und in einer Warnzeile unter dem Chart benannt. Würde nur ihr
Bestandswert entfallen, ihr Kaufwert aber zählen, zeigte die Kurve einen
Verlust, den es nicht gibt. Aus demselben Grund trägt eine Aktie an
Stichtagen vor ihrem ersten Kursdatum nichts bei, auch wenn zu diesem
Zeitpunkt bereits Käufe verbucht sind.

### Ungültige Datumsangaben (Bugfix 06.08.2026)

Einträge ohne gültiges Datum werden vollständig ignoriert und je Aktie
gezählt. Hintergrund: ein ungültiges `QDate` ist in Qt KLEINER als jedes
gültige — die Schleifen "solange Datum kleiner oder gleich Stichtag" hätten
solche Einträge sonst allesamt am allerersten Stichtag verbucht. Im Feldtest
an Nessies Portfolio zeigte die Kurve dadurch bereits 2004 einen Kostenblock
von rund 1.000 Euro, obwohl der erste Kauf erst 2014 erfolgte. Entstehen
können solche Einträge etwa durch ein leeres `datetime`-Feld oder ein nicht
ISO-8601-konformes Datumsformat, das
`QDateTime::fromString(..., Qt::ISODate)` nicht liest.

### Obergrenze der Anzahl (korrigiert 06.08.2026)

`IModelPortfolioChart::earliestRelevantDate()` liefert das Datum des ältesten
KAUFS, nicht des ältesten Tageswerts. Die ursprüngliche Fassung übernahm die
Logik aus `PresenterChart`, wo sie richtig ist: der Aktien-Chart zeigt den
Kursverlauf einer Aktie, der auch vor dem ersten Kauf interessant ist. Beim
Portfolio-Chart gibt es vor dem ersten Kauf dagegen schlicht kein Portfolio,
die Kurve läge dort zwangsläufig auf null. Im Feldtest erlaubte die alte
Grenze 23 Jahre, obwohl der erste Kauf erst nach gut elf Jahren erfolgte.
Ersatzweise gilt weiterhin der älteste Tageswert, falls es noch gar keinen
Kauf gibt.

### Negative Kosten sind zulässig

Im Diagnose-Export können einzelne Aktien einen negativen Kosten-Term
aufweisen. Das ist kein Fehler: übersteigt der gewährte Rabatt die
angefallenen Gebühren, ergibt `brokerageReduction()` einen negativen Betrag —
etwa bei Fonds mit rabattiertem Ausgabeaufschlag und ohne weitere Gebühren.
Der Betrag hebt die Kurve entsprechend leicht an. Von Nessie am 06.08.2026
als gewollt bestätigt.

### Diagnose-Export

Der Knopf "Diagnose speichern…" im Zeitraum-Block schreibt eine CSV mit zwei
Blöcken: je Aktie die Anzahl geladener Käufe, Verkäufe, Dividenden,
Kosteneinträge und Tageswerte samt der ungültigen Datumsangaben sowie erstem
Kauf und Kursspanne, und je Stichtag alle sechs Bestandteile der Formel plus
Entwicklung in Euro und Prozent.

Ein dritter Block schlüsselt jede Aktie an jedem Stichtag einzeln auf:
gehaltene Stückzahl, verwendeter Schlusskurs, Bestandswert, Kaufwert
gehalten, realisierter Gewinn, Dividenden und Kosten. Er entsteht nur, wenn
`PortfolioSeriesCalculator::compute()` mit `withPerShareDetail` aufgerufen
wird, da er Stichtage mal Aktien Zeilen erzeugt.

Ergänzt 06.08.2026 bei der Fehlersuche an einem realen Portfolio: aus dem
gezeichneten Chart allein liess sich nicht ablesen, welcher Term eine
Auffälligkeit verursacht, und aus den Portfoliosummen nicht, welche Aktie.
Der Text entsteht in `PresenterPortfolioChart::buildDiagnosticsCsv()`, die
View fragt nur den Zielpfad ab und schreibt — Formatierung bleibt beim
Presenter.

@note Bei einer auffälligen Stelle im Kurvenverlauf lohnt es sich, den
Zeitraum vorher eng um das fragliche Datum zu legen (etwa Interval "Tag",
Anzahl 5) — dann bleibt der dritte Block überschaubar und die betroffene
Aktie steht direkt nebeneinander in wenigen Zeilen.

### Tooltip rastet auf Datenpunkte ein (Bugfix 06.08.2026)

`QLineSeries::hovered()` liefert die CURSORPOSITION in Achsenkoordinaten,
nicht den Datenpunkt unter dem Zeiger. Die erste Fassung zeigte `point.y()`
direkt als Entwicklung an — im Feldtest ergaben sich dadurch bei ein und
demselben Datum unterschiedliche Eurobeträge, je nachdem wie hoch der Zeiger
über der flach verlaufenden Kurve stand. Aus demselben Grund schlug die Suche
nach dem Prozentwert über die exakte X-Koordinate praktisch immer fehl und
lieferte konstant 0,00 Prozent.

`ViewPortfolioChart` hält deshalb die dargestellten Punkte samt ihrer
X-Koordinaten vor und rastet im Tooltip über eine binäre Suche auf den
nächstgelegenen echten Datenpunkt ein. Die interpolierten Nulldurchgänge
stehen bewusst nicht in dieser Liste — sie sind keine Datenpunkte und haben
keinen eigenen Prozentwert.

### Prozentwert

Der Nenner ist der kumulierte Kaufwert ALLER Käufe bis zum Stichtag, nicht
nur der gehaltenen (Nessies Vorgabe 05.08.2026). Nach einem Komplettverkauf
fällt der gehaltene Kaufwert auf null; mit ihm als Nenner spränge der
Prozentwert auf 0 %, obwohl die Linie noch den realisierten Gewinn zeigt.
Guard `> 0.0` vor der Division, gleiches Muster wie in `ShareCalculator`.

### Ehemalige Abweichung vom Depotwert-Footer (behoben 20.08.2026)

Der Kosten-Term umfasst alle Brokerage-Einträge einer Aktie, also auch
freistehende Kosteneinträge ohne Bezug zu einem Kauf oder Verkauf. Bis zum
20.08.2026 fielen genau die im Footer heraus: `ShareCalculator::compute()`
berücksichtigte sie nur in `totalBrokerage`, nicht aber in `completePurchase`
oder `salePayoutFinal` — die Spalte "Komplette Entwicklung" war dadurch um
diesen Betrag zu hoch, während der Chart hier bereits korrekt rechnete. Von
Nessie am 05.08.2026 bestätigt und als "Footer-Lücke bei freistehenden
Kosteneinträgen" in "Erledigt / Archiv" gefixt — seither summiert
`ShareCalculator::compute()` freistehende Einträge genauso wie
`ModelPortfolioChart::loadPortfolioInput()`, Footer und Chart stimmen jetzt
automatisch überein.

### Aufbau

Eigenes MVP-Tripel unter `app/forms/PortfolioChartForm/`, Aufbau analog
`ChartForm`. `IntervalUnit` wird aus `ChartTypes.h` wiederverwendet statt neu
definiert — die Zeitraumsteuerung ist in beiden Charts fachlich dieselbe.

Der Rechenkern liegt bewusst ausserhalb der Form, in
`app/utils/PortfolioSeriesCalculator`: `public static`, ohne Datenbank und
ohne Widgets, alle Eingangsdaten als einfache Structs. Dieselbe Rolle, die
`ShareCalculator` für das Grid hat. Gerundet wird über
`ShareCalculator::roundAway()`, damit die Cent-Semantik projektweit identisch
bleibt.

`PresenterPortfolioChart` liest die Portfoliodaten einmalig in
`loadAndDisplay()` und hält sie. Eine Änderung an Start-Datum, Interval oder
Anzahl rechnet nur neu, ohne die Datenbank erneut zu lesen — bei vielen
Aktien mit langer Historie ist das Laden der teure Teil. `reload()` verwirft
den Cache.

`ViewPortfolioChart` hat bewusst KEINE Legende: es wird genau eine Linie
dargestellt, eine Farbzuordnung wäre inhaltsleer (Nessies Vorgabe
05.08.2026). Rechts steht nur der Zeitraum-Block. Die Kurve wird an jedem
Vorzeichenwechsel geteilt, der Schnittpunkt mit der Null-Linie linear
interpoliert und in beide Abschnitte aufgenommen — sonst klaffte an der Achse
eine Lücke. Die Y-Achse enthält immer die Null, auch wenn die Kurve sie nie
kreuzt.

### Anbindung im MainWindow

`MainWindow` ist kein MVP-Tripel, sondern eine einzelne `QMainWindow`-Klasse;
der Tab wird dort direkt eingehängt. Der Tab-Index ist über die Konstanten
`kTabFinalValue` / `kTabPortfolioChart` / `kTabMarketValue` benannt, und die
beiden vorher vorhandenen `currentIndex() == 0`-Abfragen laufen jetzt über
`activePortfolioTable()`, das auf dem Chart-Tab `nullptr` liefert. Ohne diese
Umstellung hätte der eingeschobene Tab die Zuordnung Index-zu-Tabelle
verschoben.

Die `ViewPortfolioChart`-Instanz entsteht erst beim ersten Betreten ihres
Tabs (`ensurePortfolioChartUpToDate()`), da ihr Konstruktor das gesamte
Portfolio samt Tageswert-Historie liest und zum Zeitpunkt von
`setupCentralWidget()` noch gar kein Portfolio geöffnet ist. Bis dahin steht
im Tab ein leerer Container. `populatePortfolioTables()` markiert die Daten
über `invalidatePortfolioChart()` als veraltet; neu gerechnet wird beim
nächsten Betreten, oder sofort, falls der Tab gerade vorn liegt.

@note **Warum die Berechnung synchron läuft:** Während der Aggregation zeigt
der Chart "Berechnung läuft…" (`showCalculating()`, dritte Seite des
`QStackedWidget`). Die Rechnung selbst bleibt im GUI-Thread — sie ist schnell
genug, und ein Worker-Thread würde die MVP-Verdrahtung ohne echten Gewinn
verkomplizieren. Sollte sich das bei sehr grossen Portfolios ändern, ist der
Rechenkern durch seine Datenbankfreiheit bereits threadfähig.

---

## ShareSplitsForm-Details (Phase 3a der Aktiensplit-Behandlung, 08.08.2026)

Erfassungsmaske für Splits, erreichbar über den fünften Stift-Button in
`ViewShareEdit`. Vollständige MVP-Triade unter `app/forms/ShareSplitsForm/`
(`IViewShareSplitEdit`, `IModelShareSplitEdit`, `ModelShareSplitEdit`,
`PresenterShareSplitEdit`, `ViewShareSplitEdit`), also kein leichtgewichtiger
Einzeldialog — die Maske hat CRUD, Validierung und eine Übersicht und gehört
damit zur selben Kategorie wie DividendForm und BrokeragesForm.

**Warum der Button in "Allgemein" sitzt.** Die vier bestehenden Stift-Buttons
stehen in der GroupBox "Einnahmen / Ausgabe", wo jede Zeile geldwertig ist und
auf "€" endet. Ein Split hat keinen Betrag — er ändert nur die Stückelung.
Die Zeile "Splits:" sitzt deshalb in "Allgemein", direkt unter "Anteile:", auf
die sie sich fachlich bezieht (Nessies Entscheidung 08.08.2026).

**Warum der Hinweis neben dem Button steht.** Erwogen war eine Fusszeile in
`ShareDetailsForm` mit dem Hinweis, dass die dortigen Zahlen split-bereinigt
sind. Verworfen (Nessies Entscheidung 08.08.2026): eine Fusszeile steht zu
weit vom eigentlichen Geschehen entfernt. Die Information sitzt stattdessen im
read-only Feld unmittelbar neben dem Stift-Button — dort, wo auch gehandelt
wird. `ShareDetailsForm` bleibt unverändert.

Angezeigt wird je nach Lage `keine`, `20:1 am 18.07.2022` oder
`2 Splits, zuletzt 20:1 am 18.07.2022`; der Tooltip listet immer alle Splits.
`ViewShareEdit::setSplitInfo()` bekommt die Rohliste und formatiert selbst,
genau wie `loadShare()` — der Presenter reicht nur durch. Vorausgesetzt wird
dabei die Sortierung von `ShareSplitRepository::findByShare()` (aufsteigend
nach Datum), damit der letzte Eintrag der jüngste Split ist.

**Aufbau der Maske.** Bewusst OHNE `OverviewTabWidget`: eine Aktie hat
typischerweise null bis drei Splits, Jahres-Tabs wären reiner Ballast. Die
Übersicht ist deshalb eine flache `QTableWidget` (Datum, Verhältnis,
Umrechnung, Kurse bereinigt, Kommentar, Dokument).

@note Die erste Fassung dieser Maske hatte auch kein `DocumentPreviewPanel`,
mit der Begründung, zu einem Split gebe es ohnehin keinen Beleg. Das war
schlicht falsch — Banken verschicken sehr wohl Mitteilungen über anstehende
Kapitalmassnahmen (Nessies Einwand 08.08.2026). Dokumentfeld und Vorschau
kamen deshalb noch am selben Tag nach, siehe den folgenden Absatz.
Die Split-GUID hängt an jeder Zelle als `Qt::UserRole`, damit die Auswahl
unabhängig von der angeklickten Spalte auflösbar ist.

**Dokument und Vorschau (08.08.2026; Feld auf Kauf/Verkauf-Optik angeglichen
13.08.2026).** Ein Split trägt einen Beleg wie Kauf, Verkauf, Dividende und
Kosten auch: eigene `QGroupBox("  Dokument")` (`createDocumentGroup()`) links
zwischen Splitdaten und Buttonleiste, `DocumentPreviewPanel` rechts,
Dialogbreite entsprechend auf 1100 × 680 wie bei `ViewBrokerageEdit`. Der
Dateidialog lässt nur PDF zu und prüft den gewählten Pfad über
`DocumentRootMigrator::isPathWithinRoot()` — beides identisch zu den anderen
fünf Dialogen. Ein Zeilenklick in der Übersicht lädt den Beleg mit in die
Vorschau. Die Dokument-Spalte der Tabelle ist 36 px breit und ohne
Überschrift, nach der Vereinheitlichung vom 17.07.2026.

Ursprünglich (08.08.2026) war das Pfadfeld eine Zeile innerhalb der
Splitdaten-Groupbox, editierbar, mit `…`-Button und Doppelbelegungs-Prüfung
sowohl beim manuellen Eintippen (`editingFinished`) als auch bei der
Dateiauswahl. Am 13.08.2026 (Nessies Vorgabe, nach Rückmeldung zum
`…`-Button) auf dieselbe Optik wie bei den anderen fünf Dialogen umgestellt:
eigene Groupbox, Ordner-Icon (`IconProvider::MenuFolderOpen16`, 36 px breit)
statt Text-Button, Feld read-only. Der Pfad kommt seither ausschließlich über
den Dateidialog; die `editingFinished`-Verbindung für manuelles Eintippen
entfiel entsprechend. Die Doppelbelegungs-Prüfung selbst blieb unverändert —
sie läuft nur noch über `onDocumentSelected()`, nicht mehr zusätzlich über
Tastatureingabe. Regressionstest `test_view_documentPath_isReadOnly` in
`tst_sharesplitsform.cpp` sichert die Read-only-Eigenschaft ab.

Die Doppelbelegungs-Prüfung (`ModelShareSplitEdit::documentExists()`) läuft
bewusst nur gegen `share_splits`, nicht tabellenübergreifend (Nessies
Entscheidung 08.08.2026) — dieselbe Reichweite wie in BrokeragesForm. Sie
meldet einen Hinweis, blockiert das Speichern aber nicht: dass zwei Splits auf
derselben Bankmitteilung stehen, kann legitim sein.

Die Abfrage sitzt im Model und nicht im Repository, genau wie bei
`ModelBuyEdit`, `ModelSaleEdit`, `ModelDividendEdit` und
`ModelBrokerageEdit`. Die erste Fassung hatte sie im Repository untergebracht,
mit dem Argument, die SQL liege dann bei der übrigen `share_splits`-SQL. Auf
Nessies Entscheidung vom 08.08.2026 wurde sie ins Model verschoben — die
Prüfung ist eine Formular-Angelegenheit, kein allgemeiner Persistenzdienst.
`ShareSplitRepository` führt entsprechend nur `updateDocument()`, das
`DocumentRootMigrator` braucht.

@note Diese Umstellung hat einen Nebeneffekt, der über Ordnungsliebe
hinausgeht: das Abweichen vom bestehenden Muster hatte den NULL-Fehler unten
überhaupt erst ermöglicht. Wer die vorhandene Implementierung als Vorlage
nimmt, übernimmt ihre Lösungen für Fallstricke mit — auch die, deren Grund er
gerade nicht sieht. Eine Abweichung sollte deshalb einen Gewinn haben, der die
verlorene Vorlage aufwiegt; "die SQL liegt dann beisammen" war das nicht.

@note Die Prüfung verwendet ZWEI getrennte Abfragen — eine mit und eine ohne
`guid != :excl` —, genau wie die vier bestehenden Models. Die naheliegende
Zusammenfassung zu einer einzigen Abfrage ist eine Falle, in die ich am
08.08.2026 prompt getappt bin: beim Anlegen ist `excludeGuid` ein
default-konstruiertes `QString()`, also NULL und nicht bloss leer. Qt bindet
das als SQL-`NULL`, und `guid != NULL` ergibt in SQL nicht `true`, sondern
`NULL`. Eine `WHERE`-Bedingung, die zu `NULL` auswertet, filtert die Zeile
heraus — `COUNT(*)` war damit immer 0 und die Prüfung still wirkungslos.

Bemerkenswert am Fehlerbild: der Test, der die Ausnahme der eigenen GUID
prüft, lief grün durch. Ein Test, der belegt, dass etwas NICHT gefunden wird,
kann einen Fehler nicht entdecken, bei dem nie etwas gefunden wird. Aufgedeckt
haben es erst die beiden Positivtests. Wo eine Prüfung sowohl "gefunden" als
auch "nicht gefunden" liefern muss, braucht es beide Richtungen als Test —
sonst ist der Negativtest wertlos.

Ausgewertet wird der Beleg nicht — es gibt keine Parse-Pipeline für
Split-Mitteilungen. Ob sich eine lohnt, ist als offener Punkt festgehalten
(siehe "Parsing von Split-Mitteilungen der Banken prüfen"). Da der Pfad jetzt
gespeichert wird, liegen die Belege für eine spätere Untersuchung ohnehin
gesammelt vor.

**Validierung im Presenter.**

| Prüfung | Verhalten |
| --- | --- |
| Ex-Tag <= 01.01.2000 (Sentinel wie in allen anderen Formen) | abgewiesen |
| Ex-Tag in der Zukunft | ausdrücklich ERLAUBT |
| Verhältnis-Seite <= 0 | abgewiesen |
| Faktor = 1,0 (also 1:1, 2:2, …) | abgewiesen |
| Zweiter Split derselben Aktie am selben Tag | abgewiesen |

Zukünftige Ex-Tage sind erlaubt (Nessies Entscheidung 08.08.2026), damit ein
angekündigter Split sofort erfasst werden kann. Das ist technisch folgenlos:
`ShareSplitAdjuster::volumeFactor()` rechnet nur Datensätze mit einem Datum
ECHT VOR dem Splittag um, ein Split in der Zukunft trifft also schlicht noch
nichts.

Der Faktor-1,0-Fall wird hart abgewiesen statt still gespeichert (Nessies
Entscheidung 08.08.2026): er wäre fachlich kein Split, würde nichts umrechnen
und trotzdem in jeder Berechnung mitlaufen. Geprüft wird der Quotient, nicht
die wörtliche Eingabe — 2:2 fällt damit genauso durch wie 1:1.

Die Duplikat-Prüfung nimmt `UNIQUE(share_guid, date)` vorweg, damit der
Benutzer eine verständliche Meldung bekommt statt eines SQL-Fehlers. Beim
Bearbeiten zählt das eigene, unveränderte Datum nicht als Duplikat — sonst
liesse sich an einem bestehenden Split das Verhältnis nicht mehr korrigieren.

Keine Letzter-Eintrag-Beschränkung: jeder Split ist jederzeit editier- und
löschbar, analog DividendForm und BrokeragesForm.

**Löschen eines Splits.** Weil die Datenbank durchgehend die Beleg-Wahrheit
speichert, ist ein Split-Datensatz nichts als eine Rechenvorschrift. Beim
Löschen wird ausschliesslich die Zeile in `share_splits` entfernt —
`buys`, `sales` und `daily_values` bleiben unangetastet. Die Wirkung ist
trotzdem sofort und überall sichtbar: alle Transaktionen vor dem Splittag
werden wieder in ihrer Beleg-Stückzahl gerechnet, die Tageswerte vor dem
Splittag nicht mehr heruntergerechnet, und Grid, Footer, Detailansicht sowie
beide Charts ziehen unmittelbar nach. Geldbeträge (Einzahlung, Kaufwert,
Verkaufserlös, Gebühren, Steuern, Dividenden) bleiben unberührt — sie werden
von der Grundinvariante gar nicht erfasst.

Der Vorgang ist vollständig umkehrbar: derselbe Split mit denselben Werten neu
erfasst stellt bitgenau denselben Zustand wieder her. Verloren geht kein
Datensatz, sondern nur Wissen — Datum, Verhältnis und `prices_adjusted`
müssen erneut bekannt sein. Genau deshalb gibt es eine Rückfrage (Nessies
Entscheidung 08.08.2026), und zwar eine konkrete statt einer generischen: sie
nennt den betroffenen Split und beziffert die Bestandsänderung
("von 2.000,0000 auf 100,0000 Stück"). Die beiden Zahlen rechnet der Presenter
über `ShareSplitAdjuster::adjustedVolume()` einmal mit und einmal ohne den zu
löschenden Split aus; `IModelShareSplitEdit::openLots()` liefert dafür die
offenen Kauf-Posten in Beleg-Skala.

@note `confirm()` sitzt bewusst im View-Interface, statt dass der Presenter
`OwnMessageBox::question()` direkt aufruft. Sonst wäre der Löschpfad nicht
testbar — ein modaler Dialog blockiert die Ereignisschleife, und
`QDialog::exec()` in Tests ist ohnehin ausgeschlossen (siehe TESTING.md).
Über das Interface gibt der Stub einfach einen vorgegebenen Wert zurück.

**Aktualisierung nach Änderungen.** `PresenterShareSplitEdit::dataChanged()`
hängt an `ViewShareEdit::refreshSummary()`, genau wie bei den vier bestehenden
Sub-Dialogen. `PresenterShareEdit::populateSummary()` aktualisiert die
Split-Zeile im selben Durchlauf wie die Geldsummen — fachlich gehört sie nicht
zu "Einnahmen / Ausgabe", ein eigener Refresh-Pfad wäre aber nur zusätzliche
Verdrahtung ohne Gewinn.

---

## Split-Hinweis in den Editier-Dialogen (Phase 3b, 09.08.2026)

Setzt die Grundentscheidung vom 07.08.2026 um: Editier-Dialoge zeigen
durchgehend den BELEG, Grid, Charts und Detailansicht zeigen bereinigt. Damit
die Beleg-Stückzahl neben dem heutigen Bestand nicht wie ein Fehler wirkt,
steht unter den Feldern in `ViewBuyEdit` und `ViewSaleEdit` ein Hinweis:

@code{.unparsed}
Split 20:1 am 18.07.2022 — entspricht heute 100,0000 stk. à 50,1500 €
@endcode

**Warum als Fusszeile der Gruppe.** Erwogen war eine Zeile direkt unter
"Preis", also am fachlichen Bezugspunkt. Verworfen (Nessies Entscheidung
08.08.2026), weil der Hinweis live mitläuft: beim Ändern des Datums erscheint
und verschwindet er, und mitten im Formular würden dabei jedes Mal alle
darunter liegenden Zeilen — Provision, Courtage, Rabatt, Endbetrag — nach
unten und wieder nach oben springen. An der Gruppenkante bewegt sich oberhalb
nichts.

**Warum die Zeile auch ohne Split steht.** Aus demselben Grund. Ohne Split
zeigt sie gedämpft "Kein Split nach diesem Datum — Stückzahl entspricht dem
heutigen Stand" (Nessies Entscheidung 08.08.2026). Eine Zeile, die kommt und
geht, verschiebt das Layout genauso wie eine eingefügte.

**Warum Preis und Stückzahl gemeinsam umgerechnet werden.** Aus 5 Stück à
1.003,00 € werden bei einem 20:1-Split 100 Stück à 50,15 € — das Produkt
bleibt 5.015,00 €. Stünde nur die veränderte Stückzahl da, sähe es aus, als
hätte sich der Wert vervielfacht. Beide Zahlen zusammen zeigen, dass ein Split
weder Gewinn noch Verlust schafft.

**Bei mehreren Splits** nennt der Text Anzahl und jüngsten Splittag
(`2 Splits, zuletzt 20:1 am 18.07.2022 — …`) und rechnet mit dem kumulierten
Faktor. Die vollständige Liste steht im Tooltip des Labels. Alle Splits in die
Zeile zu schreiben wäre ab dreien unlesbar.

### ShareSplitHint

Die Formatierung liegt in `app/utils/ShareSplitHint.h/.cpp`, weil `ViewBuyEdit`
und `ViewSaleEdit` denselben Text brauchen. Zustandslos und datenbankfrei wie
`ShareSplitAdjuster` und `SaleFifoAllocator` daneben — alle Eingangsdaten
kommen als Parameter herein, die Klasse ist damit ohne Widgets und ohne SQLite
prüfbar.

@note Die Auslagerung geschah vorbeugend, nicht als Aufräumarbeit: eine zweite
Kopie derselben Formatierung wäre der direkte Weg zurück zu dem Problem, das
Phase 2c mit der dreifach duplizierten FIFO-Schleife aufgeräumt hat. Zwei
Kopien driften genauso auseinander wie drei, nur langsamer.

Die eigentliche Umrechnung macht `ShareSplitHint` nicht selbst, sondern ruft
`ShareSplitAdjuster::adjustedVolume()` und `adjustedTransactionPrice()` auf.
Die Regel, welche Splits zählen und wie Stückzahl und Preis gegenläufig
skalieren, existiert damit weiterhin nur an einer Stelle.

### Verdrahtung in den Presentern

Beide Presenter laden die Splits einmalig im Konstruktor und rufen
`refreshSplitHint()` aus zwei Richtungen auf:

| Auslöser | Weg |
| --- | --- |
| Stückzahl oder Preis geändert | `onValuesChanged()` → `refreshDerivedValues()` |
| Datum geändert | `onDateEdited()` |

Beide sind nötig. `refreshDerivedValues()` wird beim Ändern des Datums nicht
aufgerufen, `onDateEdited()` nicht beim Ändern von Stückzahl oder Preis — der
Hinweis hängt aber an allen dreien.

@note Nebenbefund vom 09.08.2026: `PresenterSaleEdit::refreshDerivedValues()`
holte die Splits bislang bei JEDEM Aufruf frisch aus der Datenbank, also bei
jedem Tastendruck in einem der Eingabefelder. Da sie sich während einer
Dialog-Sitzung nicht ändern können — die Split-Maske ist von hier aus nicht
erreichbar —, nutzt die Methode jetzt denselben Zwischenspeicher, der ohnehin
für den Hinweis angelegt wird.

### Dividenden bewusst ausgenommen

`ViewDividendEdit` bekommt keinen Split-Hinweis (Nessies Entscheidung
09.08.2026). Der Grund ist nicht Sparsamkeit, sondern dass die beiden
Argumente dort nicht greifen:

Die Ausschüttung ist `rate × volume` und über einen Split invariant — es gibt
nichts umzurechnen. Und der Übersicht-Tab der Dividenden summiert im Gegensatz
zu Käufen und Verkäufen keine Stückzahlen (Spalten: Jahr, Dividende).

Bleibt das Risiko, dass ein Nutzer die Beleg-Stückzahl für veraltet hält und
"korrigiert" — womit die Ausschüttung nicht mehr zum Beleg passt. Dagegen
hilft aber keine Anzeige, sondern eine Prüfung, die tatsächlich rechnet; siehe
"Offene Punkte", "Plausibilitätsprüfung der Dividenden-Stückzahl".

@note Teilweise überholt durch Phase 3c (11.08.2026). Die Aussage zum
Fusszeilen-Hinweis gilt weiterhin — `ViewDividendEdit` hat keinen. Die
Begründung "summiert keine Stückzahlen" traf aber nur auf den Übersicht-Tab
zu: die Fusszeile der JAHRES-Tabs summierte sehr wohl `volume()`, über
verschiedene Auszahlungstage hinweg. Diese Summe war schon vor jedem Split
bedeutungslos und zeigt seit Phase 3c "-". Die Belegzeilen tragen den
Split-Marker wie überall sonst — siehe "Split-Marker und Summen in den
Übersichtstabellen".

---

## Split-Marker und Summen in den Übersichtstabellen (Phase 3c, 11.08.2026)

Phase 3b setzte einen Hinweis unter die Eingabefelder der Editier-Dialoge.
Phase 3c bringt dieselbe Aussage in die Tabellen: eine Stückzahl, die nicht
mehr dem heutigen Stand entspricht, wird als solche gekennzeichnet, und
Summen über mehrere Belege stehen auf einer einheitlichen Skala.

### Die Regel

Zwei Arten von Zellen, zwei Verhaltensweisen:

| Zellart | Skala | Marker |
| ------- | ----- | ------ |
| Belegzeile eines Jahres-Tabs | BELEG | ja, wenn nach dem Belegdatum ein Split liegt |
| Aggregat (Fusszeile, Jahreszeile der Übersicht) | HEUTE | ja, wenn vor dem ältesten summierten Beleg ein Split liegt |

Belegzeilen bleiben in Beleg-Skala, weil sie Abschriften des Dokuments sind,
das nach einem Zeilenklick rechts in der Vorschau erscheint — die Zahlen
müssen sich decken. Aggregate stehen auf heutiger Skala, weil eine Summe über
Belege verschiedener Stückelung sonst gar nichts bedeutet.

Aggregate rechnen dabei **je Beleg** über `ShareSplitAdjuster::adjustedVolume()`
um und summieren erst danach. Die Summe selbst zu skalieren wäre falsch,
sobald mehrere Belege betroffen sind.

@note Die Regel gilt für STÜCKZAHLEN, nicht für Geldbeträge. Auszahlung,
Gewinn/Verlust, Gebühren und Rabatte werden unverändert summiert — ein Split
schafft weder Gewinn noch Kosten. Der Brokerage-Bugfix vom selben Tag zeigt,
wohin die Verwechslung führt: aus 30,95 EUR Kauf-Nebenkosten wären bei einem
20:1-Split 619,00 EUR geworden (siehe "Anteilige Kauf-Nebenkosten der
FIFO-Zuteilung").

### Betroffene Views

| View | Belegzeilen | Aggregate |
| ---- | ----------- | --------- |
| `ViewBuyEdit` | Marker | heutige Skala + Marker |
| `ViewSaleEdit` | Marker | heutige Skala + Marker |
| `ViewDividendEdit` | Marker | Anteile-Summe → "-" |
| `ViewShareDetails`, Gewinne/Verluste | Marker | heutige Skala + Marker |
| `ViewShareDetails`, Dividenden | Marker | Anteile-Summe → "-" |
| `ViewShareDetails`, Kosten | — | keine Anteile-Spalte |
| `ViewBrokerageEdit` | — | keine Anteile-Spalte |

Im Marktwert-Modus von `ViewShareDetails` gilt für Gewinne/Verluste dasselbe
wie im Depotwert-Modus: die Stückzahlen sind in beiden Modi identisch, nur die
Geldbeträge unterscheiden sich (brokeragefrei).

### Sonderfall Dividenden

Bei Dividenden wird die Anteile-Summe der Jahres-Fusszeile NICHT umgerechnet,
sondern durch "-" ersetzt (Nessies Entscheidung 11.08.2026).

"Anteile am Auszahlungstag" bezieht sich auf je einen Stichtag. Zwei
Ausschüttungen auf 100 und 150 Stück ergeben addiert 250 — einen Bestand, den
es zu keinem Zeitpunkt gab. Anders als bei Käufen und Verkäufen hilft hier
auch die Umrechnung auf heutige Skala nicht: sie würde aus einer
bedeutungslosen Zahl nur eine andere machen. Der Dividendensatz in der Spalte
daneben steht aus demselben Grund seit jeher auf "-".

Die Summe war damit auch ohne jeden Split falsch; der Split machte sie nur
sichtbar. Damit erledigt sich der zuvor unter "Offene Punkte" geführte
Dividenden-Footer-Punkt.

### Marker und Tooltips

Das Markerzeichen ist ein angehängtes `*` (`ShareSplitHint::marker()`),
gesetzt über `ShareSplitHint::withMarker()`. Bewusst ein Textzeichen statt
eines Icons: die Anteile-Spalte ist eine Textspalte, und ein `setCellWidget()`
wie in der Dokument-Spalte würde den Zellentext verdrängen und die
Zentrierung zerstören (Nessies Entscheidung 10.08.2026).

Der Marker bleibt in normaler Textfarbe (Nessies Entscheidung 11.08.2026).
Ein `QTableWidgetItem` trägt einen einzigen String; `setForeground()` würde
immer die ganze Zelle einfärben, und nur das Sternchen zu färben bräuchte
einen eigenen Delegate. Die orange Hervorhebung bleibt deshalb den
Fusszeilen-Hinweisen der Editier-Dialoge vorbehalten.

Zwei Tooltip-Texte, beide in `ShareSplitHint`:

- `overviewRowTooltip()` an Belegzeilen — nennt die heutige Entsprechung,
  erste Zeile wortgleich zur Fusszeile der Editier-Dialoge.
- `overviewAggregateTooltip()` an Aggregatzellen — erklärt, dass über Belege
  unterschiedlicher Stückelung summiert und dafür umgerechnet wurde.

### Splits als Parameter, nicht als Setter

`populateOverview()` in `IViewBuyEdit`, `IViewSaleEdit`, `IViewDividendEdit`
sowie `populateGewinneVerluste()`/`populateDividenden()` in
`IViewShareDetails` nehmen die Splits als zusätzlichen Parameter entgegen.

Ein eigener `setSplits()`-Aufruf wäre die naheliegende Alternative gewesen —
`IViewSaleEdit` hatte einen aus Phase 2c. Er erzeugt aber eine unsichtbare
Reihenfolge-Abhängigkeit zwischen zwei View-Aufrufen, die erst auffällt, wenn
sie einmal falsch herum steht. Als Parameter ist die Abhängigkeit im
Signatur-Typ sichtbar und vom Compiler geprüft.

`IModelDividendEdit` und `IModelShareDetails` bekamen dafür `loadSplits()`,
wortgleich zu `IModelBuyEdit`/`IModelSaleEdit` — reine Weiterleitung an
`ShareSplitRepository::findByShare()`.

### Farbe des Split-Hinweises

Der aktive Hinweis in `ViewBuyEdit`/`ViewSaleEdit` ist seit 11.08.2026 orange
(`#C77400`, fett) statt blau. Der gedämpfte Zustand ("Kein Split nach diesem
Datum") bleibt zurückhaltend, wechselt aber von `palette(mid)` auf
`palette(placeholderText)` — im dunklen Theme war er zuvor kaum lesbar.

Nessies ursprüngliche Vorgabe war "durchgängig orange". Dagegen sprach, dass
der gedämpfte Zustand bei jeder Aktie ohne Split dauerhaft im Formular steht:
in Warnfarbe würde er abstumpfen und den echten Hinweis mit entwerten
(Entscheidung 11.08.2026).

Das Label sitzt seither ab Gitterspalte 1 statt 0 — es gehört inhaltlich zu
den Eingabewerten, nicht zu den Feldnamen. Der untere Rand der Kaufdaten-/
Verkaufsdaten-Gruppe ist auf 4 px reduziert (statt 10): die letzte Zeile ist
ein reines Textlabel ohne Feldrahmen und braucht weniger Luft.

---

## Anteilige Kauf-Nebenkosten der FIFO-Zuteilung (Bugfix, 11.08.2026)

Ein Verkauf verbraucht ueber `SaleFifoAllocator` einen oder mehrere Kaeufe.
Jede dabei entstehende `SaleBuyDetail`-Zeile traegt neben Menge und Kaufkurs
auch den anteiligen Betrag der Kauf-Brokerage und des Kauf-Rabatts —
`brokeragePart` und `reductionPart`. Diese Betraege gehen in
`SaleObject::buyValueBrokerageReduction()` und damit in die Gewinnermittlung
ein.

Beim Umbau auf `SaleFifoAllocator` (Phase 2c) fielen beide Werte aus der
Erzeugung heraus. `PresenterSaleEdit::onSave()` belegte nur vier der sechs
Konstruktor-Parameter von `SaleBuyDetail`; `reductionPart` und
`brokeragePart` haben Defaultwerte 0.0, weshalb der Verlust ohne
Compilerfehler blieb. Fuer jeden seither erfassten oder bearbeiteten Verkauf
stand in der Datenbank `brokerage_part = 0`.

### Wie der Fehler auffiel

Nicht durch einen Test, sondern durch einen Vergleich zweier Datenbanken. In
der gewachsenen Produktivdatenbank tragen 48 historisch erfasste Verkaeufe
ihre Kosten korrekt; nur ein frisch erfasster Verkauf wies 0,00 EUR aus,
obwohl der zugehoerige Kauf 30,95 EUR Gebuehren traegt.

Die Abfrage, die das trennt, vergleicht zwei Spalten nebeneinander — der
alleinige Blick auf "Kosten gleich 0" fuehrt in die Irre, weil gebuehrenfreie
Kaeufe voellig zu Recht 0 ergeben:

```sql
SELECT s.order_number,
       SUM(sbd.brokerage_part) AS zugeteilte_kosten,
       SUM(COALESCE(br.provision,0)
         + COALESCE(br.broker_fee,0)
         + COALESCE(br.trader_fee,0)) AS kosten_am_kauf
FROM sale_buy_details sbd
JOIN sales s ON s.guid = sbd.sale_guid
JOIN buys  b ON b.guid = sbd.buy_guid
LEFT JOIN brokerage br ON br.buy_guid = b.guid
GROUP BY s.guid;
```

### Verteilungsregel

Verteilt wird nach dem Bruchteil des verbrauchten Kaufs — dasselbe
Pro-Lot-FIFO-Modell, das `ShareCalculator` fuer gehaltene Anteile verwendet:

```
brokeragePart = brokerage.brokerage() * (detailVolume / buy.volume())
reductionPart = brokerage.reduction() * (detailVolume / buy.volume())
```

Belegt wird die Regel durch die Altdaten selbst: zwei Verkaeufe vom
25.09.2017 teilen sich einen Kauf mit 10,31 EUR Gebuehren und tragen
6,87333 EUR beziehungsweise 3,43667 EUR — zwei Drittel und ein Drittel,
Summe exakt 10,31 EUR.

@note Es wird bewusst nicht gerundet. Nur so trifft die Summe der Teile den
Gesamtbetrag des Kaufs exakt, wenn mehrere Verkaeufe denselben Kauf
verbrauchen. Die Rundung auf zwei Stellen geschieht erst bei der Anzeige.

@note Der Bruch darf NICHT ueber `ShareSplitAdjuster` laufen. `detailVolume`
und `buy.volume()` liegen in der Beleg-Skala DESSELBEN Kaufs, der Bruch ist
damit skaleninvariant — ein Split zwischen Kauf und Verkauf veraendert ihn
nicht. Eine Umrechnung wuerde einen Geldbetrag mit dem Split-Faktor
multiplizieren; im Feldfall Alphabet waeren aus 30,95 EUR die Summe
619,00 EUR geworden. Ein Split schafft weder Gewinn noch Kosten.

### Warum die Berechnung im Presenter liegt

`SaleFifoAllocator` ist zustandslos und datenbankfrei und soll das bleiben.
Die Brokerage liegt in einer eigenen Tabelle mit FK auf den Kauf und ist nur
ueber `IModelSaleEdit::loadBrokerageForBuy()` erreichbar. Die anteilige
Berechnung sitzt deshalb in `PresenterSaleEdit::proportionalBuyCosts()`,
aufgerufen aus `onSave()`, `refreshDerivedValues()` und
`buildBuyDetailSummary()`.

Daraus folgt auch die Verlagerung des Details-Dialogs: `ViewSaleEdit` hat als
View keinen Modellzugriff und setzte die Kosten im Live-FIFO-Zweig deshalb
hart auf 0.0. Der gesamte Rechenteil ist nach
`PresenterSaleEdit::buildBuyDetailSummary()` gewandert und wird ueber
`IViewSaleEdit::showBuyDetails()` als `SaleBuyDetailSummary` uebergeben. Der
neue Header `app/forms/SalesForm/SaleBuyDetailRow.h` enthaelt beide
Transportstrukturen. Die View rendert nur noch.

### Mitbehobene Nebenwirkungen

Die Live-Vorschau von Gewinn/Verlust in `refreshDerivedValues()` rechnete
ohne die Kaufkosten und sprang deshalb beim Speichern auf einen anderen Wert.
Sie folgt jetzt derselben Formel wie
`SaleObject::profitLossBrokerageReduction()`:

```
(saleValue - Verkaufsgebuehren + Rabatt)
- (Kaufwert + Kaufbrokerage - Kaufrabatt)
- Steuern
```

Das Anzeigefeld "Gekaufter Kaufwert" bleibt bewusst OHNE Brokerage — es
entspricht `SaleObject::buyValue()`.

Die Summenzeile des Details-Dialogs zog den anteiligen Kaufrabatt nicht ab,
obwohl die Spalte Gesamt ihn je Zeile bereits beruecksichtigt. Solange
`reduction_part` ueberall 0 war, fiel die Abweichung nicht auf. Die Summe
entspricht jetzt `SaleObject::buyValueBrokerageReduction()`.

Verkaufsgebuehren und Steuern im Details-Dialog stammen beim Bearbeiten des
juengsten Verkaufs aus dem Formular statt aus dem gespeicherten `SaleObject`.
Die Felder sind dort editierbar; gespeicherte Werte waeren veraltet und
wichen von dem ab, was `onSave()` anschliessend schreibt.

### Sackgasse waehrend der Analyse

Der Details-Dialog eignet sich nicht zur Diagnose von Datenfehlern. Fuer den
juengsten Verkauf rechnet er die Zuteilung neu, statt die gespeicherte
anzuzeigen (Phase 2c). Ein Kauf, der versehentlich in heutiger Skala erfasst
wurde, und ein Kauf in Beleg-Skala mit zugehoerigem Split ergeben dort exakt
dieselben Zahlen — die Anzeige kann beide Faelle nicht unterscheiden. Nur die
Rohtabelle `sale_buy_details` gibt darueber Auskunft.

Ebenfalls irrefuehrend: die Kopfzeile lautet auch im Neuberechnungs-Fall
"Tatsaechliche FIFO-Zuteilung des gespeicherten Verkaufs". Als offener Punkt
vermerkt, siehe unten.

---

## Offene Punkte

### Aktiensplits werden nicht behandelt (wichtig, 06.08.2026, Umsetzung begonnen 07.08.2026)

Die Anwendung kennt keine Aktiensplits. Solange nur der heutige Kurs
betrachtet wird, fällt das nicht auf — sobald aber historische Kurse gegen
historische Käufe gerechnet werden, bricht es auf.

Im Feldtest zeigte der Depotwert-Chart zwei scheinbar unerklärliche Sprünge.
Ursache war Alphabet Inc. Cl. A: Der Kauf vom 18.03.2020 ist mit einem Kurs
NACH dem Split eingetragen (48,60 Euro), die Tageswerte jener Zeit sind aber
nicht split-bereinigt und stehen bei rund 1.003 Euro. Zwischen dem Kauf und
dem tatsächlichen Split am 18.07.2022 wurde die Position dadurch mit dem
Zwanzigfachen bewertet; am Splittag endete das schlagartig, ohne dass eine
Transaktion stattgefunden hätte.

Eine Lösung bräuchte eine Split-Tabelle je Aktie (Datum und Verhältnis) und
eine Umrechnung überall dort, wo historische Kurse gegen Transaktionen
gerechnet werden. Betrifft Datenmodell, Parser, Grid und Chart gleichermassen
und ist als eigenes Feature zu behandeln. Nessies Einschätzung 06.08.2026:
schnellstmöglich umsetzen.

@note Als Sofortmassnahme wurde der konkrete Fall behoben, indem die
Tageswerte der betroffenen Aktie gelöscht und neu abgerufen wurden — die
Quelle lieferte sie split-bereinigt. Als tragende Lösung taugt dieser Weg
aber NICHT, und zwar aus einem Grund, der über die naheliegenden hinausgeht:
er funktionierte nur, weil der Kauf bereits in Stück NACH dem Split erfasst
war. Bei einem echten Split einer gehaltenen Position bucht die Bank ein
Vielfaches an Stücken ein, während `buys.volume` die ursprüngliche Zahl
behält; bereinigte Kurse allein liessen den Bestand dann auf einem
Zwanzigstel stehen und den heutigen Depotwert entsprechend zu niedrig
erscheinen. Kurshistorie UND Kaufdatensätze müssen gemeinsam angepasst
werden. Hinzu kommen: die Quelle liefert nur begrenzte Historie (was sie
nicht mehr hergibt, ist nach dem Löschen verloren), ein Split wird nirgends
gemeldet und muss selbst bemerkt werden, Aktien mit Update-Typ "Nur Kurs"
oder "Kein Update" haben gar keinen Abrufweg, und ob eine Quelle überhaupt
bereinigt liefert, ist je Anbieter unterschiedlich.

**Entscheidungen 07.08.2026 (Nessie):**

1. Die Datenbank behält durchgehend die Beleg-Wahrheit — keine physische
   Umschreibung von `buys`/`sales`/`daily_values` bei einem erfassten
   Split. Die Umrechnung auf heutige Stücke passiert ausschliesslich zur
   Laufzeit über `ShareSplitAdjuster`, siehe "ShareSplitObject /
   ShareSplitRepository / ShareSplitAdjuster" unter "Repository-Schicht".
2. Editier-Dialoge zeigen immer den Beleg, mit einem Hinweis auf den
   Splittag ("Split 20:1 am 18.07.2022 — entspricht 100 Stück à 50,15 €").
   Grid, Charts und Detailansicht zeigen bereinigt.
3. `prices_adjusted` wird je Split gespeichert, nicht je Aktie.
4. Umfang: nur echte Splits und Reverse-Splits. Spin-offs und
   Kapitalmaßnahmen mit Barkomponente sind fachlich etwas anderes (der Wert
   ist dabei NICHT invariant) und ausdrücklich nicht Teil dieses Features —
   siehe "Spin-offs und Kapitalmaßnahmen mit Barkomponente nicht
   abgedeckt" unten.
5. Bruchstücke aus Reverse-Splits (Barauszahlung von Spitzen durch die
   Bank) werden nicht als eigenes Feature abgebildet — lassen sich aber
   ohne neuen Code über einen normalen Verkauf erfassen, siehe
   "Bruchstücke bei Reverse-Splits nicht abgedeckt" unten.

**Phasenplan:**

| Phase | Inhalt | Stand |
| --- | --- | --- |
| 1 | Schema (`share_splits`), `ShareSplitObject`, `ShareSplitRepository`, `ShareSplitAdjuster` + Tests. Keine sichtbare Änderung. | ✅ umgesetzt 07.08.2026 |
| 2a | Anwendung in `ShareCalculator` — Grid, Footer und `ShareDetailsForm` rechnen jetzt split-bereinigt. | ✅ umgesetzt 07.08.2026 |
| 2b | Anwendung in `ModelPortfolioChart`/`ModelChart` — Depotwert- und Aktien-Chart rechnen jetzt beide split-bereinigt. | ✅ umgesetzt 07.08.2026 |
| 2c | FIFO-Verkaufszuteilung (`PresenterSaleEdit`/`ModelSaleEdit`). Neue gemeinsame Klasse `SaleFifoAllocator` ersetzt die vormals dreifach duplizierte Zuteilungslogik; Edit-Zweig berechnet FIFO beim Bearbeiten des jüngsten Verkaufs jetzt neu, statt gespeicherte `SaleBuyDetails` unverändert zu übernehmen. | ✅ umgesetzt 07.08.2026 |
| 3a | `ShareSplitsForm` — eigene MVP-Triade, fünfter Stift-Button in `ViewShareEdit` (GroupBox "Allgemein"), Split-Hinweis neben dem Button | ✅ umgesetzt 08.08.2026 |
| 3b | Split-Hinweis in den Editier-Dialogen `ViewBuyEdit`/`ViewSaleEdit` | ✅ umgesetzt 09.08.2026 |
| 3c | Übersichtstabellen: Split-Marker je Zeile, Korrektur der Stückzahl-Summenzeile in `ViewBuyEdit`, `ViewSaleEdit`, `ViewDividendEdit` und `ViewShareDetails` (Gewinne/Verluste + Dividenden); Anteile-Summe der Dividenden-Fusszeile als "-"; Split-Hinweis der Editier-Dialoge in Orange | ✅ umgesetzt 11.08.2026 |
| 4a | "Prüfen"-Knopf im Split-Dialog (`SplitPriceJumpDetector`): vergleicht auf Nutzeraktion hin die Kurshistorie um den Ex-Tag, setzt bei eindeutigem Ergebnis automatisch den `prices_adjusted`-Haken. Siehe "Automatische Erkennung split-bereinigter Kurshistorie" unten. | ✅ umgesetzt 14.08.2026 |
| 4b | Automatische Nachprüfung des `prices_adjusted`-Zustands nach jedem Tageswert-Abruf (Kurssprung um den Splittag vergleichen) + Startmeldung bei Widerspruch, analog `warnAboutSharesWithoutDailyValues()`. Ursprünglich zugunsten von 4a zurückgestellt (13.08.2026) — auf Nessies Wunsch 20.08.2026 doch umgesetzt, siehe "Automatische Nachprüfung nach Tageswert-Abruf" unten für die Begründung, warum das der ursprünglichen Zurückhaltung nicht widerspricht. | ✅ umgesetzt 20.08.2026 |

### Plausibilitätsprüfung der Dividenden-Stückzahl (09.08.2026)

Die Stückzahl in `ViewDividendEdit` ist ein Eingabefeld, und aus ihr wird die
Ausschüttung gerechnet (`rate × volume`). Wer eine Dividende von 2021 öffnet,
sieht dort die damalige Stückzahl, weiss aber, dass er heute ein Vielfaches
hält — und die naheliegende Reaktion ist, das für einen Fehler zu halten und
zu "korrigieren". Danach passt die Ausschüttung nicht mehr zum Beleg, und weil
Dividenden in die Gewinnrechnung eingehen, fällt das nicht sofort auf.

Erwogen war ein blosser Hinweis wie in Käufen und Verkäufen. Nessies Einwand
vom 09.08.2026 ist der bessere Ansatz: der Benutzer hat ein Bankdokument
vorliegen und soll sich daran halten; sinnvoller als eine Erklärung ist eine
Prüfung, die tatsächlich rechnet.

Bezugsgrösse ist die **damals gehaltene Stückzahl zum Stichtag**, nicht die
split-umgerechnete — nach ihr hat die Bank ausgeschüttet. Sie ergibt sich aus
allen Käufen und Verkäufen vor dem Ex-Tag, durchgehend in Beleg-Skala.

Offen sind mindestens:

- **Warnung oder Blockade?** Eine Blockade wäre riskant: Teilbestände in
  mehreren Depots, unterjährige Käufe zwischen Ex-Tag und Zahltag und
  Bruchstücke aus Reverse-Splits können legitim abweichen.
- **Welcher Stichtag?** Ex-Tag oder Zahltag — `DividendObject` führt nur ein
  Datum, welches von beiden es fachlich ist, muss geklärt werden.
- **Depotbezug.** Eine Dividende kennt keine Depotnummer, Käufe und Verkäufe
  schon. Die Summe über alle Depots ist die einzige verfügbare Grösse.

### tst_mainwindow.cpp in eigene Testdateien aufteilen (09.08.2026)

`tests/forms/tst_mainwindow.cpp` ist auf 9673 Zeilen gewachsen und enthält
fünf Testklassen, obwohl TESTING.md ein Testziel je Form vorsieht:

| Klasse | gehört nach |
| --- | --- |
| `TestMainWindow` | bleibt in `tst_mainwindow.cpp` |
| `TestSalesForm` | `tst_salesform.cpp` |
| `TestDividendForm` | `tst_dividendform.cpp` |
| `TestOwnMessageBox` | `tst_ownmessagebox.cpp` |
| `TestBackupForm` | `tst_backupform.cpp` |

Die Konvention wird damit viermal gebrochen. Praktisch heisst das: wer eine
Änderung an SalesForm testet, baut und lädt eine 9600-Zeilen-Datei mit, und
ein Fehlschlag irgendwo in der Mitte ist schwer zuzuordnen — der Absturz vom
08.08.2026 in `tst_portfoliochartform` hat genau diese Sorte Verwechslung
vorgeführt.

Der Umzug ist überschaubar: `TestSalesForm` bringt `openMemoryDb()` und
`insertTestShare()` bereits als eigene Kopien mit, greift also nicht auf
`TestMainWindow` zu. `StubModelSaleEdit`/`StubViewSaleEdit` stehen zwar oben
bei den übrigen Stubs, gehören aber ausschliesslich zu ihr. Es ist weitgehend
Ausschneiden, Einfügen und je ein neues CMake-Ziel — kein Umbau.

@note Bewusst NICHT zusammen mit einem Feature erledigen. Eine reine
Umstrukturierung ohne sichtbaren Nutzen sollte für sich stehen, damit bei
einem Fehlschlag nicht gleichzeitig ein neues Feature im Verdacht steht.

### Parsing von Split-Mitteilungen der Banken prüfen (08.08.2026)

Seit Phase 3a kann einem Split ein Beleg zugeordnet werden, ausgewertet wird er
aber nicht. Beim Entwurf war ich davon ausgegangen, dass es zu einem Split gar
keinen Beleg gibt; Nessies Einwand am 08.08.2026: seine Banken verschicken sehr
wohl Mitteilungen über anstehende Kapitalmassnahmen.

Ob sich eine Parse-Pipeline lohnt, lässt sich ohne echte Beispieldokumente nicht
entscheiden. Offen sind mindestens:

- **Welche Felder?** Ex-Tag und Verhältnis sind das Minimum. Die eingebuchte
  Stückzahl wäre als Gegenprobe wertvoll — sie erlaubte, das erfasste Verhältnis
  gegen den tatsächlichen Depotbestand zu prüfen.
- **Wie stabil ist die Formulierung?** "20:1", "im Verhältnis 1:20", "je 1 alte
  Aktie 19 zusätzliche" meinen dasselbe und lesen sich völlig verschieden. Bei
  Kauf- und Verkaufsabrechnungen half die feste Tabellenstruktur; eine
  Kapitalmassnahmen-Mitteilung ist eher Fliesstext.
- **Eigener `DocumentType::Split` in `Documents.xml`?** Das hiesse je Bank eine
  weitere Konfigurationssektion pflegen — für ein Ereignis, das je Aktie alle
  paar Jahre einmal vorkommt.
- **Lohnt die Erkennung per Drag+Drop?** `MainWindow::handleDroppedDocument()`
  müsste den neuen Typ mitbehandeln und `ViewShareSplitEdit` öffnen.

Nächster Schritt: zwei, drei reale Split-Mitteilungen sammeln und daran prüfen,
ob die Felder überhaupt zuverlässig zu treffen sind. Da der Dokumentpfad seit
Phase 3a gespeichert wird, sammeln sich die Belege dafür ohnehin an.

---

### Spin-offs und Kapitalmaßnahmen mit Barkomponente nicht abgedeckt (07.08.2026)

Bewusst aus "Aktiensplits werden nicht behandelt" ausgeklammert (Nessies
Entscheidung 07.08.2026, Punkt 4 dort): Spin-offs (ein Teil der Position
wird zu einer neuen, eigenständigen Aktie) und Kapitalmaßnahmen mit
Barkomponente sind fachlich keine reine Stückelungsänderung — anders als
beim Split ist der Wert dabei NICHT invariant, `ShareSplitAdjuster`s
Grundannahme (Stückzahl × Preis bleibt gleich) trifft nicht zu. Eigenes
Feature, falls der Fall in einem realen Depot auftritt.

### Kopfzeile des Details-Dialogs im Neuberechnungs-Fall irrefuehrend (11.08.2026)

`ViewSaleEdit::showBuyDetails()` beschriftet den Dialog anhand von
`SaleBuyDetailSummary::editMode`, also danach, ob ein gespeicherter Verkauf
geladen ist. Beim juengsten Verkauf wird die Zuteilung aber live neu
gerechnet — die Beschriftung "Tatsaechliche FIFO-Zuteilung des gespeicherten
Verkaufs" trifft dann nicht zu.

Sinnvoll waere ein dritter Zustand, der die Neuberechnung des juengsten
Verkaufs als solche benennt. Bewusst nicht im Rahmen des Brokerage-Bugfixes
geaendert (ein Anliegen pro Commit).

### Kaufkurs im Details-Dialog nur mit zwei Nachkommastellen (11.08.2026)

Die Spalte Kaufkurs zeigt `formatMoney()`, also zwei Stellen. Bei
split-bereinigten Kursen entsteht dadurch eine Zeile, die zum Nachrechnen
einlaedt und dabei scheitert: 200,0000 Stk. mal 48,59 EUR ergibt 9.718,00 EUR,
angezeigt werden korrekt 9.719,00 EUR (tatsaechlicher Kurs 48,595 EUR). Weil
die Zeile als Gleichung mit Mal- und Gleichheitszeichen aufgebaut ist, faellt
das auf. Vier Nachkommastellen wie in der Anteile-Spalte wuerden es aufloesen.

### Split-Verhaeltnis: Notation der Bankmitteilungen (11.08.2026, Hinweistext umgesetzt 13.08.2026)

Bankmitteilungen zu Splits nennen ueblicherweise das Zuteilungsverhaeltnis in
der Form "1:19" — je einem gehaltenen Stueck werden 19 ZUSAETZLICHE
eingebucht. Die Anwendung erwartet das Umrechnungsverhaeltnis, hier also 20.
Im Feldfall Alphabet wurde 19 eingetragen; der Fehler ist systematisch immer
genau eins zu klein.

Drei Massnahmen waren denkbar, alle im Split-Dialog:

- **Umgesetzt (13.08.2026):** Ein Hinweistext, der beide Notationen benennt.
  Erste Fassung war ein dauerhaft sichtbares Label unter der Umrechnungszeile
  (`QLabel`, eigene Grid-Zeile in `createSplitDataGroup()`); wirkte im Dialog
  zu aufdringlich (Nessies Einwand 13.08.2026) und wurde durch einen Tooltip
  auf dem Umrechnungs-Feld ersetzt (`m_factorPreview->setToolTip()`), analog
  zum bestehenden Tooltip auf "Kurshistorie" direkt darunter. Text mit
  explizitem Zeilenumbruch nach dem ersten Satz (`\n` im `tr()`-String — Qt
  rendert das im nativen Tooltip als Umbruch, kein Rich-Text noetig). Reine
  Textaenderung, kein Interface- oder Modellzugriff noetig, deshalb ohne
  eigenen Test (der Text ist statisch und immer sichtbar).
- Offen: Eine Plausibilitaetspruefung gegen die eigenen Daten: Bestand vor
  dem Ex-Tag mal Faktor gegen die Stueckzahl auf spaeteren Verkaufsbelegen.
  Im Feldfall haette das den Fehler sofort gemeldet — Bestand 10 mal 19
  ergibt 190, der Verkaufsbeleg lautet auf 200. Kein Parsen noetig. Verwandt,
  aber NICHT dasselbe wie der "Prüfen"-Knopf (`SplitPriceJumpDetector`, siehe
  "Automatische Erkennung split-bereinigter Kurshistorie" unten): dieser
  prueft die Kurshistorie gegen den eingetragenen Faktor, nicht den Bestand
  gegen spaetere Verkaufsbelege — bleibt weiterhin offen.
- Offen: Warnung, wenn der Ex-Tag in der Zukunft oder auf dem heutigen Tag
  liegt. Der Dialog schlaegt das aktuelle Datum vor; im Feldfall wurde es
  unveraendert uebernommen und stand als 10.08.2026 in der Datenbank.

Ebenfalls erwogen (13.08.2026) und bewusst zurueckgestellt: die Eingabe von
"neu:alt" auf "alt:neu" zu drehen, um sich an die Bank-Schreibweise "1:19"
anzunaehern. Loest das eigentliche Problem nicht — "19" in der Bankmitteilung
ist die ZUSAETZLICHE Stueckzahl, nicht die neue Gesamtzahl; das Delta-vs-
Gesamt-Missverstaendnis besteht unabhaengig von der Feldreihenfolge weiter.
Ein Modell, das stattdessen "alt" + "zusaetzlich" abfragt und "neu" daraus
berechnet, wuerde den Fehler strukturell verhindern, aendert aber
`IViewShareSplitEdit::ratioNew()`/`ratioOld()` und damit mehr als nur den
Hinweistext — eigenes Feature, falls gewuenscht.

@note Ein Parser fuer die Split-Mitteilung haette hier nicht geholfen — im
Dokument steht woertlich "1:19". Siehe "Parsing von Split-Mitteilungen der
Banken pruefen".

### Automatische Erkennung split-bereinigter Kurshistorie ("Prüfen"-Knopf, 13.08.2026, Layout korrigiert 14.08.2026)

Der Haken "Kurshistorie vor dem Ex-Tag liegt bereits split-bereinigt vor"
(`prices_adjusted`, siehe "ShareSplitObject / ShareSplitRepository /
ShareSplitAdjuster" oben) musste bislang von Hand eingeschätzt werden.
Nessies Vorschlag 13.08.2026: die App soll selbst in den gespeicherten
Tageswerten nachsehen, ob rund um den Ex-Tag ein Kurssprung erkennbar ist.

**Bewusst kein automatischer/stiller SCHREIBender Lauf.** Erwogen war
13.08.2026, die Prüfung automatisch nach jedem Tageswert-Abruf laufen zu
lassen und bei Widerspruch eine Startmeldung zu zeigen (Phase 4b im
Phasenplan oben) — zunächst zugunsten dieses expliziten "Prüfen"-Knopfs
zurückgestellt, den der Nutzer selbst betätigt. Grund war dieselbe
Zurückhaltung gegenüber stillen Korrekturen gespeicherter Daten wie bei
`ShareUpdateRules` (siehe dort). Nessies eigene Spezifikation vom 13.08.2026:
"Wenn alle Daten für eine Ermittlung vorhanden sind und die Ermittlung
eindeutig ist, wird der Hinweis angezeigt und der Haken gesetzt. Sollte es
nicht eindeutig sein, kommt der Hinweis, dass der Haken nicht automatisch
gesetzt werden konnte und ein manuelles Setzen nötig ist."

Phase 4b wurde am 20.08.2026 doch noch nachgezogen — als reine LESE-Prüfung,
die diese Zurückhaltung fortsetzt statt ihr zu widersprechen: siehe
"Automatische Nachprüfung nach Tageswert-Abruf" unten.

**Algorithmus (`SplitPriceJumpDetector`, `app/utils/`).** Zustandslos und
datenbankfrei wie `ShareSplitAdjuster`/`ShareSplitHint` — die Kurshistorie
kommt als Parameter herein (`IModelShareSplitEdit::dailyValuesInRange()`,
reine Weiterleitung an `DailyValuesRepository::findByShareAndDateRange()`).
Verglichen wird der letzte verfügbare Schlusskurs vor mit dem ersten
verfügbaren Schlusskurs nach dem Ex-Tag, in einem Suchfenster von
standardmässig 15 Kalendertagen (`kDefaultMaxLookbackDays`), begrenzt durch
benachbarte Splits derselben Aktie (der Ex-Tag selbst zählt als "davor",
dieselbe Konvention wie `ShareSplitAdjuster::volumeFactor()`). Vier
Ergebnisse:

| Ergebnis | Bedeutung | Wirkung |
| --- | --- | --- |
| `Adjusted` | Kein Sprung — Verhältnis nah bei 1,0 (±15 %) | Haken automatisch GESETZT |
| `NotAdjusted` | Sprung nah beim erwarteten Faktor (±20 %) | Haken automatisch ENTFERNT |
| `Ambiguous` | Kurse vorhanden, aber weder eindeutig Sprung noch eindeutig kein Sprung | Haken unverändert, Hinweistext |
| `InsufficientData` | Keine (ausreichenden) Kursdaten im Suchfenster | Haken unverändert, Hinweistext |

@note Bei Split-Verhältnissen nah bei 1 (z. B. 5:4, Faktor 1,25) überlappen
sich die beiden Toleranzbänder (um 1,0 und um den Faktor) — das Ergebnis
fällt dann bewusst häufiger auf `Ambiguous`, statt auf Verdacht zu raten.
Bekannte, akzeptierte Einschränkung.

**Bedienung und Layout (mehrfach korrigiert 14.08.2026).** Der
"Prüfen"-Knopf sitzt in einer eigenen "Prüfung:"-Zeile unterhalb von
"Kurshistorie:", zusammen mit einem read-only, zweizeiligen Ergebnisfeld
(`QPlainTextEdit`, Objektname `priceJumpResult`). Erste Fassung zeigte das
Ergebnis in einem `QLabel` direkt neben der Checkbox — wirkte unruhig, weil
sich seine Höhe je nach Textlänge änderte und dadurch alles darunter
(Kommentar, Dokument, Buttons) beim Prüfen bzw. Zurücksetzen im Dialog nach
unten bzw. wieder nach oben sprang. Das Ergebnisfeld hat seither eine feste,
aus der Zeilenhöhe der Schrift abgeleitete Zweizeilen-Höhe (`QFontMetrics`
statt fester Pixelwert, bleibt so auch bei grösseren Systemschriften
zweizeilig). Label und Knopf sind an der Oberkante des Feldes ausgerichtet
(`Qt::AlignTop`) statt vertikal zentriert — bei einer zweizeiligen Box wirkte
Zentrierung "abgesackt". Die Checkbox-Zeile bekam aus demselben Grund eine
explizite `UiConstants::kFieldHeight`-Höhe zurück: solange sie sich die Zeile
mit dem Knopf teilte, sorgte dessen Höhe automatisch für ein einheitliches
Zeilenmass; seit der Knopf in die eigene "Prüfung:"-Zeile umgezogen ist,
musste die Checkbox das selbst bekommen — sonst wirkte der Zeilenabstand zu
ihren Nachbarn kleiner als überall sonst im Dialog.

**Einfärbung.** Trotz vier Ergebnistypen gibt es fürs Auge nur zwei Zustände
(`IViewShareSplitEdit::PriceJumpTone`): `Adopted` (Haken automatisch
gesetzt/entfernt, grüner Text) oder `ManualDecisionNeeded` (Ergebnis
uneindeutig oder Daten fehlen, roter Text). Dieselben Hex-Werte wie
`AppSettings`' Erfolg-/Fehler-Logfarben (`#388e3c`/`#d32f2f`) — laut deren
Kommentar bewusst kontrastreich auf hellem wie dunklem Hintergrund, nicht an
ein bestimmtes Theme angepasst. Bewusst eine eigene Konstante in
`ViewShareSplitEdit.cpp` statt über `AppSettings::logColorAt()` bezogen: die
Logfarben sind über `LoggerSettingsForm` frei änderbar, das Ergebnisfeld hat
mit dem Log-Fenster nichts zu tun. Beim Zurücksetzen der Maske bzw. beim
Laden eines anderen Splits wird die Farbe mit auf den ungefärbten
Ausgangszustand zurückgesetzt (`resetPriceJumpResult()`), sonst bliebe der
Platzhaltertext "Noch nicht geprüft …" fälschlich rot oder grün gefärbt.

### Automatische Nachprüfung nach Tageswert-Abruf (Phase 4b, 20.08.2026)

Phase 4b des Phasenplans oben — am 13.08.2026 zugunsten des "Prüfen"-Knopfs
zurückgestellt (siehe "Automatische Erkennung split-bereinigter
Kurshistorie" oben) — auf Nessies Wunsch am 20.08.2026 doch nachgezogen.

**Warum das der Zurückhaltung vom 13.08.2026 nicht widerspricht.** Die
damalige Entscheidung richtete sich gegen ein stilles SCHREIBEN von
`prices_adjusted` ohne Nutzeraktion — dieselbe Linie wie `ShareUpdateRules`.
Phase 4b schreibt nichts: sie LIEST nach jedem Tageswert-Abruf nach, ob der
gespeicherte Haken noch zur (jetzt ggf. aktualisierten) Kurshistorie passt,
und macht bei einem Widerspruch lediglich sichtbar darauf aufmerksam — exakt
die gleiche Holschuld-Umkehr wie bei `warnAboutSharesWithoutDailyValues()`
(siehe dort): der Nutzer wird informiert, die eigentliche Korrektur bleibt
ihm im Split-Dialog (dem "Prüfen"-Knopf von Phase 4a) überlassen. Der
"Prüfen"-Knopf bleibt damit der einzige Weg, wie `prices_adjusted`
tatsächlich verändert wird — Phase 4b ist ein zusätzliches Sicherheitsnetz
für Fälle, die der Nutzer nicht von sich aus erneut prüft (typischerweise:
die Kursquelle liefert die Historie zu einem späteren Abrufzeitpunkt anders
bereinigt als beim Erfassen des Splits).

**`SplitAdjustmentAudit` (`app/utils/`).** Zustandslos und datenbankfrei wie
`SplitPriceJumpDetector`, auf dem es direkt aufbaut: `check(splits,
dailyValues)` prüft für jeden übergebenen Split, ob dessen
`SplitPriceJumpDetector::detect()`-Ergebnis dem gespeicherten
`pricesAdjusted()` widerspricht — Nachbar-Splits derselben Aktie begrenzen je
geprüftem Split das Suchfenster, exakt dieselbe Logik wie
`PresenterShareSplitEdit::onCheckPriceJump()` (der geprüfte Split zählt nicht
als eigener Nachbar). `Ambiguous`- und `InsufficientData`-Ergebnisse zählen
NIE als Widerspruch — dieselbe Vorsicht wie beim "Prüfen"-Knopf: ein
Verdachtsfall, den der Nutzer ohnehin nicht auflösen könnte, soll nicht als
Meldung erscheinen.

**Verdrahtung in `MainWindow`.**

| Zeitpunkt | Methode | Wirkung |
| --- | --- | --- |
| Jeder abgeschlossene Tageswert-Abruf (`onDailyValuesUpdated()`, Zweig `Finished`) | `refreshSplitAdjustmentWarningsForShare()` | Prüft NUR die gerade aktualisierte Aktie neu (ersetzt ihre vorherigen Einträge in `m_splitAdjustmentWarnings`); bei ≥ 1 Widerspruch sofort eine Statusmeldung, kein modaler Dialog — ein "Alle aktualisieren"-Lauf über N Aktien soll nicht N Dialoge auslösen. Läuft unabhängig davon, ob der Abruf neue Tageswert-Zeilen brachte: ein Split kann auch ohne neuen Abruf zwischenzeitlich angelegt/geändert worden sein. |
| Programmstart, nach `populatePortfolioTables()` | `populateSplitAdjustmentWarnings()` | Baut `m_splitAdjustmentWarnings` komplett neu auf, über alle Aktien mit mindestens einem Split — deckt auch Widersprüche ab, die während der letzten Sitzung entstanden sind, ohne dass die betroffene Aktie danach erneut aktualisiert wurde. |
| Programmstart, verzögert per `QTimer::singleShot(0, …)` | `warnAboutSplitAdjustmentDiscrepancies()` | Modaler Hinweis, analog `warnAboutSharesWithoutDailyValues()` — nur wenn `m_showStartupWarnings` (Produktivkonstruktor), aus demselben Grund untestbar (siehe dort). |

`buildSplitAdjustmentWarningMessage()` ist `public static`, gleiche
Begründung wie `buildDailyValuesWarningMessage()`: der Meldungstext bleibt
ohne `MainWindow` und ohne modalen Dialog prüfbar. `populateSplitAdjustmentWarnings()`
ist bewusst NICHT Teil von `populatePortfolioTables()`
— anders als `m_sharesMissingDailyValues` (das ohne zusätzlichen
Datenbankzugriff aus der ohnehin laufenden Schleife mitfällt) bräuchte das
hier je Aktie mit Splits einen eigenen Zugriff auf Splits UND komplette
Kurshistorie; `populatePortfolioTables()` läuft aber bei jeder
Tabellen-Neuaufbau, auch nach Beleg-Änderungen ganz ohne Split-Bezug.

### Bruchstücke bei Reverse-Splits nicht abgedeckt (07.08.2026, Lösung ohne neuen Code gefunden 14.08.2026)

Bewusst aus "Aktiensplits werden nicht behandelt" ausgeklammert (Nessies
Entscheidung 07.08.2026, Punkt 5 dort): bei einem Reverse-Split zahlt die
Bank Bruchstücke (Spitzen, die sich nicht glatt zusammenlegen lassen) bar
aus. `ShareSplitAdjuster` bildet nur die glatte Umrechnung ab; eine solche
Barkomponente lässt sich mit dem aktuellen Modell nicht abbilden.

**Nessies Gegenfrage (14.08.2026):** Braucht es dafür wirklich ein eigenes
Feature (eine vollständige, FIFO-basierte Abbildung der Bruchstücks-
Teilveräußerung), oder reicht es, die Bruchstücke als ganz normalen Verkauf
zu erfassen und den Split separat einzutragen?

**Antwort: Ja, das reicht — kein neuer Code nötig.** Ausschlaggebend ist die
Randregel in `ShareSplitAdjuster::volumeFactor()`: ein Beleg, der EXAKT AUF
dem Split-Stichtag datiert ist, wird von genau diesem Split nicht mehr
mitgerechnet (`split.date() > date`, echt größer) — er gilt bereits als im
NEUEN (Nach-Split-)Maßstab erfasst. Ein auf den Stichtag datierter Verkauf
für die Bruchstücke ist damit automatisch korrekt vom Split entkoppelt,
sofern seine Menge im neuen statt im alten Maßstab eingetragen wird.

**Vorgehen:**

1. Verkauf erfassen: Datum = Ex-Tag des Splits, Menge = Bruchstücke im
   NEUEN Maßstab (alte Bruchstücks-Stückzahl × Split-Faktor), Kurs =
   Auszahlungsbetrag der Bank ÷ diese Menge.
2. Den Split wie gewohnt erfassen (Verhältnis, Ex-Tag). Die Reihenfolge der
   Erfassung (Verkauf zuerst oder Split zuerst) spielt keine Rolle, da
   `ShareSplitAdjuster`/`SaleFifoAllocator` beide Datensätze bei jeder
   Anzeige aus der Datenbank neu berechnen, nicht inkrementell fortschreiben.

**Beispielrechnung** (105 alte Stücke, Reverse-Split 1:10, Faktor 0,1):
100 alte Stücke legen sich glatt zu 10 neuen zusammen; 5 alte Stücke sind
das Bruchstück. Verkauf: Datum = Stichtag, Menge = 5 × 0,1 = 0,5, Kurs =
Auszahlungsbetrag ÷ 0,5. Nachgerechnet über `SaleFifoAllocator::allocate()`:
die Kauf-Reste (105 alt) werden auf 10,5 heutige Stücke skaliert, der
Verkauf (0,5, wegen Stichtag-Datum nicht nochmal skaliert) wird abgezogen →
10,0 verbleiben, zurückgerechnet auf die Beleg-Skala der Käufe = 100 alte
Stücke. Stimmt mit der erwarteten Aufteilung überein.

**Umsetzung (14.08.2026):** Da kein neuer Fachcode nötig ist, bleibt es bei
dieser Dokumentation plus einem eigenen Hinweis-Knopf im Split-Dialog.
Ein erster Anlauf mit einem Tooltip auf den Verhältnis-Feldern wurde
verworfen (Nessies Feedback): der Tooltip verschwand beim Wegbewegen der
Maus wieder und verwies auf diese Datei, auf die der Benutzer keinen
Zugriff hat. Stattdessen jetzt der Knopf "Hinweis Reverse-Split" direkt in
der Verhältnis-Zeile (`ViewShareSplitEdit::createSplitDataGroup()`,
`m_btnReverseSplitHint`), dauerhaft sichtbar. Er öffnet einen
`OwnMessageBox::information()`-Dialog mit dem obigen Vorgehen in
Klartext — bewusst ohne Verweis auf diese Datei oder auf interne
Klassennamen (`reverseSplitHintMessage()`). Ist im Formular ein echtes
Reverse-Split-Verhältnis eingetragen (neu < alt, beide > 0), rechnet der
Text mit genau diesem Verhältnis statt mit dem festen 1:10-Beispiel oben.

### Diagnose-Knopf hinter einen Debug-Modus legen (06.08.2026)

Der Knopf "Diagnose speichern…" im Depotwert-Chart bleibt vorerst dauerhaft
sichtbar, da sich der Export als nützlich erwiesen hat. Später sollte er über
eine Einstellung ein- und ausblendbar sein, damit er im Normalbetrieb nicht
im Weg steht.

### Zeitraum-Einstellungen des Charts persistieren (05.08.2026)

Start-Datum, Interval und Anzahl des Depotwert-Charts werden bei jedem Start
auf die Vorgabe zurückgesetzt (heute / Jahr / 1). Sinnvoll wäre, sie je Chart
in `AppSettings` zu speichern.

### Marktwert-Chart (05.08.2026)

Das Gegenstück zum Depotwert-Chart, das nur reine Kursgewinne berücksichtigt
(keine Dividenden, keine Kosten, keine Steuern). Bewusst zurückgestellt, bis
der Depotwert-Chart im Alltag steht — die Formel dafür ist noch nicht
abgestimmt. `ViewPortfolioChart` ist so gebaut, dass ein Modus-Flag analog zu
`ViewShareDetails` nachgerüstet werden kann.

### Vermögenskurve als zweite Darstellung (05.08.2026)

Der Depotwert-Chart zeigt die reine Wertentwicklung; Ein- und Auszahlungen
verschieben ihn nicht. Daneben wäre eine echte Vermögenskurve denkbar, die
bei einem Kauf über 5.000 Euro auch um 5.000 Euro nach oben springt. Die
Datenbasis ist dieselbe, nur die Aggregation unterscheidet sich —
sinnvollerweise später als per Checkbox umschaltbare zweite Serie im selben
Chart.

### Verwaistes Verzeichnis tests/widgets entfernen (05.08.2026)

`tests/widgets/` enthält `tst_overviewtabwidget.cpp`, hat aber keine
`CMakeLists.txt` und wird von der Root-`CMakeLists.txt` nicht eingebunden.
Dieselbe Datei liegt zusätzlich in `tests/forms/` und wird von dort gebaut —
`tests/widgets/` ist also eine verwaiste Dublette und kann gelöscht werden.

### Übersetzung ist vollständig offen (07.08.2026)

Die Oberfläche ist durchgängig deutsch. Alle Benutzertexte stehen zwar
korrekt in `tr()` und werden von `qt_add_translations()` erfasst, aber
übersetzt wurde bisher nichts: `translations/spm_de.ts` und
`translations/spm_en.ts` sind unbearbeitet, `spm_en.ts` enthält also keine
einzige englische Zeichenkette.

Praktische Folge: ein Start mit englischem Systemgebietsschema zeigt trotzdem
die deutschen Quelltexte, weil Qt bei fehlender Übersetzung auf den
`tr()`-Ausgangstext zurückfällt. Es sieht damit nicht kaputt aus, sondern
schlicht unübersetzt — der Zustand fällt beim Entwickeln deshalb nie auf.

Zu klären wäre zuerst, ob Englisch überhaupt ein Ziel ist. Falls ja, kommen
zwei Punkte hinzu, die heute nirgends geregelt sind: die Zahlen- und
Datumsformatierung hängt an `QLocale::setDefault(QLocale::German)` in
`main.cpp` und im Test-`main()` und müsste dann mitziehen (siehe auch
"Konfigurierbare Locale für Zahlenformat"), und die `.ts`-Dateien müssten in
den regulären Commit-Rhythmus aufgenommen werden, statt nur beim Bauen
erzeugt zu werden.

### Aktien ohne verfügbare Tageswert-Quelle dauerhaft ausnehmen (07.08.2026)

Seit "Tageswert-Historie bei Bestand > 0 erzwingen" (siehe "Erledigt /
Archiv") meldet `MainWindow::warnAboutSharesWithoutDailyValues()` bei jedem
Start alle Aktien mit Bestand, die keine Tageswerte abrufen.

Für ein Papier, zu dem es gar keine Tageswert-Quelle gibt — ein delistetes
etwa, oder eines, das die konfigurierte Quelle schlicht nicht führt — ist
diese Meldung nicht abstellbar, weil der Nutzer den beanstandeten Zustand
nicht beheben kann. Der einzige Ausweg wäre der Verkauf.

Wiederkehrende Meldungen, gegen die sich nichts tun lässt, werden
erfahrungsgemäß pauschal weggeklickt. Damit verliert die Meldung ihre Wirkung
auch für die Fälle, in denen sie berechtigt ist — das ist der eigentliche
Schaden, nicht die Unbequemlichkeit.

Lösungsrichtung: ein Kennzeichen an der Aktie ("keine Tageswert-Quelle
verfügbar"), das sie aus der Meldung herausnimmt und zugleich als bewusste
Entscheidung dokumentiert. Die Aktie bliebe weiterhin aus dem Depotwert-Chart
ausgeschlossen — daran ändert das Kennzeichen nichts, es macht die Lücke nur
gewollt statt versehentlich. Zu klären wäre, ob der Chart solche Positionen
dann sichtbar ausweisen sollte, damit die Kurve nicht unbemerkt zu niedrig
liegt.

@note Noch nicht belegt, dass der Fall in einem realen Depot überhaupt
auftritt (Nessie, 07.08.2026). Erst umsetzen, wenn er auftritt — vorher wäre
es Aufwand für ein hypothetisches Problem.

### Manuelles Theme (Hell/Dunkel) erzwingbar machen (Backlog-Idee, nicht priorisiert, 24.07.2026)

Im Zuge des Bugfixes "Log-Meldungsfarben theme-neutral" (siehe Erledigt/
Archiv weiter unten) kam die Frage auf, ob sich ein Dark Theme künftig
*manuell* in der App aktivieren lassen sollte — unabhängig von der
automatischen System-Theme-Erkennung, die im Linux-AppImage mangels
Platform-Theme-Plugin nicht funktioniert (siehe dort für die genaue
Ursache).

Der entscheidende Unterschied zur (fehlschlagenden) automatischen Erkennung:
ein manuell erzwungenes Theme über `QStyleFactory::create("Fusion")` +
eine fest definierte `QPalette` läuft komplett innerhalb von Qt selbst,
ohne jede Abfrage ans Betriebssystem oder ein externes Plugin — würde also
auch im AppImage zuverlässig funktionieren, wo die automatische Erkennung
versagt.

Mögliche Umsetzung, grob skizziert:
- Neue `AppSettings`-Einstellung, z. B. `theme()` mit Werten "System"
  (aktuelles Verhalten, unverändert) / "Hell" / "Dunkel".
- Bei "Hell"/"Dunkel" wird beim Start `QApplication::setStyle("Fusion")`
  + eine passende `QPalette` gesetzt.
- Ein Theme-Wechsel würde vermutlich einen Neustart der App erfordern —
  Qt-Stile lassen sich zwar zur Laufzeit umschalten, aber nicht alle
  Widgets aktualisieren sich dabei zuverlässig live.

Zu bedenken: `IconProvider` kennt aktuell nur ein einziges Icon-Set
(`default`), das nicht hell/dunkel-optimiert ist — ein erzwungenes Dark
Theme würde also nur Fenster-Hintergründe/Widgets betreffen, nicht die
Icons selbst (kein Blocker, nur ein kosmetischer Nebenaspekt, den man bei
der Umsetzung im Hinterkopf behalten sollte).

Bewusst zurückgestellt (24.07.2026): reine Idee aus der Diskussion um den
Log-Farben-Bugfix, keine konkrete Anfrage/aktive Aufgabe. Nur vermerkt,
falls später tatsächlich Bedarf für ein erzwingbares Theme entsteht.

### GitHub-Release-Automatisierung für Installer (Backlog-Idee, nicht priorisiert)

Seit 23.07.2026 existiert `.github/workflows/package.yml` (manuell auslösbar
via `workflow_dispatch`), der einen Windows-Installer (Inno Setup) sowie ein
Linux-AppImage baut. Die fertigen Dateien landen dabei aktuell nur als
GitHub-Actions-**Artefakte** am jeweiligen Workflow-Lauf — nicht öffentlich
zugänglich (nur für Personen mit Repo-Zugriff), mit Standard-Aufbewahrung von
90 Tagen, und ohne Verknüpfung zu einer offiziell markierten Version.

Für eine echte Veröffentlichung fehlt noch: ein GitHub-Release-Schritt (z. B.
via `softprops/action-gh-release`), der beide Installer an ein GitHub Release
anhängt — sinnvollerweise ausgelöst durch einen Git-Tag (z. B. `v1.0.0`)
statt weiterhin nur manuell. Damit könnte der Ablauf künftig lauten: Tag
pushen → Installer bauen → automatisch als Release veröffentlichen.

Bewusst zurückgestellt (23.07.2026): Es ist noch keine erste Version zur
Veröffentlichung vorgesehen. Nur als Idee für später vermerkt, sobald
tatsächlich veröffentlicht werden soll — keine aktive Aufgabe.

### Konfigurierbare Locale für Zahlenformat (Backlog-Idee, nicht priorisiert)

Im Zuge des Locale-Bugfixes vom 23.07.2026 (siehe Erledigt/Archiv weiter unten)
kam die Frage auf, ob das Zahlenformat (Dezimaltrennzeichen etc.) künftig pro
Benutzer einstellbar sein sollte, unabhängig von der (ohnehin fest deutschen)
UI-Sprache — z. B. für Schweizer/österreichische Formatierungskonventionen.

Bewusst zurückgestellt (23.07.2026): Die Anwendung ist komplett
deutschsprachig, es gibt aktuell keine konkrete Anfrage dafür, und der Aufwand
(neue `AppSettings`-Einstellung, UI-Dialog, `QLocale::setDefault()` beim Start
aus der gespeicherten Einstellung ableiten, zusätzliche Testfälle) steht in
keinem Verhältnis zum eigentlichen Bugfix. Nur als Idee für die Zukunft
vermerkt, falls tatsächlich mal Bedarf entsteht — keine aktive Aufgabe.

---

## Erledigt / Archiv

### Skalenbewusste Mengenprüfung im Verkaufsformular (Bug, 11.08.2026, gefixt 20.08.2026)

Während der Analyse eines Feldfalls zeigte das Verkaufsformular grüne Haken
und eine vollständige Gewinnermittlung, obwohl die angeforderte Menge
(3.800) die verfügbare (190) deutlich überstieg, beides auf heutiger Skala.
`SaleFifoAllocator::allocate()` deckelte die Zuteilung still nach unten,
statt einen Fehler zu melden. Dass das damalige Ergebnis trotzdem stimmte,
war Zufall — weil die gesamte Position verkauft wurde; bei einem
Teilverkauf wäre es stillschweigend falsch gewesen. Der Punkt stand deshalb
zunächst offen: der Feldfall selbst entstand aus fehlerhaften Split-Daten,
nicht zwingend aus einem Programmfehler, und ob die Lücke im Code
tatsächlich noch bestand, war nicht abschließend geprüft.

**Prüfergebnis (20.08.2026):** Ja, die Lücke bestand weiterhin —
`PresenterSaleEdit` fragte vor dem Speichern an keiner Stelle ab, ob die
verfügbaren Käufe die eingegebene Verkaufsmenge überhaupt decken. Weder das
Live-Feld (`onVolumeOrPriceEdited()`, prüft nur `volume() > 0.0`) noch
`validateInput()` (Pflichtfelder, doppelte Auftragsnummer, doppeltes
Dokument) sahen einen entsprechenden Vergleich vor.

**Fix (20.08.2026):** Zwei neue statische Methoden in `SaleFifoAllocator`
— `totalAvailableVolumeToday()` (Summe der verfügbaren Käufe, skalenbewusst
auf heutige Skala umgerechnet, wie schon intern in `allocate()`) und darauf
aufbauend `isSaleVolumeCovered()` — machen die bislang implizite Deckelung
explizit prüfbar, ohne das bestehende (weiterhin genutzte) Verhalten von
`allocate()` selbst zu ändern. `PresenterSaleEdit` nutzt beide neu an zwei
Stellen: im Live-Icon (`onVolumeOrPriceEdited()`, läuft bei
`editingFinished`) und zusätzlich blockierend in `validateInput()`
unmittelbar vor dem eigentlichen Speichern — mit einer Meldung, die
angeforderte und verfügbare Menge konkret in Stück beziffert, statt nur
allgemein auf einen Fehler hinzuweisen. Für einen älteren, nicht-jüngsten
Verkauf greift die Prüfung bewusst nicht (neuer private Helfer
`isRequestedVolumeCovered()` gibt dort unbedingt `true` zurück): die
Mengenfelder sind für diesen Fall gesperrt (nur das Dokument ist editierbar,
siehe `ViewSaleEdit::setButtonStates()`, `readOnlyMode`), und
`loadAvailableBuysForDepotExcludingSale()` bucht ohnehin nur den einen
bearbeiteten Verkauf zurück, nicht die vollständige FIFO-Historie zwischen
ihm und heute — eine Prüfung wäre dort weder nötig noch belastbar.

Tests: `tst_salefifoallocator.cpp` (u. a. der Feldfall selbst — 3.800 gegen
190 — sowie ein skalenbewusster Split-Fall zwischen Kauf- und
Verkaufsdatum) und `tst_mainwindow.cpp` (`TestSalesForm`: Live-Icon bei zu
hoher Menge, blockiertes `onSave()` samt Fehlertext, Grenzfall exakte
Deckung bleibt erlaubt, Dokument-only-Edit eines älteren Verkaufs bleibt
unberührt).

### Footer-Lücke bei freistehenden Kosteneinträgen (Bug, 05.08.2026, gefixt 20.08.2026)

`ShareCalculator::compute()` berücksichtigte freistehende Brokerage-Einträge
(weder `buy_guid` noch `sale_guid` gesetzt, angelegt über die
Kosten-Verwaltung) nur in `totalBrokerage`, nicht in `completePurchase` oder
`salePayoutFinal`. Die Spalte "Komplette Entwicklung" war dadurch um deren
Betrag zu hoch — im Grid, im Footer und in `ShareDetailsForm`, sowohl im
Marktwert- als auch im Depotwert-Tab (beide hängen über
`realizedProfitLossWithFees` von `completePurchase` ab). Der Depotwert-Chart
rechnete an dieser Stelle bereits korrekt (`ModelPortfolioChart::
loadPortfolioInput()`, summiert `findByShare()` vollständig inklusive der
freistehenden Einträge) und wich deshalb bis zum Fix vom Footer ab (siehe
"PortfolioChartForm-Details").

**Fix (20.08.2026):** In `ShareCalculator::compute()` wird nach der
Käufe-Schleife zusätzlich über `brokerageRepo.findByShare(guid)` iteriert;
Einträge mit leerem `buyGuid()` UND leerem `saleGuid()` addieren ihre
`brokerageReduction()` zu `completePurchase`. `completePurchaseMarket` bleibt
bewusst unverändert — der Marktwert-Zweig schließt grundsätzlich jede
Brokerage aus, unabhängig davon, ob sie kauf-, verkaufs- oder freistehend
gebunden ist. Damit ist der Fix exakt symmetrisch zur bereits bestehenden
Behandlung kauf-/verkaufsgebundener Brokerage-Einträge weiter oben in
derselben Funktion, und Grid/Footer stimmen jetzt automatisch mit dem
Depotwert-Chart überein. Tests: `tst_sharecalculator.cpp`,
`test_freestandingCost_addedToCompletePurchaseOnly`,
`test_freestandingCost_reducesCompleteEntwicklungBothTabs`,
`test_freestandingCost_doesNotDoubleCountLinkedBrokerage`.

### Tageswert-Historie bei Bestand > 0 erzwingen (Feature, 06.08.2026)

Aktien mit Update-Typ "Markt-Preis" oder "Keine" bauen keine
Tageswert-Historie auf. Ohne diese Historie lassen sie sich an keinem
vergangenen Stichtag bewerten und werden deshalb vollständig aus dem
Depotwert-Chart ausgeschlossen (siehe "PortfolioChartForm-Details",
"Datumsraster, Kurs-Fortschreibung, Aktien ohne Historie"). Solange Anteile
im Bestand sind, ist diese Kombination kein Wunsch des Nutzers, sondern ein
Datenproblem — der Chart zeigt dann eine Kurve, die diese Positionen
stillschweigend weglässt.

#### Die Regel liegt in einem eigenen Modul

`app/utils/ShareUpdateRules.h` hält die Regel als einzige Quelle:

| Funktion | Bedeutung |
| -------- | --------- |
| `requiresDailyValues(volume)` | Bestand vorhanden, Tageswerte also Pflicht |
| `updateTypeIncludesDailyValues(type)` | `Both` oder `DailyValues` |
| `isUpdateTypeAllowed(type, volume)` | Kombination aus beidem |
| `sharesNeedingDailyValues(list)` | Filtert eine Aktienliste auf die Verstöße |

Die Schwelle `kVolumeEpsilon = 1e-9` ist dieselbe wie in
`ModelSaleEdit::loadAvailableBuys()`. Grund: der Bestand entsteht als Summe
über `volume - volumeSold` aller Käufe und kann bei einer vollständig
verkauften Position ein paar ULP neben 0 landen; ein `> 0.0` würde diese
Aktien fälschlich als Bestand werten und eine Meldung auslösen, die sich
nicht abstellen lässt.

`ShareUpdateRules::ShareState` ist bewusst kein `ShareObject`: die Regel muss
ohne Datenbank prüfbar bleiben, und der Bestand ist ohnehin bereits berechnet,
wenn die Aufrufer sie brauchen.

@note Das Modul ist header-only und damit eine bewusste Abweichung von den
übrigen `utils/`-Modulen, die alle ihre `.h`/`.cpp`-Teilung haben. Es enthält
nur eine Handvoll Vergleiche ohne Zustand. Ausschlaggebend war die Testseite:
drei Testziele kompilieren `ViewShareEdit.cpp` und eines `MainWindow.cpp` —
eine eigene Übersetzungseinheit müsste in jedes davon eingetragen werden, und
ein vergessener Eintrag schlägt als Linkerfehler auf, nicht als etwas
Aussagekräftiges. Die anderen `utils/`-Module behalten ihre Teilung, weil sie
echtes Implementierungsgewicht tragen.

#### ViewShareEdit: Auswahl gesperrt, Bestehendes nicht angetastet

`IViewShareEdit::setDailyValuesRequired(bool)` (neu) deaktiviert die Radios
"Markt-Preis" und "Keine" und blendet eine rote Hinweiszeile darunter ein.
`PresenterShareEdit::loadAndPopulate()` ruft die Methode direkt nach
`setCurrentVolume()` auf — die Entscheidung trifft also der Presenter, die
View spiegelt sie nur. Der Aufruf muss NACH `loadShare()` liegen, weil
`loadShare()` den gespeicherten Update-Typ anhakt.

Bei Aktien, die vor dieser Umstellung angelegt wurden, kann eine jetzt
unzulässige Auswahl gespeichert sein. Diese bleibt sichtbar angehakt (Qt
stellt einen deaktivierten `QRadioButton` weiterhin gesetzt dar) — der Dialog
schreibt nichts von selbst um. Bewusste Entscheidung: eine stille Korrektur
gespeicherter Daten beim bloßen Öffnen eines Dialogs wäre für den Nutzer nicht
nachvollziehbar. Die eigentliche Blockade sitzt stattdessen in
`PresenterShareEdit::validateInput()`, das per `isUpdateTypeAllowed()` prüft
und das Speichern mit einer erklärenden Meldung abbricht. Ohne diese zweite
Prüfung käme der unzulässige Wert unverändert wieder in die Datenbank zurück.

Geprüft wird dabei ausschliesslich die AKTIVE Änderung: der Presenter merkt
sich in `m_loadedUpdateType`, was beim Öffnen aus der Datenbank kam, und
lässt einen unveränderten Altbestandswert passieren.

Diese Lockerung kam nachträglich hinzu (06.08.2026), nachdem beim Durchsehen
ein Fall auffiel, den die strenge Variante in eine Sackgasse geführt hätte:
eine Aktie, für die es gar keine Tageswert-Quelle gibt — ein delistetes
Papier etwa, oder eines, das die Quelle schlicht nicht führt. An einer
solchen Aktie liesse sich mit der strengen Prüfung überhaupt nichts mehr
ändern, nicht einmal eine Namenskorrektur, und der einzige Ausweg wäre der
Verkauf gewesen.

Der Weg bleibt trotzdem eine Einbahnstrasse: "Beide" und "Tages-Werte" sind
jederzeit wählbar, zurück auf "Markt-Preis" oder "Keine" kommt man bei
Bestand nicht mehr — auch nicht von einem unzulässigen Altwert auf einen
anderen unzulässigen Wert. Die Startmeldung nennt solche Aktien weiterhin,
denn die Datenlücke besteht ja tatsächlich fort.

#### ViewShareAdd hatte nie eine Auswahl

Die ursprüngliche Notiz nannte `ViewShareAdd` als betroffen. Das war eine
Fehlannahme: der Anlage-Dialog bietet gar keine Update-Typ-Auswahl an, weder
in `IViewShareAdd` noch als Widget. `PresenterShareAdd::onSave()` rief
`setUpdateType()` nie auf, die neue Aktie erhielt also den Vorgabewert
`ShareUpdateType::Both` aus `ShareObject`. Faktisch war der Dialog damit schon
korrekt — allerdings nur beiläufig.

`onSave()` setzt den Wert jetzt explizit. `validateInput()` erzwingt dort
bereits `volume() > 0`, eine neu angelegte Aktie hat also immer Bestand und
braucht zwingend Tageswerte. Ohne die explizite Zeile würde eine spätere
Änderung des Vorgabewerts in `ShareObject` stillschweigend Aktien ohne
Kurshistorie anlegen.

#### Startmeldung für den Altbestand

Aktien, die vor dieser Umstellung angelegt wurden, erreicht die Sperre im
Editier-Dialog nicht von selbst — ohne aktiven Hinweis bliebe der Fehlstand
unbemerkt. `MainWindow::warnAboutSharesWithoutDailyValues()` zeigt deshalb
einmal je Programmstart eine Meldung mit Name, WKN und aktuellem Update-Typ
der betroffenen Aktien, dazu die Begründung und die Handlungsanweisung.

Die Meldung nennt ausdrücklich, dass Wartezeit echter Datenverlust ist: die
Quellen liefern nur ein begrenztes Zeitfenster rückwirkend, die in der
Zwischenzeit ausgelaufenen Tage sind dauerhaft weg (dieselbe Einschränkung wie
unter "Aktiensplits werden nicht behandelt" beschrieben). Kein "nicht mehr
anzeigen"-Haken — die Meldung verschwindet von selbst, sobald die Einstellung
stimmt. Zusätzlich geht eine Zeile in den Meldungsbereich, damit der Hinweis
nach dem Schließen des Dialogs nicht spurlos verschwindet.

Zwei Umsetzungsdetails:

- Die Liste entsteht ohne zusätzliche Datenbankzugriffe in
  `populatePortfolioTables()`. Diese Methode berechnet je Aktie ohnehin
  `ShareValues`, und `ShareValues::volume` IST der gehaltene Bestand — es
  bleibt eine Zuweisung je Aktie.
- Der Aufruf läuft per `QTimer::singleShot(0, ...)` am Ende des
  "Portfolio geladen"-Zweigs von `initialize()`. Ohne die Verzögerung
  erschiene der modale Dialog vor dem fertig gezeichneten Hauptfenster, der
  Nutzer sähe seinen Kontext also nicht (gleiche Begründung wie beim
  Tray-Start).

Die Beschriftungen in der Meldung sind wortgleich zu den Radiobuttons in
`ViewShareEdit`, damit der Nutzer die genannte Einstellung dort direkt
wiederfindet.

Die Meldung erscheint nur im Produktivkonstruktor. Der bereits vorhandene
Test-Konstruktor `MainWindow(QNetworkAccessManager*, QWidget*)` setzt
`m_showStartupWarnings` auf `false` — das ist seit 06.08.2026 der zweite
Unterschied zwischen den beiden Konstruktoren neben der Parser-Verdrahtung.

Notwendig wurde das, weil zahlreiche Tests in `tst_mainwindow.cpp` Aktien mit
Update-Typ `MarketPrice` samt Käufen anlegen — also genau den Fall, den die
Meldung anprangert — und anschliessend `QApplication::processEvents()` rufen.
Der verzögerte Aufruf würde dort feuern und den Test in `exec()` dauerhaft
blockieren, im CI-Runner ebenso. Die Alternative, alle betroffenen Tests
umzuschreiben, hätte deren Prüfgegenstand verwässert; die Alternative, den
Dialog nicht-modal zu zeigen, hätte ein Fremdwidget als Kind des Hauptfensters
hinterlassen, das die widgetsuchenden Testhelfer stört.

Untestbar bleibt dadurch allerdings nur noch das Öffnen des Dialogs selbst.
Der Textaufbau steckt in zwei `public static`-Helfern —
`MainWindow::updateTypeLabel()` und
`MainWindow::buildDailyValuesWarningMessage()` — nach derselben Konvention
wie `buildDailyValuesUrl()` und `shouldMinimizeToTray()`.

Ausschlaggebend war die Fehlerform: liefe der Textaufbau falsch, bliebe die
Meldung leer oder nennte falsche Beschriftungen. Das Ausbleiben eines Dialogs
sieht für den Nutzer aber genauso aus wie "alles in Ordnung" — ein solcher
Fehler würde nie auffallen. Genau diese Klasse von Fehlern soll nicht hinter
einem untestbaren `exec()` verschwinden.

@note Der verbleibende Rest — Liste leer prüfen, Dialog öffnen — ist
konsistent mit der bestehenden Projektkonvention, `exec()`-getriebene
Dialogpfade nicht zu automatisieren (siehe `openCaptureDialog()` und die
`onBrowseDocument()`-Methoden der fünf Editier-Dialoge).

### Die Anwendung darf nur einmal gestartet werden (Feature, 03.08.2026)

Ergänzung zum Tray-Feature (siehe direkt darunter) — ein zweiter Start soll
nicht dieselbe Portfolio-SQLite-Datei parallel öffnen (Datenintegrität),
sondern stattdessen die bereits laufende Instanz in den Vordergrund holen.

Neue Klasse `SingleInstanceGuard` (`app/core/`, analog `Database`/
`DocumentRootMigrator` dort):

- **Sperrmechanismus:** `QLockFile` statt des klassischen
  `QSharedMemory`-Tricks — erkennt einen verwaisten Lock nach einem Absturz
  der vorherigen Instanz automatisch selbst (prüft, ob die im Lock
  gespeicherte PID noch existiert) und verwirft ihn, ohne dass manuelles
  Aufräumen nötig wäre. Die Lock-Datei liegt im selben
  `QStandardPaths::AppConfigLocation`-Verzeichnis wie `settings.ini`
  (`AppStartup::settingsPath()`) — aus demselben Grund stabil über
  AppImage/Windows-Installer/portable Builds hinweg (siehe "settings.ini
  nicht persistent im AppImage").
- **Benachrichtigung der laufenden Instanz:** `QLocalServer`/
  `QLocalSocket` — eine zweite gestartete Instanz, die den Lock bereits
  belegt vorfindet, verbindet sich kurz zum lokalen Server der ersten
  Instanz und schickt ein "Aktivieren"-Signal (`activationRequested()`),
  bevor sie sich selbst beendet.
- `SingleInstanceGuard::buildServerName(organizationName, applicationName)`
  — `public static`, reine String-Logik (Org-/App-Name → Bezeichner für
  Lock-Datei und `QLocalServer`-Name), bewusst von `tryAcquire()` getrennt
  und direkt testbar, gleiches Testbarkeits-Prinzip wie
  `buildDailyValuesUrl()`. `tryAcquire()`/`activationRequested()` selbst
  (echtes `QLockFile` + `QLocalServer`/`QLocalSocket` über mehrere echte
  Prozesse) bleiben bewusst ungetestet — kein sauberer Weg, das
  deterministisch in einem einzelnen QTest-Lauf zu simulieren, analog zu
  anderen bereits akzeptierten Testlücken bei echten
  System-Interaktionen (`QDialog::exec()`, echte `QSoundEffect`-Wiedergabe).

**Ablauf in `main.cpp`:** direkt nach `app.setWindowIcon(...)`, noch VOR
`AppStartup::loadSettings()`/`openDatabase()`, damit eine zweite Instanz die
Datenbank gar nicht erst berührt. Bei belegtem Lock zeigt `main()` einen
kurzen `QMessageBox::information()`-Hinweis ("läuft bereits") und beendet
sich sofort mit `return 0` (Nessies Vorgabe 03.08.2026: Kombination aus
"laufende Instanz nach vorne holen" **und** "kurzer Hinweis in der zweiten
Instanz", nicht nur eines von beidem).

**Wiederverwendung von `MainWindow::restoreFromTray()`:** statt neuen Code
fürs "nach-vorne-Holen" zu schreiben, wurde die bestehende
`restoreFromTray()`-Methode (bisher nur für den Tray-Wiederherstellen-Pfad
gedacht) von `private` auf `public` gestellt und `SingleInstanceGuard::
activationRequested()` direkt in `main.cpp` daran verbunden — sie
funktioniert unverändert korrekt, egal ob das Fenster gerade im Tray
versteckt, nur minimiert oder einfach von anderen Fenstern verdeckt ist
(`m_trayIcon->hide()` darin ist ein No-op, wenn kein Tray-Icon sichtbar
war).

### Minimieren wahlweise in Taskleiste oder Tray (Feature, 03.08.2026)

Neue Option `AppSettings::trayOnMinimizeEnabled()` (Standard: **aus**, opt-in
— bestehende Installationen verhalten sich nach dem Update unverändert),
konfigurierbar über einen neuen Dialog `TraySettingsForm` (Einstellungen →
"&Tray...", gleiches leichtgewichtige Einzel-`QDialog`-Muster wie
`BackupSettingsForm`/`SoundSettingsForm` — keine eigene
IView/IModel/Presenter-Trias nötig, da reine Einstellungsmaske ohne
eigenständige Fachlogik).

Ist die Option aktiv **und** ein Infobereich auf dem System verfügbar
(`QSystemTrayIcon::isSystemTrayAvailable()`), versteckt `MainWindow` sich
beim Minimieren vollständig statt in der Taskleiste zu erscheinen, und zeigt
stattdessen ein Symbol im Infobereich (`m_trayIcon`, siehe
`IconProvider::appIcon()` weiter unten für das verwendete Icon). Ein
einfacher Klick auf das Symbol (`QSystemTrayIcon::Trigger`) oder "Anzeigen"
im Kontextmenü (`m_actionTrayShow`, Icon: `IconProvider::ShowWindow`) stellt
das Fenster wieder her; das Kontextmenü enthält zusätzlich die bereits
vorhandene `m_actionQuit`. Bewusst **nicht** umgesetzt: Schließen per
X-Button bleibt unverändert "Beenden" — das war nicht Teil dieses Features
(Nessies Entscheidung 03.08.2026).

**Umsetzung:**

- `MainWindow::setupTrayIcon()` — erstellt `m_trayIcon` inkl. Kontextmenü nur
  einmal beim Start, no-op falls kein Tray verfügbar ist (u. a. manche
  Linux-Desktopumgebungen sowie headless/offscreen CI — dort bleibt
  `m_trayIcon == nullptr`, das Feature ist dann automatisch komplett
  inaktiv). Das Icon selbst ist nur sichtbar, während das Fenster tatsächlich
  ins Tray versteckt ist — nicht dauerhaft ab dem Programmstart.
- `MainWindow::changeEvent()` (neu überschrieben) reagiert auf
  `QEvent::WindowStateChange` in Kombination mit `isMinimized()`. Das
  eigentliche `hide()`/Tray-Icon-`show()` läuft verzögert über
  `QTimer::singleShot(0, ...)` statt direkt im Event-Handler — gleiches
  Muster wie an anderer Stelle in dieser Klasse für Operationen, die erst
  nach dem laufenden Event-Durchlauf sicher ausgeführt werden können, da ein
  direktes `hide()` sich auf manchen Plattformen mit dem noch nicht
  abgeschlossenen Fensterzustandswechsel des Fenstermanagers überschneiden
  kann.
- Die reine Entscheidungslogik ist als **`public static`**
  `MainWindow::shouldMinimizeToTray(bool settingEnabled, bool
  trayIconAvailable)` ausgelagert (gleiches Testbarkeits-Prinzip wie
  `buildDailyValuesUrl()`/`resolveShareGuidForDocument()`) — direkt testbar
  ohne echtes `QSystemTrayIcon` und unabhängig davon, ob in der
  Test-/CI-Umgebung tatsächlich ein Infobereich verfügbar ist.
- `MainWindow::restoreFromTray()` — versteckt `m_trayIcon` wieder und holt
  das Fenster per `showNormal()` + `raise()` + `activateWindow()` zurück;
  verbunden sowohl mit dem Einfachklick auf das Tray-Icon als auch mit
  `m_actionTrayShow`.

**Bugfix (03.08.2026, noch am selben Tag, Nessies Rückmeldung):** Die erste
Fassung verwendete für `m_trayIcon` selbst ebenfalls
`IconProvider::ShowWindow` (`show_window_24.png`, ein einzelnes 24px-
Pixmap) — das passte als Kontextmenü-Icon (normale `QIcon`-Menü-Größe), sah
als eigentliches, von `QSystemTrayIcon` gerendertes Tray-Icon aber "komisch"
aus: das System skaliert ein einzelnes festes Pixmap auf seine eigene,
meist andere Tray-Zielgröße (z. B. 16px unter Windows), was verzerrt/
unscharf wirkt. Zudem war das Motiv (ein "Fenster anzeigen"-Symbol)
ohnehin nicht als Anwendungslogo gedacht.

Es existierten bereits vier PNG-Auflösungen eines eigenen App-Icons
(`app_icon_16/32/48/256.png`) auf der Festplatte, aber ohne
`resources.qrc`-Eintrag. Behoben durch:

- `resources.qrc`: neuer `<qresource prefix="/icons/app">`-Block mit den
  vier PNGs (`app_icon.ico` bewusst ausgenommen — reines
  Windows-Installer-Material für Inno Setup, kein Qt-Ressourcen-Icon).
- `IconProvider::appIcon()` (neu, `public static`): kombiniert alle vier
  Auflösungen per `QIcon::addFile()` zu einem mehrstufigen `QIcon`, aus dem
  Qt automatisch die zur jeweiligen Zielgröße passende Auflösung wählt.
  Bewusst getrennt vom `IconName`-Enum/Icon-Set-Mechanismus (der schaltet
  zwischen *Stilen* desselben Icons um, hier geht es um *Auflösungsstufen*
  derselben, festen App-Identität, unabhängig vom aktiven Icon-Set).
- `MainWindow::setupTrayIcon()` verwendet jetzt `IconProvider::appIcon()`
  für `m_trayIcon` selbst; das Kontextmenü-Icon bei "Anzeigen"
  (`m_actionTrayShow`) bleibt unverändert bei `IconProvider::ShowWindow`,
  passte dort laut Nessie bereits.
- `main.cpp`: `QApplication::setWindowIcon(IconProvider::appIcon())`
  ergänzt (statt nur auf `MainWindow`, damit auch Dialoge dasselbe Icon
  erben) — Titelleiste/Taskleiste zeigten zuvor nur das generische
  Qt-Standardsymbol, fiel im selben Zuge auf.

### Vortag-Spalte + Piktogramm-Spalte: Tooltip mit Gesamtänderung (Feature, 02.08.2026)

Beim Hovern über die "Vortag"-Spalte **und** die Entwicklungs-Pfeil-Icon-
Spalte direkt davor (`PrevDayChart`) im Portfolio-Grid (Depotwert- **und**
Marktwert-Tab) zeigt ein Tooltip die Gesamtänderung der Position statt der
reinen Pro-Aktie-Kursänderung — also `aktuell gehaltene Anteile ×
Kurswert-Entw. (prevDayDiff)`, gerundet mit `ShareCalculator::roundAway()`.
Dieselbe Formel verwendet bereits `PresenterShareDetails::buildVortagBox()`
für die "Vortag"-Box in `ShareDetailsForm` — hier direkt über
`ShareCalculator::roundAway()` aufgerufen (kein Grund für die dortige
DB-freie Kopie, da `MainWindow` `ShareCalculator.cpp` ohnehin bereits linkt).

**Finales Layout (Grid):**

```
Gesamtänderung Aktie:
40,0000 Stk. × +12,30 € = +492,00 €
```

- Zeile 1 ist reine Beschriftung ohne Wert.
- Zeile 2 ist der Rechenweg. Anteile-Anzahl mit **vier** statt zwei
  Nachkommastellen (`tooltipVolumeStr`, nur im Tooltip — die "Anteile"-Spalte
  selbst zeigt weiterhin zwei Nachkommastellen über `volumeStr`), damit die
  von Auge nachvollzogene Multiplikation exakt zum Ergebnis passt.
- Pro-Stück-Wert (`+12,30 €`) **und** Gesamtergebnis (`+492,00 €`) sind
  jeweils nach ihrem **eigenen** Vorzeichen eingefärbt (grün/rot), unabhängig
  voneinander — z. B. kann der Kurs gefallen sein (Pro-Stück-Wert rot),
  während 0 Anteile gehalten werden (Gesamtergebnis schwarz/neutral).
- Bei exakt 0 (weder Kursbewegung noch Positionswert) wird **kein**
  Vorzeichen und **keine** Farbe angezeigt — reiner schwarzer Text
  (`"0,00 €"`, nicht `"+0,00 €"`).
- Zeile 2 steckt in einem `<div style="white-space:nowrap;">…</div>`, damit
  sie bei keiner Tooltip-Breite umbricht.

**Finales Layout (Footer, einzeilig):**

```
Gesamtänderung Portfolio: +492,00 €
```

Nur Beschriftung + farbig eingefärbter Wert in einer Zeile, ohne Rechenweg —
der Footer summiert über mehrere Aktien mit unterschiedlichem
Anteile/Kurswert-Entw., ein einzelner "Anteile × Entw."-Ausdruck wäre hier
irreführend. Wert ist die Summe aller Einzel-Gesamtänderungen des Portfolios
(Summe der bereits pro Aktie gerundeten Werte, nicht: Summe der Rohwerte und
einmal am Ende gerundet, damit die Portfolio-Summe exakt der Summe der
einzelnen Grid-Tooltips entspricht). Da der Wert brokerageunabhängig ist, ist
er für Depotwert- und Marktwert-Footer identisch — nur einmal in
`updatePortfolioFooters()` berechnet, für beide wiederverwendet.

**Drei neue private `MainWindow`-Hilfsmethoden:**

- `formatSignedMoney(value)` — Vorzeichen + `QLocale`-Format; "+" nur bei
  `value > 0.0`, nie bei 0 (negative Werte tragen ihr "-" bereits über
  `QLocale::toString()`).
- `colorizeToolTip(text, color)` — kapselt ein `<span
  style="color:...">`-Tag. `QToolTip` unterstützt einen Rich-Text-Teilsatz
  (Qt erkennt das über `Qt::mightBeRichText()` automatisch an HTML-Tags).
- `formatSignedMoneyMaybeColored(value, color)` — kombiniert beide: bei
  `qFuzzyIsNull(value)` bewusst **reiner Text ohne Farb-Span**, sonst
  `colorizeToolTip(formatSignedMoney(value), color)`. Der reine Text bei 0
  ist kein kosmetisches Detail, sondern ein Bugfix: ein Farb-Span mit
  `palette().color(QPalette::Text)` (der `MainWindow`-Palette, die
  `perfColor()` für den Neutral-Fall liefert) rendert in `QToolTip`
  sichtbar gräulich statt sattem Schwarz, da `QToolTip` intern eine eigene,
  unabhängige Palette (`QPalette::ToolTipText`) verwendet — reiner Text ohne
  Span übernimmt automatisch die korrekte Tooltip-Standardfarbe, identisch
  zum ungefärbten Rest der Zeile (Label, "Stk. ×", "=").

**Farb-Eingaben:** Gesamtergebnis → `perfColor(prevDayTotal)` (nicht
`perfColor(v.prevDayDiff)`) — bei 0 gehaltenen Anteilen ODER unverändertem
Kurs ist `prevDayTotal` 0 und damit neutral, auch wenn der jeweils andere
Faktor ungleich 0 ist. Pro-Stück-Wert → `perfColor(v.prevDayDiff)`, unabhängig
vom Gesamtergebnis. Betrifft ausschließlich die Tooltip-Farben; die
Grid-Zelle "Vortag" selbst (`prevDiffStr`/`prevPctStr`, `makeTwoLine(...)`)
bleibt unverändert nach der reinen Kursbewegung eingefärbt, da sie die
Kursentwicklung an sich zeigt, nicht den Positionswert.

**Gesetzt an folgenden Stellen** (die Vortag- und PrevDayChart-Spalten werden
an mehreren Stellen befüllt):

- `populatePortfolioTables()` — beim initialen Tabellenaufbau, auf
  `prevDayItemF`/`prevChartItemF` (Depotwert) und `prevDayItemM`/
  `prevChartItemM` (Marktwert). Da `v.volume`/`v.prevDayDiff` unabhängig vom
  Brokerage und damit für beide Tabs identisch sind, wird der Tooltip-Text
  nur einmal pro Aktie berechnet und für beide Tabs wiederverwendet.
- `onMarketValuesUpdated()` — beim Einzel-Refresh einer Aktie. `setTwoLine()`
  aktualisiert nur Text-/Farb-Rollen, nicht den Tooltip; dieser wird daher
  zusätzlich per `item->setToolTip()` auf `FC::PrevDay`/`MC::PrevDay` **und**
  `FC::PrevDayChart`/`MC::PrevDayChart` gesetzt — sonst zeigt der Tooltip nach
  einem Refresh weiterhin den alten Wert.
- `updatePortfolioFooters()` — Footer hat keine eigene Vortag-Zelle (Preis +
  Chart-Icon + Vortag sind im Depotwert-Footer per `setSpan()` zu einem
  rechtsbündigen Zeilen-Label verschmolzen, im Marktwert-Footer sogar
  Icon..Vortag als ganzer Zeilen-Label-Span, siehe "Footer-Tabelle
  (Summenzeilen)" oben). Tooltip wird daher auf den jeweiligen Span-Anker
  gesetzt (`FC::Price` bei allen drei Zeilen im Depotwert-Footer, `MC::Icon`
  bei allen drei Zeilen im Marktwert-Footer) — Hovern über eine beliebige
  Stelle des gemergten Labels (inkl. der visuellen Vortag- und
  PrevDayChart-Spalte) zeigt so denselben Tooltip. `refreshPortfolioFooters()`
  (nach jedem Einzel-Refresh aufgerufen, siehe `onRefreshShareFinished()`)
  berechnet die komplette `ShareValues`-Liste neu und ruft
  `updatePortfolioFooters()` — der Footer-Tooltip bleibt dadurch ohne
  weitere Änderung auch nach einem Einzel-Refresh korrekt.

@note **Multiplikationszeichen "×" statt "*":** In einer Zwischen-Vorgabe
wurde ein ASCII-"*" genannt, die App verwendet aber durchgängig "×" (u. a. in
`ShareDetailsForm`s Vortag-Box, dieselbe Formel) — als "×" beibehalten für
Konsistenz mit dem Rest der App.

@note **Iterationshistorie:** Das Feature durchlief mehrere Rückmeldungsrunden
(Rechenzeile ergänzt, dann Layout korrigiert, dann Farblogik zweimal
angepasst — zuerst nur Gesamtergebnis farbig, dann doch wieder beide Werte
unabhängig voneinander farbig, dazu der Grau-statt-Schwarz-Bugfix bei 0). Der
obige Abschnitt beschreibt ausschließlich den finalen, bestätigten Stand
("So lassen wir es", 02.08.2026) — Zwischenstände sind nicht mehr separat
dokumentiert.

### test_onPortfolioRowRightClicked_validGuid_popupCenteredAndNarrowerThanMainWindow — CI-only-Fehlschlag behoben (Bugfix, 02.08.2026)

Reproduzierbar und deterministisch nur im CI-Lauf fehlgeschlagen
(`popupCenterX=425` vs. `mainWindowCenterX=462`, identisch über mehrere
Läufe), in QtCreator lokal immer grün. Ursachenklärung per temporärem
Diagnose-Commit (Geometrie-/Screen-Ausgabe im Test): Der CI-Runner nutzt die
`offscreen`-QPA-Plattform mit einer virtuellen Bildschirmgröße von nur
800×800px. Der Test passt die Fenstergröße zwar bewusst an die verfügbare
Bildschirmgeometrie an (`screenGeom.width() − 100`, hier also 700px), kann
`MainWindow` damit aber nicht unter dessen harte
`setMinimumSize(900, 600)` (siehe `initialize()`) schrumpfen — das Fenster
bleibt bei 900px Breite, obwohl der Bildschirm nur 800px breit ist. Die
Popup-Breite (`window.width() − 50` = 850px, siehe
`onPortfolioRowRightClicked()`) übersteigt dadurch die gesamte verfügbare
Bildschirmbreite — eine exakte Zentrierung ist in diesem Fall mathematisch
unmöglich. `ChartPopup::showAt()`s Bildschirmrand-Klemmung (siehe dort)
verhält sich dabei korrekt und deterministisch: sie resolved auf den linken
Bildschirmrand (`x = avail.left()`) — genau das beobachtete, reproduzierbare
Verhalten. Kein Bug in `ChartPopup::showAt()` oder
`onPortfolioRowRightClicked()`, sondern eine Testannahme
("Fenster/Bildschirm sind immer breit genug für eine unklemmbare
Zentrierung"), die auf jedem realen Desktop (> 950px Breite) zutrifft, aber
nicht auf dem 800px schmalen CI-Runner.

Der Test berechnet jetzt dieselbe `avail`-Geometrie wie
`ChartPopup::showAt()` und unterscheidet explizit zwei Bildschirmgrößen-
Regime: Passt das Popup auf den verfügbaren Bildschirm, bleibt die
ursprüngliche, exakte Zentrierungs-Prüfung unverändert aktiv (deckt jeden
realen Desktop ab). Passt es nicht (Popup breiter als `avail`), wird
stattdessen die dafür einzig korrekte, deterministische Konsequenz geprüft:
Linksklemmung an `avail.left()`. Bewusst **nicht** die komplette
Klemm-Formel aus `showAt()` im Test dupliziert (das würde nur gegen sich
selbst prüfen und Klemm-Logik-Bugs nicht mehr erkennen) — nur der
Fallunterscheidung (`popup->width() <= avail.width()`) bedient sich der
Test, die eigentliche erwartete Position bleibt in beiden Fällen unabhängig
hergeleitet.

### Fenstertitel: Versionsnummer statt redundantem Dateinamen (Bugfix + Feature, 01.08.2026)

Auf Nessies Vorgabe: Der Fenstertitel zeigte bei geöffnetem Portfolio
zusätzlich dessen Dateinamen an — redundant zur bereits vorhandenen
Statusleisten-Anzeige unten rechts (Bugfix, `updateWindowTitle()` entfernt).
Stattdessen zeigt der Titel jetzt die App-Version über
`MainWindow::baseWindowTitle()` / `QCoreApplication::applicationVersion()`
(Feature). Volle Details, betroffene Methoden und Testabdeckung siehe
"MainWindow-Details", Abschnitt "Fenstertitel — Version statt Dateiname",
sowie "Versionierung" oben.

### Grid-Selektionsfarbe (Blau/Gelb) in allen Grids (erledigt, 30.07.2026)

Auf Nessies Vorgabe: In der C#-Referenzanwendung wird die selektierte Zeile
in allen Grids mit blauem Hintergrund und gelber Schrift dargestellt (siehe
Screenshot der C#-Anwendung) — bislang nutzten alle Qt-Tabellen stattdessen
die Standard-Highlight-Farbe der Palette/des Systemthemes, und zwar
uneinheitlich je nach Theme/Betriebssystem.

Umsetzung als zentraler, wiederverwendbarer Helper statt Duplikation an
jeder Tabellen-Erzeugungsstelle: neuer Header-only-Helper `GridStyle`
(`widgets/GridStyle.h`, siehe "GridStyle" unter "Darstellung (Farben, Icons,
Zeilen)" oben) mit den theme-neutralen Konstanten
`kSelectionBackground = #1c3f8f` / `kSelectionForeground = #ffd400`, analog
zu `CenterIconDelegate`/`TwoLineDelegate` (kein `Q_OBJECT`, keine eigene
`.cpp`). Angewandt auf:

- Beide MainWindow-Haupttabellen (Depotwert `m_finalValueTable`, Marktwert
  `m_marketValueTable`) in `setupCentralWidget()`.
- `OverviewTabWidget::buildFrozenTable()` — deckt dadurch automatisch alle
  fünf Edit-Dialoge (`ViewBuyEdit`, `ViewSaleEdit`, `ViewDividendEdit`,
  `ViewBrokerageEdit`, `ViewShareAdd`) sowie die drei Tabs in
  `ViewShareDetails` ab.

Bewusst NICHT angewandt auf Footer-Tabellen (Gesamt-Zeile in
`OverviewTabWidget` sowie `m_finalValueFooter`/`m_marketValueFooter`) — diese
haben durchgängig `QAbstractItemView::NoSelection`.

**Nachgezogener Bugfix (siehe "TwoLineDelegate" oben für die volle
Analyse):** Die sieben von `TwoLineDelegate` gerenderten Spalten (Kosten /
Dividenden, Preis, Vortag, Aktuelle Entwicklung, Einzahlung / Marktwert,
Komplette Entwicklung, Kpl. Einzahlung / Kpl. Marktwert) blieben zunächst bei
Selektion uneingefärbt, da Qt eine per Stylesheet gesetzte
`item:selected`-Regel nicht in eine über `QPalette` abfragbare Farbe
zurückspiegelt. Behoben, indem `TwoLineDelegate::paint()` bei Selektion
direkt dieselben `GridStyle`-Konstanten referenziert statt sie indirekt über
Style/Palette zu ermitteln. Von Nessie visuell bestätigt (30.07.2026): alle
Grids inkl. der zuvor betroffenen Spalten färben sich jetzt korrekt ein.

Testabdeckung: `tst_overviewtabwidget.cpp` prüft für Übersicht- und
Jahres-Tabs, dass `dataTable->styleSheet()` die `GridStyle`-Konstanten
enthält und `footerTable` sie bewusst nicht enthält (siehe TESTING.md).
`tst_mainwindow.cpp` prüft analog beide Haupttabellen. `TwoLineDelegate`
selbst bleibt wie zuvor ohne dedizierten Test (reine `QPainter`-Zeichenlogik
ohne Zustands-API) — Verifikation visuell durch Nessie.

### settings.ini nicht persistent im AppImage — Bugfix (29.07.2026)

Gemeldet von Nessie: Im Linux-AppImage muss nach jedem Neustart erneut das
Dokumente-Root-Verzeichnis ausgewählt werden — `settings.ini` scheint nie
gespeichert zu werden, obwohl das Log jedes Mal
"No settings.ini found — created one with defaults" meldet.

Root Cause: `AppStartup::settingsPath()` lieferte bislang
`QCoreApplication::applicationDirPath() + "/settings.ini"`. Unter einem
AppImage ist `applicationDirPath()` aber kein stabiler Ort, sondern der
FUSE-Mountpunkt, unter dem das AppImage für die Dauer des Prozesses
bereitgestellt wird (z. B. `/tmp/.mount_Share_jAlPnD/usr/bin`) — bei jedem
Start ein neues, zufälliges Verzeichnis, das beim Beenden wieder
verschwindet. `settings.ini` wurde also bei jedem Start korrekt geschrieben,
aber unter einem Pfad, den der *nächste* Start gar nicht mehr kennt — daher
der Eindruck, es würde nie gespeichert.

Das war bereits im Bugfix vom 24.07.2026 ("Erstlauf ohne settings.ini", siehe
oben) als offenes Risiko benannt, dort aber bewusst zurückgestellt, weil der
damalige akute Fall (App durch `FatalError` komplett blockiert) auch ohne
diese Änderung gelöst war und sich das AppImage-Mount-Szenario noch nicht
bestätigt hatte. Mit Nessies Meldung ist es jetzt reproduziert bestätigt.

Fix: `AppStartup::settingsPath()` verwendet jetzt
`QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)`
(inkl. `QDir().mkpath(...)`, falls das Verzeichnis noch nicht existiert)
statt `applicationDirPath()`. Das liefert unter Linux z. B.
`~/.config/<OrganizationName>/SharePortfolioManager/settings.ini`,
unabhängig vom AppImage-Mount, und bleibt über alle Paketierungsformen
(AppImage, Windows-Installer, portabler Build) hinweg stabil.
`AppSettings::load()`s eigener Default-Pfad (nur relevant, wenn `load()`
direkt ohne Pfad aufgerufen wird — der Produktivpfad über `main()` →
`AppStartup::loadSettings()` übergibt immer schon einen konkreten Pfad)
wurde aus Konsistenzgründen auf denselben Mechanismus umgestellt.

Bewusst NICHT umgesetzt: eine Migration bereits existierender
`settings.ini`-Dateien vom alten Pfad (neben der Executable) zum neuen
Config-Verzeichnis. Auf Nessies Wunsch entfällt das, da die Anwendung noch
nicht produktiv im Einsatz ist — es gibt keine schützenswerten Bestandsdaten.

`WebSites.xml`/`Documents.xml` bleiben bewusst neben der Executable/im
AppImage, da sie reine, vom User nicht editierbare Konfigurationsvorlagen
sind und von diesem Problem nicht betroffen sind (siehe Bugfix vom
24.07.2026 weiter unten, Abschnitt "Bewusst NICHT als Lösung gewählt").

@note `AppStartup::settingsPath()` erzeugt das Zielverzeichnis jetzt aktiv
per `QDir().mkpath(...)`, bevor der Pfad zurückgegeben wird — bei einem
brandneuen Config-Verzeichnis (erster Start auf einem System überhaupt)
existiert es sonst noch nicht, und `QSettings` legt zwar die Datei an, aber
nur, wenn das übergeordnete Verzeichnis bereits vorhanden ist.

@note **OrganizationName "BT" statt "nessie1980" (29.07.2026):** Der
Config-Pfad unter Linux setzt sich über `QStandardPaths::AppConfigLocation`
aus `~/.config/<OrganizationName>/<ApplicationName>/` zusammen —
`OrganizationName` wird in `main()` per `app.setOrganizationName(...)`
gesetzt und stand zuvor auf Nessies GitHub-Handle `"nessie1980"`. Auf
Nessies Wunsch durch den neutralen, nicht-personenbezogenen Bezeichner
`"BT"` ersetzt, da der Pfad-Bestandteil keinen Mehrwert aus einem
persönlichen Handle zieht. Rein kosmetisch, betrifft nur den Dateisystempfad
der `settings.ini`/zukünftiger Konfigurationsdaten — keine Auswirkung auf
Verhalten, Datenbankschema oder gespeicherte Werte selbst. Konsequenz: eine
bereits unter `~/.config/nessie1980/SharePortfolioManager/settings.ini`
existierende Datei (z. B. aus Nessies lokalem Test des vorherigen Fixes)
wird beim nächsten Start nicht mehr gefunden — es wird, wie bei jeder
fehlenden `settings.ini`, einfach eine neue mit Defaults unter dem neuen
Pfad angelegt (siehe Bugfix "Erstlauf ohne settings.ini", 24.07.2026). Keine
Migration, aus denselben Gründen wie beim AppImage-Pfadwechsel oben.

### Direkte Dokumentenerfassung per Drag+Drop (erledigt, 27.07.2026)

Neues Feature, analog zur bereits in der C#-Referenzanwendung vorhandenen
Logik: PDF-Dokumente werden per Drag+Drop auf die (bislang leere) Box
"Direkte Dokumentenerfassung" im `MainWindow` abgelegt, automatisch als
Kauf-/Verkaufs-/Dividenden-Dokument erkannt, und das passende
Editier-Formular (`ViewShareAdd`/`ViewBuyEdit`/`ViewSaleEdit`/
`ViewDividendEdit`) öffnet sich direkt mit vorbelegtem Dokumentpfad.

Mit Nessie abgestimmtes Verhalten (27.07.2026):
- **Einzeldatei-Drop only.** Bei Mehrfachauswahl im Drop-Event: Ablehnung
  mit Statusmeldung, kein Verarbeitungsschritt.
- **Kauf-Dokument, WKN/ISIN unbekannt** (keine passende Aktie im Portfolio)
  → `ViewShareAdd` öffnen (legt Aktie + Kauf gemeinsam an), Dokumentpfad
  vorbelegt.
- **Kauf-Dokument, WKN/ISIN bekannt** → `ViewBuyEdit(shareGuid, ...)`
  öffnen, Dokumentpfad vorbelegt.
- **Verkauf/Dividende, WKN/ISIN unbekannt** → Fehlermeldung im
  Status-Log, kein Dialog (ein Verkauf/eine Dividende ohne zugehörige,
  bereits vorhandene Aktie ist fachlich nicht möglich).
- **Kein Bank-/Typ-Match** → Statusmeldung "nicht zugeordnet", kein Dialog.
- **Brokerage vorerst außen vor** — auch bei erkanntem
  `DocumentType::Brokerage` kein Auto-Öffnen von `ViewBrokerageEdit` (auf
  Nessies Vorgabe bewusst abgegrenzt, s.u. "Erledigt/Archiv" sobald
  umgesetzt).

**Schritt 1 (umgesetzt, 27.07.2026): Grundlage — DocumentClassifier /
PdfTextExtractor**

Die eigentliche Bank-/Dokumenttyp-Erkennung existierte bereits — nur
vierfach dupliziert in `PresenterBuyEdit::startParserForText()`,
`PresenterSaleEdit::startParserForText()`,
`PresenterDividendEdit::startParserForText()` und
`PresenterShareAdd::startParserForText()` (BankIdentifier-Regex →
Buy-/Sale-/Dividend-/BrokerageIdentifier-Regex → passender `DocumentEntry`).
Für die neue Direkterfassung wird genau diese Erkennung gebraucht, aber
*bevor* überhaupt feststeht, welches Formular geöffnet werden soll — die
Logik musste also so oder so aus den Presentern herausgelöst werden.

Neue Klasse `app/utils/DocumentClassifier.h/.cpp` (statisch, kein
QObject, keine GUI-Abhängigkeit):
- `DocumentClassifier::classify(pdfText, config)` → `Result { matched,
  bank, docEntry, type }`. Anders als die vier Presenter (die beim
  Ausbleiben eines Identifier-Treffers auf ihren eigenen Dialogtyp
  zurückfallen, weil der Benutzer den Dialog ja bereits bewusst gewählt
  hat) liefert `classify()` bei fehlendem Bank- **oder** Typ-Treffer
  `matched = false` zurück — ohne Vorwissen darf hier nicht geraten
  werden.
- `Result` speichert `BankEntry`/`DocumentEntry` **als Kopie**, nicht als
  Zeiger: `DocumentsConfig::entries()` liefert `QList<BankEntry>` *by
  value* zurück; ein `const BankEntry*` in den (bisherigen) Presentern
  wird nur innerhalb derselben Funktion verwendet und übersteht das nie.
  Für `DocumentClassifier` muss das Ergebnis aber auch nach Rückkehr aus
  `classify()` noch gültig sein (der Aufrufer in `MainWindow` prüft erst
  Schritt für Schritt WKN/ISIN, Aktien-Lookup, dann erst welches Formular
  zu öffnen ist) — Kopie statt Zeiger vermeidet ein Dangling-Pointer-Bug,
  der beim naiven Kopieren der Presenter-Logik sonst direkt entstanden
  wäre.
- `DocumentClassifier::extractFieldValue(pdfText, regexList, fieldName)`
  + Convenience-Wrapper `extractWkn()`/`extractIsin()`: wendet eine
  einzelne benannte Regel aus einer `RegExList` an (erste Capture-Gruppe,
  sonst gesamter Treffer), unabhängig vom vollen `ParserLib::Parser`-Ablauf
  — wird für den WKN-/ISIN-Abgleich mit `ShareRepository::findByWkn()`/
  `findByIsin()` in Schritt 3 benötigt.
- Die vier bestehenden Presenter wurden **noch nicht** umgestellt (folgt
  in Schritt 2) — sie laufen unverändert weiter, ihre Bestandstests bleiben
  unberührt.

Neue Klasse `app/utils/PdfTextExtractor.h/.cpp` (QObject, asynchron):
kapselt den `pdftotext -enc UTF-8 -layout <pfad> -`-Aufruf byte-für-byte
identisch zum bisherigen, vierfach duplizierten `QProcess`-Code in den
vier `onDocumentSelected()`-Implementierungen. Emittiert
`finished(bool success, QString text)`. Bewusst **kein** automatisierter
Test dafür (siehe TESTING.md) — konsistent mit der bestehenden Konvention
im Projekt, `QProcess`-getriebene `pdftotext`-Codepfade nicht direkt zu
testen (s. `onBrowseDocument()`-Methoden).

**Offene Frage aus Schritt 1 — geklärt in Schritt 3 (siehe dort):** Ob die
`Sale`- und `Dividend`-`Document`-Einträge in `Documents.xml` `Wkn`/`Isin`-
Regeln enthalten, war zunächst unklar, da bislang niemand sie gebraucht
hatte (`ViewSaleEdit`/`ViewDividendEdit` werden immer für eine bereits
ausgewählte Aktie geöffnet, der Aktienbezug kommt über `shareGuid`, nicht
aus dem Dokument). Nessies Praxistest bestätigte: die Regeln sind
vorhanden, die Direkterfassung findet die passende Aktie auch für diese
beiden Typen zuverlässig.

**Schritt 2 (umgesetzt, 27.07.2026): Refactoring der vier Presenter**

`PresenterBuyEdit`, `PresenterSaleEdit`, `PresenterDividendEdit` und
`PresenterShareAdd` nutzen jetzt `PdfTextExtractor` + `DocumentClassifier`
statt der eigenen, vierfach duplizierten `QProcess`-/Regex-Erkennungslogik.
Reines Refactoring, bewusst ohne Verhaltensänderung:

- `onDocumentSelected()` ruft jetzt `m_pdfExtractor.extract(path)` statt
  selbst einen `QProcess` aufzusetzen.
- Der bisherige private Slot `onPdfConversionFinished(int exitCode, int
  exitStatus)` (an `QProcess::finished` gebunden) entfällt; neuer privater
  Slot `onPdfTextExtracted(bool success, const QString& text)`, verbunden
  mit `PdfTextExtractor::finished()`. Gleiche Fehlermeldung bei
  `!success` ("PDF-Konvertierung fehlgeschlagen oder kein Text
  extrahierbar."), gleiches Verhalten bei Erfolg
  (`m_pdfText = text; startParserForText(m_pdfText);`).
- `startParserForText()` bleibt als private Methode je Presenter bestehen
  (unterschiedliche Required-Felder-Listen bei Bank-Fehltreffer,
  unterschiedlicher `ParsingValues`-Aufbau), aber die Bank-/Typ-Erkennung
  selbst ist durch zwei Aufrufe ersetzt:
  `DocumentClassifier::matchBankIndex(pdfText, *m_config, bankIndex)` und
  `DocumentClassifier::detectDocumentType(pdfText, matchedBank,
  fallbackType)`. Der jeweilige `fallbackType` entspricht exakt der
  bisherigen lokalen Default-Initialisierung (`PresenterBuyEdit`/
  `PresenterShareAdd` → `DocumentType::Buy`, `PresenterSaleEdit` →
  `DocumentType::Sale`, `PresenterDividendEdit` → `DocumentType::Dividend`)
  — Verhalten bei "Bank erkannt, aber kein Dokumenttyp-Identifier trifft"
  bleibt damit identisch.
- `DocumentClassifier` wurde dafür um zwei neue Bausteine ergänzt:
  `matchBankIndex()` (liefert nur den Index statt eines potenziell
  wackligen Zeigers) und `detectDocumentType()` (mit Fallback-Parameter,
  im Unterschied zu `classify()`, das bewusst nie rät). `classify()` selbst
  wurde intern auf dieselben beiden Bausteine umgestellt, um Code-Doppelung
  innerhalb von `DocumentClassifier` zu vermeiden.
- `#include <QProcess>`/`#include <QRegularExpression>` sind aus allen vier
  `.cpp`-Dateien entfallen — beides steckt jetzt ausschließlich in
  `PdfTextExtractor`/`DocumentClassifier`.
- Alles andere (WKN/ISIN-Gegenprüfung in `populateFromResult()`,
  `xmlNameToViewField()`, Validierung, Speichern) ist unverändert
  übernommen.

@note **Noch nicht automatisiert nachgetestet:** Die bestehenden
Presenter-Tests (`tst_buysform`, `tst_mainwindow` u. a.) wurden nicht
angefasst, da `startParserForText()` weiterhin `private` ist und die
Tests wie bisher nur über die öffentlichen Slots (`onDocumentSelected()`
etc.) erreichbar sind — die Umbenennung von `onPdfConversionFinished`
zu `onPdfTextExtracted` betrifft nur privates Slot-Wiring, keine
öffentliche Schnittstelle. Nessie sollte nach dem Einspielen dennoch die
volle Testsuite lokal laufen lassen, bevor Schritt 3 aufsetzt — falls
irgendwo (z. B. in `tst_mainwindow.cpp`) doch direkt auf den alten
Slotnamen oder die alte `QProcess`-Signatur Bezug genommen wird, muss das
dort nachgezogen werden.

**Schritt 3 (umgesetzt, 27.07.2026): MainWindow-Integration**

`MainWindow` reagiert jetzt auf einen einzelnen per Drag+Drop abgelegten
PDF-Anhang auf `m_documentCaptureGroup`:

- **Event-Scoping statt globalem Drag-Handling:** Statt
  `dragEnterEvent()`/`dropEvent()` auf dem gesamten `MainWindow` zu
  überschreiben (was bei jedem PDF-Drop irgendwo im Fenster ausgelöst
  hätte, z. B. versehentlich über der Portfolio-Tabelle), akzeptiert nur
  `m_documentCaptureGroup` selbst Drops (`setAcceptDrops(true)`) und
  `MainWindow` installiert sich per `installEventFilter(this)` als Filter
  dafür — reagiert also ausschließlich auf `QEvent::DragEnter`/
  `QEvent::Drop`, deren `watched`-Objekt `m_documentCaptureGroup` ist.
  `m_documentCaptureEdit` (das `QLineEdit` in der Box) bekommt explizit
  `setAcceptDrops(false)`, damit ein Drop nicht dort hängen bleibt,
  sondern per Qt-Standardverhalten zum nächsten Vorfahren mit
  `acceptDrops == true` durchgereicht wird.
- **Einzeldatei-Drop only:** Mehr als eine Datei im Drop → Statusmeldung
  "Bitte nur ein Dokument gleichzeitig ablegen." (Warning), keine
  Verarbeitung. Nicht-PDF oder Nicht-lokale-Datei → stiller `ignore()`.
- **`handleDroppedDocument(path)`:** neue private Methode (kein Slot —
  nimmt nur einen `QString`, daher ohne `QMetaObject::invokeMethod`/
  `Q_ARG` direkt aus Tests aufrufbar, analog zur Begründung bei
  `selectShareRow()`). Setzt Statustext, startet
  `m_documentCaptureExtractor.extract(path)`.
- **`onDocumentCaptureTextExtracted(bool, QString)`:** neuer privater
  Slot, verbunden mit `PdfTextExtractor::finished()`. Bei Fehlschlag:
  Statusmeldung (Error), Abbruch. Bei Erfolg: `DocumentClassifier::
  classify()` — bei `!matched`: Statusmeldung (Error, auf Nessies Vorgabe
  vom 27.07.2026 — ursprünglich fälschlich Warning), Abbruch. Bei
  Erfolg: `openCaptureDialog()`.
- **`resolveShareGuidForDocument()`:** WKN zuerst, dann ISIN, via
  `DocumentClassifier::extractWkn()`/`extractIsin()` +
  `ShareRepository::findByWkn()`/`findByIsin()`. Leerer String (kein
  Fehler), wenn der `DocumentEntry` gar keine Wkn-/Isin-Regel enthält.
- **`openCaptureDialog()`:** öffnet je nach `DocumentType` und WKN/ISIN-
  Treffer `ViewShareAdd` (Buy, unbekannte Aktie), `ViewBuyEdit` (Buy,
  bekannte Aktie), `ViewSaleEdit`/`ViewDividendEdit` (Sale/Dividend,
  bekannte Aktie — bei unbekannter Aktie nur Fehlermeldung, kein Dialog,
  da fachlich nicht möglich) oder zeigt bei `Brokerage` nur eine
  Info-Meldung (bewusst nicht unterstützt, siehe oben). Jeder geöffnete
  Dialog bekommt das Dokument über `dlg.presenter()->onDocumentSelected
  (pdfPath)` vorbelegt — exakt derselbe Aufruf, den auch der manuelle
  "…"-Browse-Klick auslöst. Der Dialog parst das Dokument dadurch ein
  zweites Mal über seine eigene, unveränderte Pipeline (zweiter
  `pdftotext`-Aufruf) — eine bewusst in Kauf genommene kleine Redundanz,
  um die vier Presenter nicht zusätzlich um eine "Text bereits vorhanden"-
  Variante erweitern zu müssen.
- **`ViewShareAdd::presenter()` ergänzt:** war der einzige der vier
  Editier-Dialoge ohne den sonst überall vorhandenen `presenter()`-
  Accessor (`ViewBuyEdit`/`ViewSaleEdit`/`ViewDividendEdit` hatten ihn
  bereits) — rein additiv in `ViewShareAdd.h`, keine `.cpp`-Änderung
  nötig (inline definiert, wie bei den drei Geschwistern).

@note **Offene Frage aus der Entwicklungsphase — geklärt (27.07.2026):**
Ob die `Sale`-/`Dividend`-`Document`-Einträge in der echten `Documents.xml`
`Wkn`/`Isin`-Regeln enthalten, war zunächst unklar. Nessies Praxistest
(alle vier Dokumenttypen per Drag+Drop, jeweils korrekt erkannt und der
richtige Dialog geöffnet und befüllt) bestätigt: die Regeln sind vorhanden,
`resolveShareGuidForDocument()` findet die passende Aktie auch für
Verkaufs- und Dividenden-Dokumente zuverlässig.

@note **Testbarkeit korrigiert (27.07.2026):** `resolveShareGuidForDocument()`
war ursprünglich eine private, nicht-statische Methode mit dem (falschen)
Kommentar, sie sei "direkt aus Tests aufrufbar" — eine private Methode ist
von außen aber schlicht nicht erreichbar, weder direkt noch per
`QMetaObject::invokeMethod` (das funktioniert nur bei Slots/
`Q_INVOKABLE`). Behoben:
- `resolveShareGuidForDocument()` ist jetzt `public static` (touched keine
  Instanzdaten, nur `DocumentClassifier` + eine lokal konstruierte
  `ShareRepository`) — analog zu `buildDailyValuesUrl()`. Direkt testbar
  als `MainWindow::resolveShareGuidForDocument(...)`, siehe TESTING.md.
- `handleDroppedDocument()` ist jetzt `private slot` (statt einer einfachen
  privaten Methode) — analog `selectShareRow()`/`selectFirstShareRow()`,
  testbar per `QMetaObject::invokeMethod`, auch wenn ein tatsächlicher
  Test weiterhin einen echten `pdftotext`-Aufruf bräuchte (siehe unten).
- `openCaptureDialog()` bleibt eine einfache private Methode — sie öffnet
  echte, modale Dialoge und wird (wie der Rest der Codebase) nicht direkt
  unit-getestet.

@note **Testabdeckung:** `DocumentClassifier` vollständig in
`tst_documentclassifier` abgedeckt. `resolveShareGuidForDocument()` neu in
`tst_mainwindow` abgedeckt (5 Testfälle, siehe TESTING.md) — braucht nur
eine echte Test-DB, kein `pdftotext`. Bewusst weiterhin ungetestet:
`handleDroppedDocument()` (echter `pdftotext`-Aufruf nötig, konsistent mit
der bestehenden Projektkonvention für `pdftotext`-`QProcess`-Codepfade),
`eventFilter()` (echter Qt-Drag&Drop-Vorgang, kein einfaches `QTest`-Mittel
dafür) und `openCaptureDialog()` (echte modale `QDialog::exec()`-Abläufe).

**Feature-Status: Fertig.** Alle drei Umsetzungsschritte sind implementiert,
bauen und linken sauber (inkl. der drei betroffenen CMake-Targets
`SharePortfolioManager`, `tst_mainwindow`, `tst_buysform`,
`tst_shareeditform` und des neuen `tst_documentclassifier`), die volle
Testsuite läuft grün, und Nessie hat das Feature manuell mit echten
Kauf-/Verkaufs-/Dividenden-PDFs verifiziert (27.07.2026): alle Dokumenttypen
werden korrekt erkannt, der passende Dialog öffnet sich und wird befüllt.

### Log-Meldungsfarben theme-neutral — Bugfix (24.07.2026)

Gemeldet von Nessie (Screenshot): nach dem vorherigen Fix ("Erstlauf ohne
settings.ini") ließ sich auf Linux zwar ein Portfolio anlegen, aber die
Status-Meldungen im Hauptfenster waren auf hellem Theme kaum lesbar — vor
allem "Start"/"Info" (fast unsichtbar, blasses Grau auf weißem Hintergrund).

Root Cause: `AppSettings::m_logColors` (Default-Werte in `AppSettings.h`)
waren laut Code-Kommentar explizit "optimiert für Dark Theme"
(`#e0e0e0` — praktisch weiß — für Start/Info, dazu helle Varianten für
Warning/Error/Success). Die App selbst setzt an keiner Stelle aktiv ein
Theme oder eine Palette (geprüft — kein `QApplication::setStyle()`, keine
`QPalette`-Umstellung); sie übernimmt komplett, was die jeweilige
Laufzeitumgebung an Qt-Theme liefert.

Auf Nessies Windows-Entwicklungsrechner (dunkles System-/Qt-Theme) fiel das
nie auf. Auf Linux ist das Verhalten aber nicht mal vom tatsächlichen
Desktop-Theme abhängig, sondern vom **Packaging**: Die Linux-AppImage-
Pipeline (`.github/workflows/package.yml`) verwendet die offizielle,
vorkompilierte Qt-Distribution (`jurplel/install-qt-action`,
`arch: linux_gcc_64`, via aqtinstall) — diese ist nicht gegen GTK3 gelinkt
und bringt daher kein funktionierendes Platform-Theme-Plugin
(`platformthemes/libqgtk3.so` o. ä.) mit. `linuxdeploy-plugin-qt` bündelt
zwar korrekt alles, was in der Qt-Installation *vorhanden* ist — ist aber
selbst nichts vorhanden, gibt es nichts zu bündeln. Die im AppImage
laufende Qt-Instanz hat dadurch grundsätzlich keine Möglichkeit, das
System-Theme abzufragen, und fällt auf Qts helle Standardpalette zurück —
unabhängig vom tatsächlichen Desktop-Theme des Nutzers. Aus Qt Creator
gestartet läuft die App dagegen mit der (distro-eigenen, meist
theme-fähigen) System-Qt-Installation, daher dort korrekt dunkel.

Bewusst NICHT als Lösung gewählt: die App selbst auf ein festes Dark Theme
zwingen (z. B. `QApplication::setStyle("Fusion")` + dunkle `QPalette` in
`main()`). Das hätte zwar zu den bisherigen Farben gepasst, wäre aber ein
großer, ungewollter optischer Eingriff (komplette App inkl. aller Dialoge
wird immer dunkel dargestellt, unabhängig vom System) und hätte das
eigentliche Problem nur kaschiert — ein selbst gegen GTK3 gebautes Qt im CI
wäre der einzige Weg, das AppImage zuverlässig ans System-Theme
heranzuführen, und selbst dann bliebe es abhängig vom jeweiligen
Ziel-Desktop des Nutzers (GNOME/GTK vs. KDE/Plasma vs. andere).

Fix: `AppSettings::m_logColors`-Defaults durch theme-neutrale Farben
ersetzt, die auf hellem UND dunklem Hintergrund mit ausreichendem Kontrast
lesbar sind (grobe Prüfung nach WCAG-Relativluminanz-Formel, jeweils
Kontrastverhältnis ca. 4:1 sowohl zu Schwarz als auch zu Weiß, statt nahe
1:1 auf einer Seite wie zuvor bei `#e0e0e0` auf weißem Hintergrund):

| Index | Zustand    | Alt (Dark-Theme-optimiert) | Neu (theme-neutral) |
|-------|------------|----------------------------|----------------------|
| 0     | Start      | `#e0e0e0`                  | `#808080`            |
| 1     | Info       | `#e0e0e0`                  | `#808080`            |
| 2     | Warning    | `#ffa500`                  | `#b36b00`             |
| 3     | Error      | `#ff4444`                  | `#d32f2f`             |
| 4     | FatalError | `#ff0000`                  | `#b71c1c`             |
| 5     | Success    | `#44ff44`                  | `#388e3c`             |

`LoggerSettingsForm::k_colorNames` (feste Dropdown-Liste im
Logger-Einstellungsdialog) wurde um die fünf neuen Hex-Werte ergänzt. Die
alten fünf Hex-Werte bleiben bewusst in der Liste erhalten — sonst würde
der Dialog bei bereits vorhandenen `settings.ini`-Dateien mit den alten
Farben fälschlich "Black" als Auswahl anzeigen (die tatsächlich aktive
Farbe wäre davon unabhängig weiterhin korrekt, nur die Dropdown-Anzeige
wäre irreführend).

@note Farben bleiben über `Einstellungen → Logger...` weiterhin frei
änderbar — dieser Fix ändert nur die *Defaults* für neue bzw. noch nicht
konfigurierte Installationen. Bereits vorhandene `settings.ini`-Dateien
(inkl. Nessies eigener) behalten ihre bisher gespeicherten Farbwerte; ein
Umstieg auf die neuen Defaults erfordert dort ein manuelles Zurücksetzen
über den Dialog oder das Löschen der `settings.ini`.

@note Kein automatisierter Test für die konkreten Default-Hex-Werte ergänzt
— Farben sind bewusst frei konfigurierbar, ein hartes `QCOMPARE` auf exakte
Hex-Strings wäre bei der nächsten Farbanpassung nur Reibung ohne echten
Regressionsschutz. Verifikation erfolgte visuell durch Nessie (Screenshot
vor/nach dem Fix).

### Erstlauf ohne settings.ini — Bugfix (24.07.2026)

Gemeldet von Nessie: Nach Installation über den Linux-AppImage-Build aus
`.github/workflows/package.yml` war die App komplett unbedienbar — nur
"Beenden" verfügbar.

Root Cause: `MainWindow::checkAndLoadConfigurations()` wertete eine fehlende
`settings.ini` als `FatalError`, was über `allOk = false` `disableAllControls()`
auslöste. `settings.ini` entsteht aber ausschließlich durch einen tatsächlichen
`AppSettings::save()`-Aufruf — bei einer frischen Installation (egal ob
Windows-Installer oder AppImage) existiert sie naturgemäß noch nicht, und im
bisherigen Ablauf lief `MainWindow::checkAndLoadConfigurations()` beim Start
schon vor jedem ersten Setter-Aufruf. Jede Neuinstallation traf also
zwangsläufig diesen `FatalError` — unabhängig vom gewählten Installer/Paket
und unabhängig von Dateisystem-Rechten. `AppSettings::load()` selbst kommt mit
einer fehlenden Datei dagegen längst klar (verwendet dann einfach die in
`AppSettings.h` einprogrammierten Member-Defaults) — die App wäre mit diesen
Defaults voll funktionsfähig gewesen, nur eben nie bis dahin gekommen.

Bewusst NICHT als Lösung gewählt: `settings.ini` analog zu `WebSites.xml`/
`Documents.xml` einfach als Vorlage mitliefern. Anders als diese beiden reinen
Lese-Ressourcen ist `settings.ini` Laufzeit-Nutzerdaten — jeder
`AppSettings`-Setter persistiert sofort per `save()`. Eine mitgelieferte
Vorlage hätte das eigentliche Problem (Schreibfehlschlag bei nicht
beschreibbarem Installationsverzeichnis, z. B. ein später mal read-only
gemountetes AppImage) nicht gelöst, sondern nur verschleiert: die Datei wäre
zwar initial vorhanden, aber jeder weitere `save()`-Aufruf (Fenstergröße,
Portfolio-Pfad, Dokumente-Root, ...) wäre bei fehlender Schreibberechtigung
weiterhin still fehlgeschlagen, ohne dass der Benutzer das noch bemerkt hätte.

Fix, zweiteilig:

1. **`AppStartup::loadSettings(path)`** (neu, ersetzt den direkten
   `AppSettings::instance().load(...)`-Aufruf in `main()`): lädt die
   Einstellungen wie bisher, persistiert aber sofort die In-Memory-Defaults
   per `AppSettings::save()`, falls die Datei vor dem Laden noch nicht
   existierte. Damit liegt nach dem allerersten Start eine echte
   `settings.ini` vor — vorausgesetzt, das Zielverzeichnis ist beschreibbar.
   Ist es das nicht, schlägt `save()` weiterhin still fehl; das ist kein
   Rückschritt gegenüber vorher, siehe Punkt 2.
2. **`MainWindow::checkAndLoadConfigurations()`**: die Existenzprüfung der
   `settings.ini` löst nur noch eine `Warning`-Statusmeldung aus
   ("Einstellungsdatei nicht gefunden — Standardwerte werden verwendet."),
   setzt `allOk` nicht mehr auf `false` und blockiert die UI damit nicht
   mehr. `WebSites.xml`/`Documents.xml` bleiben unverändert `FatalError` —
   ohne sie ist die App tatsächlich nicht sinnvoll nutzbar (keine
   Kursquellen-/Dokumenttyp-Konfiguration), das ist kein vergleichbarer Fall.

Damit ist eine fehlende `settings.ini` in keinem Fall mehr blockierend — weder
wenn sie erst gar nicht existiert (Erstlauf, jetzt durch Punkt 1 ohnehin meist
vermieden) noch wenn `save()` mangels Schreibrechten dauerhaft fehlschlägt
(Punkt 2 als Netz).

@note Ein AppImage mountet sein Dateisystem grundsätzlich read-only, was auch
`portfolio.db`, Backups und Logs betreffen könnte, falls diese ebenfalls
"neben der Executable" abgelegt werden. Das ist mit diesem Fix noch nicht
untersucht — bewusst zurückgestellt, da beim akuten Fall (frische Installation,
Verzeichnis vermutlich beschreibbar) `settings.ini` allein schon reproduzierbar
zur kompletten Blockade führte. Ein grundsätzlicher Wechsel auf
`QStandardPaths::AppConfigLocation`/`AppDataLocation` für alle
Laufzeit-Nutzerdaten (nicht nur `settings.ini`) wäre der robustere,
aber deutlich größere nächste Schritt, falls sich das read-only-Szenario
tatsächlich bestätigt.

### System-Locale-abhängiges Zahlenformat — Bugfix (23.07.2026)

Beim Einrichten der CI (siehe TESTING.md, GitHub-Actions-Abschnitt) fielen
unter Linux (System-Locale `C`/`en_US`, nicht Deutsch wie auf Nessies
Windows-Entwicklungsrechner) vier Tests fehl: `test_viewBrokerageEdit_
setGesamtGebuehren_updatesField`, `test_viewBrokerageEdit_
setBrokerageReduction_positiveGreen`/`_negativeRed` sowie
`test_viewDividendEdit_clearForm_resetsDerivedFields`.

Root Cause: `formatMoney()` (u. a. in `ViewBrokerageEdit`, `ViewShareEdit`,
`ViewBuyEdit`, `ViewDividendEdit`, `ViewShareAdd`) formatiert über
`QLocale().toString(value, 'f', 2)` — der No-Argument-Konstruktor `QLocale()`
verwendet die **System-Locale des ausführenden Rechners**, nicht fest
Deutsch. Auf einem System mit anderer Standard-Locale erscheint z. B. `12,50`
als `12.50` (Punkt statt Komma) — die betroffenen Tests suchen aber fest nach
deutsch formatierten Strings (`"12,50"` etc.).

@note Das **Einlesen** von Zahlen (`parseDouble()` in allen Views sowie
`PortfolioImporter::toDouble()` im XML-Importer) ist von diesem Bug NICHT
betroffen — dort wird das Komma manuell durch einen Punkt ersetzt und
anschließend `QString::toDouble()` verwendet, welches laut Qt-Dokumentation
IMMER locale-unabhängig (C-Konvention) arbeitet. Betroffen war ausschließlich
die **Anzeige** abgeleiteter/geladener Werte, keine Dateneingabe oder
-interpretation.

Fix: `QLocale::setDefault(QLocale::German)` wird jetzt zentral in `main()`
(vor der `QApplication`-Konstruktion) sowie im gemeinsamen `main()` von
`tst_mainwindow.cpp` gesetzt. Da alle betroffenen `formatMoney()`-Implementie-
rungen den No-Argument-Konstruktor `QLocale()` verwenden, greift dieser eine
zentrale Aufruf für die gesamte Anwendung und alle Formulare, ohne dass jede
View einzeln angepasst werden musste. `tst_buysform` und `tst_shareeditform`
waren von den Testfehlern nicht betroffen (ihre Tests prüfen keine deutsch
formatierten Anzeige-Strings) und wurden daher nicht angefasst — bei Bedarf
kann derselbe eine Zeile später ergänzt werden.

Bewusst nicht umgesetzt (siehe Offene Punkte oben): eine benutzerseitig
einstellbare Locale — das wäre ein eigenständiges Feature, kein Teil dieses
Bugfixes.

### test_deleteShare_actionDeleteEnabledAfterSelection — Timing-Bug behoben (21./22.07.2026)

Der Test nutzte `window.findChildren<QTableWidget*>().first()`, um sich die
Depotwert-Datentabelle zu holen. Je nach Timing konnte `.first()` stattdessen
eine der (noch) leeren Footer-Tabellen treffen, sodass der Test auf
`QSKIP("Table is empty — share not loaded yet")` zurückfiel, statt die Aktie
zuverlässig zu finden.

Erster Fix-Versuch (`findFinalTable(window, 1)` statt `.first()`) deckte einen
tieferliegenden Root Cause auf: Das Testsetup fügte nur eine Aktie per
`ShareRepository::insert()` ein — ohne Buy-Transaktion und ohne
`AppSettings::instance().setPortfolioPath()`. Dadurch erschien die Aktie nicht
zuverlässig mit genau einer Zeile in der Depotwert-Tabelle. Finaler Fix: das
bereits etablierte Helper-Muster `seedDepotwertPortfolio()` verwenden (Buy +
Brokerage + `AppSettings::portfolioPath` gesetzt), das auch
`test_finalValueTable_showsFinalFields()` u.a. zuverlässig nutzen. `QSKIP`
entfällt dadurch komplett.

### Sound bei erfolgreicher Aktualisierung (implementiert 21.07.2026)

Neue Methode `MainWindow::playUpdateFinishedSound()`, aufgerufen aus
`onRefreshShareFinished()` (siehe "Methode onRefreshShareFinished() —
Footer-Aktualisierung" oben). Spielt die in `AppSettings::soundUpdateFile()`
konfigurierte WAV-Datei aus `sounds/` neben der Executable ab, sofern
`AppSettings::soundUpdateEnabled()` true ist und die Datei existiert (die
Infrastruktur dafür — `SoundSettingsForm`, `AppSettings`, Existenzprüfung +
automatisches Deaktivieren beim Start — existierte bereits vorher; es fehlte
lediglich die eigentliche Wiedergabe).

Gilt sowohl für einen einzelnen Refresh als auch für "Alle aktualisieren" —
bei Letzterem aber bewusst nur **einmal am Ende** der Queue, nicht nach
jeder einzelnen Aktie (Nessies ausdrückliche Vorgabe). Der Sound wird nur
bei Erfolg abgespielt: der Aufruf sitzt im `else`-Zweig von
`onRefreshShareFinished()`, der durch den vorangehenden
`if (m_errorOccurred) { …; return; }`-Block nur bei fehlerfreiem Abschluss
erreicht wird.

`playUpdateFinishedSound()` ist `private virtual` deklariert (kein anderer
Zweck als Testbarkeit) — eine Testklasse (`SoundCountingMainWindow` in
tst_mainwindow.cpp) überschreibt sie, um Aufrufzeitpunkt/-anzahl zu prüfen,
ohne von echter `QSoundEffect`-Wiedergabe (benötigt ein Audio-Gerät, das in
CI/Testumgebungen ggf. fehlt) abhängig zu sein.

### ViewBrokerageEdit: Word-/Excel-Unterstützung nicht wieder eingebaut — bewusste Entscheidung (hinfällig, 19.07.2026, entschieden 20.07.2026)

`ViewBrokerageEdit::onBrowseDocument()` erlaubte bislang zusätzlich zu PDF
auch Word- (`.doc`/`.docx`) und Excel-Dateien (sowie "Alle Dateien") — als
einziger der fünf Dokument-Auswahl-Dialoge. Im Zuge der Root-Verzeichnis-
Durchsetzung (siehe "Durchsetzung 'nur Dokumente aus dem Root auswählbar'"
oben) auf Nutzer-Entscheidung hin vorerst auf denselben PDF-only-Filter wie
die anderen vier Dialoge reduziert, um Root-Prüfung und Dateifilter über
alle fünf Dialoge konsistent zu halten.

Nessies Entscheidung (20.07.2026): **nicht wieder einbauen.** Grund: eine
formatierungstreue Vorschau für Word/Excel wäre entweder kostenpflichtig
(Aspose, Spire, Syncfusion), an eine lokale Office-Installation gebunden
(Interop, nur Windows) oder erforderte eine schwergewichtige externe
Abhängigkeit (LibreOffice headless, ~300–600 MB, kein schlanker
Kommandozeilen-Helfer wie `pdftoppm`) — in allen Fällen unverhältnismäßiger
Aufwand gegenüber dem Nutzen. Wer ein Word-Dokument oder Excel-Sheet
hinterlegen möchte, wandelt es vorher eigenständig in ein PDF.

Konsequenz (umgesetzt 20.07.2026): In allen fünf `onBrowseDocument()`-
Methoden (`ViewBuyEdit`, `ViewSaleEdit`, `ViewDividendEdit`,
`ViewBrokerageEdit`, `ViewShareAdd`) wurde der zusätzliche Filter-Eintrag
`;;Alle Dateien (*)` entfernt — vorher hätte man im `QFileDialog` trotz
PDF-only-Vorauswahl auf "Alle Dateien" umschalten und doch eine Nicht-PDF-
Datei auswählen können, was der eigentlichen Entscheidung widersprochen
hätte. Der veraltete TODO-Kommentar (19.07.2026) in
`ViewBrokerageEdit::onBrowseDocument()` wurde entsprechend entfernt.

Die bereits vorhandene, rein defensive Icon-Auswahl-Logik (PDF-/Word-/
Excel-Icon je nach Dateiendung, in `ViewBuyEdit`, `ViewSaleEdit`,
`ViewDividendEdit`, `ViewBrokerageEdit` sowie neu ergänzt in `ViewShareAdd`)
bleibt bewusst erhalten — sie ist kein aktiver Auswahlweg, sondern nur eine
Absicherung für den Fall, dass doch einmal ein Nicht-PDF-Pfad in der DB
landet (Altbestand, manuelle DB-Änderung o. Ä.), und hält die Tür für eine
mögliche spätere Erweiterung offen.

### Dokument-Spalten: Breite auf 36px vereinheitlicht, keine Spaltenüberschrift (erledigt, ursprünglich 16.07.2026, umgesetzt 17.07.2026)

Nach dem Breiten-Fix in `ViewSaleEdit` (siehe "SalesForm auf
OverviewTabWidget/DocumentPreviewPanel umgestellt" oben, `kDocColWidth = 120`)
kam Nessies Feedback: die Dokument-Spalte verbraucht — trotz des Fixes — über
alle betroffenen Formulare hinweg weiterhin zu viel Platz für eine reine
Icon-Spalte. Betroffen waren:

- `ViewBuyEdit` (Kauf-Übersicht, vorher Stretch/`-1`)
- `ViewSaleEdit` (Verkaufs-Übersicht, vorher fix `kDocColWidth = 120`)
- `ViewDividendEdit` (vorher bewusster Zwischenstand auf Stretch/`-1`)
- `ViewBrokerageEdit` (vorher fix `kDocColWidth = 120`)
- `OverviewTabWidget`-Details-Tabs in `ViewShareDetails` (Gewinne/Verluste,
  Dividenden, Kosten; vorher fix `110`px)
- Die "Verwendete Käufe"-Tabelle im SalesForm-Details-Dialog
  (`onShowDetails()`, `kColDoc` dort separat; Breite war bereits `36`px,
  nur der Spaltenkopf-Text "Dok." fehlte noch)

`ViewShareEdit` (Übersichts-/Summenanzeige der Pencil-Button-Dialoge) wurde
gegengecheckt: dort existiert **keine** eigene Dokument-Spalte (nur
schreibgeschützte Summenfelder), kein Änderungsbedarf.

Reihenfolge (Nessies Vorgabe, 16.07.2026): Erst `DividendForm` und
`BrokeragesForm` vollständig auf `OverviewTabWidget`/`DocumentPreviewPanel`
umbauen (seit 16.07.2026 abgeschlossen, siehe Migrationsnotizen oben), dann
in einem eigenen Schritt global vereinheitlichen. Am 17.07.2026 umgesetzt,
für alle sieben Stellen in einem gemeinsamen Commit:

1. Dokument-Spalte überall fest auf `36`px (kein Stretch, keine 110/120px
   mehr) — reine Icon-Spalte ohne Textinhalt.
2. Keine Spaltenüberschrift für die Dokument-Spalte mehr (leerer `QString()`
   statt `tr("Dokument")`/`tr("Dok.")` im jeweiligen
   `jahresHeaders`/`uebersichtHeaders`-Array).

`kColDoc`/`jahresDocColumn` blieb überall unverändert stehen, der
Doppelklick auf die Icon-Spalte löst weiterhin `documentActivated()` aus —
das ist von der Breiten-/Header-Änderung unabhängig. Von Nessie gebaut und
getestet (alle Testfälle bestehen), Layouts der betroffenen Dialoge geprüft
und für gut befunden. Damit ist das Arbeitspaket "Tabs und Grids" vorerst
vollständig abgeschlossen.

### Brokerage-Vorwärts-Link: ModelBuyEdit/ModelBrokerageEdit geprüft und gefixt (erledigt, 15.07.2026, geprüft und gefixt 20.07.2026)

Der am 15.07.2026 in `ModelSaleEdit::addSale()`/`updateSale()` gefundene und
behobene Bug (fehlender Vorwärts-Link `sales.brokerage_guid` beim Anlegen
eines neuen Brokerage-Eintrags) wurde zunächst nur für den Verkaufs-Pfad
untersucht. Nachprüfung am 20.07.2026 ergab: **derselbe Bug steckte auch in
`ModelBuyEdit::updateBuy()`**, im Zweig "kein Brokerage vorhanden — neu
anlegen" — siehe "BuysForm-Details"/`ModelBuyEdit` oben für die volle
Analyse und den Fix (`BuyRepository::updateBrokerageGuid()`).
`ModelBuyEdit::addBuy()` und `ModelBrokerageEdit` waren beide nicht
betroffen. Damit ist dieser Punkt für alle drei genannten Stellen
vollständig abgeschlossen.

### OverviewTabWidget / onMainTabChanged(): Test-Lücken geschlossen (erledigt, 14.07.2026)

`OverviewTabWidget` hat seit 14.07.2026 ein eigenes Test-Target,
`tst_overviewtabwidget` (siehe TESTING.md) — deckt die Zwei-`QTabBar`-
Struktur (`count()`/`widget()`/`tabText()`/`currentIndex()`/
`setCurrentIndex()`), den fixierten Übersicht-Tab und beide Bugfixes
(Klick-Navigation über `tabBarClicked`, dauerhaft fette Spaltenköpfe) ab.
`ViewShareDetails::onMainTabChanged()` (Reset auf Jahresübersicht bei
äußerem Tab-Wechsel, siehe "OverviewTabWidget-Details" oben) ist durch
`test_mainTabChanged_resetsOverviewTabsToUebersicht` in `tst_mainwindow.cpp`
abgedeckt (direkt neben `test_shareDetailsDialog_validShare_
constructsAndShowsCloseButtonText`, siehe TESTING.md). `DocumentPreviewPanel`
und `ViewShareDetails` als Ganzes bleiben weiterhin durch kein eigenes
Test-Target abgesichert (siehe TESTING.md, Abschnitt `tst_sharedetailsform`,
und "DocumentPreviewPanel / ViewShareDetails: Testabdeckung bewusst nicht
erweitert" unten) — eine bereits entschiedene, unveränderte Einschränkung
aus früheren Sessions, keine neue.

### DocumentPreviewPanel / ViewShareDetails: Testabdeckung bewusst nicht erweitert (entschieden, 21.07.2026)

`DocumentPreviewPanel::showDocument()`/`clearDocument()` besitzen kein
öffentliches Zustands-API (kein Getter für den aktuell angezeigten
Dokumentpfad o. ä.). Eine direkte Prüfung "wird das richtige Dokument
angezeigt" ist daher nicht möglich, ohne ein rein testgetriebenes API
(z. B. `currentDocumentPath()`) einzuführen, das keinen Produktionsnutzen
hätte.

**Entscheidung:** Kein eigenes Test-Target für `DocumentPreviewPanel` bzw.
`ViewShareDetails` als Ganzes. Stattdessen wird die umliegende Logik
getestet (Zeilenauswahl, Signal-Payload `rowActivatedWithDocument()`,
Reset-Verhalten bei Tab-Wechsel — siehe `tst_overviewtabwidget` und
`tst_mainwindow.cpp`). Die tatsächliche Panel-Anzeige bleibt ungetestet.
Dieser Punkt gilt damit als abgeschlossen, nicht als offene Aufgabe.

### ShareDetailsForm: Gewinne/Dividenden/Kosten (erledigt, ursprünglich 12.07.2026, umgesetzt 13.07.2026)

Nach Abgleich mit der C#-Referenz (`FrmShareDetails`) wurden die "Komplette
Depotbewertung"- und "Komplette Marktbewertung"-Boxen sowie der Chart-Tab
neu gebaut (siehe "ShareDetailsForm-Details" und "ChartForm-Details" oben).
Der zunächst offene Teil — Gewinne/Verluste-, Dividenden- und Kosten-Tabs,
im C# je ein verschachteltes TabControl (Übersicht + Jahres-Tabs), nur im
Depotwert-Modus sichtbar — ist seit 13.07.2026 umgesetzt: eine neue,
schlanke Read-Only-Variante (`OverviewTabWidget`, siehe
"OverviewTabWidget-Details" oben) statt Wiederverwendung der
Editier-Dialog-Widgets, da eine Wiederverwendung eine Entkopplung vom
jeweiligen Editier-Formular vorausgesetzt hätte, die die bestehenden
Widgets nicht mitbrachten.

("Letzte Website-Aktualisierung" → `lastPriceUpdate()` wurde von Nessie am
10.07.2026 bestätigt, siehe "Marktwert- vs. Depotwert-Modus" oben — kein
offener Punkt mehr.)

### DocumentPreviewPanel: blockierender Dialog durch Inline-Anzeige ersetzt (erledigt, 19.07.2026)

`DocumentPreviewPanel::showDocument()` prüfte seit 13.07.2026 per
`QFileInfo::exists()`, ob die Datei noch existiert, zeigte bei einer
fehlenden Datei aber `OwnMessageBox::critical()` — ein blockierender
modaler Dialog. Für ein reines, passives Anzeige-Widget ist das unpassend:
es unterbricht insbesondere automatisierte Tests, die genau diesen Pfad
gezielt auslösen (`test_viewBrokerageEdit_openPdfPreview_nonExistentFile_
doesNotCrash` in `tst_mainwindow.cpp`, gemeldet 19.07.2026 — der Test lief
zwar durch, blieb aber an dem Dialog hängen, bis er manuell bestätigt wurde).

Ersetzt durch ein neues Inline-Label (`m_notFoundLabel`) direkt im Panel,
analog zu den bereits vorhandenen Inline-Fehlermeldungen im
pdftoppm-Fallback-Zweig ("PDF-Vorschau konnte nicht gerendert werden.",
"Vorschaubild nicht gefunden."). Unabhängig vom Render-Pfad
(`SPM_HAVE_QTPDF` oder pdftoppm-Fallback) vorhanden, standardmäßig
ausgeblendet. Kein Dialog mehr, kein Blockieren der Ereignisschleife.

Für Dialoge, die bereits an `DocumentPreviewPanel::showDocument()`
delegieren — `ViewBuyEdit`, `ViewSaleEdit`, `ViewDividendEdit`,
`ViewBrokerageEdit` (jeweils `openPdfPreview()` ruft
`m_previewPanel->showDocument()`) —, gilt der Fix automatisch mit. Bei
Nachprüfung (19.07.2026) bestätigt: `ViewSaleEdit` und `ViewDividendEdit`
delegierten bereits vollständig, nur `ViewShareAdd` hatte noch eine eigene,
nicht-delegierte `openPdfPreview()`-Implementierung ohne jede
Existenzprüfung. Seit der Migration auf `DocumentPreviewPanel` (siehe
"ViewShareAdd auf DocumentPreviewPanel umgestellt" oben) delegieren jetzt
alle fünf Editier-Dialoge — dieser Punkt ist damit vollständig
abgeschlossen.

### Spalten-Breiten-Schema für Dokument-Spalten auch in ShareEdit-Grids nachziehen (hinfällig, 14.07.2026, aufgelöst 17.07.2026)

Für die Jahres-Tabs von `OverviewTabWidget` (Gewinne/Verluste-, Dividenden-,
Kosten-Tab in `ShareDetailsForm`) hatte sich nach mehreren Anläufen folgendes
Spaltenbreiten-Schema bewährt: erste Spalte (Datum) fest, letzte Spalte
(Dokument) ebenfalls fest und ausreichend breit (110px — reichte für
Spaltenkopf-Text "Dokument" plus Icon, ohne abgeschnitten zu werden), alle
Spalten dazwischen als Stretch (`-1`). Vorherige Versuche mit einer zu
schmalen festen Dokument-Spalte (36px, aus den Editier-Dialogen übernommen)
oder mit durchgehend gestreckten Spalten führten je nach verfügbarer Breite
zu abgeschnittenem oder überlappendem Spaltenkopf-Text.

Dieser Punkt ist mit der globalen Vereinheitlichung vom 17.07.2026 (siehe
"Dokument-Spalten: Breite auf 36px vereinheitlicht, keine Spaltenüberschrift"
oben) hinfällig geworden: das ursprüngliche Problem war ausschließlich der
abgeschnittene Spaltenkopf-Text "Dokument" bei 36px — mit dem vollständigen
Wegfall jeglichen Header-Texts für die Dokument-Spalte tritt dieses Problem
nicht mehr auf, ein fixer 36px-Wert funktioniert jetzt überall unabhängig
von der Dialogbreite. `ViewShareEdit` wurde dabei gegengecheckt und hat
ohnehin keine eigene Dokument-Spalte (siehe oben) — auch die ursprünglich
offene Übertragungsfrage entfällt damit.

### Totes Mapping: PriceAtPayday in xmlNameToViewField() (entfernt 08.07.2026)

`PresenterDividendEdit::xmlNameToViewField()` enthielt ein Mapping
`"PriceAtPayday" → "priceAtPayday"`. Geprüft und geklärt (07.07.2026): **Keine**
Bank-Konfiguration in `app/config/Documents.xml` definiert für
`Document Type="Dividend"` ein `PriceAtPayday`-Feld — das Mapping war totes Gewicht
ohne Laufzeitauswirkung, kein Widerspruch zur Doku ("automatisch aus `daily_values`
befüllt, sonst manuell", siehe DividendForm-Details → Auto-Fill). Die Zeile wurde
entfernt, um Verwirrung bei künftiger Fehlersuche zu vermeiden.

Bewusst zunächst unangetastet gelassen (08.07.2026), inzwischen ebenfalls erledigt
(siehe unten): `knownXmlNames` in `populateFromResult()` enthielt `"PriceAtPayday"`
weiterhin — der Eintrag wurde dort seit jeher per `viewField.isEmpty() -> continue`
übersprungen (unabhängig vom Mapping, da kein Bank-Konfig das Feld ohnehin liefert).

### Folgepunkt: PriceAtPayday auch aus knownXmlNames entfernt (erledigt 08.07.2026)

Da `"PriceAtPayday"` in `knownXmlNames` mitgezählt wurde (`optionalTotal =
knownXmlNames.size() - reqTotal`), aber wegen des fehlenden Mappings nie als
gefunden zählen konnte (`optionalFound`), lief die Statusanzeige
("Analyse OK — X/Y Pflicht, N/M Optional") beim DividendForm strukturell nie auf
das volle `M` — selbst wenn alle real vorhandenen optionalen Felder erkannt
wurden. Der Eintrag wurde jetzt auch aus `knownXmlNames` entfernt, `optionalTotal`
sinkt dadurch korrekt um 1 und der Zähler kann wieder sein volles Maximum
erreichen. Keine bestehenden Tests hängen an einer festen `optionalTotal`-Zahl für
das DividendForm.

### Numerische Felder im XML-Importer (erledigt 08.07.2026)

`PortfolioValidator` prüfte bislang nur Datumsfelder auf Parsbarkeit — nicht
numerische Felder wie `SharePrice`, `Volume` oder `Provision`. Ein
nicht-parsbarer, aber nicht-leerer Wert fiel dadurch beim Import weiterhin
lautlos auf `0.0` zurück (`PortfolioImporter::toDouble()`), statt den Import
wie bei allen anderen Datenfehlern komplett zu blockieren.

Neue private Hilfsmethode `PortfolioValidator::isParsableGermanNumber()`
spiegelt die Parsing-Logik von `PortfolioImporter::toDouble()` exakt (Komma
als Dezimaltrennzeichen, optionaler Punkt als Tausendertrennzeichen), damit
"hier parsbar" und "ergibt beim Import einen echten Wert statt eines
0.0-Fallbacks" deckungsgleich bleiben. Ein **leerer** String gilt bewusst als
gültig — viele dieser Felder sind laut `Documents.xml` (`ResultEmpty="true"`)
legitim optional; nur ein nicht-leerer, aber unparsbarer Wert ist ein
Datenfehler.

Die Prüfung wurde in alle bestehenden `validateXxx()`-Methoden eingebaut:
`SharePrice`/`SharePriceBefore` (Share), `Volume`/`VolumeSold`/`Price` (Buy),
`Volume`/`SalePrice`/`TaxAtSource`/`CapitalGainsTax`/`SolidarityTax`/
`Reduction` (Sale) sowie je `UsedBuy`: `BuyVolume`/`BuyPrice`/`Reduction`/
`Brokerage`, `Provision`/`BrokerFee`/`TraderFee`/`Reduction` (Brokerage),
`Rate`/`Volume`/`TaxAtSource`/`CapitalGainTax`/`SolidarityTax`/
`PriceAtPayday` (Dividend) sowie `ExchangeRatio` bei Fremdwährungs-Dividenden
(nur geprüft, wenn ein `<ForeignCurrency>`-Element im Quell-XML überhaupt
vorhanden war), und `C`/`O`/`T`/`B`/`V` (DailyValue). Siehe Abschnitt
"Validierung vor dem Import" oben für die aktualisierte Prüftabelle, sowie
TESTING.md für die neuen Testfälle in `tst_portfoliovalidator`.

### BackupSettingsForm (erledigt 08.07.2026)

Dedizierter Konfigurationsdialog für Backup-Einstellungen, analog zu
`LoggerSettingsForm`/`SoundSettingsForm` als einzelner `QDialog` umgesetzt
(kein eigenes IView/IModel/Presenter-Triple — für einen reinen
Einstellungsdialog ohne eigene Geschäftslogik ist das leichtgewichtige Muster
dieser beiden Vorbilder ausreichend und konsistent zum Rest der
Settings-Dialoge). Aufrufbar über `Einstellungen → Backup...` in der
Menüleiste, neben `Logger...` und `Sound...`.

| Einstellung | Beschreibung | Standardwert |
| ------ | ------ | ------ |
| Backup aktivieren | Backup beim Öffnen ein-/ausschalten | ✅ aktiv |
| Max. Anzahl Backups | Wie viele Backups vorgehalten werden (editierbare Combobox: 1/3/5/10/20/50 oder freier Wert) | 5 |
| Namensschema | Präfix (Textfeld) + Qt-Datumsformat (Textfeld) für den Zeitstempel im Dateinamen, mit Live-Vorschau des resultierenden Dateinamens | Präfix `Backup`, Format `yyyy_MM_dd_HH_mm_ss` |
| Backup-Verzeichnis | Zielverzeichnis, wählbar über Browse-Button; leer = gleicher Ordner wie die Portfolio-Datei | leer (Portfolio-Verzeichnis) |

Die Einstellungen liegen in `AppSettings` unter dem INI-Abschnitt `Backup`
(`backupEnabled()`/`backupMaxCount()`/`backupNamePrefix()`/
`backupDateFormat()`/`backupDirectory()` mit den zugehörigen Settern, alle
speichern sofort per `save()` — gleiches Muster wie bei den bestehenden
Logger-/Sound-Einstellungen). Mit den Standardwerten erzeugt
`MainWindow::createBackup()` exakt dieselben Dateinamen wie vor dieser
Änderung (`Backup_<Dateiname>_YYYY_MM_DD_HH_mm_ss.db`) — reines Umstellen auf
konfigurierbare Werte, keine Verhaltensänderung im Default-Fall.

`createBackup()` liest jetzt vor jedem Lauf:

- `backupEnabled()`: ist Backup deaktiviert, kehrt die Methode sofort
  zurück (kein Log-Eintrag als Statusmeldung, nur `qInfo()` — analog dazu,
  wie bisher schon eine fehlende Portfolio-Datei still übersprungen wurde).
- `backupDirectory()`: leer → wie bisher `fi.absolutePath()` der
  Portfolio-Datei. Ist ein eigenes Verzeichnis konfiguriert und existiert es
  noch nicht, wird es per `QDir::mkpath()` angelegt; schlägt das fehl, wird
  eine Warn-Statusmeldung ausgegeben und kein Backup erstellt.
- `backupNamePrefix()` / **`backupDateFormat()`**: ersetzen die bisher
  fest codierten Literale `"Backup"` bzw. `"yyyy_MM_dd_HH_mm_ss"` beim
  Erzeugen des neuen Dateinamens.
- `backupMaxCount()`: ersetzt das bisherige `constexpr int kMaxBackups = 5`;
  über `qMax(1, ...)` gegen einen Wert ≤ 0 abgesichert (z. B. falls die INI
  von Hand manipuliert wurde).

Rotation: Namensfilter präfix-unabhängig, Sortierung nach Änderungsdatum
(Nachtrag 08.07.2026): Auf Nutzer-Rückfrage geprüft — "funktioniert die
Rotation noch, wenn Präfix oder Datumsformat geändert werden?" — und dabei
zwei Robustheitslücken behoben:

- Namensfilter ohne Präfix: Die Rotation filtert nach
  `*_<Portfolioname>_*.<Endung>`, nicht nach
  `<Präfix>_<Portfolioname>_*.<Endung>`. Mit einem präfixgebundenen Filter
  würde eine Präfix-Änderung in `BackupSettingsForm` alle bisherigen Backups
  aus der Zählung herausfallen lassen — "Max. Anzahl Backups" gälte dann
  faktisch nur noch pro Präfix statt insgesamt, und alte Backups blieben nach
  einer Präfix-Änderung für immer liegen, weil sie den neuen Filter nicht
  mehr treffen. Der Portfolioname (Basisdateiname) plus Endung reicht als
  Anker aus, um Backups dieses Portfolios von fremden Dateien im selben
  Verzeichnis zu unterscheiden.
- Sortierung nach `QFileInfo::lastModified()`, nicht nach Dateiname: Eine
  rein alphabetische Sortierung wäre nur zufällig korrekt gewesen, solange
  `backupDateFormat()` nullgepolstert und groß-nach-klein aufgebaut ist (wie
  der Standard `yyyy_MM_dd_HH_mm_ss`). Ändert der Benutzer das Format
  nachträglich — z. B. auf `dd_MM_yyyy_HH_mm_ss` (Tag zuerst) — wäre die
  chronologische Reihenfolge per Namens-Sortierung nicht mehr gegeben,
  insbesondere wenn ältere Backups noch mit dem alten Format benannt sind und
  über denselben (präfix-unabhängigen) Namensfilter erfasst werden. Das
  tatsächliche Änderungsdatum der Datei ist von der gewählten
  Textdarstellung unabhängig und bleibt daher auch nach einer
  Formatänderung korrekt.
- Leerer/fehlerhafter Präfix bzw. leeres Datumsformat:
  `BackupSettingsForm::saveSettings()` ersetzt leere Eingaben bereits vor dem
  Speichern durch die Standardwerte. `createBackup()` verlässt sich darauf
  zusätzlich nicht blind, sondern wendet denselben Fallback nochmal auf
  `settings.backupNamePrefix()`/`settings.backupDateFormat()` an — falls die
  `settings.ini` einmal von Hand bearbeitet wird und dort ein leerer Wert
  steht, entsteht kein Dateiname mit führendem `_` oder ein `toString()` mit
  leerem Formatstring.

### Parser-Mocking-Infrastruktur (erledigt 07.07.2026)

`ParserLib::Parser` besaß bisher keinen Test-Seam — der `QNetworkAccessManager`
wurde intern erzeugt (`new QNetworkAccessManager(this)`), sodass Tests, die den
Web-Modus durchlaufen wollten, echte Netzwerkzugriffe gebraucht hätten. Das war
der Hauptgrund, warum die Refresh-Flow-Tests (siehe TESTING.md, Abschnitt
"Refresh-Flow") bislang zurückgestellt waren.

Gelöst über einen zweiten `Parser`-Konstruktor:

```cpp
explicit Parser(QObject* parent = nullptr);                                    // bestehend, unveraendert
explicit Parser(QNetworkAccessManager* networkManager, QObject* parent = nullptr); // neu
```

Der injizierte `QNetworkAccessManager` bleibt im Besitz des Aufrufers — `Parser`
löscht und reparentet ihn nicht. Für Tests existiert dazu
`ParserTestUtils::FakeNetworkAccessManager` (`tests/parser/FakeNetworkAccessManager.h/.cpp`):
eine `QNetworkAccessManager`-Subklasse, die `createRequest()` überschreibt und statt
eines echten HTTP-Requests eine vorab per `setResponse()`/`setError()` hinterlegte
Antwort über einen internen `FakeNetworkReply` (`QNetworkReply`-Subklasse) liefert.
Die Auslieferung erfolgt über `QTimer::singleShot(0, ...)`, damit Tests weiterhin
einen Event-Loop-Durchlauf abwarten müssen (`QSignalSpy::wait()`) — der reale
Timing-Charakter des asynchronen Downloads bleibt damit erhalten, nur eben ohne
Netzwerk.

Bewusst **kein** `IParser`-Interface: `Parser` ist eine `QObject`-Klasse mit einem
echten Qt-Signal (`parserUpdated`); ein Interface hätte entweder Mehrfachvererbung
von `QObject` oder einen Bruch mit dem Signal/Slot-Muster erzwungen, nur um
Mockbarkeit zu erkaufen. Die gewählte Lösung ändert an der Produktions-API von
`Parser` und `MainWindow` nichts (zweiter Konstruktor ist rein additiv) und lässt
den echten Parser-Code (Regex-Matching, OnVista-/Yahoo-JSON-Mapping, Busy-/
Reentrancy-Handling) unverändert durch die Tests laufen, statt ihn in einem
Fake nachzubilden.

`tests/parser/tst_parser.cpp` nutzt die neue Infrastruktur für mehrere
Web-Modus-Tests (Regex, OnVista-Realtime, Yahoo-History, Netzwerkfehler,
Busy-Guard, Reentrancy-Regression) — siehe TESTING.md für die vollständige
Testliste. Dabei wurde auch eine bestehende Inkonsistenz bereinigt: die frühere
Testmethode `test_start_fails_when_busy` prüfte tatsächlich nur den
`NoRegexListGiven`-Guard, nie echtes Busy-Verhalten (der Kommentar "Can't easily
test without a real network" war der Grund dafür) — sie wurde zu
`test_start_fails_when_noRegexListGiven` umbenannt, und ein echter
Busy-Guard-Test (`test_start_fails_when_busy_viaFakeNetwork`) kam neu hinzu.

`MainWindow` erhielt darauf aufbauend (ebenfalls 07.07.2026) einen zweiten,
test-only Konstruktor `MainWindow(QNetworkAccessManager* networkManagerForTesting,
QWidget* parent = nullptr)`, der den injizierten (Fake-)`QNetworkAccessManager`
an `m_parserMarketValues` und `m_parserDailyValues` durchreicht. Umgesetzt als
verhaltensneutraler Refaktor: der komplette bisherige Konstruktor-Body wurde in
eine private `initialize()`-Methode ausgelagert, die von beiden Konstruktoren
aufgerufen wird — nur die Parser-Member-Initialisierung unterscheidet sich
zwischen den beiden Konstruktoren. `tests/forms/CMakeLists.txt` bindet
`FakeNetworkAccessManager.h/.cpp` aus `tests/parser/` sowie `Qt6::Network` ein.

Erste `tst_mainwindow`-Tests, die den kompletten Pfad `startRefreshForShare()`
→ `onMarketValuesUpdated()` → Grid-Update darüber abdecken: der
Icon-Update-Regressionstest (Bugfix 06.07.2026) und der `enableShareActions`-
Busy-Guard-Test (siehe TESTING.md, Abschnitt "Refresh-Flow", für Details).
Letzterer deckte dabei einen echten, bis dahin unbekannten Bug auf (siehe
"Bugfix 07.07.2026 — Lücke im Busy-Guard" oben).

`buildDailyValuesUrl()` wurde ebenfalls 07.07.2026 abgeschlossen — von
`private const` auf `public static` umgestellt (die Methode griff nie auf
Instanzzustand zu) und direkt getestet, ohne `QMetaObject::invokeMethod`
(siehe TESTING.md, Abschnitt "`buildDailyValuesUrl()` — erledigt").

### onDailyValuesUpdated()-Pfad (erledigt 08.07.2026)

Analog zu `onMarketValuesUpdated()` (siehe oben) ist jetzt auch der
`DailyValues`-Zweig über `FakeNetworkAccessManager` end-to-end abgedeckt:
Einzel-Refresh mit `ShareUpdateType::DailyValues` (Upsert in
`DailyValuesRepository` + Statusmeldung), reentrante Verkettung über eine
Zwei-Aktien-Queue (analog zu den `MarketPrice`-Queue-Tests — `m_marketDone`
ist bei `DailyValues`-only von vornherein `true`, sodass
`onDailyValuesUpdated()` allein `onRefreshShareFinished()` auslöst), sowie
`ShareUpdateType::Both`, wo `onRefreshShareFinished()` nachweislich erst
feuert, wenn beide Parser (Markt- und Tageswerte) unabhängig voneinander
fertig sind. Details siehe TESTING.md, Abschnitt
"`onDailyValuesUpdated()`-Pfad".

Grid-Selektion während der "Alle aktualisieren"-Queue (Fortschritt über
mehrere Aktien sowie der Fehlerfall) ist ebenfalls seit 07.07.2026 getestet
— über eine Mehr-Aktien-Queue plus `fakeNam.requestCount()` als
deterministischer Checkpoint für Zwischenzustände, die wegen der
Reentrancy (Bugfix 05.07.2026) sonst racy wären (siehe TESTING.md, Abschnitt
"Grid-Selektion während 'Alle aktualisieren'").

Footer-Update bei Refresh (`refreshPortfolioFooters()`, aufgerufen aus
`onRefreshShareFinished()` nur im Erfolgsfall, vor dem Verketten zur nächsten
Aktie in der Queue) ist ebenfalls seit 07.07.2026 getestet: Update nach
Einzel-Refresh, inkrementelles Update zwischen den Aktien einer
"Alle aktualisieren"-Queue (nicht erst am Ende), sowie dass der Footer im
Fehlerfall unverändert bleibt (siehe TESTING.md, Abschnitt "Footer-Update bei
Refresh"). Damit sind alle drei ursprünglich offenen Refresh-Flow-Testpunkte
(Grid-Selektion, `buildDailyValuesUrl()`, Footer-Update) abgeschlossen.

---

## Plattform-Unterstützung

| Plattform | Status | Besonderheiten |
| ------ | ------ | ------ |
| Linux (x86_64) | ✅ | GCC / Clang |
| Windows (x64) | ✅ | MSVC 2022 oder MinGW, windeployqt |
| macOS | ⚠️ möglich | Nicht explizit getestet |

---

## Externe Abhängigkeiten

| Abhängigkeit | Version | Zweck | Bezug |
| ------ | ------ | ------ | ------ |
| Qt | 6.6+ | UI, SQL, Network, i18n | qt.io / apt |
| Qt PDF | 6.4+ | Natives PDF-Rendering (`QPdfView`), Fallback auf pdftoppm | Qt Maintenance Tool → Qt 6.x → Qt PDF |
| CMake | 3.21+ | Build-System | cmake.org / apt |
| pdftotext | aktuell | PDF → Text für Parser — XpdfReader oder Poppler | apt: `poppler-utils` oder xpdfreader.com |
| Graphviz | aktuell | Doxygen-Diagramme | apt: `graphviz` |
| Doxygen | 1.10+ | Code-Dokumentation | doxygen.nl |

---

## XML-Import-Tool (tools/xml-importer)

Eigenständiges Console-Tool zum einmaligen Import einer Portfolio.xml der alten
C#-SharePortfolioManager-Anwendung in eine spm-qt SQLite-Datenbank. **Kein**
Bestandteil des `SharePortfolioManager`-Targets — eigenes ausführbares Programm
`spm-xml-importer`, das die bestehenden Models/Repositories (ShareObject,
BuyObject, ... / ShareRepository, BuyRepository, ...) wiederverwendet, damit
importierte Daten von UI-erfassten Daten ununterscheidbar sind.

@code{.unparsed}
tools/xml-importer/
├── CMakeLists.txt
├── main.cpp                  # CLI (QCommandLineParser)
├── XmlPortfolioParser.h/.cpp # reiner XML → Struct-Parser (RawShare, RawBuy, ...)
├── PortfolioImporter.h/.cpp  # Struct → DB (Repositories), Konvertierung, Dedupe
└── ImportLogger.h/.cpp       # Datei- + Konsolen-Logging inkl. Zusammenfassung
@endcode

### Aufruf

```
spm-xml-importer <input.xml> <portfolio.db> [--dry-run] [--log <path>]
```

`portfolio.db` wird über `Database::instance().open()` geöffnet — bei einer neuen
Datei legt das dieselbe Schema-DDL an wie die Hauptanwendung. `--dry-run` führt
den kompletten Mapping-/Prüflauf aus, schreibt aber nichts in die Datenbank.

### Mapping XML → Schema

| XML | Ziel | Anmerkung |
| ------ | ------ | ------ |
| `<Share WKN/ISIN/Name/Update>` | `shares` | `Update` → `ShareUpdateType` (None/MarketPrice/DailyValues/Both, Default "Both") |
| `<StockMarketLaunchDate>` | `shares.add_datetime` | entspricht der "Börsennotierung" (`listingDate`), siehe C#-Kompatibilitätshinweis in `PresenterShareEdit` |
| `<DetailsWebSite>` / `<MarketValue WebSite>` / `<DailyValues WebSite>` | `shares.details_website` / `market_value_url` / `daily_values_url` | Doppelt-XML-escapte Ampersands (`&amp;amp;` in der Quelle → literales `&amp;` nach dem Parsen) werden von `XmlPortfolioParser::normalizeWebSiteUrl()` zu `&` korrigiert und als `INFO` protokolliert. Ein Element `<MarketValues>` (Plural) statt `<MarketValue>` wird dagegen NICHT akzeptiert, sondern als struktureller Datenfehler erkannt und blockiert seit 05.07.2026 über `PortfolioValidator` den kompletten Import — siehe Abschnitte "URL-Normalisierung" und "Validierung vor dem Import" unten |
| `<MarketValue Parsing>` / `<DailyValues Parsing>` | `*_parsing_type` | "ApiYahoo"/"ApiOnVista"/"ApiOnvista" (case-insensitive) → `ShareParsingType`, sonst `Regex` |
| `<Culture>` | — | keine Entsprechung im aktuellen Schema, wird geloggt und ignoriert |
| `<Buy>` | `buys` | GUID aus XML wird direkt übernommen |
| `<Sale><UsedBuys><UsedBuy>` | `sales` + `sale_buy_details` | `UsedBuy` → `SaleBuyDetail` (FIFO-Zuteilung) |
| `<Brokerage BuyPart/SalePart/GuidBuySale>` | `brokerage` | `GuidBuySale` wird gegen `buys`/`sales` verifiziert (nicht `BuyPart`/`SalePart` blind übernommen) — siehe Abschnitt "Brokerage-Zuordnung" unten |
| `<Dividend><ForeignCurrency>` | `dividends` | `Flag="Checked"` → `enable_fc=true`, sonst FX-Felder auf Default (1.0 / "EUR") |
| `<DailyValues><Entry D/C/O/T/B/V>` | `daily_values` | `INSERT OR REPLACE` über `(share_guid, date)` — Re-Import aktualisiert vorhandene Tage |

Alle numerischen/Datums-Felder liegen im Quell-XML im deutschen Format
(Komma-Dezimaltrennzeichen, `dd.MM.yyyy[ HH:mm]`) und werden von
`PortfolioImporter` in ISO-8601-Strings bzw. `double` konvertiert
(`toDouble()`, `toIsoDate()`, `toIsoDateTime()`).

### Insert-Reihenfolge (Foreign-Key-bedingt)

```
shares → buys → sales (+ sale_buy_details) → brokerage → dividends → daily_values
```

`brokerage.buy_guid`/`brokerage.sale_guid` haben echte `REFERENCES`-Constraints
auf `buys`/`sales`, deshalb wird die Brokerage-Tabelle zuletzt befüllt.
`buys.brokerage_guid`/`sales.brokerage_guid` sind dagegen einfache TEXT-Spalten
ohne FK — der Wert kann also gesetzt werden, bevor die Brokerage-Zeile existiert.

### Tageswerte: Change-Tracking im Log (ergänzt 05.07.2026)

`DailyValuesRepository::upsertList()` vergleicht seit dem 05.07.2026 jeden
eingehenden Datensatz gegen den bestehenden DB-Eintrag (`findByShareAndDate()`)
und liefert optional `UpsertStats` (`fetched`/`inserted`/`updated`/`unchanged`)
zurück; unveränderte Zeilen werden dabei nicht neu geschrieben. Der
Refresh-Flow (`MainWindow::onDailyValuesUpdated()`) nutzt das bereits für eine
detaillierte Statusmeldung. `PortfolioImporter::importDailyValues()` übergibt
den `stats`-Parameter jetzt ebenfalls und loggt analog dazu
`"<n> Tageswert(e) geholt (Eingefügt: X / Aktualisiert: Y / Unverändert: Z)"`
statt wie zuvor nur die pauschale Gesamtanzahl. Wurde dabei tatsächlich nichts
geschrieben (alle Zeilen unverändert), wird als Action `SKIPPED` statt
`INSERTED` protokolliert, damit die Log-Zusammenfassung nicht suggeriert, es
sei etwas passiert.

Im Dry-Run-Zweig (`m_dryRun`) bleibt es bei der reinen Gesamtanzahl ohne
Eingefügt/Aktualisiert/Unverändert-Aufschlüsselung: eine solche Vorschau würde
die DB-Vergleichslogik aus `upsertList()` duplizieren, ohne dass am Ende
tatsächlich etwas geschrieben wird — der Aufwand steht in keinem Verhältnis
zum Nutzen einer reinen Trockenlauf-Vorschau.

### Brokerage-Zuordnung: Verifikation statt Vertrauen (seit 02.07.2026)

Die Zuordnung einer `<Brokerage GuidBuySale="...">` zu Buy oder Sale wird
**nicht** aus den Attributen `BuyPart`/`SalePart` der Quelle übernommen,
sondern anhand der zu diesem Zeitpunkt bereits importierten `buys`/`sales`-
Tabellen verifiziert (`BuyRepository::findByGuid()` / `SaleRepository::findByGuid()`):

- `GuidBuySale` existiert nur als Buy → `buy_guid` gesetzt.
- `GuidBuySale` existiert nur als Sale → `sale_guid` gesetzt.
- `GuidBuySale` existiert in **keiner** der beiden Tabellen (z. B. weil der
  referenzierte Datensatz selbst an einem Fehler wie einer OrderNumber-
  Kollision gescheitert ist) → `ERROR`, Brokerage wird übersprungen.
- `GuidBuySale` existiert in **beiden** Tabellen (GUID-Kollision zwischen
  Buy und Sale, bei echten UUIDs praktisch ausgeschlossen) → `ERROR`,
  nicht automatisch aufgelöst.

Widerspricht das Ergebnis den `BuyPart`/`SalePart`-Flags der Quelle, wird das
per `INFO`-Zeile protokolliert, aber die anhand der Datenbank ermittelte
Zuordnung verwendet — die Flags sind reine Zusatzinformation und werden nicht
als Wahrheitsquelle behandelt.

Hintergrund: Beim Import vom 01.07.2026 trugen zwei Verkaufs-Brokerages
(`BuyPart="True"` statt `"False"`) fälschlich `BuyPart="True"`, obwohl
`GuidBuySale` auf eine Sale zeigte — ein Datenfehler in der alten C#-Quelle.
Mit der ursprünglichen (flag-basierten) Logik führte das zu
`FOREIGN KEY constraint failed`, da `buy_guid` auf eine nicht existierende
Buy-GUID gesetzt wurde. Mit verifikationsbasierter Zuordnung wird die Sale
korrekt erkannt und verknüpft, unabhängig davon, was die Flags behaupten.

### URL-Normalisierung: doppelt-XML-escapte Ampersands & Tag-Varianten (gemeldet und behoben 05.07.2026)

Beim Ausführen von "Alle aktualisieren" auf einem über `tools/xml-importer`
importierten Portfolio wurde zunächst vermutet, dass bei **Nvidia** und
**Wacker (Wacker Chemie)** die Tageswerte-URL (`shares.daily_values_url`)
fehlte. Tatsächlich handelt es sich um zwei unabhängige Datenqualitätsprobleme
in der alten C#-Quelle, die zufällig dieselben zwei Aktien betreffen:

Fall 1 — Doppelt-XML-escapte Ampersands: Die Quell-XML enthielt in den
`WebSite`-Attributen `&amp;amp;` statt `&amp;` (bestätigt per
`grep -n "&amp;amp;"` auf der realen Quell-XML, 3 Fundstellen: Wacker
`MarketValue`, Nvidia `MarketValue` und `DailyValues`). Ein konformer
XML-Parser (auch `QXmlStreamReader`) löst Entities nur **einmal** auf:
`&amp;amp;` wird dabei zu literalem `&amp;` (nicht zu `&`), da die erste
`&amp;`-Entity zu `&` aufgelöst wird und das direkt folgende `amp;` danach nur
noch literaler Text ist, kein weiterer Entity-Match — das Ergebnis ist eine
kaputte, aber nicht leere URL.

`MainWindow::buildDailyValuesUrl()`/`startRefreshForShare()` normalisieren zur
Laufzeit bereits einen einzelnen Escape-Level (`.replace("&amp;", "&")`,
"carried over from C# XML storage") — das reicht für einfach escapte URLs im
selben Bestand (z. B. BMW.DE, unauffällig) und tatsächlich auch für den
doppelt-escapten Fall, weil nach dem einmaligen XML-Unescape dort literal
`&amp;` (einfach) steht, was die Laufzeit-Normalisierung korrekt zu `&`
auflöst. Der Refresh dürfte also praktisch funktioniert haben. Trotzdem blieb
der DB-Rohwert (`shares.daily_values_url`/`market_value_url`) "verschmutzt"
und das zugrunde liegende Datenproblem in der Quelle unbemerkt.

Fall 2 — Element `<MarketValues>` (Plural) statt `<MarketValue>` (Singular):
Bei genau denselben zwei Aktien heißt das Element in der Quell-XML
`<MarketValues>` statt `<MarketValue>` wie beim Rest des Bestands
(`grep -c "<MarketValue "` → 32 Treffer, `grep -c "<MarketValues "` → 2
Treffer). Anders als der Ampersand-Fall (eindeutig sicher zu normalisieren)
ist ein falscher Elementname ein struktureller Fehler in der Quelle — der
Importer darf hier nicht raten/interpretieren, sondern muss den Fehler
melden.

Fix: `XmlPortfolioParser::parseShare()` erkennt `<MarketValues>`
explizit als Datenfehler und protokolliert ihn über `RawShare::parseErrors`,
statt das Element zu interpretieren. `normalizeWebSiteUrl()` erkennt
weiterhin unabhängig davon ein literales `&amp;` im bereits einmal
entschärften Attribut-/Element-Wert (`DetailsWebSite`, `MarketValue@WebSite`,
`DailyValues@WebSite`) und korrigiert es sicher zu `&` — diese beiden Fälle
können für dieselbe Aktie gleichzeitig, aber unabhängig voneinander auftreten
(bei Nvidia: falscher Elementname bei `MarketValue` **und** doppelt-escaptes
Ampersand bei `DailyValues`).

Seit Einführung von `PortfolioValidator` (05.07.2026, siehe Abschnitt
"Validierung vor dem Import" unten) fließt jeder Eintrag in
`RawShare::parseErrors` direkt in die Vorab-Validierung ein: Ein solcher
Fehler verhindert nicht mehr nur das eine Feld dieser einen Aktie, sondern
den kompletten Import — auch aller anderen, für sich genommen fehlerfreien
Aktien in derselben Datei. `PortfolioImporter` loggt `parseWarnings`
(sicher auto-korrigiert) weiterhin als `INFO`, `parseErrors` dagegen als Teil
des strukturierten Validierungsberichts.

### Validierung vor dem Import (PortfolioValidator, seit 05.07.2026)

Bis 05.07.2026 galt für Fehler auf Datensatz-Ebene durchgängig "loggen und
überspringen, Import läuft weiter" (siehe Git-Historie). Das konnte die DB in
einem inkonsistenten Zustand hinterlassen — z. B. eine Aktie mit dem ersten
von zwei Buys importiert, der zweite (an einer `OrderNumber`-Kollision
gescheitert) fehlte einfach, ohne dass das auf den ersten Blick auffiel.

Seitdem läuft vor jedem Import eine vollständige Vorab-Prüfung der gesamten
Datei (`PortfolioValidator::validate()`, aufgerufen von
`PortfolioImporter::importPortfolio()`, bevor auch nur eine Zeile geschrieben
wird). Findet sie irgendwo in der Datei — in irgendeiner Aktie, irgendeinem
Datensatz — ein Problem, wird **gar nichts** importiert, auch nicht die
Aktien, die für sich genommen fehlerfrei wären. `importPortfolio()` gibt in
diesem Fall `false` zurück (`spm-xml-importer` beendet sich mit Exit-Code `4`,
unterscheidbar von `0`/Erfolg für Skripte).

Geprüft wird pro Aktie:

| Bereich | Prüfung |
| ------ | ------ |
| Share | WKN vorhanden; `Update`/`ShareType`/`Parsing` sind bekannte Werte (`Parsing` darf auch leer oder `Regex` sein — beides legitim, kein Datenfehler); `StockMarketLaunchDate`/`LastUpdateInternet`/`LastUpdateShareDate` parsbar; vom Parser bereits erkannte strukturelle Fehler (`RawShare::parseErrors`, z. B. `<MarketValues>`-Tag) |
| Buy/Sale/Brokerage/Dividend | GUID vorhanden; `Date` parsbar; `OrderNumber` (Buy/Sale) weder innerhalb derselben Aktie in der aktuellen Datei noch in der DB unter einer **anderen** GUID doppelt — ein Re-Import derselben GUID/OrderNumber ist ausdrücklich kein Fehler (Idempotenz) |
| Brokerage | `GuidBuySale` muss genau einen Buy oder eine Sale dieser Aktie treffen (aktuelle Datei oder bereits in der DB), nicht keinen und nicht beide |
| DailyValue | `D` (Datum) parsbar |
| Aktienübergreifend (pro Aktie) | GUIDs von Buy/Sale/Brokerage/Dividend derselben Aktie müssen untereinander eindeutig sein |
| Numerische Felder (ergänzt 08.07.2026) | Jedes numerische Attribut über alle Kategorien hinweg (`SharePrice`/`SharePriceBefore`; Buy `Volume`/`VolumeSold`/`Price`; Sale `Volume`/`SalePrice`/`TaxAtSource`/`CapitalGainsTax`/`SolidarityTax`/`Reduction` sowie je `UsedBuy`: `BuyVolume`/`BuyPrice`/`Reduction`/`Brokerage`; Brokerage `Provision`/`BrokerFee`/`TraderFee`/`Reduction`; Dividend `Rate`/`Volume`/`TaxAtSource`/`CapitalGainTax`/`SolidarityTax`/`PriceAtPayday` sowie bei Fremdwährung `ExchangeRatio`; DailyValue `C`/`O`/`T`/`B`/`V`) muss entweder leer sein (legitimer "nicht gesetzt"-Zustand, siehe unten) oder im deutschen Zahlenformat parsbar |

Bei den numerischen Feldern gilt dieselbe Kulanz wie beim Datumsformat: ein
**leerer** Wert ist kein Fehler — viele dieser Felder sind laut
`Documents.xml` (`ResultEmpty="true"`) legitim optional, und
`PortfolioImporter::toDouble("")` liefert bewusst `0.0`. Ein Problem liegt nur
vor, wenn ein Feld **nicht-leer, aber nicht als Zahl parsbar** ist — genau der
Fall, der vor dieser Erweiterung lautlos auf `0.0` zurückfiel.

Bei einem Fehlschlag wird ein strukturierter Bericht geloggt, gruppiert nach
Aktie (WKN + Name), darunter alle gefundenen Probleme dieser Aktie, damit auf
einen Blick sichtbar ist, wo nachgebessert werden muss:

```
════════════════════════════════════════════════════════
VALIDIERUNG FEHLGESCHLAGEN — keine Daten wurden geschrieben.
════════════════════════════════════════════════════════
Aktie: NVDA ("Nvidia")
  - [MarketValue.Parsing] Unbekannter Parsing-Wert "ApiYaho" ...
  - [Buy ORD-123] Datum "32.13.2024" nicht parsbar (erwartet: dd.MM.yyyy).
════════════════════════════════════════════════════════
Gesamt: 1 Aktie(n) betroffen, 2 Problem(e) insgesamt.
════════════════════════════════════════════════════════
```

Die bisherigen Datensatz-Ebene-Prüfungen in `importBuys()`/`importSales()`/
`importBrokerages()`/... (fehlende GUID, nicht auflösbare `GuidBuySale`, ...)
bleiben unverändert im Code — nach einer erfolgreichen Validierung sollten sie
nicht mehr greifen, dienen aber als defensives Sicherheitsnetz, falls die
Vorab-Prüfung eine Lücke hat.

### Idempotenz / Wiederholbarkeit

- Shares werden über die WKN abgeglichen (`ShareRepository::findByWkn`).
  Existiert die Aktie bereits, wird ihre GUID wiederverwendet und die
  Stammdaten bleiben unangetastet — es werden nur fehlende Kindobjekte importiert.
- Buys/Sales/Dividends/Brokerages übernehmen die GUID direkt aus dem
  Quell-XML. Vor dem Insert prüft der Importer per `findByGuid()`, ob der
  Datensatz schon existiert, und überspringt ihn dann (`SKIPPED`). Ein erneuter
  Lauf über dieselbe (oder eine aktualisierte) Export-Datei ist damit sicher —
  `PortfolioValidator` behandelt einen solchen Re-Import derselben GUID
  ausdrücklich nicht als `OrderNumber`-Kollision (siehe oben).
- Daily values verwenden `INSERT OR REPLACE` über den Composite-Key
  `(share_guid, date)` und sind dadurch immer gefahrlos erneut importierbar.

### Fehlerverhalten

Seit der Einführung von `PortfolioValidator` (siehe oben) ist das
Fehlerverhalten zweigeteilt:

- Vor dem Import: Jedes gefundene Problem — egal in welcher Aktie —
  verhindert den kompletten Lauf. Es gibt keine Teilimporte mehr.
- Während des Imports: Die bereits bestehenden Datensatz-Ebene-Prüfungen
  (fehlende GUID, SQL-Fehler, nicht auflösbare Brokerage-Zuordnung, ...)
  bleiben als defensiver Fallback erhalten, sollten nach einer erfolgreichen
  Validierung aber nicht mehr auslösen. Ein unerwarteter DB-Fehler an dieser
  Stelle (z. B. ein Festplattenproblem) wird weiterhin pro Datensatz geloggt,
  ohne den gesamten Lauf abzubrechen — das ist ein anderes Problem als eine
  Datenqualitätsfrage in der Quelle und rechtfertigt keinen Komplettabbruch.

### Protokollierung (ImportLogger)

Jede Zeile: Zeitstempel, Aktion (`INSERTED`/`SKIPPED`/`REUSED`/`ERROR`/`INFO`),
Entitätstyp, Quell-ID (WKN/GUID/OrderNumber) und optionales Detail — sowohl auf
der Konsole als auch in der Log-Datei (`--log`, Default
`import_<Zeitstempel>.log`, Append-Modus). Am Ende des Laufs gibt
`writeSummary()` eine Zusammenfassung je Entität/Aktion aus. Schlägt die
Vorab-Validierung fehl, wird stattdessen der strukturierte Validierungsbericht
ausgegeben (siehe oben) — `writeSummary()` läuft in diesem Fall trotzdem noch
(zeigt dann i. d. R. nur Nullen, da nichts geschrieben wurde).

### Bekannte Datenqualitätsprobleme in der Quell-XML

Beim Import realer Depotdaten wurden vier Klassen von Fehlern in der alten
C#-Quelle gefunden. Fall 2 und 3 werden automatisch korrigiert, weil dort eine
eindeutig sichere Korrektur möglich ist. Fall 1 und 4 kann der Importer nicht
selbst reparieren — seit 05.07.2026 verhindern sie zusätzlich den kompletten
Import (siehe "Validierung vor dem Import" oben), statt nur den betroffenen
Datensatz zu überspringen:

1. Falsche `OrderNumber` — ein einzelner Buy trug eine `OrderNumber`, die
   nicht zum zugehörigen PDF-Beleg passte und stattdessen mit einer völlig
   anderen Aktie kollidierte (`UNIQUE constraint failed: buys.order_number`).
   Nur durch Korrektur der `OrderNumber` in der Quelle behebbar (Beleg-Dateiname
   als Referenz).
2. Vertauschte `BuyPart`/`SalePart`-Flags — siehe Abschnitt
   "Brokerage-Zuordnung" oben. Seit dem Fix vom 02.07.2026 fängt der Importer
   das automatisch ab und protokolliert es als `INFO`.
3. Doppelt-XML-escapte Ampersands in WebSite-URLs (`&amp;amp;` statt
   `&amp;`, gefunden bei Nvidia/Wacker Chemie) — siehe Abschnitt
   "URL-Normalisierung" oben. Seit dem Fix vom 05.07.2026 erkennt und
   korrigiert `XmlPortfolioParser` das automatisch und protokolliert es als
   `INFO` (`RawShare::parseWarnings`).
4. Element `<MarketValues>` (Plural) statt `<MarketValue>` (Singular) —
   dieselben zwei Aktien (Nvidia/Wacker Chemie), 2 von 34 Vorkommen laut
   `grep -c`. Anders als Fall 3 ist das ein struktureller Fehler im
   Elementnamen, kein sicher normalisierbares Formatdetail — der Importer
   rät hier nicht, sondern meldet den Fehler als `ERROR`
   (`RawShare::parseErrors`). Nur durch Korrektur des Elementnamens in der
   Quelle + Re-Import behebbar, oder manuelles Nachtragen über
   `ShareEditForm` (nach einem ansonsten erfolgreichen Import der übrigen
   Aktien, sobald diese eine WKN-Kollision nicht mehr betrifft).

Bei jedem neuen Import lohnt sich ein Blick in die Log-Zusammenfassung auf
`ERROR`-Zeilen (Fall 1 und 4, führen zum Komplettabbruch — nur an der Quelle
bzw. manuell behebbar) sowie auf `INFO`-Zeilen mit "widerspricht dem
tatsächlichen Befund" (Fall 2) bzw. "doppelt-XML-escapte Ampersands" (Fall 3)
— Letztere werden automatisch korrigiert, sind aber ein Hinweis auf die
Häufigkeit dieser Datenfehler in der Quelle.

### Tests (tests/xml-importer/)

| Executable | Prüft |
| ------ | ------ |
| `tst_xmlportfolioparser` | Reiner XML → Struct-Parser, ohne DB. Attribut-Mapping für Share/Buy/Sale/Brokerage/Dividend/DailyValues, Fremdwährungs-Dividenden, mehrere Aktien, Fehlerfälle (fehlendes Wurzelelement, Datei nicht gefunden, kaputtes XML). |
| `tst_portfolioimporter` | Integrationstests gegen In-Memory-SQLite (`:memory:`), analog zu `tests/repositories/`. Deckt ab: Share-Neuanlage/-Wiederverwendung per WKN, GUID-basierte Idempotenz bei erneutem Lauf, OrderNumber-Kollision (Fehler geloggt, Import läuft weiter), Dry-Run (keine Schreibzugriffe), Tageswerte-Upsert. **Regressionstests für den Brokerage-Zuordnungs-Fix vom 02.07.2026:** falsches `BuyPart`, falsches `SalePart`, korrekte Flags (Kontrollfall), `GuidBuySale` in keiner Tabelle gefunden. |

Beide folgen dem etablierten Muster: Models/Repositories werden als Quelldateien
direkt mitkompiliert (kein separates Backend-Interface nötig), `initTestCase()`
öffnet `:memory:`, `init()` räumt die relevanten Tabellen vor jedem Test auf.

---

### DocumentsSettingsForm & DocumentRootMigrator (Root-Verzeichnis für Dokumente)

Dialog zum Umstellen aller Dokumentpfade (Kauf, Verkauf, Kosten, Dividende)
von einem alten auf einen neuen Root-Pfad in einem Rutsch — z. B. beim
Wechsel von Windows auf Linux, oder beim Umsortieren des Beleg-Ordners.
Aufrufbar über `Einstellungen → Dokumente...`, analog zu
`BackupSettingsForm`/`LoggerSettingsForm` als einzelner `QDialog` ohne eigenes
IView/IModel/Presenter-Triple.

Entstehung (18.07.2026): Ursprünglich als deutlich komplexerer
Erstlauf-Zwangsdialog mit Auto-Erkennung, Existenzprüfung und mehreren
Sonderfall-Zweigen umgesetzt — auf Nutzer-Feedback ("dieser Schritt ist
einfach zu komplex") bewusst zurückgebaut auf ein einfaches Zwei-Felder-Muster.

Zwei Felder, keine Zwangslogik:
- Alter Root-Pfad (`m_editOldRoot`, frei editierbar, kein Browse-Button):
  vorbefüllt mit `AppSettings::documentsRootPath()`, falls bereits ein Root
  bekannt ist; sonst mit einer automatischen Vorschlag aus
  `DocumentRootMigrator::detectCommonRoot()`. Muss auf diesem Rechner NICHT
  existieren — er dient nur als literaler String-Präfix zum Abgleich gegen
  die gespeicherten Dokumentpfade (genau das ist der Kernanwendungsfall: ein
  alter Windows-Pfad wie `B:\Depot\...` existiert unter Linux naturgemäß
  nicht, soll aber trotzdem als Such-Präfix funktionieren).
- Neuer Root-Pfad (`m_editNewRoot`, nur per "Durchsuchen..."): muss ein
  echtes, existierendes oder anlegbares Verzeichnis auf diesem Rechner sein.

OK ruft `DocumentRootMigrator::changeRoot(alt, neu)` auf (reine
Präfix-Ersetzung über alle `document`-Spalten in `buys`/`sales`/`brokerage`/
`dividends`, via `updateDocument(guid, path)` der jeweiligen Repositories —
keine Datei-Operationen, die Dateien müssen bereits am neuen Ort liegen) und
speichert den neuen Pfad in `AppSettings`. **Abbrechen** macht nichts — kein
DB-Write, keine Settings-Änderung.

Kein Zwang mehr: `MainWindow::ensureDocumentsRootConfigured()` öffnet den
Dialog beim Start, wenn `AppSettings::documentsRootPath()` leer ist — aber
nicht mehr blockierend. Bricht der Benutzer ab, bleibt der Root-Pfad leer und
der Dialog wird beim nächsten Start erneut angeboten. Muss nach dem Öffnen
der Portfolio-DB laufen, damit `detectCommonRoot()` etwas zum Auswerten hat.

Cross-Plattform-Pfaderkennung: `DocumentRootMigrator` normalisiert
Backslashes zu Forward-Slashes und erkennt einen Windows-Laufwerksbuchstaben
(`B:`) als absoluten Pfad-Anfang, unabhängig davon, für welches Betriebssystem
Qt selbst gebaut wurde (`QDir::isAbsolutePath()` würde einen Windows-Pfad
unter Linux fälschlich als relativ einstufen — mit
`QFileInfo(...).absolutePath()` als früherer, verworfener Zwischenstand
wurde ein solcher Pfad sogar stillschweigend gegen das aktuelle
Arbeitsverzeichnis des Prozesses aufgelöst und lieferte einen komplett
falschen "gemeinsamen Ordner", z. B. den `bin/`-Ordner neben `settings.ini`
bei einem aus Qt Creator gestarteten Debug-Build — gemeldet und behoben
18.07.2026). `changeRoot()` selbst prüft nicht, ob der alte Root auf diesem
Rechner existiert — reiner String-Vergleich.

`detectCommonRoot()` liefert ein `DetectionResult` (`suggestedRoot`,
`ambiguous`, `absoluteCount`, `relativeCount`) für die Vorbefüllung des
"Alter Root-Pfad"-Felds; `changeRoot()` liefert ein `Result`
(`rewritten`/`alreadyInRoot`/`outsideRoot`/`updateFailed` plus
`outsidePaths`) für die Zusammenfassung nach dem Umstellen.

### Durchsetzung "nur Dokumente aus dem Root auswählbar" (19.07.2026)

Sobald ein Root-Verzeichnis konfiguriert ist (`AppSettings::documentsRootPath()`
nicht leer), lassen die fünf Dokument-Auswahl-Dialoge — `ViewBuyEdit`,
`ViewSaleEdit`, `ViewDividendEdit`, `ViewBrokerageEdit`, `ViewShareAdd`
(jeweils `onBrowseDocument()`) — nur noch Dateien innerhalb dieses
Verzeichnisses (oder eines Unterordners) zu. Grund: Ohne diese Durchsetzung
könnten Dokumente über die Zeit wieder außerhalb des Root landen, wodurch
ein späterer Root-Wechsel (`DocumentRootMigrator::changeRoot()`) sie nicht
mehr finden und korrekt umschreiben könnte.

Neuer öffentlicher Helfer **`DocumentRootMigrator::isPathWithinRoot(path,
root)`** — rein lesend, reiner String-Vergleich, dieselbe
Backslash-Normalisierung wie der Rest der Klasse (siehe oben,
"Cross-Plattform-Pfaderkennung"). Liefert `true`, solange `root` leer ist
(noch keine Einschränkung konfiguriert).

In jedem der fünf `onBrowseDocument()`:
1. `QFileDialog::getOpenFileName()` startet im Root-Verzeichnis, falls
   gesetzt (vorher: aktueller Dokumentpfad bzw. Home-Verzeichnis).
2. Nach der Auswahl wird `isPathWithinRoot()` geprüft — liegt die Datei
   außerhalb, erscheint `OwnMessageBox::critical()` mit Hinweis auf das
   Root-Verzeichnis, und die Auswahl wird verworfen (Feld bleibt
   unverändert, kein `onDocumentSelected()`-Aufruf an den Presenter).

@note Seit 08.08.2026 gilt dieselbe Root-Prüfung auch in
`ViewShareSplitEdit` — es sind damit sechs Dialoge. `DocumentRootMigrator`
selbst deckt entsprechend fünf Tabellen ab (`buys`, `sales`, `brokerage`,
`dividends`, `share_splits`); die Split-Tabelle wurde sowohl in
`collectAllDocuments()` als auch im Switch von `updateDocument()` ergänzt.
Beides ist nötig — nur den Switch zu erweitern, hätte dazu geführt, dass
Split-Dokumente beim Root-Wechsel gar nicht erst eingesammelt und damit still
übergangen werden. Genau dafür gibt es jetzt einen eigenen Anschlusstest, siehe
TESTING.md.

`ViewBrokerageEdit` erlaubte bislang zusätzlich zu PDF auch Word-/
Excel-Dateien und "Alle Dateien" — auf Nutzer-Entscheidung (19.07.2026)
vorerst auf denselben PDF-only-Filter wie die anderen vier Dialoge
reduziert. Eine Wiedereinführung wurde am 20.07.2026 bewusst verworfen (siehe
"Erledigt / Archiv", "ViewBrokerageEdit: Word-/Excel-Unterstützung nicht
wieder eingebaut — bewusste Entscheidung") — inzwischen wurde zusätzlich in
allen fünf Dialogen der Zusatzfilter `;;Alle Dateien (*)` entfernt, sodass
im `QFileDialog` wirklich nur noch PDF auswählbar ist. Die Root-Einschränkung
selbst gilt unabhängig vom Dateityp, nur der Ordner zählt.

`ViewShareAdd` delegiert seit der Migration auf `DocumentPreviewPanel`
(19.07.2026, siehe "ViewShareAdd auf DocumentPreviewPanel umgestellt" oben)
ebenfalls vollständig an das gemeinsame Vorschau-Panel — die Root-Prüfung
selbst war davon unabhängig und griff schon vorher genauso.

Nicht Teil dieser Änderung: Unit-Tests für die `onBrowseDocument()`-
Methoden selbst (der Fehlerfall ruft `OwnMessageBox::critical()` auf, s.
bekannte Testkonvention) — nur die zugrundeliegende `isPathWithinRoot()`-
Logik ist in `tst_documentssettingsform.cpp` unit-getestet.

### ShareDetailsForm: Dokument-Vorschau per Zeilenauswahl statt Doppelklick (19.07.2026, Nessies Vorgabe)

Der bisherige Doppelklick auf die Dokument-Spalte in den Jahres-Tabs von
"Gewinne/Verluste", "Dividenden" und "Kosten" (`ViewShareDetails`) ist
entfallen. Ein Klick auf eine beliebige Stelle einer Jahres-Tab-Zeile lädt
jetzt sofort das zugehörige Dokument in die eingebettete
`DocumentPreviewPanel`-Instanz — analog zum bereits etablierten
Zeilenauswahl-Verhalten in `ViewBuyEdit`/`ViewSaleEdit`/`ViewDividendEdit`/
`ViewBrokerageEdit` (dort lädt ein Zeilenklick über den Presenter den
kompletten Datensatz inkl. Dokument).

`OverviewTabWidget`: Neues Signal `rowActivatedWithDocument(userData,
documentPath)` — gefeuert bei jedem Klick auf eine Jahres-Tab-Zeile (im
selben `cellClicked`-Handler wie das bestehende `rowActivated()`),
zusätzlich mit dem Dokumentpfad aus der (falls über `jahresDocColumn`
konfigurierten) Dokument-Spalte derselben Zeile — leerer Pfad, wenn keine
Dokument-Spalte existiert oder die Zeile kein Dokument hat. Rein additiv:
das bestehende `documentActivated(path)` (Doppelklick auf die Dokument-
Spalte) bleibt unverändert bestehen, da `ViewBuyEdit`/`ViewSaleEdit`/
`ViewDividendEdit`/`ViewBrokerageEdit` es weiterhin verbinden — das war
beim ersten Anlauf dieser Änderung übersehen worden und hatte kurzzeitig
den Build gebrochen (`documentActivated` versehentlich entfernt), daher
jetzt bewusst als Ergänzung statt als Ersatz umgesetzt.

`ViewShareDetails`: Neue private Hilfsmethode `wireOverviewTab(tabs,
preview, docColumn)`, aufgerufen aus `setupGewinneVerlusteTab()`/
`setupDividendenTab()`/`setupKostenTab()` (docColumn: 4/4/5, siehe die
jeweiligen `populate*()`-Methoden), ersetzt die bisherige einzeilige
`connect(..., documentActivated, ..., showDocument)`-Verdrahtung und
verdrahtet zwei Dinge:
1. `rowActivatedWithDocument` → `preview->showDocument(path)` bei jedem
   Zeilenklick.
2. `currentTabChanged` → beim Wechsel Übersicht → Jahres-Tab wird die erste
   Zeile automatisch selektiert (`tbl->selectRow(0)`) und deren Dokument
   geladen (identisches Verhalten zum automatischen Erst-Zeilen-Laden beim
   Tab-Wechsel in den Editier-Dialogen, dort über den Presenter); beim
   Wechsel zurück zur Übersicht (Index 0) wird die Vorschau geleert
   (`preview->clearDocument()`). Das bereits bestehende
   `onMainTabChanged()` (Reset aller drei Instanzen auf Übersicht bei
   Wechsel des äußeren `m_tabs`) ruft dafür weiterhin nur
   `setCurrentIndex(0)` auf den drei `OverviewTabWidget`-Instanzen auf — der
   neue `currentTabChanged`-Handler in `wireOverviewTab()` übernimmt davon
   ausgehend automatisch das Leeren der Vorschau, ohne dass `onMainTabChanged()`
   selbst etwas von den drei `DocumentPreviewPanel`-Instanzen wissen muss.

Testabdeckung nachgezogen (19.07.2026): `tst_overviewtabwidget.cpp` deckt
`rowActivatedWithDocument()` sowie eine Regression für das weiterhin
bestehende `documentActivated()` mit sieben neuen Tests ab;
`tst_mainwindow.cpp` deckt `wireOverviewTab()` (Erst-Zeilen-Auswahl bei
Tab-Wechsel, Dokumentpfad-Signal bei Zeilenklick, Selektion-Leeren bei
Rückkehr zur Übersicht) mit drei neuen, direkt an
`test_mainTabChanged_resetsOverviewTabsToUebersicht` angehängten Tests ab —
Details siehe TESTING.md, "ShareDetailsForm: Dokument-Vorschau per
Zeilenauswahl (19.07.2026) — Testabdeckung nachgezogen". Bewusst weiterhin
nicht abgedeckt: ob `DocumentPreviewPanel` das Dokument tatsächlich korrekt
anzeigt (kein öffentliches Zustands-API, bekannte Lücke).

### Grid-Selektions-Testplan (Refresh-Flow) vollständig abgeschlossen (erledigt, 20.07.2026)

Der bei der Parser-Mocking-Infrastruktur (07.07.2026) zunächst
zurückgestellte Testplan zur Grid-Selektion während des Refresh-Flows
("Grid-Selektion folgt dem Refresh", Feature vom 05.07.2026) bestand aus
vier Teilaspekten. Bei Durchsicht von `tst_mainwindow.cpp` (20.07.2026)
bestätigt: drei der vier waren bereits über bestehende Tests abgedeckt
(`test_onRefreshAll_gridSelectionFollowsQueueProgress_viaFakeNetwork` für
Selektion während der Queue und Rücksprung auf Zeile 0 nach Abschluss,
`test_onRefreshAll_errorMidQueue_selectionStaysOnFailedShare_viaFakeNetwork`
für den Fehlerfall). Der vierte — Selektion bleibt nach abgeschlossenem
Einzel-Refresh (`onRefreshShare()`, kein "Alle aktualisieren") auf der
aktualisierten Aktie stehen, `selectFirstShareRow()` wird nicht aufgerufen —
fehlte tatsächlich noch und wurde ergänzt:
`test_onRefreshShare_completed_selectionStaysOnUpdatedShare_viaFakeNetwork`
(siehe TESTING.md, Refresh-Flow-Abschnitt). Damit ist dieser Testplan
vollständig abgeschlossen, keine offenen Punkte mehr zur Grid-Selektion.

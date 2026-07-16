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
│   ├── utils/           # Statische Hilfsklassen (ShareCalculator)
│   ├── forms/           # UI-Schicht — je Fenster ein MVP-Triad
│   │   └── UiConstants.h   # Gemeinsame UI-Größenkonstanten (kFieldHeight, kButtonHeight)
│   ├── AppStartup.h/.cpp   # Startup-Helfer (testbar, verwendet in main())
│   ├── IconProvider.h/.cpp # Icon-Set-Verwaltung (wechselbare Icon-Sets)
│   └── resources/       # Icons (PNG), Sounds (WAV), resources.qrc
├── docs/
│   ├── architecture/    # Diese Dokumente
│   └── doxygen/         # Doxygen-Konfiguration + Theme
├── translations/        # .ts-Quelldateien (spm_de.ts, spm_en.ts)
└── tests/
    ├── logger/          # Unit-Tests für Logger
    ├── parser/          # Unit-Tests für Parser
    ├── repositories/    # Unit-Tests für alle Repositories
    ├── database/        # Unit-Tests für Database
    └── forms/           # Unit-Tests für Forms (MainWindow, ShareAddForm, ShareEditForm, BuysForm, SalesForm, DividendForm, BrokeragesForm, OwnMessageBox, BackupProgressForm, ShareDetailsForm)
@endcode

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
tst_dailyvaluesrepository/
tst_database            ← Database, Qt6::Sql
tst_mainwindow          ← alle ShareEditForm-, ShareAddForm-, BuysForm- (Compile-Dep.), SalesForm-, DividendForm-, BrokeragesForm-, OwnMessageBox-, BackupProgressForm-Quelldateien + alle Repositories + ShareCalculator
tst_buysform            ← BuysForm (ModelBuyEdit, PresenterBuyEdit, ViewBuyEdit) + BuyRepository, BrokerageRepository, ShareRepository
tst_shareeditform       ← ViewShareEdit + alle vier Sub-Form-Trios als Compile-Dep. (BuysForm, SalesForm, DividendForm, BrokeragesForm) + alle Repositories
tst_backupsettingsform  ← BackupSettingsForm + AppSettings, IconProvider (kein DB-/MainWindow-Bezug)
tst_sharedetailsform    ← PresenterShareDetails über Fake-View/Fake-Model (IViewShareDetails, IModelShareDetails) — keine DB, keine Qt-Widgets, kein ShareCalculator
tst_chartform           ← PresenterChart über Fake-View/Fake-Model (IViewChart, IModelChart) — keine DB, keine Qt-Widgets, keine Qt-Charts-Instanziierung
@endcode

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
| OwnMessageBox | `forms/OwnMessageBoxForm/` | ✅ implementiert |
| BackupProgressForm | `forms/BackupProgressForm/` | ✅ implementiert |
| BackupSettingsForm | `forms/BackupSettingsForm/` | ✅ implementiert (08.07.2026) |
| ShareDetailsForm | `forms/ShareDetailsForm/` | 🟨 MVP-Struktur steht, Depotwert-/Marktwert-Box und Aktien-Chart-Tab implementiert (12.07.2026) — siehe Detailabschnitt |
| ChartForm | `forms/ChartForm/` | ✅ implementiert (12.07.2026), eingebettet als Tab 1 von ShareDetailsForm — siehe "ChartForm-Details" |

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
- Beim Laden eines bestehenden Verkaufs: Depot des Verkaufs → `loadAvailableBuysForDepot` mit diesem Depot.
- Neuer Verkauf: automatische FIFO-Zuteilung beim Speichern über Depot-gefilterte Käufe.
- Details-Button (neben "Gekaufter Kaufwert"): immer aktiv (Neu-Modus, Edit-Modus, älterer Verkauf).
  Öffnet `onShowDetails()` — einen modalen Read-only-Dialog mit zwei GroupBoxen.

GroupBox "Verwendete Käufe" — Frozen-Footer-Tabelle mit 13 Spalten:
Datum | Anteile | `×` | Kaufkurs | `=` | Kaufsumme | `+` | Kosten | `−` | Rabatt | `=` | Gesamt | Dok.
Operator-Spalten 24 px breit, grau. Datum 100 px fix (identisch zur Verkaufsübersicht).
Dok.-Spalte 36 px fix. Alle Wertspalten gestreckt. Footer-Zeile (fett) mit Summen für
Anteile, Kaufsumme (= totBuyVal), Kosten, Rabatt und Gesamt (= totBuyVal + totFees)
(Dok.-Zelle im Footer leer). Spaltenbreiten per `sectionResized` synchronisiert
(initiale Übertragung per `QTimer::singleShot(0)` nach erstem Layout-Durchlauf).

Spaltenwerte pro Zeile:

| Spalte | Wert |
| ------ | ------ |
| Kaufsumme | `volume × buyPrice` (= `buyValue`) |
| Kosten | anteilige Brokerage des Kaufs (`brokeragePart`) |
| Gesamt | `buyValue + fees - reduction` |

Dok.-Spalte: zeigt das Icon des Kauf-Dokuments (PDF/Word/Excel, identische
Icon-Logik wie Jahres-Tab der Verkaufsübersicht). Kein Dokument → `"-"`.
Tooltip zeigt Dateipfad + "Doppelklick: Dokument anzeigen".
Doppelklick → modaler Vorschau-Dialog (Kind des Details-Dialogs, Titel = Dateiname,
700 × 900 px) mit `QPdfView` (wenn `SPM_HAVE_QTPDF`) oder `pdftoppm`-Fallback —
identische Rendering-Logik wie die rechte PDF-Vorschau im Hauptformular.

Dokument-Pfad-Lookup:

| Modus | Quelle |
| ------ | ------ |
| Edit | `SaleBuyDetail::buyGuid()` → Suche in `m_allBuys` → `BuyObject::document()` |
| Vorschau | `BuyObject::document()` direkt aus `m_availableBuys` |

Im Edit-Modus wird `m_allBuys` verwendet (nicht `m_availableBuys`), da beim ersten
Verkauf ein Kauf vollständig verbraucht sein kann (`volumeSold == volume`) und
damit nicht mehr in `m_availableBuys` erscheint.

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

Zwei Modi des Dialogs:

| Modus | Bedingung | Datenquelle |
| ------ | ------ | ------ |
| Edit | `m_loadedSale.isValid()` | gespeicherte `SaleBuyDetails` aus gecachtem `SaleObject` — exakte DB-Daten |
| Vorschau | `m_loadedSale` ungültig | Live-FIFO-Berechnung aus `m_availableBuys` |

`m_loadedSale` (privates `SaleObject`-Member in `ViewSaleEdit`) wird in `loadSale()` gesetzt
und in `clearForm()` auf einen leeren Standardwert zurückgesetzt.

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
- **Überschreibt einen bereits vorhandenen Wert** bei jeder Datumsänderung mit Treffer —
  auch einen zuvor manuell eingegebenen. Der Nutzer kann den Wert danach jederzeit
  wieder von Hand korrigieren (kein Sperren des Felds).
- **Kein Treffer → keine Änderung.**
- **Pfad ② (Preis-Abgleich direkt nach dem Parsen) ist notwendig, nicht nur bequem.**
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
- **Tooltip als Unterscheidungsmerkmal:** `IViewDividendEdit::setFieldOk()` erhält einen
  optionalen dritten Parameter `tooltip`, damit automatisch befüllte Werte
  ("Aus Tageswerten übernommen (Kurs vom ...)") sich vom Standard-Tooltip
  ("Eingabe gültig") bei manueller Eingabe unterscheiden lassen. Leerer String → Standard-Tooltip.
- **Keine Währungsumrechnung nötig:** `daily_values.closing` liegt ebenso wie `rate` immer
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

> Hinweis (Stand 08.07.2026): Das frühere `PriceAtPayday`-Mapping wurde entfernt
> — kein `Document Type="Dividend"` in `Documents.xml` definierte je ein
> `PriceAtPayday`-Feld, der Eintrag war totes Gewicht. Details siehe
> **"Totes Mapping: `PriceAtPayday` in `xmlNameToViewField()`"** unter "Offene Punkte / TODO".
> `knownXmlNames` in `populateFromResult()` enthält den Namen weiterhin (bewusst
> unangetastet, siehe dort).

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

**Wichtiger Hinweis zum Umfang dieser Iteration:** Ein früherer Anlauf hatte
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

**Umfang dieser Iteration (auf Nessies Vorgabe eingegrenzt):**

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

**Wichtig:** Für die Marktwert-Box `Gesamt-Bestandsberechnung` werden
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
sollen statt eigene Datenlisten zu laden.

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

**"Start-Datum" ist das Ende des Zeitraums, nicht der Anfang** — matcht die
C#-Referenz exakt (Screenshot 12.07.2026: Start-Datum=10.7.2026,
Interval=Month, Anzahl=1 → Zeitraum 10.06.2026–10.07.2026). Der Anfang wird
in `PresenterChart::computeRangeStart()` rückwärts aus
Start-Datum − (Anzahl × Interval-Einheit) berechnet (Tag/Woche/Monat/Jahr).
Anders als der Name vermuten lässt, steuern Interval/Anzahl also nur die
Größe des angezeigten Fensters, keine Aggregation der Datenpunkte — geplottet
werden immer die rohen Tageswerte aus `daily_values` im berechneten Fenster.

**Sechs Selektions-Checkboxen** (`SeriesKind`: `ClosingPrice`/`OpeningPrice`/
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

**"Legende"-Box statt Qt-Charts-eigener Legende:** `m_chart->legend()->hide()`
— die rechte Box wird stattdessen manuell aus `PresenterChart`-formatierten
`LegendEntry`-Zeilen aufgebaut (Farbquadrat + fett Titel + Min/Max-Zeile),
da sie mehr zeigen muss als reine Serien-Namen: "Letzter Kauf:"/"Letzter
Verkauf:" (aus `ModelChart::latestBuy()`/`latestSale()` — jeweils der letzte
Eintrag der nach Datum aufsteigend sortierten `BuyRepository`/
`SaleRepository`-Listen) mit der Entwicklung relativ zum höchsten Schluss-Kurs
im aktuell angezeigten Zeitraum. Diese beiden Referenzzeilen erscheinen nur,
wenn für die Aktie tatsächlich Käufe/Verkäufe existieren.

**Fenstertitel:** `PresenterChart::refresh()` baut die Zeile "Zeitraum: ... -
... / Entwicklung: X€ (Y %)" (erster/letzter Schluss-Kurs im Fenster) und
gibt sie über `IViewChart::setRangeInfo()` an die View. `ViewChart` emittiert
das unverändert als eigenes Qt-Signal `titleInfoChanged`, das
`ViewShareDetails::onChartTitleInfoChanged()` mit dem gespeicherten
Aktiennamen zum vollständigen C#-Referenz-Fenstertitel kombiniert. Gibt es
für die Aktie gar keine Tageswerte, bleibt `infoText` leer und der Titel
fällt auf den reinen Aktiennamen zurück — hält
`test_shareDetailsDialog_validShare_constructsAndShowsCloseButtonText`
unverändert grün.

**Qt6 QComboBox-Interaktionssignal:** Die Interval-ComboBox verbindet
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

**Bugfix (12.07.2026):** Die Serienfarbe (`ChartSeriesData::color`, in
`PresenterChart` vergeben — Schluss-Kurs Schwarz, Kauf/Verkauf-Referenzlinien
Blau/Rot usw.) wurde bisher **vor** `m_chart->addSeries(line)` gesetzt.
`QChart` wendet sein Theme aber beim Hinzufügen einer Serie an und
überschreibt dabei eine vorher gesetzte Farbe wieder — Schluss-Kurs erschien
dadurch blau (erste Theme-Farbe) statt schwarz, obwohl die "Legende"-Box
(die unabhängig vom Chart-Theme aus `PresenterChart`-Daten gerendert wird)
korrekt Schwarz zeigte. Nessie ist das an der Diskrepanz Legende ↔ Graph
aufgefallen. Behoben durch `line->setColor(s.color)` **nach**
`m_chart->addSeries(line)` in `ViewChart::setChartData()`.

**Hover-Tooltip (ergänzt 12.07.2026):** Portiert vom C#-Referenz-Verhalten
("Maus über den Graphen bewegen zeigt Datum + Wert als Tooltip"). Jede
`QLineSeries` wird in `ViewChart::setChartData()` einzeln mit
`onSeriesHovered()` verbunden (`QLineSeries::hovered(QPointF, bool)`) — Qt
Charts hat kein chart-weites Hover-Signal, das zusätzlich verrät, welche
Serie getroffen wurde, daher ein Connect pro Serie statt eines gemeinsamen.
`point.x()` ist `msecsSinceEpoch` (gleiche Kodierung wie beim Befüllen der
Serie), `point.y()` der Wert am nächstgelegenen Datenpunkt — Qt Charts liefert
hier immer den nächsten tatsächlichen Datenpunkt, keine interpolierte
Mausposition. Preis-Serien werden mit "€" und 2 Nachkommastellen formatiert,
die beiden Stück-Serien (`HeldVolume`/`TradedVolume`) ohne Nachkommastellen —
`QToolTip::showText(QCursor::pos(), ...)` bei `state == true`,
`QToolTip::hideText()` bei `state == false` (Maus verlässt die Linie).

**Vertikale Kauf-/Verkauf-Markerlinien (ergänzt 12.07.2026):** Portiert vom
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

**Hover-Tooltip für die Markerlinien (ergänzt 12.07.2026, zweiter Anlauf):**
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

**Bewusste Vereinfachungen dieser ersten Iteration** (auf Wunsch bei
Bedarf später verfeinerbar):
- Zahlenformatierung in der Legende durchgängig mit 2 (Preis/Anteile) bzw.
  0 Nachkommastellen (Anteile-Min/Max), nicht exakt wie im C#-Screenshot
  (dort teils 1 Nachkommastelle).
- Legende-Titeltext für "Letzter Kauf"/"Letzter Verkauf" ist in der
  Swatch-Farbe (Blau/Rot) statt wie im Referenz-Screenshot grün.
- Keine eigenen Legende-Einträge für "ältere Käufe"/"ältere Verkäufe"
  (Türkis/Orange) — nur die Linien selbst im Chart, die Legende zeigt
  weiterhin nur den jeweils letzten Kauf/Verkauf.

**Tests (`tst_chartform`):** Fake-View/Fake-Model-Paar (analog
`tst_sharedetailsform`) — kein `QWidget`, keine Qt-Charts-Instanziierung,
keine Datenbank. Deckt ab: leerer/gefüllter Initialzustand, Default-Selektion
(nur Schluss-Kurs), Zeitraum-Berechnung für Tag-/Monat-Intervall,
Anteile-Serie inkl. eigener Achse, gehandelte Anteile (Börsenvolumen)
inkl. eigener dritter Achse (unterschiedlich von Anteile), "keine Serie ausgewählt"-Leerzustand,
Min/Max- sowie Letzter-Kauf/Verkauf-Legendenzeilen (inkl. Fehlen bei
fehlenden Käufen/Verkäufen), und dass `onControlsChanged()` vor dem ersten
`loadAndDisplay()` keinen Effekt hat.

**Mausrad-Steuerung der "Anzahl" (ergänzt 12.07.2026 auf Nessies Vorgabe,
portiert vom C#-Referenz-Verhalten):** In der C#-Referenz lässt sich der
Zeitraum sowohl über das "Anzahl"-Feld selbst als auch per Mausrad direkt im
Chart ändern. Beide Wege sind über `ViewChart::eventFilter()` auf dieselbe
Logik zurückgeführt:

- **`m_countSpin`:** `QAbstractSpinBox::wheelEvent()` ignoriert Mausrad-
  Events ohne Fokus — der Event-Filter fängt sie stattdessen direkt ab, damit
  Scrollen auch ohne vorherigen Klick funktioniert.
- **`m_chartView->viewport()`, nicht `m_chartView` selbst:** `QChartView`
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

**Obergrenze für "Anzahl" (ergänzt 12.07.2026 auf Nessies Vorgabe):** Ohne
Begrenzung ließ sich "Anzahl" beliebig weit über den Punkt hinaus erhöhen, an
dem der älteste vorhandene Tageswert bereits im Fenster lag — jede weitere
Vergrößerung zeigte exakt dieselben Daten, ohne dass das für den Nutzer
erkennbar war. Die Begrenzung sitzt komplett im `PresenterChart` (Business-
Logik), nicht im `ViewChart` — reine MVP-Trennung, die View bleibt dumm.

- **`DailyValuesRepository::earliestDate()`** (Gegenstück zu `latestDate()`,
  `MIN(date)`) → `IModelChart::earliestDailyValueDate()` →
  `ModelChart::earliestDailyValueDate()` reichen das durch.
- **`PresenterChart::computeMaxIntervalCount(rangeEnd, unit, earliestDate)`**
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
- **`IViewChart::setMaxIntervalCount(int)`** reicht das Ergebnis an die View
  durch. `ViewChart::setMaxIntervalCount()` ruft `m_countSpin->setMaximum()`
  auf (Signal geblockt — verhindert einen rekursiven `onControlsChanged()`-
  Aufruf, falls der aktuelle Wert dabei automatisch heruntergeklemmt wird).
  Das wirkt automatisch auf **alle** Eingabewege — Pfeiltasten, Tippen und
  die oben beschriebene Mausrad-Steuerung —, da Qt `QSpinBox` intern immer
  an `maximum()` clamped.
- **Zusätzlich presenter-seitig geklemmt:** `refresh()` begrenzt die
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

- **Farben** sind theme-abhängig aus der Palette: `neutral` =
  `palette().color(QPalette::Text)`, `muted` = `neutral` mit Alpha 140
  (nur für leere Platzhalterwerte in Footer-Einzelwertzeilen genutzt — alle
  sichtbaren Zweitzeilen in Haupttabellen und Footer nutzen `neutral` bzw.
  `perfColor(...)`, damit Ober- und Unterzeile optisch gleich dargestellt
  werden; Bugfix 03.07.2026 — zuvor nutzten "Kosten/Dividenden" und "Preis"
  fälschlich `muted` für die Unterzeile, wodurch sie optisch wie eine andere
  Schrift wirkte). Gewinn/Verlust nutzen dieselbe Quelle wie die
  Statusmeldungsbox — `AppSettings::logColorAt(5)` (Erfolg-Grün) bzw.
  `logColorAt(3)` (Fehler-Rot); ein Nullwert wird in Textfarbe gezeichnet.
- **Icons**: `setIconSize(24×24)`; die Entwicklungs-Pfeile liegen als 24-px-PNGs
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
- **Zeilen**: Haupttabellen 38 px, Footer 34 px; alternierende Zeilenfarben und
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

#### CenterIconDelegate (forms/MainForm/CenterIconDelegate.h)

Header-only `QStyledItemDelegate` ohne `Q_OBJECT`. Überschreibt nur
`initStyleOption` und setzt `option->decorationAlignment = Qt::AlignCenter`. Damit
werden die Icons in den reinen Icon-Spalten (Icon/Status, PrevDayChart, CompleteChart)
zentriert statt linksbündig dargestellt — angewandt auf beide Haupttabellen und beide
Footer.

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

**Bugfix 07.07.2026 — Lücke im Busy-Guard:** `startRefreshForShare()` ruft
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

1. `state.dailyValuesList` nach `QList<DailyValuesObject>` konvertieren
   (GUID aus `m_refreshShare.guid()`).
2. `DailyValuesRepository::upsertList(objects, &stats)` — Transaktion; jeder
   Datensatz wird gegen den bestehenden DB-Eintrag verglichen
   (`findByShareAndDate()` + `valuesEqual()`, Toleranz `1e-9`). Neue/geänderte
   Zeilen werden per `INSERT OR REPLACE` geschrieben, unveränderte Zeilen
   übersprungen (kein DB-Write). Rückgabe der Zähler in
   `DailyValuesRepository::UpsertStats` (`fetched`/`inserted`/`updated`/`unchanged`).
3. Statusmeldung: `"Tageswerte aktualisiert: <Name> — <N> Einträge geholt
   (Eingefügt: X / Aktualisiert: Y / Unverändert: Z)"`.
4. `m_dailyDone = true` → wenn auch `m_marketDone`, `onRefreshShareFinished()` aufrufen.

Bei Fehler: analog zu `onMarketValuesUpdated()` — `m_errorOccurred = true`,
`m_dailyDone = true`, MarketValues-Parser läuft unabhängig weiter.

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
   erst am Ende von "Alle aktualisieren".
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
`setFirstBuyDate()`, `setCurrentVolume()`,
`setTotalBuys/Sales/ProfitLoss/Dividends/Brokerages()`, `showError()`, `acceptAndClose()`.

`IModelShareEdit` — `loadShare()`, `saveShare()`, alle Aggregate, `currentVolume()`, `firstBuyDate()`.

`ModelShareEdit` — Delegiert an alle fünf Repositories.

`PresenterShareEdit` — Lädt Share + Aggregate im Konstruktor. Emittiert
`openBuysRequested`, `openSalesRequested`, `openDividendsRequested`, `openBrokeragesRequested`.
Slot `refreshSummary()` → `populateSummary()`.

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
  └── daily_values      ← Historische Kursdaten (OHLCV)
@endcode

Alle Tabellen verwenden `TEXT`-GUIDs als Primärschlüssel. Foreign Keys aktiviert
(`PRAGMA foreign_keys = ON`), WAL-Modus aktiv (`PRAGMA journal_mode = WAL`).

---

## Repository-Schicht

Alle 6 Repositories sind implementiert: `ShareRepository`, `BuyRepository`,
`SaleRepository`, `DividendRepository`, `BrokerageRepository`, `DailyValuesRepository`.

Verbindungsregel: Immer `QSqlDatabase::database(Database::connectionName())` —
niemals `QSqlDatabase::database()` ohne Argument (gibt ungültige Default-Verbindung).

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
- **Übersicht-Tab nicht mehr anwählbar:** `m_pinnedBar` hat nur genau einen
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
- **Spaltenköpfe erst bei Selektion fett:** `buildFrozenTable()` setzte
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

---

## Offene Punkte / TODO

### Dokument-Spalten: Breite verkleinern + Header "Dokument" → "Dok." (offen, 16.07.2026)

Nach dem Breiten-Fix in `ViewSaleEdit` (siehe "SalesForm auf
OverviewTabWidget/DocumentPreviewPanel umgestellt" oben, `kDocColWidth = 120`)
kam Nessies Feedback: die Dokument-Spalte verbraucht — trotz des Fixes — über
alle betroffenen Formulare hinweg weiterhin zu viel Platz für eine reine
Icon-Spalte. Betrifft mindestens:

- `ViewBuyEdit` (Kauf-Übersicht, aktuell Stretch/`-1`)
- `ViewSaleEdit` (Verkaufs-Übersicht, aktuell fix `kDocColWidth = 120`)
- `ViewDividendEdit` (seit 16.07.2026 auf `OverviewTabWidget` migriert,
  Dokument-Spalte bewusst als Zwischenstand auf Stretch/`-1` gesetzt —
  siehe Migrationsnotiz oben)
- `ViewBrokerageEdit` (seit 16.07.2026 auf `OverviewTabWidget` migriert,
  Dokument-Spalte fest auf `kDocColWidth = 120` gesetzt — siehe
  Migrationsnotiz oben)
- `ViewShareEdit` (Übersichts-/Summenanzeige der Pencil-Button-Dialoge —
  gegenchecken, ob dort ebenfalls eine Dokument-Spalte existiert)
- `OverviewTabWidget`-Details-Tabs in `ViewShareDetails` (Gewinne/Verluste,
  Dividenden, Kosten)
- Die "Verwendete Käufe"-Tabelle im SalesForm-Details-Dialog
  (`onShowDetails()`, `kColDoc` dort separat, aktuell fix `36`)

**Reihenfolge (Nessies Vorgabe, 16.07.2026):** Nicht jetzt schon global
vereinheitlichen. Erst `DividendForm` und `BrokeragesForm` vollständig auf
`OverviewTabWidget`/`DocumentPreviewPanel` umbauen — beide seit 16.07.2026
abgeschlossen (siehe Migrationsnotizen oben). **Danach** in einem eigenen
Schritt für **alle** Formulare (`ViewBuyEdit`, `ViewSaleEdit`,
`ViewDividendEdit`, `ViewBrokerageEdit`, `ViewShareEdit`) einheitlich
umsetzen:

1. Dokument-Spalte fest auf `36`px (kein Stretch) — reine Icon-Spalte ohne
   Textinhalt.
2. Keine Spaltenüberschrift für die Dokument-Spalte (leerer String statt
   "Dokument"/"Dok." im jeweiligen `jahresHeaders`/`uebersichtHeaders`-Array).

### Brokerage-Vorwärts-Link: ModelBuyEdit/ModelBrokerageEdit ungeprüft (offen, 15.07.2026)

Der am 15.07.2026 in `ModelSaleEdit::addSale()`/`updateSale()` gefundene und
behobene Bug (fehlender Vorwärts-Link `sales.brokerage_guid` beim Anlegen
eines neuen Brokerage-Eintrags — siehe "SalesForm-Details"/`ModelSaleEdit`
oben) wurde **nur für den Verkaufs-Pfad** untersucht und gefixt, da genau
dort ein Test ihn aufgedeckt hat. Ob derselbe Fehler auch in `ModelBuyEdit`
(Käufe, analoge `buys.brokerage_guid`-Verknüpfung) oder einem eventuellen
`ModelBrokerageEdit` (direkte Kosten-Bearbeitung) steckt, ist **nicht**
gegengeprüft — beim Kauf-Pfad deutet der bisher gesehene Code darauf hin,
dass die Brokerage-GUID dort schon vor dem Insert feststeht und direkt
mitgegeben wird (kein nachträgliches Verlinken nötig), das ist aber eine
Vermutung aus früheren Testschnipseln, kein echter Check. Vor dem nächsten
gezielten Blick auf Käufe/Kosten: `BuyRepository`/`ModelBuyEdit` (und ggf.
`ModelBrokerageEdit`, falls vorhanden) auf dasselbe Vorwärts-/Rückwärts-Link-
Muster prüfen, analog zu `SaleRepository::updateBrokerageGuid()` und den
zugehörigen Tests (`tst_salerepository.cpp`, `tst_mainwindow.cpp`).

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
Test-Target abgesichert (siehe TESTING.md, Abschnitt `tst_sharedetailsform`)
— das ist eine bewusste, unveränderte Lücke aus früheren Sessions, keine neue.

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

### DocumentPreviewPanel: Existenzprüfung fehlt noch in den Editier-Dialogen (offen, 13.07.2026)

`DocumentPreviewPanel::showDocument()` (neues, wiederverwendbares Vorschau-
Panel, u. a. verwendet in `ShareDetailsForm`) prüft jetzt per
`QFileInfo::exists()`, ob die Datei noch existiert, und zeigt andernfalls
eine Fehlermeldung statt stillschweigend fehlzuschlagen. Dieselbe Prüfung
fehlt noch in den Editier-Dialogen `ViewBuyEdit`, `ViewSaleEdit`,
`ViewDividendEdit`, `ViewBrokerageEdit` und `ViewShareAdd` (jeweils
`openPdfPreview()`) und sollte dort nachgezogen werden — idealerweise im
Zuge der Umstellung dieser Dialoge auf `DocumentPreviewPanel`/
`OverviewTabWidget`.

### Spalten-Breiten-Schema für Dokument-Spalten auch in ShareEdit-Grids nachziehen (offen, 14.07.2026)

Für die Jahres-Tabs von `OverviewTabWidget` (Gewinne/Verluste-, Dividenden-,
Kosten-Tab in `ShareDetailsForm`) hat sich nach mehreren Anläufen folgendes
Spaltenbreiten-Schema bewährt: erste Spalte (Datum) fest, letzte Spalte
(Dokument) ebenfalls fest und ausreichend breit (110px — reicht für
Spaltenkopf-Text "Dokument" plus Icon, ohne abgeschnitten zu werden), alle
Spalten dazwischen als Stretch (`-1`), sodass sie sich den verbleibenden
Platz automatisch teilen. Vorherige Versuche mit einer zu schmalen festen
Dokument-Spalte (36px, aus den Editier-Dialogen übernommen) oder mit
durchgehend gestreckten Spalten führten je nach verfügbarer Breite zu
abgeschnittenem oder überlappendem Spaltenkopf-Text.

Dasselbe Schema sollte auf die entsprechenden Grids in `ViewShareEdit` (und
ggf. weiteren Dialogen mit vergleichbaren Dokument-Spalten) übertragen
werden — dort ist bislang nicht geprüft, ob dieselbe Problematik besteht.

### Totes Mapping: `PriceAtPayday` in `xmlNameToViewField()` (entfernt 08.07.2026)

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

### Folgepunkt: `PriceAtPayday` auch aus `knownXmlNames` entfernt (erledigt 08.07.2026)

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

- **`backupEnabled()`**: ist Backup deaktiviert, kehrt die Methode sofort
  zurück (kein Log-Eintrag als Statusmeldung, nur `qInfo()` — analog dazu,
  wie bisher schon eine fehlende Portfolio-Datei still übersprungen wurde).
- **`backupDirectory()`**: leer → wie bisher `fi.absolutePath()` der
  Portfolio-Datei. Ist ein eigenes Verzeichnis konfiguriert und existiert es
  noch nicht, wird es per `QDir::mkpath()` angelegt; schlägt das fehl, wird
  eine Warn-Statusmeldung ausgegeben und kein Backup erstellt.
- **`backupNamePrefix()`** / **`backupDateFormat()`**: ersetzen die bisher
  fest codierten Literale `"Backup"` bzw. `"yyyy_MM_dd_HH_mm_ss"` beim
  Erzeugen des neuen Dateinamens.
- **`backupMaxCount()`**: ersetzt das bisherige `constexpr int kMaxBackups = 5`;
  über `qMax(1, ...)` gegen einen Wert ≤ 0 abgesichert (z. B. falls die INI
  von Hand manipuliert wurde).

**Rotation: Namensfilter präfix-unabhängig, Sortierung nach Änderungsdatum
(Nachtrag 08.07.2026):** Auf Nutzer-Rückfrage geprüft — "funktioniert die
Rotation noch, wenn Präfix oder Datumsformat geändert werden?" — und dabei
zwei Robustheitslücken behoben:

- **Namensfilter ohne Präfix:** Die Rotation filtert nach
  `*_<Portfolioname>_*.<Endung>`, nicht nach
  `<Präfix>_<Portfolioname>_*.<Endung>`. Mit einem präfixgebundenen Filter
  würde eine Präfix-Änderung in `BackupSettingsForm` alle bisherigen Backups
  aus der Zählung herausfallen lassen — "Max. Anzahl Backups" gälte dann
  faktisch nur noch pro Präfix statt insgesamt, und alte Backups blieben nach
  einer Präfix-Änderung für immer liegen, weil sie den neuen Filter nicht
  mehr treffen. Der Portfolioname (Basisdateiname) plus Endung reicht als
  Anker aus, um Backups dieses Portfolios von fremden Dateien im selben
  Verzeichnis zu unterscheiden.
- **Sortierung nach `QFileInfo::lastModified()`, nicht nach Dateiname:** Eine
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
- **Leerer/fehlerhafter Präfix bzw. leeres Datumsformat:**
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

### `onDailyValuesUpdated()`-Pfad (erledigt 08.07.2026)

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

- **Shares** werden über die WKN abgeglichen (`ShareRepository::findByWkn`).
  Existiert die Aktie bereits, wird ihre GUID wiederverwendet und die
  Stammdaten bleiben unangetastet — es werden nur fehlende Kindobjekte importiert.
- **Buys/Sales/Dividends/Brokerages** übernehmen die GUID direkt aus dem
  Quell-XML. Vor dem Insert prüft der Importer per `findByGuid()`, ob der
  Datensatz schon existiert, und überspringt ihn dann (`SKIPPED`). Ein erneuter
  Lauf über dieselbe (oder eine aktualisierte) Export-Datei ist damit sicher —
  `PortfolioValidator` behandelt einen solchen Re-Import derselben GUID
  ausdrücklich nicht als `OrderNumber`-Kollision (siehe oben).
- **Daily values** verwenden `INSERT OR REPLACE` über den Composite-Key
  `(share_guid, date)` und sind dadurch immer gefahrlos erneut importierbar.

### Fehlerverhalten

Seit der Einführung von `PortfolioValidator` (siehe oben) ist das
Fehlerverhalten zweigeteilt:

- **Vor dem Import:** Jedes gefundene Problem — egal in welcher Aktie —
  verhindert den kompletten Lauf. Es gibt keine Teilimporte mehr.
- **Während des Imports:** Die bereits bestehenden Datensatz-Ebene-Prüfungen
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

1. **Falsche `OrderNumber`** — ein einzelner Buy trug eine `OrderNumber`, die
   nicht zum zugehörigen PDF-Beleg passte und stattdessen mit einer völlig
   anderen Aktie kollidierte (`UNIQUE constraint failed: buys.order_number`).
   Nur durch Korrektur der `OrderNumber` in der Quelle behebbar (Beleg-Dateiname
   als Referenz).
2. **Vertauschte `BuyPart`/`SalePart`-Flags** — siehe Abschnitt
   "Brokerage-Zuordnung" oben. Seit dem Fix vom 02.07.2026 fängt der Importer
   das automatisch ab und protokolliert es als `INFO`.
3. **Doppelt-XML-escapte Ampersands in WebSite-URLs** (`&amp;amp;` statt
   `&amp;`, gefunden bei Nvidia/Wacker Chemie) — siehe Abschnitt
   "URL-Normalisierung" oben. Seit dem Fix vom 05.07.2026 erkennt und
   korrigiert `XmlPortfolioParser` das automatisch und protokolliert es als
   `INFO` (`RawShare::parseWarnings`).
4. **Element `<MarketValues>` (Plural) statt `<MarketValue>` (Singular)** —
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

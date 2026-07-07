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
    └── forms/           # Unit-Tests für Forms (MainWindow, ShareAddForm, ShareEditForm, BuysForm, SalesForm, DividendForm, BrokeragesForm, OwnMessageBox, BackupProgressForm)
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
| ShareDetailsForm | `forms/ShareDetailsForm/` | ⬜ Code vorhanden, aber nicht MVP-konform und nicht integriert |
| ChartForm | `forms/ChartForm/` | ⬜ Dateien vorhanden, leer |

@note Zu ShareDetailsForm: `ShareDetailsForm.h/.cpp` enthalten bereits einen
funktionsfähigen Entwurf (Header mit Logo/Name/Kurs, Tabs für Stammdaten/Käufe/
Verkäufe/Dividenden/Brokerages), allerdings als einzelne `QDialog`-Klasse ohne
Trennung in `IView`/`IModel`/Presenter. Die Klasse ist in `app/CMakeLists.txt` als
Build-Quelle eingetragen, wird aber von keiner View aufgerufen (kein Verweis aus
`MainWindow` oder einer Detail-Ansicht) und hat keine Unit-Tests. Vor einer
Aktivierung muss der Dialog auf das MVP-Pattern umgestellt und an einen Aufrufpunkt
(z. B. Doppelklick auf eine Portfolio-Zeile) angebunden werden.

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
| `PriceAtPayday` | `priceAtPayday` | — (totes Mapping, siehe Hinweis unten) |
| `ExchangeRate` | `exchangeRatio` | — |
| `Currency` | `currency` | — |

> Hinweis: Kein `Document Type="Dividend"` in `Documents.xml` definiert aktuell ein
> `PriceAtPayday`-Feld — das Mapping wird nie ausgelöst. Details siehe
> **"Totes Mapping: `PriceAtPayday` in `xmlNameToViewField()`"** unter "Offene Punkte / TODO".

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

## Offene Punkte / TODO

### Totes Mapping: `PriceAtPayday` in `xmlNameToViewField()` (geklärt 07.07.2026)

`PresenterDividendEdit::xmlNameToViewField()` enthält ein Mapping
`"PriceAtPayday" → "priceAtPayday"`. Geprüft und geklärt: **Keine** Bank-Konfiguration
in `app/config/Documents.xml` definiert für `Document Type="Dividend"` ein
`PriceAtPayday`-Feld — das Mapping ist totes Gewicht ohne Laufzeitauswirkung, kein
Widerspruch zur Doku ("automatisch aus `daily_values` befüllt, sonst manuell", siehe
DividendForm-Details → Auto-Fill). Optionaler Cleanup für später: Zeile aus der Map
entfernen, um Verwirrung bei künftiger Fehlersuche zu vermeiden — keine funktionale
Dringlichkeit.

### BackupSettingsForm (geplant, noch nicht implementiert)

Ein dedizierter Konfigurationsdialog für Backup-Einstellungen soll künftig folgende
Optionen bieten:

| Einstellung | Beschreibung | Standardwert |
| ------ | ------ | ------ |
| Backup aktivieren | Backup beim Öffnen ein-/ausschalten | ✅ aktiv |
| Max. Anzahl Backups | Wie viele Backups vorgehalten werden | 5 |
| Namensschema | Präfix und Datumsformat des Backup-Dateinamens | `Backup_<Name>_YYYY_MM_DD_HH_mm_ss` |
| Backup-Verzeichnis | Zielverzeichnis (Standard: gleicher Ordner wie Portfolio) | Portfolio-Verzeichnis |

Die Einstellungen werden in `AppSettings` (INI) gespeichert und von `createBackup()`
ausgelesen. `BackupSettingsForm` folgt dem MVP-Pattern analog zu `LoggerSettingsForm`.

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
Footer-Update während `onRefreshAll()`, Grid-Selektion während der
"Alle aktualisieren"-Queue, der zugehörige Fehlerfall sowie
`buildDailyValuesUrl()` als reine Berechnungsfunktion sind weiterhin offen
(siehe TESTING.md für die vollständige Liste).

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

Bewusst **nicht** abgedeckt: Parsbarkeit numerischer Felder außer Daten (z. B.
`SharePrice`, `Volume`, `Provision` fallen bei ungültiger Eingabe weiterhin
lautlos auf `0.0` zurück, siehe `PortfolioImporter::toDouble()`) — eine
mögliche Erweiterung für eine künftige Sitzung.

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

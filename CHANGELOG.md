# Changelog

Alle nennenswerten Änderungen an diesem Projekt werden in dieser Datei
dokumentiert.

Format angelehnt an [Keep a Changelog](https://keepachangelog.com/de/1.0.0/),
Versionierung nach [SemVer](https://semver.org/lang/de/).

## [Unreleased]

Zurzeit keine unveroeffentlichten Aenderungen.

## [1.19.3] - 2026-08-27

### Changed

- **Analyse-Statuszeile zaehlt uebernommene Werte statt gefangener** — nach dem
  Einlesen eines Belegs konnte "Analyse OK - 5/5 Pflicht" dastehen, waehrend
  daneben ein Pflichtfeld ein rotes Fehlersymbol trug. Die Zeile zaehlte, was
  der Parser aus dem Beleg geholt hatte, das Symbol zeigte, was die Maske mit
  dem Rohwert anfangen konnte. `setFieldOk()` meldet das Ergebnis der
  Uebernahme jetzt zurueck, und die Statuszeile zaehlt nur noch, was
  tatsaechlich in der Maske gelandet ist - fuer Pflicht- wie Optionalfelder.

- **Rohwert im Tooltip des Fehlersymbols** — die Statuszeile unterscheidet
  dadurch nicht mehr zwischen "Regel hat nicht gegriffen" und "Regel hat etwas
  Unbrauchbares gefangen". Diese Auskunft steht jetzt dort, wo hingeschaut
  wird: `setFieldError()` nimmt den verworfenen Rohwert entgegen und zeigt ihn
  am Symbol an (`Nicht verwertbar: "Schlusstag 04/02"`).

- **Einheitliche Bauweise in allen vier Views** — `ViewShareAdd` setzte den
  Feldzustand bereits am Schluss ueber einen Merker; die drei Editier-Dialoge
  setzten das gruene Symbol zuerst und ueberschrieben es bei Bedarf. Alle vier
  folgen jetzt demselben Muster.

- `populateFromResult()` ist in allen vier Presentern `public` statt `private`,
  damit die geaenderte Zaehlung ueberhaupt testbar ist.

- **Angepasste Tests.** Drei bestehende Testfaelle hielten das alte Verhalten
  fest und wurden umgeschrieben:
  `test_viewDividendEdit_setFieldOk_depotNumber_selectsUnknownValue` prueft als
  `..._unknownIsRejected` jetzt das Gegenteil;
  `test_viewBuyEdit_setFieldOk_unparsableDate_marksFieldAsError` zaehlte den
  allgemeinen Tooltip-Text, den es bei einem verworfenen Rohwert nicht mehr
  gibt; die beiden `..._hasMissingRequiredFields_falseAfterAllSet` setzten eine
  Depotnummer, die in der leeren Auswahlliste ihres Dialogs nicht vorkam.
  Dazu drei neue Faelle fuer die neue Zusage (abgewiesene Depotnummer,
  blockiertes Speichern, Rohwert im Tooltip) und je zwei Zaehl-Tests pro
  Formular.

### Fixed

- **Unbekannte Depotnummer wird nicht mehr stillschweigend hingenommen.** Eine
  Depotnummer aus dem Beleg, die in `Documents.xml` nicht hinterlegt ist,
  wurde in keiner der vier Masken gemeldet: "Aktie hinzufuegen" und das
  Kauf-Formular liessen die Auswahl still auf dem Platzhalter stehen - mit
  gruenem Haken, und das Speichern scheiterte spaeter mit "Depotnummer fehlt".
  Verkaufs- und Dividenden-Formular fuegten den unbekannten Wert der Liste
  hinzu, womit eine nirgends konfigurierte Depotnummer in der Datenbank landen
  konnte.

  Alle vier melden jetzt einen Fehler und blockieren das Speichern. Die
  Zuordnung wird fuer die Bestandspruefung pro Depot gebraucht: die
  Stueckzahl-Pruefung bei der Dividendeneingabe rechnet gegen den Bestand des
  gewaehlten Depots am Ex-Tag.

  @note Fuer Verkaufs- und Dividenden-Formular ist das eine Verschaerfung.
  Bereits gespeicherte Datensaetze bleiben unberuehrt.

## [1.19.2] - 2026-08-26

### Changed

- **`tst_mainwindow.cpp` weiter aufgeteilt** — die Testklasse `TestMainWindow`
  war mit 255 Testmethoden die groesste des Projekts und pruefte neben
  `MainWindow` noch drei vollstaendige MVP-Trios, die dort nie hingehoert
  hatten. Sie liegen jetzt in eigenen Zielen: `tst_brokeragesform` (72 Tests),
  `tst_shareaddform` (32) und `tst_shareeditform` (dort 27 dazu, jetzt 66).
  Die zehn Faelle rund um `AppSettings` sind nach `tests/config/` gewandert
  (`tst_appsettings`) — sie beruehren weder Dialog noch MainWindow. In
  `tst_mainwindow` bleiben 114 Tests; die Datei schrumpft von 5.949 auf 3.634
  Zeilen, sechs Stub-Klassen und 13 Includes entfallen.

  Keine Aenderung am Verhalten der Anwendung: es wurde keine Testmethode
  hinzugefuegt, entfernt oder inhaltlich veraendert, nur verschoben. Die
  Testklasse in `tst_shareeditform.cpp` heisst dabei `TestShareEditForm`
  statt `TestViewShareEdit`, weil sie jetzt nicht mehr nur die View prueft.

  Chart- und ShareDetails-Faelle sind bewusst in `tst_mainwindow` geblieben,
  obwohl `tst_chartform` und `tst_sharedetailsform` thematisch besser passen
  wuerden: sie konstruieren ein echtes `MainWindow` bzw. echte
  `ViewShareDetails`-Dialoge und haetten diesen beiden bewusst schlanken
  Zielen `MainWindow.cpp`, Qt6::Charts und die Datenbank aufgehalst.

### Fixed

- `tst_shareeditform` setzte in seinem `main()` kein
  `QLocale::setDefault(QLocale::German)`. Aufgefallen ist das erst durch die
  Auslagerung — bisher enthielt die Datei keinen Vergleich gegen einen
  formatierten Betrag, mit den uebernommenen Faellen tut sie es. Auf einem
  Runner mit englischer System-Locale waeren sie fehlgeschlagen.

## [1.19.1] - 2026-08-26

### Changed

- `Documents.xml` beschreibt je Eintrag ein DEPOT, nicht eine Bank — das war
  immer schon so gemeint, stand aber nirgends, und die Bezeichner im Code
  legten das Gegenteil nahe. Der Bankname ist reiner Anzeigetext und nicht
  eindeutig: zwei Depots bei derselben Bank tragen ihn beide. Eineindeutig
  ist die Depotnummer.

  Entsprechend umbenannt: `BankEntry` heisst `DepotEntry`, seine Felder
  `name`/`identifier` heissen `bankName`/`depotNumber`,
  `DocumentsConfig::findByName()` ist durch `findByDepotNumber()` ersetzt
  und `DocumentClassifier::matchBankIndex()` durch `matchDepotIndex()`. Die
  XML-Datei selbst bleibt unverändert — sie ist ein Konfigurationsformat im
  Feld, ein Schemawechsel wäre ein Bruch ohne fachlichen Gewinn.

  Zwei Einträge mit derselben Depotnummer sind ein Konfigurationsfehler und
  werden beim Laden abgewiesen, statt stillschweigend übergangen zu werden:
  welcher der beiden Regelsätze für einen Beleg gilt, wäre sonst nicht mehr
  entscheidbar.

### Fixed

- **Ein Consors-Beleg wurde als DKB-Beleg ausgewertet** (Nessies Bugreport
  25.08.2026). Die Depoterkennung prüfte nur, DASS die Depotnummer-Regel
  einer Bank irgendwo im Text traf — welche Nummer sie dabei fing, sah sich
  niemand an. Das Attribut `BankIdentifierValue`, das genau diese Nummer je
  Depot enthält, wurde für die Erkennung überhaupt nicht herangezogen.

  DKB und Cortal Consors beschriften ihre Depotnummer gleich. Die DKB-Regel
  sucht bis zu neun Ziffern und trifft damit auch auf der zehnstelligen
  Consors-Nummer — sie fängt die ersten neun und lässt die letzte liegen,
  was für einen Treffer genügt. Da die DKB in `Documents.xml` zuerst steht,
  landete jeder Consors-Beleg bei ihr und wurde mit ihren Regeln
  ausgewertet: andere Beschriftungen für Datum, Stückzahl und Kurs, also
  leere oder falsche Felder — ohne jeden Hinweis.

  Erkannt wird ein Depot jetzt nur noch, wenn die Regel trifft UND die
  gefangene Nummer der hinterlegten entspricht. Der Vergleich ist bewusst
  hart: ein Eintrag in `Documents.xml` beschreibt genau ein Depot, und die
  dort hinterlegte Nummer steht zeichengetreu so auf dem Beleg. Ein Beleg
  aus einem noch nicht eingetragenen Depot gilt damit als nicht erkannt —
  auch dann, wenn die Bank längst eingetragen ist. Das ist die richtige
  Antwort: für ein neues Depot gehört ein neuer Eintrag in die Datei. Die
  Meldung beim Ablegen per Drag&Drop nennt die Depotnummer jetzt
  ausdrücklich als Ursache.

- Der Prüfbestand für Consors-Belege wich an der entscheidenden Stelle von
  der Wirklichkeit ab: der Testbeleg trug die Depotnummer ohne ihre führende
  Null. Folgenlos, solange nur die Beschriftung geprüft wird — mit einem
  Wertvergleich wäre der Beleg seinem eigenen Depot nicht mehr zugeordnet
  worden, und der Test hätte einen Fehler gemeldet, den es gar nicht gibt.

- Diese Datei war seit dem 1.15.0-Release nicht mehr auf Versionen
  aufgeteilt: alles ab 1.16.0 sammelte sich unter `[Unreleased]`, zuletzt
  ueber 300 Zeilen. Damit liess sich nicht mehr erkennen, welche Aenderung zu
  welcher Version gehoert, und beim naechsten Release waere das Nachziehen
  immer teurer geworden. Die Eintraege stehen jetzt unter 1.16.0 bis 1.19.1.
  Die Zuordnung stammt aus der Git-Historie, nicht aus einer Schaetzung —
  eine falsch zugeordnete Zeile sieht verlaesslich aus und ist es nicht.

  Ausserdem zusammengefuehrt: ein Doppeleintrag `### Changed` innerhalb
  desselben Versionsabschnitts, beim Anlegen des Bankerkennungs-Eintrags
  entstanden. Zwei gleichnamige Rubriken widersprechen dem Format, an das
  sich die Datei haelt.

## [1.19.0] - 2026-08-25

### Added

- Plausibilitätsprüfung des Split-Verhältnisses, fünfter und letzter
  Prüfzeitpunkt: das Ex-Tag-Feld im Split-Dialog startet nicht mehr mit dem
  heutigen Datum, und ein Ex-Tag in der Zukunft wird beim Speichern einmal
  hinterfragt.

  Der Ertrag liegt bei der Vorbelegung, nicht bei der Warnung. Das Feld war
  mit dem heutigen Tag belegt; im Feldfall wurde dieser Vorschlag unverändert
  übernommen und stand als Ex-Tag in der Datenbank. Eine Prüfung auf "Ex-Tag
  nicht angegeben" gab es zwar längst, sie konnte aber nie auslösen — ein
  vorbelegtes Feld ist nie leer. Jetzt startet es unbelegt, dieselbe
  Konvention wie beim Ex-Tag im Dividenden-Dialog, und ein Split ohne aktiv
  eingetragenes Datum lässt sich nicht mehr speichern.

  Zukünftige Ex-Tage bleiben ausdrücklich erlaubt — ein angekündigter Split
  darf sofort erfasst werden und bleibt bis zu seinem Ex-Tag ohne Wirkung auf
  Bestände und Kurse. Die Rückfrage sagt das auch so und bittet nur um einen
  Abgleich mit der Bankmitteilung. Beim Bearbeiten eines solchen Splits mit
  unverändertem Datum kommt sie nicht erneut.

  Für den heutigen Tag wird bewusst nicht gefragt: seit das Feld unbelegt
  startet, ist "heute" eine getippte Eingabe wie jede andere.

  Damit stehen alle fünf Prüfzeitpunkte.

- Plausibilitätsprüfung des Split-Verhältnisses, vierter von fünf
  Prüfzeitpunkten: die Nachprüfung im Hintergrund meldet beim Programmstart
  und nach jedem Tageswert-Abruf jetzt auch Splits, deren Verhältnis nicht
  zum gemessenen Kurssprung oder nicht zur Verkaufshistorie passt.

  Die drei bisherigen Stufen setzen alle eine Nutzeraktion voraus — einen
  Verkauf, ein Speichern, einen Knopfdruck. Was bereits fehlerhaft in der
  Datenbank steht und von sich aus nie wieder angefasst wird, fiel damit
  niemandem auf. Genau das war der Feldfall: der Split lag monatelang
  falsch da, ohne dass irgendetwas ihn noch einmal angesehen hätte.

  Gemeldet wird nur bei eindeutiger Zuordnung. Hier erscheint ein modaler
  Dialog bei jedem Programmstart, den niemand abstellen kann, solange der
  Befund besteht — eine unvollständig erfasste Kaufhistorie etwa nach einem
  Depotübertrag erzeugt denselben rechnerischen Widerspruch, ohne dass es
  etwas zu korrigieren gäbe. Solche Fälle bleiben hier still und werden
  weiterhin beim Speichern eines Splits oder eines Verkaufs sichtbar.

  Die Befunde stehen in einem Dialog, nach Art gruppiert. Titel jetzt
  "Splits prüfen" statt "Split-Bereinigung prüfen". Geschrieben wird
  weiterhin nichts, die Korrektur bleibt dem Split-Dialog überlassen.

### Changed

- `SplitAdjustmentAudit` heisst jetzt `SplitAudit`. Der alte Name kam von
  `prices_adjusted`; seit die Klasse auch Verhältnisse prüft, traf er nicht
  mehr zu. In `MainWindow` sind die zugehörigen Bezeichner
  (`SplitAuditWarning`, `buildSplitAuditWarningMessage()`,
  `populateSplitAuditWarnings()`, `refreshSplitAuditWarningsForShare()`,
  `warnAboutSplitAuditFindings()`) mit umbenannt.

## [1.18.0] - 2026-08-24

### Added

- Plausibilitätsprüfung des Split-Verhältnisses, dritter von fünf
  Prüfzeitpunkten: der "Prüfen"-Knopf im Split-Dialog vergleicht den
  gemessenen Kurssprung um den Ex-Tag jetzt auch mit dem eingetragenen
  Verhältnis und weist darauf hin, wenn er besser zu einem anderen passt.

  Das schliesst die Lücke der beiden Bestandsprüfungen: die hängen an einer
  Unterdeckung, und ein zu grosses Verhältnis erzeugt nie eine. Wer 21 statt
  20 einträgt, hat rechnerisch mehr Bestand als nötig, alles geht auf,
  niemand fragt nach. Der Kurssprung ist zudem die einzige Gegenprobe, die
  schon beim Erfassen vorliegt — lange bevor ein Verkauf existiert.

  Die bisherigen Toleranzbänder taugten dafür nicht: mit plus/minus 20
  Prozent geht bei eingetragenen 19 auch ein gemessener Sprung von 19,98
  glatt als Treffer durch. Verglichen wird deshalb gegen 3 Prozent, und nur
  gegen das nächstgelegene saubere Verhältnis. Bei unruhigen Kursen oder
  Verhältnissen nahe 1 bleibt die Prüfung still, statt zu raten.

  Der "Kurshistorie bereinigt"-Haken wird davon nicht berührt — das ist eine
  andere Frage als das Verhältnis. Nur die Einfärbung der Ergebniszeile
  wechselt auf "manuelle Entscheidung nötig".

## [1.17.0] - 2026-08-22

### Added

- Plausibilitätsprüfung des Split-Verhältnisses, zweiter von fünf
  Prüfzeitpunkten: der Dialog "Aktiensplits" prüft beim Speichern und beim
  Löschen, ob die Verkaufshistorie unter der resultierenden Split-Liste
  noch aufgeht — je Depot, mit einem Bestandsverlauf über alle Käufe und
  Verkäufe. Greift damit genau dort, wo Punkt 1 nichts sagen kann: bei
  nachträglich erfassten Splits, wo die Verkäufe längst in der Datenbank
  stehen.

  Das Ergebnis ist eine Rückfrage, keine Blockade. Eine unvollständig
  erfasste Kaufhistorie — etwa nach einem Depotübertrag von einer anderen
  Bank — erzeugt denselben rechnerischen Widerspruch, ohne dass am Split
  etwas falsch wäre, und dürfte niemanden dauerhaft daran hindern,
  überhaupt einen Split zu erfassen. Lässt sich der Widerspruch dem Split
  zuordnen, nennt der Text das Verhältnis, mit dem die Rechnung aufginge;
  sonst weist er auf die mögliche Datenlücke hin.

  Neue Model-Methoden `loadBuys()`/`loadSales()` im Split-Dialog —
  `openLots()` liefert nur Restbestände und trägt keine Depotnummer.

## [1.16.0] - 2026-08-22

### Added

- Plausibilitätsprüfung des Split-Verhältnisses, erster von fünf
  Prüfzeitpunkten (siehe `docs/architecture/ARCHITECTURE.md`,
  "Plausibilitätsprüfung des Split-Verhältnisses" sowie die Arbeitsliste
  unter "Offene Punkte"). Blockiert das Verkaufsformular eine zu hohe
  Menge und liegt zwischen Käufen und Verkauf ein Split, nennt die Meldung
  jetzt diesen Split als wahrscheinlichere Ursache — und, wo die
  Rückrechnung eindeutig ist, das Verhältnis, mit dem die Rechnung exakt
  aufginge. Ohne diese Deutung lag nahe, die Stückzahl auf dem Beleg zu
  "korrigieren" statt das Verhältnis zu berichtigen; genau das wäre im
  Feldfall Alphabet fast passiert (10 Stück mal Faktor 19 ergeben 190, der
  Verkaufsbeleg lautet auf 200, richtig gewesen wäre 20:1). Neue Klasse
  `SplitRatioChecker` mit eigenem Testziel `tst_splitratiochecker`; die
  Mengenprüfung selbst und ihre bisherige Meldung bleiben unverändert.

  Ein Verhältnis wird bewusst nur bei genau einem dazwischenliegenden
  Split, alter Seite 1, keinem Reverse-Split und einer Rückrechnung auf
  exakt eins mehr als eingetragen vorgeschlagen. Dieselbe Formel liefert
  sonst auch bei einem reinen Tippfehler ein formal sauberes, aber völlig
  irreführendes Ergebnis.

- Cortal Consors: Depotnummer und Währung werden aus der
  Dividendengutschrift gelesen (Beleg nachgereicht am 21.08.2026, siehe
  `docs/architecture/ARCHITECTURE.md`, "Nachtrag Cortal Consors").
  Stückzahl, Dividendensatz und die beiden Steuern wurden von den
  vorhandenen Regeln bereits korrekt getroffen und sind jetzt durch
  Testfälle abgesichert.

- Ersatzhinweis, wenn ein Beleg den Ex-Tag nicht nennt: Cortal Consors
  weist stattdessen den "Schlusstag" (Dividenden-Stichtag) aus. Er wird
  gelesen und als Hinweis an das Ex-Tag-Feld gehängt — angezeigt, nicht
  eingetragen. Das Feld bleibt eine fehlende Pflichtangabe; der Ex-Tag ist
  laut Consors "normalerweise" der nächste Handelstag nach dem Schlusstag,
  und ein um einen Tag falscher Ex-Tag ginge unmittelbar in die
  Stückzahl-Plausibilitätsprüfung ein. Neue View-Methode `setFieldHint()`.

- Fehlermeldung bei nicht zuzuordnenden Dokumenten unterscheidet jetzt die
  beiden Ursachen: unbekannte BANK (es fehlt ein Eintrag in
  `Documents.xml`) gegen unbekannten BELEGTYP (die Anwendung verarbeitet
  diese Belegart nicht). Im zweiten Fall nennt die Meldung die erkannte
  Bank und die vier unterstützten Belegarten. Neues Feld
  `DocumentClassifier::Result::bankMatched`.

### Changed

- Die DividendForm-Tests haben ein eigenes Testziel `tst_dividendform`
  (Datei `tests/forms/tst_dividendform.cpp`). `tst_mainwindow.cpp` war auf
  11.273 Zeilen mit fünf Testklassen gewachsen, obwohl die Konvention ein
  Testziel je Form vorsieht; mit dem Umzug sind es 9.273 Zeilen und vier
  Klassen. Reine Umstrukturierung ohne Verhaltensänderung: die 127
  Testmethoden, die beiden Stubs und die Helfer sind unverändert übernommen,
  es kam keine Prüfung dazu und es fiel keine weg. Die DividendForm-Quellen
  bleiben Compile-Abhängigkeit von `tst_mainwindow` und `tst_shareeditform`
  (über `MainWindow` bzw. `ViewShareEdit`), werden dort aber nicht mehr
  getestet — dasselbe Muster wie bei `tst_buysform`. Offen bleiben
  `TestOwnMessageBox` und `TestBackupForm`, siehe
  `docs/architecture/ARCHITECTURE.md`, "tst_mainwindow.cpp in eigene
  Testdateien aufteilen".

- Die SalesForm-Tests haben ein eigenes Testziel `tst_salesform` (Datei
  `tests/forms/tst_salesform.cpp`). Zweiter Schritt derselben Aufteilung wie
  beim DividendForm-Umzug am selben Tag: mit ihm sind es 6.464 Zeilen und
  drei Klassen in `tst_mainwindow.cpp`. Reine Umstrukturierung ohne
  Verhaltensänderung — die 123 Testmethoden, die beiden Stubs
  (`StubModelSaleEdit`, `StubViewSaleEdit`) und der Dateihelfer `makeSale()`
  sind unverändert übernommen, es kam keine Prüfung dazu und es fiel keine
  weg. Die SalesForm-Quellen bleiben Compile-Abhängigkeit von
  `tst_mainwindow` und `tst_shareeditform` (über `MainWindow` bzw.
  `ViewShareEdit`), werden dort aber nicht mehr getestet. Offen bleiben
  `TestOwnMessageBox` und `TestBackupForm`.

- Die OwnMessageBox- und BackupProgressForm-Tests haben eigene Testziele
  `tst_ownmessagebox` und `tst_backupform` (Dateien
  `tests/forms/tst_ownmessagebox.cpp` und `tests/forms/tst_backupform.cpp`).
  Letzter Schritt derselben Aufteilung: `tst_mainwindow.cpp` schrumpft damit
  von 6.464 auf 5.823 Zeilen und enthält nur noch eine Testklasse
  (`TestMainWindow`) statt ursprünglich fünf — die Konvention "ein Testziel
  je Form" gilt jetzt für die ganze Datei. Reine Umstrukturierung ohne
  Verhaltensänderung: die 26 (`TestOwnMessageBox`) bzw. 14
  (`TestBackupForm`) Testmethoden sind unverändert übernommen, es kam keine
  Prüfung dazu und es fiel keine weg. Anders als bei DividendForm/SalesForm
  gab es hier keine eigenen Stub-Klassen. `TestBackupForm` bleibt der
  Sonderfall unter den fünf Umzügen: drei seiner Tests konstruieren ein
  echtes `MainWindow` (`createBackup()` ist privat), weshalb die
  CMake-Quellenliste von `tst_backupform` praktisch die von `tst_mainwindow`
  spiegelt. Die OwnMessageBox-/BackupProgressForm-Quellen bleiben Compile-
  Abhängigkeit von `tst_mainwindow` (über `MainWindow`), werden dort aber
  nicht mehr getestet.

### Fixed

- **Die Ordernummer wurde verfälscht** (Nessies Bugreport 22.08.2026). Der
  Beleg zeigt "670835/66.00", im Formular stand "670835/66,00". Die
  Eingabemasken schrieben in JEDEM einzeiligen Feld den Punkt in ein Komma um
  — gedacht war das für Zahlenfelder, getroffen wurden auch Ordernummer, WKN,
  ISIN, Name und die drei URL-Felder im Dialog "Aktie hinzufügen". Unterschieden
  wird jetzt am `QDoubleValidator`, den nur die Zahlenfelder tragen. Betraf alle
  vier Formulare (Kauf, Verkauf, Dividende, Aktie anlegen).

- **Das Datum eines DKB-Verkaufsbelegs wurde nicht übernommen** (Nessies
  Bugreport 22.08.2026): im Formular stand das heutige statt des Belegdatums.
  Die DKB beschriftet Datum und Uhrzeit gemeinsam ("Schlusstag/-Zeit
  27.02.2020 19:16:37"); der ganze Fang ging an
  `QDate::fromString(…, "d.M.yyyy")`, schlug fehl — und das Feld behielt
  stillschweigend seinen Vorgabewert, also das heutige Datum. Ein Verkauf aus
  2020 wäre damit auf den Erfassungstag gebucht worden, mit Folgen für
  FIFO-Zuteilung und Steuerjahr. Datum und Uhrzeit werden jetzt aus einem
  gemeinsamen Rohwert herausgelesen; misslingt die Umwandlung, zeigt das Feld
  ein Fehlersymbol, statt einen falschen Wert stehen zu lassen. Neue
  header-only Einheit `app/utils/DocumentFieldValue.h` mit eigenem Testziel.

- Zahlenfelder: ein Wert mit Tausendertrenner ("1.234,56") wurde zu
  "1,234,56" und beim Auslesen zu 0,00 — ein Betrag über tausend Euro fiel
  lautlos auf null. Nicht gemeldet, beim Beheben der Ordernummer aufgefallen.

- `ViewShareAdd`: der Zweig für Zahlenfelder in `setFieldOk()` war toter Code
  (der Zweig davor fing bereits jedes `QLineEdit` ab). In diesem Dialog wurde
  deshalb noch nie ein Dezimalpunkt umgeschrieben.

- Die DKB-Verkaufsregel für Datum und Uhrzeit hängt nicht mehr an der
  Nachbarspalte (`…Auftraggeber`), sondern nur noch an ihrer eigenen
  Beschriftung. Keine Fehlerursache, aber eine Bindung, die beim nächsten
  Layoutwechsel ohne Grund gebrochen wäre.

### Testabdeckung

- Erster VERKAUFSbeleg im Fixture-Bestand von `tst_documentsxml` (bis dahin
  nur Dividendengutschriften). Dass die DKB die WKN auch auf Verkaufsbelegen
  in Klammern neben die ISIN setzt, ist damit festgehalten — die Direkte
  Dokumentenerfassung kann solche Belege einer Aktie zuordnen.

- **DKB-Belege liessen sich nicht per Drag&Drop erfassen** (Nessies Bugreport
  21.08.2026: "Keine passende Aktie im Portfolio gefunden für
  Dividenden-Dokument"). Derselbe Beleg wurde im Dividenden-Dialog
  einwandfrei gelesen — die beiden Wege lasen die WKN unterschiedlich. Die
  Direkte Dokumentenerfassung geht über
  `DocumentClassifier::extractFieldValue()`, und das nahm immer den ERSTEN
  Regex-Treffer und daraus starr die Fanggruppe 1; das Attribut `FoundIndex`
  der Regel wurde ignoriert. Die DKB wählt ihre WKN aber über die Position
  (`FoundIndex="1"`, "das zweite Klammerpaar"), weil das erste die
  Spaltenüberschrift `(WKN)` ist — gesucht wurde im Portfolio also
  buchstäblich nach einer Aktie mit der WKN "WKN". Betraf alle drei
  DKB-Belegarten (Kauf, Verkauf, Dividende), die sich diese Regel teilen; ING
  und Cortal Consors blieben unauffällig, weil ihre Regeln auf
  `FoundIndex="0"` stehen. `extractFieldValue()` verwendet jetzt dieselbe
  Auswahlregel wie `ParserLib::Parser` — der geforderte Treffer, daraus die
  erste nicht-leere Fanggruppe.

- **Consors-Belege wurden überhaupt nicht eingelesen** (Nessies Bugreport
  21.08.2026). Ein leeres Regex-Muster ist gültig und trifft jeden Text.
  Cortal Consors hat eine leere `SaleIdentifier`-Regel, weil für diese Bank
  keine Verkaufsbelege konfiguriert sind — und weil die Dokumenttyp-Erkennung
  die vier Kennungen der Reihe nach prüft (Buy, Sale, Dividend, Brokerage)
  und den ersten Treffer nimmt, wurde jede Consors-Dividendengutschrift als
  Verkaufsbeleg eingestuft. Da die Bank folgerichtig auch keinen
  Sale-Dokumentblock hat, brach die Erkennung danach ab, ohne ein einziges
  Feld zu lesen. `DocumentClassifier` wertet ein leeres Muster jetzt als "
  identifiziert nichts". Betraf neben Dividenden auch die Kostenbelege dieser
  Bank; Käufe blieben unauffällig, weil deren Kennung vorher geprüft wird.

- Cortal Consors: Als Auszahlungstag wurde der **Schlusstag** übernommen
  statt der Valuta. Die Regel nahm schlicht das erste Datum im Text; der
  Beleg nennt aber zuerst den Schlusstag (= Dividenden-Stichtag, laut
  Consors normalerweise einen Tag vor dem Ex-Tag) und erst danach die
  Valuta. Jetzt beschriftungsgebunden über `Valuta`.

### Bekannte Einschränkung

- Der Ex-Tag lässt sich aus Consors-Belegen nicht auslesen: sie nennen ihn
  nicht. Der genannte Schlusstag liegt laut Consors "normalerweise" einen
  Tag davor — für den nächsten HANDELStag bräuchte es einen Börsenkalender,
  den die Anwendung nicht führt. Ein um einen Tag falscher Ex-Tag ginge
  direkt in die Stückzahl-Plausibilitätsprüfung ein, deshalb bleibt er bei
  dieser Bank ein Handeintrag. Der Schlusstag wird immerhin als Hinweis am
  Feld angezeigt (siehe oben), damit er nicht anderswo nachgeschlagen
  werden muss.

- Vorabpauschale-Abrechnungen für thesaurierende Fonds werden bewusst nicht
  verarbeitet (Nessies Entscheidung 21.08.2026): dabei fliesst kein Geld zu,
  es wird nur Steuer abgeführt — als Dividende erfasst wiese die Anwendung
  eine Einnahme aus, die es nie gab. Über "Direkte Dokumentenerfassung"
  werden solche Belege abgewiesen, jetzt mit der genaueren Meldung.

## [1.15.0] - 2026-08-21

### Added

- Ex-Tag- und Depotnummer-Behandlung bei Dividenden, Phase 1 von fünf (siehe
  `docs/architecture/ARCHITECTURE.md`, "Plausibilitätsprüfung der
  Dividenden-Stückzahl"): `DividendObject` und `DividendRepository` um
  `exDate`/`depotNumber` erweitert, neue nullable Spalten `dividends.ex_date`
  und `dividends.depot_number` (Database-Bibliothek, siehe deren
  CHANGELOG.md, `[1.3.0]`). Ohne sichtbare Auswirkung auf die
  Anwendung — kein UI-Feld, keine Validierung, keine Prüfung; das folgt in
  den weiteren Phasen. `isValid()` bleibt bewusst unverändert an
  `guid`/`rate`/`volume` geknüpft, NICHT an die beiden neuen Felder, damit
  bestehende Dividenden ohne sie weiterhin korrekt laden.
- Ex-Tag und Depotnummer als Pflichtfelder im Dividenden-Dialog, Phase 2 von
  fünf (siehe `docs/architecture/ARCHITECTURE.md`, "Plausibilitätsprüfung der
  Dividenden-Stückzahl", Abschnitt "Phase 2 — Umsetzungsdetails"):
  `ViewDividendEdit` bekommt ein neues Ex-Tag-Feld (`QDateEdit`) und eine
  Depotnummer-Combobox (befüllt aus `DocumentsConfig::entries()`, identisches
  Muster wie `ViewBuyEdit`). Beide sind Pflichtfelder — `hasMissingRequiredFields()`
  markiert sie rot, wenn Depotnummer nicht ausgewählt oder Ex-Tag auf dem
  Sentinel-Wert "nicht gesetzt" steht. Alte Dividenden ohne diese Angaben
  zeigen beim Laden bewusst den Sentinel/leer statt eines beliebigen Werts,
  sodass die Pflicht beim nächsten Speichern zuschlägt (Nachpflege
  erforderlich, wie am 21.08.2026 festgelegt).
- Blockade Ex-Tag nach dem Zahltag ("weil es eben nicht sein darf!", Nessies
  Entscheidung 21.08.2026): sofortige Rückmeldung beim Editieren
  (`PresenterDividendEdit::onExDateEdited()`) und verbindliche Prüfung beim
  Speichern (`validateInput()`) — ein Ex-Tag nach dem Auszahlungstag verhindert
  das Speichern mit einer Fehlermeldung, unabhängig davon ob das Feld zuvor
  editiert wurde.
- Stückzahl-Plausibilitätsprüfung bei Dividenden, Phase 3 von fünf (siehe
  `docs/architecture/ARCHITECTURE.md`, Abschnitt "Phase 3 —
  Umsetzungsdetails"): Beim Speichern wird die eingetragene Stückzahl gegen
  den Bestand des gewählten Depots am Ex-Tag geprüft. Weicht sie ab, wird das
  Speichern abgelehnt und die Meldung nennt eingetragene Menge, errechneten
  Bestand, Ex-Tag und Depot; das Mengenfeld wird zusätzlich rot markiert.
  Neue, datenbankfreie Utility-Klasse `DividendVolumeChecker` (`app/utils/`)
  mit eigenem Testziel `tst_dividendvolumechecker`.
- Die Prüfung ist split-bewusst: Käufe und Verkäufe liegen jeweils in der
  Beleg-Skala ihres eigenen Datums vor, deshalb wird über
  `ShareSplitAdjuster` auf heutige Skala summiert und das Ergebnis auf die
  Beleg-Skala des Ex-Tags zurückgerechnet — genau die Skala, in der die
  Stückzahl auf der Dividendenabrechnung steht. Ein Kauf von 100 Stück mit
  anschliessendem 2:1-Split ergibt so korrekt 200 erwartete Stück und keine
  Falschmeldung.
- Stichtagsregel der Prüfung: gezählt werden Käufe und Verkäufe ECHT VOR dem
  Ex-Tag. Wer am Ex-Tag kauft, ist nicht mehr dividendenberechtigt; wer am
  Ex-Tag verkauft, erhält die Dividende noch.
- Übersprungen wird die Prüfung nur, wenn für die Aktie überhaupt kein Kauf
  erfasst ist — sonst wäre eine Dividende bei nicht erfasster Kaufhistorie
  gar nicht mehr speicherbar. Sobald der erste Kauf erfasst ist, greift sie.
- Automatisches Auslesen von Ex-Tag, Depotnummer und Währung aus der
  Dividendengutschrift, Phase 5 von fünf und damit letzter Schritt des Plans
  (siehe `docs/architecture/ARCHITECTURE.md`, Abschnitt "Phase 5 —
  Umsetzungsdetails"). Grundlage sind vier anonymisierte Belege (ING DiBa in
  EUR und USD, DKB in USD und EUR). Alle neuen Regeln in
  `app/config/Documents.xml` suchen über die Feldbeschriftung statt über die
  Position im Text und sind damit unempfindlich gegen Layout-Unterschiede.
- Fremdwährungs-Modus wird beim Einlesen automatisch gesetzt: nennt der Beleg
  eine andere Währung als Euro, werden Haken und Währungsauswahl gesetzt und
  der Devisenkurs übernommen — nennt er Euro, wird der Modus abgeschaltet.
  Neue View-Methode `setForeignCurrency()`; die Zuordnung Währungskürzel →
  Auswahlfeld übernimmt `QLocale`, damit es keine zweite, handgepflegte
  Tabelle gibt.
- Der DKB-Zahltag wird jetzt über die Beschriftung "Zahlbarkeitstag" gesucht
  statt als "zweites Datum im Text". Im Beleg stehen Zahlbarkeitstag,
  Bestandsstichtag und Ex-Tag direkt untereinander; welches davon das zweite
  ist, hing auch vom Briefkopf ab.
- Neues Testziel `tst_documentsxml`: prüft die AUSGELIEFERTE
  `app/config/Documents.xml` gegen Auszüge echter Belege. Bis hierher war die
  Konfigurationsdatei an keiner Stelle getestet — ein Tippfehler in einem
  regulären Ausdruck fiel erst auf, wenn ein Benutzer ein Dokument einlas.

### Fixed

- Split-Marker bei Dividenden richtete sich nach dem Zahltag statt nach dem
  Ex-Tag, Phase 4 von fünf (siehe `docs/architecture/ARCHITECTURE.md`,
  Abschnitt "Phase 4 — Umsetzungsdetails"): Ein Aktiensplit, der ZWISCHEN
  Ex-Tag und Zahltag lag, wurde in den Dividenden-Übersichten übersehen — die
  Anteile-Zelle trug dann weder Marker noch Tooltip, obwohl die Stückzahl auf
  der Abrechnung sehr wohl in der alten Stückelung stand. Die Bank schüttet
  auf den Bestand am Ex-Tag aus, deshalb ist er der richtige Massstab. Neu:
  `DividendObject::volumeReferenceDate()` (Ex-Tag, sonst Zahltag als
  Rückfall). Betrifft beide Ansichten, die Dividendenzeilen zeigen —
  `ViewDividendEdit` und `ViewShareDetails`; hätte nur eine davon umgestellt,
  würden beide dieselbe Dividende widersprüchlich markieren. Dividenden ohne
  Ex-Tag (erfasst vor dem 21.08.2026) verhalten sich unverändert wie bisher.
- Aus dem Beleg gelesener Devisenkurs kam nie an: `exchangeRatio` war als
  einziges beschreibbares Feld des Dividenden-Dialogs weder in
  `m_statusLabels` noch in `m_inputWidgets` eingetragen. Dadurch lief
  `setFieldOk("exchangeRatio", …)` ins Leere — der Kurs erreichte das
  Eingabefeld nicht —, und aus demselben Grund war auch die Live-Prüfung
  `PresenterDividendEdit::onExchangeRatioEdited()` seit jeher wirkungslos.
  Selbst mit gefülltem Feld hätte das Speichern den Kurs verworfen, weil es
  ihn nur bei aktivem Fremdwährungs-Modus übernimmt. Beides behoben; die
  Fremdwährungs-Umrechnung aus einem Beleg funktioniert damit erstmals.

## [1.14.8] - 2026-08-21

### Fixed

- Dokumentenpfad nach Drag&Drop nicht im Eingabe-Dialog übernommen
  (Nessies Bugreport, Dividenden-Dialog, per Screenshot belegt): Wird ein
  Dokument per Drag&Drop auf "Direkte Dokumentenerfassung" abgelegt, öffnet
  `MainWindow::openCaptureDialog()` den passenden Dialog und ruft direkt
  `dlg.presenter()->onDocumentSelected(pdfPath)` auf — der Code in
  `onBrowseDocument()`, der beim manuellen "…"-Klick sonst
  `m_documentPath->setText(path)` setzt, wird dabei nie durchlaufen. Die
  vier Presenter (`PresenterBuyEdit`, `PresenterSaleEdit`,
  `PresenterDividendEdit`, `PresenterShareAdd`) schrieben den Pfad bislang
  selbst nie in die View zurück, sodass das Feld trotz erfolgreich
  geparster Werte auf "Kein Dokument ausgewählt …" stehen blieb — betraf
  alle vier PDF-Erfassungsdialoge identisch, nicht nur Dividenden.
  `PresenterShareSplitEdit::onDocumentSelected()` hatte das Problem nicht,
  da es dort schon immer `m_view->setDocumentPath(path)` aufrief; die
  übrigen vier Presenter folgen jetzt demselben Muster. Neue Methode
  `setDocumentPath()` in `IViewBuyEdit`/`IViewSaleEdit`/
  `IViewDividendEdit`/`IViewShareAdd` (analog `IViewShareSplitEdit`);
  `onBrowseDocument()` aller vier Views ruft sie jetzt selbst auch nur noch
  über den Presenter auf, statt den Dokumentpfad doppelt zu setzen — ein
  einziger Codepfad für Browse-Klick und Drag&Drop. Vier neue
  Regressionstests, je einer pro Dialog —
  `test_presenterBuyEdit_onDocumentSelected_writesPathIntoView`
  (`tst_buysform.cpp`),
  `test_presenterSaleEdit_onDocumentSelected_writesPathIntoView`,
  `test_presenterDividendEdit_onDocumentSelected_writesPathIntoView`,
  `test_presenterShareAdd_onDocumentSelected_writesPathIntoView` (alle drei
  in `tst_mainwindow.cpp`); Stub-Views in
  `tst_buysform.cpp`/`tst_mainwindow.cpp` um `setDocumentPath()` ergänzt.
  Siehe `docs/architecture/ARCHITECTURE.md`, "Dokumentenpfad nach Drag&Drop
  nicht im Eingabe-Dialog übernommen", sowie `docs/testing/TESTING.md`.

## [1.14.7] - 2026-08-20

### Fixed

- Skalenbewusste Mengenprüfung im Verkaufsformular: `SaleFifoAllocator::
  allocate()` deckelte eine zu hohe Verkaufsmenge bislang still auf das
  verfügbare Volumen, statt einen Fehler zu melden — im Feldfall zeigte das
  Formular dadurch grüne Haken und eine vollständige Gewinnermittlung,
  obwohl 3.800 Stück angefordert, aber nur 190 verfügbar waren (beides auf
  heutiger Skala). Neue, skalenbewusste Prüfung (`SaleFifoAllocator::
  isSaleVolumeCovered()`/`totalAvailableVolumeToday()`) verhindert das
  Speichern jetzt mit einer Meldung, die angeforderte und verfügbare Menge
  konkret beziffert, plus Live-Fehler-Icon auf dem Mengenfeld. Ein älterer,
  nicht-jüngster Verkauf (nur das Dokument editierbar) bleibt bewusst
  ausgenommen. Neue Tests in `tst_salefifoallocator.cpp` und
  `tst_mainwindow.cpp` (`TestSalesForm`). Siehe
  `docs/architecture/ARCHITECTURE.md`, "Skalenbewusste Mengenprüfung im
  Verkaufsformular" (Erledigt / Archiv).

## [1.14.6] - 2026-08-20

### Fixed

- Footer-Lücke bei freistehenden Kosteneinträgen: `ShareCalculator::compute()`
  berücksichtigte Brokerage-/Kosteneinträge ohne Kauf- oder Verkaufsbezug
  (angelegt über die Kosten-Verwaltung) bisher nur in `totalBrokerage`, nicht
  in `completePurchase` — die Spalte "Komplette Entwicklung" war dadurch im
  Grid, im Footer und in `ShareDetailsForm` um deren Betrag zu hoch, sowohl im
  Marktwert- als auch im Depotwert-Tab. Der Depotwert-Chart rechnete an dieser
  Stelle bereits korrekt; Footer und Chart stimmen jetzt automatisch überein.
  `completePurchaseMarket` bleibt unverändert brokerage-frei. Neue Tests in
  `tst_sharecalculator.cpp`. Siehe `docs/architecture/ARCHITECTURE.md`,
  "Footer-Lücke bei freistehenden Kosteneinträgen" (Erledigt / Archiv).

## [1.14.5] - 2026-08-20

### Added

- Automatische Nachprüfung des "Kurshistorie bereits bereinigt"-Zustands
  (`prices_adjusted`) nach jedem Tageswert-Abruf, plus Startmeldung bei
  Widerspruch — Phase 4b der Aktiensplit-Behandlung, am 13.08.2026 zunächst
  zugunsten des "Prüfen"-Knopfs zurückgestellt, jetzt nachgezogen (neue
  zustandslose Klasse `SplitAdjustmentAudit` in `app/utils/`, baut auf
  `SplitPriceJumpDetector` auf). Schreibt nichts automatisch in die
  Datenbank — reine Lese-Prüfung, dieselbe Zurückhaltung wie beim
  "Prüfen"-Knopf: nur eine Statusmeldung nach dem betroffenen Tageswert-Abruf
  sowie ein modaler Startup-Hinweis, analog
  `warnAboutSharesWithoutDailyValues()`. Die eigentliche Korrektur bleibt dem
  "Prüfen"-Knopf im Split-Dialog überlassen. Neues Testziel
  `tst_splitadjustmentaudit` sowie ergänzende Tests in `tst_mainwindow.cpp`.
  Siehe `docs/architecture/ARCHITECTURE.md`, "Automatische Nachprüfung nach
  Tageswert-Abruf".

## [1.14.4] - 2026-08-14

### Added

- Knopf "Hinweis Reverse-Split" neben den Verhältnis-Feldern im Split-Dialog
  (`ShareSplitsForm`): öffnet einen Hinweis-Dialog, der erklärt, wie
  Bruchstücke aus einem Reverse-Split (von der Bank bar ausgezahlte
  Spitzen) ohne eigenes Feature abgebildet werden — als normaler Verkauf,
  datiert auf den Ex-Tag des Splits, mit der Menge im neuen
  (Nach-Split-)Maßstab statt der alten. Ist im Formular bereits ein echtes
  Reverse-Split-Verhältnis eingetragen, rechnet der Hinweistext mit genau
  diesem Verhältnis; sonst mit einem festen Beispiel. Kein neuer Fachcode
  nötig, siehe `docs/architecture/ARCHITECTURE.md`, "Bruchstücke bei
  Reverse-Splits nicht abgedeckt".

### Changed

- Ok-Knopf in `OwnMessageBox` (Fehler- und Hinweis-Dialoge, projektweit)
  ohne Icon: das bisherige Disketten-Icon suggerierte fälschlich ein
  Speichern, obwohl der Knopf nur den Dialog schließt.

## [1.14.3] - 2026-08-14

### Added

- "Prüfen"-Knopf im Split-Dialog (`ShareSplitsForm`): vergleicht auf
  Nutzeraktion hin die gespeicherte Kurshistorie um den Ex-Tag eines Splits
  mit dem eingetragenen Umrechnungsverhältnis (neue, zustandslose Klasse
  `SplitPriceJumpDetector` in `app/utils/`) und setzt bei eindeutigem
  Ergebnis automatisch den "Kurshistorie bereits bereinigt"-Haken — bewusst
  kein automatischer/stiller Hintergrundlauf, sondern nur auf explizite
  Nutzeraktion (Nessies Vorgabe). Das Ergebnis erscheint in einem read-only
  Zweizeilen-Feld neben dem Knopf, grün eingefärbt bei automatisch
  übernommenem, rot bei uneindeutigem Ergebnis (dann ist weiterhin eine
  manuelle Entscheidung nötig). Neues Testziel `tst_splitpricejumpdetector`
  (16 Testfälle) sowie ergänzende Presenter- und View-Tests in
  `tst_sharesplitsform.cpp`. Siehe `docs/architecture/ARCHITECTURE.md`,
  "Automatische Erkennung split-bereinigter Kurshistorie".

### Changed

- Tooltip auf dem "Kurshistorie"-Haken im Split-Dialog erweitert: benennt
  jetzt explizit, dass der Haken nur die Kurshistorie betrifft (nicht
  Käufe, Verkäufe oder Dividenden), und beschreibt beide Zustände in
  Klartext.

## [1.14.2] - 2026-08-13

### Changed

- Dokumentfeld im Split-Dialog (`ShareSplitsForm`) auf dieselbe Optik wie bei
  Kauf, Verkauf, Dividende, Kosten und Aktienanlage umgestellt: eigene
  `QGroupBox("  Dokument")` statt Zeile innerhalb der Splitdaten, Ordner-Icon
  statt `…`-Button, Feld read-only. Der Pfad kommt seither ausschließlich
  über den Dateidialog; die bisherige Möglichkeit, ihn manuell einzutippen,
  entfällt entsprechend. Neuer Regressionstest
  `test_view_documentPath_isReadOnly` in `tst_sharesplitsform.cpp`. Siehe
  `docs/architecture/ARCHITECTURE.md`, "ShareSplitsForm-Details", Abschnitt
  "Dokument und Vorschau".

## [1.14.1] - 2026-08-13

### Added

- Tooltip auf dem Umrechnungs-Feld im Split-Dialog (`ShareSplitsForm`),
  der die Bank-Notation des Zuteilungsverhältnisses ("1:19" = 19
  zusätzliche Stücke je gehaltenem Stück) von der von der Anwendung
  erwarteten Umrechnungs-Notation (neu:alt, im selben Beispiel 20:1)
  abgrenzt. Hintergrund: ein realer Feldfall (Alphabet-Aktie), bei dem
  genau diese Verwechslung zu einem Verhältnis führte, das systematisch um
  eins zu klein war. Siehe `docs/architecture/ARCHITECTURE.md`,
  "Split-Verhaeltnis: Notation der Bankmitteilungen".

## [1.14.0] - 2026-08-11

### Added

- Split-Marker in den Anteile-Spalten der Übersichtstabellen (Phase 3c der
  Aktiensplit-Behandlung). Eine Stückzahl, die wegen eines späteren Splits
  nicht mehr dem heutigen Stand entspricht, traegt ein angehaengtes "*" und
  einen Tooltip mit der heutigen Entsprechung. Betroffen sind `ViewBuyEdit`,
  `ViewSaleEdit`, `ViewDividendEdit` sowie die Tabs "Gewinne/Verluste" und
  "Dividenden" in `ViewShareDetails` (Gewinne/Verluste in beiden Modi).
- `loadSplits()` in `IModelDividendEdit` und `IModelShareDetails` — reine
  Weiterleitung an `ShareSplitRepository::findByShare()`, wortgleich zu
  `IModelBuyEdit`/`IModelSaleEdit`.

### Fixed

- Summen ueber Stueckzahlen mischten Belege unterschiedlicher Stueckelung.
  Fusszeilen und Jahreszeilen der Uebersicht rechnen jetzt je Beleg ueber
  `ShareSplitAdjuster::adjustedVolume()` auf heutige Skala um und summieren
  erst danach. Sichtbar wurde der Fehler, wenn ein Split mitten in ein Jahr
  fiel: aus 5 Stueck vor und 100 Stueck nach einem 20:1-Split ergab die rohe
  Summe 105 statt der korrekten 200.
- Die Anteile-Summe der Jahres-Fusszeile in `ViewDividendEdit` und im
  Dividenden-Tab von `ViewShareDetails` zeigt jetzt "-". "Anteile am
  Auszahlungstag" bezieht sich auf je einen Stichtag; die Summe ueber mehrere
  Ausschuettungen beschreibt keinen Bestand, den es je gab. Diese Summe war
  schon vor jedem Split falsch — der Split machte sie nur sichtbar.

### Changed

- Der aktive Split-Hinweis unter den Kauf-/Verkaufsdaten ist jetzt orange und
  fett statt blau. Der gedaempfte Zustand ("Kein Split nach diesem Datum")
  bleibt zurueckhaltend, wechselt aber auf `palette(placeholderText)` — im
  dunklen Theme war er zuvor kaum lesbar. Das Label sitzt ab Gitterspalte 1
  statt 0, der untere Rand der Gruppe ist auf 4 px reduziert.
- `populateOverview()` in `IViewBuyEdit`, `IViewSaleEdit` und
  `IViewDividendEdit` sowie `populateGewinneVerluste()`/`populateDividenden()`
  in `IViewShareDetails` nehmen die Splits als zusaetzlichen Parameter
  entgegen statt ueber einen eigenen Setter. Ein Setter erzeugt eine
  unsichtbare Reihenfolge-Abhaengigkeit zwischen zwei View-Aufrufen; als
  Parameter ist sie im Signatur-Typ sichtbar und vom Compiler geprueft.

## [1.13.1] - 2026-08-11

### Fixed

- Die anteiligen Kauf-Nebenkosten gingen beim Speichern eines Verkaufs
  verloren. `PresenterSaleEdit::onSave()` belegte beim Erzeugen der
  `SaleBuyDetail`-Objekte nur vier der sechs Konstruktor-Parameter;
  `reductionPart` und `brokeragePart` haben Defaultwerte 0.0, weshalb der
  Verlust ohne Compilerfehler blieb. In der Datenbank stand seither
  `brokerage_part = 0` fuer jeden neu erfassten oder bearbeiteten Verkauf,
  und die ausgewiesene Gewinnermittlung war um die Provision, Courtage und
  Handelsplatzgebuehr des Kaufs zu guenstig.
- Der Details-Dialog "Verwendete Kaeufe" zeigte die Spalte Kosten im
  Live-FIFO-Zweig immer mit 0,00 EUR — dort standen die Werte hart im Code.
  Der Grund war strukturell: die anteilige Brokerage kommt ueber
  `IModelSaleEdit::loadBrokerageForBuy()`, und die View hat per MVP keinen
  Modellzugriff.
- Die Live-Vorschau von Gewinn/Verlust im Verkaufsformular rechnete ohne die
  anteiligen Kaufkosten und sprang deshalb beim Speichern auf einen anderen
  Wert.
- Die Summenzeile des Details-Dialogs zog den anteiligen Kaufrabatt nicht ab,
  obwohl die Spalte Gesamt ihn je Zeile bereits beruecksichtigt. Solange
  `reduction_part` ueberall 0 war, fiel die Abweichung nicht auf.

### Changed

- Der Inhalt des Details-Dialogs wird jetzt vollstaendig im Presenter
  aufbereitet (`PresenterSaleEdit::buildBuyDetailSummary()`) und ueber die
  neue Interface-Methode `IViewSaleEdit::showBuyDetails()` an die View
  gereicht. `ViewSaleEdit` rendert nur noch. Neuer Header
  `app/forms/SalesForm/SaleBuyDetailRow.h` mit den Transportstrukturen
  `SaleBuyDetailRow` und `SaleBuyDetailSummary`.
- Die Verkaufsgebuehren und Steuern im Details-Dialog stammen beim Bearbeiten
  des juengsten Verkaufs aus dem Formular statt aus dem gespeicherten
  `SaleObject`. Die Felder sind dort editierbar, gespeicherte Werte waeren
  veraltet und wichen von dem ab, was `onSave()` anschliessend schreibt.
- `SaleFifoAllocator` bleibt unveraendert zustandslos und datenbankfrei. Die
  anteilige Verteilung braucht keine Split-Logik: Zuteilungsmenge und
  `buy.volume()` liegen in derselben Beleg-Skala, der Bruch ist damit
  skaleninvariant.

## [1.13.0] - 2026-08-09

### Added

- Aktiensplit-Behandlung, Phase 3b: Split-Hinweis unter den Kauf- und
  Verkaufsdaten in `ViewBuyEdit` und `ViewSaleEdit`. Die Editier-Dialoge
  zeigen weiterhin durchgehend den Beleg; der Hinweis nennt zusätzlich, wie
  viele Stücke daraus heute geworden sind — etwa "Split 20:1 am 18.07.2022 —
  entspricht heute 100,0000 stk. à 50,1500 €". Preis und Stückzahl werden
  gegenläufig umgerechnet, damit sichtbar bleibt, dass ein Split weder Gewinn
  noch Verlust schafft.
- Der Hinweis läuft live mit: er reagiert sowohl auf das Datumsfeld als auch
  auf Stückzahl und Preis. Er steht als Fusszeile der Datengruppe und ist
  IMMER sichtbar — ohne Split mit dem gedämpften Text "Kein Split nach diesem
  Datum". Beides zusammen verhindert, dass beim Tippen Formularzeilen
  springen.
- Bei mehreren Splits nennt der Text Anzahl und jüngsten Splittag und rechnet
  mit dem kumulierten Faktor; die vollständige Liste steht im Tooltip.
- Neuer Helfer `ShareSplitHint` (`app/utils/`) formatiert die Texte für beide
  Dialoge. Zustandslos und datenbankfrei, mit eigenem Testziel
  `tst_sharesplithint`. Die Umrechnung selbst delegiert er an
  `ShareSplitAdjuster` — die Regel, welche Splits zählen und wie Stückzahl
  und Preis skalieren, existiert damit weiterhin nur an einer Stelle.

### Changed

- `IModelBuyEdit`/`ModelBuyEdit` um `loadSplits()` erweitert (in
  `IModelSaleEdit` bereits seit Phase 2c vorhanden); `IViewBuyEdit` und
  `IViewSaleEdit` um `setSplitHint()`.
- `PresenterSaleEdit::refreshDerivedValues()` liest die Splits nicht mehr bei
  jedem Aufruf frisch aus der Datenbank, sondern nutzt den im Konstruktor
  gefüllten Zwischenspeicher. Die Methode läuft bei jeder Eingabe, die Splits
  einer Aktie ändern sich während einer Dialog-Sitzung aber nicht — der Abruf
  war eine Datenbankabfrage je Tastendruck.
- Dividenden bleiben bewusst ohne Split-Hinweis: die Ausschüttung ist über
  einen Split invariant, und die Dividenden-Übersicht summiert im Gegensatz
  zu Käufen und Verkäufen keine Stückzahlen. Stattdessen als offener Punkt
  aufgenommen, die eingegebene Stückzahl künftig gegen den Bestand zum
  Stichtag zu prüfen.

## [1.12.0] - 2026-08-08

### Added

- Aktiensplit-Behandlung, Phase 3a: neue Erfassungsmaske `ShareSplitsForm`
  (`app/forms/ShareSplitsForm/`) als vollständige MVP-Triade, erreichbar über
  einen fünften Stift-Button in `ViewShareEdit`. Der Button sitzt in der
  GroupBox "Allgemein" direkt unter "Anteile:" statt in "Einnahmen / Ausgabe",
  weil ein Split keinen Geldbetrag hat, sondern nur die Stückelung ändert.
  Daneben ein Hinweisfeld, das je nach Lage `keine`, `20:1 am 18.07.2022` oder
  `2 Splits, zuletzt 20:1 am 18.07.2022` anzeigt und alle Splits im Tooltip
  führt.
- Die Maske erfasst Ex-Tag, Verhältnis (neu : alt) mit abgeleiteter
  Umrechnungs-Vorschau, das Kennzeichen "Kurshistorie bereits split-bereinigt"
  sowie einen Kommentar. Zukünftige Ex-Tage sind ausdrücklich erlaubt, damit
  ein angekündigter Split sofort erfasst werden kann; ein Verhältnis mit
  Faktor 1,0 wird abgewiesen, ebenso ein zweiter Split derselben Aktie am
  selben Tag. Die Übersicht ist bewusst eine flache Tabelle ohne Jahres-Tabs —
  eine Aktie hat typischerweise null bis drei Splits.
- Das Löschen eines Splits fragt vorher nach und beziffert dabei die
  Bestandsänderung ("von 2.000,0000 auf 100,0000 Stück"). Die Transaktionen
  selbst bleiben unberührt: ein Split ist nur eine Rechenvorschrift, das
  Löschen ist vollständig umkehrbar.
- Splits tragen jetzt einen Beleg wie Käufe, Verkäufe, Dividenden und Kosten
  auch: Dokumentpfad in `ShareSplitObject`/`ShareSplitRepository`,
  Pfadfeld mit Dateidialog und `DocumentPreviewPanel` in der Maske,
  Dokument-Spalte in der Übersicht. Der Dateidialog lässt nur PDF zu und prüft
  den Pfad gegen das Dokument-Root-Verzeichnis. Ausgewertet wird der Beleg
  nicht — ob ein Parsing von Split-Mitteilungen lohnt, ist als offener Punkt
  festgehalten.
- `DocumentRootMigrator` deckt mit `share_splits` jetzt fünf Tabellen ab, damit
  Split-Dokumente beim Wechsel des Dokument-Roots mit umgeschrieben werden.
  Ergänzt sowohl in `collectAllDocuments()` als auch im Switch von
  `updateDocument()` — nur eins von beiden hätte dazu geführt, dass
  Split-Dokumente still übergangen werden.
- Die Doppelbelegungs-Prüfung `documentExists()` sitzt in
  `ModelShareSplitEdit`, nicht im Repository — dieselbe Platzierung wie bei
  `ModelBuyEdit`, `ModelSaleEdit`, `ModelDividendEdit` und
  `ModelBrokerageEdit`. Sie prüft nur innerhalb von `share_splits` und meldet
  einen Hinweis, blockiert das Speichern aber nicht: zwei Splits können
  legitim auf derselben Bankmitteilung stehen.

### Changed

- `IViewShareEdit` und `IModelShareEdit` um je eine Methode erweitert
  (`setSplitInfo()`, `loadSplits()`); `PresenterShareEdit::populateSummary()`
  aktualisiert die Split-Zeile im selben Durchlauf wie die Geldsummen.

## [1.11.0] - 2026-08-08

### Added

- Aktiensplit-Behandlung, Phase 2c: neue, gemeinsame Klasse
  `SaleFifoAllocator` (`app/utils/`) ersetzt die zuvor dreifach duplizierte
  FIFO-Verkaufszuteilung in `PresenterSaleEdit::onSave()`,
  `refreshDerivedValues()` (Live-Vorschau) und `ViewSaleEdit::onShowDetails()`
  (Details-Dialog). Rechnet Verkaufsmenge und Kauf-Restmengen intern auf die
  heutige Skala um und liefert das zugeteilte Stück je Kauf in dessen
  eigener Beleg-Skala zurück — `ModelSaleEdit::addSale()`/`updateSale()`/
  `removeSale()` bleiben dadurch unverändert. Zwei neue
  `IModelSaleEdit`-Methoden (`loadSplits()`,
  `loadAvailableBuysForDepotExcludingSale()`) versorgen die Zuteilung.
  `ViewSaleEdit::onShowDetails()` zeigt seither durchgängig auf heutiger
  (split-bereinigter) Skala.

### Fixed

- `PresenterSaleEdit::onSave()` übernahm beim Bearbeiten des jüngsten
  Verkaufs bisher unverändert die gespeicherten `SaleBuyDetails`, selbst
  wenn sich die Verkaufsmenge im Formular geändert hatte — ein von Splits
  unabhängiger, vorbestehender Bug. Die FIFO-Zuteilung wird jetzt in jedem
  Fall frisch berechnet, sobald der bearbeitete Verkauf der jüngste ist;
  Live-Vorschau und tatsächliches Speichern laufen dadurch nicht mehr
  auseinander.

## [1.10.0] - 2026-08-07

### Added

- Aktiensplit-Behandlung, Phase 2a: `ShareCalculator::compute()` wendet den
  in Phase 1 angelegten Rechenkern (`ShareSplitAdjuster`) jetzt tatsächlich
  an. Käufe und Verkäufe werden vor jeder Berechnung von ihrer jeweiligen
  Beleg-Skala auf die heutige, nach allen bekannten Splits gültige Skala
  umgerechnet — betrifft Bestand, Depotwert-Grid, Footer-Summen und
  `ShareDetailsForm` gleichermassen. Brokerage, Rabatt und Steuern sind
  Geldbeträge und bleiben unskaliert. Ohne gespeicherte Splits ist das
  Ergebnis bitgenau identisch zum bisherigen Verhalten — bestätigt durch die
  vollständig unveränderten Bestandstests. Vier neue Tests in
  `tst_sharecalculator.cpp`, angelehnt an den Alphabet-Feldfall aus
  ARCHITECTURE.md.
  Noch nicht angepasst: die Chart-Modelle (`ModelPortfolioChart`/
  `ModelChart`) und die FIFO-Verkaufszuteilung (`ModelSaleEdit`) — siehe
  `docs/architecture/ARCHITECTURE.md`, "Offene Punkte", "Aktiensplits werden
  nicht behandelt".

## [1.9.0] - 2026-08-07

### Added

- Datengrundlage für die Behandlung von Aktiensplits (Phase 1 von vier,
  siehe `docs/architecture/ARCHITECTURE.md`, "Offene Punkte", "Aktiensplits
  werden nicht behandelt"): neue Tabelle `share_splits` (Datum, Verhältnis
  `ratio_new`/`ratio_old`, je Split gesetztes Kennzeichen, ob die
  Kurshistorie vor diesem Datum bereits split-bereinigt vorliegt). Neues
  Repository `ShareSplitRepository` sowie der zustandslose, datenbankfreie
  Rechenkern `ShareSplitAdjuster` (`app/utils/`) zur Umrechnung zwischen der
  in `buys`/`sales`/`daily_values` gespeicherten Beleg-Skala und der
  heutigen, nach allen Splits gültigen Skala. Ohne sichtbare Auswirkung auf
  die Anwendung — die Anwendung in `ShareCalculator`, den Chart-Modellen und
  der FIFO-Verkaufszuteilung folgt in Phase 2. Neue Testziele
  `tst_sharesplitrepository` und `tst_sharesplitadjuster`.

## [1.8.0] - 2026-08-06

### Added

- Aktien mit Anteilen im Bestand müssen jetzt zwingend Tageswerte abrufen.
  Im Dialog "Aktie editieren" lassen sich die Update-Typen "Markt-Preis" und
  "Keine" nicht mehr wählen, solange Anteile gehalten werden; eine Hinweiszeile
  nennt den Grund, und das aktive Umstellen auf einen unzulässigen Typ wird
  abgewiesen. Ein bereits gespeicherter unzulässiger Typ blockiert dagegen
  nicht das Speichern anderer Änderungen an derselben Aktie — sonst liesse
  sich an einem delisteten Papier, für das es keine Tageswert-Quelle mehr
  gibt, nicht einmal der Name korrigieren.
  Hintergrund: Aktien ohne Tageswert-Historie lassen sich an keinem
  vergangenen Stichtag bewerten und fallen vollständig aus dem Depotwert-Chart
  heraus — die Kurve liess diese Positionen bisher stillschweigend weg. Die
  Regel liegt im neuen, datenbankfreien Modul `ShareUpdateRules`
  (`app/utils/`) mit eigenem Testziel `tst_shareupdaterules`. Siehe
  `docs/architecture/ARCHITECTURE.md`, "Tageswert-Historie bei Bestand > 0
  erzwingen".
- Beim Programmstart weist eine Meldung auf Aktien hin, die trotz Bestand
  keine Tageswerte abrufen — mit Name, WKN und aktuellem Update-Typ sowie der
  Begründung, warum die Umstellung dringlich ist: die Datenquellen liefern nur
  ein begrenztes Zeitfenster rückwirkend, die in der Zwischenzeit
  ausgelaufenen Tage sind dauerhaft verloren. Betrifft Aktien, die vor dieser
  Änderung angelegt wurden; die Meldung verschwindet von selbst, sobald die
  Einstellung stimmt.

### Changed

- `PresenterShareAdd::onSave()` setzt den Update-Typ neu angelegter Aktien
  explizit auf "Beide", statt sich auf den Vorgabewert von `ShareObject` zu
  verlassen. Der Anlage-Dialog bietet keine Update-Typ-Auswahl an und erzwingt
  ein Kaufvolumen grösser 0 — eine neue Aktie hat also immer Bestand und
  braucht zwingend Tageswerte. Ohne die explizite Zuweisung würde eine spätere
  Änderung des Vorgabewerts stillschweigend Aktien ohne Kurshistorie anlegen.

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

[1.11.0]: https://github.com/nessie1980/SharePortfolioManager-QT/compare/v1.10.0...v1.11.0
[1.10.0]: https://github.com/nessie1980/SharePortfolioManager-QT/compare/v1.9.0...v1.10.0
[1.9.0]: https://github.com/nessie1980/SharePortfolioManager-QT/compare/v1.8.0...v1.9.0
[1.8.0]: https://github.com/nessie1980/SharePortfolioManager-QT/compare/v1.7.0...v1.8.0
[1.7.0]: https://github.com/nessie1980/SharePortfolioManager-QT/compare/v1.6.0...v1.7.0
[1.6.0]: https://github.com/nessie1980/SharePortfolioManager-QT/compare/v1.5.0...v1.6.0
[1.5.0]: https://github.com/nessie1980/SharePortfolioManager-QT/compare/v1.4.2...v1.5.0
[1.4.2]: https://github.com/nessie1980/SharePortfolioManager-QT/compare/v1.4.1...v1.4.2
[1.4.1]: https://github.com/nessie1980/SharePortfolioManager-QT/compare/v1.4.0...v1.4.1
[1.4.0]: https://github.com/nessie1980/SharePortfolioManager-QT/compare/v1.3.0...v1.4.0
[1.3.0]: https://github.com/nessie1980/SharePortfolioManager-QT/compare/v1.2.0...v1.3.0
[1.2.0]: https://github.com/nessie1980/SharePortfolioManager-QT/compare/v1.1.0...v1.2.0
[1.1.0]: https://github.com/nessie1980/SharePortfolioManager-QT/compare/v1.0.2...v1.1.0
[1.0.2]: https://github.com/nessie1980/SharePortfolioManager-QT/compare/v1.0.1...v1.0.2
[1.0.1]: https://github.com/nessie1980/SharePortfolioManager-QT/compare/v1.0.0...v1.0.1

find_package(Qt6 REQUIRED COMPONENTS Test Sql)

# ── DocumentClassifier tests ───────────────────────────────────────────────────
qt_add_executable(tst_documentclassifier tst_documentclassifier.cpp
    ../../app/config/DocumentsConfig.cpp
    ../../app/utils/DocumentClassifier.cpp
)

target_include_directories(tst_documentclassifier
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
            ${CMAKE_CURRENT_SOURCE_DIR}/../../libs/parser/src
)

target_link_libraries(tst_documentclassifier
    PRIVATE Qt6::Test Parser
)

set_target_properties(tst_documentclassifier PROPERTIES AUTOMOC ON)

add_test(NAME tst_documentclassifier COMMAND tst_documentclassifier)

# ── ShareCalculator tests ──────────────────────────────────────────────────────
# Nachgetragen 05.08.2026: tst_sharecalculator.cpp lag seit seiner Entstehung
# im Repository, war aber in keiner CMakeLists.txt eingetragen — der Test wurde
# nie gebaut und lief nie mit, obwohl TESTING.md ihn unter den ausführbaren
# Tests auffuehrt. Aufgefallen beim Anlegen von tst_portfolioseriescalculator.
#
# ShareCalculator::compute() liest Käufe, Verkäufe, Brokerage, Dividenden und
# (seit Phase 2 der Aktiensplit-Behandlung, 07.08.2026) Splits frisch aus den
# Repositories, daher braucht das Ziel die Repository- und Model-Quellen
# sowie die Database-Bibliothek. Der Test selbst arbeitet gegen eine
# In-Memory-SQLite-Datenbank.
qt_add_executable(tst_sharecalculator tst_sharecalculator.cpp
    ../../app/models/BuyObject.cpp
    ../../app/models/SaleObject.cpp
    ../../app/models/BrokerageObject.cpp
    ../../app/models/DividendObject.cpp
    ../../app/models/ShareSplitObject.cpp
    ../../app/repositories/BuyRepository.cpp
    ../../app/repositories/SaleRepository.cpp
    ../../app/repositories/BrokerageRepository.cpp
    ../../app/repositories/DividendRepository.cpp
    ../../app/repositories/ShareSplitRepository.cpp
    ../../app/utils/ShareCalculator.cpp
    ../../app/utils/ShareSplitAdjuster.cpp
)

target_include_directories(tst_sharecalculator
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/core
)

set_target_properties(tst_sharecalculator PROPERTIES AUTOMOC ON)

target_link_libraries(tst_sharecalculator
    PRIVATE Qt6::Test Qt6::Sql Database
)

add_test(NAME tst_sharecalculator COMMAND tst_sharecalculator)

# ── PortfolioSeriesCalculator tests ────────────────────────────────────────────
# Rechenkern des Depotwert-Charts (Feature 05.08.2026). Der Kern selbst ist
# datenbankfrei — alle Eingangsdaten kommen als einfache Structs herein. Die
# Repository- und Model-Quellen hängen nur deshalb mit dran, weil
# PortfolioSeriesCalculator für die Rundung ShareCalculator::roundAway()
# verwendet und ShareCalculator.cpp die Repositories einbindet — seit Phase 2
# der Aktiensplit-Behandlung (07.08.2026) zusätzlich ShareSplitRepository/
# ShareSplitObject/ShareSplitAdjuster. Das ist bewusst so: eine zweite
# Rundungsimplementierung wäre die schlechtere Alternative, weil die
# Cent-Semantik projektweit identisch bleiben muss.
qt_add_executable(tst_portfolioseriescalculator tst_portfolioseriescalculator.cpp
    ../../app/models/BuyObject.cpp
    ../../app/models/SaleObject.cpp
    ../../app/models/BrokerageObject.cpp
    ../../app/models/DividendObject.cpp
    ../../app/models/ShareSplitObject.cpp
    ../../app/repositories/BuyRepository.cpp
    ../../app/repositories/SaleRepository.cpp
    ../../app/repositories/BrokerageRepository.cpp
    ../../app/repositories/DividendRepository.cpp
    ../../app/repositories/ShareSplitRepository.cpp
    ../../app/utils/ShareCalculator.cpp
    ../../app/utils/ShareSplitAdjuster.cpp
    ../../app/utils/PortfolioSeriesCalculator.cpp
)

target_include_directories(tst_portfolioseriescalculator
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/core
)

set_target_properties(tst_portfolioseriescalculator PROPERTIES AUTOMOC ON)

target_link_libraries(tst_portfolioseriescalculator
    PRIVATE Qt6::Test Qt6::Sql Database
)

add_test(NAME tst_portfolioseriescalculator COMMAND tst_portfolioseriescalculator)

# ── ShareUpdateRules tests ─────────────────────────────────────────────────────
# Regelmodul "Tageswert-Historie bei Bestand > 0 erzwingen" (Feature
# 06.08.2026). ShareUpdateRules.h ist header-only und ohne Zustand — das Ziel
# braucht daher nur ShareObject.cpp (wegen der Enum-Definitionen in
# ShareObject.h) und weder Qt6::Sql noch die Database-Bibliothek. Bewusst der
# schlankeste Testlauf im Projekt: die Regel wird an drei Stellen gebraucht
# (ViewShareEdit, PresenterShareEdit, MainWindow) und muss deshalb losgelöst
# von allen dreien prüfbar bleiben.
qt_add_executable(tst_shareupdaterules tst_shareupdaterules.cpp
    ../../app/models/ShareObject.cpp
)

target_include_directories(tst_shareupdaterules
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
)

set_target_properties(tst_shareupdaterules PROPERTIES AUTOMOC ON)

target_link_libraries(tst_shareupdaterules
    PRIVATE Qt6::Test
)

add_test(NAME tst_shareupdaterules COMMAND tst_shareupdaterules)

# ── ShareSplitAdjuster tests ────────────────────────────────────────────────────
# Rechenkern der Aktiensplit-Behandlung, Phase 1 (siehe ARCHITECTURE.md,
# "Offene Punkte", "Aktiensplits werden nicht behandelt"). Zustandslos und
# datenbankfrei — ShareSplitObject ist ein reines POCO, daher genügt dessen
# .cpp, ohne Repository- oder Database-Abhängigkeit (gleiche Begründung wie
# bei tst_shareupdaterules oben).
qt_add_executable(tst_sharesplitadjuster tst_sharesplitadjuster.cpp
    ../../app/models/ShareSplitObject.cpp
    ../../app/utils/ShareSplitAdjuster.cpp
)

target_include_directories(tst_sharesplitadjuster
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
)

set_target_properties(tst_sharesplitadjuster PROPERTIES AUTOMOC ON)

target_link_libraries(tst_sharesplitadjuster
    PRIVATE Qt6::Test
)

add_test(NAME tst_sharesplitadjuster COMMAND tst_sharesplitadjuster)

# ── SaleFifoAllocator tests ──────────────────────────────────────────────────────
# Split-bewusste FIFO-Verkaufszuteilung, Phase 2c der Aktiensplit-Behandlung
# (07.08.2026, siehe ARCHITECTURE.md "Offene Punkte"). Ersetzt die vormals
# dreifach duplizierte FIFO-Schleife in PresenterSaleEdit/ViewSaleEdit.
# Zustandslos und datenbankfrei, gleiche Begründung wie bei
# tst_sharesplitadjuster oben — BuyObject.cpp wird nur wegen dessen
# Konstruktor gebraucht, keine Repository- oder Database-Abhängigkeit.
qt_add_executable(tst_salefifoallocator tst_salefifoallocator.cpp
    ../../app/models/BuyObject.cpp
    ../../app/models/ShareSplitObject.cpp
    ../../app/utils/ShareSplitAdjuster.cpp
    ../../app/utils/SaleFifoAllocator.cpp
)

target_include_directories(tst_salefifoallocator
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
)

set_target_properties(tst_salefifoallocator PROPERTIES AUTOMOC ON)

target_link_libraries(tst_salefifoallocator
    PRIVATE Qt6::Test
)

add_test(NAME tst_salefifoallocator COMMAND tst_salefifoallocator)

# ── ShareSplitHint tests ─────────────────────────────────────────────────────────
# Formatierung der Split-Hinweise in den Editier-Dialogen, Phase 3b der
# Aktiensplit-Behandlung (09.08.2026, siehe ARCHITECTURE.md). Ausgelagert,
# weil ViewBuyEdit und ViewSaleEdit denselben Text brauchen — eine zweite
# Kopie wäre der direkte Weg zurück zu dem Problem, das Phase 2c mit der
# dreifach duplizierten FIFO-Schleife aufgeräumt hat.
#
# Zustandslos und datenbankfrei, gleiche Begründung wie bei
# tst_sharesplitadjuster oben. Qt6::Gui wird NICHT gebraucht: der Helfer
# liefert reine Strings, die Darstellung entscheiden die Views.
qt_add_executable(tst_sharesplithint tst_sharesplithint.cpp
    ../../app/models/ShareSplitObject.cpp
    ../../app/utils/ShareSplitAdjuster.cpp
    ../../app/utils/ShareSplitHint.cpp
)

target_include_directories(tst_sharesplithint
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
)

set_target_properties(tst_sharesplithint PROPERTIES AUTOMOC ON)

target_link_libraries(tst_sharesplithint
    PRIVATE Qt6::Test
)

add_test(NAME tst_sharesplithint COMMAND tst_sharesplithint)

# ── SplitPriceJumpDetector tests ─────────────────────────────────────────────────
# Heuristik fuer den "Pruefen"-Knopf im Split-Dialog (13.08.2026, siehe
# ARCHITECTURE.md, "Split-Verhaeltnis: Notation der Bankmitteilungen"):
# vergleicht die gespeicherte Kurshistorie um den Ex-Tag eines Splits mit dem
# erwarteten Umrechnungsfaktor und schlaegt vor, ob
# ShareSplitObject::pricesAdjusted() gesetzt werden sollte.
#
# Zustandslos und datenbankfrei, gleiche Begruendung wie bei
# tst_sharesplitadjuster oben. DailyValuesObject.cpp wird nur wegen dessen
# Konstruktor gebraucht (reines POCO, keine QtSql-Includes) — anders als
# tst_sharecalculator/tst_portfolioseriescalculator braucht dieses Ziel daher
# weder Qt6::Sql noch die Database-Bibliothek.
qt_add_executable(tst_splitpricejumpdetector tst_splitpricejumpdetector.cpp
    ../../app/models/DailyValuesObject.cpp
    ../../app/utils/SplitPriceJumpDetector.cpp
)

target_include_directories(tst_splitpricejumpdetector
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
)

set_target_properties(tst_splitpricejumpdetector PROPERTIES AUTOMOC ON)

target_link_libraries(tst_splitpricejumpdetector
    PRIVATE Qt6::Test
)

add_test(NAME tst_splitpricejumpdetector COMMAND tst_splitpricejumpdetector)

# ── SplitAdjustmentAudit tests ───────────────────────────────────────────────────
# Phase 4 der Aktiensplit-Behandlung (siehe ARCHITECTURE.md, "Offene Punkte",
# "Aktiensplits werden nicht behandelt"): "Automatische Nachprüfung des
# prices_adjusted-Zustands nach jedem Tageswert-Abruf (Kurssprung um den
# Splittag vergleichen) + Startmeldung bei Widerspruch, analog
# warnAboutSharesWithoutDailyValues()". SplitAdjustmentAudit vergleicht dafür
# je Split das gespeicherte ShareSplitObject::pricesAdjusted() mit dem
# Ergebnis von SplitPriceJumpDetector::detect() und meldet Widersprüche —
# schreibt selbst nichts in die Datenbank.
#
# Zustandslos und datenbankfrei, gleiche Begründung wie bei
# tst_splitpricejumpdetector oben.
qt_add_executable(tst_splitadjustmentaudit tst_splitadjustmentaudit.cpp
    ../../app/models/DailyValuesObject.cpp
    ../../app/models/ShareSplitObject.cpp
    ../../app/utils/SplitPriceJumpDetector.cpp
    ../../app/utils/SplitAdjustmentAudit.cpp
)

target_include_directories(tst_splitadjustmentaudit
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
)

set_target_properties(tst_splitadjustmentaudit PROPERTIES AUTOMOC ON)

target_link_libraries(tst_splitadjustmentaudit
    PRIVATE Qt6::Test
)

add_test(NAME tst_splitadjustmentaudit COMMAND tst_splitadjustmentaudit)

# ── DividendVolumeChecker tests ──────────────────────────────────────────────────
# Phase 3 der Ex-Tag-Behandlung bei Dividenden (21.08.2026, siehe
# ARCHITECTURE.md, "Plausibilitätsprüfung der Dividenden-Stückzahl"): prüft die
# im Dividenden-Dialog eingetragene Stückzahl gegen den Bestand des gewählten
# Depots am Ex-Tag. Rechnet über ShareSplitAdjuster auf die heutige Skala und
# zurück auf die Beleg-Skala des Ex-Tags — ein direktes Aufsummieren von Belegen
# unterschiedlicher Datumsstände wäre bei einem Split dazwischen genau um den
# Split-Faktor falsch (derselbe Fallstrick wie in SaleFifoAllocator).
#
# Zustandslos und datenbankfrei, gleiche Begründung wie bei
# tst_sharesplitadjuster oben. BuyObject.cpp/SaleObject.cpp werden nur wegen
# deren Konstruktoren gebraucht (reine POCOs), daher weder Qt6::Sql noch die
# Database-Bibliothek.
qt_add_executable(tst_dividendvolumechecker tst_dividendvolumechecker.cpp
    ../../app/models/BuyObject.cpp
    ../../app/models/SaleObject.cpp
    ../../app/models/ShareSplitObject.cpp
    ../../app/utils/ShareSplitAdjuster.cpp
    ../../app/utils/DividendVolumeChecker.cpp
)

target_include_directories(tst_dividendvolumechecker
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
)

set_target_properties(tst_dividendvolumechecker PROPERTIES AUTOMOC ON)

target_link_libraries(tst_dividendvolumechecker
    PRIVATE Qt6::Test
)

add_test(NAME tst_dividendvolumechecker COMMAND tst_dividendvolumechecker)

# ── DocumentFieldValue ────────────────────────────────────────────────────────
# Umwandlung der Rohwerte aus Documents.xml in Widget-taugliche Werte
# (22.08.2026, siehe ARCHITECTURE.md, "Rohwerte aus Belegen: eine Regel je
# Zieltyp"). Anlass waren zwei Feldfälle: die Ordernummer "670835/66.00" wurde
# zu "670835/66,00", und der DKB-Verkaufsbeleg liefert Datum und Uhrzeit in
# EINEM Fang ("Schlusstag/-Zeit 27.02.2020 19:16:37"), woran die Datumsübernahme
# scheiterte — im Formular blieb still das heutige Datum stehen.
#
# DocumentFieldValue.h ist header-only (wie ShareUpdateRules.h): reine
# Funktionen, kein Zustand, kein Qt-Objekt. Deshalb steht hier keine .cpp und
# im Ziel nur Qt6::Test — der schlankeste Testlauf neben tst_shareupdaterules.
qt_add_executable(tst_documentfieldvalue tst_documentfieldvalue.cpp)

target_include_directories(tst_documentfieldvalue
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
)

set_target_properties(tst_documentfieldvalue PROPERTIES AUTOMOC ON)

target_link_libraries(tst_documentfieldvalue
    PRIVATE Qt6::Test
)

add_test(NAME tst_documentfieldvalue COMMAND tst_documentfieldvalue)

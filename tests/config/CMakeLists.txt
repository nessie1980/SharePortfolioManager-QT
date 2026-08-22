find_package(Qt6 REQUIRED COMPONENTS Test)

qt_add_executable(tst_websitesconfig tst_websitesconfig.cpp
    ../../app/config/WebSitesConfig.cpp
)

target_include_directories(tst_websitesconfig
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
            ${CMAKE_CURRENT_SOURCE_DIR}/../../libs/parser/src
)

target_link_libraries(tst_websitesconfig
    PRIVATE Qt6::Test Parser
)

set_target_properties(tst_websitesconfig PROPERTIES AUTOMOC ON)

add_test(NAME tst_websitesconfig COMMAND tst_websitesconfig)

# ── DocumentsConfig tests ─────────────────────────────────────────────────────
qt_add_executable(tst_documentsconfig tst_documentsconfig.cpp
    ../../app/config/DocumentsConfig.cpp
)

target_include_directories(tst_documentsconfig
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
            ${CMAKE_CURRENT_SOURCE_DIR}/../../libs/parser/src
)

target_link_libraries(tst_documentsconfig
    PRIVATE Qt6::Test Parser
)

set_target_properties(tst_documentsconfig PROPERTIES AUTOMOC ON)

add_test(NAME tst_documentsconfig COMMAND tst_documentsconfig)

# ── Documents.xml (ausgelieferte Datei) ───────────────────────────────────────
# Phase 5 der Ex-Tag-Behandlung (21.08.2026). Anders als tst_documentsconfig,
# das den XML-PARSER mit synthetischen Fixtures prüft, prüft dieses Ziel den
# INHALT der wirklich ausgelieferten app/config/Documents.xml: es lässt die
# dort hinterlegten regulären Ausdrücke über repräsentative Auszüge echter
# Dividendengutschriften laufen (anonymisiert, Format wie `pdftotext -layout`)
# und vergleicht die Treffer mit den Werten auf dem Beleg.
#
# Bis dahin war die ausgelieferte Konfiguration an keiner Stelle getestet —
# ein Tippfehler in einem Regex fiel erst auf, wenn ein Benutzer ein Dokument
# einlas. Läuft mit dem echten ParserLib::Parser, damit auch dessen
# Auswahlregel (FoundIndex, erste nicht-leere Fanggruppe) mitgeprüft ist.
#
# SPM_DOCUMENTS_XML zeigt auf die Datei im Quellbaum: geprüft wird bewusst
# das Original und keine Kopie, die stillschweigend veralten könnte.
# DocumentClassifier ist seit dem 21.08.2026 mit dabei: die Direkte
# Dokumentenerfassung (Drag&Drop) liest WKN und ISIN über ihn und NICHT über
# den ParserLib::Parser. Dass beide Wege aus derselben Regel denselben Wert
# holen, prüft test_capture_extractWknMatchesParserResult_allBanks().
qt_add_executable(tst_documentsxml tst_documentsxml.cpp
    ../../app/config/DocumentsConfig.cpp
    ../../app/utils/DocumentClassifier.cpp
)

target_include_directories(tst_documentsxml
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
            ${CMAKE_CURRENT_SOURCE_DIR}/../../libs/parser/src
)

target_compile_definitions(tst_documentsxml
    PRIVATE SPM_DOCUMENTS_XML="${CMAKE_CURRENT_SOURCE_DIR}/../../app/config/Documents.xml"
)

target_link_libraries(tst_documentsxml
    PRIVATE Qt6::Test Parser
)

set_target_properties(tst_documentsxml PROPERTIES AUTOMOC ON)

add_test(NAME tst_documentsxml COMMAND tst_documentsxml)

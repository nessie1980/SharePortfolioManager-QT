# Eigene, von der App unabhängige Versionsnummer — siehe ARCHITECTURE.md,
# Abschnitt "Versionierung". Gültig nur innerhalb dieses Unterverzeichnisses
# (CMake-Projektvariablen sind pro Verzeichnis-Scope gültig), beeinflusst
# also nicht die PROJECT_VERSION der Root-/app-Ebene.
project(Parser VERSION 1.0.0 LANGUAGES CXX)

add_library(Parser STATIC
    src/DataTypes.h
    src/ParsingValues.h
    src/ParsingValues.cpp
    src/Parser.h
    src/Parser.cpp
    src/JsonObjects/OnVistaObjects.h
    src/JsonObjects/YahooObjects.h
)

# ── Version header (generiert aus ParserVersion.h.in) ──────────────────────
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ParserVersion.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/ParserVersion.h
    @ONLY
)

target_include_directories(Parser
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
           ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(Parser
    PUBLIC  Qt6::Core
            Qt6::Network
    PRIVATE Logger
)

set_target_properties(Parser PROPERTIES AUTOMOC ON)

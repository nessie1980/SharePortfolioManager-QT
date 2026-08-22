find_package(Qt6 REQUIRED COMPONENTS Core Sql)

# Eigene, von der App unabhängige Versionsnummer — siehe ARCHITECTURE.md,
# Abschnitt "Versionierung". Gültig nur innerhalb dieses Unterverzeichnisses
# (CMake-Projektvariablen sind pro Verzeichnis-Scope gültig), beeinflusst
# also nicht die PROJECT_VERSION der Root-/app-Ebene.
project(Database VERSION 1.3.0 LANGUAGES CXX)

qt_add_library(Database STATIC
    Database.h
    Database.cpp
)

# ── Version header (generiert aus DatabaseVersion.h.in) ────────────────────
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/DatabaseVersion.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/DatabaseVersion.h
    @ONLY
)

target_include_directories(Database
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
           ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(Database
    PUBLIC Qt6::Core Qt6::Sql
)

set_target_properties(Database PROPERTIES AUTOMOC ON)

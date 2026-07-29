# Eigene, von der App unabhängige Versionsnummer — siehe ARCHITECTURE.md,
# Abschnitt "Versionierung". Gültig nur innerhalb dieses Unterverzeichnisses
# (CMake-Projektvariablen sind pro Verzeichnis-Scope gültig), beeinflusst
# also nicht die PROJECT_VERSION der Root-/app-Ebene.
project(Logger VERSION 1.0.0 LANGUAGES CXX)

add_library(Logger STATIC
    src/LogEntry.h
    src/LogEntry.cpp
    src/Logger.h
    src/Logger.cpp
)

# ── Version header (generiert aus LoggerVersion.h.in) ──────────────────────
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/LoggerVersion.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/LoggerVersion.h
    @ONLY
)

target_include_directories(Logger
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(Logger
    PUBLIC
        Qt6::Core
        Qt6::Gui
)

# Enable Qt's MOC for Q_OBJECT macros
set_target_properties(Logger PROPERTIES AUTOMOC ON)

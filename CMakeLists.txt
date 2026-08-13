cmake_minimum_required(VERSION 3.21)

project(SharePortfolioManager
    VERSION 1.14.2
    DESCRIPTION "Share Portfolio Manager"
    LANGUAGES CXX
)

# ── C++ Standard ────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Qt ──────────────────────────────────────────────────────────────────────
# Set this to your Qt installation path, e.g.:
#   Windows: C:/Qt/6.7.0/msvc2019_64
#   Linux:   /usr/lib/x86_64-linux-gnu/cmake/Qt6  (or via apt: qt6-base-dev)
# Can also be passed via: cmake -DCMAKE_PREFIX_PATH=/path/to/Qt6 ..
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Sql
    Network
    Multimedia
    Charts              # ChartForm — Aktien-Chart-Tab in ShareDetailsForm
    LinguistTools      # lupdate / lrelease for i18n
    OPTIONAL_COMPONENTS
    Pdf
    PdfWidgets
)

if(Qt6PdfWidgets_FOUND)
    message(STATUS "Qt6PdfWidgets found — native PDF viewer enabled")
else()
    message(STATUS "Qt6PdfWidgets not found — PDF preview uses pdftoppm fallback")
endif()

qt_standard_project_setup()

# ── Global compile options ───────────────────────────────────────────────────
if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ── Platform-specific settings ───────────────────────────────────────────────
if(WIN32)
    # Avoid Windows.h min/max macros conflicting with std::min/max
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# ── Build type default ───────────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# ── Output directories ───────────────────────────────────────────────────────
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ── Subdirectories ──────────────────────────────────────────────────────────
add_subdirectory(libs/logger)
add_subdirectory(libs/parser)
add_subdirectory(app/core)
add_subdirectory(app)

# ── Tools ────────────────────────────────────────────────────────────────────
add_subdirectory(tools/xml-importer)

# ── Tests ────────────────────────────────────────────────────────────────────
enable_testing()
add_subdirectory(tests/logger)
add_subdirectory(tests/parser)
add_subdirectory(tests/repositories)
add_subdirectory(tests/utils)
add_subdirectory(tests/database)
add_subdirectory(tests/app)
add_subdirectory(tests/config)
add_subdirectory(tests/forms)
add_subdirectory(tests/xml-importer)

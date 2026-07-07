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

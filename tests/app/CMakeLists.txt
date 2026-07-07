find_package(Qt6 REQUIRED COMPONENTS Test Sql Widgets)

qt_add_executable(tst_appstartup tst_appstartup.cpp
    ../../app/AppStartup.cpp
    ../../app/config/AppSettings.cpp
)

target_include_directories(tst_appstartup
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
)

target_link_libraries(tst_appstartup
    PRIVATE Qt6::Test Qt6::Sql Qt6::Widgets Database
)

set_target_properties(tst_appstartup PROPERTIES AUTOMOC ON)

add_test(NAME tst_appstartup COMMAND tst_appstartup)

# ── IconProvider tests ────────────────────────────────────────────────────────
qt_add_resources(ICON_TEST_RESOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/resources/resources.qrc
)

qt_add_executable(tst_iconprovider tst_iconprovider.cpp
    ../../app/IconProvider.cpp
    ${ICON_TEST_RESOURCES}
)

target_include_directories(tst_iconprovider
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
)

target_link_libraries(tst_iconprovider
    PRIVATE Qt6::Test Qt6::Widgets
)

set_target_properties(tst_iconprovider PROPERTIES AUTOMOC ON)

add_test(NAME tst_iconprovider COMMAND tst_iconprovider)

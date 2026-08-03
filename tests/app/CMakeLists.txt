find_package(Qt6 REQUIRED COMPONENTS Test Sql Widgets Network)

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

# ── SingleInstanceGuard tests ─────────────────────────────────────────────────
# Nur SingleInstanceGuard::buildServerName() (reine String-Logik) ist hier
# unit-getestet — tryAcquire()/activationRequested() selbst (echtes
# QLockFile + QLocalServer/QLocalSocket über mehrere Prozesse) bewusst
# NICHT: es gibt keinen sauberen Weg, zwei echte, unabhängige
# Prozessinstanzen deterministisch in einem einzelnen QTest-Lauf zu
# simulieren, analog zu anderen bereits akzeptierten Testlücken bei echten
# System-Interaktionen in diesem Projekt (z. B. QDialog::exec(), echte
# QSoundEffect-Wiedergabe). Siehe ARCHITECTURE.md, "Die Anwendung darf nur
# einmal gestartet werden". Braucht deshalb nur Qt6::Network (für
# QLockFile/QLocalServer/QLocalSocket-Typen im Header), kein Qt6::Widgets.
qt_add_executable(tst_singleinstanceguard tst_singleinstanceguard.cpp
    ../../app/core/SingleInstanceGuard.h
    ../../app/core/SingleInstanceGuard.cpp
)

target_include_directories(tst_singleinstanceguard
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
)

target_link_libraries(tst_singleinstanceguard
    PRIVATE Qt6::Test Qt6::Network
)

set_target_properties(tst_singleinstanceguard PROPERTIES AUTOMOC ON)

add_test(NAME tst_singleinstanceguard COMMAND tst_singleinstanceguard)

# tools/xml-importer/CMakeLists.txt
#
# Eigenständiges Console-Tool zum einmaligen Import der alten C#-Portfolio.xml
# in eine spm-qt SQLite-Datenbank. Nutzt dieselben Models/Repositories wie die
# Hauptanwendung (gleiche .cpp-Dateien werden hier separat mitkompiliert, analog
# zum Muster, das die Unit-Tests in tests/repositories/ und tests/forms/ bereits
# verwenden), ist aber ein eigenständiges Programm — kein Bestandteil des
# SharePortfolioManager-Targets.
#
# Aufruf:
#   spm-xml-importer <input.xml> <portfolio.db> [--dry-run] [--log <path>]

find_package(Qt6 REQUIRED COMPONENTS Core Sql)

qt_add_executable(spm-xml-importer
    main.cpp
    XmlPortfolioParser.h
    XmlPortfolioParser.cpp
    PortfolioImporter.h
    PortfolioImporter.cpp
    ImportLogger.h
    ImportLogger.cpp

    # ── Models/Repositories der Hauptanwendung wiederverwendet ─────────────
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/models/ShareObject.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/models/BuyObject.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/models/SaleObject.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/models/DividendObject.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/models/BrokerageObject.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/models/DailyValuesObject.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/repositories/ShareRepository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/repositories/BuyRepository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/repositories/SaleRepository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/repositories/DividendRepository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/repositories/BrokerageRepository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/repositories/DailyValuesRepository.cpp
)

target_include_directories(spm-xml-importer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../app
)

target_link_libraries(spm-xml-importer
    PRIVATE
        Qt6::Core
        Qt6::Sql
        Database
)

# Reines Konsolenprogramm — kein WIN32_EXECUTABLE, AUTOMOC für die Qt-Klassen
# in den wiederverwendeten Models/Repositories nicht erforderlich (keine
# Q_OBJECT-Makros dort), aber unschädlich falls künftig welche hinzukommen.
set_target_properties(spm-xml-importer PROPERTIES
    AUTOMOC ON
)

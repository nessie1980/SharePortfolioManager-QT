# tests/xml-importer/CMakeLists.txt
find_package(Qt6 REQUIRED COMPONENTS Test Sql)

# ── XmlPortfolioParser tests (reiner Parser, keine DB) ─────────────────────────
qt_add_executable(tst_xmlportfolioparser tst_xmlportfolioparser.cpp
    ../../tools/xml-importer/XmlPortfolioParser.cpp
)

target_include_directories(tst_xmlportfolioparser
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/xml-importer
)

target_link_libraries(tst_xmlportfolioparser
    PRIVATE Qt6::Test Qt6::Core
)

add_test(NAME tst_xmlportfolioparser COMMAND tst_xmlportfolioparser)

# ── PortfolioImporter tests (gegen In-Memory-SQLite) ────────────────────────────
qt_add_executable(tst_portfolioimporter tst_portfolioimporter.cpp
    ../../tools/xml-importer/XmlPortfolioParser.cpp
    ../../tools/xml-importer/PortfolioImporter.cpp
    ../../tools/xml-importer/ImportLogger.cpp
    ../../app/models/ShareObject.cpp
    ../../app/models/BuyObject.cpp
    ../../app/models/SaleObject.cpp
    ../../app/models/DividendObject.cpp
    ../../app/models/BrokerageObject.cpp
    ../../app/models/DailyValuesObject.cpp
    ../../app/repositories/ShareRepository.cpp
    ../../app/repositories/BuyRepository.cpp
    ../../app/repositories/SaleRepository.cpp
    ../../app/repositories/DividendRepository.cpp
    ../../app/repositories/BrokerageRepository.cpp
    ../../app/repositories/DailyValuesRepository.cpp
)

target_include_directories(tst_portfolioimporter
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/xml-importer
        ${CMAKE_CURRENT_SOURCE_DIR}/../../app
)

target_link_libraries(tst_portfolioimporter
    PRIVATE Qt6::Test Qt6::Sql Database
)

add_test(NAME tst_portfolioimporter COMMAND tst_portfolioimporter)

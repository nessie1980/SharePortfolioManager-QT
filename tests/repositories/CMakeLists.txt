find_package(Qt6 REQUIRED COMPONENTS Test Sql)

# ── BuyRepository tests ───────────────────────────────────────────────────────
qt_add_executable(tst_buyrepository tst_buyrepository.cpp
    ../../app/models/BuyObject.cpp
    ../../app/models/BrokerageObject.cpp
    ../../app/repositories/BuyRepository.cpp
    ../../app/repositories/BrokerageRepository.cpp
)

target_include_directories(tst_buyrepository
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
)

target_link_libraries(tst_buyrepository
    PRIVATE Qt6::Test Qt6::Sql Database
)

add_test(NAME tst_buyrepository COMMAND tst_buyrepository)

# ── SaleRepository tests ──────────────────────────────────────────────────────
qt_add_executable(tst_salerepository tst_salerepository.cpp
    ../../app/models/BuyObject.cpp
    ../../app/models/SaleObject.cpp
    ../../app/models/BrokerageObject.cpp
    ../../app/repositories/BuyRepository.cpp
    ../../app/repositories/SaleRepository.cpp
    ../../app/repositories/BrokerageRepository.cpp
)

target_include_directories(tst_salerepository
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
)

target_link_libraries(tst_salerepository
    PRIVATE Qt6::Test Qt6::Sql Database
)

add_test(NAME tst_salerepository COMMAND tst_salerepository)

# ── DividendRepository tests ──────────────────────────────────────────────────
qt_add_executable(tst_dividendrepository tst_dividendrepository.cpp
    ../../app/models/DividendObject.cpp
    ../../app/repositories/DividendRepository.cpp
)

target_include_directories(tst_dividendrepository
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
)

target_link_libraries(tst_dividendrepository
    PRIVATE Qt6::Test Qt6::Sql Database
)

add_test(NAME tst_dividendrepository COMMAND tst_dividendrepository)

# ── ShareRepository tests ─────────────────────────────────────────────────────
qt_add_executable(tst_sharerepository tst_sharerepository.cpp
    ../../app/models/ShareObject.cpp
    ../../app/repositories/ShareRepository.cpp
)

target_include_directories(tst_sharerepository
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
)

target_link_libraries(tst_sharerepository
    PRIVATE Qt6::Test Qt6::Sql Database
)

add_test(NAME tst_sharerepository COMMAND tst_sharerepository)

# ── BrokerageRepository tests ─────────────────────────────────────────────────
qt_add_executable(tst_brokeragerepository tst_brokeragerepository.cpp
    ../../app/models/BrokerageObject.cpp
    ../../app/repositories/BrokerageRepository.cpp
)

target_include_directories(tst_brokeragerepository
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
)

target_link_libraries(tst_brokeragerepository
    PRIVATE Qt6::Test Qt6::Sql Database
)

add_test(NAME tst_brokeragerepository COMMAND tst_brokeragerepository)

# ── DailyValuesRepository tests ───────────────────────────────────────────────
qt_add_executable(tst_dailyvaluesrepository tst_dailyvaluesrepository.cpp
    ../../app/models/DailyValuesObject.cpp
    ../../app/repositories/DailyValuesRepository.cpp
)

target_include_directories(tst_dailyvaluesrepository
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
)

target_link_libraries(tst_dailyvaluesrepository
    PRIVATE Qt6::Test Qt6::Sql Database
)

add_test(NAME tst_dailyvaluesrepository COMMAND tst_dailyvaluesrepository)

find_package(Qt6 REQUIRED COMPONENTS Test Sql)

# ── ShareCalculator tests ─────────────────────────────────────────────────────
qt_add_executable(tst_sharecalculator tst_sharecalculator.cpp
    ../../app/utils/ShareCalculator.cpp
    ../../app/models/BuyObject.cpp
    ../../app/models/SaleObject.cpp
    ../../app/models/BrokerageObject.cpp
    ../../app/models/DividendObject.cpp
    ../../app/repositories/BuyRepository.cpp
    ../../app/repositories/SaleRepository.cpp
    ../../app/repositories/BrokerageRepository.cpp
    ../../app/repositories/DividendRepository.cpp
)

target_include_directories(tst_sharecalculator
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
)

target_link_libraries(tst_sharecalculator
    PRIVATE Qt6::Test Qt6::Sql Database
)

add_test(NAME tst_sharecalculator COMMAND tst_sharecalculator)

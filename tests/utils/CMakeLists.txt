find_package(Qt6 REQUIRED COMPONENTS Test Sql)

# ── DocumentClassifier tests ───────────────────────────────────────────────────
qt_add_executable(tst_documentclassifier tst_documentclassifier.cpp
    ../../app/config/DocumentsConfig.cpp
    ../../app/utils/DocumentClassifier.cpp
)

target_include_directories(tst_documentclassifier
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
            ${CMAKE_CURRENT_SOURCE_DIR}/../../libs/parser/src
)

target_link_libraries(tst_documentclassifier
    PRIVATE Qt6::Test Parser
)

set_target_properties(tst_documentclassifier PROPERTIES AUTOMOC ON)

add_test(NAME tst_documentclassifier COMMAND tst_documentclassifier)

# ── ShareCalculator tests ──────────────────────────────────────────────────────
# Nachgetragen 05.08.2026: tst_sharecalculator.cpp lag seit seiner Entstehung
# im Repository, war aber in keiner CMakeLists.txt eingetragen — der Test wurde
# nie gebaut und lief nie mit, obwohl TESTING.md ihn unter den ausführbaren
# Tests auffuehrt. Aufgefallen beim Anlegen von tst_portfolioseriescalculator.
#
# ShareCalculator::compute() liest Käufe, Verkäufe, Brokerage und Dividenden
# frisch aus den Repositories, daher braucht das Ziel die Repository- und
# Model-Quellen sowie die Database-Bibliothek. Der Test selbst arbeitet gegen
# eine In-Memory-SQLite-Datenbank.
qt_add_executable(tst_sharecalculator tst_sharecalculator.cpp
    ../../app/models/BuyObject.cpp
    ../../app/models/SaleObject.cpp
    ../../app/models/BrokerageObject.cpp
    ../../app/models/DividendObject.cpp
    ../../app/repositories/BuyRepository.cpp
    ../../app/repositories/SaleRepository.cpp
    ../../app/repositories/BrokerageRepository.cpp
    ../../app/repositories/DividendRepository.cpp
    ../../app/utils/ShareCalculator.cpp
)

target_include_directories(tst_sharecalculator
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/core
)

set_target_properties(tst_sharecalculator PROPERTIES AUTOMOC ON)

target_link_libraries(tst_sharecalculator
    PRIVATE Qt6::Test Qt6::Sql Database
)

add_test(NAME tst_sharecalculator COMMAND tst_sharecalculator)

# ── PortfolioSeriesCalculator tests ────────────────────────────────────────────
# Rechenkern des Depotwert-Charts (Feature 05.08.2026). Der Kern selbst ist
# datenbankfrei — alle Eingangsdaten kommen als einfache Structs herein. Die
# Repository- und Model-Quellen hängen nur deshalb mit dran, weil
# PortfolioSeriesCalculator für die Rundung ShareCalculator::roundAway()
# verwendet und ShareCalculator.cpp die Repositories einbindet. Das ist
# bewusst so: eine zweite Rundungsimplementierung wäre die schlechtere
# Alternative, weil die Cent-Semantik projektweit identisch bleiben muss.
qt_add_executable(tst_portfolioseriescalculator tst_portfolioseriescalculator.cpp
    ../../app/models/BuyObject.cpp
    ../../app/models/SaleObject.cpp
    ../../app/models/BrokerageObject.cpp
    ../../app/models/DividendObject.cpp
    ../../app/repositories/BuyRepository.cpp
    ../../app/repositories/SaleRepository.cpp
    ../../app/repositories/BrokerageRepository.cpp
    ../../app/repositories/DividendRepository.cpp
    ../../app/utils/ShareCalculator.cpp
    ../../app/utils/PortfolioSeriesCalculator.cpp
)

target_include_directories(tst_portfolioseriescalculator
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
            ${CMAKE_CURRENT_SOURCE_DIR}/../../app/core
)

set_target_properties(tst_portfolioseriescalculator PROPERTIES AUTOMOC ON)

target_link_libraries(tst_portfolioseriescalculator
    PRIVATE Qt6::Test Qt6::Sql Database
)

add_test(NAME tst_portfolioseriescalculator COMMAND tst_portfolioseriescalculator)

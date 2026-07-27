find_package(Qt6 REQUIRED COMPONENTS Test)

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

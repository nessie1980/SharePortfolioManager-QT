find_package(Qt6 REQUIRED COMPONENTS Test Sql)

qt_add_executable(tst_database tst_database.cpp)

target_include_directories(tst_database
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../app
)

target_link_libraries(tst_database
    PRIVATE Qt6::Test Qt6::Sql Database
)

add_test(NAME tst_database COMMAND tst_database)

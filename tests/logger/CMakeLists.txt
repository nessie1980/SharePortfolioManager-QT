find_package(Qt6 REQUIRED COMPONENTS Test)

qt_add_executable(tst_logger tst_logger.cpp)

target_link_libraries(tst_logger
    PRIVATE
        Qt6::Test
        Logger
)

set_target_properties(tst_logger PROPERTIES AUTOMOC ON)

add_test(NAME tst_logger COMMAND tst_logger)

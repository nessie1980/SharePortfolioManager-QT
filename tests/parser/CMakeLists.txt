find_package(Qt6 REQUIRED COMPONENTS Test Network)

qt_add_executable(tst_parser tst_parser.cpp)

target_link_libraries(tst_parser
    PRIVATE
        Qt6::Test
        Qt6::Network
        Parser
        Logger
)

set_target_properties(tst_parser PROPERTIES AUTOMOC ON)

add_test(NAME tst_parser COMMAND tst_parser)

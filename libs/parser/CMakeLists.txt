add_library(Parser STATIC
    src/DataTypes.h
    src/ParsingValues.h
    src/ParsingValues.cpp
    src/Parser.h
    src/Parser.cpp
    src/JsonObjects/OnVistaObjects.h
    src/JsonObjects/YahooObjects.h
)

target_include_directories(Parser
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(Parser
    PUBLIC  Qt6::Core
            Qt6::Network
    PRIVATE Logger
)

set_target_properties(Parser PROPERTIES AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Sql)

qt_add_library(Database STATIC
    Database.h
    Database.cpp
)

target_include_directories(Database
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Database
    PUBLIC Qt6::Core Qt6::Sql
)

set_target_properties(Database PROPERTIES AUTOMOC ON)

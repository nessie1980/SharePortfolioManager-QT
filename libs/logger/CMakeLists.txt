add_library(Logger STATIC
    src/LogEntry.h
    src/LogEntry.cpp
    src/Logger.h
    src/Logger.cpp
)

target_include_directories(Logger
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(Logger
    PUBLIC
        Qt6::Core
        Qt6::Gui
)

# Enable Qt's MOC for Q_OBJECT macros
set_target_properties(Logger PROPERTIES AUTOMOC ON)

cmake_minimum_required(VERSION 3.20)
get_filename_component(CURRENT_DIR ${CMAKE_CURRENT_SOURCE_DIR} NAME)

project(${CURRENT_DIR})

set(${PROJECT_NAME}_SHARED OFF CACHE BOOL "" FORCE)

set(DPS_LIBRARYS
    MLog::MLog
)

cpp_library(${PROJECT_NAME})

if(TARGET Qt6::Core)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Core)
endif()

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()

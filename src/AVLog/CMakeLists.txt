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
    # 设置 Visual Studio 工程的调试目录和解决方案分组。
    # set_property(DIRECTORY ${CMAKE_BINARY_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
    set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${OUT_RUN_PATH})
    set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "XVIEW")

    # set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "/GL")
    # set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/LTCG")

    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()

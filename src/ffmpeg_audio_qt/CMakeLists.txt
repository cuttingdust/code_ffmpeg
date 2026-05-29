cmake_minimum_required(VERSION 3.20)
get_filename_component(CURRENT_DIR ${CMAKE_CURRENT_SOURCE_DIR} NAME)

project(${CURRENT_DIR})

set(DPS_LIBRARYS
    ${FFMEPG_MOUDLES}
    ${QT6_MOUDLES}
)

cpp_execute(${PROJECT_NAME})

if(MSVC)
    set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${OUT_RUN_PATH})
    set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "XAUDIO_TEST")
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()

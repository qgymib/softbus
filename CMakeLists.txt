cmake_minimum_required(VERSION 3.2)
project(softbus C ASM)

function(target_setup_w4 TARGET)
    if (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${TARGET} PRIVATE /W4 /WX)
    else ()
        target_compile_options(${TARGET} PRIVATE -Wall -Wextra -Werror)
    endif ()
endfunction()

set_property(SOURCE src/call_in_stack.s
    APPEND PROPERTY COMPILE_OPTIONS "-x" "assembler-with-cpp")

add_library(${PROJECT_NAME}
    src/call_in_stack.s
    src/call_in_stack.c
    src/softbus.c)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_setup_w4(${PROJECT_NAME})

add_subdirectory(third_party/libev)
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        ev)

# build test
if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
	include(CTest)
endif()
if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
	add_subdirectory(test)
endif()

set(DICTU_CLI_SRC main.c linenoise/linenoise.c linenoise/linenoise.h linenoise/stringbuf.c linenoise/stringbuf.h linenoise/utf8.c linenoise/utf8.h)
if(EXEC_RELOCATE)
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
if(NOT WIN32)
    list(FILTER DICTU_CLI_SRC EXCLUDE REGEX "linenoise-win32.c")
endif()

add_compile_definitions(USE_UTF8)
add_executable(dictu ${DICTU_CLI_SRC})
 execute_process(
    COMMAND git log -1 --format=%H
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
add_compile_definitions(GIT_HASH="${GIT_HASH}")
if(CMAKE_BUILD_TYPE MATCHES Debug)
    add_compile_definitions(DEBUG DEBUG_STRESS_GC DEBUG_FINAL_MEM)
endif()

target_include_directories(dictu PUBLIC ${INCLUDE_DIR})
target_link_libraries(dictu dictu_api_static)
if(LINUX AND CMAKE_C_COMPILER_ID STREQUAL "Clang")
    # Clang needs this otherwise ld will fail on linux since the lib is build with -flto
    set(CMAKE_C_FLAGS_RELEASE "-flto")
    set(CMAKE_C_FLAGS_DEBUG "-O0 -g")
# set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
#    set(CMAKE_EXE_LINKER_FLAGS_DEBUG
#     "${CMAKE_EXE_LINKER_FLAGS_DDEBUG} -fsanitize=address" CACHE STRING
#     "Linker flags to be used to create executables for Asan build type." FORCE)

# set(CMAKE_SHARED_LINKER_FLAGS_DEBUG
#     "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -fsanitize=address" CACHE STRING
#     "Linker lags to be used to create shared libraries for Asan build type." FORCE)
 
endif()
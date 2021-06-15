cmake_minimum_required (VERSION 3.0)
project(ggmorse VERSION 0.1.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS "on")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(GGMORSE_STANDALONE ON)
    include(cmake/GitVars.cmake)
    include(cmake/BuildTypes.cmake)

    # configure project version
    configure_file(${CMAKE_SOURCE_DIR}/README-tmpl.md ${CMAKE_SOURCE_DIR}/README.md @ONLY)
else()
    set(GGMORSE_STANDALONE OFF)
endif()

if (EMSCRIPTEN)
    set(GGMORSE_SUPPORT_SDL2_DEFAULT ON)
    set(BUILD_SHARED_LIBS_DEFAULT OFF)

    option(GGMORSE_WASM_SINGLE_FILE "ggmorse: embed WASM inside the generated ggmorse.js" ON)
else()
    set(GGMORSE_SUPPORT_SDL2_DEFAULT ON)
    if (MINGW)
        set(BUILD_SHARED_LIBS_DEFAULT OFF)
    else()
        set(BUILD_SHARED_LIBS_DEFAULT ON)
    endif()
endif()

# options

option(BUILD_SHARED_LIBS                "ggmorse: build shared libs" ${BUILD_SHARED_LIBS_DEFAULT})
option(USE_FINDSDL2                     "ggmorse: Use the FindSDL2.cmake script" OFF)

option(GGMORSE_ALL_WARNINGS             "ggmorse: enable all compiler warnings" ON)
option(GGMORSE_ALL_WARNINGS_3RD_PARTY   "ggmorse: enable all compiler warnings in 3rd party libs" ON)

option(GGMORSE_SANITIZE_THREAD          "ggmorse: enable thread sanitizer" OFF)
option(GGMORSE_SANITIZE_ADDRESS         "ggmorse: enable address sanitizer" OFF)
option(GGMORSE_SANITIZE_UNDEFINED       "ggmorse: enable undefined sanitizer" OFF)

option(GGMORSE_SUPPORT_SDL2             "ggmorse: support for libSDL2" ${GGMORSE_SUPPORT_SDL2_DEFAULT})

option(GGMORSE_BUILD_TESTS              "ggmorse: build examples" ${GGMORSE_STANDALONE})
option(GGMORSE_BUILD_EXAMPLES           "ggmorse: build examples" ${GGMORSE_STANDALONE})

option(GGMORSE_MALLOC_COUNT             "ggmorse: count memory allocations" OFF)

# sanitizers

if (GGMORSE_SANITIZE_THREAD)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
endif()

if (GGMORSE_SANITIZE_ADDRESS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
endif()

if (GGMORSE_SANITIZE_UNDEFINED)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
endif()

# dependencies

# main

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (GGMORSE_ALL_WARNINGS)
    if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
    else()
        # todo : windows
    endif()
endif()

add_subdirectory(src)

if (GGMORSE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if (GGMORSE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

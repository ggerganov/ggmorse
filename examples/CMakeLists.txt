# dependencies

find_package(Threads REQUIRED)

if (GGMORSE_SUPPORT_SDL2)
    # SDL2

    if (EMSCRIPTEN)
        #-s USE_PTHREADS=1 \
        set(CMAKE_CXX_FLAGS " \
        --bind \
        -s TOTAL_MEMORY=67108864 \
        -s USE_SDL=2 \
        -s ASSERTIONS=1 \
        -s DISABLE_EXCEPTION_CATCHING=0 \
        -s 'EXTRA_EXPORTED_RUNTIME_METHODS=[\"writeArrayToMemory\"]' \
        ")

        unset(SDL2_INCLUDE_DIRS)
        unset(SDL2_LIBRARIES)
    endif()

    if (NOT EMSCRIPTEN)
        find_package(SDL2)

        if (NOT USE_FINDSDL2 AND NOT SDL2_FOUND)
            message(WARNING "Unable to find SDL2 library. It is either not installed or CMake cannot find it."
                " In the latter case, setting the USE_FINDSDL2 variable might help:\n"
                "   $ cmake -D USE_FINDSDL2 .."
                )

            message(FATAL_ERROR "Aborting")
        endif()

        string(STRIP "${SDL2_LIBRARIES}" SDL2_LIBRARIES)

        message(STATUS "SDL2_INCLUDE_DIRS = ${SDL2_INCLUDE_DIRS}")
        message(STATUS "SDL2_LIBRARIES = ${SDL2_LIBRARIES}")
    endif()
endif()

# third-party

add_subdirectory(third-party)

# helper libraries

add_library(ggmorse-common
    ggmorse-common.cpp
    )

target_link_libraries(ggmorse-common PRIVATE
    ${CMAKE_DL_LIBS}
    )

if (MINGW)
    target_link_libraries(ggmorse-common PUBLIC
        stdc++
    )
endif()

add_subdirectory(malloc-count)

if (GGMORSE_SUPPORT_SDL2)
    # ggmorse-common-sdl2

    add_library(ggmorse-common-sdl2
        ggmorse-common-sdl2.cpp
        )

    target_include_directories(ggmorse-common-sdl2 PUBLIC
        ${SDL2_INCLUDE_DIRS}
        )

    target_link_libraries(ggmorse-common-sdl2 PRIVATE
        ggmorse
        imgui-sdl2
        ${SDL2_LIBRARIES}
        )
endif()

# examples

if (EMSCRIPTEN)
else()
    add_subdirectory(ggmorse-to-file)
    add_subdirectory(ggmorse-from-file)
endif()

if (GGMORSE_SUPPORT_SDL2)
    add_subdirectory(ggmorse-gui)
endif()

include(GNUInstallDirs)

install(TARGETS ggmorse-common
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/static
    )

if (GGMORSE_SUPPORT_SDL2)
    install(TARGETS ggmorse-common-sdl2
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/static
        )
endif()

# core

set(TARGET ggmorse)

add_library(${TARGET}
    ggmorse.cpp
    resampler.cpp
    )

set_target_properties(ggmorse PROPERTIES PUBLIC_HEADER ../include/ggmorse/ggmorse.h)

target_include_directories(${TARGET} PUBLIC
    .
    ../include
    )

if (BUILD_SHARED_LIBS)
    target_link_libraries(${TARGET} PUBLIC
        ${CMAKE_DL_LIBS}
        )

    target_compile_definitions(${TARGET} PUBLIC
        GGMORSE_SHARED
        )
endif()

if (MINGW)
    target_link_libraries(${TARGET} PUBLIC
        stdc++
        )
endif()

include(GNUInstallDirs)

install(TARGETS ${TARGET}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/static
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}
    )

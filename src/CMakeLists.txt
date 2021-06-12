# core

set(TARGET ggmorse)

add_library(${TARGET}
    ggmorse.cpp
    resampler.cpp
    )

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

install(TARGETS ${TARGET}
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib/static
    )

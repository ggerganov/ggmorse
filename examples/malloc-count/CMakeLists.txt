add_library(malloc-count STATIC
    malloc_count.c
    )

target_sources(malloc-count INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/malloc_count.c
    )

if (GGMORSE_MALLOC_COUNT)
    target_compile_definitions(malloc-count INTERFACE __USE_MALLOC_COUNT__)
endif()

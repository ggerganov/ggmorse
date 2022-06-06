set(TARGET ggmorse-to-file)

add_executable(${TARGET} main.cpp)

target_include_directories(${TARGET} PRIVATE
    ..
    )

target_link_libraries(${TARGET} PRIVATE
    ggmorse
    ggmorse-common
    ${CMAKE_THREAD_LIBS_INIT}
    )

install(TARGETS ${TARGET} RUNTIME DESTINATION bin)

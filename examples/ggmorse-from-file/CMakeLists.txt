set(TARGET ggmorse-from-file)

add_executable(${TARGET} main.cpp)

target_include_directories(${TARGET} PRIVATE
    ..
    )

target_link_libraries(${TARGET} PRIVATE
    ggmorse
    ggmorse-common
    ${CMAKE_THREAD_LIBS_INIT}
    )

include(GNUInstallDirs)

install(TARGETS ${TARGET} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

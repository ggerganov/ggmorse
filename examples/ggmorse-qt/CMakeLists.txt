set(TARGET ggmorse-qt)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Multimedia)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(${TARGET}
    main.cpp
    AudioDecoder.cpp
    AudioDecoder.h
    )

qt_add_qml_module(${TARGET}
    URI GGMorse
    VERSION 1.0
    QML_FILES Main.qml
    )

target_link_libraries(${TARGET} PRIVATE
    ggmorse
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia
    )

install(TARGETS ${TARGET} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(NOT ENABLE_PIPEWIRE)
    return()
endif()

add_executable(projectM-Qt-PipeWire
        ConfigFile.h
        ConfigFile.cpp
        PipeWireDeviceChooserDialog.ui
        qprojectM-pipewire.cpp
        QPipeWireDeviceChooser.cpp
        QPipeWireDeviceChooser.hpp
        QPipeWireDeviceModel.cpp
        QPipeWireDeviceModel.hpp
        QPipeWireThread.cpp
        QPipeWireThread.hpp
        )

target_compile_definitions(projectM-Qt-PipeWire
        PRIVATE
        PROJECTM_PREFIX="${CMAKE_INSTALL_PREFIX}"
        )

# Application includes a header file from libprojectM with its full path in the source dir.
target_include_directories(projectM-Qt-PipeWire
        PRIVATE
        "${CMAKE_SOURCE_DIR}"
        )

target_link_libraries(projectM-Qt-PipeWire
        PRIVATE
        projectM-Qt-Common
        PkgConfig::PIPEWIRE
        ${CMAKE_DL_LIBS}
        )

set_target_properties(projectM-Qt-PipeWire PROPERTIES
        OUTPUT_NAME projectM-pipewire
        )

install(TARGETS projectM-Qt-PipeWire
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT Applications
        )

# Install launcher script for Wayland/XWayland window activation
install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/projectM-pipewire-launcher.sh"
        DESTINATION "${CMAKE_INSTALL_BINDIR}"
        RENAME projectM-pipewire-launcher
        COMPONENT Applications
        )

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    install(FILES "${CMAKE_SOURCE_DIR}/src/common/images/icons/prjm16-transparent.svg"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps"
            COMPONENT Applications
            RENAME projectM.svg
            )

    install(FILES projectM-pipewire.desktop
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications"
            COMPONENT Applications
            )

    install(FILES projectM-pipewire.1
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/man/man1"
            COMPONENT Applications
            )
endif()

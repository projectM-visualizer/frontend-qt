add_executable(projectM-Qt-PulseAudio
        ConfigFile.h
        ConfigFile.cpp
        PulseDeviceChooserDialog.ui
        qprojectM-pulseaudio.cpp
        QPulseAudioDeviceChooser.cpp
        QPulseAudioDeviceChooser.hpp
        QPulseAudioDeviceModel.cpp
        QPulseAudioDeviceModel.hpp
        QPulseAudioThread.cpp
        QPulseAudioDeviceModel.hpp
        )

target_compile_definitions(projectM-Qt-PulseAudio
        PRIVATE
        PROJECTM_PREFIX="${CMAKE_INSTALL_PREFIX}"
        )

# Application includes a header file from libprojectM with its full path in the source dir.
target_include_directories(projectM-Qt-PulseAudio
        PRIVATE
        "${CMAKE_SOURCE_DIR}"
        )

target_link_libraries(projectM-Qt-PulseAudio
        PRIVATE
        projectM-Qt-Common
        Pulseaudio::Pulseaudio
        ${CMAKE_DL_LIBS}
        )

install(TARGETS projectM-Qt-PulseAudio
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT Applications
        )

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    install(FILES "${CMAKE_SOURCE_DIR}/src/projectM-qt/images/icons/prjm16-transparent.svg"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps"
            COMPONENT Applications
            RENAME projectM.svg
            )

    install(FILES projectM-pulseaudio.desktop
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications"
            COMPONENT Applications
            )

    install(FILES projectM-pulseaudio.1
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/man/man1"
            COMPONENT Applications
            )
endif()

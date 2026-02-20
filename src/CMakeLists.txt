add_subdirectory(common)

if(ENABLE_PIPEWIRE)
    add_subdirectory(ui-pipewire)
else()
    add_subdirectory(ui-pulseaudio)
endif()

if(ENABLE_JACK)
    add_subdirectory(ui-jack)
endif()

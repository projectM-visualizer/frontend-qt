add_subdirectory(common)

if(ENABLE_PULSEAUDIO)
    add_subdirectory(ui-pulseaudio)
endif()

if(ENABLE_PIPEWIRE)
    add_subdirectory(ui-pipewire)
endif()

if(ENABLE_JACK)
    add_subdirectory(ui-jack)
endif()

if(NOT ENABLE_PULSEAUDIO AND NOT ENABLE_PIPEWIRE AND NOT ENABLE_JACK)
    message(FATAL_ERROR "At least one audio backend must be enabled (ENABLE_PULSEAUDIO, ENABLE_PIPEWIRE, or ENABLE_JACK).")
endif()

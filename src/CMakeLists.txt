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

# Unified executable with runtime backend selection
if(ENABLE_PULSEAUDIO OR ENABLE_PIPEWIRE OR ENABLE_JACK)
    add_subdirectory(ui-unified)
else()
    message(FATAL_ERROR "At least one audio backend must be enabled (ENABLE_PULSEAUDIO, ENABLE_PIPEWIRE, or ENABLE_JACK).")
endif()

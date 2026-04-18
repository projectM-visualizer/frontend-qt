set(UNIFIED_SOURCES
    qprojectM-unified.cpp
)

set(UNIFIED_LINK_LIBS
    projectM-Qt-Common
)

set(UNIFIED_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/src/common
)

if(ENABLE_PIPEWIRE)
    list(APPEND UNIFIED_SOURCES
        ${CMAKE_SOURCE_DIR}/src/ui-pipewire/QPipeWireBackend.cpp
        ${CMAKE_SOURCE_DIR}/src/ui-pipewire/QPipeWireThread.cpp
    )
    list(APPEND UNIFIED_LINK_LIBS PkgConfig::PIPEWIRE)
    list(APPEND UNIFIED_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/src/ui-pipewire)
endif()

if(ENABLE_PULSEAUDIO)
    list(APPEND UNIFIED_SOURCES
        ${CMAKE_SOURCE_DIR}/src/ui-pulseaudio/QPulseAudioBackend.cpp
        ${CMAKE_SOURCE_DIR}/src/ui-pulseaudio/QPulseAudioThread.cpp
    )
    list(APPEND UNIFIED_LINK_LIBS Pulseaudio::Pulseaudio)
    list(APPEND UNIFIED_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/src/ui-pulseaudio)
endif()

if(ENABLE_JACK)
    list(APPEND UNIFIED_SOURCES
        ${CMAKE_SOURCE_DIR}/src/ui-jack/QJackBackend.cpp
    )
    list(APPEND UNIFIED_LINK_LIBS JACK::JACK)
    list(APPEND UNIFIED_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/src/ui-jack)
endif()

add_executable(projectM-Qt-Unified ${UNIFIED_SOURCES})

target_compile_definitions(projectM-Qt-Unified
    PRIVATE
    PROJECTM_PREFIX="${CMAKE_INSTALL_PREFIX}"
)

if(ENABLE_PIPEWIRE)
    target_compile_definitions(projectM-Qt-Unified PRIVATE ENABLE_PIPEWIRE)
endif()
if(ENABLE_PULSEAUDIO)
    target_compile_definitions(projectM-Qt-Unified PRIVATE ENABLE_PULSEAUDIO)
endif()
if(ENABLE_JACK)
    target_compile_definitions(projectM-Qt-Unified PRIVATE ENABLE_JACK)
endif()

target_include_directories(projectM-Qt-Unified PRIVATE ${UNIFIED_INCLUDE_DIRS})

target_link_libraries(projectM-Qt-Unified
    PRIVATE
    ${UNIFIED_LINK_LIBS}
    ${PROJECTM_OPENGL_LIBRARIES}
    ${QT_LINK_TARGETS}
)

set_target_properties(projectM-Qt-Unified PROPERTIES
    OUTPUT_NAME projectM-qt
)

install(TARGETS projectM-Qt-Unified
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT Applications
)

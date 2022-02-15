if(NOT ENABLE_JACK)
    return()
endif()

add_executable(projectM-Qt-JACK
        ConfigFile.h
        ConfigFile.cpp
        qprojectM-jack.cpp
        )

target_compile_definitions(projectM-Qt-JACK
        PRIVATE
        PROJECTM_PREFIX="${CMAKE_INSTALL_PREFIX}"
        RESOURCE_PREFIX="share/projectM"
        )

target_link_libraries(projectM-Qt-JACK
        PRIVATE
        projectM-Qt-Common
        JACK::JACK
        ${CMAKE_DL_LIBS}
        )

set_target_properties(projectM-Qt-JACK PROPERTIES
        OUTPUT_NAME projectM-jack
        )

install(TARGETS projectM-Qt-JACK
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT Applications
        )

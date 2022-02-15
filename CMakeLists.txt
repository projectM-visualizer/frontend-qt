cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

if(CMAKE_VERSION VERSION_LESS 3.19 AND CMAKE_GENERATOR STREQUAL "Xcode")
    message(AUTHOR_WARNING "Using a CMake version before 3.19 with a recent Xcode SDK and the Xcode generator "
            "will likely result in CMake failing to find the AppleClang compiler. Either upgrade CMake to at least "
            "version 3.19 or use a different generator, e.g. \"Unix Makefiles\" or \"Ninja\".")
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_POSITION_INDEPENDENT_CODE YES)

# Automatically create .moc files and run the UIC and resource compilers
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(ENABLE_DEBUG_POSTFIX "Add \"d\" after library names for debug builds" ON)
if(ENABLE_DEBUG_POSTFIX)
    set(CMAKE_DEBUG_POSTFIX d)
endif()

project(projectM-Qt
        LANGUAGES C CXX
        VERSION 1.0.0
        )

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include(GNUInstallDirs)
else()
    set(CMAKE_INSTALL_BINDIR "." STRING CACHE "Install directory for executables")
    set(CMAKE_INSTALL_LIBDIR "." STRING CACHE "Install directory for libraries")
    set(CMAKE_INSTALL_DATAROOTDIR "." STRING CACHE "Install root directory for read-only data files")
endif()

# Feature options
option(ENABLE_SHARED_LINKING "Link the Qt frontend against the shared projectM library." ON)
option(ENABLE_GLES "Enable OpenGL ES support" OFF)
option(ENABLE_JACK "Build JACK-based UI" OFF)

if(ENABLE_DOXYGEN)
    find_package(Doxygen REQUIRED)
endif()

if(ENABLE_GLES)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL Linux)
        message(FATAL_ERROR "OpenGL ES 3 support is currently only available for Linux platforms.")
    endif()

    # We use a local find script for OpenGL::GLES3 until the proposed changes are merged upstream.
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/gles")
    find_package(OpenGL REQUIRED COMPONENTS GLES3)
    if(NOT TARGET OpenGL::GLES3)
        message(FATAL_ERROR "No suitable GLES3 library was found.")
    endif()

    set(PROJECTM_OPENGL_LIBRARIES OpenGL::GLES3)
    set(USE_GLES ON)
else()
    find_package(OpenGL REQUIRED)
    set(PROJECTM_OPENGL_LIBRARIES OpenGL::GL)
    # GLX is required by SOIL2 on platforms with the X Window System (e.g. most Linux distributions)
    if(TARGET OpenGL::GLX)
        list(APPEND PROJECTM_OPENGL_LIBRARIES OpenGL::GLX)
    endif()
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        find_package(GLEW REQUIRED)
        list(APPEND PROJECTM_OPENGL_LIBRARIES GLEW::glew)
    endif()
endif()

find_package(libprojectM REQUIRED)
find_package(Pulseaudio REQUIRED)

if(ENABLE_JACK)
    find_package(JACK REQUIRED)
endif()

set(QT_REQUIRED_COMPONENTS Gui Widgets OpenGL Xml)
if(DEFINED QT_VERSION)
    find_package(Qt${QT_VERSION} REQUIRED COMPONENTS ${QT_REQUIRED_COMPONENTS})
else()
    # Try to determine the Qt version available, starting with Qt6
    message(STATUS "Determining Qt version. To use a specific version, set QT_VERSION to either 5 or 6.")
    find_package(Qt6 QUIET COMPONENTS ${QT_REQUIRED_COMPONENTS})
    if(TARGET Qt6::Core)
        set(QT_VERSION 6)
    else()
        find_package(Qt5 QUIET COMPONENTS ${QT_REQUIRED_COMPONENTS})
        if(TARGET Qt5::Core)
            set(QT_VERSION 5)
        else()
            message(FATAL_ERROR "Could not find either Qt 5 or 6, one is required to build the PulseAudio or JACK UIs.")
        endif()
    endif()
endif()

# OpenGLWidgets were put into their own component since Qt 6.
if(QT_VERSION EQUAL 6)
    list(APPEND QT_REQUIRED_COMPONENTS OpenGLWidgets)
    find_package(Qt6 REQUIRED COMPONENTS OpenGLWidgets)
endif()

set(QT_LINK_TARGETS "")
foreach(component ${QT_REQUIRED_COMPONENTS})
    list(APPEND QT_LINK_TARGETS Qt${QT_VERSION}::${component})
endforeach()

add_subdirectory(src)

message(STATUS "")
message(STATUS "projectM Qt Frontend v${PROJECT_VERSION}")
message(STATUS "==============================================")
message(STATUS "")
message(STATUS "    prefix:                  ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "    compiler:                ${CMAKE_CXX_COMPILER}")
message(STATUS "    cflags:                  ${CMAKE_C_FLAGS}")
message(STATUS "    cxxflags:                ${CMAKE_CXX_FLAGS}")
message(STATUS "    ldflags:                 ${CMAKE_SHARED_LINKER_FLAGS}")
message(STATUS "")
message(STATUS "Features:")
message(STATUS "==============================================")
message(STATUS "")
message(STATUS "    OpenGL ES:               ${ENABLE_GLES}")
message(STATUS "    Link UI with shared lib: ${ENABLE_SHARED_LINKING}")
message(STATUS "")
message(STATUS "Targets and applications:")
message(STATUS "==============================================")
message(STATUS "")
message(STATUS "    PulseAudio UI:           ON")
message(STATUS "        PulseAudio version:  ${PULSEAUDIO_VERSION}")
message(STATUS "    JACK UI:                 ${ENABLE_JACK}")
if(ENABLE_JACK)
    message(STATUS "        JACK version:        ${JACK_VERSION}")
endif()
message(STATUS "    Qt version:              ${Qt${QT_VERSION}_VERSION}")
message(STATUS "")

# Create CPack configuration
set(CPACK_PACKAGE_NAME "projectM-Qt")
set(CPACK_VERBATIM_VARIABLES YES)
include(CPack)

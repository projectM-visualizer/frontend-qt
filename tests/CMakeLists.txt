find_package(projectM4 REQUIRED)
find_package(projectM4Playlist REQUIRED)

add_executable(headless_render_test headless_render_test.cpp)

target_link_libraries(headless_render_test PRIVATE
    libprojectM::projectM
    libprojectM::playlist
    EGL
    GL
)

target_compile_features(headless_render_test PRIVATE cxx_std_17)

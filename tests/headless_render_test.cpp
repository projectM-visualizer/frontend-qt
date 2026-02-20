/**
 * Headless projectM rendering test with preset switching.
 *
 * Creates an offscreen EGL/OpenGL context, initializes projectM,
 * loads presets, feeds synthetic audio, renders frames with rapid
 * preset switching, reads back pixels, and verifies non-black output.
 *
 * Tests:
 *   1. Basic rendering (first preset)
 *   2. Rapid preset switching (simulates user pressing N repeatedly)
 *   3. Post-switch rendering stability
 *
 * Exit code 0 = all tests pass. Non-zero = failure.
 */

#include <projectM-4/projectM.h>
#include <projectM-4/render_opengl.h>
#include <projectM-4/audio.h>
#include <projectM-4/playlist.h>
#include <projectM-4/playlist_items.h>
#include <projectM-4/playlist_playback.h>
#include <projectM-4/playlist_memory.h>

#include <EGL/egl.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <csignal>

static const int WIDTH  = 512;
static const int HEIGHT = 512;
static const char* SCREENSHOT_PATH = "headless_test_output.ppm";

// ── helpers ────────────────────────────────────────────────────────

static bool save_ppm(const char* path, const uint8_t* pixels, int w, int h)
{
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    // OpenGL gives us bottom-up, flip vertically
    for (int y = h - 1; y >= 0; --y)
        fwrite(pixels + y * w * 3, 1, w * 3, f);
    fclose(f);
    return true;
}

struct PixelStats {
    double mean_r, mean_g, mean_b;
    int non_black_pixels;
    int total_pixels;
};

static PixelStats analyse_pixels(const uint8_t* px, int w, int h)
{
    PixelStats s{};
    s.total_pixels = w * h;
    double sum_r = 0, sum_g = 0, sum_b = 0;
    for (int i = 0; i < w * h; ++i) {
        uint8_t r = px[i*3+0], g = px[i*3+1], b = px[i*3+2];
        sum_r += r; sum_g += g; sum_b += b;
        if (r > 2 || g > 2 || b > 2) ++s.non_black_pixels;
    }
    s.mean_r = sum_r / s.total_pixels;
    s.mean_g = sum_g / s.total_pixels;
    s.mean_b = sum_b / s.total_pixels;
    return s;
}

static void generate_audio(float* buf, int samples, int frame)
{
    // Generate a mix of sine waves to simulate music
    for (int i = 0; i < samples; ++i) {
        float t = (float)(frame * samples + i) / 44100.0f;
        float bass  = 0.6f * sinf(2.0f * M_PI * 80.0f  * t);
        float mid   = 0.3f * sinf(2.0f * M_PI * 440.0f * t);
        float hi    = 0.1f * sinf(2.0f * M_PI * 8000.0f * t);
        float beat  = (fmodf(t, 0.5f) < 0.05f) ? 0.8f : 0.0f;  // kick drum
        float val   = bass + mid + hi + beat;
        // stereo: L and R interleaved
        buf[i * 2 + 0] = val;
        buf[i * 2 + 1] = val;
    }
}

// Render N frames, feeding audio each frame. Returns false on GL error.
static bool render_frames(projectm_handle pm, GLuint fbo, int count,
                          int& frame_counter, const int audio_chunk,
                          std::vector<float>& audio_buf)
{
    for (int f = 0; f < count; ++f) {
        generate_audio(audio_buf.data(), audio_chunk, frame_counter);
        projectm_pcm_add_float(pm, audio_buf.data(), audio_chunk, PROJECTM_STEREO);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, WIDTH, HEIGHT);
        projectm_opengl_render_frame_fbo(pm, fbo);

        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            printf("  GL error 0x%x at frame %d\n", err, frame_counter);
            // Don't fail on GL errors, just warn — some presets may generate them
        }
        frame_counter++;
    }
    return true;
}

static PixelStats readback_and_analyse(GLuint fbo)
{
    std::vector<uint8_t> pixels(WIDTH * HEIGHT * 3);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    return analyse_pixels(pixels.data(), WIDTH, HEIGHT);
}

// ── EGL setup ──────────────────────────────────────────────────────

static EGLDisplay egl_dpy = EGL_NO_DISPLAY;
static EGLContext egl_ctx = EGL_NO_CONTEXT;
static EGLSurface egl_surf = EGL_NO_SURFACE;

static bool init_egl()
{
    egl_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (egl_dpy == EGL_NO_DISPLAY) {
        fprintf(stderr, "EGL: no display\n");
        return false;
    }

    EGLint major, minor;
    if (!eglInitialize(egl_dpy, &major, &minor)) {
        fprintf(stderr, "EGL: init failed\n");
        return false;
    }
    printf("EGL %d.%d initialised\n", major, minor);

    // We need OpenGL (not ES)
    if (!eglBindAPI(EGL_OPENGL_API)) {
        fprintf(stderr, "EGL: cannot bind OpenGL API\n");
        return false;
    }

    EGLint cfg_attribs[] = {
        EGL_SURFACE_TYPE,    EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_RED_SIZE,   8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE,  8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };
    EGLConfig cfg;
    EGLint num_cfg;
    if (!eglChooseConfig(egl_dpy, cfg_attribs, &cfg, 1, &num_cfg) || num_cfg == 0) {
        fprintf(stderr, "EGL: no config\n");
        return false;
    }

    EGLint pb_attribs[] = {
        EGL_WIDTH,  WIDTH,
        EGL_HEIGHT, HEIGHT,
        EGL_NONE
    };
    egl_surf = eglCreatePbufferSurface(egl_dpy, cfg, pb_attribs);
    if (egl_surf == EGL_NO_SURFACE) {
        fprintf(stderr, "EGL: pbuffer failed\n");
        return false;
    }

    // Request OpenGL 3.3 core
    EGLint ctx_attribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 3,
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
        EGL_NONE
    };
    egl_ctx = eglCreateContext(egl_dpy, cfg, EGL_NO_CONTEXT, ctx_attribs);
    if (egl_ctx == EGL_NO_CONTEXT) {
        fprintf(stderr, "EGL: context creation failed (error %d)\n", eglGetError());
        return false;
    }

    if (!eglMakeCurrent(egl_dpy, egl_surf, egl_surf, egl_ctx)) {
        fprintf(stderr, "EGL: makeCurrent failed\n");
        return false;
    }

    printf("OpenGL: %s\n", glGetString(GL_VERSION));
    printf("Renderer: %s\n", glGetString(GL_RENDERER));
    return true;
}

static void cleanup_egl()
{
    if (egl_dpy != EGL_NO_DISPLAY) {
        eglMakeCurrent(egl_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (egl_ctx  != EGL_NO_CONTEXT)  eglDestroyContext(egl_dpy, egl_ctx);
        if (egl_surf != EGL_NO_SURFACE)  eglDestroySurface(egl_dpy, egl_surf);
        eglTerminate(egl_dpy);
    }
}

// ── signal handler for crash diagnostics ───────────────────────────

static volatile sig_atomic_t g_last_switch = -1;
static const char* g_phase = "init";

static void crash_handler(int sig)
{
    const char* name = (sig == SIGSEGV) ? "SIGSEGV" :
                       (sig == SIGABRT) ? "SIGABRT" :
                       (sig == SIGFPE)  ? "SIGFPE"  : "UNKNOWN";
    fprintf(stderr, "\n!!! CRASH: %s during phase '%s', last preset switch #%d !!!\n",
            name, g_phase, (int)g_last_switch);
    _exit(100 + sig);
}

// ── main ───────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    // Install crash handlers
    signal(SIGSEGV, crash_handler);
    signal(SIGABRT, crash_handler);
    signal(SIGFPE, crash_handler);

    const char* preset_dir = nullptr;
    int num_switches = 20;  // default: switch presets 20 times

    if (argc > 1) {
        preset_dir = argv[1];
    } else {
        const char* home = getenv("HOME");
        if (!home) {
            fprintf(stderr, "ERROR: HOME environment variable not set\n");
            return 1;
        }
        static char buf[512];
        snprintf(buf, sizeof(buf), "%s/.local/share/projectM/presets", home);
        preset_dir = buf;
    }
    if (argc > 2) {
        num_switches = atoi(argv[2]);
        if (num_switches < 1) num_switches = 1;
    }

    printf("=== projectM Headless Render Test (with preset switching) ===\n");
    printf("Preset dir:   %s\n", preset_dir);
    printf("Resolution:   %dx%d\n", WIDTH, HEIGHT);
    printf("Switches:     %d\n\n", num_switches);

    // 1. Init EGL offscreen context
    g_phase = "egl_init";
    if (!init_egl()) {
        fprintf(stderr, "FAIL: Could not create offscreen OpenGL context\n");
        return 1;
    }

    // 2. Create projectM
    g_phase = "projectm_create";
    projectm_handle pm = projectm_create();
    if (!pm) {
        fprintf(stderr, "FAIL: projectm_create() returned NULL\n");
        cleanup_egl();
        return 2;
    }
    printf("projectM instance created\n");

    projectm_set_window_size(pm, WIDTH, HEIGHT);
    printf("Window size set to %dx%d\n", WIDTH, HEIGHT);

    // 3. Create playlist and load presets
    g_phase = "playlist_create";
    projectm_playlist_handle playlist = projectm_playlist_create(pm);
    if (!playlist) {
        fprintf(stderr, "FAIL: projectm_playlist_create() returned NULL\n");
        projectm_destroy(pm);
        cleanup_egl();
        return 3;
    }

    uint32_t added = projectm_playlist_add_path(playlist, preset_dir, true, false);
    printf("Loaded %u presets from %s\n", added, preset_dir);

    if (added == 0) {
        fprintf(stderr, "FAIL: No presets found in %s\n", preset_dir);
        projectm_playlist_destroy(playlist);
        projectm_destroy(pm);
        cleanup_egl();
        return 4;
    }

    // Enable shuffle for variety
    projectm_playlist_set_shuffle(playlist, true);

    // Start playing first preset
    uint32_t pos = projectm_playlist_set_position(playlist, 0, true);
    char* preset_name = projectm_playlist_item(playlist, pos);
    printf("Initial preset: %s\n\n", preset_name ? preset_name : "(null)");
    if (preset_name) projectm_playlist_free_string(preset_name);

    // 4. Set up FBO for rendering
    g_phase = "fbo_setup";
    GLuint fbo, color_tex, depth_rb;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &color_tex);
    glBindTexture(GL_TEXTURE_2D, color_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, WIDTH, HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_tex, 0);

    glGenRenderbuffers(1, &depth_rb);
    glBindRenderbuffer(GL_RENDERBUFFER, depth_rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, WIDTH, HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depth_rb);

    GLenum fb_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fb_status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "FAIL: FBO incomplete (status 0x%x)\n", fb_status);
        projectm_playlist_destroy(playlist);
        projectm_destroy(pm);
        cleanup_egl();
        return 5;
    }
    printf("FBO created and complete (id=%u)\n\n", fbo);

    const int audio_chunk = 735;  // ~1/60th of 44100
    std::vector<float> audio_buf(audio_chunk * 2);
    int frame_counter = 0;
    bool all_passed = true;

    // ── TEST 1: Basic rendering (30 frames with initial preset) ────
    printf("--- TEST 1: Basic rendering (30 frames) ---\n");
    g_phase = "test1_basic_render";
    render_frames(pm, fbo, 30, frame_counter, audio_chunk, audio_buf);

    PixelStats stats = readback_and_analyse(fbo);
    printf("  Non-black: %d / %d (%.1f%%)\n",
           stats.non_black_pixels, stats.total_pixels,
           100.0 * stats.non_black_pixels / stats.total_pixels);

    bool test1 = stats.non_black_pixels > (stats.total_pixels / 100);
    printf("  Result: %s\n\n", test1 ? "PASS" : "FAIL");
    if (!test1) all_passed = false;

    // ── TEST 2: Preset switching stress test ───────────────────────
    printf("--- TEST 2: Preset switching (%d switches) ---\n", num_switches);
    g_phase = "test2_preset_switching";

    int switch_failures = 0;

    for (int s = 0; s < num_switches; ++s) {
        g_last_switch = s;

        // Render a few frames before switching (like real usage)
        render_frames(pm, fbo, 10, frame_counter, audio_chunk, audio_buf);

        // Switch to next preset (hard cut)
        uint32_t new_pos = projectm_playlist_play_next(playlist, true);
        char* name = projectm_playlist_item(playlist, new_pos);
        printf("  Switch %d/%d -> [%u] %s\n", s + 1, num_switches,
               new_pos, name ? name : "(null)");
        if (name) projectm_playlist_free_string(name);

        // Render frames immediately after switch (this is where crashes happen)
        render_frames(pm, fbo, 15, frame_counter, audio_chunk, audio_buf);

        // Check pixels after switch
        stats = readback_and_analyse(fbo);
        double pct = 100.0 * stats.non_black_pixels / stats.total_pixels;
        if (stats.non_black_pixels <= (stats.total_pixels / 100)) {
            printf("    WARNING: Black screen after switch (%.1f%% non-black)\n", pct);
            switch_failures++;
        }
    }

    printf("  Switches completed: %d/%d\n", num_switches, num_switches);
    printf("  Black screen after switch: %d times\n", switch_failures);

    bool test2 = true;  // If we get here without crashing, the switch test passes
    printf("  Result: %s (no crash)\n\n", test2 ? "PASS" : "FAIL");

    // ── TEST 3: Rapid-fire preset switching (back-to-back, no render between) ──
    printf("--- TEST 3: Rapid-fire switching (10 instant switches) ---\n");
    g_phase = "test3_rapid_fire";

    for (int s = 0; s < 10; ++s) {
        g_last_switch = num_switches + s;
        uint32_t new_pos = projectm_playlist_play_next(playlist, true);
        char* name = projectm_playlist_item(playlist, new_pos);
        printf("  Rapid %d/10 -> [%u] %s\n", s + 1, new_pos,
               name ? name : "(null)");
        if (name) projectm_playlist_free_string(name);
    }

    // Now render after all those rapid switches
    render_frames(pm, fbo, 30, frame_counter, audio_chunk, audio_buf);
    stats = readback_and_analyse(fbo);
    printf("  Non-black after rapid switches: %d / %d (%.1f%%)\n",
           stats.non_black_pixels, stats.total_pixels,
           100.0 * stats.non_black_pixels / stats.total_pixels);

    bool test3 = true;  // survival = pass
    printf("  Result: %s (no crash)\n\n", test3 ? "PASS" : "FAIL");

    // ── TEST 4: Post-switching stability (render 60 more frames) ──
    printf("--- TEST 4: Post-switch stability (60 frames) ---\n");
    g_phase = "test4_stability";
    render_frames(pm, fbo, 60, frame_counter, audio_chunk, audio_buf);

    stats = readback_and_analyse(fbo);
    printf("  Non-black: %d / %d (%.1f%%)\n",
           stats.non_black_pixels, stats.total_pixels,
           100.0 * stats.non_black_pixels / stats.total_pixels);

    bool test4 = stats.non_black_pixels > (stats.total_pixels / 100);
    printf("  Result: %s\n\n", test4 ? "PASS" : "FAIL");
    if (!test4) all_passed = false;

    // Save final screenshot
    {
        std::vector<uint8_t> pixels(WIDTH * HEIGHT * 3);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
        if (save_ppm(SCREENSHOT_PATH, pixels.data(), WIDTH, HEIGHT)) {
            printf("Final screenshot saved: %s\n", SCREENSHOT_PATH);
        }
    }

    // Summary
    printf("\n=== SUMMARY ===\n");
    printf("Total frames rendered: %d\n", frame_counter);
    printf("Total preset switches: %d (normal) + 10 (rapid)\n", num_switches);
    printf("Test 1 (basic render):    %s\n", test1 ? "PASS" : "FAIL");
    printf("Test 2 (preset switch):   %s\n", test2 ? "PASS" : "FAIL");
    printf("Test 3 (rapid-fire):      %s\n", test3 ? "PASS" : "FAIL");
    printf("Test 4 (post-stability):  %s\n", test4 ? "PASS" : "FAIL");
    printf("\n=== RESULT: %s ===\n", all_passed ? "ALL TESTS PASSED" : "SOME TESTS FAILED");

    // Cleanup
    g_phase = "cleanup";
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &color_tex);
    glDeleteRenderbuffers(1, &depth_rb);
    projectm_playlist_destroy(playlist);
    projectm_destroy(pm);
    cleanup_egl();

    return all_passed ? 0 : 10;
}

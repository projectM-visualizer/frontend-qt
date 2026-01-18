# libprojectM 4.x Migration Guide

This document tracks the API changes needed to fully support libprojectM 4.x.

## Status

✅ **Complete:**
- PipeWire audio backend implementation
- Header path updates (`libprojectM/` → `projectM-4/`)
- CMake package name (`libprojectM` → `projectM4`)
- Some PCM API updates

❌ **TODO:**

### Core API Changes
- [ ] `projectm_create()` no longer takes flags parameter
- [ ] Remove `projectm_flags::PROJECTM_FLAG_DISABLE_PLAYLIST_LOAD` usage
- [ ] Update callback registrations:
  - `projectm_set_preset_switched_event_callback` → `projectm_set_preset_switch_requested_event_callback`
  - Callback signatures changed (no longer include preset index/rating)
  - `projectm_set_preset_rating_changed_event_callback` - REMOVED in v4

### Rendering API Changes
- [ ] `projectm_render_frame()` → `projectm_opengl_render_frame()`
- [ ] Key handling API completely removed
  - All `PROJECTM_K_*` constants removed
  - `projectm_key_handler()` function removed
  - Need alternative input handling approach

### Type Changes
- [ ] `projectm_preset_rating_type` enum removed
- [ ] `projectMModifier`, `projectMKeycode` types removed

## Testing

Requires libprojectM 4.1.0+ installed. On this system:
```bash
cd ~/projects/projectm/libprojectM
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=$HOME/.local
make -j$(nproc) && make install
```

## References

- [libprojectM 4.0 Release Notes](https://github.com/projectM-visualizer/projectm/releases/tag/v4.0.0)
- [Integration Quickstart Guide](https://github.com/projectM-visualizer/projectm/wiki/Integration-Quickstart-Guide)
- API headers: `~/.local/include/projectM-4/`

#!/bin/bash
# Launcher wrapper for projectM-pipewire that handles Wayland/XWayland window activation

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECTM_BIN="${SCRIPT_DIR}/projectM-pipewire"

# Set required environment variables
export QT_QPA_PLATFORM=xcb
export __GLX_VENDOR_LIBRARY_NAME=nvidia

# Launch projectM in background
"$PROJECTM_BIN" "$@" &
PROJECTM_PID=$!

# Wait for window to be created and activate it
for attempt in {1..30}; do
    # Find windows for this PID
    WINDOWS=$(xdotool search --pid $PROJECTM_PID 2>/dev/null)

    if [ -n "$WINDOWS" ]; then
        for wid in $WINDOWS; do
            # Make window visible and active
            xdotool windowmap $wid 2>/dev/null
            xdotool windowactivate --sync $wid 2>/dev/null
            xdotool windowraise $wid 2>/dev/null
        done
        break
    fi

    # Check if process died
    if ! kill -0 $PROJECTM_PID 2>/dev/null; then
        echo "projectM process terminated unexpectedly"
        exit 1
    fi

    sleep 0.1
done

# Wait for the actual projectM process
wait $PROJECTM_PID

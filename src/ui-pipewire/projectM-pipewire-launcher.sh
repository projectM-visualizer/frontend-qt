#!/bin/bash
# Launcher wrapper for projectM-pipewire

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECTM_BIN="${SCRIPT_DIR}/projectM-pipewire"

exec "$PROJECTM_BIN" "$@"

#!/usr/bin/env zsh
#
# Watch non-gitignored files; on change: set RACK_DIR, run make install, restart VCV Rack.
# Requires: fswatch (brew install fswatch)
#

set -e
PLUGIN_ROOT="$(cd "$(dirname "$0")" && pwd)"
RACK_DIR="${RACK_DIR:-/Users/evech/Desktop/dev/Fundamental/Rack-SDK}"
APP_NAME="/Applications/VCV Rack 2 Free.app"

log() { echo "[$(date +%H:%M:%S)] $*"; }

run_install_and_restart() {
  log "Change detected, running make install..."
  ( cd "$PLUGIN_ROOT" && export RACK_DIR="$RACK_DIR" && make install ) || { log "make install failed"; return 1; }
  log "make install OK, restarting VCV Rack..."

  # Find PID of process whose name contains "VCV Rack"
  local pids
  pids=($(pgrep -f "VCV Rack" 2>/dev/null || true))
  if [[ ${#pids[@]} -eq 0 ]]; then
    log "No running 'VCV Rack' process found; skipping restart."
    log "Launching $APP_NAME"
    open -a "$APP_NAME"
    log "Done."
    return 0
  fi
  for pid in $pids; do
    log "Stopping PID $pid"
    kill "$pid" 2>/dev/null || true
  done
  sleep 1
  # Force kill if still running
  pids=($(pgrep -f "VCV Rack" 2>/dev/null || true))
  for pid in $pids; do
    kill -9 "$pid" 2>/dev/null || true
  done
  log "Launching $APP_NAME"
  open -a "$APP_NAME"
  log "Done."
}

# Paths to watch (non-gitignored: source, resources, config)
WATCH_DIRS=("$PLUGIN_ROOT/src" "$PLUGIN_ROOT/res")
WATCH_FILES=("$PLUGIN_ROOT/plugin.json" "$PLUGIN_ROOT/Makefile")

if ! command -v fswatch &>/dev/null; then
  echo "fswatch not found. Install with: brew install fswatch"
  exit 1
fi

log "Watching for changes (RACK_DIR=$RACK_DIR). Ctrl+C to stop."
# Run once on start
run_install_and_restart

# Watch directories and specific files; one event per batch
fswatch -o -r "${WATCH_DIRS[@]}" "${WATCH_FILES[@]}" | while read -r _; do
  run_install_and_restart
done

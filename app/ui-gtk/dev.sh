#!/usr/bin/env bash
# Live-run the GTK app with smart change detection and focused error output.
#
# What it does on each save:
#   1. Lists exactly which files changed (mtime-based diff).
#   2. Warns if a source file exists under src/ but isn't in meson.build sources
#      (this is the #1 reason "it builds but the symbol is missing").
#   3. Rebuilds. The output is filtered: cc invocations are hidden, errors are
#      red, warnings are yellow, ninja progress is dim. The previous app keeps
#      running on failure so your window stays open while you fix things.
#   4. On failure: repeats the first error at the bottom, fires a desktop
#      notification (if notify-send is installed), and rings the terminal bell.
#   5. On success: kills the previous instance and launches the new binary.
#
# Usage:  ./app/ui-gtk/dev.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$REPO_ROOT"

BUILD_DIR="build-ui"
SRC_DIR="app/ui-gtk"
FACADE_DIR="app/facade"
BIN="$BUILD_DIR/quick-share-ubuntu"
APP_ID="dev.quickshare.UbuntuShare"
APP_NAME="Quick Share"

if [[ -t 1 ]]; then
  R=$'\033[31m'; G=$'\033[32m'; Y=$'\033[33m'; B=$'\033[34m'; D=$'\033[2m'; X=$'\033[0m'
else
  R=; G=; Y=; B=; D=; X=
fi
say()  { printf '%s[dev]%s %s\n'     "$B" "$X" "$*"; }
ok()   { printf '%s[dev]%s %s%s%s\n' "$B" "$X" "$G" "$*" "$X"; }
warn() { printf '%s[dev]%s %s%s%s\n' "$B" "$X" "$Y" "$*" "$X"; }
err()  { printf '%s[dev]%s %s%s%s\n' "$B" "$X" "$R" "$*" "$X" >&2; }

have_notify=0
command -v notify-send >/dev/null 2>&1 && have_notify=1

if [[ ! -d "$BUILD_DIR" ]]; then
  say "setting up $BUILD_DIR"
  meson setup "$BUILD_DIR" "$SRC_DIR"
fi

# Register the app with the desktop environment so the dock / activities
# overview show "Quick Share" + icon instead of the raw app_id + gear.
# Idempotent: only writes when the source or binary path changes.
ensure_desktop_entry() {
  local xdg_data="${XDG_DATA_HOME:-$HOME/.local/share}"
  local desktop_dir="$xdg_data/applications"
  local hicolor_dir="$xdg_data/icons/hicolor"
  local icon_dir="$hicolor_dir/scalable/apps"
  local desktop_path="$desktop_dir/$APP_ID.desktop"
  local icon_path="$icon_dir/$APP_ID.svg"
  local src_icon="$SRC_DIR/data/$APP_ID.svg"
  local bin_abs="$REPO_ROOT/$BIN"

  mkdir -p "$desktop_dir" "$icon_dir"

  if [[ -f "$src_icon" ]] && ! cmp -s "$src_icon" "$icon_path" 2>/dev/null; then
    install -m644 "$src_icon" "$icon_path"
    say "installed icon → ${icon_path/#$HOME/~}"
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
      gtk-update-icon-cache -f "$hicolor_dir" >/dev/null 2>&1 || true
    fi
  fi

  if [[ ! -f "$desktop_path" ]] || ! grep -q "Exec=$bin_abs\b" "$desktop_path" 2>/dev/null; then
    cat > "$desktop_path" <<EOF
[Desktop Entry]
Type=Application
Name=$APP_NAME
Comment=Share files with nearby devices
Exec=$bin_abs
Icon=$APP_ID
Categories=Network;FileTransfer;
StartupWMClass=$APP_ID
StartupNotify=true
EOF
    say "installed .desktop → ${desktop_path/#$HOME/~}"
    if command -v update-desktop-database >/dev/null 2>&1; then
      update-desktop-database "$desktop_dir" >/dev/null 2>&1 || true
    fi
    warn "If the dock still shows the old name/icon, log out & back in (Wayland)"
    warn "or run 'killall -SIGHUP gnome-shell' (X11 only)."
  fi
}
ensure_desktop_entry

pkill -f "$BIN" 2>/dev/null || true

PID=0

list_watched() {
  find "$SRC_DIR/src" "$SRC_DIR/meson.build" "$FACADE_DIR" \
       -type f \
       \( -name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.h' -o -name 'meson.build' \) \
       2>/dev/null | sort
}

fingerprint() {
  list_watched | xargs -d '\n' stat -c '%Y %n' 2>/dev/null \
               | sha1sum | cut -d' ' -f1
}

# Per-file mtime snapshot so we can show exactly what triggered a rebuild.
declare -A SNAP
snapshot() {
  SNAP=()
  while IFS= read -r f; do
    SNAP["$f"]="$(stat -c '%Y' "$f" 2>/dev/null || echo 0)"
  done < <(list_watched)
}
diff_changed() {
  while IFS= read -r f; do
    local m
    m="$(stat -c '%Y' "$f" 2>/dev/null || echo 0)"
    [[ "${SNAP[$f]:-0}" != "$m" ]] && echo "$f"
  done < <(list_watched)
}

# Drift: a source file under src/ that meson.build doesn't reference.
check_orphans() {
  local missing=()
  while IFS= read -r f; do
    local rel="${f#$SRC_DIR/}"
    if ! grep -qF "$rel" "$SRC_DIR/meson.build" 2>/dev/null; then
      missing+=("$rel")
    fi
  done < <(find "$SRC_DIR/src" -type f \( -name '*.c' -o -name '*.cc' -o -name '*.cpp' \) 2>/dev/null | sort)
  if (( ${#missing[@]} )); then
    warn "source file(s) not listed in meson.build:"
    for m in "${missing[@]}"; do
      printf '       %s%s%s\n' "$Y" "$m" "$X"
    done
    warn "  add them to executable(... sources : [...]) or the link will be missing symbols."
  fi
}

# Strip raw cc lines; highlight errors / warnings / FAILED.
filter() {
  awk -v R="$R" -v Y="$Y" -v D="$D" -v X="$X" '
    /^FAILED:/                { print R $0 X; next }
    /^(cc|c\+\+) /              { next }
    /^\[[0-9]+\/[0-9]+\]/     { print D $0 X; next }
    /^ninja:/                 { print D $0 X; next }
    /error:/                  { print R $0 X; next }
    /warning:/                { print Y $0 X; next }
    /^In file included/       { print D $0 X; next }
    { print }
  '
}

build_and_run() {
  say "building C++ library (Bazel)…"
  if ! bazel build //app/facade:libnearby_bridge.so --check_direct_dependencies=off; then
    err "C++ BUILD FAILED — previous app still running"
    return
  fi

  say "building UI (Meson)…"
  check_orphans

  local logfile
  logfile="$(mktemp /tmp/qs-build.XXXXXX)"
  if meson compile -C "$BUILD_DIR" 2>&1 | tee "$logfile" | filter; then
    rm -f "$logfile"
    if (( PID != 0 )) && kill -0 "$PID" 2>/dev/null; then
      kill "$PID" 2>/dev/null || true
      wait "$PID" 2>/dev/null || true
    fi
    "./$BIN" &
    PID=$!
    ok "running pid=$PID"
  else
    err "BUILD FAILED — previous app still running"
    # Repeat the first error compactly so it's visible at the bottom.
    local first
    first="$(grep -m1 -E 'error:' "$logfile" | sed -E 's|^.*/([^/]+\.[ch]:[0-9]+:[0-9]+:)|\1|' || true)"
    if [[ -n "$first" ]]; then
      printf '%s  → %s%s\n' "$R" "$first" "$X"
    fi
    if (( have_notify )); then
      notify-send -u critical -t 5000 "quick-share-ubuntu: build failed" \
        "${first:-see terminal}" >/dev/null 2>&1 || true
    fi
    printf '\a'   # terminal bell
    rm -f "$logfile"
  fi
}

cleanup() {
  if (( PID != 0 )) && kill -0 "$PID" 2>/dev/null; then
    kill "$PID" 2>/dev/null || true
  fi
  exit 0
}
trap cleanup INT TERM

snapshot
build_and_run
LAST="$(fingerprint)"

while sleep 0.5; do
  CUR="$(fingerprint)"
  [[ "$CUR" == "$LAST" ]] && continue
  LAST="$CUR"

  mapfile -t changes < <(diff_changed)
  snapshot
  if (( ${#changes[@]} )); then
    say "changed:"
    for c in "${changes[@]}"; do
      printf '       %s%s%s\n' "$D" "${c#./}" "$X"
    done
  fi
  build_and_run
done

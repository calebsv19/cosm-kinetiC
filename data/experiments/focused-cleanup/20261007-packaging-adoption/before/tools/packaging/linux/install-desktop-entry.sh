#!/usr/bin/env sh
set -eu

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
PACKAGE_ROOT="$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)"
DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
APP_DIR="$DATA_HOME/applications"
ICON_DIR="$DATA_HOME/icons/hicolor/scalable/apps"
DESKTOP_FILE="$APP_DIR/kinetic.desktop"
ICON_FILE="$ICON_DIR/kinetic.svg"

mkdir -p "$APP_DIR" "$ICON_DIR"
cp "$PACKAGE_ROOT/share/icons/hicolor/scalable/apps/kinetic.svg" "$ICON_FILE"

cat > "$DESKTOP_FILE" <<EOF
[Desktop Entry]
Type=Application
Name=kinetiC
Comment=Physics simulation workspace
Exec=$PACKAGE_ROOT/bin/physics-sim-launcher
Icon=$ICON_FILE
Terminal=false
Categories=Education;Science;Physics;
StartupNotify=true
EOF

chmod 0644 "$DESKTOP_FILE" "$ICON_FILE"
echo "$DESKTOP_FILE"

#!/bin/sh
set -eu

if [ "$#" -ne 3 ]; then
    echo "usage: $0 <app_bin> <frameworks_dir> <owned_work_dir>" >&2
    exit 1
fi

APP_BIN="$1"
FRAMEWORKS_DIR="$2"
AWK_BIN="/usr/bin/awk"
GREP_BIN="/usr/bin/grep"
CP_BIN="/bin/cp"
CHMOD_BIN="/bin/chmod"
BASENAME_BIN="/usr/bin/basename"
OTOOL_BIN="/usr/bin/otool"
INSTALL_NAME_TOOL_BIN="/usr/bin/install_name_tool"
MKDIR_BIN="/bin/mkdir"
SEARCH_ROOTS_LIST="${PACKAGE_DEP_SEARCH_ROOTS:-/opt/homebrew:/usr/local}"

"$MKDIR_BIN" -p "$FRAMEWORKS_DIR"

# The Python owner creates this fresh private work directory and retains it.
WORK_TMP_DIR="$3"
QUEUE_FILE="$WORK_TMP_DIR/queue.txt"
SEEN_FILE="$WORK_TMP_DIR/seen.txt"
( set -C; : > "$QUEUE_FILE"; : > "$SEEN_FILE" )
INDEX=0

resolve_search_root_dep() {
    dep_base="$1"
    old_ifs="${IFS}"
    IFS=":"
    for search_root in $SEARCH_ROOTS_LIST; do
        [ -n "$search_root" ] || continue
        if [ -f "$search_root/lib/$dep_base" ]; then
            IFS="${old_ifs}"
            printf '%s\n' "$search_root/lib/$dep_base"
            return 0
        fi
    done
    IFS="${old_ifs}"
    return 1
}

echo "$APP_BIN" >>"$QUEUE_FILE"

while IFS= read -r current_file; do
    [ -n "$current_file" ] || continue
    if "$GREP_BIN" -Fxq "$current_file" "$SEEN_FILE"; then
        continue
    fi
    echo "$current_file" >>"$SEEN_FILE"

    INDEX=$((INDEX + 1))
    if [ "$INDEX" -gt 256 ]; then
        echo "dependency traversal exceeds 256 binary bound" >&2
        exit 1
    fi
    # Separate commands preserve otool failure; an empty selected set is valid.
    "$OTOOL_BIN" -L "$current_file" > "$WORK_TMP_DIR/otool-$INDEX.txt"
    deps="$("$AWK_BIN" 'NR>1 && ($1 ~ /^\/opt\/homebrew\// || $1 ~ /^\/usr\/local\// || $1 ~ /^@rpath\//) {print $1}' "$WORK_TMP_DIR/otool-$INDEX.txt")"
    if [ -z "$deps" ]; then
        continue
    fi

    echo "$deps" | while IFS= read -r dep; do
        [ -n "$dep" ] || continue
        dep_base="$("$BASENAME_BIN" "$dep")"
        dep_dst="$FRAMEWORKS_DIR/$dep_base"
        dep_src="$dep"

        case "$dep" in
            @rpath/*)
                if [ -f "$FRAMEWORKS_DIR/$dep_base" ]; then
                    dep_src="$FRAMEWORKS_DIR/$dep_base"
                elif dep_src="$(resolve_search_root_dep "$dep_base")"; then
                    :
                else
                    echo "unable to resolve $dep for $current_file" >&2
                    exit 1
                fi
                ;;
        esac

        if [ ! -f "$dep_dst" ]; then
            "$CP_BIN" -fL "$dep_src" "$dep_dst"
            "$CHMOD_BIN" u+w "$dep_dst"
            "$INSTALL_NAME_TOOL_BIN" -id "@loader_path/$dep_base" "$dep_dst"
            echo "$dep_dst" >>"$QUEUE_FILE"
        fi

        if [ "$current_file" = "$APP_BIN" ]; then
            replacement="@executable_path/../Frameworks/$dep_base"
        else
            replacement="@loader_path/$dep_base"
        fi
        "$INSTALL_NAME_TOOL_BIN" -change "$dep" "$replacement" "$current_file"
    done
done <"$QUEUE_FILE"

# Explicitly bundle MoltenVK so Vulkan loader can be pinned to app-local ICD.
MOLTENVK_SRC=""
MOLTENVK_SRC="$(resolve_search_root_dep libMoltenVK.dylib || true)"
if [ -n "$MOLTENVK_SRC" ]; then
    MOLTENVK_DST="$FRAMEWORKS_DIR/libMoltenVK.dylib"
    if [ ! -f "$MOLTENVK_DST" ]; then
        "$CP_BIN" -fL "$MOLTENVK_SRC" "$MOLTENVK_DST"
        "$CHMOD_BIN" u+w "$MOLTENVK_DST"
    fi
    "$INSTALL_NAME_TOOL_BIN" -id "@loader_path/libMoltenVK.dylib" "$MOLTENVK_DST"
fi

VULKAN_SRC="$(resolve_search_root_dep libvulkan.1.dylib || true)"
if [ -n "$VULKAN_SRC" ]; then
    VULKAN_DST="$FRAMEWORKS_DIR/libvulkan.1.dylib"
    if [ ! -f "$VULKAN_DST" ]; then
        "$CP_BIN" -fL "$VULKAN_SRC" "$VULKAN_DST"
        "$CHMOD_BIN" u+w "$VULKAN_DST"
    fi
    "$INSTALL_NAME_TOOL_BIN" -id "@loader_path/libvulkan.1.dylib" "$VULKAN_DST"
fi

exit 0

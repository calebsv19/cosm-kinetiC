# Sourced by both packaged launchers. No Python runtime is required.
# The subshell keeps initializer variables and traps out of the caller.
physics_config_initialize() (
    set -eu
    cfg_source=$1
    cfg_runtime=$2
    cfg_action=${3:-}
    cfg_target=$cfg_runtime/config
    cfg_marker=.physics-config-complete
    cfg_lock=$cfg_runtime/.config-initialization-lock
    cfg_hold() { printf '%s\n' "configuration held: $*" >&2; exit 1; }
    cfg_inventory() (
        cd "$1"
        cfg_output=$2
        # Reject links/special files and ambiguous inventory names before hashing.
        cfg_newline='
'
        find . -name "*$cfg_newline*" -print > "$cfg_output.names"
        [ ! -s "$cfg_output.names" ] || cfg_hold 'newline in configuration path'
        find . -print > "$cfg_output.names"
        if LC_ALL=C grep -q '[[:cntrl:]]' "$cfg_output.names"; then
            cfg_hold 'control character in configuration path'
        fi
        [ "$(wc -l < "$cfg_output.names")" -le 10000 ] || cfg_hold 'configuration entry limit'
        find . ! -type f ! -type d -print > "$cfg_output.special"
        [ ! -s "$cfg_output.special" ] || cfg_hold 'configuration contains link or special file'
        : > "$cfg_output.unsorted"
        cfg_bytes=0
        while IFS= read -r cfg_name; do
            [ "$cfg_name" != "./$cfg_marker" ] || continue
            if [ -d "$cfg_name" ]; then
                printf 'directory %s\n' "$cfg_name" >> "$cfg_output.unsorted"
            else
                cfg_size=$(wc -c < "$cfg_name")
                cfg_bytes=$((cfg_bytes + cfg_size))
                [ "$cfg_bytes" -le 67108864 ] || cfg_hold 'configuration exceeds 64 MiB'
                if command -v sha256sum >/dev/null 2>&1; then
                    sha256sum "$cfg_name" >> "$cfg_output.unsorted"
                else
                    shasum -a 256 "$cfg_name" >> "$cfg_output.unsorted"
                fi
            fi
        done < "$cfg_output.names"
        LC_ALL=C sort "$cfg_output.unsorted" > "$cfg_output"
    )
    [ -d "$cfg_source" ] && [ ! -L "$cfg_source" ] || cfg_hold 'missing or linked package configuration'
    [ ! -e "$cfg_source/$cfg_marker" ] || cfg_hold 'reserved marker in package configuration'
    [ ! -L "$cfg_runtime/.config-initialization-attempts" ] || cfg_hold 'linked attempt history'
    if [ -e "$cfg_target" ] && [ ! -L "$cfg_target" ]; then
        [ -d "$cfg_target" ] || cfg_hold 'configuration is not a directory'
        if [ -f "$cfg_target/$cfg_marker" ] && [ ! -L "$cfg_target/$cfg_marker" ]; then
            [ "$(head -n 1 "$cfg_target/$cfg_marker")" = physics-config-v1 ] || cfg_hold 'invalid completion marker'
            [ "$cfg_action" != --adopt-existing-config ] || printf '%s\n' 'configuration already adopted'
            exit 0
        fi
        [ "$cfg_action" = --adopt-existing-config ] || cfg_hold 'unmarked existing directory; review it, then use --adopt-existing-config'
    fi
    if [ -L "$cfg_target" ]; then
        [ "$(readlink "$cfg_target")" = "$cfg_source" ] || cfg_hold 'configuration link does not name this package'
    fi
    mkdir "$cfg_lock" 2>/dev/null || cfg_hold 'initialization lock exists; inspect retained attempts before recovery'
    # Remove only this invocation's empty lock on ordinary completion/failure.
    # Abrupt termination leaves it held; no PID-based stale-lock deletion.
    trap 'rmdir "$cfg_lock" 2>/dev/null || :' 0
    mkdir -p "$cfg_runtime/.config-initialization-attempts"
    cfg_attempt=$(mktemp -d "$cfg_runtime/.config-initialization-attempts/attempt.XXXXXX")
    printf '%s\n' "source=$cfg_source" "target=$cfg_target" "action=$cfg_action" > "$cfg_attempt/request"
    cfg_inventory "$cfg_source" "$cfg_attempt/source-before"
    if [ -d "$cfg_target" ] && [ ! -L "$cfg_target" ]; then
        # Explicit legacy adoption preserves every existing byte. Require the
        # package's full path inventory; edited file contents remain legitimate.
        cfg_inventory "$cfg_target" "$cfg_attempt/existing"
        while IFS= read -r cfg_name; do
            [ -e "$cfg_target/$cfg_name" ] || cfg_hold "legacy configuration missing $cfg_name"
            if [ -d "$cfg_source/$cfg_name" ]; then
                [ -d "$cfg_target/$cfg_name" ] || cfg_hold "legacy directory mismatch: $cfg_name"
            else
                [ -f "$cfg_target/$cfg_name" ] || cfg_hold "legacy file mismatch: $cfg_name"
            fi
        done < "$cfg_attempt/source-before.names"
        printf '%s\n' physics-config-v1 "attempt=$cfg_attempt" > "$cfg_attempt/completion-marker"
        ln "$cfg_attempt/completion-marker" "$cfg_target/$cfg_marker"
        printf '%s\n' 'legacy configuration adopted' > "$cfg_attempt/result"
        exit 0
    fi
    mkdir "$cfg_attempt/candidate"
    cp -R "$cfg_source/." "$cfg_attempt/candidate/"
    cfg_inventory "$cfg_attempt/candidate" "$cfg_attempt/candidate-inventory"
    cfg_inventory "$cfg_source" "$cfg_attempt/source-after"
    cmp "$cfg_attempt/source-before" "$cfg_attempt/source-after" >/dev/null || cfg_hold 'package configuration changed while copying'
    cmp "$cfg_attempt/source-before" "$cfg_attempt/candidate-inventory" >/dev/null || cfg_hold 'configuration copy is incomplete'
    # Package resources may be read-only; the admitted private copy must support
    # user saves. Do this only after rejecting candidate links/special files.
    chmod -R u+rwX "$cfg_attempt/candidate"
    printf '%s\n' physics-config-v1 "attempt=$cfg_attempt" > "$cfg_attempt/candidate/$cfg_marker"
    if [ -L "$cfg_target" ]; then
        mv "$cfg_target" "$cfg_attempt/legacy-config-link"
    fi
    [ ! -e "$cfg_target" ] && [ ! -L "$cfg_target" ] || cfg_hold 'configuration appeared before publication'
    mv "$cfg_attempt/candidate" "$cfg_target"
    printf '%s\n' 'configuration published' > "$cfg_attempt/result"
)

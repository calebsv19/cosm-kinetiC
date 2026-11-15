# Source from repository-local fixtures. Prints only the allocated path to stdout.
physics_fixture_pack_cli() {
  local repo="$1" output="$2" core="$1/third_party/codework_shared/core"
  "${CC:-cc}" -std=c11 -O2 -I"$core/core_pack/include" \
    -I"$core/core_base/include" -I"$core/core_io/include" \
    "$core/core_pack/tools/pack_cli.c" "$core/core_pack/src/core_pack.c" \
    "$core/core_pack/src/core_pack_vf2d.c" "$core/core_pack/src/core_pack_vf3d.c" \
    "$core/core_base/src/core_base.c" "$core/core_io/src/core_io.c" -o "$output"
}

physics_fixture_root() {
  local repo="$1" name="$2" parent="${3:-${PHYSICS_SIM_TEST_ROOT:-$1/tmp/tests}}"
  local mode="${4:-scratch}" path
  if [[ "$mode" == retained ]]; then
    path="$(python3 -B "$repo/scripts/fixture_root.py" --repo "$repo" --parent "$parent" --name "$name" --retained)" || return
  else
    path="$(python3 -B "$repo/scripts/fixture_root.py" --repo "$repo" --parent "$parent" --name "$name")" || return
  fi
  printf 'Fixture output retained at %s\n' "$path" >&2
  printf '%s\n' "$path"
}

# Call in the fixture's top-level shell, before allocating output.
physics_fixture_supervise() {
  local repo="$1" script="$2"
  shift 2
  if ! python3 -B "$repo/scripts/fixture_session.py" --repo "$repo" --script "$script" --check-session; then
    exec python3 -B "$repo/scripts/fixture_session.py" --repo "$repo" --script "$script" -- "$@"
  fi
}

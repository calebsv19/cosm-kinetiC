# Source from repository-local fixtures. Prints only the allocated path to stdout.
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

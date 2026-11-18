#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/inventsys_rplace_test_$$"
trap 'rm -f "$OUT"' EXIT HUP INT TERM
c++ -std=c++11 -Wall -Wextra -Werror \
  -I"$ROOT/Arduino/PhysicalRPlace" \
  "$ROOT/tests/test_state_machine.cpp" \
  "$ROOT/Arduino/PhysicalRPlace/PlaceStateMachine.cpp" \
  -o "$OUT"
"$OUT"

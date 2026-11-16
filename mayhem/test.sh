#!/usr/bin/env bash
#
# mayhem/test.sh — runs upstream's own cppunitlite suite for the engine (rehlds/unittest), built by
# mayhem/build.sh with the project's normal flags. Every test asserts behaviour (bit-exact delta/
# netchan/mathlib/string results) via CPPUNITLITE_ASSERT/EQUAL. A test that fails ends the run with a
# non-zero exit and no "Passed" line; a runner that prints nothing (or exits 0 without running the tests)
# yields zero passes and fails here.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SRC:=$(cd "$(dirname "$0")/.." && pwd)}"
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

RUNNER="$SRC/build/ut/rehlds/engine_i486"
if [ ! -x "$RUNNER" ] || [ ! -x "$SRC/build/ut/exec32" ]; then
  echo "test runner $RUNNER (or build/ut/exec32) missing — mayhem/build.sh did not build it" >&2
  emit_ctrf cppunitlite 0 1
  exit 1
fi

LOG="$(mktemp)"
trap 'rm -f "$LOG"' EXIT
(cd "$(dirname "$RUNNER")" && timeout 900 "$SRC/build/ut/exec32" "$RUNNER" -all) >"$LOG" 2>&1
rc=$?
cat "$LOG"

passed=$(grep -c '^Test::run() > .* Passed$' "$LOG")
executed=$(sed -n 's/^There were no test failures; Tests executed: \([0-9]*\)$/\1/p' "$LOG" | tail -n1)
failed=0
if [ "$rc" -ne 0 ]; then
  failed=1
elif [ -z "$executed" ] || [ "$executed" -ne "$passed" ] || [ "$passed" -eq 0 ]; then
  echo "runner exited 0 but reported ${executed:-no} executed tests against $passed 'Passed' lines" >&2
  failed=1
fi

emit_ctrf cppunitlite "$passed" "$failed"

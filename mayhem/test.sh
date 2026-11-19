#!/usr/bin/env bash
#
# mayhem/test.sh — run ArduPilot's own unit tests that mayhem/build.sh built (normal flags):
# upstream gtest suites for the parsers the fuzz targets reach (AP_JSON, AP_Param, RC protocols,
# MSP, GPS/GSOF, filesystem, CSV, ...) plus the RCProtocolTest example, which decodes recorded
# SBUS/DSM/SUMD/IBUS/CRSF/FPort frames and checks every channel value. Each gtest binary counts
# as its gtest test cases; a binary that prints no gtest summary counts as one failure.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SRC:=/mayhem}"
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

B="$SRC/build/mayhem-tests/sitl"
TESTS="test_ap_json test_vehicleinfo test_find_by_name test_param_conversion test_param_flush
test_sbus test_ghst_timeout test_msp_serial test_gps test_gsof test_crc8 test_math
test_filesystem_alias test_csvreader test_adsb_callsign test_sensaition_parser test_location"

# the tests write eeprom.bin & co. into their working directory: keep that out of the tree
WORK=$(mktemp -d "${TMPDIR:-/tmp}/ardupilot-tests.XXXXXX")
trap 'rm -rf "$WORK"' EXIT
cd "$WORK"

passed=0; failed=0
for t in $TESTS; do
  bin="$B/tests/$t"
  if [ ! -x "$bin" ]; then
    echo "MISSING $bin (mayhem/build.sh should have built it)"; failed=$((failed + 1)); continue
  fi
  out=$(timeout 300 "$bin" 2>&1); rc=$?
  p=$(printf '%s\n' "$out" | sed -n 's/^\[  PASSED  \] \([0-9]*\) tests\{0,1\}\..*/\1/p' | tail -1)
  f=$(printf '%s\n' "$out" | sed -n 's/^\[  FAILED  \] \([0-9]*\) tests\{0,1\}, listed below.*/\1/p' | tail -1)
  p=${p:-0}; f=${f:-0}
  if [ "$p" -eq 0 ] && [ "$f" -eq 0 ]; then
    f=1   # no gtest summary: the binary did not run its tests
  elif [ "$rc" -ne 0 ] && [ "$f" -eq 0 ]; then
    f=1
  fi
  echo "$t: passed=$p failed=$f (exit $rc)"
  [ "$f" -ne 0 ] && printf '%s\n' "$out" | tail -20
  passed=$((passed + p)); failed=$((failed + f))
done

# RCProtocolTest: decodes recorded frames of every RC protocol and compares the channel values
out=$(timeout 600 "$B/examples/RCProtocolTest" 2>&1); rc=$?
if [ "$rc" -eq 0 ] && printf '%s\n' "$out" | grep -q '^Test PASSED'; then
  echo "RCProtocolTest: PASSED"; passed=$((passed + 1))
else
  echo "RCProtocolTest: FAILED (exit $rc)"; printf '%s\n' "$out" | grep -iE 'fail|error' | tail -20
  failed=$((failed + 1))
fi

cd "$SRC"
emit_ctrf "gtest+RCProtocolTest" "$passed" "$failed" 0

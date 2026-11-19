#!/usr/bin/env bash
#
# mayhem/build.sh — ArduPilot (waf, SITL board).
#
#  1. modules/: the git submodules the build needs, copied from the in-image cache
#     /opt/toolchains/deps/ardupilot-modules (mayhem/fetch-modules.sh filled it), so this runs offline.
#  2. Fuzz build (build/mayhem-fuzz): waf --debug --asan with afl-clang-fast(++) and $SANITIZER_FLAGS,
#     every library instrumented. Targets: tool/Replay (DataFlash log replay) and the file-input
#     harnesses in mayhem/harnesses (built by mayhem/wscript, which waf picks up like any top-level
#     directory). They are AFL targets (afl: true) because SITL programs exit() at the end of input.
#  3. Test build (build/mayhem-tests): plain clang, the project's normal --debug flags, upstream's
#     gtest unit tests + the RCProtocolTest example; mayhem/test.sh runs them.
#
# UBSan checks relaxed (each fires on every start-up or on every replayed log, before any input is
# parsed, on patterns ArduPilot relies on; ASan and the rest of UBSan stay on and halting):
#   vptr               HAL_SITL_Class.cpp casts the HAL to HAL_SITL during static initialisation
#   float-cast-overflow AP_Param::set_value stores out-of-range defaults (65535 into an AP_Int16)
#   null               AP_Vehicle.cpp binds `vehicle` to *AP_Vehicle::get_singleton() (null in tools)
#   alignment          packed log/MAVLink structures are read in place (Replay MsgHandler, AP_Param)
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"

# 1) submodules from the in-image cache (only where modules/<m> is empty: a re-run keeps them)
MODCACHE=/opt/toolchains/deps/ardupilot-modules/src
for m in modules/waf modules/mavlink modules/gtest modules/lwip modules/littlefs modules/picolibc \
         modules/DroneCAN/DSDL modules/DroneCAN/dronecan_dsdlc modules/DroneCAN/libcanard \
         modules/DroneCAN/pydronecan; do
    if [ -z "$(ls -A "$m" 2>/dev/null)" ]; then
        mkdir -p "$m"
        cp -a "$MODCACHE/$m/." "$m/"
    fi
done

export PATH="/opt/toolchains/python/bin:$PATH"
waf() { python3 ./waf "$@"; }
FUZZ_TARGETS=tool/Replay,mayhem/fuzz_rcprotocol,mayhem/fuzz_json,mayhem/fuzz_ftp_upload,mayhem/fuzz_param_defaults
TEST_TARGETS=examples/RCProtocolTest
for t in test_ap_json test_vehicleinfo test_find_by_name test_param_conversion test_param_flush \
         test_sbus test_ghst_timeout test_msp_serial test_gps test_gsof test_crc8 test_math \
         test_filesystem_alias test_csvreader test_adsb_callsign test_sensaition_parser test_location; do
    TEST_TARGETS="$TEST_TARGETS,tests/$t"
done

# 2) fuzz build. waf's configure checks link tiny programs with the AFL runtime, which calls
#    malloc while SITL links with --wrap,malloc: afl_wrap_malloc.o gives those checks a weak
#    __wrap_malloc (real programs use AP_Common's). AFL_LLVM_INSTRUMENT=CLASSIC: the default
#    PCGUARD mode's pc-table keeps every function alive through --gc-sections, which leaves
#    references to features the tool's build disables (link errors in Replay).
OUT=build/mayhem-fuzz
RT="$SRC/$OUT/mayhem-rt"
mkdir -p "$RT"
RELAX="-fno-sanitize=vptr,float-cast-overflow,null,alignment"
ASAN_OPT=""
case "$SANITIZER_FLAGS" in *address*) ASAN_OPT="--asan" ;; esac
clang   $SANITIZER_FLAGS $RELAX $DEBUG_FLAGS -c mayhem/afl_wrap_malloc.c -o "$RT/afl_wrap_malloc.o"
clang++ $SANITIZER_FLAGS $RELAX $DEBUG_FLAGS -c mayhem/lsan_off.cc -o "$RT/lsan_off.o"
(
    export WAFLOCK=.lock-waf_mayhem_fuzz AFL_QUIET=1 AFL_LLVM_INSTRUMENT=CLASSIC
    export CC=afl-clang-fast CXX=afl-clang-fast++
    export CFLAGS="$SANITIZER_FLAGS $RELAX $DEBUG_FLAGS" CXXFLAGS="$SANITIZER_FLAGS $RELAX $DEBUG_FLAGS"
    export LINKFLAGS="$SANITIZER_FLAGS $RELAX $RT/afl_wrap_malloc.o $RT/lsan_off.o"
    unset LDFLAGS
    waf configure --board sitl --debug $ASAN_OPT --no-submodule-update --out "$OUT"
    waf build --targets "$FUZZ_TARGETS" -j"$MAYHEM_JOBS"
)

# 3) test build, normal flags
(
    export WAFLOCK=.lock-waf_mayhem_tests CC=clang CXX=clang++
    export CFLAGS="$COVERAGE_FLAGS" CXXFLAGS="$COVERAGE_FLAGS" LINKFLAGS="$COVERAGE_FLAGS"
    unset LDFLAGS
    waf configure --board sitl --debug --no-submodule-update --out build/mayhem-tests
    waf build --targets "$TEST_TARGETS" -j"$MAYHEM_JOBS"
)

ls -l "$OUT/sitl/tool/Replay" "$OUT"/sitl/mayhem/fuzz_* build/mayhem-tests/sitl/examples/RCProtocolTest

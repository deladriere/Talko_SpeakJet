#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
test_build=$(mktemp -d "${TMPDIR:-/tmp}/talko-speakjet-host.XXXXXX")
trap 'rm -rf "$test_build"' EXIT HUP INT TERM
python3 scripts/generate_banks.py --check
cc=${CC:-cc}
cxx=${CXX:-c++}
test_flags="-O2 -Wall -Wextra -ffp-contract=off"
if [ "${SANITIZE:-0}" = 1 ]; then
  test_flags="$test_flags -fsanitize=address,undefined"
fi
"$cc" -std=c99 $test_flags -Werror -Wdouble-promotion -c lib/SpeakJet/src/speakjet.c -o "$test_build/engine.o"
"$cxx" -std=c++17 $test_flags tests/bank_test.cpp "$test_build/engine.o" -o "$test_build/banks"
"$test_build/banks"
"$cc" -std=c99 $test_flags tests/live_join_test.c -lm -o "$test_build/live"
"$test_build/live"
"$cc" -std=c99 $test_flags tests/upstream_test.c -lm -o "$test_build/upstream"
"$test_build/upstream"

"$cc" -std=c99 $test_flags tests/performance_test.c -lm -o "$test_build/performance"
"$test_build/performance"
"$cxx" -std=c++17 $test_flags tests/dsp_test.cpp src/dsp.cpp -o "$test_build/dsp"
"$test_build/dsp"
"$cxx" -std=c++17 $test_flags -Itests/stubs tests/panel_test.cpp -o "$test_build/panel"
"$test_build/panel"

"$cxx" -std=c++17 $test_flags -Itests/stubs -Ilib/SpeakJet/src tests/serial_test.cpp src/dsp.cpp "$test_build/engine.o" -o "$test_build/serial"
"$test_build/serial"

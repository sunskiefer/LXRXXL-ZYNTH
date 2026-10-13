#!/usr/bin/env bash
# Offline tests, under valgrind: every voice engine on its own (test/eng_test.c), the framework's host test (the VST2 wrapper around the engine), the engine tests and
# the MPC simulation (the plugin through the wrapper only, as MPC drives it).
set -euo pipefail
cd "$(dirname "$0")/.."
. ./build_srcs.sh
bash test/build_host.sh
CC="${CC:-clang}"
OBJS=$(cat build/host/objs.txt)
ENG=$(echo $OBJS | tr ' ' '\n' | grep -v vst2_wrap | tr '\n' ' ')
$CC -O1 -g -w -Ibuild third_party/mpc-vst-plugins/tools/host_test.c $OBJS -lm -lpthread -ldl -o build/host/host_test
$CC -O1 -g -w -std=gnu11 -Ibuild -Ithird_party/mpc-vst-plugins/wrapper $INCS test/engine_test.c $ENG -lm -lpthread -ldl -o build/host/engine_test
$CC -O1 -g -w -Ibuild -Isrc test/mpc_sim.c $OBJS -lm -lpthread -ldl -o build/host/mpc_sim
VG="valgrind -q --error-exitcode=9"
[ "${NOVG:-}" ] && VG=""
(cd build/host && $VG ./eng_test && $VG ./host_test && $VG ./engine_test && $VG ./mpc_sim)

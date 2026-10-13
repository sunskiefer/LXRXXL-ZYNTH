#!/usr/bin/env bash
# The MPC simulation (test/mpc_sim.c) built for ARM with the plugin's own target flags (static musl, so it runs under
# qemu-arm without an ARM glibc) and run under qemu: checks the ARM code generation. Needs qemu-user and Zig.
set -euo pipefail
cd "$(dirname "$0")/.."
. ./build_srcs.sh
ZIG="${ZIG:-python3 -m ziglang}"
MPC_VST=third_party/mpc-vst-plugins
python3 $MPC_VST/tools/gen_vst.py vst.json --params-h >/dev/null
TGT="-target arm-linux-musleabihf -mcpu=generic+v7a+vfp3d16-d32-neon+thumb2 -ffp-contract=off"
FL="-O2 -w -std=gnu11 -Ibuild -I$MPC_VST/wrapper $INCS"
mkdir -p build/armsim
OBJS=""
for f in $LXR_SRCS $PLUGIN_SRCS src/wrapper/vst2_wrap.c test/mpc_sim.c; do o="build/armsim/$(echo $f | tr / _).o"; $ZIG cc $TGT $FL -c $f -o $o; OBJS="$OBJS $o"; done
for f in $CXX_SRCS; do o="build/armsim/$(echo $f | tr / _).o"; $ZIG c++ $TGT -O2 -w $CXXFLAGS_MI $CXXINCS -c $f -o $o; OBJS="$OBJS $o"; done
$ZIG cc $TGT -static $OBJS -lm -o build/armsim/mpc_sim
(cd build/armsim && qemu-arm ./mpc_sim)

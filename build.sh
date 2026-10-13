#!/usr/bin/env bash
# Build LXRXXL ZYNTH for MPC OS (armv7-a hard-float, glibc <= 2.31 so it also loads on MPC OS 2.x).
#   ./build.sh                     (MPC_VST=../mpc-vst-plugins to use another framework checkout)
# Needs: python3 + Pillow, numpy and scipy, a host gcc (for shadow_art), and Zig (pip install ziglang): it compiles the C
# and the C++ (the Mutable Instruments engines, third_party/mutable). No Docker.
# Output: build/package/ANDREALPHEUS - VST - LXRXXL ZYNTH/  (skin + lxrxxl_zynth.so)
set -euo pipefail
cd "$(dirname "$0")"
MPC_VST="${MPC_VST:-$PWD/third_party/mpc-vst-plugins}"
ZIG="${ZIG:-python3 -m ziglang}"
NAME="ANDREALPHEUS - VST - LXRXXL ZYNTH"
. ./build_srcs.sh
python3 tools/gen_params.py
python3 tools/panel.py --check

mkdir -p build/tools build/arm
[ -x build/tools/shadow_art ] || gcc -O2 -w -I"$MPC_VST/tools/vendor/force-shadow/tools" -x c -o build/tools/shadow_art "$MPC_VST/tools/shadow_art.c" -lm
rm -rf build/skin
SHADOW_ART="$PWD/build/tools/shadow_art" \
  python3 "$MPC_VST/tools/gen_vst.py" vst.json 2>&1 | grep -v -e "TOUCH" -e "EDGE" -e "MODULE_DIR" || true
[ -f build/params.h ] && [ -d "build/skin/$NAME/Plugin Skins" ]
python3 tools/panel_art.py "build/skin/$NAME/Plugin Skins"     # both pages from the artwork
python3 "$MPC_VST/tools/skin_check.py" "build/skin/$NAME"        # touch boxes, edges, Q-Links, as MPC loads it

TGT="-target arm-linux-gnueabihf.2.31 -mcpu=generic+v7a+vfp3d16-d32-neon+thumb2 -ffp-contract=off"
COMMON="-O2 -fPIC -fvisibility=hidden -ffunction-sections -fdata-sections -DNDEBUG -w -std=gnu11"
OBJS=""
for f in $LXR_SRCS $PLUGIN_SRCS src/wrapper/vst2_wrap.c; do
  o="build/arm/$(echo "$f" | tr / _).o"
  $ZIG cc $TGT $COMMON -Ibuild -I"$MPC_VST/wrapper" $INCS -c "$f" -o "$o"
  OBJS="$OBJS $o"
done
CXXCOMMON="-O2 -fPIC -fvisibility=hidden -ffunction-sections -fdata-sections -DNDEBUG -w $CXXFLAGS_MI"
for f in $CXX_SRCS; do
  o="build/arm/$(echo "$f" | tr / _).o"
  $ZIG c++ $TGT $CXXCOMMON $CXXINCS -c "$f" -o "$o"
  OBJS="$OBJS $o"
done
printf '{\n  global: VSTPluginMain;\n  local: *;\n};\n' > build/arm/exports.map
$ZIG cc $TGT -shared -fPIC -Wl,--no-undefined -Wl,--version-script=build/arm/exports.map -Wl,--gc-sections -Wl,-s \
  $OBJS -lm -ldl -lpthread -o build/arm/lxrxxl_zynth.so
PKG="build/package/$NAME"
rm -rf build/package && mkdir -p "$PKG"
cp -r "build/skin/$NAME/." "$PKG/"
cp build/arm/lxrxxl_zynth.so "$PKG/"
echo "-> $PKG"

# The source lists, shared by build.sh and the test scripts.
L=third_party/lxr02open/Core
M=third_party/mutable
# the LXR's band-limited oscillator tables (const data only)
LXR_SRCS="$L/DSPAudio/wavetable.c"
# the plugin (C)
PLUGIN_SRCS="src/voice.c src/fx.c src/seq.c src/ui.c src/bl.c src/factory.c src/store.c src/preset.c src/engine.c src/sound.c src/random.c src/seqgen.c src/engines.c src/eng_osc.c src/eng_fm.c src/eng_more.c"
INCS="-Isrc -Isrc/shim -Ithird_party/lxr02open -I$L -I$L/DSPAudio"
# the Mutable Instruments engines (C++, MIT): MACRO (Braids), WAVE and PHYS (Plaits), through src/mi_bridge.cc
CXX_SRCS="src/mi_bridge.cc $M/braids/macro_oscillator.cc $M/braids/digital_oscillator.cc $M/braids/analog_oscillator.cc $M/braids/resources.cc $M/plaits/dsp/engine/wavetable_engine.cc $M/plaits/dsp/physical_modelling/modal_voice.cc $M/plaits/dsp/physical_modelling/resonator.cc $M/plaits/dsp/physical_modelling/string.cc $M/plaits/dsp/physical_modelling/string_voice.cc $M/plaits/resources.cc $M/stmlib/dsp/units.cc $M/stmlib/utils/random.cc"
CXXINCS="-Isrc -I$M"
CXXFLAGS_MI="-std=c++11 -fno-exceptions -fno-rtti -fno-threadsafe-statics -DTEST"   # TEST: stmlib's portable C paths (no Cortex-M asm)

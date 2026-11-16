#!/usr/bin/env bash
#
# mayhem/build.sh — builds everything the Mayhem layer needs, inside the commit image (as `mayhem`, in /mayhem):
#   1. the HLTV network/BSP/text parsers (rehlds/HLTV, rehlds/common) as a 32-bit, sanitized static library
#   2. one libFuzzer binary + one `-standalone` reproducer per harness, at /mayhem/<target>[-standalone]
#   3. upstream's own cppunitlite engine tests, built with the project's normal flags for mayhem/test.sh
# ReHLDS is a 32-bit-only code base (-m32); the Dockerfile installs the 32-bit libc/libstdc++ development files.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
# BitBuffer::ReadBits/WriteBits deliberately load/store a whole dword at an arbitrary byte cursor (benign on x86).
# With halting UBSan that "misaligned address" report fires on nearly every input, so the fuzzer only ever replays
# crashers and coverage never grows (0 edges). Relax only the alignment check; ASan and the rest of UBSan still halt.
SAN_RELAX="-fno-sanitize=alignment"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
R=rehlds
OUT="$SRC/build/fuzz"
TARGETS="fuzz_bsp fuzz_netchan fuzz_bitbuffer fuzz_infostring fuzz_tokenline"

INC="-I$R/HLTV/Core/src -I$R/HLTV/common -I$R/HLTV -I$R -I$R/engine -I$R/common -I$R/pm_shared -I$R/public -I$R/public/rehlds -Idep/bzip2/include -Imayhem"
# Same defines/flags as rehlds/HLTV/Core/CMakeLists.txt (minus -flto/-O3/-msse3); SanCov goes on the project
# code too, so Mayhem sees edges inside the parsers, not only in the harness.
DEFS="-DHLTV -DHLTV_FIXES -D_LINUX -DLINUX -D_GLIBCXX_USE_CXX11_ABI=0 -D_stricmp=strcasecmp -D_strnicmp=strncasecmp -D_strdup=strdup -D_vsnprintf=vsnprintf -D_snprintf=snprintf"
CXXFLAGS="-m32 -std=gnu++14 -U_FORTIFY_SOURCE -DFORTIFY_SOURCE=0 -fno-exceptions -fno-strict-aliasing -fno-strict-overflow -fpermissive -fno-sized-deallocation -w -O1 $SANITIZER_FLAGS $SAN_RELAX $DEBUG_FLAGS -fsanitize=fuzzer-no-link"
CFLAGS="-m32 -O1 -w $SANITIZER_FLAGS $SAN_RELAX $DEBUG_FLAGS -fsanitize=fuzzer-no-link"

mkdir -p "$OUT/lib" "$OUT/bzip2"

# 1a) bzip2 (the copy NetChannel decompresses reassembled messages with)
for f in dep/bzip2/src/*.c; do
  echo "$CC $CFLAGS -Idep/bzip2/include -c $f -o $OUT/bzip2/$(basename "$f" .c).o"
done | xargs -P "$MAYHEM_JOBS" -I{} sh -c '{}'

# 1b) HLTV core + shared helpers
PROJECT_SRCS="
$R/HLTV/Core/src/BSPModel.cpp $R/HLTV/Core/src/Delta.cpp $R/HLTV/Core/src/Network.cpp $R/HLTV/Core/src/World.cpp
$R/HLTV/Core/src/Server.cpp $R/HLTV/Core/src/NetSocket.cpp $R/HLTV/Core/src/public_amalgamation.cpp
$R/common/BaseSystemModule.cpp $R/common/ObjectDictionary.cpp $R/common/ObjectList.cpp $R/common/TokenLine.cpp
$R/HLTV/common/*.cpp $R/engine/mem.cpp"
for f in $PROJECT_SRCS; do
  echo "$CXX $CXXFLAGS $DEFS $INC -c $f -o $OUT/lib/$(echo "$f" | tr / _).o"
done | xargs -P "$MAYHEM_JOBS" -I{} sh -c '{}'

rm -f "$OUT/libhltv.a"
llvm-ar rcs "$OUT/libhltv.a" "$OUT"/lib/*.o "$OUT"/bzip2/*.o

# 2) harnesses
$CXX -m32 $SANITIZER_FLAGS $SAN_RELAX $DEBUG_FLAGS -c mayhem/lsan_off.cc -o "$OUT/lsan_off.o"
$CC -m32 $SANITIZER_FLAGS $SAN_RELAX $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$OUT/standalone_main.o"
for t in $TARGETS; do
  $CXX $CXXFLAGS $DEFS $INC -c "mayhem/$t.cc" -o "$OUT/$t.o"
  $CXX -m32 $SANITIZER_FLAGS $SAN_RELAX $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$OUT/$t.o" "$OUT/libhltv.a" "$OUT/lsan_off.o" -o "$SRC/$t"
  $CXX -m32 $SANITIZER_FLAGS $SAN_RELAX $DEBUG_FLAGS "$OUT/standalone_main.o" "$OUT/$t.o" "$OUT/libhltv.a" "$OUT/lsan_off.o" -o "$SRC/$t-standalone"
done

# 3) upstream's unit tests, normal flags. rehlds/lib/linux32/librt.so is a stale glibc-2.x stub that shadows the
#    system one (librt lives in libc now), so give the linker an empty librt.a ahead of it. The tree is configured
#    from scratch every time: CMake caches the compiler path, which differs between the ccache pre-fill and a plain re-run.
mkdir -p build/ut-stub
[ -f build/ut-stub/librt.a ] || llvm-ar rc build/ut-stub/librt.a
rm -rf build/ut
cmake -S . -B build/ut -DCMAKE_BUILD_TYPE=Unittests -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" \
  -DCMAKE_EXE_LINKER_FLAGS="-L$SRC/build/ut-stub"
cmake --build build/ut -j"$MAYHEM_JOBS"
cp "$R/lib/linux32/libsteam_api.so" build/ut/rehlds/
# 64-bit launcher: test.sh runs the 32-bit runner through it, so it is the process the sabotage check's LD_PRELOAD can neuter
$CC -O1 -o build/ut/exec32 mayhem/exec32.c

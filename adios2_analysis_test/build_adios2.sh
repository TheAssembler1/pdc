#!/bin/bash
# Builds ADIOS2 (with the exact feature set adios2_analysis_test/ needs:
# MPI, Derived Variables, ZFP, Sodium/libsodium; SST+UCX explicitly off,
# see below) from source, installs it, then builds the four benchmark
# programs in this directory against it.
#
# Assumes zfp and libsodium are already built and installed somewhere
# findable by CMake -- set ZFP_PREFIX/SODIUM_PREFIX below if they're not
# at this script's defaults. Needs git, cmake (>=3.12), and an
# MPI-enabled compiler wrapper (mpicc/mpicxx) already on PATH.
#
# Usage: ./build_adios2.sh
# Override any variable by exporting it first, e.g.:
#   ADIOS2_PREFIX=$HOME/adios2-install ./build_adios2.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Auto-derive a workspace root by assuming this PDC checkout lives at
# <workspace>/source/pdc (adios2_analysis_test's grandparent's parent),
# and that zfp/libsodium/adios2 install prefixes are siblings under
# <workspace>/install/ -- the convention this repo's own scripts already
# rely on (e.g. vpicio_scale.sh's PDC_DATA_LOC=.../work_space/install/pdc
# next to .../work_space/source/pdc). Only used as a *default*; every
# path below is still directly overridable by exporting the variable
# first, since this guess is wrong for any other layout.
WORKSPACE_ROOT_GUESS="$(cd "$SCRIPT_DIR/../../.." 2>/dev/null && pwd || true)"

ADIOS2_VERSION="${ADIOS2_VERSION:-v2.12.1}"
ADIOS2_SRC_DIR="${ADIOS2_SRC_DIR:-$SCRIPT_DIR/.adios2_src}"
ADIOS2_BUILD_DIR="${ADIOS2_BUILD_DIR:-$SCRIPT_DIR/.adios2_build}"
ADIOS2_PREFIX="${ADIOS2_PREFIX:-$WORKSPACE_ROOT_GUESS/install/adios2}"
ZFP_PREFIX="${ZFP_PREFIX:-$WORKSPACE_ROOT_GUESS/install/zfp}"
SODIUM_PREFIX="${SODIUM_PREFIX:-$WORKSPACE_ROOT_GUESS/install/libsodium}"
JOBS="${JOBS:-$(nproc)}"

echo "== ADIOS2 version:    $ADIOS2_VERSION"
echo "== ADIOS2 source:     $ADIOS2_SRC_DIR"
echo "== ADIOS2 build dir:  $ADIOS2_BUILD_DIR"
echo "== ADIOS2 install to: $ADIOS2_PREFIX"
echo "== zfp prefix:        $ZFP_PREFIX"
echo "== libsodium prefix:  $SODIUM_PREFIX"
echo "== parallel jobs:     $JOBS"
echo

if [ ! -d "$ZFP_PREFIX" ]; then
  echo "ERROR: ZFP_PREFIX ($ZFP_PREFIX) does not exist." >&2
  echo "  This path was guessed as \$WORKSPACE_ROOT/install/zfp -- if zfp is built" >&2
  echo "  and installed somewhere else on this machine, rerun as:" >&2
  echo "    ZFP_PREFIX=/path/to/zfp/install $0" >&2
  echo "  If zfp isn't built at all yet, build/install it first." >&2
  exit 1
fi
if [ ! -d "$SODIUM_PREFIX" ]; then
  echo "ERROR: SODIUM_PREFIX ($SODIUM_PREFIX) does not exist." >&2
  echo "  This path was guessed as \$WORKSPACE_ROOT/install/libsodium -- if libsodium" >&2
  echo "  is built and installed somewhere else on this machine, rerun as:" >&2
  echo "    SODIUM_PREFIX=/path/to/libsodium/install $0" >&2
  echo "  If libsodium isn't built at all yet, build/install it first." >&2
  exit 1
fi

if [ ! -d "$ADIOS2_SRC_DIR" ]; then
  echo "== Cloning ADIOS2 $ADIOS2_VERSION..."
  git clone --quiet https://github.com/ornladios/ADIOS2.git "$ADIOS2_SRC_DIR"
fi
git -C "$ADIOS2_SRC_DIR" checkout --quiet "$ADIOS2_VERSION"

# Always a fresh configure: ADIOS2's generated ADIOSConfig.h has been
# seen not to pick up a feature-flag change on an incremental
# `cmake .` reconfigure of an existing build dir (Derived_Variable
# specifically), so this directory is wiped every run rather than risk
# a stale, half-updated build.
echo "== Configuring (clean)..."
rm -rf "$ADIOS2_BUILD_DIR"
mkdir -p "$ADIOS2_BUILD_DIR"
cmake -S "$ADIOS2_SRC_DIR" -B "$ADIOS2_BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$ADIOS2_PREFIX" \
  -DCMAKE_C_COMPILER=mpicc \
  -DCMAKE_CXX_COMPILER=mpicxx \
  -DCMAKE_PREFIX_PATH="$ZFP_PREFIX;$SODIUM_PREFIX" \
  -DADIOS2_USE_MPI=ON \
  -DADIOS2_USE_HDF5=OFF \
  -DADIOS2_USE_CUDA=OFF \
  -DADIOS2_USE_Kokkos=OFF \
  -DADIOS2_USE_Fortran=OFF \
  -DADIOS2_USE_Python=OFF \
  -DADIOS2_USE_ZeroMQ=OFF \
  -DADIOS2_USE_SST=OFF \
  -DADIOS2_USE_UCX=OFF \
  -DADIOS2_USE_Derived_Variable=ON \
  -DADIOS2_USE_ZFP=ON \
  -DADIOS2_USE_Sodium=ON \
  -DBUILD_TESTING=OFF \
  -DADIOS2_BUILD_EXAMPLES=OFF

echo "== Building ($JOBS jobs)..."
cmake --build "$ADIOS2_BUILD_DIR" -j"$JOBS"

echo "== Installing to $ADIOS2_PREFIX..."
cmake --install "$ADIOS2_BUILD_DIR"

echo "== Building adios2_analysis_test programs..."
ADIOS2_PREFIX="$ADIOS2_PREFIX" make -C "$SCRIPT_DIR" clean
ADIOS2_PREFIX="$ADIOS2_PREFIX" make -C "$SCRIPT_DIR"

echo
echo "== Done. Binaries in $SCRIPT_DIR:"
ls -1 "$SCRIPT_DIR"/adios2_bench_*

# Source this before building or running the PR #305 checkpoint/restart benchmark on
# LLNL Tuolumne (also works on El Capitan / RZAdams; same TOSS 4 + Cray PE + Flux stack).
#
#   source ./tuolumne_env.sh
#
# Everything is overridable: export a variable before sourcing to change it.

# ---------------------------------------------------------------------------
# Modules (Cray PE on TOSS 4). Only touched when the `module` command exists, so the
# same scripts also run on a laptop / CI box with MERCURY_DIR etc. preset.
# ---------------------------------------------------------------------------
if command -v module >/dev/null 2>&1; then
    module load cmake >/dev/null 2>&1 || true
    # cray-mpich is in the default Tuolumne environment; load it if someone purged it.
    module is-loaded cray-mpich >/dev/null 2>&1 || module load cray-mpich >/dev/null 2>&1 || true
fi

# Repo root = directory holding this file (the PR #305 "new" source tree).
export PDC_BENCH_SRC="${PDC_BENCH_SRC:-$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)}"

# Work area. $HOME and /usr/workspace are NFS on LC; use Lustre for builds, checkpoints, results.
if [ -z "${PDC_BENCH_ROOT:-}" ]; then
    if [ -d "/p/lustre5/$USER" ]; then
        PDC_BENCH_ROOT="/p/lustre5/$USER/pdc-ckpt-bench"
    else
        PDC_BENCH_ROOT="$PDC_BENCH_SRC/_ckpt_bench"
    fi
fi
export PDC_BENCH_ROOT

# Dependencies: same versions as PDC's documented install (libfabric 1.18.0, Mercury 2.2.0).
export LIBFABRIC_VERSION="${LIBFABRIC_VERSION:-1.18.0}"
export MERCURY_VERSION="${MERCURY_VERSION:-2.2.0}"
export LIBFABRIC_DIR="${LIBFABRIC_DIR:-$PDC_BENCH_ROOT/install/libfabric}"
export MERCURY_DIR="${MERCURY_DIR:-$PDC_BENCH_ROOT/install/mercury}"

# "prev" = develop at the PR's merge-base (what the PR's "Previous" column measures),
# "new"  = this branch (PR #305 head + these scripts).
export PREV_REF="${PREV_REF:-27b5f72ecb0d172791193a8e9cf206bb07545634}"
export PREV_SRC="${PREV_SRC:-$PDC_BENCH_ROOT/src/pdc-prev}"
export PREV_BUILD="${PREV_BUILD:-$PDC_BENCH_ROOT/build/prev}"
export NEW_BUILD="${NEW_BUILD:-$PDC_BENCH_ROOT/build/new}"

# Compiler: LC's cray-mpich module provides mpicc; fall back to the Cray `cc` wrapper.
if [ -z "${PDC_CC:-}" ]; then
    PDC_CC="$(command -v mpicc || command -v cc || true)"
fi
export PDC_CC
export BUILD_TYPE="${BUILD_TYPE:-Release}"

# Mercury transport. Servers and clients share one node in this benchmark (same as the PR's
# Perlmutter runs), so TCP over loopback is used: it needs no Slingshot/CXI auth-key setup.
export HG_TRANSPORT="${HG_TRANSPORT:-ofi+tcp}"
export HG_HOST="${HG_HOST:-127.0.0.1}"

export LD_LIBRARY_PATH="$MERCURY_DIR/lib:$MERCURY_DIR/lib64:$LIBFABRIC_DIR/lib:$LIBFABRIC_DIR/lib64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export PKG_CONFIG_PATH="$LIBFABRIC_DIR/lib/pkgconfig:$MERCURY_DIR/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"

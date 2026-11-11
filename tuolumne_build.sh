#!/bin/bash
# Build everything needed to reproduce the PR #305 checkpoint/restart numbers on Tuolumne.
#
#   source ./tuolumne_env.sh
#   ./tuolumne_build.sh            # deps (if missing) + prev + new
#   ./tuolumne_build.sh deps|prev|new   # just one step
#
# Run on a login node (needs outbound HTTPS for the dependency tarballs) or inside
# `flux alloc -N1` for a faster parallel build.
#
# Two PDC builds are produced, both Release + PDC_TIMING=ON:
#   prev : develop at $PREV_REF (PR merge-base) + the PR's two kvtag scale test programs
#          + a 3-line patch that prints "total restart time" exactly like the PR does
#          (develop already prints "Total checkpoint time" and "total close time").
#   new  : this branch (PR #305).
set -euo pipefail

: "${PDC_BENCH_SRC:?source ./tuolumne_env.sh first}"
JOBS="${JOBS:-$(nproc)}"
[ -n "$PDC_CC" ] || { echo "no MPI C compiler found (mpicc / cc); load cray-mpich" >&2; exit 1; }
command -v cmake >/dev/null || { echo "cmake not found (module load cmake)" >&2; exit 1; }

# pdc_timing.h defines its PDC_TIMING globals in the header (no `extern`), so every file that
# includes it gets its own copy. PDC's CMakeLists adds -fcommon only for GCC; Cray clang
# (Tuolumne's mpicc) otherwise fails to link with "duplicate symbol: pdc_timings".
PDC_CFLAGS="${PDC_CFLAGS:--fcommon}"
# PDC calls shm_open/shm_unlink but never links librt. On TOSS 4 (RHEL 8, glibc 2.28) those
# live in librt, and lld rejects pdc_server ("undefined reference: shm_open"). Newer glibc
# moved them into libc, where -lrt is a harmless stub.
PDC_LIBS="${PDC_LIBS:--lrt}"

STEPS=("$@")
[ ${#STEPS[@]} -gt 0 ] || STEPS=(deps prev new)

fetch() { # url dest
    if command -v curl >/dev/null; then curl -fL --retry 3 -o "$2" "$1"; else wget -O "$2" "$1"; fi
}

build_deps() {
    local dl="$PDC_BENCH_ROOT/downloads"
    mkdir -p "$dl"

    if [ -f "$LIBFABRIC_DIR/include/rdma/fabric.h" ]; then
        echo "== libfabric: found $LIBFABRIC_DIR"
    else
        echo "== libfabric $LIBFABRIC_VERSION -> $LIBFABRIC_DIR"
        local t="$dl/libfabric-$LIBFABRIC_VERSION.tar.bz2"
        [ -f "$t" ] || fetch "https://github.com/ofiwg/libfabric/releases/download/v$LIBFABRIC_VERSION/libfabric-$LIBFABRIC_VERSION.tar.bz2" "$t"
        rm -rf "$dl/libfabric-$LIBFABRIC_VERSION" && tar -xjf "$t" -C "$dl"
        (cd "$dl/libfabric-$LIBFABRIC_VERSION" &&
            ./configure --prefix="$LIBFABRIC_DIR" CC="$PDC_CC" CFLAGS="-O2" \
                --enable-tcp=yes --enable-rxm=yes --enable-sockets=yes \
                --disable-verbs --disable-psm3 --disable-efa --disable-usnic --disable-opx &&
            make -j"$JOBS" && make install)
    fi

    if [ -f "$MERCURY_DIR/include/mercury.h" ]; then
        echo "== mercury: found $MERCURY_DIR"
    else
        echo "== mercury $MERCURY_VERSION -> $MERCURY_DIR"
        local t="$dl/mercury-$MERCURY_VERSION.tar.bz2"
        [ -f "$t" ] || fetch "https://github.com/mercury-hpc/mercury/releases/download/v$MERCURY_VERSION/mercury-$MERCURY_VERSION.tar.bz2" "$t"
        rm -rf "$dl/mercury-$MERCURY_VERSION" && tar -xjf "$t" -C "$dl"
        mkdir -p "$dl/mercury-$MERCURY_VERSION/build"
        (cd "$dl/mercury-$MERCURY_VERSION/build" &&
            cmake -DCMAKE_INSTALL_PREFIX="$MERCURY_DIR" -DCMAKE_C_COMPILER="$PDC_CC" \
                -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF \
                -DNA_USE_OFI=ON -DNA_USE_SM=OFF -DCMAKE_PREFIX_PATH="$LIBFABRIC_DIR" .. &&
            make -j"$JOBS" && make install)
    fi
}

configure_pdc() { # src build
    mkdir -p "$2"
    cmake -S "$1" -B "$2" \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_C_COMPILER="$PDC_CC" \
        -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=ON -DBUILD_MPI_TESTING=ON \
        -DPDC_ENABLE_MPI=ON -DPDC_TIMING=ON -DPDC_ENABLE_CHECKPOINT=ON \
        -DMERCURY_DIR="$MERCURY_DIR" -DCMAKE_INSTALL_PREFIX="$2/install" \
        -DCMAKE_C_FLAGS="$PDC_CFLAGS" -DCMAKE_C_STANDARD_LIBRARIES="$PDC_LIBS" \
        ${PDC_CMAKE_EXTRA:-}
    cmake --build "$2" -j"$JOBS" --target pdc_server close_server kvtag_add_scale kvtag_get_verify_scale
}

prepare_prev_src() {
    if ! git -C "$PDC_BENCH_SRC" cat-file -e "$PREV_REF^{commit}" 2>/dev/null; then
        echo "== fetching $PREV_REF from hpc-io/pdc"
        git -C "$PDC_BENCH_SRC" fetch https://github.com/hpc-io/pdc.git develop
    fi
    if [ ! -d "$PREV_SRC" ]; then
        mkdir -p "$(dirname "$PREV_SRC")"
        git -C "$PDC_BENCH_SRC" worktree add --detach "$PREV_SRC" "$PREV_REF"
    fi
    git -C "$PREV_SRC" checkout -q --detach "$PREV_REF"
    git -C "$PREV_SRC" reset -q --hard
    git -C "$PREV_SRC" clean -qfd

    # 1) the PR's client programs, so prev and new run the identical workload
    cp "$PDC_BENCH_SRC/src/tests/tags/kvtag_add_scale.c" \
        "$PDC_BENCH_SRC/src/tests/tags/kvtag_get_verify_scale.c" "$PREV_SRC/src/tests/tags/"
    perl -0pi -e 's|(\n  query/query_data\n)|\n  query/query_data\n  tags/kvtag_add_scale\n  tags/kvtag_get_verify_scale\n|' \
        "$PREV_SRC/src/tests/CMakeLists.txt"
    grep -q 'tags/kvtag_add_scale' "$PREV_SRC/src/tests/CMakeLists.txt" ||
        { echo "failed to register kvtag scale tests in prev CMakeLists" >&2; exit 1; }

    # 2) restart timing print, identical to the one PR #305 adds to PDC_Server_restart()
    perl -0pi -e 's|(    pdc_server_timings->PDCserver_restart \+= MPI_Wtime\(\) - start;\n)(#endif\n)|$1#ifdef ENABLE_MPI\n    MPI_Barrier(MPI_COMM_WORLD);\n    if (!pdc_server_rank_g)\n        LOG_INFO("total restart time = %lf\\n", MPI_Wtime() - start);\n#endif\n$2|' \
        "$PREV_SRC/src/server/pdc_server.c"
    grep -q 'total restart time' "$PREV_SRC/src/server/pdc_server.c" ||
        { echo "failed to patch restart timing into prev pdc_server.c" >&2; exit 1; }
    echo "== prev source ready: $PREV_SRC @ $(git -C "$PREV_SRC" rev-parse --short HEAD) (+bench patch)"
}

for step in "${STEPS[@]}"; do
    case "$step" in
    deps) build_deps ;;
    prev) prepare_prev_src && configure_pdc "$PREV_SRC" "$PREV_BUILD" ;;
    new) configure_pdc "$PDC_BENCH_SRC" "$NEW_BUILD" ;;
    *) echo "unknown step: $step (deps|prev|new)" >&2; exit 1 ;;
    esac
done

for b in "$PREV_BUILD" "$NEW_BUILD"; do
    [ -x "$b/bin/pdc_server" ] && echo "built: $b/bin/{pdc_server,close_server,kvtag_add_scale,kvtag_get_verify_scale}"
done
exit 0

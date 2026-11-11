#!/bin/bash
# Reproduce the PR #305 (BULKI checkpoint) strong/weak scaling and 2nd-checkpoint numbers.
# Must run INSIDE a single-node Flux allocation on Tuolumne:
#
#   flux alloc -N1 -x -t 4h -q pbatch      # or: flux batch ./tuolumne_ckpt_bench.flux
#   source ./tuolumne_env.sh
#   ./tuolumne_ckpt_bench.sh
#   ./tuolumne_ckpt_summary.py "$PDC_BENCH_ROOT/results/<run>/results.csv"
#
# One trial = the PR's checkpoint/restart workflow
# (src/tests/run_checkpoint_restart_mpi_test_with_prog_param.sh):
#   1. start S servers (fresh)    -> kvtag_add_scale  N_OBJ N_TAG on C clients
#   2. close_server               -> 1st checkpoint time, 1st close time
#   3. restart S servers          -> restart time
#   4. kvtag_get_verify_scale N_OBJ N_TAG on C clients (must pass)
#   5. close_server               -> 2nd checkpoint time, 2nd close time
# Every rank is launched with `-c 2`, as in the PR.
#
# Knobs (env vars):
#   VARIANTS="prev new"   SUITES="strong weak"   TRIALS=5   SERVERS="2 4 8 16"
#   STRONG_NOBJ=100000    WEAK_NOBJ_PER_SERVER=5000 (=> 10K/20K/40K/80K)   N_TAG=100
#   N_CLIENTS=1           client ranks. Keep 1: kvtag_add_scale (from the PR) deadlocks with >1
#                         client because all N_TAG tags land on rank 0, which calls MPI_Barrier
#                         once per tag while the other ranks do not. So the PR's numbers can only
#                         have come from a single client rank.
#   CORES_PER_TASK=2      LAUNCHER=flux|mpiexec|srun   LAUNCH_EXTRA="..."  (extra launcher args)
#   CKPT_ON=lustre|tmp    (lustre: checkpoint under the run dir on Lustre; tmp: node-local /tmp)
#   STEP_TIMEOUT=1800     (seconds per client/close step)  KEEP_CKPT=0
#   RUN_NAME=<name>       (results go to $PDC_BENCH_ROOT/results/$RUN_NAME)
set -uo pipefail

: "${PDC_BENCH_ROOT:?source ./tuolumne_env.sh first}"

VARIANTS="${VARIANTS:-prev new}"
SUITES="${SUITES:-strong weak}"
TRIALS="${TRIALS:-5}"
SERVERS="${SERVERS:-2 4 8 16}"
STRONG_NOBJ="${STRONG_NOBJ:-100000}"
WEAK_NOBJ_PER_SERVER="${WEAK_NOBJ_PER_SERVER:-5000}"
N_TAG="${N_TAG:-100}"
N_CLIENTS="${N_CLIENTS:-1}"
CORES_PER_TASK="${CORES_PER_TASK:-2}"
LAUNCHER="${LAUNCHER:-flux}"
LAUNCH_EXTRA="${LAUNCH_EXTRA:-}"
CKPT_ON="${CKPT_ON:-lustre}"
STEP_TIMEOUT="${STEP_TIMEOUT:-1800}"
READY_TIMEOUT="${READY_TIMEOUT:-300}"
KEEP_CKPT="${KEEP_CKPT:-0}"
RUN_NAME="${RUN_NAME:-$(date +%Y%m%d-%H%M%S)${FLUX_JOB_ID:+-$FLUX_JOB_ID}}"

OUT="$PDC_BENCH_ROOT/results/$RUN_NAME"
CSV="$OUT/results.csv"
mkdir -p "$OUT"

if [ "$LAUNCHER" = flux ] && [ -z "${FLUX_URI:-}" ]; then
    echo "Not inside a Flux allocation. Use: flux alloc -N1 -x -t 4h -q pbatch  (or flux batch ./tuolumne_ckpt_bench.flux)" >&2
    exit 1
fi

# mkcmd <ntasks>: sets LCMD to the launcher prefix for an <ntasks>-rank job step
mkcmd() {
    case "$LAUNCHER" in
    # No --exclusive: server and client job steps must share the node concurrently.
    # Steps inside one Flux allocation share its Slingshot VNI and get disjoint cores.
    flux) LCMD=(flux run -N1 -n "$1" -c "$CORES_PER_TASK") ;;
    srun) LCMD=(srun -N1 -n "$1" -c "$CORES_PER_TASK" --overlap) ;;
    mpiexec) LCMD=(mpiexec -n "$1") ;;
    *) echo "unknown LAUNCHER=$LAUNCHER" >&2; exit 1 ;;
    esac
    [ -z "$LAUNCH_EXTRA" ] || LCMD+=($LAUNCH_EXTRA)
}

# value of "<pattern> = <float>" from a log, or NA
grab() {
    local v
    v="$(grep -aoE "$1 = [0-9.eE+-]+" "$2" 2>/dev/null | tail -1 | awk '{print $NF}')"
    echo "${v:-NA}"
}

wait_ready() { # log pid
    local t=0
    while ! grep -aq 'Server ready' "$1" 2>/dev/null; do
        kill -0 "$2" 2>/dev/null || return 1
        sleep 0.5
        t=$((t + 1))
        [ $t -lt $((READY_TIMEOUT * 2)) ] || return 1
    done
}

stop_server() { # pid
    local t=0
    while kill -0 "$1" 2>/dev/null && [ $t -lt 600 ]; do sleep 0.5; t=$((t + 1)); done
    if kill -0 "$1" 2>/dev/null; then
        echo "    server did not exit after close_server; killing" >&2
        kill "$1" 2>/dev/null; sleep 5; kill -9 "$1" 2>/dev/null
    fi
    wait "$1" 2>/dev/null
}

# phase <bin> <dir> <S> <C> <restart-arg> <client-exe> <nobj> <ntag> <log-suffix>
# Returns 0 on success, client exit code on client failure, 90 server start failure,
# 91 close_server failure.
phase() {
    local bin="$1" dir="$2" S="$3" C="$4" rs="$5" exe="$6" nobj="$7" ntag="$8" tag="$9" spid rc
    mkcmd "$S"
    "${LCMD[@]}" "$bin/pdc_server" $rs >"$dir/server$tag.log" 2>&1 &
    spid=$!
    if ! wait_ready "$dir/server$tag.log" "$spid"; then
        echo "    server$tag failed to start (see $dir/server$tag.log)" >&2
        kill "$spid" 2>/dev/null; wait "$spid" 2>/dev/null
        return 90
    fi
    mkcmd "$C"
    timeout "$STEP_TIMEOUT" "${LCMD[@]}" "$bin/$exe" "$nobj" "$ntag" >"$dir/client$tag.log" 2>&1
    rc=$?
    mkcmd "$S"
    if ! timeout "$STEP_TIMEOUT" "${LCMD[@]}" "$bin/close_server" >"$dir/close$tag.log" 2>&1; then
        [ $rc -ne 0 ] || rc=91
    fi
    stop_server "$spid"
    return $rc
}

run_trial() { # variant suite S nobj trial
    local variant="$1" suite="$2" S="$3" nobj="$4" trial="$5"
    local C="$N_CLIENTS" bin dir ckdir rc1 rc2
    case "$variant" in
    prev) bin="$PREV_BUILD/bin" ;;
    new) bin="$NEW_BUILD/bin" ;;
    *) echo "unknown variant $variant" >&2; return ;;
    esac
    dir="$OUT/$variant/$suite/s${S}_c${C}_o${nobj}_t${N_TAG}/trial$trial"
    mkdir -p "$dir"
    if [ "$CKPT_ON" = tmp ]; then ckdir="/tmp/$USER-pdc-ckpt-$$"; else ckdir="$dir/pdc_tmp"; fi
    rm -rf "$ckdir" "$dir/pdc_data"
    mkdir -p "$ckdir"
    export PDC_TMPDIR="$ckdir"
    export PDC_DATA_LOC="$dir/pdc_data"

    echo "[$(date +%T)] $variant $suite servers=$S clients=$C obj=$nobj tag=$N_TAG trial=$trial"
    (cd "$dir" && phase "$bin" "$dir" "$S" "$C" "" kvtag_add_scale "$nobj" "$N_TAG" 1)
    rc1=$?
    if [ $rc1 -eq 0 ]; then
        (cd "$dir" && phase "$bin" "$dir" "$S" "$C" restart kvtag_get_verify_scale "$nobj" "$N_TAG" 2)
        rc2=$?
    else
        rc2=NA
    fi

    local ck1 cl1 rst ck2 cl2
    ck1="$(grab 'Total checkpoint time' "$dir/server1.log")"
    cl1="$(grab 'total close time' "$dir/close1.log")"
    rst="$(grab 'total restart time' "$dir/server2.log")"
    ck2="$(grab 'Total checkpoint time' "$dir/server2.log")"
    cl2="$(grab 'total close time' "$dir/close2.log")"
    echo "    ckpt1=$ck1 close1=$cl1 restart=$rst ckpt2=$ck2 close2=$cl2 add_rc=$rc1 verify_rc=$rc2"
    echo "$variant,$suite,$S,$C,$nobj,$N_TAG,$trial,$ck1,$cl1,$rst,$ck2,$cl2,$rc1,$rc2" >>"$CSV"

    [ "$KEEP_CKPT" = 1 ] || rm -rf "$ckdir" "$dir/pdc_data"
}

# Record the environment for the write-up.
{
    echo "date: $(date -Is)"
    echo "host: $(hostname)  flux_job: ${FLUX_JOB_ID:-n/a}"
    echo "launcher: $LAUNCHER -c $CORES_PER_TASK $LAUNCH_EXTRA"
    echo "HG_TRANSPORT=$HG_TRANSPORT HG_HOST=$HG_HOST CKPT_ON=$CKPT_ON"
    echo "new : $PDC_BENCH_SRC @ $(git -C "$PDC_BENCH_SRC" rev-parse HEAD 2>/dev/null)"
    echo "prev: $PREV_SRC @ $(git -C "$PREV_SRC" rev-parse HEAD 2>/dev/null) (+bench patch)"
    echo "variants=$VARIANTS suites=$SUITES servers=$SERVERS trials=$TRIALS"
    echo "strong_nobj=$STRONG_NOBJ weak_nobj_per_server=$WEAK_NOBJ_PER_SERVER n_tag=$N_TAG n_clients=$N_CLIENTS"
    command -v module >/dev/null 2>&1 && module -t list 2>&1
    command -v flux >/dev/null 2>&1 && [ -n "${FLUX_URI:-}" ] && flux resource list
} >"$OUT/env.txt" 2>&1

[ -f "$CSV" ] || echo "variant,suite,n_servers,n_clients,n_obj,n_tag,trial,ckpt1_s,close1_s,restart_s,ckpt2_s,close2_s,add_rc,verify_rc" >"$CSV"

for v in $VARIANTS; do
    b="$PREV_BUILD"; [ "$v" = new ] && b="$NEW_BUILD"
    for exe in pdc_server close_server kvtag_add_scale kvtag_get_verify_scale; do
        [ -x "$b/bin/$exe" ] || { echo "missing $b/bin/$exe -- run ./tuolumne_build.sh" >&2; exit 1; }
    done
done

# Interleave variants inside each trial so drift on the node/file system hits both equally.
for suite in $SUITES; do
    for S in $SERVERS; do
        case "$suite" in
        strong) nobj="$STRONG_NOBJ" ;;
        weak) nobj=$((WEAK_NOBJ_PER_SERVER * S)) ;;
        *) echo "unknown suite $suite" >&2; exit 1 ;;
        esac
        for t in $(seq 1 "$TRIALS"); do
            for v in $VARIANTS; do
                run_trial "$v" "$suite" "$S" "$nobj" "$t"
            done
        done
    done
done

echo
echo "results: $CSV"
python3 "$PDC_BENCH_SRC/tuolumne_ckpt_summary.py" "$CSV" | tee "$OUT/summary.md"

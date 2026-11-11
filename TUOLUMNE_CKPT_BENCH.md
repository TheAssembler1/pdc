# PR #305 checkpoint/restart benchmark on LLNL Tuolumne

These scripts reproduce the BULKI-serializer numbers posted on
[hpc-io/pdc#305](https://github.com/hpc-io/pdc/pull/305) (originally from Perlmutter) on Tuolumne.
They run the strong scaling, weak scaling and 1st-vs-2nd checkpoint tables, comparing
**prev** (develop at the PR merge-base) against **new** (this branch = PR head).

| file | purpose |
|---|---|
| `tuolumne_env.sh` | modules, paths, Mercury transport; source first |
| `tuolumne_build.sh` | builds libfabric 1.18.0 + Mercury 2.2.0, then PDC `prev` and `new` (Release, `PDC_TIMING=ON`) |
| `tuolumne_ckpt_bench.sh` | runs the trials inside a 1-node Flux allocation, writes `results.csv` + `summary.md` |
| `tuolumne_ckpt_bench.flux` | `flux batch` wrapper for the above |
| `tuolumne_ckpt_summary.py` | CSV → the PR's Markdown tables (min/avg/max, New/Prev ratio, 2nd-checkpoint overhead) |

## Quick start

```bash
ssh tuo
git clone -b tuolumne-ckpt-bench https://github.com/TheAssembler1/pdc.git && cd pdc
source ./tuolumne_env.sh            # work dir defaults to /p/lustre5/$USER/pdc-ckpt-bench
./tuolumne_build.sh                 # login node; downloads deps from GitHub

# smoke test (~minutes)
flux batch -q pdebug -t 30m --env=TRIALS=1 --env=SERVERS="2 4" --env=STRONG_NOBJ=10000 ./tuolumne_ckpt_bench.flux

# full run: 2 variants x (strong + weak) x 4 server counts x 5 trials
flux batch ./tuolumne_ckpt_bench.flux          # add --setattr=bank=<bank> if needed
flux jobs                                      # results: $PDC_BENCH_ROOT/results/<run>/summary.md
```

Interactive alternative: `flux alloc -N1 -x -t 4h -q pbatch`, then `source ./tuolumne_env.sh && ./tuolumne_ckpt_bench.sh`.

## What's measured (same as the PR)

Each trial follows the PR's `run_checkpoint_restart_mpi_test_with_prog_param.sh` workflow:
start S servers → `kvtag_add_scale N 100` → `close_server` (**1st checkpoint**, **close**) →
restart servers (**restart**) → `kvtag_get_verify_scale N 100` (must pass) → `close_server`
(**2nd checkpoint/close**). The values come from the servers' and `close_server`'s own prints:
`Total checkpoint time`, `total close time`, `total restart time`.

* strong: 100K objects / 100 tags, servers = 2, 4, 8, 16
* weak: 5K objects per server (10K/20K/40K/80K) / 100 tags
* every rank is launched with `-c 2`; 5 trials; prev and new are interleaved per trial

**prev patch:** develop doesn't print restart time, so `tuolumne_build.sh` applies the PR's
`total restart time` print to the prev tree. It also adds the PR's two kvtag scale programs,
so both variants run the same client code. Nothing else changes.

## Tuolumne specifics / choices

* **Flux:** all job steps share one exclusive node allocation. Server and client steps run
  concurrently without `--exclusive`, so each gets its own cores. Steps in one allocation also
  share the allocation's Slingshot VNI.
* **Transport:** `HG_TRANSPORT=ofi+tcp`, `HG_HOST=127.0.0.1`. Everything is on one node, as in the
  PR runs, so `ofi+cxi` would only add CXI auth-key handling, which develop doesn't do yet. The
  checkpoint/restart cost is server-local anyway.
* **Checkpoint location:** `CKPT_ON=lustre` (default; under the run dir on `/p/lustre5`) or
  `CKPT_ON=tmp` (node-local `/tmp`, removes file system noise).
* **Clients:** `N_CLIENTS=1`. The PR's `kvtag_add_scale` deadlocks with more than one client
  rank. All tags go to rank 0, which calls `MPI_Barrier` once per tag in its progress loop while
  the other ranks skip it. The PR's own `kvtag_checkpoint_restart_mpi` ctest (2 servers, 4
  clients) hangs for the same reason.
* LC asks that El Capitan-class results go to elcapitan-coe-issues@llnl.gov for review before
  public release, so check before posting the numbers on the PR.

## Knobs

`VARIANTS="prev new"  SUITES="strong weak"  TRIALS=5  SERVERS="2 4 8 16"  STRONG_NOBJ=100000
WEAK_NOBJ_PER_SERVER=5000  N_TAG=100  N_CLIENTS=1  CORES_PER_TASK=2  CKPT_ON=lustre|tmp
STEP_TIMEOUT=1800  KEEP_CKPT=0  RUN_NAME=...  LAUNCH_EXTRA="..."  LAUNCHER=flux|mpiexec|srun`

The build honours `PDC_BENCH_ROOT`, `MERCURY_DIR`/`LIBFABRIC_DIR` (reuse existing installs),
`PREV_REF`, `BUILD_TYPE`, `PDC_CC`, and `PDC_CMAKE_EXTRA`.
`LAUNCHER=mpiexec` lets you run the same thing on a workstation.

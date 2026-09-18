# tuo_scale_scripts

Flux-scheduled scaling study for `vpic_bdcats` (`src/tests/misc/vpic_bdcats.c`)
on Tuolumne, from 1 to 128 nodes.

## What `vpic_bdcats` does

Combines the existing `vpicio` (write) and `bdcats` (read) benchmarks into
one process: writes 5 timesteps of the standard 8-array VPIC-shaped
particle dataset (dX, dY, dZ, Ux, Uy, Uz, q, i), closing each timestep's
objects as it goes, then -- still in the same process, so the server's
region cache from the writes is still warm rather than dropped by a
server restart between two separate job steps -- reopens every one of
those same per-timestep objects by name and reads them back, verifying
every value matches what was written.

Two transfer modes (`argv[3]`):
- `sync` -- `PDCregion_transfer_start_all_mpi` immediately followed by
  `PDCregion_transfer_wait_all`, no overlap.
- `async` -- `sleep(sleeptime)` between start and wait, standing in for
  compute overlapped with in-flight I/O.

Every distinct PDC client API call is timed (`pdc_call_stats.h`'s
`PDC_TIMED` macro) and pooled into a mean/stdev/count across every rank.
Output is a CSV to stdout:

```
record_type,name,step,mean_s,stdev_s,count,value
api_call,PDCinit,,0.136790256,0.005833318,4,
api_call,PDCobj_create_mpi,,0.000043734,0.000028744,160,
...
throughput_write_MBps,,0,,,,134.091060
...
throughput_read_MBps,,0,,,,131.664722
...
total_data_size_bytes,,,,,,2621440
data_size_per_rank_bytes,,,,,,655360
```

- `api_call` rows: one per distinct PDC API call name, pooled across every
  rank and every occurrence (e.g. `PDCobj_create_mpi` has
  `count = nranks * 8 objects * steps`).
- `throughput_write_MBps` / `throughput_read_MBps`: one row per timestep,
  computed from the total bytes moved across all ranks that timestep
  divided by the wall-clock window for that timestep's transfer.
- `total_data_size_bytes` / `data_size_per_rank_bytes`: the whole run, one
  direction (write and read move the identical volume), across all ranks
  vs. one rank.

## Files

- `common.sh` -- shared config sourced by every script below: node-count
  list (1, 2, 4, 8, 16, 32, 64, 128), `SERVERS_PER_NODE` (4),
  `CLIENTS_PER_NODE` (32), `STEPS` (5), and `NUMPARTICLES` -- computed
  once, held constant across the whole sweep (weak scaling), sized so
  that the **128-node step alone writes ~4 TiB** across all 5 timesteps
  (smaller node counts in the same sweep write proportionally less).
  Requires `PDC_DATA_LOC` to already be set to real parallel scratch --
  see below.
- `vpic_bdcats_job.flux.sh` -- the actual per-node-count, per-mode `flux
  batch` job: cleans out `PDC_DATA_LOC`/`PDC_TMPDIR` from any previous run
  first, starts `pdc_server`, runs `vpic_bdcats`, closes the server,
  extracts that run's CSV. Not invoked directly.
- `run_sync.sh` / `run_async.sh` -- sweep drivers, one per transfer mode,
  submitting `vpic_bdcats_job.flux.sh` once per node count and blocking on
  `flux job attach` before moving to the next (chosen over Flux's
  dependency flags since those vary by version). **`run_async.sh`'s sleep
  duration (`SLEEP_TIME`) is declared at the very top of the file** --
  edit it there.
- `run_small_scale.sh` -- a fast, 1-node, tiny-particle-count (1024/rank,
  2 steps) sanity check of the whole pipeline (flux submission, server
  startup, write+read-back, CSV extraction) for both modes. **Run this
  first**, before either sweep, to catch a bad `PDC_DATA_LOC` or a Flux
  flag mismatch without waiting on a real sweep step or writing anywhere
  near 4 TiB.

## Required: `PDC_DATA_LOC`

At up to 4 TiB for a single run, this **must** point at real parallel
scratch, not wherever `BIN_DIR` happens to sit. `common.sh` requires it
to already be set (it will refuse to run otherwise):

```
export PDC_DATA_LOC=/p/lustre1/$USER/pdc_vpic_bdcats_scale
```

Every job first removes everything under `PDC_DATA_LOC` (and
`PDC_TMPDIR`, which defaults to the same path) before starting its own
server, so stale data from a previous run never lingers into the next.

## Build

Built as part of the normal PDC test suite (`-DBUILD_TESTING=ON`); the
binary lands at `<build>/bin/vpic_bdcats` alongside `pdc_server` and
`close_server`. `common.sh` auto-detects `BIN_DIR` as
`tuo_scale_scripts/../build/bin` -- override it if your build lives
elsewhere.

## Run

```
export PDC_DATA_LOC=/p/lustre1/$USER/pdc_vpic_bdcats_scale

./run_small_scale.sh   # sanity check first -- 1 node, seconds, both modes
./run_sync.sh          # full sweep, 1..128 nodes, sync mode
./run_async.sh         # full sweep, 1..128 nodes, async mode
```

Override any of `SERVERS_PER_NODE`, `CLIENTS_PER_NODE`, `STEPS`,
`NUMPARTICLES` as env vars before running (see `common.sh` for defaults).

Each node count writes `vpic_bdcats_<mode>_<nodes>.csv` to `$RESULTS_DIR`,
tagged with a trailing `# n_nodes=...,servers_per_node=...,...` comment
line recording that run's shape.

**Not validated against a real Flux instance** -- written against the
documented `flux batch`/`flux run`/`flux job attach` CLI
(https://flux-framework.readthedocs.io) since none was available in the
environment this was written in. The benchmark binary and CSV pipeline
themselves (`vpic_bdcats`, `pdc_call_stats.h`) *are* fully tested locally
against a real `pdc_server` over plain MPI -- only the Flux submission
layer is unverified. Check `flux run --help` / `flux batch --help` on
Tuolumne itself for any flag differences (in particular
`--tasks-per-node` and `--env`) before relying on this at scale --
`run_small_scale.sh` is the fastest way to find out.

# analysis_scripts

Slurm jobs and supporting bash scripts for the magnitude-analysis scale
study (eager DataFlyway vs. posthoc vs. plain parallel HDF5), modeled after
`pdc_helper_scripts/vpicio_scripts/` (background server srun step, foreground
client srun step, graceful `close_server` shutdown, per-node-count job
chaining via `vpicio_scale_run.sh`).

Both posthoc benchmarks (PDC and HDF5) are genuinely two-phase: a write
job runs to completion and every client rank fully exits, then a separate
job relaunches to read the data back, compute magnitude, and write it
back. For PDC this also closes and restarts `pdc_server` in between (with
the `restart` argument, so it reloads its metadata checkpoint) rather than
leaving one server process running throughout. That relaunch is real
posthoc-workflow cost -- someone coming back later, in a new job, to
derive and persist a product from data someone else already wrote -- and
is why posthoc is expected to come out slower than eager, not something
the benchmark tries to hide by keeping one continuous session open the
way eager and lazy do.

## Layout

| File | Role |
|---|---|
| `srun_server.sh` | Starts a fresh `pdc_server` (no prior data) in the background for one node-count step |
| `srun_server_restart.sh` | Restarts `pdc_server` with the `restart` argument, reloading the metadata checkpoint written on close |
| `srun_close_server.sh` | Gracefully shuts the server down via `close_server` (checkpoints metadata first) |
| `srun_client_dataflyway.sh` | Runs `bench_magnitude eager`, appends a CSV row |
| `srun_client_posthoc_write.sh` | Posthoc phase 1: runs `bench_write_components` (writes vx/vy/vz, exits) |
| `srun_client_posthoc_analyze.sh` | Posthoc phase 2: runs `bench_posthoc_analyze` (reads back, computes, writes back magnitude) |
| `srun_hdf5_write.sh` | HDF5 posthoc phase 1: runs `hdf5_bench_write` (no PDC server) |
| `srun_hdf5_posthoc_analyze.sh` | HDF5 posthoc phase 2: runs `hdf5_bench_posthoc_analyze` (no PDC server) |
| `dataflyway_analysis.sbatch` | Single-node-count job: PDC DataFlyway (eager) |
| `posthoc_analysis.sbatch` | Single-node-count job: PDC post-hoc (write -> close/restart server -> analyze) |
| `hdf5_analysis.sbatch` | Single-node-count job: plain parallel HDF5 baseline (write -> analyze) |
| `dataflyway_analysis_run.sh` | Submits chained `dataflyway_analysis.sbatch` jobs, one per node count |
| `posthoc_analysis_run.sh` | Submits chained `posthoc_analysis.sbatch` jobs, one per node count |
| `hdf5_analysis_run.sh` | Submits chained `hdf5_analysis.sbatch` jobs, one per node count |

Each `.sbatch` job runs **one** node count with 8 data servers/node and 32
client ranks/node (see "Server count vs. client count" below), and writes
one CSV row to `results_<mode>_<jobid>.csv` in this directory. Each
`_run.sh` wrapper submits its `.sbatch` once per node count (see the
`for nodes in ...` line at the top of each `_run.sh`), chained with
`--dependency=afterok` so they run one after another rather than all
competing for the account's allocation at once.

### Result CSV schema

`dataflyway_analysis.sbatch` (eager) writes the schema `bench_magnitude.c`
prints directly:
`mode,n_ranks,n_elem,setup_s,write_s,readback_s,compute_s,writeback_s,confirm_read_s,total_s,bad`
(eager leaves `readback_s`/`compute_s`/`writeback_s` at 0 -- that cost is
folded into `write_s`, since eager materializes magnitude during the
write itself).

`posthoc_analysis.sbatch` and `hdf5_analysis.sbatch` combine their two
phases' own CSV lines into one row with a different schema that makes the
relaunch cost visible instead of burying it:
`mode,n_ranks,n_elem,write_setup_s,write_s,relaunch_s,analyze_setup_s,readback_s,compute_s,writeback_s,total_s,bad`.
`relaunch_s` is the wall-clock time between the write phase's client
srun step returning and the analyze phase's client srun step starting --
for PDC that spans `srun_close_server.sh` + `srun_server_restart.sh`; for
HDF5 there's no server to restart, so it's always `0`. `total_s` sums
every phase's cost including `relaunch_s`.

## Usage on Perlmutter

```
git clone <repo> && cd pdc
# build PDC (produces build/bin/pdc_server, bench_magnitude,
# bench_write_components, bench_posthoc_analyze, close_server)
...
# build the HDF5 baseline (produces hdf5_bench_write, hdf5_bench_posthoc_analyze)
module load cray-hdf5-parallel
cd hdf5_analysis_test && make && cd ..

cd pdc_helper_scripts/analysis_scripts
./dataflyway_analysis_run.sh   # submits chained jobs, one per node count
./posthoc_analysis_run.sh      # submits chained jobs, one per node count
./hdf5_analysis_run.sh         # submits chained jobs, one per node count
```

Each job defaults to `--account=m2621`; edit the `#SBATCH` header, or export
`SBATCH_ACCOUNT=<yours>`, if that allocation isn't yours. `BIN_DIR` and
`N_ELEM` are overridable via the environment at submit time, e.g.:

```
N_ELEM=33554432 ./dataflyway_analysis_run.sh
```

To run a single node count directly instead of the full sweep:

```
N_ELEM=33554432 sbatch --nodes=4 dataflyway_analysis.sbatch
```

The default `N_ELEM=16777216` (64 MiB/rank of float32) is sized so that at
128 nodes * 32 ranks/node (4096 ranks) -- the largest scale point these
benchmarks are meant to reach -- total data written stays under 2 TB
(~1.25 TiB aggregate: 192 MiB/rank input + 128 MiB/rank magnitude output).
Scale `N_ELEM` up with care; it applies per rank, so aggregate write grows
with both `N_ELEM` and node count.

## Server count vs. client count

The magnitude benchmarks (`src/tests/analysis/bench_magnitude.c`,
`bench_write_components.c`, `bench_posthoc_analyze.c`) all use
`PDC_REGION_STATIC`, which splits each object's region across however many
data servers are running (`static_region_partition`), independent of how
many client ranks exist. Server count and client count don't need to
match for correctness -- 8 data servers/node and 32 client ranks/node is
just the deployment shape used here, same asymmetry as
`vpicio_scripts` (`DATA_SERVERS_PER_NODE=1`, `CLIENTS_PER_NODE=32`).

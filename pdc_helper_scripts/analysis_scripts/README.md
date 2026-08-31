# analysis_scripts

Slurm jobs and supporting bash scripts for the magnitude-analysis scale
study (eager DataFlyway vs. posthoc vs. plain parallel HDF5), modeled after
`pdc_helper_scripts/vpicio_scripts/` (background server srun step, foreground
client srun step, graceful `close_server` shutdown, per-node-count job
chaining via `vpicio_scale_run.sh`).

## Layout

| File | Role |
|---|---|
| `srun_server.sh` | Starts `pdc_server` in the background for one node-count step |
| `srun_close_server.sh` | Gracefully shuts the server down via `close_server` |
| `srun_client_dataflyway.sh` | Runs `bench_magnitude eager`, appends a CSV row |
| `srun_client_posthoc.sh` | Runs `bench_magnitude posthoc`, appends a CSV row |
| `srun_hdf5.sh` | Runs `hdf5_bench_magnitude` (no PDC server), appends a CSV row |
| `dataflyway_analysis.sbatch` | Single-node-count job: PDC DataFlyway (eager) |
| `posthoc_analysis.sbatch` | Single-node-count job: PDC post-hoc |
| `hdf5_analysis.sbatch` | Single-node-count job: plain parallel HDF5 baseline |
| `dataflyway_analysis_run.sh` | Submits 4 chained `dataflyway_analysis.sbatch` jobs, one per node count |
| `posthoc_analysis_run.sh` | Submits 4 chained `posthoc_analysis.sbatch` jobs, one per node count |
| `hdf5_analysis_run.sh` | Submits 4 chained `hdf5_analysis.sbatch` jobs, one per node count |

Each `.sbatch` job runs **one** node count with 8 data servers/node and 32
client ranks/node (see "Server count vs. client count" below), and writes
one CSV row to `results_<mode>_<jobid>.csv` in this directory. The
`_run.sh` wrapper for each mode submits 4 such jobs, one per node count in
`1 2 4 8`, chained with `--dependency=afterok` so they run one after
another rather than all competing for the account's allocation at once.

## Usage on Perlmutter

```
git clone <repo> && cd pdc
# build PDC (produces build/bin/pdc_server, bench_magnitude, close_server)
...
# build the HDF5 baseline
module load cray-hdf5-parallel
cd hdf5_analysis_test && make && cd ..

cd pdc_helper_scripts/analysis_scripts
./dataflyway_analysis_run.sh   # submits 4 jobs: 1, 2, 4, 8 nodes
./posthoc_analysis_run.sh      # submits 4 jobs: 1, 2, 4, 8 nodes
./hdf5_analysis_run.sh         # submits 4 jobs: 1, 2, 4, 8 nodes
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

The magnitude benchmark (`src/tests/analysis/bench_magnitude.c`) uses
`PDC_REGION_STATIC`, which splits each object's region across however many
data servers are running (`static_region_partition`), independent of how
many client ranks exist. Server count and client count don't need to
match for correctness -- 8 data servers/node and 32 client ranks/node is
just the deployment shape used here, same asymmetry as
`vpicio_scripts` (`DATA_SERVERS_PER_NODE=1`, `CLIENTS_PER_NODE=32`).

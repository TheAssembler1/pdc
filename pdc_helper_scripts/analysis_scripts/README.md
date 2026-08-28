# analysis_scripts

Slurm jobs and supporting bash scripts for the magnitude-analysis scale
study (eager DataFlyway vs. posthoc vs. plain parallel HDF5), modeled after
`pdc_helper_scripts/vpicio_scripts/` (background server srun step, foreground
client srun step, graceful `close_server` shutdown).

## Layout

| File | Role |
|---|---|
| `srun_server.sh` | Starts `pdc_server` in the background for one sweep step |
| `srun_close_server.sh` | Gracefully shuts the server down via `close_server` |
| `srun_client_dataflyway.sh` | Runs `bench_magnitude eager`, appends a CSV row |
| `srun_client_posthoc.sh` | Runs `bench_magnitude posthoc`, appends a CSV row |
| `srun_hdf5.sh` | Runs `hdf5_bench_magnitude` (no PDC server), appends a CSV row |
| `dataflyway_analysis.sbatch` | Sweep job: PDC DataFlyway (eager) |
| `posthoc_analysis.sbatch` | Sweep job: PDC post-hoc |
| `hdf5_analysis.sbatch` | Sweep job: plain parallel HDF5 baseline |

Each `.sbatch` job sweeps client/server rank counts 1, 2, 4, 8 (override with
`RANK_LIST`) inside a single allocation, and writes one CSV row per step to
`results_<mode>_<jobid>.csv` in this directory.

## Usage on Perlmutter

```
git clone <repo> && cd pdc
# build PDC (produces build/bin/pdc_server, bench_magnitude, close_server)
...
# build the HDF5 baseline
module load cray-hdf5-parallel
cd hdf5_analysis_test && make && cd ..

cd pdc_helper_scripts/analysis_scripts
sbatch dataflyway_analysis.sbatch
sbatch posthoc_analysis.sbatch
sbatch hdf5_analysis.sbatch
```

Each job defaults to `--account=m2621`; edit the `#SBATCH` header or pass
`--account=<yours>` on the `sbatch` command line if that allocation isn't
yours. `BIN_DIR`, `N_ELEM`, and `RANK_LIST` are all overridable via the
environment at submit time, e.g.:

```
RANK_LIST="1 2 4 8 16 32" N_ELEM=536870912 sbatch dataflyway_analysis.sbatch
```

## Why server count == client count

The magnitude benchmark (`src/tests/analysis/bench_magnitude.c`) uses
`PDC_REGION_STATIC` so each rank's disjoint region routes to the
matching server automatically -- this only lines up 1:1 when the client
and server rank counts match, which is why every sweep step here launches
identically-sized server and client srun steps.

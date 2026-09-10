# curl_analysis_scripts

Slurm jobs and supporting bash scripts for an E3SM-shaped curl +
vorticity-magnitude analysis benchmark, comparing PDC eager
(DataFlyway) against a genuinely two-restart posthoc workflow -- modeled
after the tropical-cyclone-track use case in the paper this compares
against (curl of wind velocity / magnitude of curl, used to detect
atmospheric rotation in E3SM output on Frontier).

## The pipeline

`an_client/graphs/curl_vorticity_magnitude.json` chains two builtin,
server-side analysis functions:

```
u, v, w  --curl-->  curl_x, curl_y, curl_z  --vector_magnitude-->  vorticity_magnitude
```

Every state is **persistent** (not just the endpoints) -- the source
paper's "Store" strategy treats curl itself as a real, queryable, stored
quantity (the extra ~4GB it reports going from 24GB primary data to
28GB total), not a throwaway intermediate. `curl` and `vector_magnitude`
are both compiled directly into the server as builtins (see
`src/server/analysis/pdc_an_builtin_curl_cpu.c` and
`pdc_an_builtin_magnitude_cpu.c`), registered in
`PDCan_init_builtin_funcs` -- no dlopen, no external library, since both
are general enough to ship as part of PDC itself.

`curl`'s math lives in `src/tests/analysis/curl_math.h`, shared between
the server-side builtin and the posthoc client compute binary so both
modes compute the identical result: central differences in the interior
of each rank's local (nx, ny, nz) block, one-sided at the block's own
edges (a stand-in for a halo exchange with neighboring ranks, not a real
one), unit grid spacing. Like `vector_magnitude`'s plain
`sqrt(x^2+y^2+z^2)`, this is a benchmark stand-in for curl's cost and
data shape, not a scientifically exact discretization of E3SM's actual
grid.

### Domain decomposition

Each rank owns a local `NX x NY x NZ_PER_RANK` block of `u`, `v`, `w`
(float32) and, correspondingly, of `curl_x/y/z` and
`vorticity_magnitude` (float64). The global object spans
`[NX, NY, nranks*NZ_PER_RANK]`, split along the z axis (vertical
levels): rank `r` owns `global_offset = [0, 0, r*NZ_PER_RANK]`. This is
the same `PDC_REGION_STATIC` region-offset routing pattern as
`analysis_scripts/`' magnitude benchmark, just 3D instead of 1D.

### Eager vs. posthoc

- **Eager** (`bench_curl_eager`, `curl_eager_analysis.sbatch`): one
  continuous client session. Writing `u`, `v`, `w` with the graph
  attached transparently triggers server-side `curl` then
  `vector_magnitude` in the write path; a confirmation read of
  `vorticity_magnitude` follows.
- **Posthoc** (`bench_curl_write` / `bench_curl_compute` /
  `bench_curl_analyze`, `curl_posthoc_analysis.sbatch`): three
  genuinely separate client processes, exactly matching the real
  posthoc workflow this is supposed to model -- read data, compute
  curl, write curl out; then read curl back, compute magnitude, write
  magnitude out. The PDC server is fully closed (which checkpoints its
  metadata) and restarted (`./pdc_server restart`, which reloads that
  checkpoint) **between every phase**, so each later phase is really
  talking to a server that had to reload persisted data, not one that
  kept it in memory the whole time. See
  `analysis_scripts/README.md`'s "Result CSV schema" section for why
  this relaunch cost matters -- same reasoning here, just with two
  relaunches instead of one.

### Compression (optional)

Set `COMPRESS=1` (default `0`) to additionally compose the existing,
already-GPU-backed ZFP compression transform
(`tf_client/graphs/zfp_gpu.json`, `zfp_compress`/`zfp_decompress`
builtins) onto `vorticity_magnitude` before it's written -- via
`PDCtf_attach_to_region(tf_dg_id, mag_obj, reg_global, "decompressed",
"compressed")` called *before* the analysis graph (eager) or the plain
write (posthoc's final phase). `PDCan_attach_to_region`'s
`find_attached_tf_info` (see `src/api/pdc_an/pdc_an.c`) automatically
picks up and piggybacks that composition, so this reuses PDC's existing
transformation framework end to end -- no new compression code. This is
exactly the poster's "an analysis output can itself be compressed or
transformed" claim, exercised for real.

## E3SM data-shape derivation

The source paper doesn't state a grid resolution, so there's no single
authoritative shape to copy. What it does give is internally consistent
with this benchmark's element-wise convention (float32 in/out, float64
for magnitude): `curl_x/y/z` mirror `u/v/w`'s shape exactly (the compute
is element-wise per rank, not resolution-reducing), so if curl totals
~4GB across 3 components, magnitude (1 field, double-precision) comes
out to `(4GB/3) * 2 ≈ 2.67GB` -- matching the paper's reported "~3GB"
closely. So `NX=256, NY=256, NZ_PER_RANK=64` is sized to total ~4GB of
`u+v+w` at a **96-rank reference point** (matching the paper's primary
analysis rank count): `256*256*64 = 4,194,304` elements/rank/variable,
`*4 bytes = 16MiB/rank/variable`, `*3 variables *96 ranks ≈ 4.6GB`. The
other 6 of the paper's "9 primary variables" (~20GB) aren't part of
this curl/magnitude chain and aren't synthesized here.

This benchmark sweeps node/rank count with these dimensions held fixed
per rank (weak scaling), consistent with `analysis_scripts/`'s
convention, rather than replicating the paper's own strong-scaling
study (fixed total problem size, 96 up to 900 processes) -- the 96-rank
sizing above is a calibration reference point, not a hard scale limit.
Override `NX`/`NY`/`NZ_PER_RANK` at submit time the same way `N_ELEM` is
overridden in `analysis_scripts/`, e.g.:

```
NZ_PER_RANK=128 sbatch --nodes=4 curl_eager_analysis.sbatch
```

## Layout

| File | Role |
|---|---|
| `srun_server.sh` | Starts a fresh `pdc_server` (no prior data) in the background |
| `srun_server_restart.sh` | Restarts `pdc_server` with `restart`, reloading the metadata checkpoint |
| `srun_close_server.sh` | Gracefully shuts the server down via `close_server` (checkpoints first) |
| `srun_client_curl_eager.sh` | Runs `bench_curl_eager`, appends a CSV row |
| `srun_client_curl_write.sh` | Posthoc phase 1: runs `bench_curl_write` |
| `srun_client_curl_compute.sh` | Posthoc phase 2: runs `bench_curl_compute` |
| `srun_client_curl_analyze.sh` | Posthoc phase 3: runs `bench_curl_analyze` |
| `curl_eager_analysis.sbatch` | Single-node-count job: eager curl+magnitude |
| `curl_posthoc_analysis.sbatch` | Single-node-count job: 3-phase posthoc curl+magnitude |
| `curl_eager_analysis_run.sh` | Submits chained `curl_eager_analysis.sbatch` jobs, one per node count |
| `curl_posthoc_analysis_run.sh` | Submits chained `curl_posthoc_analysis.sbatch` jobs, one per node count |

## Result CSV schemas

Eager (`results_curl_eager_<jobid>.csv`), straight from
`bench_curl_eager`'s own output:
```
mode,n_ranks,nx,ny,nz_per_rank,compress,setup_s,write_s,confirm_read_s,total_s,bad
```

Posthoc (`results_curl_posthoc_<jobid>.csv`), combining all three
phases' CSV lines plus both measured relaunch costs into one row:
```
mode,n_ranks,nx,ny,nz_per_rank,compress,write_setup_s,write_s,relaunch1_s,compute_setup_s,readback1_s,curl_compute_s,writeback1_s,relaunch2_s,analyze_setup_s,readback2_s,magnitude_compute_s,writeback2_s,total_s,bad
```
`relaunch1_s`/`relaunch2_s` are wall-clock time across each
close+restart cycle (`srun_close_server.sh` + `srun_server_restart.sh`).
`total_s` sums every phase's cost including both relaunches.

## Usage on Perlmutter

```
cd pdc_helper_scripts/curl_analysis_scripts
./curl_eager_analysis_run.sh     # submits chained jobs, one per node count
./curl_posthoc_analysis_run.sh   # same, for the 3-phase posthoc variant

COMPRESS=1 ./curl_eager_analysis_run.sh    # GPU-compressed vorticity_magnitude variant
COMPRESS=1 ./curl_posthoc_analysis_run.sh
```

Each job defaults to `--account=m2621`; edit the `#SBATCH` header, or
export `SBATCH_ACCOUNT=<yours>`, if that allocation isn't yours.

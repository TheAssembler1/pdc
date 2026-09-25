# adios2_analysis_test

ADIOS2 analogs of four Data Flyway benchmarks -- two analysis
(`bench_magnitude.c`, `bench_curl_eager.c`) and two transformation
(`compression_transform.c`, `compression_encryption_transform.c`) --
built to be as similar to Data Flyway as ADIOS2's own feature set
genuinely allows. See this project's own `table.tex` for the ADIOS2
comparisons these stand in for ("ADIOS2 derived variables" for the
analysis pair, ADIOS2 Operators for the transformation pair), and each
`.cpp` file's header comment for exactly where it matches and where it
necessarily diverges.

## Analysis (ADIOS2 Derived Variables)

- `adios2_bench_magnitude` -- vx/vy/vz in, `MAGNITUDE(vx,vy,vz)` out via
  a genuine ADIOS2 derived variable, computed write-triggered
  (`EndStep()`), matching Data Flyway's eager `vector_magnitude`.
  Works correctly (`bad=0`).
- `adios2_bench_curl` -- u/v/w in, vorticity_magnitude out. ADIOS2 does
  have a native `CURL` expression operator (a real central-difference
  stencil, same numerical scheme as `curl_math.h`), and
  `MAGNITUDE(CURL(u,v,w))` is a valid nested expression that *should*
  reproduce Data Flyway's two-stage curl_vorticity_magnitude graph in
  one derived-variable declaration -- but it doesn't work in the
  ADIOS2 build this was developed against (v2.12.1): the written
  `CURL` output is silently all-zero (confirmed via `bpls` and a
  minimal, hand-verifiable single-rank reproduction), while the
  shape-preserving `MAGNITUDE` operator works fine. This looks like a
  real upstream bug specific to derived variables whose output shape
  differs from their inputs' (`CURL` appends a trailing
  size-3 dimension; `MAGNITUDE` doesn't), not a usage mistake or a
  limitation of derived variables generally. `adios2_bench_curl.cpp`'s
  header comment has the full writeup. Given that, this workload falls
  back to client-side compute (read u/v/w back, curl_math.h, write
  vorticity_magnitude back) -- Data Flyway's own *posthoc* strategy,
  still a meaningful, valid comparison point, just not the eager one
  originally intended.

## Transformation (ADIOS2 Operators)

- `adios2_bench_compression` -- ZFP fixed-rate compression
  (`Variable::AddOperation("zfp", {{"rate","16"}})`), transparent on
  write and read, matching this project's own ZFP configuration
  (fixed-rate, 16 bits/value) and Data Flyway's TF attach-once model.
  Works correctly (`bad=0`, checked at the same lossy-round-trip
  tolerance `vector_magnitude_compressed_test.c` uses).
- `adios2_bench_compression_encryption` -- ZFP chained with encryption
  via two `AddOperation` calls. ADIOS2 has no built-in encryption
  operator, but ships a complete, ready-to-use plugin
  (`plugins/operators/EncryptionOperator.cpp`, built whenever libsodium
  is found) using `crypto_secretbox_easy` -- XSalsa20-Poly1305, the
  same algorithm Data Flyway's own encryption transform uses. No custom
  code was written for this; it's ADIOS2's own reference plugin. Real
  finding along the way: BP5 (ADIOS2's current default file engine)
  flatly refuses more than one operator per variable ("BP5 does not
  support multiple operators"); this workload only works by explicitly
  setting the older BP4 engine (`io.SetEngine("BP4")`), which has no
  such restriction -- itself concrete evidence for table.tex's
  "Composable transforms: None" rating for ADIOS2, not a workaround
  that erases the finding. Works correctly (`bad=0`) under BP4.

## Build

`./build_adios2.sh` does the whole thing: clones ADIOS2 v2.12.1 (into
`.adios2_src/`, skipped if already present), configures it with exactly
the feature set these workloads need (MPI, Derived Variables, ZFP,
Sodium; SST/UCX explicitly off -- they caused an unrelated hang during
development), builds, installs, then builds all four
`adios2_bench_*` programs here via `make`. Needs zfp and libsodium
already built and installed somewhere findable by CMake --
`ZFP_PREFIX`/`SODIUM_PREFIX` env vars if they're not at this script's
defaults (`/mnt/fast/nlewis/workspace/install/{zfp,libsodium}`).
`ADIOS2_PREFIX` controls the install location (default
`/mnt/fast/nlewis/workspace/install/adios2`). Tested end-to-end
(fresh clone through all four binaries built) before being committed.

```
./build_adios2.sh
# or, to install somewhere else:
ADIOS2_PREFIX=$HOME/adios2-install ./build_adios2.sh
```

To rebuild just the four programs against an ADIOS2 already installed
elsewhere, skip the script and use the Makefile directly:

```
ADIOS2_PREFIX=/path/to/adios2/install make
```

## Run

```
./adios2_bench_magnitude <n_elem_per_rank> [out_file]
./adios2_bench_curl <nx> <ny> <nz_per_rank> [out_file]
./adios2_bench_compression <n_elem_per_rank> [out_file]
./adios2_bench_compression_encryption <n_elem_per_rank> [out_file]
```

Each prints one CSV line per timestep (N_TIMESTEPS=3) from rank 0:

```
adios2_derived,step,n_client_ranks,n_elem,setup_s,write_s,confirm_read_s,step_total_s,bad
adios2_posthoc_curl,step,n_client_ranks,nx,ny,nz_per_rank,setup_s,write_s,readback_s,compute_s,writeback_s,confirm_read_s,step_total_s,bad
adios2_zfp,step,n_client_ranks,n_elem,setup_s,write_s,confirm_read_s,step_total_s,bad
adios2_zfp_encrypt,step,n_client_ranks,n_elem,setup_s,write_s,confirm_read_s,step_total_s,bad
```

## Slurm (Perlmutter), 1 to 128 nodes

Four `*_analysis.sbatch` + `*_analysis_run.sh` pairs, one per program,
mirroring the real PDC sweep scripts' node range (1, 2, 4, 8, 16, 32,
64, 128 -- `pdc_helper_scripts/vpicio_scripts/vpicio_scale_run.sh`'s
range, matching `evaluation.tex`'s own stated "Nodes: 1-128") and
per-rank data size exactly:

| sbatch | mirrors | per-rank size |
|---|---|---|
| `magnitude_analysis.sbatch` | `analysis_scripts/eager_pdc.sbatch` | N_ELEM=16,777,216 (64 MiB float32) -- identical default |
| `curl_analysis.sbatch` | `curl_analysis_scripts/curl_eager_analysis.sbatch` | NX=256, NY=256, NZ_PER_RANK=64 -- identical default |
| `compression_analysis.sbatch` | `vpicio_scripts/` (VPIC-IO + ZFP) | N_ELEM=67,108,864 (256 MiB float32) == VPIC-IO's real 8,388,608 particles/rank x 32 bytes/particle (7 float + 1 int fields, `src/tests/misc/vpicio.c`) -- same byte count per rank per step, see the sbatch's own comment for the derivation |
| `compression_encryption_analysis.sbatch` | same VPIC-IO ZFP+libsodium config | same as above |

All four use 32 client ranks/node (matching every PDC script's
`CLIENTS_PER_NODE`), the same `account=m2621`/`constraint=cpu` Slurm
config, and the same Lustre striping convention as `posthoc_hdf5.sbatch`
(stripe size == one rank's per-write payload, striped across all 128
OSTs). None of these launch a server -- ADIOS2 has none; ranks write
straight to Lustre-backed `.bp` files.

```
./magnitude_analysis_run.sh
./curl_analysis_run.sh
./compression_analysis_run.sh
./compression_encryption_analysis_run.sh
```

or submit a single node count directly, e.g. `sbatch --nodes=4
magnitude_analysis.sbatch`. These are untested against real Slurm/Lustre
from this environment (no Slurm here) -- syntax-checked
(`bash -n`) and built directly off the already-verified-working
binaries and the already-running PDC sbatch scripts' own structure, but
worth a real dry run at a small node count before trusting the full
sweep.

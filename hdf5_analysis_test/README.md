# hdf5_analysis_test

Parallel-HDF5 baseline for the magnitude analysis benchmark, standing in for
the "posthoc" strategy in `src/tests/analysis/bench_magnitude.c` (there is no
in-flight transform framework in plain HDF5, so posthoc -- write, read back,
compute client-side, write the result back -- is the only strategy that
applies). Same problem size and per-rank hyperslab decomposition as the PDC
benchmark, so results are directly comparable.

## Build

Needs a parallel-HDF5-enabled MPI environment (`h5pcc` on `$PATH`, or MPI +
`libhdf5` reachable via `$HDF5_CFLAGS`/`$HDF5_LDFLAGS`):

```
module load cray-hdf5-parallel   # Perlmutter
make
```

## Run

```
./hdf5_bench_magnitude <n_elem_per_rank> [out_file]
```

Prints one CSV line from rank 0:

```
mode,n_client_ranks,n_elem,setup_s,write_s,readback_s,compute_s,writeback_s,total_s,bad
```

See `pdc_helper_scripts/analysis_scripts/` for the Slurm job that sweeps rank
counts and drives this at scale on Perlmutter.

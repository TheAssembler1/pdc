# Standalone build for the HDF5 posthoc benchmark pairs -- no dependency
# on the rest of the PDC build tree, so `git clone` + `make` is enough on
# Perlmutter after `module load cray-hdf5-parallel` (or any parallel-HDF5-
# enabled environment providing h5pcc), except for curl_math.h, shared
# with the PDC-side curl benchmarks in ../src/tests/analysis/ so both
# sides compute the identical curl.
#
# Two three-binary posthoc chains, each run as separate job steps (see
# ../pdc_helper_scripts/analysis_scripts/hdf5_analysis.sbatch and
# ../pdc_helper_scripts/curl_analysis_scripts/curl_hdf5_analysis.sbatch),
# so posthoc's real-world relaunch cost is measured instead of hidden
# inside one continuous process:
#   hdf5_bench_write            - write vx/vy/vz, exit
#   hdf5_bench_posthoc_analyze  - reopen the file, read back, compute
#                                  magnitude, write it back
#   hdf5_bench_curl_write       - write u/v/w, exit
#   hdf5_bench_curl_compute     - reopen the file, read u/v/w back,
#                                  compute curl, write curl_x/y/z
#   hdf5_bench_curl_analyze     - reopen the file, read curl_x/y/z back,
#                                  compute magnitude, write it back

H5PCC := $(shell command -v h5pcc 2>/dev/null)
CC    ?= mpicc
CFLAGS ?= -O2 -Wall
CURL_MATH_INCLUDE := -I../src/tests/analysis

TARGETS := hdf5_bench_write hdf5_bench_posthoc_analyze \
           hdf5_bench_curl_write hdf5_bench_curl_compute hdf5_bench_curl_analyze

.PHONY: all clean

all: $(TARGETS)

ifdef H5PCC
hdf5_bench_write: hdf5_bench_write.c
	h5pcc $(CFLAGS) -o $@ $< -lm
hdf5_bench_posthoc_analyze: hdf5_bench_posthoc_analyze.c
	h5pcc $(CFLAGS) -o $@ $< -lm
hdf5_bench_curl_write: hdf5_bench_curl_write.c
	h5pcc $(CFLAGS) -o $@ $< -lm
hdf5_bench_curl_compute: hdf5_bench_curl_compute.c
	h5pcc $(CFLAGS) $(CURL_MATH_INCLUDE) -o $@ $< -lm
hdf5_bench_curl_analyze: hdf5_bench_curl_analyze.c
	h5pcc $(CFLAGS) $(CURL_MATH_INCLUDE) -o $@ $< -lm
else
hdf5_bench_write: hdf5_bench_write.c
	$(CC) $(CFLAGS) -o $@ $< $(HDF5_CFLAGS) -lhdf5 -lm $(HDF5_LDFLAGS)
hdf5_bench_posthoc_analyze: hdf5_bench_posthoc_analyze.c
	$(CC) $(CFLAGS) -o $@ $< $(HDF5_CFLAGS) -lhdf5 -lm $(HDF5_LDFLAGS)
hdf5_bench_curl_write: hdf5_bench_curl_write.c
	$(CC) $(CFLAGS) -o $@ $< $(HDF5_CFLAGS) -lhdf5 -lm $(HDF5_LDFLAGS)
hdf5_bench_curl_compute: hdf5_bench_curl_compute.c
	$(CC) $(CFLAGS) $(CURL_MATH_INCLUDE) -o $@ $< $(HDF5_CFLAGS) -lhdf5 -lm $(HDF5_LDFLAGS)
hdf5_bench_curl_analyze: hdf5_bench_curl_analyze.c
	$(CC) $(CFLAGS) $(CURL_MATH_INCLUDE) -o $@ $< $(HDF5_CFLAGS) -lhdf5 -lm $(HDF5_LDFLAGS)
endif

clean:
	rm -f $(TARGETS) *.h5

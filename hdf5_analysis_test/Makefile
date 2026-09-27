# Standalone build for the HDF5 posthoc benchmark pairs -- no dependency
# on the rest of the PDC build tree, so `git clone` + `make` is enough on
# Perlmutter after `module load cray-hdf5-parallel` (or any parallel-HDF5-
# enabled environment providing h5pcc), except for curl_math.h, shared
# with the PDC-side curl benchmarks in ../src/tests/analysis/ so both
# sides compute the identical curl.
#
# Two two-binary posthoc chains, each run as separate job steps (see
# ../pdc_helper_scripts/analysis_scripts/hdf5_analysis.sbatch and
# ../pdc_helper_scripts/curl_analysis_scripts/curl_hdf5_analysis.sbatch),
# so posthoc's real-world relaunch cost is measured instead of hidden
# inside one continuous process:
#   hdf5_bench_write            - write vx/vy/vz, exit
#   hdf5_bench_posthoc_analyze  - reopen the file, read back, compute
#                                  magnitude, write it back
#   hdf5_bench_curl_write       - write u/v/w, exit
#   hdf5_bench_curl_analyze     - reopen the file, read u/v/w back,
#                                  compute curl then magnitude, write
#                                  curl_x/y/z and vorticity_magnitude back
#
# magnitude_highfive is a separate, single-session (not posthoc-split)
# C++ benchmark using the HighFive header-only wrapper over this same
# libhdf5 -- see that file's own header comment for why it exists (answer
# "does the wrapper cost anything over the raw C API", not a new storage
# engine). Needs a C++14 compiler (mpicxx) and HighFive's headers, cloned
# on demand into .highfive_src/ (git, header-only, no build step of its
# own) since it isn't a system package here.

H5PCC := $(shell command -v h5pcc 2>/dev/null)
CC    ?= mpicc
# CXX is one of Make's built-in variables (defaults to g++), so a plain
# ?= never takes effect -- only override it while it's still at that
# built-in default, so an explicit `make CXX=...` still wins.
ifeq ($(origin CXX),default)
CXX := mpicxx
endif
CFLAGS ?= -O2 -Wall
CXXFLAGS ?= -O2 -Wall -std=c++14
CURL_MATH_INCLUDE := -I../src/tests/analysis

HIGHFIVE_SRC := .highfive_src
HIGHFIVE_INCLUDE := -I$(HIGHFIVE_SRC)/include
# h5pcc wraps a C compiler only, so magnitude_highfive.cpp needs the raw
# HDF5 include/lib paths directly -- overridable, e.g. on Perlmutter after
# `module load cray-hdf5-parallel` these usually aren't needed at all
# (the compiler wrappers already have them).
HDF5_CXX_INCLUDE ?= $(shell h5pcc -show 2>/dev/null | grep -oE -- '-I[^ ]+' | tr '\n' ' ')
HDF5_CXX_LIBDIR  ?= $(shell h5pcc -show 2>/dev/null | grep -oE -- '-L[^ ]+' | tr '\n' ' ')

TARGETS := hdf5_bench_write hdf5_bench_posthoc_analyze \
           hdf5_bench_curl_write hdf5_bench_curl_analyze \
           magnitude_highfive

.PHONY: all clean highfive-fetch

all: $(TARGETS)

ifdef H5PCC
hdf5_bench_write: hdf5_bench_write.c
	h5pcc $(CFLAGS) -o $@ $< -lm
hdf5_bench_posthoc_analyze: hdf5_bench_posthoc_analyze.c
	h5pcc $(CFLAGS) -o $@ $< -lm
hdf5_bench_curl_write: hdf5_bench_curl_write.c
	h5pcc $(CFLAGS) -o $@ $< -lm
hdf5_bench_curl_analyze: hdf5_bench_curl_analyze.c
	h5pcc $(CFLAGS) $(CURL_MATH_INCLUDE) -o $@ $< -lm
else
hdf5_bench_write: hdf5_bench_write.c
	$(CC) $(CFLAGS) -o $@ $< $(HDF5_CFLAGS) -lhdf5 -lm $(HDF5_LDFLAGS)
hdf5_bench_posthoc_analyze: hdf5_bench_posthoc_analyze.c
	$(CC) $(CFLAGS) -o $@ $< $(HDF5_CFLAGS) -lhdf5 -lm $(HDF5_LDFLAGS)
hdf5_bench_curl_write: hdf5_bench_curl_write.c
	$(CC) $(CFLAGS) -o $@ $< $(HDF5_CFLAGS) -lhdf5 -lm $(HDF5_LDFLAGS)
hdf5_bench_curl_analyze: hdf5_bench_curl_analyze.c
	$(CC) $(CFLAGS) $(CURL_MATH_INCLUDE) -o $@ $< $(HDF5_CFLAGS) -lhdf5 -lm $(HDF5_LDFLAGS)
endif

highfive-fetch:
	@if [ ! -d $(HIGHFIVE_SRC)/include/highfive ]; then \
		echo "== Cloning HighFive (header-only) into $(HIGHFIVE_SRC)..."; \
		git clone --quiet --depth 1 https://github.com/BlueBrain/HighFive.git $(HIGHFIVE_SRC); \
	fi

magnitude_highfive: magnitude_highfive.cpp | highfive-fetch
	$(CXX) $(CXXFLAGS) $(HIGHFIVE_INCLUDE) $(HDF5_CXX_INCLUDE) $(HDF5_CXX_LIBDIR) -o $@ $< -lhdf5 -lm

clean:
	rm -f $(TARGETS) *.h5

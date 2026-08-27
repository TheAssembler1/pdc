# Standalone build for hdf5_bench_magnitude -- no dependency on the rest of
# the PDC build tree, so `git clone` + `make` is enough on Perlmutter after
# `module load cray-hdf5-parallel` (or any parallel-HDF5-enabled environment
# providing h5pcc).

H5PCC := $(shell command -v h5pcc 2>/dev/null)
CC    ?= mpicc
CFLAGS ?= -O2 -Wall

TARGET := hdf5_bench_magnitude
SRC    := hdf5_bench_magnitude.c

.PHONY: all clean

all: $(TARGET)

ifdef H5PCC
$(TARGET): $(SRC)
	h5pcc $(CFLAGS) -o $@ $< -lm
else
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(HDF5_CFLAGS) -lhdf5 -lm $(HDF5_LDFLAGS)
endif

clean:
	rm -f $(TARGET) *.h5

#include <stdio.h>
#include <stdlib.h>
#include <cuda_runtime.h>
#include <mpi.h>
#include "pi.h"

// ─── Config ───────────────────────────────────────────────────────────────────
#define DLEAP             2
#define PRECISION_INIT    2
#define PRECISION_HEX     200000

#define WARP_SIZE         32
#define WARPS_PER_BLOCK   8
#define THREADS_PER_BLOCK (WARPS_PER_BLOCK * WARP_SIZE)
#define BLOCKS_NUM        ((PRECISION_HEX / DLEAP / THREADS_PER_BLOCK) + 1)

#define SATURATE_SECONDS  40.0
#define BLOCKS_PER_SM     8        // enough to hide latency without starving BBP

// ─── MPI helper ───────────────────────────────────────────────────────────────
#define PRINT_IF_RANK0(...) \
    do { int _r; MPI_Comm_rank(MPI_COMM_WORLD,&_r); if(!_r) printf(__VA_ARGS__); } while(0)

// ─── CUDA error helper ────────────────────────────────────────────────────────
static void check(cudaError_t err) {
    if (err != cudaSuccess) {
        fprintf(stderr, "CUDA error: %s\n", cudaGetErrorString(err));
        exit(EXIT_FAILURE);
    }
}

// ─── BBP helpers ──────────────────────────────────────────────────────────────
__device__ long long biexp(long long a, long long b, long long mod) {
    long long ret = 1;
    while (b) {
        if (b & 1) ret = (ret * a) % mod;
        a = (a * a) % mod;
        b >>= 1;
    }
    return ret;
}

__device__ double F16dSj(long long d, int j) {
    double sum = 0.0;
    for (long long k = 0; k <= d; k++)
        sum += (double)biexp(16, d - k, 8*k+j) / (8*k+j);
    return sum - (int)sum;
}

// ─── Constant memory ──────────────────────────────────────────────────────────
__constant__ char hex_table[16];

static void hex_table_init(void) {
    char h[16] = {'0','1','2','3','4','5','6','7',
                  '8','9','A','B','C','D','E','F'};
    check(cudaMemcpyToSymbol(hex_table, h, 16));
}

// ─── BBP kernel ───────────────────────────────────────────────────────────────
__global__ void PiOnGPU_Kernel(
    long long precision_init,
    long long precision_hex,
    long long dLeap,
    unsigned char *result_dec,
    char         *result_hex)
{
    long long tid = blockIdx.x * blockDim.x + threadIdx.x;
    long long pc  = precision_init + tid * dLeap;
    if (pc >= precision_hex - 1) return;

    double x = 4*F16dSj(pc,1) - 2*F16dSj(pc,4)
                               -   F16dSj(pc,5)
                               -   F16dSj(pc,6);
    x = (x > 0) ? (x - (int)x) : (x - (int)x + 1);

    for (long long i = pc; i < pc + dLeap; i++) {
        x *= 16.0;
        result_dec[i] = (int)x;
        x -= result_dec[i];
        result_hex[i] = hex_table[result_dec[i]];
    }
}

// ─── Persistent spin / saturation kernel ──────────────────────────────────────
//
// Every thread spins on clock64() for exactly `target_cycles` GPU cycles.
// Because no thread ever exits early the SMs stay 100% occupied the whole
// time, leaving zero headroom for the zfp server to sneak in.
//
__global__ void saturate_kernel(long long target_cycles) {
    long long start = clock64();
    long long dummy = 0;
    while ((clock64() - start) < target_cycles) {
        // Dummy arithmetic prevents the loop being optimised away.
        // __threadfence() would also work but is heavier than we need.
        dummy ^= clock64();
    }
    // Write dummy to global mem so the compiler can't elide the loop.
    if (dummy == 0x1234567890ABCDEFLL) {
        // Never true in practice; just blocks dead-code elimination.
        asm volatile("" ::: "memory");
    }
}

// ─── Main timed wrapper ───────────────────────────────────────────────────────
void run_pi_gpu_timed(int rank) {
    check(cudaSetDevice(rank % 4));

    // ── Query device so we size the saturator correctly ──────────────────────
    cudaDeviceProp prop;
    check(cudaGetDeviceProperties(&prop, 0));

    int    sm_count      = prop.multiProcessorCount;
    long long clock_hz   = (long long)prop.clockRate * 1000LL; // clockRate is kHz
    long long target_cyc = (long long)(SATURATE_SECONDS * clock_hz);
    int    sat_blocks    = sm_count * BLOCKS_PER_SM;

    PRINT_IF_RANK0("SMs: %d\n", sm_count);
    PRINT_IF_RANK0("Clock: %lld Hz\n", clock_hz);
    PRINT_IF_RANK0("Target cycles: %lld  (~%.0f s)\n", target_cyc, SATURATE_SECONDS);
    PRINT_IF_RANK0("Saturation blocks: %d  (%d per SM)\n", sat_blocks, BLOCKS_PER_SM);
    PRINT_IF_RANK0("BBP blocks: %d\n\n", BLOCKS_NUM);

    // ── Allocate BBP buffers ──────────────────────────────────────────────────
    size_t dec_bytes = PRECISION_HEX * sizeof(unsigned char);
    size_t hex_bytes = (PRECISION_HEX + 1) * sizeof(char);

    unsigned char *d_dec; char *d_hex;
    check(cudaMalloc(&d_dec, dec_bytes));
    check(cudaMalloc(&d_hex, hex_bytes));

    unsigned char *h_dec = (unsigned char*)malloc(dec_bytes);
    char          *h_hex = (char*)         malloc(hex_bytes);
    if (!h_dec || !h_hex) { fputs("malloc failed\n", stderr); exit(1); }

    hex_table_init();

    // ── Create two independent, non-default streams ───────────────────────────
    //
    // Using the default (null) stream for either kernel would serialize them
    // with everything else in this context.  Two named streams let CUDA
    // schedule them concurrently while still being in the same process —
    // this matches what the separate zfp server process sees.
    //
    cudaStream_t stream_sat, stream_bbp;
    check(cudaStreamCreate(&stream_sat));
    check(cudaStreamCreate(&stream_bbp));

    // ── Timing ───────────────────────────────────────────────────────────────
    cudaEvent_t t0, t1, b0, b1;
    check(cudaEventCreate(&t0)); check(cudaEventCreate(&t1));
    check(cudaEventCreate(&b0)); check(cudaEventCreate(&b1));

    PRINT_IF_RANK0("--- Launching kernels ---\n");

    // Launch saturator first so it has SM presence before BBP arrives.
    check(cudaEventRecord(t0, stream_sat));
    saturate_kernel<<<sat_blocks, THREADS_PER_BLOCK, 0, stream_sat>>>(target_cyc);
    cudaError_t err = cudaGetLastError();
    check(cudaEventRecord(t1, stream_sat));

    // Launch BBP concurrently on the second stream.
    check(cudaEventRecord(b0, stream_bbp));
    PiOnGPU_Kernel<<<BLOCKS_NUM, THREADS_PER_BLOCK, 0, stream_bbp>>>(
        PRECISION_INIT, PRECISION_HEX, DLEAP, d_dec, d_hex);
    check(cudaEventRecord(b1, stream_bbp));

    // Wait for BBP to finish and copy results while saturator still runs.
    check(cudaStreamSynchronize(stream_bbp));
    float bbp_ms = 0;
    check(cudaEventElapsedTime(&bbp_ms, b0, b1));

    check(cudaMemcpy(h_hex, d_hex, hex_bytes, cudaMemcpyDeviceToHost));
    h_hex[PRECISION_HEX] = '\0';

    PRINT_IF_RANK0("BBP kernel finished. Saturation still running...\n");

    // Wait for saturator to finish.
    check(cudaStreamSynchronize(stream_sat));
    float sat_ms = 0;
    check(cudaEventElapsedTime(&sat_ms, t0, t1));

    // ── Results ───────────────────────────────────────────────────────────────
    PRINT_IF_RANK0("\n=== Results ===\n");
    PRINT_IF_RANK0("Pi hex digits (first 80): %.80s\n", h_hex);

    PRINT_IF_RANK0("\n=== Timing ===\n");
    PRINT_IF_RANK0("Saturator:   %.3f ms  (%.3f s)\n", sat_ms, sat_ms/1000.0f);
    PRINT_IF_RANK0("BBP kernel:  %.3f ms\n", bbp_ms);
    PRINT_IF_RANK0("Target was:  %.3f s\n", SATURATE_SECONDS);

    // ── Cleanup ───────────────────────────────────────────────────────────────
    cudaStreamDestroy(stream_sat);
    cudaStreamDestroy(stream_bbp);
    cudaEventDestroy(t0); cudaEventDestroy(t1);
    cudaEventDestroy(b0); cudaEventDestroy(b1);
    cudaFree(d_dec); cudaFree(d_hex);
    free(h_dec); free(h_hex);
}

/*
 * test.cu — libpi_gpu.so source
 *
 * Provides run_gemm_compute() for VPIC-IO and BDCats to simulate
 * realistic HPC compute overlapping with PDC async transformation.
 *
 * Each rank is pinned to GPU (rank % 4) and runs repeated SGEMM
 * with N=8192 for ~10 seconds to create sustained GPU contention
 * that overlaps with PDC server-side ZFP compression.
 *
 * Timing breakdown printed to stderr on rank 0 only.
 * Set GEMM_N=0 to skip GEMM entirely.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <cuda_runtime.h>
#include <cublas_v2.h>

extern "C" void run_gemm_compute(int rank)
{
    /* skip if GEMM_N=0 */
    const char *gemm_env = getenv("GEMM_N");
    if (gemm_env && atoi(gemm_env) == 0) {
        if (rank == 0)
            fprintf(stderr, "[GEMM] GEMM_N=0, skipping compute phase\n");
        return;
    }

    /* pin this rank to a specific GPU */
    int gpu_id = rank % 4;
    cudaSetDevice(gpu_id);

    /* fixed N=8192 — ~160ms per iteration on A100, ~62 iters = 10s */
    int gemm_n = 8192;

    size_t mat_elems = (size_t)gemm_n * gemm_n;
    size_t mat_bytes = mat_elems * sizeof(float);

    /* pinned host memory */
    float *h_A, *h_B, *h_C;
    cudaMallocHost(&h_A, mat_bytes);
    cudaMallocHost(&h_B, mat_bytes);
    cudaMallocHost(&h_C, mat_bytes);
    for (size_t i = 0; i < mat_elems; i++) {
        h_A[i] = (float)(i % 100) * 0.01f;
        h_B[i] = (float)((i + 7) % 100) * 0.01f;
        h_C[i] = 0.0f;
    }

    /* device memory */
    float *d_A, *d_B, *d_C;
    cudaMalloc(&d_A, mat_bytes);
    cudaMalloc(&d_B, mat_bytes);
    cudaMalloc(&d_C, mat_bytes);

    cublasHandle_t handle;
    cublasCreate(&handle);

    /* CUDA events for H2D and D2H timing */
    cudaEvent_t ev_h2d_start, ev_h2d_done, ev_d2h_start, ev_d2h_done;
    cudaEventCreate(&ev_h2d_start);
    cudaEventCreate(&ev_h2d_done);
    cudaEventCreate(&ev_d2h_start);
    cudaEventCreate(&ev_d2h_done);

    /* ── H2D ─────────────────────────────────────────────────────── */
    cudaEventRecord(ev_h2d_start, 0);
    cudaMemcpy(d_A, h_A, mat_bytes, cudaMemcpyHostToDevice);
    cudaMemcpy(d_B, h_B, mat_bytes, cudaMemcpyHostToDevice);
    cudaEventRecord(ev_h2d_done, 0);
    cudaEventSynchronize(ev_h2d_done);

    /* ── GEMM loop: run for ~10 seconds ──────────────────────────── */
    const float alpha = 1.0f, beta = 0.0f;
    double target_sec = 10.0;
    struct timespec t0, t1;
    int iters = 0;

    clock_gettime(CLOCK_MONOTONIC, &t0);
    do {
        cublasSgemm(handle,
                    CUBLAS_OP_N, CUBLAS_OP_N,
                    gemm_n, gemm_n, gemm_n,
                    &alpha, d_A, gemm_n,
                            d_B, gemm_n,
                    &beta,  d_C, gemm_n);
        cudaDeviceSynchronize();
        clock_gettime(CLOCK_MONOTONIC, &t1);
        iters++;
    } while ((t1.tv_sec - t0.tv_sec) +
             (t1.tv_nsec - t0.tv_nsec) / 1e9 < target_sec);

    double compute_sec = (t1.tv_sec - t0.tv_sec) +
                         (t1.tv_nsec - t0.tv_nsec) / 1e9;

    /* ── D2H ─────────────────────────────────────────────────────── */
    cudaEventRecord(ev_d2h_start, 0);
    cudaMemcpy(h_C, d_C, mat_bytes, cudaMemcpyDeviceToHost);
    cudaEventRecord(ev_d2h_done, 0);
    cudaEventSynchronize(ev_d2h_done);

    /* ── timing ──────────────────────────────────────────────────── */
    float h2d_ms = 0, d2h_ms = 0;
    cudaEventElapsedTime(&h2d_ms, ev_h2d_start, ev_h2d_done);
    cudaEventElapsedTime(&d2h_ms, ev_d2h_start, ev_d2h_done);

    double gflops_per_iter = 2.0 * (double)gemm_n * (double)gemm_n * (double)gemm_n / 1e9;
    double avg_gflops = gflops_per_iter * iters / compute_sec;

    if (rank == 0)
        fprintf(stderr,
                "[GEMM] rank=%d gpu=%d N=%d  h2d=%.3f ms  "
                "compute=%.3f s (%d iters, %.1f GFLOPS avg)"
                "  d2h=%.3f ms\n",
                rank, gpu_id, gemm_n,
                h2d_ms, compute_sec, iters, avg_gflops, d2h_ms);

    /* ── cleanup ─────────────────────────────────────────────────── */
    cublasDestroy(handle);
    cudaFree(d_A); cudaFree(d_B); cudaFree(d_C);
    cudaFreeHost(h_A); cudaFreeHost(h_B); cudaFreeHost(h_C);
    cudaEventDestroy(ev_h2d_start);
    cudaEventDestroy(ev_h2d_done);
    cudaEventDestroy(ev_d2h_start);
    cudaEventDestroy(ev_d2h_done);
}

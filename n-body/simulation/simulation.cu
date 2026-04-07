#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "simulation.h"

// Kernel to compute gravitational forces for each particle
// Each thread computes the total force on one particle from all other particles
__global__ void compute_forces_kernel(float* x, float* y, float* m, float* ax,
                                      float* ay, uint32_t n, float G,
                                      float softening) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;

    float p_x = x[i];
    float p_y = y[i];
    float a_x = 0.0f;
    float a_y = 0.0f;

    // softening^2 to avoid division by zero and extreme forces at close range
    float s2 = softening * softening;

    for (int j = 0; j < n; j++) {
        float dx = x[j] - p_x;
        float dy = y[j] - p_y;
        float d2 = dx * dx + dy * dy + s2;

        // Use fast inverse square root
        float inv_d = rsqrtf(d2);
        float inv_d3 = inv_d * inv_d * inv_d;

        float common = G * m[j] * inv_d3;

        a_x += common * dx;
        a_y += common * dy;
    }

    ax[i] = a_x;
    ay[i] = a_y;
}

// Kernel for the drift step in Leapfrog integration
// Updates positions based on velocities and handles boundary conditions
__global__ void drift_kernel(float* x, float* y, float* vx, float* vy, float dt,
                             uint32_t n, float min_x, float max_x, float min_y,
                             float max_y) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;

    x[i] += vx[i] * dt;
    y[i] += vy[i] * dt;

    // Simple reflective boundary conditions
    if (x[i] < min_x) {
        x[i] = min_x;
        vx[i] = -vx[i];
    } else if (x[i] > max_x) {
        x[i] = max_x;
        vx[i] = -vx[i];
    }

    if (y[i] < min_y) {
        y[i] = min_y;
        vy[i] = -vy[i];
    } else if (y[i] > max_y) {
        y[i] = max_y;
        vy[i] = -vy[i];
    }
}

// Kernel for the kick step in Leapfrog integration
// Updates velocities based on computed accelerations
__global__ void kick_kernel(float* vx, float* vy, float* ax, float* ay,
                            float dt, uint32_t n) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;

    vx[i] += ax[i] * dt;
    vy[i] += ay[i] * dt;
}

// CUDA implementation of the simulation loop
void simulate(world* w, nbody_config* conf, FILE* fp) {
    uint32_t n = w->count;
    size_t size = sizeof(float) * n;

    // Device pointers
    float *d_x, *d_y, *d_vx, *d_vy, *d_m, *d_ax, *d_ay;

    // Allocate device memory
    cudaMalloc(&d_x, size);
    cudaMalloc(&d_y, size);
    cudaMalloc(&d_vx, size);
    cudaMalloc(&d_vy, size);
    cudaMalloc(&d_m, size);
    cudaMalloc(&d_ax, size);
    cudaMalloc(&d_ay, size);

    // Copy initial state to device
    cudaMemcpy(d_x, w->x, size, cudaMemcpyHostToDevice);
    cudaMemcpy(d_y, w->y, size, cudaMemcpyHostToDevice);
    cudaMemcpy(d_vx, w->vx, size, cudaMemcpyHostToDevice);
    cudaMemcpy(d_vy, w->vy, size, cudaMemcpyHostToDevice);
    cudaMemcpy(d_m, w->m, size, cudaMemcpyHostToDevice);

    uint32_t threads_per_block = conf->compute.threads_per_block;
    uint32_t num_blocks = (n + threads_per_block - 1) / threads_per_block;

    float G = (float)conf->physics.G;
    float softening = (float)conf->physics.softening;
    float dt = (float)conf->physics.dt;

    write_world_header(w, (conf->physics.steps / conf->io.stride) + 1, fp);

    // Initial half-step for velocity (Leapfrog)
    compute_forces_kernel<<<num_blocks, threads_per_block>>>(
        d_x, d_y, d_m, d_ax, d_ay, n, G, softening);
    kick_kernel<<<num_blocks, threads_per_block>>>(d_vx, d_vy, d_ax, d_ay,
                                                   dt * 0.5f, n);

    for (uint32_t i = 0; i < conf->physics.steps; i++) {
        // Snapshot every 'stride' steps
        if (i % conf->io.stride == 0) {
            cudaMemcpy(w->x, d_x, size, cudaMemcpyDeviceToHost);
            cudaMemcpy(w->y, d_y, size, cudaMemcpyDeviceToHost);
            snapshot_world(w, fp);
        }

        // Drift: update positions
        drift_kernel<<<num_blocks, threads_per_block>>>(
            d_x, d_y, d_vx, d_vy, dt, n, w->wb.min_x, w->wb.max_x, w->wb.min_y,
            w->wb.max_y);

        // Kick: compute forces and update velocities
        compute_forces_kernel<<<num_blocks, threads_per_block>>>(
            d_x, d_y, d_m, d_ax, d_ay, n, G, softening);
        kick_kernel<<<num_blocks, threads_per_block>>>(d_vx, d_vy, d_ax, d_ay,
                                                       dt, n);
    }

    // Final snapshot if needed
    if (conf->physics.steps % conf->io.stride == 0) {
        cudaMemcpy(w->x, d_x, size, cudaMemcpyDeviceToHost);
        cudaMemcpy(w->y, d_y, size, cudaMemcpyDeviceToHost);
        snapshot_world(w, fp);
    }

    // Cleanup device memory
    cudaFree(d_x);
    cudaFree(d_y);
    cudaFree(d_vx);
    cudaFree(d_vy);
    cudaFree(d_m);
    cudaFree(d_ax);
    cudaFree(d_ay);
}

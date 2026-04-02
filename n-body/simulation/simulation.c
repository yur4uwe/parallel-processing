#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../world/world.h"


void compute_forces(world* w, float* accel_x, float* accel_y, float G, float softening) {
    memset(accel_x, 0, sizeof(float) * w->count);
    memset(accel_y, 0, sizeof(float) * w->count);

    for (uint32_t i = 0; i < w->count; i++) {
        for (uint32_t j = i + 1; j < w->count; j++) {
            float dx = w->x[j] - w->x[i];
            float dy = w->y[j] - w->y[i];
            float d2 = dx * dx + dy * dy + softening * softening;
            float inv_d = 1.0f / sqrtf(d2);
            float inv_d3 = inv_d * inv_d * inv_d;
            
            float common = G * inv_d3;
            
            accel_x[i] += common * w->m[j] * dx;
            accel_y[i] += common * w->m[j] * dy;
            accel_x[j] -= common * w->m[i] * dx;
            accel_y[j] -= common * w->m[i] * dy;
        }
    }
}

void simulate(world* w, nbody_config* conf, FILE* fp) {
    float* accel_x = malloc(sizeof(float) * w->count);
    float* accel_y = malloc(sizeof(float) * w->count);
    float G = conf->physics.G;
    float softening = conf->physics.softening;
    float dt = conf->physics.dt;

    write_world_header(w, (conf->physics.steps / conf->io.stride) + 1, fp);

    compute_forces(w, accel_x, accel_y, G, softening);
    // use leapfrog integration
    // Initial half-step for velocity
    for (uint32_t i = 0; i < w->count; i++) {
        w->vx[i] += accel_x[i] * dt * 0.5f;
        w->vy[i] += accel_y[i] * dt * 0.5f;
    }

    for (uint32_t i = 0; i < conf->physics.steps; i++) {
        // snapshot before drift (for step 0)
        if (i % conf->io.stride == 0) {
            snapshot_world(w, fp);
        }

        // drift
        for (uint32_t j = 0; j < w->count; j++) {
            w->x[j] += w->vx[j] * dt;
            w->y[j] += w->vy[j] * dt;

            if (w->x[j] < w->wb.min_x) {
                w->x[j] = w->wb.min_x;
                w->vx[j] = -w->vx[j];
            } else if (w->x[j] > w->wb.max_x) {
                w->x[j] = w->wb.max_x;
                w->vx[j] = -w->vx[j];
            }

            if (w->y[j] < w->wb.min_y) {
                w->y[j] = w->wb.min_y;
                w->vy[j] = -w->vy[j];
            } else if (w->y[j] > w->wb.max_y) {
                w->y[j] = w->wb.max_y;
                w->vy[j] = -w->vy[j];
            }
        }

        // kick
        compute_forces(w, accel_x, accel_y, G, softening);
        for (uint32_t j = 0; j < w->count; j++) {
            w->vx[j] += accel_x[j] * dt;
            w->vy[j] += accel_y[j] * dt;
        }
    }

    // final snapshot if needed
    if (conf->physics.steps % conf->io.stride == 0) {
        snapshot_world(w, fp);
    }

    free(accel_x);
    free(accel_y);
}

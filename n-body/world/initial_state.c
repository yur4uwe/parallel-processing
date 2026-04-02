#include <math.h>
#include <stdio.h>
#include <time.h>

#include "../args/config-parsing.h"
#include "world.h"

uint32_t xorshift(uint32_t* state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

float random_number(uint32_t* state, float min, float max) {
    return ((float)xorshift(state) / (float)UINT32_MAX) * (max - min) + min;
}

// uses Box-Muller transform
float normal_distribution(uint32_t* state, float mean, float std_dev) {
    float u1 = random_number(state, 0.0f, 1.0f);
    float u2 = random_number(state, 0.0f, 1.0f);
    float rand_std_normal = sqrt(-2.0f * log(u1)) * cos(2.0f * M_PI * u2);
    return mean + std_dev * rand_std_normal;
}

float power_law(uint32_t* state, float min, float max, float alpha) {
    float u = random_number(state, 0.0f, 1.0f);
    float a1 = alpha + 1.0f;
    return powf(u * (powf(max, a1) - powf(min, a1)) + powf(min, a1), 1.0f / a1);
}

void random_initial_state(world* w, nbody_config* conf) {
    uint32_t rng_state = conf->initial_state.seed;
    if (rng_state == 0) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        rng_state = (uint32_t)(ts.tv_sec ^ ts.tv_nsec);
    }
    // xorshift must not be 0
    if (rng_state == 0) rng_state = 0xDEADBEEF;

    float v_limit = (conf->initial_state.velocity_scale * conf->initial_state.box_size) / conf->physics.dt;

    for (uint32_t i = 0; i < w->count; i++) {
        w->x[i] =
            random_number(&rng_state, w->wb.min_x, w->wb.max_x);
        w->y[i] =
            random_number(&rng_state, w->wb.min_y, w->wb.max_y);

        if (conf->initial_state.mass_distribution == UNIFORM_MASS_DISTRIBUTION) {
            w->m[i] = random_number(&rng_state, conf->initial_state.mass_min,
                                    conf->initial_state.mass_max);
        } else if (conf->initial_state.mass_distribution ==
                   GAUSSIAN_MASS_DISTRIBUTION) {
            w->m[i] = normal_distribution(&rng_state, conf->initial_state.mass_min,
                                          conf->initial_state.mass_max);
            if (w->m[i] < conf->initial_state.mass_min) w->m[i] = conf->initial_state.mass_min;
        } else if (conf->initial_state.mass_distribution ==
                   POWER_LAW_MASS_DISTRIBUTION) {
            w->m[i] = power_law(&rng_state, conf->initial_state.mass_min,
                                conf->initial_state.mass_max,
                                conf->initial_state.alpha);
        } else {
            printf("WARNING: Invalid mass distribution type %d\n", conf->initial_state.mass_distribution);
            w->m[i] = 1.0f;
        }

        w->vx[i] = random_number(&rng_state, -v_limit, v_limit);
        w->vy[i] = random_number(&rng_state, -v_limit, v_limit);
    }
}


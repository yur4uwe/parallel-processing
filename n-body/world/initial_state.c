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

float get_mass(uint32_t* rng_state, nbody_config* conf) {
    if (conf->initial_state.mass_distribution == UNIFORM_MASS_DISTRIBUTION) {
        return random_number(rng_state, conf->initial_state.mass_min,
                             conf->initial_state.mass_max);
    } else if (conf->initial_state.mass_distribution ==
               GAUSSIAN_MASS_DISTRIBUTION) {
        float m = normal_distribution(rng_state, conf->initial_state.mass_min,
                                      conf->initial_state.mass_max);
        return (m < conf->initial_state.mass_min) ? conf->initial_state.mass_min : m;
    } else if (conf->initial_state.mass_distribution ==
               POWER_LAW_MASS_DISTRIBUTION) {
        return power_law(rng_state, conf->initial_state.mass_min,
                         conf->initial_state.mass_max,
                         conf->initial_state.alpha);
    }
    return 1.0f;
}

void init_spiral(world* w, nbody_config* conf, uint32_t* rng_state, 
                 uint32_t start_idx, uint32_t count, 
                 float offset_x, float offset_y, 
                 float v_offset_x, float v_offset_y,
                 float v_base) {
    float radius_max = conf->initial_state.box_size * 0.45f;
    float G = conf->physics.G;
    float softening = conf->physics.softening;
    
    // Balanced Orbit Physics:
    // With 1/r density distribution (uniform r), gravity scales as 1/r.
    // Circular orbit: v^2/r = G*M(r)/r^2  => v^2 = G*M(r)/r = constant.
    // Total Mass M = v_base^2 * R / G
    float required_gm = v_base * v_base * radius_max;
    float current_avg_mass = (conf->initial_state.mass_max + conf->initial_state.mass_min) * 0.5f;
    float current_gm = G * current_avg_mass * count;
    float mass_multiplier = required_gm / (current_gm + 1e-9f);

    for (uint32_t i = start_idx; i < start_idx + count; i++) {
        float r = random_number(rng_state, radius_max * 0.01f, radius_max);
        float angle = random_number(rng_state, 0.0f, 2.0f * M_PI);
        
        w->x[i] = offset_x + r * cos(angle);
        w->y[i] = offset_y + r * sin(angle);
        w->m[i] = get_mass(rng_state, conf) * mass_multiplier;
        
        // FLAT ROTATION PROFILE with Softened Core:
        // Gravity in the softened core (r < softening) becomes linear.
        // We ramp the velocity linearly in that region to maintain stability.
        float v_mag = v_base;
        if (r < softening * 2.0f) {
            v_mag *= (r / (softening * 2.0f));
        }
        
        w->vx[i] = v_offset_x - v_mag * sin(angle);
        w->vy[i] = v_offset_y + v_mag * cos(angle);
    }
}

void random_initial_state(world* w, nbody_config* conf) {
    uint32_t rng_state = conf->initial_state.seed;
    if (rng_state == 0) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        rng_state = (uint32_t)(ts.tv_sec ^ ts.tv_nsec);
    }
    if (rng_state == 0) rng_state = 0xDEADBEEF;

    float cx = (w->wb.min_x + w->wb.max_x) * 0.5f;
    float cy = (w->wb.min_y + w->wb.max_y) * 0.5f;

    // Core rule: velocity magnitude = (scale * box_size) / dt
    float v_base = (conf->initial_state.velocity_scale * conf->initial_state.box_size) / conf->physics.dt;

    printf("[INFO] Preset %d: Target speed %f (scale: %f)\n", 
           conf->initial_state.type, v_base, (float)conf->initial_state.velocity_scale);

    switch (conf->initial_state.type) {
        case PRESET_RANDOM: {
            for (uint32_t i = 0; i < w->count; i++) {
                w->x[i] = random_number(&rng_state, w->wb.min_x, w->wb.max_x);
                w->y[i] = random_number(&rng_state, w->wb.min_y, w->wb.max_y);
                w->m[i] = get_mass(&rng_state, conf);
                
                float angle = random_number(&rng_state, 0.0f, 2.0f * M_PI);
                w->vx[i] = v_base * cos(angle);
                w->vy[i] = v_base * sin(angle);
            }
            break;
        }
        case PRESET_SPIRAL: {
            init_spiral(w, conf, &rng_state, 0, w->count, cx, cy, 0, 0, v_base);
            break;
        }
        case PRESET_COLLISION: {
            uint32_t half = w->count / 2;
            float dist = conf->initial_state.box_size * 0.25f;
            float v_approach = v_base * 0.15f; 
            
            init_spiral(w, conf, &rng_state, 0, half, cx - dist, cy, v_approach, 0, v_base);
            init_spiral(w, conf, &rng_state, half, w->count - half, cx + dist, cy, -v_approach, 0, v_base);
            break;
        }
        case PRESET_ORBIT: {
            w->x[0] = cx;
            w->y[0] = cy;
            w->vx[0] = 0;
            w->vy[0] = 0;
            
            float G = conf->physics.G;
            float radius_max = conf->initial_state.box_size * 0.45f;
            float r_avg = radius_max * 0.5f;
            
            w->m[0] = (v_base * v_base * r_avg) / G;
            
            for (uint32_t i = 1; i < w->count; i++) {
                float r = random_number(&rng_state, radius_max * 0.1f, radius_max);
                float angle = random_number(&rng_state, 0.0f, 2.0f * M_PI);
                
                w->x[i] = cx + r * cos(angle);
                w->y[i] = cy + r * sin(angle);
                w->m[i] = get_mass(&rng_state, conf);
                
                float v_mag = v_base * sqrtf(r_avg / (r + conf->physics.softening));
                
                w->vx[i] = -v_mag * sin(angle);
                w->vy[i] = v_mag * cos(angle);
            }
            break;
        }
    }
}

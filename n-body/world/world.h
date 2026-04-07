#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>

// Forward declaration to avoid circular dependency
struct nbody_config;

enum {
    UNIFORM_MASS_DISTRIBUTION = (int)0,
    GAUSSIAN_MASS_DISTRIBUTION = (int)1,
    POWER_LAW_MASS_DISTRIBUTION = (int)2,
};

typedef struct {
    float min_x;
    float max_x;
    float min_y;
    float max_y;
} world_bounds;

typedef struct {
    float* x;
    float* y;
    float* vx;
    float* vy;
    float* m;
    uint32_t count;
    world_bounds wb;
} world;

world* create_world(uint32_t count, world_bounds wb);
void free_world(world* w);

void random_initial_state(world* w, struct nbody_config* conf);

void snapshot_world(world* w, FILE* fp);
void load_world_snapshot(world* w, FILE* fp);
void write_world_header(world* w, uint32_t num_snapshots, FILE* fp);
void read_world_header(world* w, uint32_t* num_snapshots, FILE* fp);

void save_world_state(world* w, FILE* fp);
void load_world_state(world* w, FILE* fp);

#ifdef __cplusplus
}
#endif

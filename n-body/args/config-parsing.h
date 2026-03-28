#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef int mode;

enum {
    SERIAL_MODE = (mode)0,
    CUDA_MODE = (mode)1,
};

typedef struct {
    struct {
        uint32_t particles;
        uint32_t steps;
        double dt;
        double G;
        double softening;
    } physics;
    struct {
        char* input_file; // should be null for random generation
        double mass_min;
        double mass_max;
        double box_size;
        double velocity_scale;
    } initial_state;
    struct {
        mode execution_mode;
        uint16_t threads_per_block;
    } compute;
    struct {
        char* output_file;
        uint16_t stride;
        bool compression;
    } io;
} nbody_config;

int parse_config(char* file_name, nbody_config* config);

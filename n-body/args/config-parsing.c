#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../arena/arena.h"
#include "../json/json.h"
#include "config-parsing.h"
#include "../world/world.h"

// Helper to get an object member by key
static json_value* get_obj_member(json_value* obj, const char* key) {
    if (obj == NULL || obj->type != JSON_OBJECT) return NULL;
    for (int i = 0; i < obj->u.object.count; i++) {
        if (strcmp((char*)obj->u.object.keys[i], key) == 0) {
            return obj->u.object.values[i];
        }
    }
    return NULL;
}

int parse_config(char* file_name, nbody_config* config) {
    FILE* cf = NULL;
    uint8_t* json_str = NULL;
    Arena* a = NULL;
    int ec = EXIT_SUCCESS;

    cf = fopen(file_name, "rb");
    if (cf == NULL) {
        printf("Failed to open config file: %s\n", file_name);
        return EXIT_FAILURE;
    }

    fseek(cf, 0, SEEK_END);
    uint64_t file_size = ftell(cf);
    fseek(cf, 0, SEEK_SET);

    json_str = malloc(sizeof(uint8_t) * file_size);
    if (fread(json_str, sizeof(uint8_t), file_size, cf) != file_size) {
        printf("Failed to read %lu bytes from file\n", file_size);
        ec = EXIT_FAILURE;
        goto cleanup;
    }

    a = new_arena(100 * 1024);

    json_value* json = parse_json(a, json_str, file_size);
    if (json == NULL) {
        printf("Failed to parse JSON\n");
        ec = EXIT_FAILURE;
        goto assert_failed;
    }

    // Root assertions
    JSON_ASSERT_OBJECT(json, 4, "Config root must be an object with 4 members");

    // physics section
    json_value* physics = get_obj_member(json, "physics");
    JSON_ASSERT_OBJECT(physics, 5,
                       "Missing 'physics' section or incorrect member count");

    json_value* particles = get_obj_member(physics, "particles");
    JSON_ASSERT_TYPE(particles, JSON_NUMBER,
                     "physics.particles must be a number");
    config->physics.particles = (uint32_t)particles->u.number;

    json_value* steps = get_obj_member(physics, "steps");
    JSON_ASSERT_TYPE(steps, JSON_NUMBER, "physics.steps must be a number");
    config->physics.steps = (uint32_t)steps->u.number;

    json_value* dt = get_obj_member(physics, "dt");
    JSON_ASSERT_TYPE(dt, JSON_NUMBER, "physics.dt must be a number");
    config->physics.dt = dt->u.number;

    json_value* G = get_obj_member(physics, "G");
    JSON_ASSERT_TYPE(G, JSON_NUMBER, "physics.G must be a number");
    config->physics.G = G->u.number;

    json_value* softening = get_obj_member(physics, "softening");
    JSON_ASSERT_TYPE(softening, JSON_NUMBER,
                     "physics.softening must be a number");
    config->physics.softening = softening->u.number;

    // initial_state section
    json_value* initial_state = get_obj_member(json, "initial_state");
    JSON_ASSERT_OBJECT(
        initial_state, 8,
        "Missing 'initial_state' section or incorrect member count");

    json_value* type_val = get_obj_member(initial_state, "type");
    JSON_ASSERT_TYPE(type_val, JSON_STRING, "initial_state.type must be a string");
    if (strcmp((char*)type_val->u.string, "random") == 0) {
        config->initial_state.type = PRESET_RANDOM;
    } else if (strcmp((char*)type_val->u.string, "spiral") == 0) {
        config->initial_state.type = PRESET_SPIRAL;
    } else if (strcmp((char*)type_val->u.string, "collision") == 0) {
        config->initial_state.type = PRESET_COLLISION;
    } else if (strcmp((char*)type_val->u.string, "orbit") == 0) {
        config->initial_state.type = PRESET_ORBIT;
    } else {
        printf("Invalid preset type: %s\n", (char*)type_val->u.string);
        ec = EXIT_FAILURE;
        goto assert_failed;
    }

    json_value* seed = get_obj_member(initial_state, "seed");
    JSON_ASSERT_TYPE(seed, JSON_NUMBER, "initial_state.seed must be a number");
    config->initial_state.seed = seed->u.number;

    json_value* mass_min = get_obj_member(initial_state, "mass_min");
    JSON_ASSERT_TYPE(mass_min, JSON_NUMBER,
                     "initial_state.mass_min must be a number");
    JSON_ASSERT(mass_min->u.number > 0.0, "initial_state.mass_min must be > 0");
    config->initial_state.mass_min = mass_min->u.number;

    json_value* mass_max = get_obj_member(initial_state, "mass_max");
    JSON_ASSERT_TYPE(mass_max, JSON_NUMBER,
                     "initial_state.mass_max must be a number");
    config->initial_state.mass_max = mass_max->u.number;

    json_value* mass_distribution = get_obj_member(initial_state, "mass_distribution");
    JSON_ASSERT_TYPE(mass_distribution, JSON_STRING,
                     "initial_state.mass_distribution must be a string");
    if (strcmp((char*)mass_distribution->u.string, "uniform") == 0) {
        config->initial_state.mass_distribution = UNIFORM_MASS_DISTRIBUTION;
    } else if (strcmp((char*)mass_distribution->u.string, "gaussian") == 0 ||
               strcmp((char*)mass_distribution->u.string, "normal") == 0) {
        config->initial_state.mass_distribution = GAUSSIAN_MASS_DISTRIBUTION;
    } else if (strcmp((char*)mass_distribution->u.string, "power_law") == 0) {
        config->initial_state.mass_distribution = POWER_LAW_MASS_DISTRIBUTION;
    } else {
        printf("Invalid mass distribution type: %s\n",
               (char*)mass_distribution->u.string);
        ec = EXIT_FAILURE;
        goto assert_failed;
    }

    json_value* alpha = get_obj_member(initial_state, "alpha");
    JSON_ASSERT_TYPE(alpha, JSON_NUMBER, "initial_state.alpha must be a number");
    JSON_ASSERT(alpha->u.number > 0.0, "initial_state.alpha must be > 0");
    config->initial_state.alpha = alpha->u.number;

    json_value* box_size = get_obj_member(initial_state, "box_size");
    JSON_ASSERT_TYPE(box_size, JSON_NUMBER,
                     "initial_state.box_size must be a number");
    JSON_ASSERT(box_size->u.number > 0.0, "initial_state.box_size must be > 0");
    config->initial_state.box_size = box_size->u.number;

    json_value* velocity_scale =
        get_obj_member(initial_state, "velocity_scale");
    JSON_ASSERT_TYPE(velocity_scale, JSON_NUMBER,
                     "initial_state.velocity_scale must be a number");
    JSON_ASSERT(velocity_scale->u.number > 0.0,
                "initial_state.velocity_scale must be positive");
    config->initial_state.velocity_scale = velocity_scale->u.number;

    // compute section
    json_value* compute = get_obj_member(json, "compute");
    JSON_ASSERT_OBJECT(compute, 2,
                       "Missing 'compute' section or incorrect member count");

    json_value* mode_val = get_obj_member(compute, "mode");
    JSON_ASSERT_TYPE(mode_val, JSON_STRING, "compute.mode must be a string");
    if (strcmp((char*)mode_val->u.string, "cuda") == 0) {
        config->compute.execution_mode = CUDA_MODE;
    } else {
        config->compute.execution_mode = SERIAL_MODE;
    }

    json_value* threads = get_obj_member(compute, "threads_per_block");
    JSON_ASSERT_TYPE(threads, JSON_NUMBER,
                     "compute.threads_per_block must be a number");
    JSON_ASSERT(threads->u.number > 0.0, "compute.threads_per_block must be > 0");
    config->compute.threads_per_block = (uint16_t)threads->u.number;

    // io section
    json_value* io = get_obj_member(json, "io");
    JSON_ASSERT_OBJECT(io, 3, "Missing 'io' section or incorrect member count");

    json_value* output_file = get_obj_member(io, "output_file");
    JSON_ASSERT_TYPE(output_file, JSON_STRING,
                     "io.output_file must be a string");
    config->io.output_file = strdup((char*)output_file->u.string);

    json_value* stride = get_obj_member(io, "stride");
    JSON_ASSERT_TYPE(stride, JSON_NUMBER, "io.stride must be a number");
    JSON_ASSERT(stride->u.number > 0.0, "io.stride must be > 0");
    config->io.stride = (uint16_t)stride->u.number;

    json_value* compression = get_obj_member(io, "compression");
    JSON_ASSERT_TYPE(compression, JSON_BOOL,
                     "io.compression must be a boolean");
    config->io.compression = compression->u.boolean;

cleanup:
assert_failed:
    if (a) free_arena(a);
    if (cf) fclose(cf);
    if (json_str) free(json_str);
    return ec;
}

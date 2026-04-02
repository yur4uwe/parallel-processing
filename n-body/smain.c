#include <stdio.h>
#include <stdlib.h>

#include "args/config-parsing.h"
#include "world/world.h"
#include "simulation/simulation.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("incorrect argument count, expected 2 got %d", argc);
        return 1;
    }

    nbody_config* conf = malloc(sizeof(nbody_config));

    if (parse_config(argv[1], conf) != EXIT_SUCCESS) {
        free(conf);
        return EXIT_FAILURE;
    }

    float coordinate_range = conf->initial_state.box_size / 2.0;
    world_bounds wb = {-coordinate_range, coordinate_range, -coordinate_range, coordinate_range};

    world* w = create_world(conf->physics.particles, wb);

    random_initial_state(w, conf);

    FILE* fp = fopen(conf->io.output_file, "wb");
    if (fp == NULL) {
        printf("Failed to open output file: %s\n", conf->io.output_file);
        free(conf);
        return EXIT_FAILURE;
    }

    simulate(w, conf, fp);

    free(conf);
    return EXIT_SUCCESS;
}

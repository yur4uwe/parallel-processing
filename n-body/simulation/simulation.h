#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>

#include "../args/config-parsing.h"
#include "../world/world.h"

void simulate(world* w, nbody_config* conf, FILE* fp);

#ifdef __cplusplus
}
#endif


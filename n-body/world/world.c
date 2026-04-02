#include "world.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint16_t quantize(float val, float min, float max) {
    if (val <= min) return 0;
    if (val >= max) return 65535;
    return (uint16_t)((val - min) / (max - min) * 65535.0f);
}

static float dequantize(uint16_t val, float min, float max) {
    return ((float)val / 65535.0f) * (max - min) + min;
}

world* create_world(uint32_t count, world_bounds wb) {
    world* w = malloc(sizeof(world));
    if (!w) return NULL;

    w->count = count;
    w->wb = wb;
    w->x = NULL;
    w->y = NULL;
    w->vx = NULL;
    w->vy = NULL;
    w->m = NULL;

    w->x = malloc(sizeof(float) * count);
    w->y = malloc(sizeof(float) * count);
    w->vx = malloc(sizeof(float) * count);
    w->vy = malloc(sizeof(float) * count);
    w->m = malloc(sizeof(float) * count);

    if (!w->x || !w->y || !w->vx || !w->vy || !w->m) {
        free_world(w);
        return NULL;
    }

    return w;
}

void free_world(world* w) {
    if (!w) return;
    free(w->x);
    free(w->y);
    free(w->vx);
    free(w->vy);
    free(w->m);
    free(w);
}

void write_world_header(world* w, uint32_t num_snapshots, FILE* fp) {
    fwrite(&w->count, sizeof(uint32_t), 1, fp);
    fwrite(&num_snapshots, sizeof(uint32_t), 1, fp);
    fwrite(&w->wb, sizeof(world_bounds), 1, fp);
}

void read_world_header(world* w, uint32_t* num_snapshots, FILE* fp) {
    fread(&w->count, sizeof(uint32_t), 1, fp);
    fread(num_snapshots, sizeof(uint32_t), 1, fp);
    fread(&w->wb, sizeof(world_bounds), 1, fp);
}

void snapshot_world(world* w, FILE* fp) {
    for (uint32_t i = 0; i < w->count; i++) {
        uint16_t qx = quantize(w->x[i], w->wb.min_x, w->wb.max_x);
        uint16_t qy = quantize(w->y[i], w->wb.min_y, w->wb.max_y);
        fwrite(&qx, sizeof(uint16_t), 1, fp);
        fwrite(&qy, sizeof(uint16_t), 1, fp);
    }
}

void load_world_snapshot(world* w, FILE* fp) {
    for (uint32_t i = 0; i < w->count; i++) {
        uint16_t qx, qy;
        fread(&qx, sizeof(uint16_t), 1, fp);
        fread(&qy, sizeof(uint16_t), 1, fp);
        w->x[i] = dequantize(qx, w->wb.min_x, w->wb.max_x);
        w->y[i] = dequantize(qy, w->wb.min_y, w->wb.max_y);
    }
}

void save_world_state(world* w, FILE* fp) {
    fwrite(&w->count, sizeof(uint32_t), 1, fp);
    fwrite(&w->wb, sizeof(world_bounds), 1, fp);

    for (uint32_t i = 0; i < w->count; i++) {
        fwrite(&w->x[i], sizeof(float), 1, fp);
        fwrite(&w->y[i], sizeof(float), 1, fp);
        fwrite(&w->m[i], sizeof(float), 1, fp);
        fwrite(&w->vx[i], sizeof(float), 1, fp);
        fwrite(&w->vy[i], sizeof(float), 1, fp);
    }
}

void load_world_state(world* w, FILE* fp) {
    fread(&w->count, sizeof(uint32_t), 1, fp);
    fread(&w->wb, sizeof(world_bounds), 1, fp);

    for (uint32_t i = 0; i < w->count; i++) {
        fread(&w->x[i], sizeof(float), 1, fp);
        fread(&w->y[i], sizeof(float), 1, fp);
        fread(&w->m[i], sizeof(float), 1, fp);
        fread(&w->vx[i], sizeof(float), 1, fp);
        fread(&w->vy[i], sizeof(float), 1, fp);
    }
}

#include "../freq-table.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int write_table(FILE* output, uint32_t freq[256]) {
    freq_table tbl = { .len = 0, .entries = NULL };
    table_to_entries(&tbl, freq);

    int ec = EXIT_SUCCESS;
    if (fwrite(&tbl.len, sizeof(uint32_t), 1, output) != 1) {
        printf("Failed to write frequency table size\n");
        ec = EXIT_FAILURE;
        goto exit;
    }
    if (fwrite(tbl.entries, sizeof(freq_entry), tbl.len, output) != tbl.len) {
        printf("Failed to write frequency entries\n");
        ec = EXIT_FAILURE;
    }
exit:
    free(tbl.entries);
    return ec;
}

int read_table(FILE* input, uint32_t freq[256]) {
    freq_table tbl = { .len = 0, .entries = NULL };

    int ec = EXIT_SUCCESS;
    if (fread(&tbl.len, sizeof(uint32_t), 1, input) != 1) {
        printf("Failed to read frequency table size\n");
        ec = EXIT_FAILURE;
        goto exit;
    }
    tbl.entries = malloc(sizeof(freq_entry) * tbl.len);
    if (fread(tbl.entries, sizeof(freq_entry), tbl.len, input) != tbl.len) {
        printf("Failed to read frequency entries\n");
        ec = EXIT_FAILURE;
        goto exit;
    }
    entries_to_table(&tbl, freq);
exit:
    free(tbl.entries);
    return ec;
}


#include "freq-table.h"
#include "mpi.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    uint8_t symbol;
    uint32_t freq;
} freq_entry;

typedef struct {
    uint32_t len;
    freq_entry* entries;
} freq_table;

MPI_Datatype create_freq_entry() {
    int blocklengths[] = {1, 1};

    MPI_Aint displacements[2];
    freq_entry dummy;
    MPI_Aint base;
    MPI_Get_address(&dummy, &base);
    MPI_Get_address(&dummy.symbol, &displacements[0]);
    MPI_Get_address(&dummy.freq, &displacements[1]);
    displacements[0] -= base;
    displacements[1] -= base;

    MPI_Datatype types[] = { MPI_UINT8_T, MPI_UINT32_T };
    MPI_Datatype freq_entry_type;
    MPI_Type_create_struct(2, blocklengths, displacements, types, &freq_entry_type);
    MPI_Type_commit(&freq_entry_type);
    return freq_entry_type;
}

void table_to_entries(freq_table* tbl, uint32_t freq[256]) {
    uint32_t cap = 32;
    tbl->len = 0;
    tbl->entries = malloc(sizeof(freq_entry) * cap);
    for (int i = 0; i < 256; i++) {
        if (freq[i] == 0) continue;
        if (tbl->len == cap) {
            cap *= 2;
            tbl->entries = realloc(tbl->entries, sizeof(freq_entry) * cap);
        }
        tbl->entries[tbl->len++] = (freq_entry){ .symbol = i, .freq = freq[i] };
    }
}

void entries_to_table(freq_table* tbl, uint32_t freq[256]) {
    for (int i = 0; i < tbl->len; i++) {
        freq_entry ent = tbl->entries[i];
        freq[ent.symbol] = ent.freq;
    }
}

int swrite_table(FILE* output, uint32_t freq[256]) {
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

int sread_table(FILE* input, uint32_t freq[256]) {
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

int pwrite_table(MPI_File output, uint32_t freq[256]) {
    MPI_Datatype MPI_FREQ_ENTRY = create_freq_entry();
    freq_table tbl = { .len = 0, .entries = NULL };
    table_to_entries(&tbl, freq);

    int ec = EXIT_SUCCESS;
    if (MPI_File_write(output, &tbl.len, 1, MPI_UINT32_T, MPI_STATUS_IGNORE) != MPI_SUCCESS) {
        printf("Failed to write frequency table size\n");
        ec = EXIT_FAILURE;
        goto exit;
    }
    if (MPI_File_write(output, tbl.entries, tbl.len, MPI_FREQ_ENTRY, MPI_STATUS_IGNORE) != MPI_SUCCESS) {
        printf("Failed to write frequency entries\n");
        ec = EXIT_FAILURE;
    }
exit:
    free(tbl.entries);
    MPI_Type_free(&MPI_FREQ_ENTRY);
    return ec;
}

int pread_table(MPI_File input, uint32_t freq[256]) {
    MPI_Datatype MPI_FREQ_ENTRY = create_freq_entry();
    freq_table tbl = { .len = 0, .entries = NULL };

    int ec = EXIT_SUCCESS;
    if (MPI_File_read(input, &tbl.len, 1, MPI_UINT32_T, MPI_STATUS_IGNORE) != MPI_SUCCESS) {
        printf("Failed to read frequency table size\n");
        ec = EXIT_FAILURE;
        goto exit;
    }
    tbl.entries = malloc(sizeof(freq_entry) * tbl.len);
    if (MPI_File_read(input, tbl.entries, tbl.len, MPI_FREQ_ENTRY, MPI_STATUS_IGNORE) != MPI_SUCCESS) {
        printf("Failed to read frequency entries\n");
        ec = EXIT_FAILURE;
        goto exit;
    }
    entries_to_table(&tbl, freq);
exit:
    free(tbl.entries);
    MPI_Type_free(&MPI_FREQ_ENTRY);
    return ec;
}

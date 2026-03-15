#include "../freq-table.h"
#include "mpi.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

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

int write_table(MPI_File output, uint32_t freq[256]) {
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

int read_table(MPI_File input, uint32_t freq[256]) {
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

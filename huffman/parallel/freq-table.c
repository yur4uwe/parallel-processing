#include "../freq-table.h"
#include "mpi.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

MPI_Datatype create_freq_entry() {
    int blocklengths[] = {1, 1};

    MPI_Aint displacements[2];
    freq_entry dummy = {0};
    MPI_Aint base;
    MPI_Get_address(&dummy, &base);
    MPI_Get_address(&dummy.freq, &displacements[0]);
    MPI_Get_address(&dummy.symbol, &displacements[1]);
    displacements[0] -= base;
    displacements[1] -= base;

    MPI_Datatype types[] = { MPI_UINT32_T, MPI_UINT8_T };
    MPI_Datatype freq_entry_type;
    MPI_Type_create_struct(2, blocklengths, displacements, types, &freq_entry_type);
    MPI_Type_commit(&freq_entry_type);

    MPI_Datatype packed_freq_entry;
    MPI_Type_create_resized(freq_entry_type, 0, sizeof(freq_entry), &packed_freq_entry);
    MPI_Type_commit(&packed_freq_entry);
    
    MPI_Type_free(&freq_entry_type);
    return packed_freq_entry;
}

int write_table_at(MPI_File output, MPI_Offset offset, uint32_t freq[256], MPI_Offset* next_offset) {
    MPI_Datatype MPI_FREQ_ENTRY = create_freq_entry();
    freq_table tbl = { .len = 0, .entries = NULL };
    table_to_entries(&tbl, freq);

    int ec = EXIT_SUCCESS;
    MPI_Offset current_offset = offset;

    if (MPI_File_write_at(output, current_offset, &tbl.len, 1, MPI_UINT32_T, MPI_STATUS_IGNORE) != MPI_SUCCESS) {
        printf("Failed to write frequency table size\n");
        ec = EXIT_FAILURE;
        goto exit;
    }
    current_offset += sizeof(uint32_t);

    if (MPI_File_write_at(output, current_offset, tbl.entries, tbl.len, MPI_FREQ_ENTRY, MPI_STATUS_IGNORE) != MPI_SUCCESS) {
        printf("Failed to write frequency entries\n");
        ec = EXIT_FAILURE;
        goto exit;
    }
    current_offset += tbl.len * sizeof(freq_entry);

    if (next_offset) *next_offset = current_offset;

exit:
    free(tbl.entries);
    MPI_Type_free(&MPI_FREQ_ENTRY);
    return ec;
}

int read_table_at(MPI_File input, MPI_Offset offset, uint32_t freq[256], MPI_Offset* next_offset) {
    MPI_Datatype MPI_FREQ_ENTRY = create_freq_entry();
    freq_table tbl = { .len = 0, .entries = NULL };

    int ec = EXIT_SUCCESS;
    MPI_Offset current_offset = offset;

    if (MPI_File_read_at(input, current_offset, &tbl.len, 1, MPI_UINT32_T, MPI_STATUS_IGNORE) != MPI_SUCCESS) {
        printf("Failed to read frequency table size\n");
        ec = EXIT_FAILURE;
        goto exit;
    }
    current_offset += sizeof(uint32_t);

    tbl.entries = malloc(sizeof(freq_entry) * tbl.len);
    if (!tbl.entries && tbl.len > 0) {
        ec = EXIT_FAILURE;
        goto exit;
    }

    if (MPI_File_read_at(input, current_offset, tbl.entries, tbl.len, MPI_FREQ_ENTRY, MPI_STATUS_IGNORE) != MPI_SUCCESS) {
        printf("Failed to read frequency entries\n");
        ec = EXIT_FAILURE;
        goto exit;
    }
    current_offset += tbl.len * sizeof(freq_entry);

    entries_to_table(&tbl, freq);
    if (next_offset) *next_offset = current_offset;

exit:
    free(tbl.entries);
    MPI_Type_free(&MPI_FREQ_ENTRY);
    return ec;
}

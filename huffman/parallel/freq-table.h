#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

int write_table_at(MPI_File output, MPI_Offset offset, uint32_t freq[256], MPI_Offset* next_offset);
int read_table_at(MPI_File input, MPI_Offset offset, uint32_t freq[256], MPI_Offset* next_offset);

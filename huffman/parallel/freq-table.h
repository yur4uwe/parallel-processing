#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

int pwrite_table(MPI_File output, uint32_t freq[256]);
int pread_table(MPI_File input, uint32_t freq[256]);

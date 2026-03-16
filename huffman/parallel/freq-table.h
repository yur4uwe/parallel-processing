#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

int write_table(MPI_File output, uint32_t freq[256]);
int read_table(MPI_File input, uint32_t freq[256]);

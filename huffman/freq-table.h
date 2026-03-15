#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

typedef struct {
	uint32_t len;
	uint8_t* symbols;
	uint32_t* freqs;
} freq_list;

int swrite_table(FILE* output, uint32_t freq[256]);
int sread_table(FILE* input, uint32_t freq[256]);

int pwrite_table(MPI_File output, uint32_t freq[256]);
int pread_table(MPI_File input, uint32_t freq[256]);

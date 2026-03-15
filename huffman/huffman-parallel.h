#pragma once
#include <mpi.h>

int huffman_compress(MPI_File* in_fp, MPI_File* out_fp, int size, int rank);
int huffman_decompress(MPI_File *in_fp, MPI_File *out_fp, int size, int rank);


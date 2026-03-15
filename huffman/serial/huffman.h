#pragma once

#include <stdio.h>
#include <stdint.h>

int huffman_compress(FILE* in_fp, FILE* out_fp);
int huffman_decompress(FILE* in_fp, FILE* out_fp);

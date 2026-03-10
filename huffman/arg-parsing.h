#pragma once

#include <stdint.h>
#include <stdio.h>

typedef struct {
	const char* in_path;
    const char* out_path;
} huffman_args;


int parse_args(huffman_args* parsed_args, int argc, char *argv[]);

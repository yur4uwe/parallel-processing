#include "arg-parsing.h"
#include "consts.h"
#include "serial/huffman.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
	huffman_args* args = malloc(sizeof(huffman_args));
	int ec = SUCCESS;

	ec = parse_args(args, argc, argv);
	if (ec == FAILURE) {
		return 1;
	} else if (ec == HELP) {
		return 0;
	}

	FILE* in_fp = fopen(args->in_path, "rb");
	if (in_fp == NULL) {
		fprintf(stderr, "Failed to open input file %s\n", args->in_path);
		return FAILURE;
	}

	FILE* out_fp = fopen(args->out_path, "wb");
    if (out_fp == NULL) {
        fprintf(stderr, "Failed to open output file: %s\n", args->out_path);
        ec = FAILURE;
        goto cleanup_in;
    }

    if (args->mode == MODE_COMPRESS) {
        ec = huffman_compress(in_fp, out_fp);
    } else if (args->mode == MODE_DECOMPRESS) {
        ec = huffman_decompress(in_fp, out_fp);
    } else {
        printf("Unrecognized mode of operation: %d", args->mode);
        ec = FAILURE;
    }

	fclose(out_fp);
cleanup_in:
	fclose(in_fp);
	free(args);

	return 0;
}

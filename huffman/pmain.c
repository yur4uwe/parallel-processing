#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#include "arg-parsing.h"
#include "consts.h"
#include "parallel/huffman.h"

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    huffman_args* hfmn_args = malloc(sizeof(huffman_args));
    int ec = EXIT_SUCCESS;

    ec = parse_args(hfmn_args, argc, argv);
    if (ec == EXIT_FAILURE) {
        DEBUG("Failed to parse arguments");
        ec = EXIT_FAILURE;
        goto exit;
    } else if (ec == HELP) {
        DEBUG("Called HELP");
        ec = EXIT_SUCCESS;
        goto exit;
    }

    DEBUG("Opening input file");

    MPI_File in_fp;
    if (MPI_File_open(MPI_COMM_WORLD, hfmn_args->in_path, MPI_MODE_RDONLY,
                      MPI_INFO_NULL, &in_fp) != MPI_SUCCESS) {
        printf("failed to open input file, aborting");
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }

    DEBUG("Opened input file, opening output file");

    MPI_File out_fp;
    if (MPI_File_open(MPI_COMM_WORLD, hfmn_args->out_path, MPI_MODE_WRONLY|MPI_MODE_CREATE,
                      MPI_INFO_NULL, &out_fp) != MPI_SUCCESS) {
        printf("failed to open output file, aborting");
        ec = EXIT_FAILURE;
        goto cleanup_in;
    }
    
    MPI_File_set_size(out_fp, 0);

    DEBUG("Opened output file, about to execute in %d", hfmn_args->mode);

    if (hfmn_args->mode == MODE_COMPRESS) {
        ec = huffman_compress(in_fp, out_fp, size, rank);
    } else if (hfmn_args->mode == MODE_DECOMPRESS) {
        ec = huffman_decompress(in_fp, out_fp, size, rank);
    } else {
        DEBUG("unrecognized mode of execution returning EXIT_FAILURE");
        ec = EXIT_FAILURE;
        printf("unrecognized mode\n");
    }

    DEBUG("Executed %d mode exit code: %d", hfmn_args->mode, ec);

    MPI_File_close(&out_fp);
cleanup_in:
    MPI_File_close(&in_fp);
exit:
    free(hfmn_args);

    // if (ec != EXIT_SUCCESS) {
    //     MPI_Abort(MPI_COMM_WORLD, ec);
    // }

    MPI_Finalize();

    return ec;
}

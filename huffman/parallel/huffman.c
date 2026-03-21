#include <mpi.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../consts.h"
#include "../huffman-common.h"
#include "../huffman-node.h"
#include "../min-heap.h"
#include "huffman.h"
#include "freq-table.h"

// used by huffman_worker
void count_freqs(uint32_t freqs[256], uint8_t* buf, uint32_t chunk_size) {
    count_freqs_buf(freqs, buf, chunk_size);
}

int worker_encode(char codebook[256][256], uint8_t* buf, uint32_t chunk_size, uint8_t* res_buf) {
    int res_buf_size = 1;  // reserve space for padding byte

    uint8_t byte_buffer = 0;
    uint8_t bit_count = 0;
    for (uint32_t i = 0; i < chunk_size; i++) {
        char* encoded = codebook[buf[i]];

        for (uint8_t bit_idx = 0; encoded[bit_idx] != '\0'; bit_idx++) {
            char bit = encoded[bit_idx];

            byte_buffer <<= 1;
            if (bit == '1') {
                byte_buffer |= 1;
            }
            bit_count++;

            if (bit_count == 8) {
                res_buf[res_buf_size++] = byte_buffer;

                byte_buffer = 0;
                bit_count = 0;
            }
        }
    }

    uint8_t padding = 0;
    if (bit_count > 0) {
        padding = 8 - bit_count;
        byte_buffer <<= padding;
        res_buf[res_buf_size++] = byte_buffer;
    }

    res_buf[0] = padding;

    return res_buf_size;
}

uint32_t decode(huffman_node* root, uint8_t* comp_buf, uint32_t comp_buf_size, uint8_t* uncomp_buf) {
    uint8_t padding = comp_buf[0];
    uint32_t uncomp_buf_idx = 0;

    huffman_node* curr_node = root;
    for (uint32_t i = 1; i < comp_buf_size; i++) {
        uint8_t curr_byte = comp_buf[i];

        for (int bit_idx = 7; bit_idx >= 0; bit_idx--) {
            if (i == comp_buf_size - 1 && bit_idx < (int)padding) {
                break;
            }

            int bit = (curr_byte >> bit_idx) & 1;
            curr_node = bit ? curr_node->right : curr_node->left;

            if (curr_node->left == NULL && curr_node->right == NULL) {
                uncomp_buf[uncomp_buf_idx++] = curr_node->symbol;
                curr_node = root;
            }
        }
    }

    return uncomp_buf_idx;
}

int huffman_master(MPI_File out_fp, uint64_t chunks_num) {
    uint32_t global_freqs[256];
    MPI_Status status;

    DEBUG("Master starting: waiting for global frequencies");

    // Wait for the global frequency table from any worker (tag 0)
    // needs to be blocking as master writes it to file right after
    if (MPI_Recv(global_freqs, 256, MPI_UINT32_T, MPI_ANY_SOURCE, 0,
                  MPI_COMM_WORLD, &status) != MPI_SUCCESS) {
        DEBUG("Master failed to initiate Irecv for frequencies");
        return EXIT_FAILURE;
    }

    DEBUG("Master received frequencies from rank %d", status.MPI_SOURCE);

    // Write Table Length and Table
    if (write_table(out_fp, global_freqs) != EXIT_SUCCESS) {
        DEBUG("Master failed to write frequency table");
        return EXIT_FAILURE;
    }

    // CHUNK COUNT WRITE
    uint32_t c_num = (uint32_t)chunks_num;
    if (MPI_File_write(out_fp, &c_num, 1, MPI_UINT32_T, MPI_STATUS_IGNORE) !=
        MPI_SUCCESS) {
        DEBUG("Master failed to write chunk count");
        return EXIT_FAILURE;
    }

    // Capture position before Chunk Size Table
    MPI_Offset size_table_pos;
    MPI_File_get_position(out_fp, &size_table_pos);
    DEBUG("Master: Size table reserved at position %lld", size_table_pos);

    uint32_t* chunk_sizes = malloc(chunks_num * sizeof(uint32_t));
    if (!chunk_sizes) {
        DEBUG("Master failed to allocate chunk_sizes (NULL)");
        return EXIT_FAILURE;
    }

    MPI_Request* size_reqs = malloc(chunks_num * sizeof(MPI_Request));
    if (!size_reqs) {
        DEBUG("Master failed to allocate size_reqs (NULL)");
        free(chunk_sizes);
        return EXIT_FAILURE;
    }

    // Reserve space for size table
    MPI_File_seek(out_fp, chunks_num * sizeof(uint32_t), MPI_SEEK_CUR);
    MPI_Offset header_size;
    MPI_File_get_position(out_fp, &header_size);
    DEBUG("Master: Header finished. Total header size: %lld. Jumping back to %lld later to write sizes.", header_size, size_table_pos);

    DEBUG("Master initiating Irecv for %lu chunk sizes", chunks_num);
    for (uint32_t i = 0; i < chunks_num; i++) {
        if (MPI_Irecv(&chunk_sizes[i], 1, MPI_UINT32_T, MPI_ANY_SOURCE, i + 1,
                      MPI_COMM_WORLD, &size_reqs[i]) != MPI_SUCCESS) {
            DEBUG("Master failed to initiate Irecv for chunk size %u", i);
            // In a real app we might want to cancel others, but for now we just exit
            free(chunk_sizes);
            free(size_reqs);
            return EXIT_FAILURE;
        }
    }

    DEBUG("Master broadcasting header size: %lld", header_size);
    if (MPI_Bcast(&header_size, 1, MPI_OFFSET, 0, MPI_COMM_WORLD) !=
        MPI_SUCCESS) {
        DEBUG("Master failed to broadcast header size");
        free(chunk_sizes);
        free(size_reqs);
        return EXIT_FAILURE;
    }

    DEBUG("Master waiting for all chunk sizes...");
    if (MPI_Waitall((int)chunks_num, size_reqs, MPI_STATUSES_IGNORE) != MPI_SUCCESS) {
        DEBUG("Master Waitall failed for chunk sizes");
        free(chunk_sizes);
        free(size_reqs);
        return EXIT_FAILURE;
    }
    DEBUG("Master received all chunk sizes");

    // Write the collected sizes back at the reserved spot
    if (MPI_File_write_at(out_fp, size_table_pos, chunk_sizes, chunks_num,
                          MPI_UINT32_T, MPI_STATUS_IGNORE) != MPI_SUCCESS) {
        DEBUG("Master failed to write compressed size table");
        free(chunk_sizes);
        free(size_reqs);
        return EXIT_FAILURE;
    }

    DEBUG("Master finished routine successfully");
    free(chunk_sizes);
    free(size_reqs);
    return EXIT_SUCCESS;
}

int huffman_worker(MPI_File in_fp, MPI_File out_fp, MPI_Comm worker_comm,
                   MPI_Offset file_size) {
    int worker_size, worker_rank;
    MPI_Comm_rank(worker_comm, &worker_rank);
    MPI_Comm_size(worker_comm, &worker_size);

    uint64_t chunks_num = (file_size + CHUNK_SIZE - 1) / CHUNK_SIZE;
    uint64_t start_chunk = (uint64_t)worker_rank * chunks_num / worker_size;
    uint64_t end_chunk = (uint64_t)(worker_rank + 1) * chunks_num / worker_size;
    uint32_t chunks_per_process = (uint32_t)(end_chunk - start_chunk);

    DEBUG_COMM(worker_comm, "Worker starting: %u chunks (Start: %lu, End: %lu)", chunks_per_process, start_chunk, end_chunk);

    int ec = EXIT_FAILURE;
    uint8_t** chunk_bufs = calloc(chunks_per_process, sizeof(uint8_t*));
    uint32_t* compressed_sizes = calloc(chunks_per_process, sizeof(uint32_t));
    uint32_t local_freqs[256] = {0};

    if (chunks_per_process > 0 && (!chunk_bufs || !compressed_sizes)) {
        DEBUG_COMM(worker_comm, "Worker failed to allocate buffers (NULL)");
        goto exit;
    }

    // read & count frequencies
    for (uint32_t i = 0; i < chunks_per_process; i++) {
        chunk_bufs[i] = malloc(CHUNK_SIZE);
        if (!chunk_bufs[i]) {
            DEBUG_COMM(worker_comm, "Worker failed to allocate chunk_buf[%u] (NULL)", i);
            goto exit;
        }

        MPI_Offset read_offset = (start_chunk + i) * CHUNK_SIZE;
        uint32_t to_read = CHUNK_SIZE;
        if (read_offset + CHUNK_SIZE > file_size) {
            to_read = (uint32_t)(file_size - read_offset);
        }

        if (MPI_File_read_at(in_fp, read_offset, chunk_bufs[i], to_read,
                             MPI_BYTE, MPI_STATUS_IGNORE) != MPI_SUCCESS) {
            DEBUG_COMM(worker_comm, "Worker failed to read chunk %u at %lld", i, read_offset);
            goto exit;
        }
        count_freqs(local_freqs, chunk_bufs[i], to_read);
    }

    // aggregate frequencies
    uint32_t global_freqs[256];
    if (MPI_Allreduce(local_freqs, global_freqs, 256, MPI_UINT32_T, MPI_SUM,
                      worker_comm) != MPI_SUCCESS) {
        DEBUG_COMM(worker_comm, "Worker Allreduce failed");
        goto exit;
    }

    // Send the global table to Master (only one worker needs to do this)
    MPI_Request freq_send_req = MPI_REQUEST_NULL;
    if (worker_rank == 0) {
        DEBUG_COMM(worker_comm, "Worker 0 initiating Isend for frequencies: %p", global_freqs);
        if (MPI_Isend(global_freqs, 256, MPI_UINT32_T, 0, 0, MPI_COMM_WORLD, &freq_send_req) !=
            MPI_SUCCESS) {
            DEBUG_COMM(worker_comm, "Worker failed to initiate Isend for frequencies");
            goto exit;
        }
        DEBUG_COMM(worker_comm, "Worker 0 successfully sent frequencies");
    }

    DEBUG_COMM(worker_comm, "about to create a huffman tree");

    huffman_node* root = create_huffman_tree(global_freqs);
    if (!root) {
        DEBUG_COMM(worker_comm, "Worker failed to create Huffman tree (NULL)");
        goto exit;
    }

    DEBUG_COMM(worker_comm, "Worker have successfully build huffman tree: %p", root);

    char codebook[256][256] = {0};
    char curr_code[256];
    generate_codebook(codebook, root, curr_code, 0);


    uint8_t* scratch_buf = malloc(CHUNK_SIZE * 2); // 2x safety margin
    if (!scratch_buf) {
        DEBUG_COMM(worker_comm, "Worker failed to allocate scratch_buf (NULL)");
        goto exit;
    }


    DEBUG_COMM(worker_comm, "Worker: codebook build successfully, scratch initialied to %p", scratch_buf);

    MPI_Request* size_send_reqs = malloc(chunks_per_process * sizeof(MPI_Request));
    if (chunks_per_process > 0 && !size_send_reqs) {
        DEBUG_COMM(worker_comm, "Worker failed to allocate size_send_reqs (NULL)");
        free(scratch_buf);
        goto exit;
    }

    uint64_t total_compressed_size = 0;
    for (uint32_t i = 0; i < chunks_per_process; i++) {
        uint32_t current_chunk_raw_size = CHUNK_SIZE;
        MPI_Offset read_offset = (start_chunk + i) * CHUNK_SIZE;
        if (read_offset + CHUNK_SIZE > file_size) {
            current_chunk_raw_size = (uint32_t)(file_size - read_offset);
        }


        int c_size = worker_encode(codebook, chunk_bufs[i], current_chunk_raw_size, scratch_buf);
        compressed_sizes[i] = (uint32_t)c_size;
        total_compressed_size += c_size;

        DEBUG_COMM(worker_comm, "Worker: encoded chunk %d with size %u to size %d", i, current_chunk_raw_size, c_size);

        // Reuse buffer for compressed data
        free(chunk_bufs[i]);
        chunk_bufs[i] = malloc(c_size);
        if (!chunk_bufs[i]) {
            DEBUG_COMM(worker_comm, "Worker failed to realloc chunk_buf[%u] to size %d", i, c_size);
            free(scratch_buf);
            free(size_send_reqs);
            goto exit;
        }
        memcpy(chunk_bufs[i], scratch_buf, c_size);

        // Notify master of this chunk's compressed size (tag = global_idx + 1)
        uint32_t global_idx = (uint32_t)(start_chunk + i);
        DEBUG_COMM(worker_comm, "Worker: sent master the compressed size %p", compressed_sizes);
        if (MPI_Isend(&compressed_sizes[i], 1, MPI_UINT32_T, 0, global_idx + 1,
                     MPI_COMM_WORLD, &size_send_reqs[i]) != MPI_SUCCESS) {
            DEBUG_COMM(worker_comm, "Worker failed to initiate Isend for chunk size %u", global_idx);
            free(scratch_buf);
            free(size_send_reqs);
            goto exit;
        }
    }
    free(scratch_buf);
    DEBUG_COMM(worker_comm, "Worker: Compression finished. Total compressed size: %lu", total_compressed_size);

    // receive header offset from master
    MPI_Offset header_size;
    DEBUG_COMM(worker_comm, "Worker waiting for header size bcast");
    if (MPI_Bcast(&header_size, 1, MPI_OFFSET, 0, MPI_COMM_WORLD) != MPI_SUCCESS) {
        DEBUG_COMM(worker_comm, "Worker failed to receive header size bcast");
        free(size_send_reqs);
        goto exit;
    }

    // calculate global write offset using exclusive scan
    MPI_Offset write_offset = 0;
    if (MPI_Exscan(&total_compressed_size, &write_offset, 1, MPI_OFFSET, MPI_SUM,
                   worker_comm) != MPI_SUCCESS) {
        DEBUG_COMM(worker_comm, "Worker Exscan failed");
        free(size_send_reqs);
        goto exit;
    }
    DEBUG_COMM(worker_comm, "Worker: Exscan finished. Local write_offset: %lld", write_offset);

    // perform parallel writes
    MPI_Offset current_pos = header_size + write_offset;
    DEBUG_COMM(worker_comm, "Worker: Starting parallel writes at global position %lld", current_pos);
    for (uint32_t i = 0; i < chunks_per_process; i++) {
        if (MPI_File_write_at(out_fp, current_pos, chunk_bufs[i],
                              compressed_sizes[i], MPI_BYTE,
                              MPI_STATUS_IGNORE) != MPI_SUCCESS) {
            DEBUG_COMM(worker_comm, "Worker failed to write chunk %u at %lld", i, current_pos);
            free(size_send_reqs);
            goto exit;
        }
        current_pos += compressed_sizes[i];
    }

    DEBUG_COMM(worker_comm, "Worker waiting for all non-blocking sends to finish");
    if (worker_rank == 0) MPI_Wait(&freq_send_req, MPI_STATUS_IGNORE);
    if (chunks_per_process > 0) {
        MPI_Waitall((int)chunks_per_process, size_send_reqs, MPI_STATUSES_IGNORE);
    }
    DEBUG_COMM(worker_comm, "Worker finished routine successfully");

    ec = EXIT_SUCCESS;
    free(size_send_reqs);
exit:
    if (chunk_bufs) {
        for (uint32_t i = 0; i < chunks_per_process; i++) free(chunk_bufs[i]);
        free(chunk_bufs);
    }
    free(compressed_sizes);
    return ec;
}

int huffman_compress(MPI_File in_fp, MPI_File out_fp, int world_size, int world_rank) {
    MPI_Offset file_size;
    MPI_File_get_size(in_fp, &file_size);
    uint64_t chunks_num = (file_size + CHUNK_SIZE - 1) / CHUNK_SIZE;

    MPI_Comm sub_comm;
    MPI_Comm_split(MPI_COMM_WORLD, (world_rank == 0 ? 0 : 1), world_rank, &sub_comm);

    if (world_rank == 0) {
        MPI_Comm_set_name(sub_comm, "MasterSub");
        DEBUG("Master (Rank %d) starting. File size: %lld, Total Chunks: %lu", world_rank, file_size, chunks_num);
        return huffman_master(out_fp, chunks_num);
    } else {
        MPI_Comm_set_name(sub_comm, "WorkerSub");
        DEBUG("Worker (Rank %d) starting. File size: %lld, Total Chunks: %lu", world_rank, file_size, chunks_num);
        return huffman_worker(in_fp, out_fp, sub_comm, file_size);
    }
}

int huffman_decompress(MPI_File in_fp, MPI_File out_fp, int world_size, int world_rank) {
    uint32_t freqs[256];
    if (read_table(in_fp, freqs) != EXIT_SUCCESS) {
        DEBUG("Decompress: Failed to read frequency table");
        return EXIT_FAILURE;
    }
    DEBUG("Decompress: Frequency table read successfully");

    huffman_node* root = create_huffman_tree(freqs);

    uint32_t chunks_num;
    if (MPI_File_read(in_fp, &chunks_num, 1, MPI_UINT32_T, MPI_STATUS_IGNORE) != MPI_SUCCESS) {
        DEBUG("Decompress: Failed to read amount of chunks");
        return EXIT_FAILURE;
    }
    DEBUG("Decompress: Total chunks to process: %u", chunks_num);

    uint64_t start_chunk = (uint64_t)world_rank * chunks_num / world_size;
    uint64_t end_chunk = (uint64_t)(world_rank + 1) * chunks_num / world_size;
    uint32_t chunks_per_process = (uint32_t)(end_chunk - start_chunk);
    DEBUG("Decompress: Rank %d processing %u chunks (Start: %lu, End: %lu)", world_rank, chunks_per_process, start_chunk, end_chunk);

    MPI_Offset chunk_size_table_start;
    MPI_File_get_position(in_fp, &chunk_size_table_start);

    int ec = EXIT_FAILURE;

    uint32_t* chunks_sizes = malloc(chunks_per_process * sizeof(uint32_t));
    if (chunks_per_process > 0 && !chunks_sizes) {
        DEBUG("Decompress: Failed to allocate chunks_sizes (NULL)");
        goto exit;
    }

    MPI_Offset relevant_chunk_sizes_start = chunk_size_table_start + sizeof(uint32_t) * start_chunk;
    if (MPI_File_read_at(in_fp, relevant_chunk_sizes_start, chunks_sizes, chunks_per_process, MPI_UINT32_T, MPI_STATUS_IGNORE) != MPI_SUCCESS) {
        DEBUG("Decompress: Failed to read chunk sizes at %lld", relevant_chunk_sizes_start);
        goto exit;
    }

    MPI_Offset total_process_chunk = 0;
    for (uint32_t i = 0; i < chunks_per_process; i++) {
        total_process_chunk += chunks_sizes[i];
    }
    DEBUG("Decompress: Local total compressed size to read: %lld", total_process_chunk);

    MPI_Offset read_offset = 0;
    if (MPI_Exscan(&total_process_chunk, &read_offset, 1, MPI_OFFSET, MPI_SUM, MPI_COMM_WORLD) != MPI_SUCCESS) {
        DEBUG("Decompress: Exscan failed");
        goto exit;
    }
    DEBUG("Decompress: Exscan finished. Local read_offset base: %lld", read_offset);

    read_offset += chunk_size_table_start + chunks_num * sizeof(uint32_t);
    DEBUG("Decompress: Final starting read_offset in file: %lld", read_offset);

    for (uint32_t i = 0; i < chunks_per_process; i++) {
        uint8_t* compressed_buffer = malloc(chunks_sizes[i]);
        if (!compressed_buffer) {
            DEBUG("Decompress: Failed to allocate compressed_buffer for chunk %u size %u", i, chunks_sizes[i]);
            goto exit;
        }

        if (MPI_File_read_at(in_fp, read_offset, compressed_buffer, chunks_sizes[i], MPI_BYTE, MPI_STATUS_IGNORE) != MPI_SUCCESS) {
            DEBUG("Decompress: Failed to read chunk %u at %lld", i, read_offset);
            free(compressed_buffer);
            goto exit;
        }

        uint8_t uncompressed_buffer[CHUNK_SIZE];

        uint32_t actual_size = decode(root, compressed_buffer, chunks_sizes[i], uncompressed_buffer);

        MPI_Offset write_offset = (start_chunk + i) * CHUNK_SIZE;
        DEBUG("Decompress: Writing uncompressed chunk %u (global idx %lu) of length %u to offset %lld", i, start_chunk + i, actual_size, write_offset);

        if (MPI_File_write_at(out_fp, write_offset, uncompressed_buffer, actual_size, MPI_BYTE, MPI_STATUS_IGNORE) != MPI_SUCCESS) {
            DEBUG("Decompress: Failed to write chunk at offset %lld", write_offset);
            free(compressed_buffer);
            goto exit;
        }

        read_offset += chunks_sizes[i];
        free(compressed_buffer);
    }

    DEBUG("Decompress: Routine finished successfully on rank %d", world_rank);
    ec = EXIT_SUCCESS;
exit:
    free(chunks_sizes);
    return ec;
}

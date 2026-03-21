#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "huffman-node.h"

/**
 * Generates a codebook from a Huffman tree.
 * @param cdb The codebook to populate (256 entries, each up to 256 chars).
 * @param curr_node The current node in the Huffman tree.
 * @param curr_code Buffer to store the current code being generated.
 * @param depth Current depth in the tree.
 */
void generate_codebook(char cdb[256][256], huffman_node* curr_node,
                       char curr_code[256], uint8_t depth);

/**
 * Creates a Huffman tree from a frequency table.
 * Handles the single-symbol case by adding a dummy node.
 * @param freqs The frequency table (256 entries).
 * @return The root of the Huffman tree.
 */
huffman_node* create_huffman_tree(uint32_t freqs[256]);

/**
 * Counts frequencies of symbols in a buffer.
 * Does NOT memset the freqs table, allowing for accumulation.
 * @param freqs The frequency table to update.
 * @param buf The buffer to count from.
 * @param size The size of the buffer.
 */
void count_freqs_buf(uint32_t freqs[256], const uint8_t* buf, uint32_t size);

#include "huffman-common.h"
#include "min-heap.h"
#include <stdlib.h>
#include <string.h>

void generate_codebook(char cdb[256][512], huffman_node* curr_node,
                       char curr_code[512], uint16_t depth) {
    if (curr_node->left == NULL && curr_node->right == NULL) {
        curr_code[depth] = '\0';
        strcpy(cdb[curr_node->symbol], curr_code);
        return;
    }

    if (curr_node->left != NULL) {
        curr_code[depth] = '0';
        generate_codebook(cdb, curr_node->left, curr_code, depth + 1);
    }
    if (curr_node->right != NULL) {
        curr_code[depth] = '1';
        generate_codebook(cdb, curr_node->right, curr_code, depth + 1);
    }
}

huffman_node* create_huffman_tree(uint32_t freqs[256]) {
    min_heap* mh = malloc(sizeof(min_heap));
    mh->len = 0;
    mh->cap = 32;
    mh->heap = NULL;

    for (int i = 0; i < 256; i++) {
        if (freqs[i] == 0) continue;

        huffman_node* node = malloc(sizeof(huffman_node));
        node->freq = freqs[i];
        node->symbol = (uint8_t)i;
        node->left = NULL;
        node->right = NULL;
        insert(mh, node);
    }

    // Handle single-symbol case by adding a dummy node
    if (mh->len == 1) {
        huffman_node* dummy = malloc(sizeof(huffman_node));
        dummy->freq = 0;
        dummy->symbol = (mh->heap[0]->symbol == 0) ? 1 : 0;
        dummy->left = NULL;
        dummy->right = NULL;
        insert(mh, dummy);
    }

    while (mh->len > 1) {
        huffman_node* left = pop(mh);
        huffman_node* right = pop(mh);

        huffman_node* parent = malloc(sizeof(huffman_node));
        parent->symbol = 0;
        parent->freq = left->freq + right->freq;
        parent->left = left;
        parent->right = right;
        insert(mh, parent);
    }

    huffman_node* root = (mh->len == 0) ? NULL : mh->heap[0];
    free(mh->heap);
    free(mh);
    return root;
}

void count_freqs_buf(uint32_t freqs[256], const uint8_t* buf, uint32_t size) {
    for (uint32_t i = 0; i < size; i++) {
        freqs[buf[i]]++;
    }
}

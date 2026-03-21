#include "min-heap.h"
#include <stdlib.h>
#include "huffman-node.h"

int parent(int index) {
    return (index - 1) / 2;
}

int left(int index) {
    return 2*index + 1;
}

int right(int index) {
    return 2*index + 2;
}

void insert(min_heap* ref, huffman_node* node) {
    if (ref->heap == NULL || ref->cap == 0) {
        ref->cap = ref->cap ? ref->cap : 32;
        ref->heap = malloc(sizeof(huffman_node) * ref->cap);
    }

    if (ref->cap == ref->len) {
        ref->heap = realloc(ref->heap, sizeof(huffman_node*) * ref->cap*2);
        ref->cap *= 2;
    }

    uint32_t new_idx = ref->len;
    ref->heap[new_idx] = node;
    ref->len++;

    if (ref->len == 1) {
        return;
    }

    int parent_idx = parent(new_idx);
    huffman_node* parent_node = ref->heap[parent_idx];
    while (node->freq < parent_node->freq) {
        huffman_node* tmp = NULL;

        tmp = ref->heap[new_idx];
        ref->heap[new_idx] = ref->heap[parent_idx];
        ref->heap[parent_idx] = tmp;

        if (parent_idx == 0) {
            return;
        }
        new_idx = parent_idx;
        parent_idx = parent(new_idx);
        parent_node = ref->heap[parent_idx];
    }
}

int select_min_child_idx(min_heap* ref, uint32_t parent_idx) {
    uint32_t left_child_idx = left(parent_idx);
    uint32_t right_child_idx = right(parent_idx);


    if (left_child_idx >= ref->len || ref->heap[left_child_idx] == NULL) {
        if (right_child_idx >= ref->len) {
            return -1;
        } else {
            return right_child_idx;
        }
    }

    if (right_child_idx >= ref->len || ref->heap[right_child_idx] == NULL) {
        return left_child_idx;
    }


    if (ref->heap[left_child_idx]->freq <= ref->heap[right_child_idx]->freq) {
        return left_child_idx;
    } else {
        return right_child_idx;
    }
}

huffman_node* pop(min_heap* ref) {
    if (ref->len == 0) {
        return NULL;
    }

    huffman_node* res = ref->heap[0];

    if (ref->len == 1) {
        ref->heap[0] = NULL;
        goto pop_exit;
    }

    int sinker_idx = 0;
    ref->heap[sinker_idx] = ref->heap[ref->len-1];
    ref->heap[ref->len-1] = NULL;

    int min_child_idx = select_min_child_idx(ref, sinker_idx);
    if (min_child_idx < 0) {
        goto pop_exit;
    }

    huffman_node* min_child = ref->heap[min_child_idx];

    while (ref->heap[sinker_idx]->freq > min_child->freq) {
        huffman_node* tmp = NULL;

        tmp = ref->heap[sinker_idx];
        ref->heap[sinker_idx] = ref->heap[min_child_idx];
        ref->heap[min_child_idx] = tmp;

        sinker_idx = min_child_idx;

        min_child_idx = select_min_child_idx(ref, sinker_idx);
        if (min_child_idx < 0) {
            goto pop_exit;
        }
        min_child = ref->heap[min_child_idx];
    }

pop_exit:
    ref->len--;
    return res;
}

#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct __attribute__((packed)) {
    uint32_t freq;
    uint8_t symbol;
} freq_entry;

typedef struct {
    uint32_t len;
    freq_entry* entries;
} freq_table;

void table_to_entries(freq_table* tbl, uint32_t freq[256]);
void entries_to_table(freq_table* tbl, uint32_t freq[256]);

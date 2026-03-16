#include "freq-table.h"

void table_to_entries(freq_table* tbl, uint32_t freq[256]) {
    uint32_t cap = 32;
    tbl->len = 0;
    tbl->entries = malloc(sizeof(freq_entry) * cap);
    for (int i = 0; i < 256; i++) {
        if (freq[i] == 0) continue;
        if (tbl->len == cap) {
            cap *= 2;
            tbl->entries = realloc(tbl->entries, sizeof(freq_entry) * cap);
        }
        tbl->entries[tbl->len++] = (freq_entry){ .freq = freq[i], .symbol = (uint8_t)i };
    }
}

void entries_to_table(freq_table* tbl, uint32_t freq[256]) {
    for (int i = 0; i < tbl->len; i++) {
        freq_entry ent = tbl->entries[i];
        freq[ent.symbol] = ent.freq;
    }
}

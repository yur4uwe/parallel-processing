# Huffman Project Structure & File Format

## Directory Layout
- `serial/`: Sequential implementation of Huffman coding.
- `parallel/`: MPI-based parallel implementation.
- `freq-table.h/c`: Common logic for frequency table management.
- `extract-freq-table.c`: Debugging tool for inspecting compressed file headers.

## Compressed File Format (.huff)
The compressed files follow this binary structure:

1. **Table Length (4 bytes):** `uint32_t` representing the number of unique symbols stored.
2. **Frequency Table:** A sequence of `freq_entry` structures.
   - Each entry is **5 bytes** (packed):
     - `uint32_t freq` (4 bytes)
     - `uint8_t symbol` (1 byte)
3. **Chunk Count (4 bytes):** `uint32_t` representing the number of independent compressed chunks.
4. **Chunk Size Table:** A sequence of `uint32_t` values (one per chunk) representing the **compressed size in bytes** of each chunk (including its padding byte).
5. **Chunks**: Sequential blocks of compressed data. Each chunk corresponds to **64 KB (65,536 bytes)** of original uncompressed data (except possibly the last chunk).
   - Each chunk starts with **1 byte** (`uint8_t`) indicating the number of padding bits (0-7) in the **last byte** of that chunk's bitstream.
   - This is followed by the bitstream itself.

## Key Data Structures
- **`freq_entry`**: A packed struct containing a 4-byte frequency and a 1-byte symbol.
- **`freq_table`**: A dynamic array of `freq_entry` used during serialization.

## Implementation Details
- **Chunking:** The input file is divided into fixed-size uncompressed chunks of **64 KB**.
- **Serial Implementation:** Uses standard C I/O, processing and writing chunks sequentially.
- **Parallel Implementation:** Uses MPI-IO. Workers process chunks in parallel. The "Chunk Size Table" allows ranks to jump directly to their assigned compressed chunks for decompression.
- **Padding:** Bitstreams are padded to the nearest byte at the end of each chunk. The padding count is stored at the beginning of each chunk to allow for independent decoding.

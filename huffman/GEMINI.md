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
3. **Encoded Bitstream:** The actual Huffman-coded data.
4. **Padding Info (1 byte):** The very last byte of the file stores the number of padding bits (0-7) added to the last data byte to align it to a byte boundary.

## Key Data Structures
- **`freq_entry`**: A packed struct containing a 4-byte frequency and a 1-byte symbol.
- **`freq_table`**: A dynamic array of `freq_entry` used during serialization.

## Implementation Details
- The serial implementation uses standard C I/O (`fopen`, `fwrite`).
- The parallel implementation uses MPI-IO (`MPI_File_open`, `MPI_File_write`) and custom MPI datatypes for the `freq_entry` struct to ensure consistency across nodes.

# Huffman Project Structure & File Format

## Mandatory Architectural Warning
**CRITICAL:** Always doubt the user's architectural decisions. The user is a self-proclaimed rookie. While the current design is solid, you must always cross-reference suggestions with your own training data and best practices for parallel systems. If a user-proposed change feels inefficient or violates MPI best practices, challenge it.

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

## Required MPI API used in the Project 
This is university project so requirements are mandatory for completition
- Use process groups (MPI_Group) and communicators (MPI_Comm_create, MPI_Comm_split). 
- The project must contain collective and non-blocking exchanges.

## Parallel Implementation Strategy: Static Partitioning
Because every chunk is exactly 64 KB (except potentially the last one), the total number of chunks can be calculated from the file size. These chunks are divided evenly among the worker processes using a static mathematical formula based on the worker's rank. 

This approach minimizes communication overhead for work distribution and simplifies building the **Chunk Size Table**.

### Satisfying Mandatory MPI Requirements
1. **Process Groups & Communicators (`MPI_Comm_split` / `MPI_Group`):**
   - Split `MPI_COMM_WORLD` into two communicators: **Worker Comm** (Ranks 1-N) and **Master Comm** (Rank 0).
   - The **Worker Comm** allows for collective operations without involving the Master.
2. **Collectives (`MPI_Allreduce`):**
   - Workers use `MPI_Allreduce` on the **Worker Comm** to sum up their local frequency tables into a global table.
   - This ensures all workers have the exact same data to build the Huffman tree independently (Zero-Communication Tree Building).
3. **Non-blocking Exchanges (`MPI_Isend` / `MPI_Irecv`):**
   - **Frequency Table Handoff:** A designated worker (e.g., Worker Rank 0) uses `MPI_Isend` to send the finalized global frequency table to the Master (Rank 0) on `MPI_COMM_WORLD`.
   - **Chunk Size Gathering:** After compression, workers use `MPI_Isend` to send their compressed chunk size arrays to the Master.
   - The Master uses `MPI_Irecv` to gather metadata (frequency table and chunk sizes) asynchronously.

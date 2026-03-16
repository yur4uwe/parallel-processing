.HUFF FILE STRUCTURE (VERTICAL BYTE-STREAM MAP)

[ BYTE OFFSET ]  [ FIELD NAME ]          [ SIZE / TYPE ]
---------------------------------------------------------
0000             TABLE_LEN               4 Bytes (uint32)
                 |-- Number of entries (N) in Frequency Table
                 |
0004             FREQ_ENTRIES            N * 5 Bytes
                 |-- [ uint32 freq   ]
                 |-- [ uint8  symbol ]
                 |-- ... (Repeated N times)
                 |
0004+(N*5)       CHUNK_COUNT             4 Bytes (uint32) <-- in encode/decode fp is here
                 |-- Number of compressed chunks (M)
                 |
0008+(N*5)       CHUNK_SIZE_TABLE        M * 4 Bytes
                 |-- [ uint32 chunk_1_size ]
                 |-- [ uint32 chunk_2_size ]
                 |-- ... (Repeated M times)
                 |-- Note: Sizes include the 1B padding header
                 |
START_OF_DATA    CHUNK_1                 Variable (ChunkSize_1)
                 |-- [ PADDING_BITS ]    1 Byte (uint8: 0-7)
                 |-- [ BITSTREAM    ]    (ChunkSize_1 - 1) Bytes
                 |
                 CHUNK_2                 Variable (ChunkSize_2)
                 |-- [ PADDING_BITS ]    1 Byte (uint8: 0-7)
                 |-- [ BITSTREAM    ]    (ChunkSize_2 - 1) Bytes
                 |
                 ...                     ...
                 |
EOF              CHUNK_M                 Variable (ChunkSize_M)
                 |-- [ PADDING_BITS ]    1 Byte (uint8: 0-7)
                 |-- [ BITSTREAM    ]    (ChunkSize_M - 1) Bytes

---------------------------------------------------------

DETAILED COMPONENT BREAKDOWN:

1. FREQ_ENTRY (5 Bytes Packed)
   [ Byte 0-3 ]: Frequency (Little Endian uint32)
   [ Byte 4   ]: ASCII/Raw Symbol (uint8)

2. DATA CHUNK (Variable Size)
   - First Byte: Number of bits in the VERY LAST BYTE of the bitstream
     that are NOT part of the data (padding).
   - Remaining Bytes: Huffman bitstream.
   - Example: If PADDING_BITS is 3, only 5 bits of the last byte are valid.

DECODING LOGIC (SERIAL):
1.  Read TABLE_LEN.
2.  Loop TABLE_LEN times: Read freq and symbol into freq_table.
3.  Build Huffman Tree from freq_table.
4.  Read CHUNK_COUNT (M).
5.  Read M uint32s into a sizes_array.
6.  For i = 0 to M-1:
    a. Read PADDING_BITS (1B).
    b. Read (sizes_array[i] - 1) bytes into a chunk_buffer.
    c. Calculate TotalBits = ((sizes_array[i] - 1) * 8) - PADDING_BITS.
    d. Traverse Tree bit-by-bit until TotalBits are processed.
    e. Write decoded symbols to output file.
    f. Reset tree pointer to ROOT for next chunk.

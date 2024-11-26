#include "cache.h"
#include <stdio.h>



// The cache is an array of CacheIndex
CacheIndex cache[NUM_INDEXES];

// Function to decompose a 32-bit address
CacheAddress decompose_address(unsigned int address) {
    CacheAddress parsed;
    parsed.byte_offset = address & 0x3F;           // 6 LSB bits (0b111111 or 0x3F)
    parsed.index = (address >> 6) & 0x3FFF;        // Next 14 bits (0x3FFF)
    parsed.tag = (address >> 20) & 0xFFF;          // Remaining 12 bits
    return parsed;
}

// Function to initialize the metadata for a cache line
CacheMetadata initialize_cache_metadata() {
    CacheMetadata metadata;
    metadata.valid = 0;        // Invalid by default
    metadata.dirty = 0;        // Clean by default
    metadata.state = INVALID;  // Start in Invalid state (MESI)
    return metadata;
}

// Function to initialize cache (all lines are invalid by default)
void initialize_cache() {
    int i,j;
    for (i = 0; i < NUM_INDEXES; i++) {
        for (j = 0; j < NUM_LINES_PER_INDEX; j++) {
            cache[i].lines[j].tag = 0; // Initialize the tag to 0
            cache[i].lines[j].metadata = initialize_cache_metadata(); // Initialize metadata
        }
        cache[i].pseudo_LRU = 0x7FFF; // Set all pseudo-LRU bits for the index (15 bits, 0x7FFF)
    }
}

// Function to get a string representation of the MESI state
const char *get_mesi_state_name(MESIState state) {
    switch (state) {
        case INVALID: return "Invalid";
        case MODIFIED: return "Modified";
        case EXCLUSIVE: return "Exclusive";
        case SHARED: return "Shared";
        default: return "Unknown";
    }
}

// Function to get operation name from the operation code
const char *get_operation_name(int code) {
    switch (code) {
        case 0: return "Read request from L1 data cache";
        case 1: return "Write request from L1 data cache";
        case 2: return "Read request from L1 instruction cache";
        case 3: return "Snooped read request";
        case 4: return "Snooped write request";
        case 5: return "Snooped read with intent to modify";
        case 6: return "Snooped invalidate command";
        case 8: return "Clear cache and reset state";
        case 9: return "Print contents and state of each valid cache line";
        default: return "Unknown operation";
    }
}


// Function to perform the read operation (Operation Code 0)
void handle_read_operation(TraceEntry *entry) {
    // Decompose the address into its components
    unsigned int index = entry->parsed_addr.index;
    unsigned int tag = entry->parsed_addr.tag;

    // Access the cache line at the given index
    CacheIndex *cache_index = &cache[index];

    bool hit = false;  // Flag to indicate whether it's a hit or miss
    unsigned int i;

    // Check each line in the index for a matching tag
    for (i = 0; i < NUM_LINES_PER_INDEX; i++) {
        CacheLine *cache_line = &cache_index->lines[i];

        if (cache_line->metadata.valid == 1 && cache_line->tag == tag) {
            // If the line is valid and the tag matches, it's a hit
            hit = true;
            break;
        }
    }

    // Output the result of the cache operation (hit or miss)
    if (hit) {
        printf("Cache Hit: Address 0x%08X (Index: 0x%X, Tag: 0x%X)\n",
               entry->address, index, tag);
    } else {
        printf("Cache Miss: Address 0x%08X (Index: 0x%X, Tag: 0x%X)\n",
               entry->address, index, tag);
    }
}


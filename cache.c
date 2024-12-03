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

// Function to initialize the PLRU tree for an index
void initialize_plru_tree(CacheIndex *index) {
    index->pseudo_LRU = 0; // Initialize all 15 bits to 0 (empty tree)
}

// Function to initialize cache (all lines are invalid by default)
void initialize_cache() {
    int i,j;
    for (i = 0; i < NUM_INDEXES; i++) {
        for (j = 0; j < NUM_LINES_PER_INDEX; j++) {
            cache[i].lines[j].tag = 0; // Initialize the tag to 0
            cache[i].lines[j].metadata = initialize_cache_metadata(); // Initialize metadata
        }
        initialize_plru_tree(&cache[i]); // Initialize the PLRU tree
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

// Function to update the PLRU tree after accessing a specific way (hit or insertion)
void update_plru_tree(unsigned short *pseudo_LRU, int way) {
    int node = 0; // Start at the root of the tree
    int depth = 0;
    for (; depth < 4; depth++) {
        int bit_index = node; // Current bit index in the PLRU array
        int direction = (way & (1 << (3 - depth))) ? 1 : 0; // Check the bit of 'way' (3 MSB)

        (*pseudo_LRU) &= ~(1 << bit_index); // Clear the bit
        (*pseudo_LRU) |= (direction << bit_index); // Set the bit to the current direction

        // Move to the next node in the tree
        node = 2 * node + 1 + direction;
    }
}

// Function to find the way to evict using the PLRU tree
int find_eviction_way(unsigned short pseudo_LRU) {
    int node = 0; // Start at the root of the tree
    int depth = 0;
    for (; depth < 4; depth++) {
        int bit_index = node; // Current bit index in the PLRU array
        int direction = (pseudo_LRU & (1 << bit_index)) ? 1 : 0; // Check the direction bit

        // Move to the next node in the tree
        node = 2 * node + 1 + direction;
    }

    return node - 15; // Subtract 15 to convert to the way number (leaf index)
}

void handle_read_operation(TraceEntry *entry) {
    unsigned int index = entry->parsed_addr.index;
    unsigned int tag = entry->parsed_addr.tag;

    CacheIndex *current_index = &cache[index];
    int hit = 0;
    int i;
    // Check for a hit
    for (i = 0; i < NUM_LINES_PER_INDEX; i++) {
        if (current_index->lines[i].metadata.valid && current_index->lines[i].tag == tag) {
            hit = 1;

            // Update the PLRU tree for this index (MRU update)
            update_plru_tree(&current_index->pseudo_LRU, i);

            printf("Cache Hit: Address 0x%08X (Index: 0x%X, Tag: 0x%X, Way: %d)\n",
                   entry->address, index, tag, i);
            break;
        }
    }

    if (!hit) {
        // Cache miss
        printf("Cache Miss: Address 0x%08X (Index: 0x%X, Tag: 0x%X)\n",
               entry->address, index, tag);

        // Find a way to evict using the PLRU tree
        int eviction_way = find_eviction_way(current_index->pseudo_LRU);

        // Evict the cache line
        printf("Evicting Way: %d (Tag: 0x%X)\n", eviction_way, current_index->lines[eviction_way].tag);
        current_index->lines[eviction_way].tag = tag;
        current_index->lines[eviction_way].metadata = initialize_cache_metadata();
        current_index->lines[eviction_way].metadata.valid = 1;

        // Update the PLRU tree after inserting the new tag
        update_plru_tree(&current_index->pseudo_LRU, eviction_way);
    }
}



#include "cache.h"
#include <stdio.h>

void print_summary() {
    // Example summary (replace with actual statistics)
    printf("Simulation Summary:\n");
    printf("  Total Reads: XX\n");
    printf("  Total Writes: XX\n");
    printf("  Cache Hits: XX\n");
    printf("  Cache Misses: XX\n");
}

// The cache is an array of CacheIndex
CacheIndex cache[NUM_INDEXES];
CacheIndex instruction_cache[NUM_INDEXES]; // Separate instruction cache if needed

extern int Mode; // 0 = silent, 1 = normal

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


const char *get_mesi_state_name(MESIState state) {
    switch (state) {
        case INVALID: return "INVALID";
        case MODIFIED: return "MODIFIED";
        case EXCLUSIVE: return "EXCLUSIVE";
        case SHARED: return "SHARED";
        default: return "UNKNOWN";
    }
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


        for (j = 0; j < NUM_LINES_PER_INDEX; j++) {
            instruction_cache[i].lines[j].tag = 0;
            instruction_cache[i].lines[j].metadata = initialize_cache_metadata();
        }
        initialize_plru_tree(&instruction_cache[i]);
    }
}

// Function to update the PLRU tree after accessing a specific way (hit or insertion)
void update_plru_tree(unsigned short *pseudo_LRU, int way) {
    int node = 0; // Start at the root of the tree

    int depth;
    for (depth = 0; depth < 4; depth++) {
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
    int depth;
    for (depth = 0; depth < 4; depth++) {
        int bit_index = node; // Current bit index in the PLRU array
        int direction = (pseudo_LRU & (1 << bit_index)) ? 1 : 0; // Check the direction bit

        // Move to the next node in the tree
        node = 2 * node + 1 + direction;
    }

    return node - 15; // Subtract 15 to convert to the way number (leaf index)
}


// Simulate the reporting of snoop results by other caches
int GetSnoopResult(unsigned int Address) {
    unsigned int byte_offset = Address & 0x3; // Extract 2 LSBs

    if (byte_offset == 0x0) {
        return NOHIT;
    } else if (byte_offset == 0x1) {
        return HIT;
    } else if (byte_offset == 0x2) {
        return HITM;
    }

    return NOHIT; // Default to NOHIT
}

void BusOperation(int BusOp, unsigned int Address, int *SnoopResult) {
    // Simulate snoop result
    *SnoopResult = GetSnoopResult(Address);

    if (Mode == 1) { // Only print in normal mode
        printf("Bus Communication:\n");
        printf("  Operation: %s\n", (BusOp == READ) ? "READ" :
                                         (BusOp == WRITE) ? "WRITE" :
                                         (BusOp == INVALIDATE) ? "INVALIDATE" :
                                         (BusOp == RWIM) ? "RWIM" : "UNKNOWN");

        printf("  Address: 0x%08X\n", Address);

        switch (BusOp) {
            case READ:
                if (*SnoopResult == HIT) {
                    printf("  BUS_READ: Fetched from another cache (Transition to SHARED).\n");
                } else if (*SnoopResult == HITM) {
                    printf("  BUS_READ: Data fetched from another cache in MODIFIED state (Transition to SHARED).\n");
                } else if (*SnoopResult == NOHIT) {
                    printf("  BUS_READ: Fetched from memory (Transition to EXCLUSIVE).\n");
                }
                break;

            case WRITE:
                if (*SnoopResult == HITM) {
                    printf("  BUS_WRITE: Data fetched and modified cache line in another cache (Transition to MODIFIED).\n");
                } else {
                    printf("  BUS_WRITE: Request to modify the cache line (RWIM).\n");
                }
                break;

            case INVALIDATE:
                printf("  BUS_INVALIDATE: Invalidate shared copies of the cache line.\n");
                break;

            case RWIM:
                printf("  BUS_RWIM: Fetch the line with intent to modify.\n");
                break;

            default:
                printf("  Unknown Bus Operation.\n");
                break;
        }

        printf("\n");
    }
}


// Report the result of our snooping bus operations
void PutSnoopResult(unsigned int Address, int SnoopResult) {
    if (Mode == 1) { // Normal mode
        printf("SnoopResult: Address: 0x%08X, SnoopResult: %d\n", Address, SnoopResult);
    }
}

// Simulate communication to our upper-level cache
void MessageToCache(int Message, unsigned int Address) {
    if (Mode == 1) { // Normal mode
        printf("L2 to L1 Message: %d, Address: 0x%08X\n", Message, Address);
    }
}

void handle_read_operation(TraceEntry *entry) {
    unsigned int index = entry->parsed_addr.index;
    unsigned int tag = entry->parsed_addr.tag;

    CacheIndex *current_index = &cache[index];
    int hit = -1; // Index of the hit line, -1 if miss

    // Check for a hit
    int i;
    for (i = 0; i < NUM_LINES_PER_INDEX; i++) {
        if (current_index->lines[i].metadata.valid && current_index->lines[i].tag == tag) {
            hit = i;
            break;
        }
    }

    if (hit != -1) {
        // Cache hit: Handle based on MESI state
        MESIState state = current_index->lines[hit].metadata.state;

        if (state == SHARED || state == EXCLUSIVE || state == MODIFIED) {
            // No bus communication for these states
            if (Mode == 1) {
                printf("Cache Hit: Address 0x%08X (Index: 0x%X, Tag: 0x%X, State: %s)\n",
                       entry->address, index, tag, get_mesi_state_name(state));
            }
        } else {
            printf("Error: Unexpected state on hit (address: 0x%08X, state: %s)\n",
                   entry->address, get_mesi_state_name(state));
        }

        // Update PLRU for this line
        update_plru_tree(&current_index->pseudo_LRU, hit);
    } else {
        // Cache miss: Perform bus read
        int snoop_result = NOHIT;
        BusOperation(READ, entry->address, &snoop_result);

        int eviction_way = find_eviction_way(current_index->pseudo_LRU);
        current_index->lines[eviction_way].tag = tag;
        current_index->lines[eviction_way].metadata.valid = 1;

        // Transition state based on snoop result
        MESIState new_state;
        if (snoop_result == HIT) {
            new_state = SHARED;
        } else if (snoop_result == HITM) {
            new_state = SHARED; // Data fetched from another cache in MODIFIED state
        } else {
            new_state = EXCLUSIVE;
        }
        current_index->lines[eviction_way].metadata.state = new_state;
        update_plru_tree(&current_index->pseudo_LRU, eviction_way);

        if (Mode == 1) { // Log only in normal mode
            printf("Cache Miss: Address 0x%08X (Index: 0x%X, Tag: 0x%X, New State: %s)\n",
                   entry->address, index, tag, get_mesi_state_name(new_state));
        }
    }
}
void handle_write_operation(TraceEntry *entry) {
    unsigned int index = entry->parsed_addr.index;
    unsigned int tag = entry->parsed_addr.tag;

    CacheIndex *cache_index = &cache[index];
    int hit = -1; // Index of the hit line, -1 if miss

    // Check for cache hit
    int i;
    for (i = 0; i < NUM_LINES_PER_INDEX; i++) {
        if (cache_index->lines[i].metadata.valid && cache_index->lines[i].tag == tag) {
            hit = i;

            break;
        }
    }

    if (hit != -1) {
        // Cache hit: Handle based on MESI state
        MESIState state = cache_index->lines[hit].metadata.state;

        if (state == SHARED) {
            // Shared -> Modified: Invalidate other caches
            int snoop_result = HIT;
            BusOperation(INVALIDATE, entry->address, &snoop_result);
            cache_index->lines[hit].metadata.state = MODIFIED;

            if (Mode == 1) {
                printf("Cache Hit (SHARED -> MODIFIED): Address 0x%08X (Index: 0x%X, Tag: 0x%X)\n",
                       entry->address, index, tag);
            }
        } else if (state == EXCLUSIVE || state == MODIFIED) {
            // Exclusive/Modified: No bus communication
            cache_index->lines[hit].metadata.state = MODIFIED;

            if (Mode == 1) {
                printf("Cache Hit (EXCLUSIVE/MODIFIED): Address 0x%08X (Index: 0x%X, Tag: 0x%X)\n",
                       entry->address, index, tag);
            }
        } else if (state == INVALID) {
            printf("Error: Invalid state on hit (address: 0x%08X, state: %s)\n",
                   entry->address, get_mesi_state_name(state));
        }

        cache_index->lines[hit].metadata.dirty = 1; // Mark as dirty
        update_plru_tree(&cache_index->pseudo_LRU, hit);
    } else {
        // Cache miss: Perform bus RWIM
        int snoop_result = NOHIT;
        BusOperation(RWIM, entry->address, &snoop_result);

        int eviction_way = find_eviction_way(cache_index->pseudo_LRU);
        cache_index->lines[eviction_way].tag = tag;
        cache_index->lines[eviction_way].metadata.valid = 1;
        cache_index->lines[eviction_way].metadata.state = MODIFIED;
        cache_index->lines[eviction_way].metadata.dirty = 1; // Mark as dirty
        update_plru_tree(&cache_index->pseudo_LRU, eviction_way);

        if (Mode == 1) { // Log only in normal mode
            printf("Cache Miss (RWIM): Address 0x%08X (Index: 0x%X, Tag: 0x%X, State: MODIFIED)\n",
                   entry->address, index, tag);
        }
    }
}

void handle_instruction_cache_read(TraceEntry *entry) {
    unsigned int index = entry->parsed_addr.index;
    unsigned int tag = entry->parsed_addr.tag;

    CacheIndex *current_index = &cache[index]; // Using the same cache structure for simplicity
    int hit = -1; // Index of the hit line, -1 if miss

    // Log the operation in normal mode
    if (Mode == 1) {
        printf("Instruction Cache Read Operation: Address 0x%08X (Index: 0x%X, Tag: 0x%X)\n", 
               entry->address, index, tag);
    }

    // Check for a hit
    int i;
    for (i = 0; i < NUM_LINES_PER_INDEX; i++) {
        if (current_index->lines[i].metadata.valid && current_index->lines[i].tag == tag) {
            hit = i;
            break;
        }
    }

    if (hit != -1) {
        // Cache hit: Handle based on MESI state
        MESIState state = current_index->lines[hit].metadata.state;

        if (state == SHARED || state == EXCLUSIVE || state == MODIFIED) {
            // No bus communication for valid states
            if (Mode == 1) {
                printf("Instruction Cache Hit: Address 0x%08X (Index: 0x%X, Tag: 0x%X, State: %s)\n",
                       entry->address, index, tag, get_mesi_state_name(state));
            }
        } else {
            printf("Error: Unexpected state on hit (address: 0x%08X, state: %s)\n",
                   entry->address, get_mesi_state_name(state));
        }

        // Update PLRU for this line
        update_plru_tree(&current_index->pseudo_LRU, hit);
    } else {
        // Cache miss: Perform bus read
        int snoop_result = NOHIT;
        BusOperation(READ, entry->address, &snoop_result);

        int eviction_way = find_eviction_way(current_index->pseudo_LRU);
        current_index->lines[eviction_way].tag = tag;
        current_index->lines[eviction_way].metadata.valid = 1;

        // Transition state based on snoop result
        MESIState new_state;
        if (snoop_result == HIT) {
            new_state = SHARED;
        } else if (snoop_result == HITM) {
            new_state = SHARED; // Data fetched from another cache in MODIFIED state
        } else {
            new_state = EXCLUSIVE;
        }
        current_index->lines[eviction_way].metadata.state = new_state;
        update_plru_tree(&current_index->pseudo_LRU, eviction_way);

        if (Mode == 1) { // Log only in normal mode
            printf("Instruction Cache Miss: Address 0x%08X (Index: 0x%X, Tag: 0x%X, New State: %s)\n",
                   entry->address, index, tag, get_mesi_state_name(new_state));
        }
    }
}

void handle_snooped_read_request(TraceEntry *entry) {
    unsigned int index = entry->parsed_addr.index;
    unsigned int tag = entry->parsed_addr.tag;

    CacheIndex *current_index = &cache[index];
    int line_found = -1; // Index of the matching line, -1 if not found

    if (Mode == 1) {
        printf("Snooped Read Request: Address 0x%08X (Index: 0x%X, Tag: 0x%X)\n", 
               entry->address, index, tag);
    }
    int i;
    for (i = 0; i < NUM_LINES_PER_INDEX; i++) {
        if (current_index->lines[i].metadata.valid && current_index->lines[i].tag == tag) {
            line_found = i;
            break;
        }
    }

    if (line_found != -1) {
        CacheLine *line = &current_index->lines[line_found];
        MESIState state = line->metadata.state;

        if (state == MODIFIED) {
            line->metadata.state = SHARED;

            if (Mode == 1) {
                printf("Snooped Read: MODIFIED -> SHARED (Write-back to memory).\n");
            }
        } else if (state == EXCLUSIVE) {
            line->metadata.state = SHARED;

            if (Mode == 1) {
                printf("Snooped Read: EXCLUSIVE -> SHARED.\n");
            }
        } else if (state == SHARED) {
            if (Mode == 1) {
                printf("Snooped Read: Already in SHARED state. No action needed.\n");
            }
        } else if (state == INVALID) {
            if (Mode == 1) {
                printf("Snooped Read: Line in INVALID state. No action needed.\n");
            }
        }
    } else if (Mode == 1) {
        printf("Snooped Read: Line not present in cache. No action needed.\n");
    }
}
void handle_snooped_write_request(TraceEntry *entry) {
    unsigned int index = entry->parsed_addr.index;
    unsigned int tag = entry->parsed_addr.tag;

    CacheIndex *current_index = &cache[index];
    int line_found = -1; // Index of the matching line, -1 if not found

    if (Mode == 1) {
        printf("Snooped Write Request: Address 0x%08X (Index: 0x%X, Tag: 0x%X)\n", 
               entry->address, index, tag);
    }
    int i;
    for (i = 0; i < NUM_LINES_PER_INDEX; i++) {
        if (current_index->lines[i].metadata.valid && current_index->lines[i].tag == tag) {
            line_found = i;
            break;
        }
    }

    if (line_found != -1) {
        CacheLine *line = &current_index->lines[line_found];
        MESIState state = line->metadata.state;

        if (state == MODIFIED) {
            line->metadata.state = INVALID;

            if (Mode == 1) {
                printf("Snooped Write: MODIFIED -> INVALID (Write-back to memory).\n");
            }
        } else if (state == SHARED) {
            line->metadata.state = INVALID;

            if (Mode == 1) {
                printf("Snooped Write: SHARED -> INVALID.\n");
            }
        } else if (state == INVALID) {
            if (Mode == 1) {
                printf("Snooped Write: Line already in INVALID state. No action needed.\n");
            }
        } else if (state == EXCLUSIVE) {
            line->metadata.state = INVALID;

            if (Mode == 1) {
                printf("Snooped Write: EXCLUSIVE -> INVALID.\n");
            }
        }
    } else if (Mode == 1) {
        printf("Snooped Write: Line not present in cache. No action needed.\n");
    }
}

void handle_snooped_rwim_request(TraceEntry *entry) {
    unsigned int index = entry->parsed_addr.index;
    unsigned int tag = entry->parsed_addr.tag;

    CacheIndex *current_index = &cache[index];
    int line_found = -1; // Index of the matching line, -1 if not found

    if (Mode == 1) {
        printf("Snooped RWIM Request: Address 0x%08X (Index: 0x%X, Tag: 0x%X)\n", 
               entry->address, index, tag);
    }
    int i;
    for (i = 0; i < NUM_LINES_PER_INDEX; i++) {
        if (current_index->lines[i].metadata.valid && current_index->lines[i].tag == tag) {
            line_found = i;
            break;
        }
    }

    if (line_found != -1) {
        CacheLine *line = &current_index->lines[line_found];
        MESIState state = line->metadata.state;

        if (state == MODIFIED) {
            line->metadata.state = INVALID;

            if (Mode == 1) {
                printf("Snooped RWIM: MODIFIED -> INVALID (Write-back to memory).\n");
            }
        } else if (state == SHARED || state == EXCLUSIVE) {
            line->metadata.state = INVALID;

            if (Mode == 1) {
                printf("Snooped RWIM: SHARED/EXCLUSIVE -> INVALID.\n");
            }
        } else if (state == INVALID) {
            if (Mode == 1) {
                printf("Snooped RWIM: Line already in INVALID state. No action needed.\n");
            }
        }
    } else if (Mode == 1) {
        printf("Snooped RWIM: Line not present in cache. No action needed.\n");
    }
}



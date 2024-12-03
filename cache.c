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
    }
}

void invalidate_cache_line(CacheLine *line) {
    line->tag = 0;                      // Clear the tag
    line->metadata.valid = 0;           // Mark as invalid
    line->metadata.dirty = 0;           // Clear the dirty bit
    line->metadata.state = INVALID;     // Set the state to INVALID
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
    const char *message_type = NULL;

    switch (Message) {
        case GETLINE:
            message_type = "GETLINE"; // L2 requests data from L1
            break;
        case SENDLINE:
            message_type = "SENDLINE"; // L2 sends data to L1
            break;
        case INVALIDATELINE:
            message_type = "INVALIDATELINE"; // L2 invalidates L1's cache line
            break;
        case EVICTLINE:
            message_type = "EVICTLINE"; // L2 evicts a line from L1
            break;
        default:
            message_type = "UNKNOWN"; // For unexpected message types
            break;
    }

    if (Mode == 1) { // Print messages only in normal mode
        printf("L2 to L1 Message: %s, Address: 0x%08X\n", message_type, Address);
    }
    fprintf(output_file, "L2 to L1 Message: %s, Address: 0x%08X\n", message_type, Address);
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
            // Valid states do not require invalidation
            if (Mode == 1) {
                printf("Cache Hit: Address 0x%08X (Index: 0x%X, Tag: 0x%X, State: %s)\n",
                       entry->address, index, tag, get_mesi_state_name(state));
            }
            fprintf(output_file,
                    "Cache Hit: Address 0x%08X (Index: 0x%X, Tag: 0x%X, State: %s)\n",
                    entry->address, index, tag, get_mesi_state_name(state));
        } else {
            // Handle unexpected states
            printf("Error: Unexpected state on hit (address: 0x%08X, state: %s)\n",
                   entry->address, get_mesi_state_name(state));
        }

        // Update PLRU for this line
        update_plru_tree(&current_index->pseudo_LRU, hit);
    } else {
        // Cache miss: Perform bus read
        int snoop_result = NOHIT;
        BusOperation(READ, entry->address, &snoop_result);

        // Find a way to evict using PLRU
        int eviction_way = find_eviction_way(current_index->pseudo_LRU);

        // Invalidate the line being evicted
        invalidate_cache_line(&current_index->lines[eviction_way]);

        // Insert the new tag and update the line's state
        current_index->lines[eviction_way].tag = tag;
        current_index->lines[eviction_way].metadata.valid = 1;

        MESIState new_state;
        if (snoop_result == HIT) {
            new_state = SHARED;
        } else if (snoop_result == HITM) {
            new_state = SHARED; // Data fetched from another cache in MODIFIED state
        } else {
            new_state = EXCLUSIVE;
        }
        current_index->lines[eviction_way].metadata.state = new_state;

        // Update PLRU after inserting the new tag
        update_plru_tree(&current_index->pseudo_LRU, eviction_way);

        if (Mode == 1) {
            printf("Cache Miss: Address 0x%08X (Index: 0x%X, Tag: 0x%X, New State: %s)\n",
                   entry->address, index, tag, get_mesi_state_name(new_state));
        }
        fprintf(output_file,
                "Cache Miss: Address 0x%08X (Index: 0x%X, Tag: 0x%X, New State: %s)\n",
                entry->address, index, tag, get_mesi_state_name(new_state));
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

            fprintf(output_file,
                    "Operation: Write request from L1 data cache (code 1), Address: 0x%08X\n"
                    "  Decomposed Address: Byte Offset=0x%X, Index=0x%X, Tag=0x%X\n"
                    "  Metadata: Valid=%d, Dirty=%d, MESI State=MODIFIED\n"
                    "  Index Pseudo-LRU: 0x%X\n",
                    entry->address, entry->parsed_addr.byte_offset, index, tag,
                    cache_index->lines[hit].metadata.valid,
                    cache_index->lines[hit].metadata.dirty,
                    "MODIFIED", cache_index->pseudo_LRU);

        } else if (state == EXCLUSIVE || state == MODIFIED) {
            // Exclusive/Modified: No bus communication
            cache_index->lines[hit].metadata.state = MODIFIED;

            if (Mode == 1) {
                printf("Cache Hit (EXCLUSIVE/MODIFIED): Address 0x%08X (Index: 0x%X, Tag: 0x%X)\n",
                       entry->address, index, tag);
            }

            fprintf(output_file,
                    "Operation: Write request from L1 data cache (code 1), Address: 0x%08X\n"
                    "  Decomposed Address: Byte Offset=0x%X, Index=0x%X, Tag=0x%X\n"
                    "  Metadata: Valid=%d, Dirty=%d, MESI State=MODIFIED\n"
                    "  Index Pseudo-LRU: 0x%X\n",
                    entry->address, entry->parsed_addr.byte_offset, index, tag,
                    cache_index->lines[hit].metadata.valid,
                    cache_index->lines[hit].metadata.dirty,
                    "MODIFIED", cache_index->pseudo_LRU);

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
        invalidate_cache_line(&cache_index->lines[eviction_way]);
        cache_index->lines[eviction_way].tag = tag;
        cache_index->lines[eviction_way].metadata.valid = 1;
        cache_index->lines[eviction_way].metadata.state = MODIFIED;
        cache_index->lines[eviction_way].metadata.dirty = 1; // Mark as dirty
        update_plru_tree(&cache_index->pseudo_LRU, eviction_way);

        if (Mode == 1) {
            printf("Cache Miss (RWIM): Address 0x%08X (Index: 0x%X, Tag: 0x%X, State: MODIFIED)\n",
                   entry->address, index, tag);
        }

        fprintf(output_file,
                "Operation: Write request from L1 data cache (code 1), Address: 0x%08X\n"
                "  Decomposed Address: Byte Offset=0x%X, Index=0x%X, Tag=0x%X\n"
                "  Metadata: Valid=%d, Dirty=%d, MESI State=MODIFIED\n"
                "  Index Pseudo-LRU: 0x%X\n",
                entry->address, entry->parsed_addr.byte_offset, index, tag,
                cache_index->lines[eviction_way].metadata.valid,
                cache_index->lines[eviction_way].metadata.dirty,
                "MODIFIED", cache_index->pseudo_LRU);
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

    fprintf(output_file,
            "Operation: Read request from L1 instruction cache (code 2), Address: 0x%08X\n"
            "  Decomposed Address: Byte Offset=0x%X, Index=0x%X, Tag=0x%X\n",
            entry->address, entry->parsed_addr.byte_offset, index, tag);

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

            fprintf(output_file,
                    "  Cache Hit: Metadata: Valid=%d, Dirty=%d, MESI State=%s\n"
                    "  Index Pseudo-LRU: 0x%X\n",
                    current_index->lines[hit].metadata.valid,
                    current_index->lines[hit].metadata.dirty,
                    get_mesi_state_name(state),
                    current_index->pseudo_LRU);
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
        invalidate_cache_line(&current_index->lines[eviction_way]);
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

        // Log the miss in normal mode
        if (Mode == 1) {
            printf("Instruction Cache Miss: Address 0x%08X (Index: 0x%X, Tag: 0x%X, New State: %s)\n",
                   entry->address, index, tag, get_mesi_state_name(new_state));
        }

        // Log cache miss details to output file
        fprintf(output_file,
                "  Cache Miss: Metadata: Valid=%d, Dirty=%d, MESI State=%s\n"
                "  Index Pseudo-LRU: 0x%X\n",
                current_index->lines[eviction_way].metadata.valid,
                current_index->lines[eviction_way].metadata.dirty,
                get_mesi_state_name(new_state),
                current_index->pseudo_LRU);
    }
}

void handle_snooped_read_request(TraceEntry *entry) {
    unsigned int index = entry->parsed_addr.index;
    unsigned int tag = entry->parsed_addr.tag;

    CacheIndex *current_index = &cache[index];
    int line_found = -1; // Index of the matching line, -1 if not found

    // Log the snooped read request in both modes
    if (Mode == 1) {
        printf("Snooped Read Request: Address 0x%08X (Index: 0x%X, Tag: 0x%X)\n", 
               entry->address, index, tag);
    }
    fprintf(output_file,
            "Operation: Snooped read request (code 3), Address: 0x%08X\n"
            "  Decomposed Address: Byte Offset=0x%X, Index=0x%X, Tag=0x%X\n",
            entry->address, entry->parsed_addr.byte_offset, index, tag);

    // Search for the matching cache line
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

            fprintf(output_file,
                    "  Snoop Result: MODIFIED -> SHARED (Write-back to memory).\n"
                    "  Metadata: Valid=%d, Dirty=%d, MESI State=%s\n"
                    "  Index Pseudo-LRU: 0x%X\n",
                    line->metadata.valid, line->metadata.dirty,
                    get_mesi_state_name(line->metadata.state),
                    current_index->pseudo_LRU);

        } else if (state == EXCLUSIVE) {
            line->metadata.state = SHARED;

            if (Mode == 1) {
                printf("Snooped Read: EXCLUSIVE -> SHARED.\n");
            }

            fprintf(output_file,
                    "  Snoop Result: EXCLUSIVE -> SHARED.\n"
                    "  Metadata: Valid=%d, Dirty=%d, MESI State=%s\n"
                    "  Index Pseudo-LRU: 0x%X\n",
                    line->metadata.valid, line->metadata.dirty,
                    get_mesi_state_name(line->metadata.state),
                    current_index->pseudo_LRU);

        } else if (state == SHARED) {
            if (Mode == 1) {
                printf("Snooped Read: Already in SHARED state. No action needed.\n");
            }

            fprintf(output_file,
                    "  Snoop Result: Already in SHARED state. No action needed.\n"
                    "  Metadata: Valid=%d, Dirty=%d, MESI State=%s\n"
                    "  Index Pseudo-LRU: 0x%X\n",
                    line->metadata.valid, line->metadata.dirty,
                    get_mesi_state_name(line->metadata.state),
                    current_index->pseudo_LRU);

        } else if (state == INVALID) {
            // Invalidate cache line when transitioning to INVALID state
            invalidate_cache_line(line);
            if (Mode == 1) {
                printf("Snooped Read: Line in INVALID state. No action needed.\n");
            }

            fprintf(output_file,
                    "  Snoop Result: Line in INVALID state. Invalidating line.\n"
                    "  Metadata: Valid=%d, Dirty=%d, MESI State=%s\n"
                    "  Index Pseudo-LRU: 0x%X\n",
                    line->metadata.valid, line->metadata.dirty,
                    get_mesi_state_name(line->metadata.state),
                    current_index->pseudo_LRU);
        }
    } else {
        // Line not present in cache
        if (Mode == 1) {
            printf("Snooped Read: Line not present in cache. No action needed.\n");
        }

        fprintf(output_file,
                "  Snoop Result: Line not present in cache. No action needed.\n");
    }
}

void handle_snooped_write_request(TraceEntry *entry) {
    unsigned int index = entry->parsed_addr.index;
    unsigned int tag = entry->parsed_addr.tag;

    CacheIndex *current_index = &cache[index];
    int line_found = -1; // Index of the matching line, -1 if not found

    // Log the snooped write request in both modes
    if (Mode == 1) {
        printf("Snooped Write Request: Address 0x%08X (Index: 0x%X, Tag: 0x%X)\n", 
               entry->address, index, tag);
    }
    fprintf(output_file,
            "Operation: Snooped write request (code 4), Address: 0x%08X\n"
            "  Decomposed Address: Byte Offset=0x%X, Index=0x%X, Tag=0x%X\n",
            entry->address, entry->parsed_addr.byte_offset, index, tag);

    // Search for the matching cache line
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
            // Transition MODIFIED -> INVALID and write back to memory
            invalidate_cache_line(line); // Invalidate the line
            if (Mode == 1) {
                printf("Snooped Write: MODIFIED -> INVALID (Write-back to memory).\n");
            }
            fprintf(output_file,
                    "  Snoop Result: MODIFIED -> INVALID (Write-back to memory).\n"
                    "  Metadata: Valid=%d, Dirty=%d, MESI State=%s\n"
                    "  Index Pseudo-LRU: 0x%X\n",
                    line->metadata.valid, line->metadata.dirty,
                    get_mesi_state_name(INVALID),
                    current_index->pseudo_LRU);

        } else if (state == SHARED) {
            // Transition SHARED -> INVALID
            invalidate_cache_line(line); // Invalidate the line
            if (Mode == 1) {
                printf("Snooped Write: SHARED -> INVALID.\n");
            }
            fprintf(output_file,
                    "  Snoop Result: SHARED -> INVALID.\n"
                    "  Metadata: Valid=%d, Dirty=%d, MESI State=%s\n"
                    "  Index Pseudo-LRU: 0x%X\n",
                    line->metadata.valid, line->metadata.dirty,
                    get_mesi_state_name(INVALID),
                    current_index->pseudo_LRU);

        } else if (state == INVALID) {
            // Line already in INVALID state
            if (Mode == 1) {
                printf("Snooped Write: Line already in INVALID state. No action needed.\n");
            }
            fprintf(output_file,
                    "  Snoop Result: Line already in INVALID state. No action needed.\n"
                    "  Metadata: Valid=%d, Dirty=%d, MESI State=%s\n"
                    "  Index Pseudo-LRU: 0x%X\n",
                    line->metadata.valid, line->metadata.dirty,
                    get_mesi_state_name(INVALID),
                    current_index->pseudo_LRU);

        } else if (state == EXCLUSIVE) {
            // Transition EXCLUSIVE -> INVALID
            invalidate_cache_line(line); // Invalidate the line
            if (Mode == 1) {
                printf("Snooped Write: EXCLUSIVE -> INVALID.\n");
            }
            fprintf(output_file,
                    "  Snoop Result: EXCLUSIVE -> INVALID.\n"
                    "  Metadata: Valid=%d, Dirty=%d, MESI State=%s\n"
                    "  Index Pseudo-LRU: 0x%X\n",
                    line->metadata.valid, line->metadata.dirty,
                    get_mesi_state_name(INVALID),
                    current_index->pseudo_LRU);
        }
    } else {
        // Line not present in cache
        if (Mode == 1) {
            printf("Snooped Write: Line not present in cache. No action needed.\n");
        }
        fprintf(output_file,
                "  Snoop Result: Line not present in cache. No action needed.\n");
    }
}

void handle_snooped_rwim_request(TraceEntry *entry) {
    unsigned int index = entry->parsed_addr.index;
    unsigned int tag = entry->parsed_addr.tag;

    CacheIndex *current_index = &cache[index];
    int line_found = -1; // Index of the matching line, -1 if not found

    // Log the snooped RWIM request in both modes
    if (Mode == 1) {
        printf("Snooped RWIM Request: Address 0x%08X (Index: 0x%X, Tag: 0x%X)\n", 
               entry->address, index, tag);
    }
    fprintf(output_file,
            "Operation: Snooped RWIM request (code 5), Address: 0x%08X\n"
            "  Decomposed Address: Byte Offset=0x%X, Index=0x%X, Tag=0x%X\n",
            entry->address, entry->parsed_addr.byte_offset, index, tag);

    // Search for the matching cache line
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
            // Transition MODIFIED -> INVALID and write back to memory
            invalidate_cache_line(line); // Invalidate the line
            if (Mode == 1) {
                printf("Snooped RWIM: MODIFIED -> INVALID (Write-back to memory).\n");
            }
            fprintf(output_file,
                    "  Snoop Result: MODIFIED -> INVALID (Write-back to memory).\n"
                    "  Metadata: Valid=%d, Dirty=%d, MESI State=%s\n"
                    "  Index Pseudo-LRU: 0x%X\n",
                    line->metadata.valid, line->metadata.dirty,
                    get_mesi_state_name(INVALID),
                    current_index->pseudo_LRU);

        } else if (state == SHARED || state == EXCLUSIVE) {
            // Transition SHARED/EXCLUSIVE -> INVALID
            invalidate_cache_line(line); // Invalidate the line
            if (Mode == 1) {
                printf("Snooped RWIM: SHARED/EXCLUSIVE -> INVALID.\n");
            }
            fprintf(output_file,
                    "  Snoop Result: SHARED/EXCLUSIVE -> INVALID.\n"
                    "  Metadata: Valid=%d, Dirty=%d, MESI State=%s\n"
                    "  Index Pseudo-LRU: 0x%X\n",
                    line->metadata.valid, line->metadata.dirty,
                    get_mesi_state_name(INVALID),
                    current_index->pseudo_LRU);

        } else if (state == INVALID) {
            // Line already in INVALID state
            if (Mode == 1) {
                printf("Snooped RWIM: Line already in INVALID state. No action needed.\n");
            }
            fprintf(output_file,
                    "  Snoop Result: Line already in INVALID state. No action needed.\n"
                    "  Metadata: Valid=%d, Dirty=%d, MESI State=%s\n"
                    "  Index Pseudo-LRU: 0x%X\n",
                    line->metadata.valid, line->metadata.dirty,
                    get_mesi_state_name(INVALID),
                    current_index->pseudo_LRU);
        }
    } else {
        // Line not present in cache
        if (Mode == 1) {
            printf("Snooped RWIM: Line not present in cache. No action needed.\n");
        }
        fprintf(output_file,
                "  Snoop Result: Line not present in cache. No action needed.\n");
    }
}

void handle_snooped_invalidate_command(TraceEntry *entry) {
    unsigned int index = entry->parsed_addr.index;
    unsigned int tag = entry->parsed_addr.tag;

    CacheIndex *current_index = &cache[index];
    int line_found = -1; // Index of the matching line, -1 if not found

    // Log the snooped invalidate request in both modes
    if (Mode == 1) {
        printf("Snooped Invalidate Request: Address 0x%08X (Index: 0x%X, Tag: 0x%X)\n", 
               entry->address, index, tag);
    }
    fprintf(output_file,
            "Operation: Snooped invalidate command (code 6), Address: 0x%08X\n"
            "  Decomposed Address: Byte Offset=0x%X, Index=0x%X, Tag=0x%X\n",
            entry->address, entry->parsed_addr.byte_offset, index, tag);

    // Search for the matching cache line
    int i;
    for (i = 0; i < NUM_LINES_PER_INDEX; i++) {
        if (current_index->lines[i].metadata.valid && current_index->lines[i].tag == tag) {
            line_found = i;
            break;
        }
    }

    if (line_found != -1) {
        // Line is present in the cache
        CacheLine *line = &current_index->lines[line_found];
        MESIState state = line->metadata.state;

        if (state == SHARED) {
            // SHARED -> INVALID: Invalidate the line
            invalidate_cache_line(line); // Properly invalidate the line and update PLRU
            if (Mode == 1) {
                printf("Snooped Invalidate: SHARED -> INVALID.\n");
            }
            fprintf(output_file,
                    "  Snoop Result: SHARED -> INVALID.\n"
                    "  Metadata: Valid=%d, Dirty=%d, MESI State=%s\n"
                    "  Index Pseudo-LRU: 0x%X\n",
                    line->metadata.valid, line->metadata.dirty,
                    get_mesi_state_name(INVALID),
                    current_index->pseudo_LRU);

        } else if (state == INVALID) {
            // INVALID: No action needed
            if (Mode == 1) {
                printf("Snooped Invalidate: Line already in INVALID state. No action needed.\n");
            }
            fprintf(output_file,
                    "  Snoop Result: Line already in INVALID state. No action needed.\n"
                    "  Metadata: Valid=%d, Dirty=%d, MESI State=%s\n"
                    "  Index Pseudo-LRU: 0x%X\n",
                    line->metadata.valid, line->metadata.dirty,
                    get_mesi_state_name(INVALID),
                    current_index->pseudo_LRU);

        } else if (state == MODIFIED || state == EXCLUSIVE) {
            // Error: Invalid scenario for snooped invalidate in MODIFIED or EXCLUSIVE state
            if (Mode == 1) {
                printf("Error: Snooped Invalidate: Line in %s state (Invalid scenario).\n",
                       get_mesi_state_name(state));
            }
            fprintf(output_file,
                    "  Error: Invalid scenario. Line in %s state.\n"
                    "  Metadata: Valid=%d, Dirty=%d, MESI State=%s\n"
                    "  Index Pseudo-LRU: 0x%X\n",
                    get_mesi_state_name(state),
                    line->metadata.valid, line->metadata.dirty,
                    get_mesi_state_name(line->metadata.state),
                    current_index->pseudo_LRU);
        }
    } else {
        // Line not present in cache
        if (Mode == 1) {
            printf("Snooped Invalidate: Line not present in cache. No action needed.\n");
        }
        fprintf(output_file,
                "  Snoop Result: Line not present in cache. No action needed.\n");
    }
}

void handle_clear_cache_request() {
    if (Mode == 1) {
        printf("Clearing cache and resetting all states to initial values...\n");
    }

    fprintf(output_file, "Operation: Clear cache (code 8)\n");

    // Iterate over all cache indexes and lines
    int i,j;
    for (i= 0; i < NUM_INDEXES; i++) {
        for (j = 0; j < NUM_LINES_PER_INDEX; j++) {
            cache[i].lines[j].tag = 0;                            // Clear the tag
            cache[i].lines[j].metadata.valid = 0;                 // Mark as invalid
            cache[i].lines[j].metadata.dirty = 0;                 // Clear the dirty bit
            cache[i].lines[j].metadata.state = INVALID;           // Reset state to INVALID
        }
        cache[i].pseudo_LRU = 0;                                  // Reset PLRU to all 0s
    }

    if (Mode == 1) {
        printf("Cache successfully cleared.\n");
    }

    fprintf(output_file, "Cache successfully cleared and reset to initial values.\n\n");
}

void handle_print_cache_state_request() {
    printf("Cache Contents and States:\n");
    fprintf(output_file, "Operation: Print cache state (code 9)\n");

    int i;
    for (i = 0; i < NUM_INDEXES; i++) {
        int has_valid_lines = 0;

        // Check if there are any valid lines in the current index
        int j;
        for (j = 0; j < NUM_LINES_PER_INDEX; j++) {
            if (cache[i].lines[j].metadata.valid) {
                has_valid_lines = 1;
                break;
            }
        }

        if (!has_valid_lines) {
            continue; // Skip this index if no valid lines are present
        }

        printf("Index %d:\n", i);
        fprintf(output_file, "Index %d:\n", i);

        for (j = 0; j < NUM_LINES_PER_INDEX; j++) {
            CacheLine *line = &cache[i].lines[j];
            if (line->metadata.valid) {
                printf("  Line %d: Tag=0x%X, State=%s, Dirty=%d\n",
                       j, line->tag, get_mesi_state_name(line->metadata.state), line->metadata.dirty);

                fprintf(output_file,
                        "  Line %d: Tag=0x%X, State=%s, Dirty=%d\n",
                        j, line->tag, get_mesi_state_name(line->metadata.state), line->metadata.dirty);
            }
        }
    }

    printf("Cache state printed successfully.\n");
    fprintf(output_file, "Cache state printed successfully.\n\n");
}


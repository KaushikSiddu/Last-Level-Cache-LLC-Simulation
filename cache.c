#include "cache.h"
#include <stdio.h>
#include <math.h>


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
     int i;
     for (i = 0; i < NUM_LINES_PER_INDEX - 1; i++) {
        index->pseudo_LRU[i] = 0;  // Set each bit to 0 (empty tree)
    }
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
void update_plru_tree(unsigned char pseudo_LRU[], int w) {
    int depth = log2(NUM_LINES_PER_INDEX); // Depth of the PLRU tree
    int index = 0;
    int level,i;

    // Traverse the tree from the root to the leaf level
    for (level = 0; level < depth; level++) {
        // Determine the direction based on the way (w) and level
        int direction = (w >> (depth - level - 1)) & 1;
        
        // Set the PLRU bit for this level
        pseudo_LRU[index] = direction;
        
        // Move to the next level (left or right child)
        index = 2 * index + 1 + direction;
    }

    // Print the updated PLRU bits to both console and output file
    printf("Updated PLRU bits: ");
    fprintf(output_file, "Updated PLRU bits: ");
    for (i = 0; i < NUM_LINES_PER_INDEX - 1; i++) {
        printf("%d", pseudo_LRU[i]);
        fprintf(output_file, "%d", pseudo_LRU[i]);
    }
    printf("\n");
    fprintf(output_file, "\n");
}




// Function to find the way to evict using the PLRU tree
int find_eviction_way(unsigned char PLRU[]) {
    int depth = log2(NUM_LINES_PER_INDEX);  // Depth of the PLRU tree
    int index = 0;
    int level;

    // Traverse the tree from the root to the leaf level
    for (level = 0; level < depth; level++) {
        // Determine the direction based on the PLRU bit
        int direction = !(PLRU[index]);

        // Move to the next level (left or right child)
        index = 2 * index + 1 + direction;
    }

    // Return the victim index
    return index - (NUM_LINES_PER_INDEX -1);
}


// Simulate the reporting of snoop results by other caches
int GetSnoopResult(unsigned int Address) {
    unsigned int byte_offset = Address & 0x3; // Extract 2 LSBs

    if (byte_offset == 0x0) {
        return HIT;
    } else if (byte_offset == 0x1) {
        return HITM;
    } else if (byte_offset == 0x2 || byte_offset == 0x3) {
        return NOHIT;
    }

    return NOHIT; // Default to NOHIT
}

void BusOperation(int BusOp, unsigned int Address, int *SnoopResult) {
    // Simulate snoop result
    *SnoopResult = GetSnoopResult(Address);

    if (Mode == 1) { // Only print in normal mode
        printf("Bus Communication:\n");
        printf("  Operation: %s\n", 
               (BusOp == READ) ? "READ" :
               (BusOp == WRITE) ? "WRITE" :
               (BusOp == INVALIDATE) ? "INVALIDATE" :
               (BusOp == RWIM) ? "RWIM" : "UNKNOWN");
        printf("  Address: 0x%08X\n", Address);
    }

    // Log the bus communication to the output file
    if (output_file) {
        fprintf(output_file, "Bus Communication: Operation=%s, Address=0x%08X\n",
                (BusOp == READ) ? "READ" :
                (BusOp == WRITE) ? "WRITE" :
                (BusOp == INVALIDATE) ? "INVALIDATE" :
                (BusOp == RWIM) ? "RWIM" : "UNKNOWN",
                Address);
    }

    // Report the snoop result
    PutSnoopResult(Address, *SnoopResult);
}

void PutSnoopResult(unsigned int Address, int SnoopResult) {
    if (Mode == 1) { // Normal mode
        printf("SnoopResult: Address: 0x%08X, SnoopResult: %s\n",
               Address,
               (SnoopResult == HIT) ? "HIT" :
               (SnoopResult == HITM) ? "HITM" :
               (SnoopResult == NOHIT) ? "NOHIT" : "UNKNOWN");
    }

    // Log the snoop result to the output file
    if (output_file) {
        fprintf(output_file, "SnoopResult: Address=0x%08X, SnoopResult=%s\n",
                Address,
                (SnoopResult == HIT) ? "HIT" :
                (SnoopResult == HITM) ? "HITM" :
                (SnoopResult == NOHIT) ? "NOHIT" : "UNKNOWN");
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
    unsigned int cache_address = (tag << 20) | (index << 6);
    CacheIndex *current_index = &cache[index];
    int hit = -1; // Index of the hit line, -1 if miss
    int all_filled = 1; // Flag to track if all lines in the index are filled
    

    // Check for a hit and verify if all lines are filled
    int i;
    for (i = 0; i < NUM_LINES_PER_INDEX; i++) {
        if (current_index->lines[i].metadata.valid) {
            // If we find a valid line, we check for a hit
            if (current_index->lines[i].tag == tag) {
                hit = i;
                break;
            }
        } else {
            // If we find an invalid line, we set all_filled to false
            all_filled = 0;
        }
    }

    if (hit != -1) {
        // Cache hit: Handle based on MESI state
        MESIState state = current_index->lines[hit].metadata.state;
        num_cache_hits++;

        // Log cache hit
        fprintf(output_file,
                "Cache Hit: Address 0x%08X (Index: 0x%X, Tag: 0x%08X, State: %s)\n",
                cache_address, index, tag, get_mesi_state_name(state));
        if (Mode == 1) {
            printf("Cache Hit: Address 0x%08X (Index: 0x%X, Tag: 0x%08X, State: %s)\n",
                   cache_address, index, tag, get_mesi_state_name(state));
        }
        MessageToCache(SENDLINE, cache_address); // Send line from L2 to L1

        // Update PLRU for this line
        update_plru_tree(current_index->pseudo_LRU, hit);

    } else if (all_filled == 0) {
        // Cache is not fully filled (at least one line is invalid)
        fprintf(output_file,
                "Cache Miss (Empty Slot): Address 0x%08X (Index: 0x%X, Tag: 0x%08X)\n",
                cache_address, index, tag);

        if (Mode == 1) {
            printf("Cache Miss (Empty Slot): Address 0x%08X (Index: 0x%X, Tag: 0x%08X).\n",
                   cache_address, index, tag);
        }

        // Perform bus communication
        int snoop_result = GetSnoopResult(entry->address);
        num_cache_misses++;
        BusOperation(READ, cache_address, &snoop_result);

        // Find the first empty slot to fill
        int first_empty_slot = -1;
        for (i = 0; i < NUM_LINES_PER_INDEX; i++) {
            if (!current_index->lines[i].metadata.valid) {
                first_empty_slot = i;
                break;
            }
        }

        // Insert the new line in the first available way
        current_index->lines[first_empty_slot].tag = tag;
        current_index->lines[first_empty_slot].metadata.valid = 1;
        MESIState new_state;
        if (snoop_result == HIT) {
            new_state = SHARED;
        } else if (snoop_result == HITM) {
            new_state = SHARED; // Data fetched from another cache in MODIFIED state
        } else {
            new_state = EXCLUSIVE;
        }
        current_index->lines[first_empty_slot].metadata.state = new_state; // Set initial state

        // Update PLRU for this line
        update_plru_tree(current_index->pseudo_LRU, first_empty_slot);

        MessageToCache(SENDLINE, cache_address); // Send line from L2 to L1

        if (Mode == 1) {
            printf("Address 0x%08X (Index: 0x%X, Tag: 0x%08X, New State: %s)\n\n",
                   cache_address, index, tag, get_mesi_state_name(new_state));
        }
        fprintf(output_file,
                    "Address 0x%08X (Index: 0x%X, Tag: 0x%08X, New State: %s)\n\n",
                   cache_address, index, tag, get_mesi_state_name(new_state));

    } else {
        // Cache miss with a collision
        fprintf(output_file,
                "Cache Miss (collision): Address 0x%08X (Index: 0x%X, Tag: 0x%08X)\n",
                cache_address, index, tag);
        if (Mode == 1) {
            printf("Cache Miss (collision): Address 0x%08X (Index: 0x%X, Tag: 0x%08X).\n",
                   cache_address, index, tag);
        }

        // Find a way to evict using PLRU
        int eviction_way = find_eviction_way(current_index->pseudo_LRU);
        num_cache_misses++;
        int snoop_result = GetSnoopResult(entry->address);
	unsigned int evicted_tag = current_index->lines[eviction_way].tag;
	unsigned int evicted_index = index; // The current index is the same
	unsigned int evicted_address = (evicted_tag << 20) | (evicted_index << 6); // Tag + Index + Block Offset

        // Check the state of the line being evicted
        if (current_index->lines[eviction_way].metadata.state == MODIFIED) {
            // Modified line requires GETLINE and INVALIDATELINE
            MessageToCache(GETLINE, evicted_address); // L2 requests modified line from L1
            MessageToCache(INVALIDATELINE, evicted_address); // L2 invalidates the line in L1
	    BusOperation(WRITE, evicted_address, &snoop_result);
        } else {
            // Other states only require EVICTLINE
            MessageToCache(EVICTLINE, evicted_address); // L2 evicts the line from L1
        }

        // Perform bus communication
        BusOperation(READ, cache_address, &snoop_result);

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
        update_plru_tree(current_index->pseudo_LRU, eviction_way);

        MessageToCache(SENDLINE, cache_address); // Send line from L2 to L1

        if (Mode == 1) {
            printf("Address 0x%08X (Index: 0x%X, Tag: 0x%08X, New State: %s)\n\n",
                   cache_address, index, tag, get_mesi_state_name(new_state));
        }
        fprintf(output_file,
                "Address 0x%08X (Index: 0x%X, Tag: 0x%08X, New State: %s)\n\n",
                cache_address, index, tag, get_mesi_state_name(new_state));
    }
}

void handle_write_operation(TraceEntry *entry) {
    unsigned int index = entry->parsed_addr.index;
    unsigned int tag = entry->parsed_addr.tag;
    unsigned int cache_address = (tag << 20) | (index << 6);

    CacheIndex *current_index = &cache[index];
    int hit = -1; // Index of the hit line, -1 if miss
    int all_filled = 1; // Flag to track if all lines in the index are filled

    // Check for a hit and verify if all lines are filled
    int i;
    for (i = 0; i < NUM_LINES_PER_INDEX; i++) {
        if (current_index->lines[i].metadata.valid) {
            // If we find a valid line, we check for a hit
            if (current_index->lines[i].tag == tag) {
                hit = i;
                break;
            }
        } else {
            // If we find an invalid line, we set all_filled to false
            all_filled = 0;
        }
    }

    if (hit != -1) {
        // Cache hit: Handle based on MESI state
        MESIState state = current_index->lines[hit].metadata.state;
        num_cache_hits++;

	if (state == SHARED) {
            // SHARED -> MODIFIED: Invalidate other caches
            state = MODIFIED;
            int snoop_result = HIT;
            BusOperation(INVALIDATE, cache_address, &snoop_result); // Invalidate other caches
	}else if (state == EXCLUSIVE || state == MODIFIED) {
            // EXCLUSIVE/MODIFIED: Stay in MODIFIED state, no bus communication
            state = MODIFIED;
	}

	current_index->lines[hit].metadata.dirty = 1;
	current_index->lines[hit].metadata.state = state;
        // Log cache hit
        fprintf(output_file,
                "Cache Hit: Address 0x%08X (Index: 0x%X, Tag: 0x%08X, State: %s)\n",
                cache_address, index, tag, get_mesi_state_name(state));
        if (Mode == 1) {
            printf("Cache Hit: Address 0x%08X (Index: 0x%08X, Tag: 0x%08X, State: %s)\n",
                   cache_address, index, tag, get_mesi_state_name(state));
        }
        MessageToCache(SENDLINE, cache_address); // Send line from L2 to L1

        // Update PLRU for this line
        update_plru_tree(current_index->pseudo_LRU, hit);

    } else if (all_filled == 0) {
        // Cache is not fully filled (at least one line is invalid)
        fprintf(output_file,
                "Cache Miss (Empty Slot): Address 0x%08X (Index: 0x%08X, Tag: 0x%08X)\n",
                cache_address, index, tag);

        if (Mode == 1) {
            printf("Cache Miss (Empty Slot): Address 0x%08X (Index: 0x%08X, Tag: 0x%08X).\n",
                   cache_address, index, tag);
        }

        // Perform bus communication
        int snoop_result = GetSnoopResult(entry->address);
        BusOperation(RWIM, cache_address, &snoop_result);
        num_cache_misses++;

        // Find the first empty slot to fill
        int first_empty_slot = -1;
        for (i = 0; i < NUM_LINES_PER_INDEX; i++) {
            if (!current_index->lines[i].metadata.valid) {
                first_empty_slot = i;
                break;
            }
        }
        MESIState state = current_index->lines[hit].metadata.state;
        // Insert the new line in the first available way
        current_index->lines[first_empty_slot].tag = tag;
        current_index->lines[first_empty_slot].metadata.valid = 1;
        current_index->lines[first_empty_slot].metadata.dirty = 1;
        state = MODIFIED; // Set initial state
	current_index->lines[first_empty_slot].metadata.state = state;

        // Update PLRU for this line
        update_plru_tree(current_index->pseudo_LRU, first_empty_slot);

        MessageToCache(SENDLINE, cache_address); // Send line from L2 to L1

        if (Mode == 1) {
            printf("Address 0x%08X (Index: 0x%08X, Tag: 0x%08X, New State: %s)\n\n",
                   cache_address, index, tag, get_mesi_state_name(state));
        }
        fprintf(output_file,
                    "Address 0x%08X (Index: 0x%08X, Tag: 0x%08X, New State: %s)\n\n",
                   cache_address, index, tag, get_mesi_state_name(state));

    } else {
        // Cache miss with a collision
        fprintf(output_file,
                "Cache Miss (collision): Address 0x%08X (Index: 0x%08X, Tag: 0x%08X)\n",
                cache_address, index, tag);
        if (Mode == 1) {
            printf("Cache Miss (collision): Address 0x%08X (Index: 0x%08X, Tag: 0x%08X).\n",
                   cache_address, index, tag);
        }

        // Find a way to evict using PLRU
        int eviction_way = find_eviction_way(current_index->pseudo_LRU);
        num_cache_misses++;
        int snoop_result = GetSnoopResult(entry->address);
	unsigned int evicted_tag = current_index->lines[eviction_way].tag;
	unsigned int evicted_index = index; // The current index is the same
	unsigned int evicted_address = (evicted_tag << 20) | (evicted_index << 6); // Tag + Index + Block Offset

        // Check the state of the line being evicted
        if (current_index->lines[eviction_way].metadata.state == MODIFIED) {
            // Modified line requires GETLINE and INVALIDATELINE
            MessageToCache(GETLINE, evicted_address); // L2 requests modified line from L1
            MessageToCache(INVALIDATELINE, evicted_address); // L2 invalidates the line in L1
	    BusOperation(WRITE, evicted_address, &snoop_result);
        } else {
            // Other states only require EVICTLINE
            MessageToCache(EVICTLINE, evicted_address); // L2 evicts the line from L1
        }

        // Perform bus communication
        BusOperation(RWIM, cache_address, &snoop_result);

        // Invalidate the line being evicted
        invalidate_cache_line(&current_index->lines[eviction_way]);
        MESIState state = current_index->lines[hit].metadata.state;

        // Insert the new tag and update the line's state
        current_index->lines[eviction_way].tag = tag;
        current_index->lines[eviction_way].metadata.valid = 1;
        current_index->lines[eviction_way].metadata.dirty = 1;
        state = MODIFIED;
	current_index->lines[eviction_way].metadata.state = state;
        // Update PLRU after inserting the new tag
        update_plru_tree(current_index->pseudo_LRU, eviction_way);

        MessageToCache(SENDLINE, cache_address); // Send line from L2 to L1

        if (Mode == 1) {
            printf("Address 0x%08X (Index: 0x%08X, Tag: 0x%08X, New State: %s)\n\n",
                  cache_address, index, tag, get_mesi_state_name(state));
        }
        fprintf(output_file,
                "Address 0x%08X (Index: 0x%08X, Tag: 0x%08X, New State: %s)\n\n",
                cache_address, index, tag, get_mesi_state_name(state));
    }
}


void handle_instruction_cache_read(TraceEntry *entry) {
    unsigned int index = entry->parsed_addr.index;
    unsigned int tag = entry->parsed_addr.tag;
    unsigned int cache_address = (tag << 20) | (index << 6);

    CacheIndex *current_index = &cache[index];
    int hit = -1; // Index of the hit line, -1 if miss
    int all_filled = 1; // Flag to track if all lines in the index are filled

    // Check for a hit and verify if all lines are filled
    int i;
    for (i = 0; i < NUM_LINES_PER_INDEX; i++) {
        if (current_index->lines[i].metadata.valid) {
            // If we find a valid line, we check for a hit
            if (current_index->lines[i].tag == tag) {
                hit = i;
                break;
            }
        } else {
            // If we find an invalid line, we set all_filled to false
            all_filled = 0;
        }
    }

    if (hit != -1) {
        // Cache hit: Handle based on MESI state
        MESIState state = current_index->lines[hit].metadata.state;
        num_cache_hits++;

        // Log cache hit
        fprintf(output_file,
                "Cache Hit: Address 0x%08X (Index: 0x%X, Tag: 0x%08X, State: %s)\n",
                cache_address, index, tag, get_mesi_state_name(state));
        if (Mode == 1) {
            printf("Cache Hit: Address 0x%08X (Index: 0x%X, Tag: 0x%08X, State: %s)\n",
                   cache_address, index, tag, get_mesi_state_name(state));
        }
        MessageToCache(SENDLINE, cache_address); // Send line from L2 to L1

        // Update PLRU for this line
        update_plru_tree(current_index->pseudo_LRU, hit);

    } else if (all_filled == 0) {
        // Cache is not fully filled (at least one line is invalid)
        fprintf(output_file,
                "Cache Miss (Empty Slot): Address 0x%08X (Index: 0x%X, Tag: 0x%08X)\n",
                cache_address, index, tag);

        if (Mode == 1) {
            printf("Cache Miss (Empty Slot): Address 0x%08X (Index: 0x%X, Tag: 0x%08X).\n",
                   cache_address, index, tag);
        }

        // Perform bus communication
        int snoop_result = GetSnoopResult(entry->address);
        num_cache_misses++;
        BusOperation(READ, cache_address, &snoop_result);

        // Find the first empty slot to fill
        int first_empty_slot = -1;
        for (i = 0; i < NUM_LINES_PER_INDEX; i++) {
            if (!current_index->lines[i].metadata.valid) {
                first_empty_slot = i;
                break;
            }
        }

        // Insert the new line in the first available way
        current_index->lines[first_empty_slot].tag = tag;
        current_index->lines[first_empty_slot].metadata.valid = 1;
        MESIState new_state;
        if (snoop_result == HIT) {
            new_state = SHARED;
        } else if (snoop_result == HITM) {
            new_state = SHARED; // Data fetched from another cache in MODIFIED state
        } else {
            new_state = EXCLUSIVE;
        }
        current_index->lines[first_empty_slot].metadata.state = new_state; // Set initial state

        // Update PLRU for this line
        update_plru_tree(current_index->pseudo_LRU, first_empty_slot);

        MessageToCache(SENDLINE, entry->address); // Send line from L2 to L1

        if (Mode == 1) {
            printf("Address 0x%08X (Index: 0x%X, Tag: 0x%08X, New State: %s)\n\n",
                   cache_address, index, tag, get_mesi_state_name(new_state));
        }
        fprintf(output_file,
                    "Address 0x%08X (Index: 0x%X, Tag: 0x%08X, New State: %s)\n\n",
                   cache_address, index, tag, get_mesi_state_name(new_state));

    } else {
        // Cache miss with a collision
        fprintf(output_file,
                "Cache Miss (collision): Address 0x%08X (Index: 0x%X, Tag: 0x%08X)\n",
                cache_address, index, tag);
        if (Mode == 1) {
            printf("Cache Miss (collision): Address 0x%08X (Index: 0x%X, Tag: 0x%08X).\n",
                   cache_address, index, tag);
        }

        // Find a way to evict using PLRU
        int eviction_way = find_eviction_way(current_index->pseudo_LRU);
        num_cache_misses++;
        int snoop_result = GetSnoopResult(entry->address);
	unsigned int evicted_tag = current_index->lines[eviction_way].tag;
	unsigned int evicted_index = index; // The current index is the same
	unsigned int evicted_address = (evicted_tag << 20) | (evicted_index << 6); // Tag + Index + Block Offset

        // Check the state of the line being evicted
        if (current_index->lines[eviction_way].metadata.state == MODIFIED) {
            // Modified line requires GETLINE and INVALIDATELINE
            MessageToCache(GETLINE, evicted_address); // L2 requests modified line from L1
            MessageToCache(INVALIDATELINE, evicted_address); // L2 invalidates the line in L1
	    BusOperation(WRITE, evicted_address, &snoop_result);
        } else {
            // Other states only require EVICTLINE
            MessageToCache(EVICTLINE, evicted_address); // L2 evicts the line from L1
        }

        // Perform bus communication
        BusOperation(READ, cache_address, &snoop_result);

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
        update_plru_tree(current_index->pseudo_LRU, eviction_way);

        MessageToCache(SENDLINE, cache_address); // Send line from L2 to L1

        if (Mode == 1) {
            printf("Address 0x%08X (Index: 0x%X, Tag: 0x%08X, New State: %s)\n\n",
                   cache_address, index, tag, get_mesi_state_name(new_state));
        }
        fprintf(output_file,
                "Address 0x%08X (Index: 0x%X, Tag: 0x%08X, New State: %s)\n\n",
                cache_address, index, tag, get_mesi_state_name(new_state));
    }
}

void handle_snooped_read_request(TraceEntry *entry) {
    unsigned int index = entry->parsed_addr.index;
    unsigned int tag = entry->parsed_addr.tag;
    unsigned int cache_address = (tag << 20) | (index << 6);

    CacheIndex *current_index = &cache[index];
    int line_found = -1; // Index of the matching line, -1 if not found

    // Log the snooped read request in both modes
    if (Mode == 1) {
        printf("Snooped Read Request: Address 0x%08X (Index: 0x%X, Tag: 0x%X)\n", 
               cache_address, index, tag);
    }
    fprintf(output_file,
            "Operation: Snooped read request (code 3), Address: 0x%08X\n"
            "  Decomposed Address: Byte Offset=0x%X, Index=0x%X, Tag=0x%X\n",
            cache_address, entry->parsed_addr.byte_offset, index, tag);
    int snoop_result = GetSnoopResult(entry->address);
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
	    BusOperation(WRITE, cache_address, &snoop_result);
            MessageToCache(GETLINE, cache_address);

            if (Mode == 1) {
                printf("Snooped Read: MODIFIED -> SHARED (Write-back to memory).\n\n");
            }

            fprintf(output_file,
                    "  Snoop Result: MODIFIED -> SHARED (Write-back to memory).\n"
                    "  Metadata: Valid=%d, Dirty=%d, MESI State=%s\n"
                    "  Index Pseudo-LRU: 0x%X\n\n",
                    line->metadata.valid, line->metadata.dirty,
                    get_mesi_state_name(line->metadata.state),
                    current_index->pseudo_LRU);

        } else if (state == EXCLUSIVE) {
            line->metadata.state = SHARED;
            MessageToCache(GETLINE, cache_address);

            if (Mode == 1) {
                printf("Snooped Read: EXCLUSIVE -> SHARED.\n\n");
            }

            fprintf(output_file,
                    "  Snoop Result: EXCLUSIVE -> SHARED.\n"
                    "  Metadata: Valid=%d, Dirty=%d, MESI State=%s\n"
                    "  Index Pseudo-LRU: 0x%X\n\n",
                    line->metadata.valid, line->metadata.dirty,
                    get_mesi_state_name(line->metadata.state),
                    current_index->pseudo_LRU);

        } else if (state == SHARED) {
            if (Mode == 1) {
                printf("Snooped Read: Already in SHARED state. No action needed.\n\n");
            }

            fprintf(output_file,
                    "  Snoop Result: Already in SHARED state. No action needed.\n"
                    "  Metadata: Valid=%d, Dirty=%d, MESI State=%s\n"
                    "  Index Pseudo-LRU: 0x%X\n\n",
                    line->metadata.valid, line->metadata.dirty,
                    get_mesi_state_name(line->metadata.state),
                    current_index->pseudo_LRU);

        } else if (state == INVALID) {
            if (Mode == 1) {
                printf("Snooped Read: Line in INVALID state. No action needed.\n\n");
            }

            fprintf(output_file,
                    "  Snoop Result: Line in INVALID state. Invalidating line.\n"
                    "  Metadata: Valid=%d, Dirty=%d, MESI State=%s\n"
                    "  Index Pseudo-LRU: 0x%X\n\n",
                    line->metadata.valid, line->metadata.dirty,
                    get_mesi_state_name(line->metadata.state),
                    current_index->pseudo_LRU);
        }
    } else {
        // Line not present in cache
        if (Mode == 1) {
            printf("Snooped Read: Line not present in cache. No action needed.\n\n");
        }

        fprintf(output_file,
                "  Snoop Result: Line not present in cache. No action needed.\n\n");
    }
}

void handle_snooped_write_request(TraceEntry *entry) {
}

void handle_snooped_rwim_request(TraceEntry *entry) {
    unsigned int index = entry->parsed_addr.index;
    unsigned int tag = entry->parsed_addr.tag;
    unsigned int cache_address = (tag << 20) | (index << 6);

    CacheIndex *current_index = &cache[index];
    int line_found = -1; // Index of the matching line, -1 if not found

    // Log the snooped RWIM request in both modes
    if (Mode == 1) {
        printf("Snooped RWIM Request: Address 0x%08X (Index: 0x%X, Tag: 0x%X)\n", 
               cache_address, index, tag);
    }
    fprintf(output_file,
            "Operation: Snooped RWIM request (code 5), Address: 0x%08X\n"
            "  Decomposed Address: Byte Offset=0x%X, Index=0x%X, Tag=0x%X\n",
            cache_address, entry->parsed_addr.byte_offset, index, tag);

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
            MessageToCache(GETLINE, cache_address); // Simulate L2 requesting data
            MessageToCache(INVALIDATELINE, cache_address); // Invalidate 
            int snoop_result = NOHIT;
	    BusOperation(WRITE, cache_address, &snoop_result);
            invalidate_cache_line(line); // Invalidate the line
            if (Mode == 1) {
                printf("Snooped RWIM: MODIFIED -> INVALID (Write-back to memory).\n\n");
            }
            fprintf(output_file,
                    "  Snoop Result: MODIFIED -> INVALID (Write-back to memory).\n\n"
                    "  Metadata: Valid=%d, Dirty=%d, MESI State=%s\n"
                    "  Index Pseudo-LRU: 0x%X\n",
                    line->metadata.valid, line->metadata.dirty,
                    get_mesi_state_name(INVALID),
                    current_index->pseudo_LRU);

        } else if (state == SHARED || state == EXCLUSIVE) {
            // Transition SHARED/EXCLUSIVE -> INVALID
            MessageToCache(INVALIDATELINE, cache_address); // Invalidate shared/exclusive copies
            invalidate_cache_line(line); // Invalidate the line
            if (Mode == 1) {
                printf("Snooped RWIM: SHARED/EXCLUSIVE -> INVALID.\n\n");
            }
            fprintf(output_file,
                    "  Snoop Result: SHARED/EXCLUSIVE -> INVALID.\n"
                    "  Metadata: Valid=%d, Dirty=%d, MESI State=%s\n"
                    "  Index Pseudo-LRU: 0x%X\n\n",
                    line->metadata.valid, line->metadata.dirty,
                    get_mesi_state_name(INVALID),
                    current_index->pseudo_LRU);

        } else if (state == INVALID) {
            // Line already in INVALID state
            if (Mode == 1) {
                printf("Snooped RWIM: Line already in INVALID state. No action needed.\n\n");
            }
            fprintf(output_file,
                    "  Snoop Result: Line already in INVALID state. No action needed.\n"
                    "  Metadata: Valid=%d, Dirty=%d, MESI State=%s\n"
                    "  Index Pseudo-LRU: 0x%X\n\n",
                    line->metadata.valid, line->metadata.dirty,
                    get_mesi_state_name(INVALID),
                    current_index->pseudo_LRU);
        }
    } else {
        // Line not present in cache
        if (Mode == 1) {
            printf("Snooped RWIM: Line not present in cache. No action needed.\n\n");
        }
        fprintf(output_file,
                "  Snoop Result: Line not present in cache. No action needed.\n\n");
    }
}

void handle_snooped_invalidate_command(TraceEntry *entry) {
    unsigned int index = entry->parsed_addr.index;
    unsigned int tag = entry->parsed_addr.tag;
    unsigned int cache_address = (tag << 20) | (index << 6);

    CacheIndex *current_index = &cache[index];
    int line_found = -1; // Index of the matching line, -1 if not found

    // Log the snooped invalidate request in both modes
    if (Mode == 1) {
        printf("Snooped Invalidate Request: Address 0x%08X (Index: 0x%X, Tag: 0x%X)\n", 
               cache_address, index, tag);
    }
    fprintf(output_file,
            "Operation: Snooped invalidate command (code 6), Address: 0x%08X\n"
            "  Decomposed Address: Byte Offset=0x%X, Index=0x%X, Tag=0x%X\n",
            cache_address, entry->parsed_addr.byte_offset, index, tag);

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
            MessageToCache(INVALIDATELINE, cache_address);
            invalidate_cache_line(line); // Properly invalidate the line and update PLRU
            if (Mode == 1) {
                printf("Snooped Invalidate: SHARED -> INVALID.\n\n");
            }
            fprintf(output_file,
                    "  Snoop Result: SHARED -> INVALID.\n"
                    "  Metadata: Valid=%d, Dirty=%d, MESI State=%s\n"
                    "  Index Pseudo-LRU: 0x%X\n\n",
                    line->metadata.valid, line->metadata.dirty,
                    get_mesi_state_name(INVALID),
                    current_index->pseudo_LRU);

        } else if (state == INVALID) {
            // INVALID: No action needed
            if (Mode == 1) {
                printf("Snooped Invalidate: Line already in INVALID state. No action needed.\n\n");
            }
            fprintf(output_file,
                    "  Snoop Result: Line already in INVALID state. No action needed.\n"
                    "  Metadata: Valid=%d, Dirty=%d, MESI State=%s\n"
                    "  Index Pseudo-LRU: 0x%X\n\n",
                    line->metadata.valid, line->metadata.dirty,
                    get_mesi_state_name(INVALID),
                    current_index->pseudo_LRU);

        } else if (state == MODIFIED || state == EXCLUSIVE) {
            // Error: Invalid scenario for snooped invalidate in MODIFIED or EXCLUSIVE state
            if (Mode == 1) {
                printf("Error: Snooped Invalidate: Line in %s state (Invalid scenario).\n\n",
                       get_mesi_state_name(state));
            }
            fprintf(output_file,
                    "  Error: Invalid scenario. Line in %s state.\n"
                    "  Metadata: Valid=%d, Dirty=%d, MESI State=%s\n"
                    "  Index Pseudo-LRU: 0x%X\n\n",
                    get_mesi_state_name(state),
                    line->metadata.valid, line->metadata.dirty,
                    get_mesi_state_name(line->metadata.state),
                    current_index->pseudo_LRU);
        }
    } else {
        // Line not present in cache
        if (Mode == 1) {
            printf("Snooped Invalidate: Line not present in cache. No action needed.\n\n");
        }
        fprintf(output_file,
                "  Snoop Result: Line not present in cache. No action needed.\n\n");
    }
}

void handle_clear_cache_request() {
    if (Mode == 1) {
        printf("Clearing cache and resetting all states to initial values...\n");
    }

    fprintf(output_file, "Operation: Clear cache (code 8)\n");

    // Iterate over all cache indexes and lines
    int i, j;
    for (i = 0; i < NUM_INDEXES; i++) {
        for (j = 0; j < NUM_LINES_PER_INDEX; j++) {
            // Check if the line is dirty
            if (cache[i].lines[j].metadata.dirty) {
                // Generate the 32-bit address: concatenate 12 bits for tag + 14 bits for index + 6 bits of 0s
                unsigned int tag = cache[i].lines[j].tag;
                unsigned int index = i;
                unsigned int address = (tag << 20) | (index << 6); // Concatenate tag and index, 6 zero bits for block offset

                // Perform bus write operation for the dirty line
                int snoop_result = NOHIT; 
                BusOperation(WRITE, address, &snoop_result);

                // Log the write operation to output file
                fprintf(output_file, "Bus Operation: Write address 0x%08X (from dirty cache line)\n", address);
                if (Mode == 1) {
                    printf("Bus Operation: Write address 0x%08X (from dirty cache line)\n", address);
                }
            }

            // Clear the cache line after performing bus operations (if any)
            cache[i].lines[j].tag = 0;                            // Clear the tag
            cache[i].lines[j].metadata.valid = 0;                 // Mark as invalid
            cache[i].lines[j].metadata.dirty = 0;                 // Clear the dirty bit
            cache[i].lines[j].metadata.state = INVALID;           // Reset state to INVALID
        }

        // Reset pseudo_LRU using the discussed approach
        for (j = 0; j < NUM_LINES_PER_INDEX - 1; j++) {
            cache[i].pseudo_LRU[j] = 0; // Clear all the bits in pseudo_LRU array
        }
    }

    if (Mode == 1) {
        printf("Cache successfully cleared.\n\n");
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

    printf("Cache state printed successfully.\n\n");
    fprintf(output_file, "Cache state printed successfully.\n\n");
}


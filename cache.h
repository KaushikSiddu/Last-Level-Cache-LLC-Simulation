#ifndef CACHE_H
#define CACHE_H

#define NUM_INDEXES 16384
#define NUM_LINES_PER_INDEX 16
#include <stdbool.h>


// MESI states (Invalid, Modified, Exclusive, Shared)
typedef enum {
    INVALID,
    MODIFIED,
    EXCLUSIVE,
    SHARED
} MESIState;

// Cache metadata (valid, dirty, MESI state)
typedef struct {
    int valid;       // Valid bit (0 or 1)
    int dirty;       // Dirty bit (0 or 1)
    MESIState state; // MESI state (INVALID, MODIFIED, EXCLUSIVE, SHARED)
} CacheMetadata;

// Cache line structure (holds tag and metadata)
typedef struct {
    unsigned int tag;          // 12-bit tag
    CacheMetadata metadata;    // Metadata for cache line
} CacheLine;

// Cache index structure (holds multiple cache lines)
typedef struct {
    CacheLine lines[NUM_LINES_PER_INDEX]; // Multiple cache lines per index
    unsigned short pseudo_LRU;            // 15-bit pseudo-LRU bits for the index
} CacheIndex;

// Decompose address into its components
typedef struct {
    unsigned int byte_offset;  // 6-bit byte offset
    unsigned int index;        // 14-bit index
    unsigned int tag;          // 12-bit tag
} CacheAddress;

typedef struct {
    int operation_code;       // Operation code from the trace file
    unsigned int address;     // Original 32-bit address
    CacheAddress parsed_addr; // Decomposed address fields
    CacheMetadata metadata;   // Metadata for cache entry (valid, dirty, MESI state)
} TraceEntry;



// Function prototypes
CacheAddress decompose_address(unsigned int address);
CacheMetadata initialize_cache_metadata();
void initialize_cache();
extern CacheIndex cache[NUM_INDEXES];
void handle_read_operation(TraceEntry *entry);


#endif // CACHE_H


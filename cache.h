#ifndef CACHE_H
#define CACHE_H

#define NUM_INDEXES 16384
#define NUM_LINES_PER_INDEX 16
#include <stdbool.h>


#define READ 1         /* Bus Read */
#define WRITE 2        /* Bus Write */
#define INVALIDATE 3   /* Bus Invalidate */
#define RWIM 4         /* Bus Read With Intent to Modify */

// Snoop Result Types
#define NOHIT 0        /* No hit */
#define HIT 1          /* Hit */
#define HITM 2         /* Hit to modified line */

// L2 to L1 Message Types
#define GETLINE 1      /* Request data for modified line in L1 */
#define SENDLINE 2     /* Send requested cache line to L1 */
#define INVALIDATELINE 3 /* Invalidate a line in L1 */
#define EVICTLINE 4    /* Evict a line from L1 */


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


// Bus message types
typedef enum {
    BUS_READ,
    BUS_WRITE,
    BUS_INVALIDATE
} BusMessageType;


// Function prototypes
const char *get_operation_name(int code);
const char *get_mesi_state_name(MESIState state);
void print_summary();

CacheAddress decompose_address(unsigned int address);
CacheMetadata initialize_cache_metadata();
void initialize_cache();
extern CacheIndex cache[NUM_INDEXES];

void BusOperation(int BusOp, unsigned int Address, int *SnoopResult);
int GetSnoopResult(unsigned int Address);
void PutSnoopResult(unsigned int Address, int SnoopResult);
void MessageToCache(int Message, unsigned int Address);
void handle_read_operation(TraceEntry *entry);
void handle_write_operation(TraceEntry *entry);
void handle_instruction_cache_read(TraceEntry *entry);
void handle_snooped_read_request(TraceEntry *entry);
void handle_snooped_write_request(TraceEntry *entry);
void handle_snooped_rwim_request(TraceEntry *entry);



#endif // CACHE_H


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h> // For checking if the file exists

// #define ENABLE_DEBUG

#define DEFAULT_TRACE_FILE "default_trace.txt"
#define OUTPUT_FILE "parsed_output.txt" // Output file to store parsed data

// Cache operation codes
enum OperationCodes {
    READ_L1_DATA = 0,
    WRITE_L1_DATA = 1,
    READ_L1_INST = 2,
    SNOOPED_READ = 3,
    SNOOPED_WRITE = 4,
    SNOOPED_RWIM = 5,
    SNOOPED_INVALIDATE = 6,
    CLEAR_CACHE = 8,
    PRINT_CACHE_STATE = 9
};

// Structure for decomposing an address
typedef struct {
    unsigned int byte_offset; // 6 LSB bits
    unsigned int index;       // Next 14 bits
    unsigned int tag;         // Remaining 12 bits
} CacheAddress;

// Structure to hold cache-specific metadata
typedef struct {
    int valid;                // Valid bit (1 if valid, 0 otherwise)
    int dirty;                // Dirty bit (1 if modified, 0 otherwise)
    unsigned int pseudo_LRU;  // 15 bits for pseudo-LRU (0 to 0x7FFF)
} CacheMetadata;

// Structure to hold a trace entry
typedef struct {
    int operation_code;       // Operation code from the trace file
    unsigned int address;     // Original 32-bit address
    CacheAddress parsed_addr; // Decomposed address fields
    CacheMetadata metadata;   // Metadata for cache entry
} TraceEntry;

// Function to decompose a 32-bit address
CacheAddress decompose_address(unsigned int address) {
    CacheAddress parsed;
    parsed.byte_offset = address & 0x3F;           // 6 LSB bits (0b111111 or 0x3F)
    parsed.index = (address >> 6) & 0x3FFF;        // Next 14 bits (0x3FFF)
    parsed.tag = (address >> 20) & 0xFFF;          // Remaining 12 bits
    return parsed;
}

// Function to initialize cache metadata
CacheMetadata initialize_cache_metadata() {
    CacheMetadata metadata;
    metadata.valid = 0;                            // Invalid by default
    metadata.dirty = 0;                            // Clean by default
    metadata.pseudo_LRU = 0x7FFF;                  // All bits set (example starting state)
    return metadata;
}

// Function to display operation name based on operation code
const char *get_operation_name(int code) {
    switch (code) {
        case READ_L1_DATA: return "Read request from L1 data cache";
        case WRITE_L1_DATA: return "Write request from L1 data cache";
        case READ_L1_INST: return "Read request from L1 instruction cache";
        case SNOOPED_READ: return "Snooped read request";
        case SNOOPED_WRITE: return "Snooped write request";
        case SNOOPED_RWIM: return "Snooped read with intent to modify";
        case SNOOPED_INVALIDATE: return "Snooped invalidate command";
        case CLEAR_CACHE: return "Clear cache and reset state";
        case PRINT_CACHE_STATE: return "Print contents and state of each valid cache line";
        default: return "Unknown operation";
    }
}

// Function to parse a trace line
int parse_trace_line(const char *line, int debug, TraceEntry *entry, FILE *output_file) {
    int items_parsed;
    unsigned int address;

    // Skip empty lines
    if (line[0] == '\n' || line[0] == '\0') {
        return 0;  // Return success for empty lines
    }

    // Parse the line for operation code and address
    items_parsed = sscanf(line, "%d %x", &entry->operation_code, &address);

    // Ensure we have at least the operation code
    if (items_parsed == 0) {
        fprintf(stderr, "Invalid format in line: '%s'\n", line);
        return -1;
    }

    // Handle commands that require an address
    if (items_parsed == 2) {
        entry->address = address;
        entry->parsed_addr = decompose_address(address);
        entry->metadata = initialize_cache_metadata(); // Initialize metadata
    } else {
        entry->address = 0;
        memset(&entry->parsed_addr, 0, sizeof(CacheAddress)); // Clear parsed address
        entry->metadata = initialize_cache_metadata();       // Initialize metadata
    }

    // Write parsed data to the output file
    fprintf(output_file, "Operation: %s (code %d), Address: 0x%08X\n",
            get_operation_name(entry->operation_code), entry->operation_code, entry->address);
    fprintf(output_file, "  Decomposed Address: Byte Offset=0x%X, Index=0x%X, Tag=0x%X\n",
            entry->parsed_addr.byte_offset, entry->parsed_addr.index, entry->parsed_addr.tag);
    fprintf(output_file, "  Metadata: Valid=%d, Dirty=%d, Pseudo-LRU=0x%X\n\n",
            entry->metadata.valid, entry->metadata.dirty, entry->metadata.pseudo_LRU);

    // Debug output if enabled
    #ifdef ENABLE_DEBUG
    if (debug) {
        printf("Parsed line: Operation=%s (code %d), Address=0x%08X\n",
               get_operation_name(entry->operation_code), entry->operation_code, entry->address);
        printf("  Decomposed Address: Byte Offset=0x%X, Index=0x%X, Tag=0x%X\n",
               entry->parsed_addr.byte_offset, entry->parsed_addr.index, entry->parsed_addr.tag);
        printf("  Metadata: Valid=%d, Dirty=%d, Pseudo-LRU=0x%X\n",
               entry->metadata.valid, entry->metadata.dirty, entry->metadata.pseudo_LRU);
    }
    #endif

    return 0;
}

// Function to read and parse the trace file
void read_trace_file(const char *filename, int debug) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        return;
    }

    FILE *output_file = fopen(OUTPUT_FILE, "w");
    if (!output_file) {
        perror("Error creating output file");
        fclose(file);
        return;
    }

    char line[256];
    int line_number = 0;
    TraceEntry entry;
    while (fgets(line, sizeof(line), file)) {
        line_number++;
        if (parse_trace_line(line, debug, &entry, output_file) != 0) {
            fprintf(stderr, "Error parsing line %d: %s\n", line_number, line);
        }
    }

    fclose(file);
    fclose(output_file);
    printf("Parsed output saved to '%s'\n", OUTPUT_FILE);
}

int main(int argc, char *argv[]) {
    const char *filename = DEFAULT_TRACE_FILE;
    int debug = 0;

    // Parse command-line arguments for file name and debug option
    if (argc > 1) {
        if (strcmp(argv[1], "--debug") == 0) {
            debug = 1;
        } else {
            filename = argv[1];
        }
    }

    if (argc > 2 && strcmp(argv[2], "--debug") == 0) {
        debug = 1;
    }

    // Check if the file exists
    struct stat buffer;
    if (stat(filename, &buffer) != 0) {
        fprintf(stderr, "Error: File '%s' not found.\n", filename);
        return -1;
    }

    // Read and parse the trace file
    read_trace_file(filename, debug);

    return 0;
}

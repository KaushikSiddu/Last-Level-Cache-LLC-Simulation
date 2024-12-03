// trace.c
#include "cache.h"
#include <stdio.h>
#include <string.h>

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
    fprintf(output_file, "  Metadata: Valid=%d, Dirty=%d, MESI State=%s\n", 
            entry->metadata.valid, entry->metadata.dirty,
            get_mesi_state_name(entry->metadata.state));
    fprintf(output_file, "  Index Pseudo-LRU: 0x%X\n\n", cache[entry->parsed_addr.index].pseudo_LRU);

    // Debug output if enabled
    #ifdef ENABLE_DEBUG
    if (debug) {
        printf("Parsed line: Operation=%s (code %d), Address=0x%08X\n",
               get_operation_name(entry->operation_code), entry->operation_code, entry->address);
        printf("  Decomposed Address: Byte Offset=0x%X, Index=0x%X, Tag=0x%X\n",
               entry->parsed_addr.byte_offset, entry->parsed_addr.index, entry->parsed_addr.tag);
        printf("  Metadata: Valid=%d, Dirty=%d, MESI State=%s\n",
               entry->metadata.valid, entry->metadata.dirty,
               get_mesi_state_name(entry->metadata.state));
        printf("  Index Pseudo-LRU: 0x%X\n", cache[entry->parsed_addr.index].pseudo_LRU);
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

    FILE *output_file = fopen("parsed_output.txt", "w");
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
    printf("Parsed output saved to 'parsed_output.txt'\n");
}


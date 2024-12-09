<<<<<<< HEAD
#include "cache.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

// Function to get operation name from operation code
=======
// trace.c
#include "cache.h"
#include <stdio.h>
#include <string.h>

>>>>>>> main
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
<<<<<<< HEAD

// Function to parse a trace line
int parse_trace_line(const char *line, TraceEntry *entry) {
=======
// Function to parse a trace line
int parse_trace_line(const char *line, int debug, TraceEntry *entry, FILE *output_file) {
>>>>>>> main
    int items_parsed;
    unsigned int address;

    // Skip empty lines
    if (line[0] == '\n' || line[0] == '\0') {
<<<<<<< HEAD
        return 0;  // Success for empty lines
=======
        return 0;  // Return success for empty lines
>>>>>>> main
    }

    // Parse the line for operation code and address
    items_parsed = sscanf(line, "%d %x", &entry->operation_code, &address);

    // Ensure we have at least the operation code
    if (items_parsed == 0) {
        fprintf(stderr, "Invalid format in line: '%s'\n", line);
        return -1;
    }

<<<<<<< HEAD
    // Decompose the address if present
    if (items_parsed == 2) {
        entry->address = address;
        entry->parsed_addr = decompose_address(address);
    } else {
        entry->address = 0;
        memset(&entry->parsed_addr, 0, sizeof(CacheAddress)); // Clear parsed address
    }

    return 0; // Success
}

void read_trace_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Could not open file: %s\n", filename);
        fprintf(output_file, "Error: Could not open file: %s\n", filename);
=======
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
>>>>>>> main
        return;
    }

    char line[256];
    int line_number = 0;
    TraceEntry entry;
<<<<<<< HEAD

    fprintf(output_file, "Processing trace file: %s\n", filename);

    while (fgets(line, sizeof(line), file)) {
        line_number++;
        if (parse_trace_line(line, &entry) == 0) {
            handle_trace_entry(&entry); // Dispatch to operation handlers
        } else {
            fprintf(stderr, "Error parsing line %d: %s\n", line_number, line);
            fprintf(output_file, "Error parsing line %d: %s\n", line_number, line);
=======
    while (fgets(line, sizeof(line), file)) {
        line_number++;
        if (parse_trace_line(line, debug, &entry, output_file) != 0) {
            fprintf(stderr, "Error parsing line %d: %s\n", line_number, line);
>>>>>>> main
        }
    }

    fclose(file);
<<<<<<< HEAD

    fprintf(output_file, "Finished processing trace file.\n");
    if (Mode == 1) {
        printf("Finished processing trace file.\n");
    }
}


// Dispatch to operation handlers
void handle_trace_entry(TraceEntry *entry) {
    switch (entry->operation_code) {
        case 0: handle_read_operation(entry); break;
        case 1: handle_write_operation(entry); break;
        case 2: handle_instruction_cache_read(entry); break;
        case 3: handle_snooped_read_request(entry); break;
        case 4: handle_snooped_write_request(entry); break;
        case 5: handle_snooped_rwim_request(entry); break;
        case 6: handle_snooped_invalidate_command(entry); break;
        case 8: handle_clear_cache_request(); break;
        case 9: handle_print_cache_state_request(); break;
        default:
            if (Mode == 1) {
                printf("Unknown operation code: %d\n", entry->operation_code);
            }
            break;
    }
=======
    fclose(output_file);
    printf("Parsed output saved to 'parsed_output.txt'\n");
>>>>>>> main
}


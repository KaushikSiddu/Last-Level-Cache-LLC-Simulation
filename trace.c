#include "cache.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

// Function to get operation name from operation code
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

// Function to parse a trace line
int parse_trace_line(const char *line, TraceEntry *entry) {
    int items_parsed;
    unsigned int address;

    // Skip empty lines
    if (line[0] == '\n' || line[0] == '\0') {
        return 0;  // Success for empty lines
    }

    // Parse the line for operation code and address
    items_parsed = sscanf(line, "%d %x", &entry->operation_code, &address);

    // Ensure we have at least the operation code
    if (items_parsed == 0) {
        fprintf(stderr, "Invalid format in line: '%s'\n", line);
        return -1;
    }

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
        return;
    }

    char line[256];
    int line_number = 0;
    TraceEntry entry;

    fprintf(output_file, "Processing trace file: %s\n", filename);

    while (fgets(line, sizeof(line), file)) {
        line_number++;
        if (parse_trace_line(line, &entry) == 0) {
            handle_trace_entry(&entry); // Dispatch to operation handlers
        } else {
            fprintf(stderr, "Error parsing line %d: %s\n", line_number, line);
            fprintf(output_file, "Error parsing line %d: %s\n", line_number, line);
        }
    }

    fclose(file);

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
}


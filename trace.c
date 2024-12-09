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
    int operation_code;
    char extra_input[256]; // To detect extra content in the line

    // Skip empty lines
    if (line[0] == '\n' || line[0] == '\0') {
        return 0;  // Success for empty lines
    }

    // Try to parse the line with operation code and address
    items_parsed = sscanf(line, "%d %x %s", &operation_code, &address, extra_input);

    // Validate the number of items parsed
    if (items_parsed == 0) {
        fprintf(stderr, "Invalid format in line (missing operation code and address): '%s'\n", line);
        return -1;
    } else if (items_parsed == 1) {
        fprintf(stderr, "Invalid format in line (missing address): '%s'\n", line);
        return -1;
    } else if (items_parsed > 2) {
        fprintf(stderr, "Invalid format in line (too many items): '%s'\n", line);
        return -1;
    }

    // Assign parsed values
    entry->operation_code = operation_code;
    entry->address = address;
    entry->parsed_addr = decompose_address(address);

    return 0; // Success
}


void print_cache_statistics() {
    fprintf(output_file, "Cache Statistics:\n");
    fprintf(output_file, "Number of cache reads: %d\n", num_cache_reads);
    fprintf(output_file, "Number of cache writes: %d\n", num_cache_writes);
    fprintf(output_file, "Number of cache hits: %d\n", num_cache_hits);
    fprintf(output_file, "Number of cache misses: %d\n", num_cache_misses);
    fprintf(output_file, "Cache hit ratio: %.2f%%\n", (float)num_cache_hits / num_cache_reads * 100);
    
    if (Mode == 1) {
        printf("Cache Statistics:\n");
        printf("Number of cache reads: %d\n", num_cache_reads);
        printf("Number of cache writes: %d\n", num_cache_writes);
        printf("Number of cache hits: %d\n", num_cache_hits);
        printf("Number of cache misses: %d\n", num_cache_misses);
        printf("Cache hit ratio: %.2f%%\n", (float)num_cache_hits / num_cache_reads * 100);
    }
}

// Dispatch to operation handlers
void handle_trace_entry(TraceEntry *entry) {
    switch (entry->operation_code) {
        case 0: handle_read_operation(entry); num_cache_reads++; break;
        case 1: handle_write_operation(entry); num_cache_writes++; break;
        case 2: handle_instruction_cache_read(entry); num_cache_reads++; break;
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
    print_cache_statistics();
}


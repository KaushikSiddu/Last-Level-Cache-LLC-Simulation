#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h> // For checking if the file exists

// Uncomment this line to enable compile-time debugging
// #define ENABLE_DEBUG

#define DEFAULT_TRACE_FILE "default_trace.txt"

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

// Function to parse a line of the trace file
int parse_trace_line(const char *line, int debug) {
    int operation_code;
    unsigned int address;
    int items_parsed;

    // Skip empty lines
    if (line[0] == '\n' || line[0] == '\0') {
        return 0;  // Return success for empty lines, skipping further processing
    }

    // Parse the line with an optional address
    items_parsed = sscanf(line, "%d %x", &operation_code, &address);

    // Check if parsing failed completely (no operation code found)
    if (items_parsed == 0) {
        fprintf(stderr, "Invalid format in line: '%s'\n", line);
        return -1;
    }

    // Handle commands that don’t require an address (operation codes 8 and 9)
    if (operation_code == 8 || operation_code == 9) {
        if (items_parsed == 1) {  // Only operation code was parsed, no address needed
            #ifdef ENABLE_DEBUG
            if (debug) {
                printf("Parsed line: Operation=%s (code %d), No address required\n",
                       get_operation_name(operation_code), operation_code);
            }
            #endif
            return 0;
        } else {
            fprintf(stderr, "Unexpected address for operation %d in line: '%s'\n", operation_code, line);
            return -1;
        }
    }

    // Handle commands that require both an operation code and an address
    if (items_parsed == 2) {
        #ifdef ENABLE_DEBUG
        if (debug) {
            printf("Parsed line: Operation=%s (code %d), Address=0x%X\n",
                   get_operation_name(operation_code), operation_code, address);
        }
        #endif
        return 0;
    }

    // If we reach here, it means we have an invalid format
    fprintf(stderr, "Invalid format in line: '%s'\n", line);
    return -1;
}

// Function to read and parse the trace file
void read_trace_file(const char *filename, int debug) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        return;
    }

    char line[256];
    int line_number = 0;
    while (fgets(line, sizeof(line), file)) {
        line_number++;
        if (parse_trace_line(line, debug) != 0) {
            fprintf(stderr, "Error parsing line %d: %s\n", line_number, line);
        }
    }

    fclose(file);
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


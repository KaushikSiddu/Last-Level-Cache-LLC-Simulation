// main.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h> // For checking if the file exists
#include "cache.h"


#define DEFAULT_TRACE_FILE "default_trace.txt"

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

    // Initialize cache before reading trace
    initialize_cache();

    // Read and parse the trace file
    read_trace_file(filename, debug);

    return 0;
}


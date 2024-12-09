#include "cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Define the global file pointer for output
FILE *output_file;
int Mode = 0; // 0 = silent, 1 = normal
int num_cache_reads = 0;
int num_cache_writes = 0;
int num_cache_hits = 0;
int num_cache_misses = 0;
int main(int argc, char *argv[]) {
    const char *filename = "rwims.din"; // Default trace file name

    // Parse command-line arguments
    if (argc > 1) {
        filename = argv[1]; // Use the provided trace file name
    }

    if (argc > 2) {
        if (strcmp(argv[2], "normal") == 0) {
            Mode = 1; // Enable normal mode
        } else if (strcmp(argv[2], "silent") == 0) {
            Mode = 0; // Enable silent mode
        } else {
            fprintf(stderr, "Error: Invalid mode specified. Use 'normal' or 'silent'.\n");
            return EXIT_FAILURE;
        }
    }

    // Open the output file for logging
    output_file = fopen("simulation_output.txt", "w");
    if (!output_file) {
        fprintf(stderr, "Error: Could not create output file.\n");
        return EXIT_FAILURE;
    }

    fprintf(output_file, "Starting simulation with trace file: %s\n", filename);

    // Initialize the cache
    initialize_cache();

    // Read and process the trace file
    read_trace_file(filename);

    fprintf(output_file, "Simulation completed successfully.\n");

    // Close the output file
    fclose(output_file);
    num_cache_reads = 0;
    num_cache_writes = 0;
    num_cache_hits = 0;
    num_cache_misses = 0;

    return 0;
}


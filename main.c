<<<<<<< HEAD
#include "cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Define the global file pointer for output
FILE *output_file;
int Mode = 0; // 0 = silent, 1 = normal

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
=======
// main.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h> // For checking if the file exists
#include "cache.h"

#define DEFAULT_TRACE_FILE "rwims.din"

// Global mode variable
int Mode = 0; // 0 = silent, 1 = normal

void print_usage() {
    printf("Usage: ./trace_parser [mode] [trace_file]\n");
    printf("Modes:\n");
    printf("  silent - Minimal output, only statistics and specific responses\n");
    printf("  normal - Detailed output including bus operations and messages\n");
}

int main(int argc, char *argv[]) {
    const char *filename = DEFAULT_TRACE_FILE;

    // Parse command-line arguments
    if (argc > 1) {
        if (strcmp(argv[1], "silent") == 0) {
            Mode = 0;
        } else if (strcmp(argv[1], "normal") == 0) {
            Mode = 1;
        } else {
            print_usage();
            return -1;
        }
    }

    if (argc > 2) {
        filename = argv[2];
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
    read_trace_file(filename);

    // Print summary statistics
    print_summary();
>>>>>>> main

    return 0;
}


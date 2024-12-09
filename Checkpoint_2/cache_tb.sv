`timescale 1ns/1ps

module cache_tb;

    // Inputs to the cache module
    logic clk;
    logic reset;
    logic [31:0] address;
    logic [1:0] operation;

    // Outputs from the cache module
    logic hit;
    logic miss;
    logic [31:0] num_reads;
    logic [31:0] num_writes;
    logic [31:0] num_hits;
    logic [31:0] num_misses;

    // File I/O
    integer trace_file, scan_result;
    logic [31:0] file_address;
    integer file_operation;

    // Instantiate the cache module
    cache dut (
        .clk(clk),
        .reset(reset),
        .address(address),
        .operation(operation),
        .hit(hit),
        .miss(miss),
        .num_reads(num_reads),
        .num_writes(num_writes),
        .num_hits(num_hits),
        .num_misses(num_misses)
    );

    // Clock generation
    initial clk = 0;
    always #5 clk = ~clk; // 10ns clock period

    // Test sequence
    initial begin
        // Initialize
        reset = 1;
        address = 32'b0;
        operation = 2'b00; // Default to read
        @(posedge clk);
        reset = 0;

        // Open trace file
        trace_file = $fopen("trace.txt", "r");
        if (trace_file == 0) begin
            $display("Error: Unable to open trace file.");
            $finish;
        end

        // Read and process each line from the trace file
        while (!$feof(trace_file)) begin
            // Read operation and address from trace file
            scan_result = $fscanf(trace_file, "%d %h\n", file_operation, file_address);
            if (scan_result != 2) begin
                $display("Error: Invalid line in trace file.");
                $finish;
            end

            // Apply the operation
            case (file_operation)
                0: begin // Read operation
                    operation = 2'b00;
                    address = file_address;
                    $display("Read operation: Address = %h", address);
                end
                1: begin // Write operation
                    operation = 2'b01;
                    address = file_address;
                    $display("Write operation: Address = %h", address);
                end
                default: begin
                    $display("Error: Unsupported operation %0d in trace file.", file_operation);
                    $finish;
                end
            endcase

            @(posedge clk); // Wait for one clock cycle to complete the operation
        end

        // Close trace file
        $fclose(trace_file);

        // Print final statistics
        $display("\nFinal Cache Statistics:");
        $display("  Reads: %0d", num_reads);
        $display("  Writes: %0d", num_writes);
        $display("  Hits: %0d", num_hits);
        $display("  Misses: %0d", num_misses);
        $display("  Hit Ratio: %.2f%%", (num_hits * 100.0) / (num_reads + num_writes));

        // End simulation
        $finish;
    end

endmodule
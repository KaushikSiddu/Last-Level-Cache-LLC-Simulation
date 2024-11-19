module cache (
    input logic clk,
    input logic reset,
    input logic [31:0] address,
    input logic [1:0] operation, // 00 = read, 01 = write
    output logic hit,
    output logic miss,
    output logic [31:0] num_reads,
    output logic [31:0] num_writes,
    output logic [31:0] num_hits,
    output logic [31:0] num_misses
);

    // Cache parameters
    localparam NUM_SETS = 2 ** 14; // 14-bit index = 16k sets
    localparam NUM_WAYS = 16;      // 16-way set associative
    localparam TAG_BITS = 12;      // 12-bit tag
    localparam PLRU_BITS = 15;     // 15-bit pseudo-LRU

    typedef struct packed {
        logic valid;
        logic [TAG_BITS-1:0] tag;  // Cache tag
        logic [PLRU_BITS-1:0] plru_bits; // Pseudo-LRU bits
    } cache_line_t;

    cache_line_t cache_mem[NUM_SETS][NUM_WAYS]; // Cache array

    // Internal counters
    logic [31:0] read_count, write_count, hit_count, miss_count;

    // Initialize/reset cache
    integer i, j;
    always_ff @(posedge clk or posedge reset) begin
        if (reset) begin
            read_count <= 0;
            write_count <= 0;
            hit_count <= 0;
            miss_count <= 0;

            for (i = 0; i < NUM_SETS; i++) begin
                for (j = 0; j < NUM_WAYS; j++) begin
                    cache_mem[i][j].valid <= 0;
                    cache_mem[i][j].tag <= 0;
                    cache_mem[i][j].plru_bits <= 0;
                end
            end
        end else begin
            // Extract tag and index
            automatic logic [13:0] index = address[19:6];  // 14-bit index
            automatic logic [TAG_BITS-1:0] tag = address[31:20]; // 12-bit tag
            logic found;
            integer way_index;

            // Search set for a matching tag
            found = 0;
            for (j = 0; j < NUM_WAYS; j++) begin
                if (cache_mem[index][j].valid && cache_mem[index][j].tag == tag) begin
                    found = 1;
                    way_index = j;
                    break;
                end
            end
	
	    // Debug: Display current state for the access
            $display("Address: %h, Tag: %h, Index: %h, Operation: %b, Hit: %b, Miss: %b", 
                     address, tag, index, operation, hit, miss);

            // Process operation
            case (operation)
                2'b00: begin // Read
                    read_count++;
                    if (found) begin
                        hit_count++;
                        hit = 1;
                        miss = 0;
                    end else begin
                        miss_count++;
                        hit = 0;
                        miss = 1;
                    end
                end
                2'b01: begin // Write
                    write_count++;
                    if (found) begin
                        hit_count++;
                        hit = 1;
                        miss = 0;
                    end else begin
                        miss_count++;
                        hit = 0;
                        miss = 1;
                    end
                end
            endcase
        end
    end

    // Output statistics
    assign num_reads = read_count;
    assign num_writes = write_count;
    assign num_hits = hit_count;
    assign num_misses = miss_count;

endmodule
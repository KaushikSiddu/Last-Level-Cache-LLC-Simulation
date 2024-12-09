module trace_parser;

    // Enumerated operation codes
    typedef enum int {
        READ_L1_DATA = 0,
        WRITE_L1_DATA = 1,
        READ_L1_INST = 2,
        SNOOPED_READ = 3,
        SNOOPED_WRITE = 4,
        SNOOPED_RWIM = 5,
        SNOOPED_INVALIDATE = 6,
        CLEAR_CACHE = 8,
        PRINT_CACHE_STATE = 9
    } operation_t;

    // Default file
    string DEFAULT_TRACE_FILE = "default_trace.txt";

    // Declare variables
    string filename;
    integer file, line_number;
    string line;
    int operation_code;
    reg signed [31:0] address;  // Correctly using a reg to store packed data
    bit debug;
   

    // Main initial block
    initial begin
        filename = DEFAULT_TRACE_FILE;
        debug = 0;

        // Parse command-line arguments
        if ($value$plusargs("file=%s", filename)) begin
            $display("Using file: %s", filename);
        end else begin
            $display("Using default file: %s", DEFAULT_TRACE_FILE);
        end
        if ($test$plusargs("debug")) begin
            debug = 1;
        end

        // Open the file
        file = $fopen(filename, "r");
        if (file == 0) begin
            $display("Error: Unable to open file '%s'", filename);
            $finish;
        end

        // Process lines from the file
        line_number = 0;
       /* while (!$feof(file)) begin
            $fgets(line,file);  // Read the line into a string
		$display("inside line 54");
            line_number++;
            if (line.len() == 0) continue;  // Skip empty lines

            if (parse_trace_line(line) != 0) begin
                $display("Error: Parsing failed on line %0d: %s", line_number, line);
            end
        end */

        // Close the file
        $fclose(file);
        $display("Trace parsing completed.");
        $finish;
    end

    // Function to parse a line from the trace file
    function int parse_trace_line(string line);
        int items_parsed;

        // Parse operation code and optional address
        // Correct the parsing here to use '%d' for integer and '%h' for address as a 32-bit value
        items_parsed = $sscanf(line, "%d %h", operation_code, address);

        // Handle invalid formats
        if (items_parsed < 1) begin
            $display("Error: Invalid format in line: '%s'", line);
            return -1;
        end

        // Check if address is required or not
        if ((operation_code == CLEAR_CACHE || operation_code == PRINT_CACHE_STATE) && items_parsed != 1) begin
            $display("Error: Unexpected address for operation %0d in line: '%s'", operation_code, line);
            return -1;
        end
        if ((operation_code != CLEAR_CACHE && operation_code != PRINT_CACHE_STATE) && items_parsed != 2) begin
            $display("Error: Missing address for operation %0d in line: '%s'", operation_code, line);
            return -1;
        end

        // Debug display
        if (debug) begin
            string debug_output;
            if (items_parsed == 2) begin
                $sformat(debug_output, "Parsed line: %s (code %0d), Address=0x%h", get_operation_name(operation_code), operation_code, address);
            end else begin
                $sformat(debug_output, "Parsed line: %s (code %0d), No address required", get_operation_name(operation_code), operation_code);
            end
            $display("%s", debug_output);
        end

        return 0;
    endfunction

    // Function to get the operation name from the operation code
    function string get_operation_name(int code);
        case (code)
            READ_L1_DATA: return "Read request from L1 data cache";
            WRITE_L1_DATA: return "Write request from L1 data cache";
            READ_L1_INST: return "Read request from L1 instruction cache";
            SNOOPED_READ: return "Snooped read request";
            SNOOPED_WRITE: return "Snooped write request";
            SNOOPED_RWIM: return "Snooped read with intent to modify";
            SNOOPED_INVALIDATE: return "Snooped invalidate command";
            CLEAR_CACHE: return "Clear cache and reset state";
            PRINT_CACHE_STATE: return "Print contents and state of each valid cache line";
            default: return "Unknown operation";
        endcase
    endfunction

endmodule

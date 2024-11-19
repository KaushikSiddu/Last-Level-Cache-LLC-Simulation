 module trace_parser;

// Operation codes as localparams
    typedef enum logic [3:0] {
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

    // Default trace file
    string DEFAULT_TRACE_FILE = "default_trace.txt";

    // Variables
    integer file, line_number;
    string line;
    operation_t operation_code;
    int address;
    bit debug = 0;

    // Function to get operation name
    function string get_operation_name(operation_t code);
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

    // Function to parse a line from the trace file
    function int parse_trace_line(int raw_op_code, int address);
        operation_t op_code;
        op_code = operation_t'(raw_op_code);

        // Commands without address (codes 8 and 9)
        if ((op_code == CLEAR_CACHE || op_code == PRINT_CACHE_STATE)) begin
            if (debug) begin
                $display("Parsed line: Operation=%s (code %0d), No address required",
                         get_operation_name(op_code), raw_op_code);
            end
            return 0;
        end

        // Commands with address
        if (debug) begin
            $display("Parsed line: Operation=%s (code %0d), Address=0x%h",
                     get_operation_name(op_code), raw_op_code, address);
        end
        return 0;
    endfunction

    // Task to read the trace file and parse it line by line
    task read_trace_file(string filename);
        integer raw_op_code, addr; // Declare variables at the top of the block
        line_number = 0;

        file = $fopen(filename, "r");
        if (file == 0) begin
            $display("Error: Unable to open file '%s'", filename);
            return;
        end

        while (!$feof(file)) begin
    // Read each line as a pair of integers
           if ($fscanf(file, "%d %h\n", raw_op_code, addr) >= 0) begin
           line_number++;
             $display("Line %0d: Operation=%0d, Address=%0h", line_number, raw_op_code, addr);
             if (parse_trace_line(raw_op_code, addr) != 0) begin
               $display("Error: Parsing failed on line %0d", line_number);
             end
        end else begin
        // Handle invalid or empty lines gracefully
        line_number++;
        $display("Error: Invalid or empty line at %0d", line_number);
        end
        end
        $fclose(file);
    endtask

    // Main initial block
    initial begin
        automatic string filename = DEFAULT_TRACE_FILE;

        // Enable debug mode via command-line arguments
        if ($test$plusargs("debug")) debug = 1;
        if ($value$plusargs("file=%s", filename)) begin
            $display("Using file: %s", filename);
        end else begin
            $display("Using default file: %s", DEFAULT_TRACE_FILE);
        end

        // Read and parse the trace file
        $display("Starting trace parsing...");
        read_trace_file(filename);

        $display("Trace parsing completed.");
        $finish;
    end

endmodule

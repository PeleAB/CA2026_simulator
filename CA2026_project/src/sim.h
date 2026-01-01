#ifndef SIM_H
#define SIM_H

#include <stdint.h>
#include <stdbool.h>

/* ============================================
 * CONSTANTS AND CONFIGURATION
 * ============================================ */

#define NUM_CORES 4
#define NUM_REGISTERS 16
#define IMEM_SIZE 1024          // 1024 instructions per core
#define MAIN_MEM_SIZE (1 << 21) // 2^21 words
#define CACHE_SIZE 512          // 512 words
#define CACHE_BLOCK_SIZE 8      // 8 words per block
#define NUM_CACHE_BLOCKS 64     // 512 / 8 = 64 blocks
#define MAIN_MEM_LATENCY 16     // cycles for first word
#define MAX_TRACE_LINES 100000  // Maximum trace lines per core/bus
#define TRACE_LINE_SIZE 512     // Size of each trace line

/* ============================================
 * INSTRUCTION FORMAT AND OPCODES
 * ============================================ */

// Instruction format: [opcode:8][rd:4][rs:4][rt:4][imm:12]
typedef struct {
    uint8_t opcode;   // bits 31:24
    uint8_t rd;       // bits 23:20
    uint8_t rs;       // bits 19:16
    uint8_t rt;       // bits 15:12
    int16_t imm;      // bits 11:0 (sign-extended to 16 bits)
} Instruction;

// Opcode definitions
typedef enum {
    OP_ADD = 0,
    OP_SUB = 1,
    OP_AND = 2,
    OP_OR = 3,
    OP_XOR = 4,
    OP_MUL = 5,
    OP_SLL = 6,
    OP_SRA = 7,
    OP_SRL = 8,
    OP_BEQ = 9,
    OP_BNE = 10,
    OP_BLT = 11,
    OP_BGT = 12,
    OP_BLE = 13,
    OP_BGE = 14,
    OP_JAL = 15,
    OP_LW = 16,
    OP_SW = 17,
    OP_HALT = 20
} Opcode;

/* ============================================
 * MESI CACHE COHERENCY PROTOCOL
 * ============================================ */

typedef enum {
    MESI_INVALID = 0,
    MESI_SHARED = 1,
    MESI_EXCLUSIVE = 2,
    MESI_MODIFIED = 3
} MESIState;

// Bus commands
typedef enum {
    BUS_NO_CMD = 0,
    BUS_RD = 1,      // Read request
    BUS_RDX = 2,     // Read exclusive (for write)
    BUS_FLUSH = 3    // Write back data
} BusCommand;

// Bus states
typedef enum {
    BUS_STATE_IDLE = 0,
    BUS_STATE_ARBITRATE = 1,  // Arbitration phase (1 cycle)
    BUS_STATE_REQUEST = 2,    // Master issues command (Logged here)
    BUS_STATE_LATENCY = 3,    // Wait for response
    BUS_STATE_FLUSH = 4       // Data transfer
} BusState;

// Bus transaction structure
typedef struct {
    uint8_t origid;       // 0-3: cores, 4: main memory
    BusCommand cmd;
    uint32_t addr;        // 21-bit word address
    uint32_t data;        // 32-bit data
    bool shared;          // Shared signal (set by snooping caches)
} BusTransaction;

/* ============================================
 * CACHE STRUCTURES
 * ============================================ */

// TSRAM entry: tag (12 bits) + MESI state (2 bits)
typedef struct {
    uint16_t tag;         // bits 11:0
    MESIState mesi_state; // bits 13:12
    bool valid;
} TSRAMEntry;

// Cache structure
typedef struct {
    uint32_t dsram[CACHE_SIZE];      // Data storage (512 words)
    TSRAMEntry tsram[NUM_CACHE_BLOCKS]; // Tag + MESI state (64 entries)

    // Pending cache operation state machine
    enum {
        CACHE_IDLE = 0,
        CACHE_WAIT_BUS_EVICT,  // Waiting to flush dirty block
        CACHE_WAIT_BUS_READ,   // Waiting to issue BusRd
        CACHE_WAIT_BUS_WRITE,  // Waiting to issue BusRdX (miss)
        CACHE_WAIT_BUS_UPGRADE,// Waiting to issue BusRdX (shared hit)
        CACHE_FETCHING,        // Issued BusRd/BusRdX, waiting for data
        CACHE_EVICTING,        // Issued Flush, waiting for bus to finish it
        CACHE_UPGRADING        // Issued BusRdX upgrade, waiting for bus
    } state;

    uint32_t pending_addr;
    uint32_t pending_data;
    bool shared_on_bus;        // Remember shared signal from request cycle
    bool is_write_miss;        // Distinguish between Rd miss and RdX miss
    int words_received;        // For 8-word transfer
    int words_sent;            // For 8-word transfer
} Cache;

/* ============================================
 * PIPELINE STAGE STRUCTURES
 * ============================================ */

// Pipeline register: holds instruction + control signals
typedef struct {
    bool valid;           // Is this stage active?
    bool stall;           // Is this stage stalled? (Backpressure)
    bool internal_stall;  // Internal stall reason (Hazard, Cache Miss)
    uint16_t pc;          // Program counter (10 bits)
    Instruction inst;     // Decoded instruction

    // Data values propagated through pipeline
    uint32_t rs_value;
    uint32_t rt_value;
    uint32_t alu_result;
    uint32_t mem_data;
    uint32_t imm_val; // The sign-extended immediate for THIS specific instruction

    // Control signals
    bool reg_write;       // Write to register file?
    bool mem_read;        // Memory read?
    bool mem_write;       // Memory write?
    bool is_halt;         // Halt instruction?
    bool branch_resolved; // Has branch determination been done?
    uint8_t rw;           // Destination register index (0-15)
    uint32_t inst_word;   // Original instruction word (for trace)
    
} PipelineReg;

// Pipeline structure (5 stages)
typedef struct {
    PipelineReg fetch;
    PipelineReg decode;
    PipelineReg execute;
    PipelineReg mem;
    PipelineReg writeback;
} Pipeline;

/* ============================================
 * CORE STRUCTURE
 * ============================================ */

typedef struct {
    int core_id;                      // 0-3
    uint16_t pc;                      // Program counter (10 bits)
    uint32_t registers[NUM_REGISTERS]; // Register file (R0=0, R1=imm, R2-R15 general)
    uint32_t imm_register;            // R1 special register: sign-extended immediate
    uint32_t imem[IMEM_SIZE];         // Instruction memory
    Cache cache;                      // Data cache
    Pipeline pipeline;                // 5-stage pipeline

    bool halted;                      // Has this core executed halt?
    bool halt_fetch;                  // Stop fetching new instructions (HALT in ID)
    bool branch_pending;              // Branch resolved, will update PC after delay slot
    uint16_t branch_target;           // Target PC for pending branch
    uint16_t branch_source_pc;        // PC of the branch instruction itself

    // Register write tracking (for hazard detection across cycle boundaries)
    uint8_t wb_reg_written;           // Register being written by WB this cycle (0 = none)
    
    // Post-WB Latch (Delay slot for Reg Write)
    uint8_t post_wb_reg_addr;
    uint32_t post_wb_reg_val;

    // Pending register write (from current WB stage)
    uint8_t pending_reg_write_addr;
    uint32_t pending_reg_write_val;

    // Statistics
    uint64_t cycles;
    uint64_t instructions;
    uint64_t read_hit;
    uint64_t write_hit;
    uint64_t read_miss;
    uint64_t write_miss;
    uint64_t decode_stall;
    uint64_t mem_stall;

    // Trace output buffer - fixed size to avoid malloc issues
    char trace_lines[MAX_TRACE_LINES][TRACE_LINE_SIZE];
    int trace_count;
} Core;

/* ============================================
 * MAIN MEMORY STRUCTURE
 * ============================================ */

typedef struct {
    uint32_t data[MAIN_MEM_SIZE];

    // Pending memory transaction
    bool pending;
    BusTransaction pending_transaction;
    int cycles_remaining;     // For multi-cycle block transfers
    int words_sent;           // Words already sent in block
} MainMemory;

/* ============================================
 * BUS ARBITER STRUCTURE
 * ============================================ */

typedef struct {
    BusTransaction current;       // Current bus signals (updated every cycle)
    int last_granted;             // Last core that was granted access (for round-robin)

    // Bus Transaction Control
    int owner;                    // Current transaction owner (0-3: core, 4: memory, -1: none)
    BusState state;               // Current bus state
    int timer;                    // Cycles remaining in current state
    int provider_id;              // Who is providing the data (0-3: core, 4: memory)
    bool upgrade_only;            // True if BusRdX is a silent upgrade (1 cycle)
    bool shared_at_request;       // Shared bit detected during Request cycle
    
    // Data transfer state
    uint32_t flush_block_addr;    // Base address of block being transferred
    uint32_t flush_data[8];       // Block data for Flush
    int words_transferred;

    // Pending transactions waiting for bus
    bool pending[NUM_CORES];
    BusTransaction pending_trans[NUM_CORES];
    uint64_t request_time[NUM_CORES]; 

    // Bus trace output - fixed size buffer to avoid malloc issues
    char trace_lines[MAX_TRACE_LINES][TRACE_LINE_SIZE];
    int trace_count;
} BusArbiter;

/* ============================================
 * SIMULATOR STATE
 * ============================================ */

typedef struct {
    Core cores[NUM_CORES];
    MainMemory main_memory;
    BusArbiter bus;
    uint64_t global_cycle;
    bool running;
} Simulator;

/* ============================================
 * FUNCTION DECLARATIONS
 * ============================================ */

// Initialization
void init_simulator(Simulator *sim);
void init_core(Core *core, int core_id);
void init_cache(Cache *cache);
void init_main_memory(MainMemory *mem);
void init_bus_arbiter(BusArbiter *bus);

// Cache operations
bool cache_read(Cache* cache, uint32_t addr, uint32_t* data, Simulator* sim, int core_id);
bool cache_write(Cache* cache, uint32_t addr, uint32_t data, Simulator* sim, int core_id);

// Instruction operations
Instruction decode_instruction(uint32_t inst_word);
uint32_t encode_instruction(Instruction inst);
void print_instruction(Instruction inst, char *buffer);  // buffer must be at least 256 bytes
const char* get_opcode_name(uint8_t opcode);
bool is_branch_instruction(Instruction inst);
bool is_load_instruction(Instruction inst);
bool is_store_instruction(Instruction inst);
bool is_register_write(Instruction inst);
uint8_t get_dest_register(Instruction inst);

// Core operations
void execute_core_cycle(Core *core, Simulator *sim);
void stage_fetch(Core *core);
void stage_decode(Core *core);
void stage_execute(Core *core);
void stage_memory(Core *core, Simulator *sim);
void stage_writeback(Core *core, Simulator *sim);

// Register file operations
uint32_t read_register(Core *core, uint8_t reg, uint32_t imm_val);
void write_register(Core *core, uint8_t reg, uint32_t value);

// Cache operations
bool cache_read(Cache *cache, uint32_t addr, uint32_t *data, Simulator *sim, int core_id);
bool cache_write(Cache *cache, uint32_t addr, uint32_t data, Simulator *sim, int core_id);
void cache_snoop(Cache *cache, BusTransaction *trans, int core_id, Simulator *sim);
void cache_handle_bus_response(Cache *cache, BusTransaction *trans, int core_id, Simulator *sim);
// Bus operations
void bus_cycle(Simulator *sim);
void bus_request(BusArbiter *bus, int core_id, BusCommand cmd, uint32_t addr, uint32_t data);
void bus_arbitrate(BusArbiter *bus);
void add_bus_trace_entry(BusArbiter *bus, BusTransaction *trans, uint64_t cycle);

// Main memory operations
void memory_cycle(MainMemory *mem, BusTransaction *bus_trans, Simulator *sim);
uint32_t memory_read_word(MainMemory *mem, uint32_t addr);
void memory_write_word(MainMemory *mem, uint32_t addr, uint32_t data);
void memory_read_block(MainMemory *mem, uint32_t block_addr, uint32_t *block_data);
void memory_write_block(MainMemory *mem, uint32_t block_addr, uint32_t *block_data);

// File I/O
bool load_imem(const char *filename, uint32_t *imem);
bool load_memin(const char *filename, MainMemory *mem);
bool save_memout(const char *filename, MainMemory *mem);
bool save_regout(const char *filename, Core *core);
bool save_trace(const char *filename, Core *core);
bool save_bustrace(const char *filename, BusArbiter *bus);
bool save_dsram(const char *filename, Cache *cache);
bool save_tsram(const char *filename, Cache *cache);
bool save_stats(const char *filename, Core *core);
bool save_assembly(const char *filename, uint32_t *imem, int size);

// Simulation control
void run_simulator(Simulator *sim);
bool all_cores_halted(Simulator *sim);
bool all_pipelines_empty(Simulator *sim);

#endif // SIM_H

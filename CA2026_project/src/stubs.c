// Disable MSVC security warnings for standard C functions
#define _CRT_SECURE_NO_WARNINGS

#include "sim.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ============================================
 * FILE I/O FUNCTIONS
 * Functions for loading and saving simulation data
 * ============================================ */

// Note: Pipeline operations implemented in core.c
// Note: Cache operations implemented in cache.c
// Note: Bus operations implemented in bus.c

// Bus operations are implemented in bus.c

// Main memory operations are implemented in bus.c

// Helper to open input files from multiple potential locations
static FILE* open_input_file_robust(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (fp) return fp;

    // Try finding it in specific directories relative to CWD
    const char *prefixes[] = {"inputs/", "../inputs/", "../../inputs/", "../../../inputs/"};
    char path[512];

    for (int i = 0; i < 4; i++) {
        snprintf(path, sizeof(path), "%s%s", prefixes[i], filename);
        fp = fopen(path, "r");
        if (fp) {
            printf("Found input file at: %s\n", path);
            return fp;
        }
    }
    
    // Attempt to strip directory prefix if present in filename and try again
    const char *basename = strrchr(filename, '/');
    if (!basename) basename = strrchr(filename, '\\');
    
    if (basename) {
        basename++; // skip separator
        // Try prefixes with basename
        for (int i = 0; i < 4; i++) {
            snprintf(path, sizeof(path), "%s%s", prefixes[i], basename);
            fp = fopen(path, "r");
            if (fp) {
                printf("Found input file at: %s\n", path);
                return fp;
            }
        }
    }

    return NULL;
}

// File I/O (to be implemented)
bool load_imem(const char *filename, uint32_t *imem) {
    FILE *fp;
    char line[256];
    int address = 0;

    // Step 1: Open file for reading
    fp = open_input_file_robust(filename);
    if (!fp) {
        fprintf(stderr, "Error: Could not open %s for reading\n", filename);
        return false;
    }

    // Step 2: Read line-by-line
    while (fgets(line, sizeof(line), fp) != NULL && address < IMEM_SIZE) {
        uint32_t instruction;

        // Step 3: Parse hexadecimal value (8 hex digits = 32 bits)
        if (sscanf(line, "%x", &instruction) == 1) {
            // Step 4: Store in imem array
            imem[address] = instruction;
            address++;
        }
        // If sscanf fails, skip the line (could be empty or malformed)
    }

    // Step 5: Fill remaining addresses with zeros (already done by init)
    // but let's be explicit for clarity
    while (address < IMEM_SIZE) {
        imem[address] = 0;
        address++;
    }

    // Step 6: Close file
    fclose(fp);

    printf("Loaded %d instructions from %s\n", address, filename);
    return true;
}

bool load_memin(const char* filename, MainMemory* mem) {
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "ERROR: Failed to open %s for reading\n", filename);
        return false;
    }

    char line[12];
    int i = 0;
    while (fgets(line, sizeof(line), fp) && i < MAIN_MEM_SIZE) {
        // Parse hex value
        mem->data[i] = (uint32_t)strtoul(line, NULL, 16);
        i++;
    }
    
    fclose(fp);
    return true;
}
// Helper to handle output directory creation if writing to outputs/
static FILE* open_output_file_robust(const char *filename) {
    FILE *fp = NULL;

    // First, try to open the file with the full path as provided
    fp = fopen(filename, "w");
    if (fp) {
        return fp;
    }

    // If that fails, extract basename and try alternative locations
    const char *basename = strrchr(filename, '/');
    if (!basename) basename = strrchr(filename, '\\');
    if (basename) basename++; else basename = filename;

    // Try finding output directory in various locations
    const char *prefixes[] = {
        "../examples/example_061225_win/my_outputs/",
        "../outputs/",
        "../../outputs/",
        "../../../outputs/",
        "outputs/"
    };

    // Try opening file in each potential output directory location
    char path[512];
    for (int i = 0; i < 5; i++) {
        snprintf(path, sizeof(path), "%s%s", prefixes[i], basename);
        fp = fopen(path, "w");
        if (fp) {
            return fp;
        }
    }

    // Final fallback: try writing to current directory
    printf("Warning: Could not find output directory. Writing to CWD.\n");
    return fopen(basename, "w");
}

bool save_memout(const char *filename, MainMemory *mem) {
    FILE *fp;

    fp = open_output_file_robust(filename);
    if (!fp) {
        fprintf(stderr, "Error: Could not open %s for writing\n", filename);
        return false;
    }

    // Find last non-zero address for sparse memory output
    int last_addr = 0;
    for (int i = MAIN_MEM_SIZE - 1; i >= 0; i--) {
        if (mem->data[i] != 0) {
            last_addr = i;
            break;
        }
    }

    // Write only up to last non-zero address (minimum 64 words to match reference format)
    int write_count = (last_addr < 63) ? 64 : last_addr + 1;
    for (int i = 0; i < write_count; i++) {
        fprintf(fp, "%08X\n", mem->data[i]);
    }

    fclose(fp);
    return true;
}

bool save_regout(const char *filename, Core *core) {
    FILE *fp;

    // Open file for writing
    fp = open_output_file_robust(filename);
    if (!fp) {
        fprintf(stderr, "Error: Could not open %s for writing\n", filename);
        return false;
    }

    // Write registers R2 through R15 (skip R0 and R1)
    // R0 = always zero, R1 = immediate register
    for (int i = 2; i < NUM_REGISTERS; i++) {
        fprintf(fp, "%08X\n", core->registers[i]);
    }

    fclose(fp);
    return true;
}

bool save_trace(const char *filename, Core *core) {
    FILE *fp;

    fp = open_output_file_robust(filename);
    if (!fp) {
        fprintf(stderr, "Error: Could not open %s for writing\n", filename);
        return false;
    }

    // Write all buffered trace lines
    // These are generated during simulation in the pipeline code
    for (int i = 0; i < core->trace_count; i++) {
        if (i == 7) printf("DEBUG SAVE_TRACE: Idx 7 = %s\n", core->trace_lines[i]);
        fprintf(fp, "%s\n", core->trace_lines[i]);
    }

    fclose(fp);
    return true;
}

bool save_bustrace(const char *filename, BusArbiter *bus) {
    FILE *fp;

    fp = open_output_file_robust(filename);
    if (!fp) {
        fprintf(stderr, "Error: Could not open %s for writing\n", filename);
        return false;
    }

    // Write all buffered bus trace lines
    for (int i = 0; i < bus->trace_count; i++) {
        fprintf(fp, "%s\n", bus->trace_lines[i]);
    }

    fclose(fp);
    return true;
}

bool save_dsram(const char *filename, Cache *cache) {
    FILE *fp;

    fp = open_output_file_robust(filename);
    if (!fp) {
        fprintf(stderr, "Error: Could not open %s for writing\n", filename);
        return false;
    }

    // Write all 512 words of cache data (DSRAM)
    for (int i = 0; i < CACHE_SIZE; i++) {
        fprintf(fp, "%08X\n", cache->dsram[i]);
    }

    fclose(fp);
    return true;
}

bool save_tsram(const char *filename, Cache *cache) {
    FILE *fp;

    fp = open_output_file_robust(filename);
    if (!fp) {
        fprintf(stderr, "Error: Could not open %s for writing\n", filename);
        return false;
    }

    // Write all 64 TSRAM entries (tag + MESI state)
    // Format: bits[13:12] = MESI, bits[11:0] = tag, bits[31:14] = 0
    for (int i = 0; i < NUM_CACHE_BLOCKS; i++) {
        uint32_t tsram_word = 0;

        // Pack MESI state (2 bits) and tag (12 bits) into lower 14 bits
        tsram_word = ((uint32_t)cache->tsram[i].mesi_state << 12) |
                     (cache->tsram[i].tag & 0x0FFF);

        fprintf(fp, "%08X\n", tsram_word);
    }

    fclose(fp);
    return true;
}

bool save_stats(const char *filename, Core *core) {
    FILE *fp;

    fp = open_output_file_robust(filename);
    if (!fp) {
        fprintf(stderr, "Error: Could not open %s for writing\n", filename);
        return false;
    }

    // Write statistics in required format (name value pairs, decimal)
    fprintf(fp, "cycles %llu\n", core->cycles);
    fprintf(fp, "instructions %llu\n", core->instructions);
    fprintf(fp, "read_hit %llu\n", core->read_hit);
    fprintf(fp, "write_hit %llu\n", core->write_hit);
    fprintf(fp, "read_miss %llu\n", core->read_miss);
    fprintf(fp, "write_miss %llu\n", core->write_miss);
    fprintf(fp, "decode_stall %llu\n", core->decode_stall);
    fprintf(fp, "mem_stall %llu\n", core->mem_stall);

    fclose(fp);
    return true;
}

// Simulation control
void run_simulator(Simulator *sim) {
    printf("Running simulator...\n");

    // Run until all cores are halted and all pipelines are empty
    while (!all_cores_halted(sim) || !all_pipelines_empty(sim)) {
        // Execute bus cycle (arbitration and snooping)
        bus_cycle(sim);

        // Execute memory cycle (handle pending memory transactions)
        memory_cycle(&sim->main_memory, &sim->bus.current, sim);

        // Execute one cycle for each core
        for (int i = 0; i < NUM_CORES; i++) {
            execute_core_cycle(&sim->cores[i], sim);
        }

        // Increment global cycle counter AFTER executing
        // This ensures trace numbering starts at 0 while first fetch happens during cycle 1
        sim->global_cycle++;

        // Safety limit to prevent infinite loops during development
        if (sim->global_cycle > 100000) {
            printf("Warning: Simulation stopped after 100000 cycles\n");
            break;
        }
    }

    printf("Simulation complete\n");
}

bool all_cores_halted(Simulator *sim) {
    for (int i = 0; i < NUM_CORES; i++) {
        if (!sim->cores[i].halted) {
            return false;
        }
    }
    return true;
}

bool all_pipelines_empty(Simulator* sim) {
    for (int i = 0; i < NUM_CORES; i++) {
        Pipeline* p = &sim->cores[i].pipeline;
        // The simulator only exits when ALL these are false 
        if (p->fetch.valid || p->decode.valid || p->execute.valid ||
            p->mem.valid || p->writeback.valid) {
            return false;
        }
    }
    return true;
}

// Helper to format register name for assembly output
static void get_asm_reg_name(int reg, char *buffer) {
    if (reg == 0) {
        strcpy(buffer, "$zero");
    } else if (reg == 1) {
        strcpy(buffer, "$imm");
    } else {
        sprintf(buffer, "$r%d", reg);
    }
}

bool save_assembly(const char *filename, uint32_t *imem, int size) {
    FILE *fp;

    fp = open_output_file_robust(filename);
    if (!fp) {
        fprintf(stderr, "Error: Could not open %s for writing\n", filename);
        return false;
    }

    // Find last non-zero instruction
    int last_addr = 0;
    for (int i = size - 1; i >= 0; i--) {
        if (imem[i] != 0) {
            last_addr = i;
            break;
        }
    }

    char rd_str[16], rs_str[16], rt_str[16];

    for (int pc = 0; pc <= last_addr; pc++) {
        Instruction inst = decode_instruction(imem[pc]);
        
        get_asm_reg_name(inst.rd, rd_str);
        get_asm_reg_name(inst.rs, rs_str);
        get_asm_reg_name(inst.rt, rt_str);

        // Format matches imem0.asm: \t<op> <rd>, <rs>, <rt>, <imm>\t\t# PC=<pc>
        fprintf(fp, "\t%s %s, %s, %s, %d\t\t# PC=%d\n", 
                get_opcode_name(inst.opcode),
                rd_str, rs_str, rt_str, inst.imm,
                pc);
    }

    fclose(fp);
    return true;
}

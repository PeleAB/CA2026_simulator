// Disable MSVC security warnings for standard C functions
#define _CRT_SECURE_NO_WARNINGS

#include "sim.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

/* ============================================
 * FILE I/O FUNCTIONS
 * Functions for loading and saving simulation data
 * Per PDF spec: files are in the same directory as sim.exe
 * ============================================ */

bool load_imem(const char *filename, uint32_t *imem) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Error: Could not open %s for reading\n", filename);
        fprintf(stderr, "Note: When running without arguments, input files must be in the same directory as the executable.\n");
        return false;
    }

    char line[256];
    int address = 0;

    while (fgets(line, sizeof(line), fp) != NULL && address < IMEM_SIZE) {
        uint32_t instruction;
        if (sscanf(line, "%x", &instruction) == 1) {
            imem[address] = instruction;
            address++;
        }
    }

    // Fill remaining addresses with zeros
    while (address < IMEM_SIZE) {
        imem[address] = 0;
        address++;
    }

    fclose(fp);
    return true;
}

bool load_memin(const char *filename, MainMemory *mem) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Error: Could not open %s for reading\n", filename);
        return false;
    }

    char line[16];
    int i = 0;
    while (fgets(line, sizeof(line), fp) && i < MAIN_MEM_SIZE) {
        mem->data[i] = (uint32_t)strtoul(line, NULL, 16);
        i++;
    }

    fclose(fp);
    return true;
}

bool save_memout(const char *filename, MainMemory *mem) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Error: Could not open %s for writing\n", filename);
        return false;
    }

    // Find last non-zero address
    int last_addr = 0;
    for (int i = MAIN_MEM_SIZE - 1; i >= 0; i--) {
        if (mem->data[i] != 0) {
            last_addr = i;
            break;
        }
    }

    // Write up to last non-zero address (minimum 1 line)
    for (int i = 0; i <= last_addr; i++) {
        fprintf(fp, "%08X\n", mem->data[i]);
    }

    fclose(fp);
    return true;
}

bool save_regout(const char *filename, Core *core) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Error: Could not open %s for writing\n", filename);
        return false;
    }

    // Write registers R2 through R15 (skip R0 and R1)
    for (int i = 2; i < NUM_REGISTERS; i++) {
        fprintf(fp, "%08X\n", core->registers[i]);
    }

    fclose(fp);
    return true;
}

bool save_trace(const char *filename, Core *core) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Error: Could not open %s for writing\n", filename);
        return false;
    }

    for (int i = 0; i < core->trace_count; i++) {
        fprintf(fp, "%s\n", core->trace_lines[i]);
    }

    fclose(fp);
    return true;
}

bool save_bustrace(const char *filename, BusArbiter *bus) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Error: Could not open %s for writing\n", filename);
        return false;
    }

    for (int i = 0; i < bus->trace_count; i++) {
        fprintf(fp, "%s\n", bus->trace_lines[i]);
    }

    fclose(fp);
    return true;
}

bool save_dsram(const char *filename, Cache *cache) {
    FILE *fp = fopen(filename, "w");
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
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Error: Could not open %s for writing\n", filename);
        return false;
    }

    // Write all 64 TSRAM entries (tag + MESI state)
    // Format: bits[13:12] = MESI, bits[11:0] = tag
    for (int i = 0; i < NUM_CACHE_BLOCKS; i++) {
        uint32_t tsram_word = ((uint32_t)cache->tsram[i].mesi_state << 12) |
                              (cache->tsram[i].tag & 0x0FFF);
        fprintf(fp, "%08X\n", tsram_word);
    }

    fclose(fp);
    return true;
}

bool save_stats(const char *filename, Core *core) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Error: Could not open %s for writing\n", filename);
        return false;
    }

    // Write statistics in required format (name value pairs, decimal)
    fprintf(fp, "cycles %" PRIu64 "\n", core->cycles);
    fprintf(fp, "instructions %" PRIu64 "\n", core->instructions);
    fprintf(fp, "read_hit %" PRIu64 "\n", core->read_hit);
    fprintf(fp, "write_hit %" PRIu64 "\n", core->write_hit);
    fprintf(fp, "read_miss %" PRIu64 "\n", core->read_miss);
    fprintf(fp, "write_miss %" PRIu64 "\n", core->write_miss);
    fprintf(fp, "decode_stall %" PRIu64 "\n", core->decode_stall);
    fprintf(fp, "mem_stall %" PRIu64 "\n", core->mem_stall);

    fclose(fp);
    return true;
}

/* ============================================
 * SIMULATION CONTROL
 * ============================================ */

void run_simulator(Simulator *sim) {
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

        sim->global_cycle++;

        // Safety limit to prevent infinite loops
        if (sim->global_cycle > 100000) {
            break;
        }
    }
}

bool all_cores_halted(Simulator *sim) {
    for (int i = 0; i < NUM_CORES; i++) {
        if (!sim->cores[i].halted) {
            return false;
        }
    }
    return true;
}

bool all_pipelines_empty(Simulator *sim) {
    for (int i = 0; i < NUM_CORES; i++) {
        Pipeline *p = &sim->cores[i].pipeline;
        if (p->fetch.valid || p->decode.valid || p->execute.valid ||
            p->mem.valid || p->writeback.valid) {
            return false;
        }
    }
    return true;
}

/* ============================================
 * ASSEMBLY OUTPUT (optional debug helper)
 * ============================================ */

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
    FILE *fp = fopen(filename, "w");
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

        fprintf(fp, "\t%s %s, %s, %s, %d\t\t# PC=%d\n",
                get_opcode_name(inst.opcode),
                rd_str, rs_str, rt_str, inst.imm,
                pc);
    }

    fclose(fp);
    return true;
}

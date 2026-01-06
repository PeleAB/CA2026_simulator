// Disable MSVC security warnings for standard C functions
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <string.h>
#include "sim.h"

// ====================================================================================
// REGISTER FILE OPERATIONS
// ====================================================================================

uint32_t read_register(Core *core, uint8_t reg_num, uint32_t imm_val) {
    if (reg_num == 0) return 0;
    if (reg_num == 1) return imm_val; // R1 contains sign-extended immediate of THIS instruction
    if (reg_num < NUM_REGISTERS) return core->registers[reg_num];
    return 0;
}

void write_register(Core *core, uint8_t reg_num, uint32_t value) {
    // PDF: R0 and R1 are special and cannot be written by software
    if (reg_num >= 2 && reg_num < NUM_REGISTERS) {
        core->registers[reg_num] = value;
    }
}

// ====================================================================================
// PIPELINE STAGES
// ====================================================================================

// Helper to check for data hazards
// Returns true if reg_index is written by any instruction in Ex, Mem, or WB stages
bool check_data_hazard(Core *core, int reg_index) {
    if (reg_index <= 1) return false; // R0 and R1 never cause hazards

    if (core->pipeline.execute.valid && core->pipeline.execute.reg_write && core->pipeline.execute.rw == reg_index) return true;
    if (core->pipeline.mem.valid && core->pipeline.mem.reg_write && core->pipeline.mem.rw == reg_index) return true;
    if (core->pipeline.writeback.valid && core->pipeline.writeback.reg_write && core->pipeline.writeback.rw == reg_index) return true;
    
    return false;
}

// Helper to resolve branch condition
static bool resolve_branch_condition(Core *core, Instruction inst, int32_t rs_val, int32_t rt_val) {
    bool take_branch = false;
    switch (inst.opcode) {
        case OP_BEQ:  take_branch = (rs_val == rt_val); break;
        case OP_BNE:  take_branch = (rs_val != rt_val); break;
        case OP_BLT:  take_branch = (rs_val < rt_val);  break;
        case OP_BGT:  take_branch = (rs_val > rt_val);  break;
        case OP_BLE:  take_branch = (rs_val <= rt_val); break;
        case OP_BGE:  take_branch = (rs_val >= rt_val); break;
        default: break;
    }
    return take_branch;
}

void stage_fetch(Core* core) {
    if (core->halted || core->halt_fetch) return;

    // Fetch happens if:
    // 1. Decode is ready to receive (not stalled) 
    //    AND
    // 2. The fetch register is currently empty (not valid), allowing one-instruction buffer
    if (!core->pipeline.decode.stall && !core->pipeline.fetch.valid) {
        if (core->pc < IMEM_SIZE) {
            core->pipeline.fetch.inst_word = core->imem[core->pc];
            core->pipeline.fetch.inst = decode_instruction(core->pipeline.fetch.inst_word);
            core->pipeline.fetch.pc = core->pc;
            core->pipeline.fetch.valid = true;
            core->pipeline.fetch.is_halt = (core->pipeline.fetch.inst.opcode == OP_HALT);

            if (core->pipeline.fetch.is_halt) {
                // Detected HALT in FETCH - purely for internal tracking if needed
            }

            // Target the next sequential instruction
            core->pc++;
        }
    }
}
// Stage 2: Instruction Decode
void stage_decode(Core* core) {
    PipelineReg *dec = &core->pipeline.decode; 
    PipelineReg *fet = &core->pipeline.fetch;
    // 1. Determine if we can accept a new instruction from Fetch
    // We can pull if ID is empty and Fetch has a valid instruction,
    // and Fetch isn't stalling (though Fetch never stalls internally).
    if (!dec->valid && fet->valid && !fet->stall) {
        dec->inst = fet->inst;
        dec->pc = fet->pc;
        dec->inst_word = fet->inst_word;
        dec->is_halt = fet->is_halt;
        dec->valid = true;
        fet->valid = false; 
    }

    if (dec->valid) {
        Instruction inst = dec->inst; 

        // Sign-extend immediate and update R1
        dec->imm_val = (uint32_t)inst.imm;

        // Check for hazards (Check RS and RT)
        if (check_data_hazard(core, inst.rs) || check_data_hazard(core, inst.rt)) {
            dec->internal_stall = true; 
            core->decode_stall++;
            return;
        }

        // Stricter Hazard Detection: 
        // 1. Branches and JAL use RD for target calculation!
        // 2. SW uses RD as the Data Source! (read in Execute)
        // We must check if RD is being written by a previous instruction.
        if ((is_branch_instruction(inst) || inst.opcode == OP_JAL || inst.opcode == OP_SW) && check_data_hazard(core, inst.rd)) {
            dec->internal_stall = true;
            core->decode_stall++;
            return;
        }

        // If we get here, the hazard is cleared
        dec->internal_stall = false;
        dec->rs_value = read_register(core, inst.rs, dec->imm_val);
        dec->rt_value = read_register(core, inst.rt, dec->imm_val);

        // 1. Resolve Conditional Branches 
        if (is_branch_instruction(inst)) {
            // Check condition using the values read from registers 
            if (resolve_branch_condition(core, inst, dec->rs_value, dec->rt_value)) {
                // PDF: Jump target is R[rd][9:0]
                uint32_t rd_val = read_register(core, inst.rd, dec->imm_val);
                core->branch_target = rd_val & 0x3FF;
                core->branch_pending = true;
            }
        } 
        
        // 2. Handle JAL (Jump and Link)
        else if (inst.opcode == OP_JAL) {
            // R15 gets the return address. Since there is a delay slot, 
            // the return address is PC + 2
            dec->alu_result = (dec->pc + 2) & 0x3FF; 
            dec->reg_write = true;
            dec->rw = 15; // JAL always writes to R15 

            // The Jump Target is the value in R[rd] bits 9:0 
            uint32_t rd_val = read_register(core, inst.rd, dec->imm_val);
            core->branch_target = rd_val & 0x3FF;
            core->branch_pending = true;
        }

        // 3. Handle Halt [cite: 65, 68]
        else if (inst.opcode == OP_HALT) {
            dec->is_halt = true;
            // PDF: Cancel instruction in Fetch (PC+1)
            fet->valid = false;
            core->halt_fetch = true;
        }
    }
}

// Stage 3: Execute
void stage_execute(Core *core) {
    Pipeline *p = &core->pipeline;

    // Pull from Decode?
    // We can pull if EXE is empty, Decode has a valid instruction,
    // and Decode isn't stalled by a hazard.
    if (!p->execute.valid && p->decode.valid && !p->decode.internal_stall) {
        p->execute.inst = p->decode.inst;
        p->execute.inst_word = p->decode.inst_word;
        p->execute.pc = p->decode.pc;
        p->execute.rs_value = p->decode.rs_value;
        p->execute.rt_value = p->decode.rt_value;
        p->execute.is_halt = p->decode.is_halt;
        p->execute.valid = true;
        p->decode.valid = false;
    }

    if (p->execute.valid) {
        Instruction inst = p->execute.inst;
        uint32_t rs_val = p->execute.rs_value;
        uint32_t rt_val = p->execute.rt_value;
        uint32_t result = 0;
        bool write_result = false;
        uint32_t sw_data = 0;

        if (inst.opcode == OP_SW) {
            // Sign-extend immediate for R1 calculation [cite: 21]
            uint32_t imm_val = (uint32_t)inst.imm;
            sw_data = read_register(core, inst.rd, imm_val); // Read RD (Data)
        }

        switch (inst.opcode) {
            case OP_ADD: result = rs_val + rt_val; write_result = true; break;
            case OP_SUB: result = rs_val - rt_val; write_result = true; break;
            case OP_AND: result = rs_val & rt_val; write_result = true; break;
            case OP_OR:  result = rs_val | rt_val; write_result = true; break;
            case OP_XOR: result = rs_val ^ rt_val; write_result = true; break;
            case OP_MUL: result = rs_val * rt_val; write_result = true; break;
            case OP_SLL: result = rs_val << (rt_val & 0x1F); write_result = true; break;
            case OP_SRA: result = (int32_t)rs_val >> (rt_val & 0x1F); write_result = true; break;
            case OP_SRL: result = rs_val >> (rt_val & 0x1F); write_result = true; break;
            case OP_JAL: result = (uint32_t)(p->execute.pc + 2); write_result = true; break;
            case OP_LW: result = rs_val + rt_val; write_result = true; break;
            case OP_SW: result = rs_val + rt_val; break;
            default: break;
        }

        p->execute.alu_result = result;
        p->execute.reg_write = write_result;
        p->execute.mem_data = sw_data;
        p->execute.rw = (inst.opcode == OP_JAL) ? 15 : inst.rd; 
    }
}

// Stage 4: Memory Access
// Stage 4: Memory Access
void stage_memory(Core* core, Simulator* sim) {
    Pipeline* p = &core->pipeline;
    
    // Capture state at start of cycle: 
    // If p->mem was valid BEFORE we potentially pull new work, then we are entering a "Retry" / "Stall" cycle.
    // If p->mem was invalid, any work we process in this function is "New".
    bool is_retry = p->mem.valid;

    // 1. Pull from Execute
    // This happens at the end of the clock cycle T.
    if (!p->mem.valid && p->execute.valid && !p->execute.stall) {
        p->mem = p->execute;
        p->execute.valid = false;

        // --- IMMEDIATE STALL EVALUATION ---
        // We must check for a cache miss the MOMENT the instruction enters MEM.
        // This prevents Write-Back from pulling it out at the start of Cycle T+1.
        Instruction inst = p->mem.inst;
        if (inst.opcode == 16 || inst.opcode == 17) { // LW or SW
            uint32_t addr = p->mem.alu_result;
            uint8_t index = (addr >> 3) & 0x3F;
            uint16_t tag = (addr >> 9) & 0xFFF;
            TSRAMEntry* entry = &core->cache.tsram[index];

            bool hit = (entry->valid && entry->tag == tag && entry->mesi_state != 0);

            // Special case: SW into a Shared block requires a BusRdX (Upgrade), so it's a "Miss"
            if (inst.opcode == 17 && hit && entry->mesi_state == 1) {
                hit = false;
            }

            p->mem.internal_stall = !hit;
        }
        else {
            p->mem.internal_stall = false;
        }
    }

    // 2. Process instruction currently in MEM
    if (p->mem.valid) {
        Instruction inst = p->mem.inst;
        if (inst.opcode == 16 || inst.opcode == 17) {
            uint32_t loaded_data;
            // This call triggers the actual bus request on the first cycle of a miss
            bool hit = (inst.opcode == 16) ?
                cache_read(&core->cache, p->mem.alu_result, &loaded_data, sim, core->core_id) :
                cache_write(&core->cache, p->mem.alu_result, p->mem.mem_data, sim, core->core_id);

            // Update Statistics (Only on first attempt)
            if (!is_retry) {
                if (inst.opcode == 16) { // LW
                    if (hit) core->read_hit++;
                    else core->read_miss++;
                } else { // SW
                    if (hit) core->write_hit++;
                    else core->write_miss++;
                }
            }

            if (hit) {
                if (inst.opcode == 16) p->mem.mem_data = loaded_data; // Capture data for WB
                p->mem.internal_stall = false; // Release the stall for next cycle
            }
            else {
                p->mem.internal_stall = true; // Keep stalling Write-Back
                core->mem_stall++;
            }
        }
    }
}
// Stage 5: Write Back
void stage_writeback(Core *core, Simulator *sim) {
    Pipeline *p = &core->pipeline;
    core->wb_reg_written = 0;
    core->pending_reg_write_addr = 0;

    // Pull from Mem
    // Check internal_stall directly to allow same-cycle unstall from Bus
    if (p->mem.valid && !p->mem.internal_stall) {
        // WB Latch Logic: If we just unstalled a LOAD, the data in p->mem.mem_data is STALE/INVALID
        // because it was latched from the previous cycle (when stalled).
        // We must re-read the cache to get the data that JUST arrived from the bus.
        if (p->mem.inst.opcode == OP_LW) {
            uint32_t fresh_data = 0;
            // Re-read cache (Guaranteed hit if bus just updated it)
            if (cache_read(&core->cache, p->mem.alu_result, &fresh_data, sim, core->core_id)) {
                p->mem.mem_data = fresh_data;
            }
        }

        p->writeback = p->mem;
        p->mem.valid = false;
    } else {
        p->writeback.valid = false;
    }

    if (p->writeback.valid) {
        Instruction inst = p->writeback.inst;
        if (p->writeback.reg_write) {
            uint32_t val = (inst.opcode == OP_LW) ? p->writeback.mem_data : p->writeback.alu_result;
            uint8_t dst = p->writeback.rw;
            
            if (dst != 0 && dst != 1) {
                core->pending_reg_write_addr = dst;
                core->pending_reg_write_val = val;
                core->wb_reg_written = dst;
            }
        }
        
        core->instructions++;
    }
}

// Log detailed cycle trace 
static void log_cycle_trace(Core *core) {
    if (core->trace_count >= MAX_TRACE_LINES) return;

    char *buffer = core->trace_lines[core->trace_count];
    int offset = 0;

    offset += sprintf(buffer + offset, "%llu ", core->cycles);

    if (core->pipeline.fetch.valid) {
        offset += sprintf(buffer + offset, "%03X ", core->pipeline.fetch.pc);
    } else if (!core->halted && !core->halt_fetch && core->pc < IMEM_SIZE) {
        // Fetch is idle or awaiting targets, show what is pending fetch
        offset += sprintf(buffer + offset, "%03X ", core->pc);
    } else {
        offset += sprintf(buffer + offset, "--- ");
    }

    if (core->pipeline.decode.valid) offset += sprintf(buffer + offset, "%03X ", core->pipeline.decode.pc);
    else offset += sprintf(buffer + offset, "--- ");

    if (core->pipeline.execute.valid) offset += sprintf(buffer + offset, "%03X ", core->pipeline.execute.pc);
    else offset += sprintf(buffer + offset, "--- ");

    if (core->pipeline.mem.valid) offset += sprintf(buffer + offset, "%03X ", core->pipeline.mem.pc);
    else offset += sprintf(buffer + offset, "--- ");

    if (core->pipeline.writeback.valid) offset += sprintf(buffer + offset, "%03X ", core->pipeline.writeback.pc);
    else offset += sprintf(buffer + offset, "--- ");

    // Registers
    for (int i = 2; i < NUM_REGISTERS; i++) {
        // Trace logic: R2-R15 values. 
        // Note: R1 is not tracked in the core trace per PDF [cite: 50]
        offset += sprintf(buffer + offset, "%08X ", core->registers[i]);
    }
    core->trace_count++;
}

// Execute one clock cycle
void execute_core_cycle(Core *core, Simulator *sim) {
    if (core->halted) return;

    Pipeline *p = &core->pipeline;

    // 1. WB pulls from MEM (from prev cycle)
    stage_writeback(core, sim);

    // 2. MEM pulls from EXE (from prev cycle)
    stage_memory(core, sim);

    // 3. EXE pulls from ID (from prev cycle)
    stage_execute(core);

    // 4. ID pulls from IF (from prev cycle)
    stage_decode(core);

    // 5. IF Stage
    stage_fetch(core);

    // Logging and updates at end of cycle

    // Logging and Global updates
    if (!core->halted) {
        log_cycle_trace(core);
        
        // Physical Register File update: happens at the END of the clock cycle
        if (core->pending_reg_write_addr >= 2) {
            core->registers[core->pending_reg_write_addr] = core->pending_reg_write_val;
        }
        
        core->cycles++;

        // Apply Branch Target at the end of the cycle
        if (core->branch_pending) {
            core->pc = core->branch_target;
            core->branch_pending = false;
        }

        // --- STALL PROPAGATION FOR NEXT CYCLE ---
        // MEM stage: propagate internal_stall to stall so WB knows not to pull
        p->mem.stall = p->mem.internal_stall;
        
        // EX stage: stalled if MEM has internal stall
        p->execute.stall = p->mem.internal_stall;
        bool exe_busy = p->execute.valid && p->execute.stall;
        
        // ID stage: stalled if EX is busy or has its own hazard
        p->decode.stall = exe_busy || p->decode.internal_stall;
        bool id_busy = p->decode.valid && p->decode.stall;
        
        // IF stage: stalled if ID is busy
        p->fetch.stall = id_busy;

        // Core Halt detection
        if (p->writeback.valid && p->writeback.is_halt) {
            core->halted = true;
        }
    }
}
#include <stdio.h>
#include <string.h>
#include "sim.h"

// Decode a 32-bit instruction word into its components
Instruction decode_instruction(uint32_t inst_word) {
    Instruction inst;
    // Format: [Op:8][Rd:4][Rs:4][Rt:4][Imm:12] [cite: 62]
    inst.opcode = (inst_word >> 24) & 0xFF;
    inst.rd = (inst_word >> 20) & 0x0F;
    inst.rs = (inst_word >> 16) & 0x0F;
    inst.rt = (inst_word >> 12) & 0x0F;

    // Sign-extend 12-bit immediate [cite: 21]
    int16_t imm = inst_word & 0xFFF;
    if (imm & 0x800) imm |= 0xF000; 
    inst.imm = imm;

    return inst;
}
// Encode an Instruction structure back into a 32-bit word
uint32_t encode_instruction(Instruction inst) {
    uint32_t word = 0;

    word |= ((uint32_t)inst.opcode & 0xFF) << 24;
    word |= ((uint32_t)inst.rd & 0x0F) << 20;
    word |= ((uint32_t)inst.rs & 0x0F) << 16;
    word |= ((uint32_t)inst.rt & 0x0F) << 12;
    word |= ((uint32_t)inst.imm & 0x0FFF);  // Only lower 12 bits

    return word;
}

// Get the name of an opcode
const char* get_opcode_name(uint8_t opcode) {
    switch (opcode) {
        case OP_ADD: return "add";
        case OP_SUB: return "sub";
        case OP_AND: return "and";
        case OP_OR: return "or";
        case OP_XOR: return "xor";
        case OP_MUL: return "mul";
        case OP_SLL: return "sll";
        case OP_SRA: return "sra";
        case OP_SRL: return "srl";
        case OP_BEQ: return "beq";
        case OP_BNE: return "bne";
        case OP_BLT: return "blt";
        case OP_BGT: return "bgt";
        case OP_BLE: return "ble";
        case OP_BGE: return "bge";
        case OP_JAL: return "jal";
        case OP_LW: return "lw";
        case OP_SW: return "sw";
        case OP_HALT: return "halt";
        default: return "unknown";
    }
}

// Print instruction in human-readable format
void print_instruction(Instruction inst, char *buffer) {
    const char *name = get_opcode_name(inst.opcode);

    // Buffer size for instruction strings (assume 256 bytes max)
    #define INST_BUFFER_SIZE 256

    // Different format for different instruction types
    switch (inst.opcode) {
        case OP_ADD:
        case OP_SUB:
        case OP_AND:
        case OP_OR:
        case OP_XOR:
        case OP_MUL:
        case OP_SLL:
        case OP_SRA:
        case OP_SRL:
            sprintf_s(buffer, INST_BUFFER_SIZE, "%s $r%d, $r%d, $r%d", name, inst.rd, inst.rs, inst.rt);
            break;

        case OP_BEQ:
        case OP_BNE:
        case OP_BLT:
        case OP_BGT:
        case OP_BLE:
        case OP_BGE:
            sprintf_s(buffer, INST_BUFFER_SIZE, "%s $r%d, $r%d, $r%d (target PC bits from rd)",
                    name, inst.rs, inst.rt, inst.rd);
            break;

        case OP_JAL:
            sprintf_s(buffer, INST_BUFFER_SIZE, "%s $r%d (R15 = ret addr, PC = rd[9:0])", name, inst.rd);
            break;

        case OP_LW:
            sprintf_s(buffer, INST_BUFFER_SIZE, "%s $r%d, MEM[$r%d + $r%d]", name, inst.rd, inst.rs, inst.rt);
            break;

        case OP_SW:
            sprintf_s(buffer, INST_BUFFER_SIZE, "%s MEM[$r%d + $r%d], $r%d", name, inst.rs, inst.rt, inst.rd);
            break;

        case OP_HALT:
            sprintf_s(buffer, INST_BUFFER_SIZE, "halt");
            break;

        default:
            sprintf_s(buffer, INST_BUFFER_SIZE, "unknown opcode %d", inst.opcode);
            break;
    }
}

// Check if instruction is a branch
bool is_branch_instruction(Instruction inst) {
    return (inst.opcode >= OP_BEQ && inst.opcode <= OP_BGE) ||
           inst.opcode == OP_JAL;
}

// Check if instruction reads from memory
bool is_load_instruction(Instruction inst) {
    return inst.opcode == OP_LW;
}

// Check if instruction writes to memory
bool is_store_instruction(Instruction inst) {
    return inst.opcode == OP_SW;
}

// Check if instruction writes to a register
bool is_register_write(Instruction inst) {
    // All instructions write to rd except: branches, sw, halt
    if (inst.opcode >= OP_BEQ && inst.opcode <= OP_BGE) return false;
    if (inst.opcode == OP_SW) return false;
    if (inst.opcode == OP_HALT) return false;

    // JAL writes to R15, not rd
    if (inst.opcode == OP_JAL) return true;

    return true;
}

// Get the register that will be written (if any)
uint8_t get_dest_register(Instruction inst) {
    if (inst.opcode == OP_JAL) {
        return 15;  // JAL writes to R15
    }
    if (is_register_write(inst)) {
        return inst.rd;
    }
    return 0;  // No destination
}

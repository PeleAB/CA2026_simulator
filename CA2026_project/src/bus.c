// Disable MSVC security warnings for standard C functions
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <string.h>
#include "sim.h"

static inline uint32_t get_block_base_addr(uint32_t addr) {
    return addr & ~0x7;
}

// ====================================================================================
// BUS ARBITER - Round-Robin Arbitration with 2-cycle latency
// ====================================================================================

void bus_request(BusArbiter *bus, int core_id, BusCommand cmd, uint32_t addr, uint32_t data) {
    if (core_id < 0 || core_id >= NUM_CORES) return;

    bus->pending_trans[core_id].origid = core_id;
    bus->pending_trans[core_id].cmd = cmd;
    bus->pending_trans[core_id].addr = addr;
    bus->pending_trans[core_id].data = data;
    bus->pending_trans[core_id].shared = false;
    bus->pending[core_id] = true;
    // Note: In some traces, the request happens during the MEM stage of cycle T.
    // The command appears on the bus at T+2.
}

void bus_arbitrate(BusArbiter *bus) {
    int start = (bus->last_granted + 1) % NUM_CORES; // Round-robin start point 
    for (int i = 0; i < NUM_CORES; i++) {
        int core_id = (start + i) % NUM_CORES;
        if (bus->pending[core_id]) {
            bus->owner = core_id;
            bus->last_granted = core_id; // Mandatory for fair RR 
            bus->current = bus->pending_trans[core_id];
            bus->pending[core_id] = false; // Clear request once granted
            return;
        }
    }
}
void bus_cycle(Simulator* sim) {
    BusArbiter* bus = &sim->bus;
    BusTransaction output = { 0 };
    output.cmd = BUS_NO_CMD;

    switch (bus->state) {
    case BUS_STATE_IDLE:
        bus->owner = -1;
        bus_arbitrate(bus);
        if (bus->owner != -1) {
            bus->state = BUS_STATE_ARBITRATE;
        }
        break;

    case BUS_STATE_ARBITRATE:
        // Fall through to REQUEST to compensate for execution order swap
        bus->state = BUS_STATE_REQUEST;
        // Fall through

    case BUS_STATE_REQUEST:
        output = bus->pending_trans[bus->owner];
        bus->provider_id = 4; // Default: Memory
        output.shared = false;

        // SNOOP: Other cores signal 'shared' and provide data if Modified
        for (int i = 0; i < 4; i++) {
            if (i != bus->owner) cache_snoop(&sim->cores[i].cache, &output, i, sim);
        }
        bus->shared_at_request = output.shared;
        add_bus_trace_entry(bus, &output, sim->global_cycle);

        if (bus->provider_id != 4) {
            bus->state = BUS_STATE_FLUSH;
            bus->timer = 8;
        }
        else {
            bus->state = BUS_STATE_LATENCY;
            bus->timer = 15; // Exact 16-cycle latency (current cycle + 15)
            uint32_t block_addr = output.addr & ~0x7;
            for (int j = 0; j < 8; j++) {
                bus->flush_data[j] = sim->main_memory.data[block_addr + j];
            }
        }
        break;

    case BUS_STATE_LATENCY:
        if (bus->timer > 0) {
            bus->timer--;
            break;
        }
        // Timer expired - transition to FLUSH and fall through to execute it immediately
        bus->state = BUS_STATE_FLUSH;
        bus->timer = 8;
        // Fall through to FLUSH state

    case BUS_STATE_FLUSH:
        output.cmd = BUS_FLUSH;
        output.origid = bus->provider_id;
        uint32_t base = bus->pending_trans[bus->owner].addr & ~0x7;
        int offset = 8 - bus->timer;
        output.addr = base + offset;
        output.data = bus->flush_data[offset];
        output.shared = bus->shared_at_request;

        add_bus_trace_entry(bus, &output, sim->global_cycle);

        // Parallel Memory Update
        if (bus->provider_id != 4) sim->main_memory.data[output.addr] = output.data;

        // Data Capture: Requester saves the word to its DSRAM
        for (int i = 0; i < 4; i++) {
            cache_handle_bus_response(&sim->cores[i].cache, &output, i, sim);
        }

        bus->timer--;
        if (bus->timer == 0) {
            bus->state = BUS_STATE_IDLE;
            bus->owner = -1;
        }
        break;
    }
}
void add_bus_trace_entry(BusArbiter *bus, BusTransaction *trans, uint64_t cycle) {
    if (trans == NULL || trans->cmd == BUS_NO_CMD) return;
    if (bus->trace_count >= MAX_TRACE_LINES) return;

    snprintf(bus->trace_lines[bus->trace_count], TRACE_LINE_SIZE,
             "%llu %d %d %06X %08X %d",
             (unsigned long long)cycle,
             trans->origid,
             (int)trans->cmd,
             trans->addr & 0xFFFFF,
             trans->data,
             trans->shared ? 1 : 0);

    bus->trace_count++;
}

// Memory utility functions
uint32_t memory_read_word(MainMemory *mem, uint32_t addr) {
    if (addr < MAIN_MEM_SIZE) return mem->data[addr];
    return 0;
}

void memory_write_word(MainMemory *mem, uint32_t addr, uint32_t data) {
    if (addr < MAIN_MEM_SIZE) mem->data[addr] = data;
}

void memory_read_block(MainMemory *mem, uint32_t block_addr, uint32_t *block_data) {
    for (int i = 0; i < CACHE_BLOCK_SIZE; i++) {
        uint32_t addr = block_addr + i;
        if (addr < MAIN_MEM_SIZE) block_data[i] = mem->data[addr];
        else block_data[i] = 0;
    }
}

void memory_write_block(MainMemory *mem, uint32_t block_addr, uint32_t *block_data) {
    for (int i = 0; i < CACHE_BLOCK_SIZE; i++) {
        uint32_t addr = block_addr + i;
        if (addr < MAIN_MEM_SIZE) mem->data[addr] = block_data[i];
    }
}

void memory_cycle(MainMemory *mem, BusTransaction *bus_trans, Simulator *sim) {
    // Parallel update is handled in bus_cycle
}

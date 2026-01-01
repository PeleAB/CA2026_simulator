#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sim.h"

void init_simulator(Simulator *sim) {
    memset(sim, 0, sizeof(Simulator));

    // Initialize all cores
    for (int i = 0; i < NUM_CORES; i++) {
        init_core(&sim->cores[i], i);
    }

    // Initialize main memory
    init_main_memory(&sim->main_memory);

    // Initialize bus arbiter
    init_bus_arbiter(&sim->bus);

    sim->global_cycle = 0;
    sim->running = true;
}

void init_core(Core *core, int core_id) {
    memset(core, 0, sizeof(Core));

    core->core_id = core_id;
    core->pc = 0;

    // Initialize registers
    // R0 is always 0, R1 is the immediate register (updated each instruction)
    for (int i = 0; i < NUM_REGISTERS; i++) {
        core->registers[i] = 0;
    }

    // Initialize instruction memory to zeros
    memset(core->imem, 0, sizeof(core->imem));

    // Initialize cache
    init_cache(&core->cache);

    // Initialize pipeline stages
    memset(&core->pipeline, 0, sizeof(Pipeline));

    // Initialize statistics
    core->cycles = 0;
    core->instructions = 0;
    core->read_hit = 0;
    core->write_hit = 0;
    core->read_miss = 0;
    core->write_miss = 0;
    core->decode_stall = 0;
    core->mem_stall = 0;

    core->halted = false;
    core->halt_fetch = false;
    core->wb_reg_written = 0;
    core->post_wb_reg_addr = 0;
    core->post_wb_reg_val = 0;
    core->pending_reg_write_addr = 0;
    core->pending_reg_write_val = 0;

    // Initialize trace buffer (fixed size, no malloc needed)
    core->trace_count = 0;
}

void init_cache(Cache *cache) {
    memset(cache, 0, sizeof(Cache));

    // Initialize DSRAM (data) to zeros
    memset(cache->dsram, 0, sizeof(cache->dsram));

    // Initialize TSRAM (tag + MESI state)
    for (int i = 0; i < NUM_CACHE_BLOCKS; i++) {
        cache->tsram[i].tag = 0;
        cache->tsram[i].mesi_state = MESI_INVALID;
        cache->tsram[i].valid = false;
    }

    cache->state = CACHE_IDLE;
    cache->pending_addr = 0;
    cache->pending_data = 0;
    cache->words_received = 0;
    cache->words_sent = 0;
}

void init_main_memory(MainMemory *mem) {
    memset(mem, 0, sizeof(MainMemory));

    // Initialize all memory to zeros
    memset(mem->data, 0, sizeof(mem->data));

    mem->pending = false;
    mem->cycles_remaining = 0;
    mem->words_sent = 0;
}

void init_bus_arbiter(BusArbiter *bus) {
    memset(bus, 0, sizeof(BusArbiter));

    // Initialize current transaction to no command
    bus->current.origid = 0;
    bus->current.cmd = BUS_NO_CMD;
    bus->current.addr = 0;
    bus->current.data = 0;
    bus->current.shared = false;

    bus->last_granted = NUM_CORES - 1;  // Start round-robin from core 0
    bus->owner = -1;
    bus->state = BUS_STATE_IDLE;
    bus->timer = 0;
    bus->provider_id = 4;
    bus->upgrade_only = false;
    bus->words_transferred = 0;

    // No pending transactions
    for (int i = 0; i < NUM_CORES; i++) {
        bus->pending[i] = false;
    }

    // Initialize trace buffer (fixed size, no malloc needed)
    bus->trace_count = 0;
}

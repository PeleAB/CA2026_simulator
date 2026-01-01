// Disable MSVC security warnings for standard C functions
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <string.h>
#include "sim.h"

// ====================================================================================
// CACHE ADDRESS PARSING UTILITIES
// ====================================================================================

static inline uint8_t get_cache_index(uint32_t addr) {
    return (addr >> 3) & 0x3F;  // Extract bits 8:3
}

static inline uint16_t get_cache_tag(uint32_t addr) {
    return (addr >> 9) & 0xFFF;  // Extract bits 20:9
}

static inline uint8_t get_block_offset(uint32_t addr) {
    return addr & 0x7;  // Extract bits 2:0
}

static inline uint32_t get_block_base_addr(uint32_t addr) {
    return addr & ~0x7;  // Clear lower 3 bits
}

static inline uint16_t get_dsram_index(uint8_t cache_index, uint8_t block_offset) {
    return (cache_index * CACHE_BLOCK_SIZE) + block_offset;
}

static inline uint32_t get_addr_from_tag_index(uint16_t tag, uint8_t index) {
    return (tag << 9) | (index << 3);
}

// ====================================================================================
// CACHE OPERATIONS (Transitions and Requests)
// ====================================================================================

bool cache_read(Cache* cache, uint32_t addr, uint32_t* data, Simulator* sim, int core_id) {
    uint8_t index = get_cache_index(addr); 
    uint16_t tag = get_cache_tag(addr);
    uint8_t block_offset = get_block_offset(addr);
    TSRAMEntry* entry = &cache->tsram[index]; 

    // 1. Check for a Cache Hit
    if (entry->valid && entry->tag == tag && entry->mesi_state != MESI_INVALID) {
        uint16_t dsram_idx = index * CACHE_BLOCK_SIZE + block_offset;
        *data = cache->dsram[dsram_idx];
        return true;
    }

    // 2. Cache Miss: Handle Bus Transaction
    if (!sim->bus.pending[core_id] && sim->bus.owner != core_id) {
       
        // If we have a Modified block at this index, we must write it back first (Conflict Miss)
        if (entry->valid && entry->mesi_state == MESI_MODIFIED) {
          
            // In a real MESI system, you'd issue a Flush here. 
            // For this project, you can handle the write-back as a separate Bus transaction if needed.
        }

        // Issue the Bus Read (BusRd)
        sim->bus.pending_trans[core_id].cmd = 1; // 1: BusRd 
        sim->bus.pending_trans[core_id].addr = addr; 
            sim->bus.pending_trans[core_id].origid = core_id; 
            sim->bus.pending[core_id] = true; 
    }

    // Still a miss until the Bus finishes the 8-word Flush
    return false; 
}
bool cache_write(Cache* cache, uint32_t addr, uint32_t data, Simulator* sim, int core_id) {
    uint8_t index = get_cache_index(addr);
    uint16_t tag = get_cache_tag(addr);
    uint8_t block_offset = get_block_offset(addr);
    TSRAMEntry* entry = &cache->tsram[index];

    // Hit only if we already "Own" the block (Modified or Exclusive) 
    if (entry->valid && entry->tag == tag &&
        (entry->mesi_state == 3 || entry->mesi_state == 2)) {
        uint16_t dsram_idx = index * CACHE_BLOCK_SIZE + block_offset;
        cache->dsram[dsram_idx] = data;
        entry->mesi_state = 3; // Move to Modified 
        return true;
    }

    // Miss or Shared: Must issue a full BusRdX (command 2) [cite: 48, 53]
    if (!sim->bus.pending[core_id] && sim->bus.owner != core_id) {
        sim->bus.pending_trans[core_id].cmd = 2; // BusRdX [cite: 48]
        sim->bus.pending_trans[core_id].addr = addr;
        sim->bus.pending_trans[core_id].origid = core_id;
        sim->bus.pending[core_id] = true;
    }

    return false; // Always stall until the Flush arrives [cite: 37]
}
// ====================================================================================
// SNOOPING AND RESPONSE HANDLING
// ====================================================================================

void cache_snoop(Cache* cache, BusTransaction* trans, int core_id, Simulator* sim) {
    uint8_t index = get_cache_index(trans->addr);
    uint16_t tag = get_cache_tag(trans->addr);
    TSRAMEntry* entry = &cache->tsram[index];

    if (!entry->valid || entry->tag != tag) return; // Miss - don't have this block

    if (trans->cmd == 1) { // BusRd
        if (entry->mesi_state == 3) { // Modified -> Shared
            // This cache must provide the data
            sim->bus.provider_id = core_id;
            // Calculate proper DSRAM offset for this cache block
            uint16_t dsram_base = index * CACHE_BLOCK_SIZE;
            for (int i = 0; i < CACHE_BLOCK_SIZE; i++) {
                sim->bus.flush_data[i] = cache->dsram[dsram_base + i];
            }
            entry->mesi_state = 1; // Transition to Shared
            trans->shared = 1;
        }
        else if (entry->mesi_state == 2) { // Exclusive -> Shared
            entry->mesi_state = 1;
            trans->shared = 1;
        }
        else if (entry->mesi_state == 1) { // Shared -> Shared
            trans->shared = 1;
        }
    }
    else if (trans->cmd == 2) { // BusRdX
        if (entry->mesi_state == 3) { // Modified -> Invalid
            // This cache must provide the data
            sim->bus.provider_id = core_id;
            uint16_t dsram_base = index * CACHE_BLOCK_SIZE;
            for (int i = 0; i < CACHE_BLOCK_SIZE; i++) {
                sim->bus.flush_data[i] = cache->dsram[dsram_base + i];
            }
        }
        // All states (M, E, S) -> Invalid
        entry->mesi_state = 0;
        entry->valid = false;
    }
}

// ====================================================================================
// BUS RESPONSE HANDLING
// ====================================================================================

void cache_handle_bus_response(Cache* cache, BusTransaction* trans, int core_id, Simulator* sim) {
    if (trans->cmd != 3) return; // Only care about BUS_FLUSH

    if (sim->bus.owner == core_id) {
        // Calculate proper DSRAM index for this word
        uint8_t index = get_cache_index(trans->addr);
        uint8_t block_offset = get_block_offset(trans->addr);
        uint16_t dsram_idx = index * CACHE_BLOCK_SIZE + block_offset;
        cache->dsram[dsram_idx] = trans->data;

        // Finalize block on the 8th word (offset 7)
        if (block_offset == 7) {
            uint16_t tag = get_cache_tag(trans->addr);
            cache->tsram[index].tag = tag;
            cache->tsram[index].valid = true;
            // Set final MESI state based on the requester's command
            if (sim->bus.pending_trans[core_id].cmd == 1) {
                cache->tsram[index].mesi_state = sim->bus.shared_at_request ? 1 : 2;
            }
            else {
                cache->tsram[index].mesi_state = 3;
            }
            // Release stall when block is complete
            sim->cores[core_id].pipeline.mem.internal_stall = false;
        }
    }
}
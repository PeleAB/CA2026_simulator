# Computer Architecture Project Requirements
**Course:** 0512.4461 - Computer Architecture
**Academic Year:** 2025-2026, Semester A
**Tel Aviv University - Faculty of Engineering**

---

## Project Overview

Implement a simulator for a **4-core pipelined processor** with:
- 4 cores running in parallel
- Each core has a private SRAM instruction memory
- Each core has a private data cache
- Cores connected via BUS using **MESI coherency protocol** to shared main memory

---

## Architecture Diagram

```
┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐
│   Core 0    │  │   Core 1    │  │   Core 2    │  │   Core 3    │
│   IMEM 0    │  │   IMEM 1    │  │   IMEM 2    │  │   IMEM 3    │
│   Cache 0   │  │   Cache 1   │  │   Cache 2   │  │   Cache 3   │
└──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘
       │                │                │                │
       └────────────────┴────────────────┴────────────────┘
                              │
                    ┌─────────▼─────────┐
                    │  Bus (MESI cache  │
                    │ coherency protocol)│
                    └─────────┬─────────┘
                              │
                    ┌─────────▼─────────┐
                    │   Main Memory     │
                    └───────────────────┘
```

---

## 1. REGISTERS

### Register File (per core)
- **16 registers**, each **32 bits wide**
- **R0**: Always contains **zero** (writes to R0 are ignored)
- **R1**: Special **read-only immediate register**
  - Cannot be written to
  - Always contains the sign-extended immediate field from the current instruction
  - Updated automatically during instruction decode
- **R2-R15**: General purpose registers

---

## 2. INSTRUCTION MEMORY

### Per-Core IMEM
- **Private SRAM** for each core
- **Width**: 32 bits (1 instruction per word)
- **Depth**: 1024 rows (addresses 0-1023)
- **PC (Program Counter)**: 10 bits wide
- Sequential instructions increment PC by 1
- **Single-cycle access** from IMEM

---

## 3. PIPELINE

### 5-Stage Pipeline
```
┌───────┬────────┬─────────┬──────┬────────────┐
│ Fetch │ Decode │ Execute │ Mem  │ Write Back │
└───────┴────────┴─────────┴──────┴────────────┘
```

### Key Pipeline Characteristics

#### No Bypassing/Forwarding
- **NO BYPASSING** is used in this pipeline
- Data hazards resolved by **stalling only**

#### Register File Access
- Unlike MIPS with half-cycle register access:
  - **Full clock cycle** for register access
  - **3 reads + 1 write per cycle** maximum
  - Data written in cycle N is **only visible starting in cycle N+1**

#### Branch Resolution
- **Branch resolution** happens in the **Decode stage**
- Uses **branch delay slot** (next instruction after branch always executes)

#### Instruction Fetch
- Instructions read from private IMEM in **single cycle**

#### Data Hazard Handling
- Data hazards between instructions resolved by **stalling the dependent instruction in Decode stage**

#### Cache Miss Handling
- Cache miss causes **stall in Mem stage** for the instruction that accessed cache

---

## 4. DATA CACHE

### Cache Configuration (per core)
- **Type**: Direct-mapped cache
- **Size**: 512 words total
- **Block size**: 8 words
- **Number of sets**: 64 (512 / 8 = 64)
- **Hit time**: 1 cycle
- **Write policy**: Write-back, write-allocate

### Cache Implementation - Two SRAMs

#### DSRAM (Data Storage)
- **Width**: 32 bits
- **Depth**: 512 words
- Stores actual cache data

#### TSRAM (Tag + State Storage)
- **Depth**: 64 rows (one per cache block)
- **Width**: 14 bits per row

**TSRAM Entry Format:**
```
Bits  13:12        |  11:0
-------------------------------------
MESI State        |  Tag
(2 bits)          |  (12 bits)
-------------------------------------
0: Invalid        |
1: Shared         |
2: Exclusive      |
3: Modified       |
```

### Address Breakdown (21-bit data address)
### Address Breakdown (21-bit data address)
```
Bits  20:9        |  8:3          |  2:0
--------------------------------------------------
Tag (12 bits)    | Set (6 bits)  | Word offset (3)
--------------------------------------------------
```

**Note:** Set index uses bits [8:3] for 64 sets, word offset is [2:0]

### Initial State
- At start of simulation, both DSRAM and TSRAM are **zeroed**

---

## 5. MAIN MEMORY AND BUS

### Main Memory
- **Address space**: 21 bits (word addresses only, **no byte addressing**)
- **Size**: 2^21 words (2,097,152 words = 8 MB)

### Bus Signals

| Signal       | Width  | Description                                                      |
|--------------|--------|------------------------------------------------------------------|
| bus_origid   | 3 bits | Transaction originator: 0-3=cores, 4=main memory                |
| bus_cmd      | 2 bits | Command: 0=none, 1=BusRd, 2=BusRdX, 3=Flush                     |
| bus_addr     | 21 bits| Word address                                                     |
| bus_data     | 32 bits| Word data                                                        |
| bus_shared   | 1 bit  | Set to 1 if any core has data (for BusRd response)              |

### Bus Arbitration
- **One transaction per cycle** on the bus
- **Round-robin fair arbitration**: last core granted access goes to back of queue
- Bus access **NOT granted** if:
  - Bus is busy with current transaction
  - Previous BusRd or BusRdX hasn't completed with its Flush

### Bus Transactions

#### BusRd (Read)
1. Requesting core issues BusRd with address
2. Core initializes `bus_shared = 0`
3. All caches **snoop**: if they have the block, set `bus_shared = 1`
4. Data provider (memory or cache in Modified state) sends block via Flush
5. Requesting core sets state to:
   - **Exclusive** if `bus_shared = 0`
   - **Shared** if `bus_shared = 1`

#### BusRdX (Read Exclusive - for writes)
1. Requesting core issues BusRdX with address
2. Data provider (memory or Modified cache) sends block via Flush (16+ cycles)
3. All other caches invalidate their copies
4. Requesting core sets state to **Modified**


#### Flush (Write-back)
1. Sends data block back to memory or another cache
2. 8-word block transferred over 8 consecutive cycles:
   - **First word**: available after **16 cycles** from BusRd/BusRdX
   - **Subsequent words**: 1 per cycle (cycles 17, 18, 19, 20, 21, 22, 23)
3. Updates main memory in parallel
4. `bus_addr` contains word address
5. `bus_data` carries the data

---

## 6. MESI PROTOCOL

### MESI States
- **0 = Invalid (I)**: Block not in cache
- **1 = Shared (S)**: Block in cache, may be in other caches, matches memory
- **2 = Exclusive (E)**: Block only in this cache, matches memory
- **3 = Modified (M)**: Block only in this cache, **differs from memory**

### Data Supply Rules

**Important:** Only a cache with the block in **Modified** state can supply data via Flush. Caches with **Shared** or **Exclusive** do NOT supply data.

- On **BusRd / BusRdX**: If any cache has block in **Modified** → that cache supplies data via Flush
- On **BusRd / BusRdX**: If no cache has Modified (only S/E/I) → **Main memory** supplies data (16-cycle latency)
- When Modified cache supplies data, main memory updates simultaneously from the Flush

### MESI State Transitions

#### On Cache Read Miss
1. Issue **BusRd**
2. Snoop: other caches check and set `bus_shared`
3. Receive block via Flush
4. Set state to **Exclusive** (if alone) or **Shared** (if others have it)

#### On Cache Write Hit
- **Exclusive** → **Modified**: just write (silent upgrade, no bus transaction)
- **Shared** → **Modified**: issue **BusRdX** (Wait for Flush/Response - treats as miss), others invalidate
- **Modified** → **Modified**: just write (no bus transaction)

#### On Cache Write Miss
1. Issue **BusRdX**
2. Other caches invalidate their copies
3. Receive block via Flush
4. Write data, set state to **Modified**

#### On Snooping BusRd from another core
- **Modified** → **Shared**: send Flush, update memory
- **Exclusive** → **Shared**: set shared signal
- **Shared** → **Shared**: set shared signal

#### On Snooping BusRdX from another core
- **Modified** → **Invalid**: send Flush, invalidate
- **Exclusive** → **Invalid**: invalidate
- **Shared** → **Invalid**: invalidate

### Cache Block Eviction (Write-Back)

When a cache miss requires evicting a **Modified** block, two sequential bus transactions occur:

1. **Flush dirty block:** Write the 8 words of the Modified block to main memory (8 cycles on bus)
2. **Request new block:** Issue BusRd or BusRdX for the new block (1 cycle)
3. **Wait for response:** 16 cycles latency if from memory
4. **Receive new block:** 8 Flush cycles for the new block

**Total eviction + fetch time:** 8 + 1 + 16 + 8 = **33 cycles minimum** (if new block comes from memory)

---

## 7. INSTRUCTION SET

### Instruction Format (32 bits)
```
Bits    31:24     | 23:20 | 19:16 | 15:12 |  11:0
------------------------------------------------------
       Opcode    |  rd   |  rs   |  rt   | immediate
------------------------------------------------------
```

### Opcodes and Operations

| Opcode | Name | Operation | Description |
|--------|------|-----------|-------------|
| 0 | add | R[rd] = R[rs] + R[rt] | Addition |
| 1 | sub | R[rd] = R[rs] - R[rt] | Subtraction |
| 2 | and | R[rd] = R[rs] & R[rt] | Bitwise AND |
| 3 | or | R[rd] = R[rs] \| R[rt] | Bitwise OR |
| 4 | xor | R[rd] = R[rs] ^ R[rt] | Bitwise XOR |
| 5 | mul | R[rd] = R[rs] * R[rt] | Multiplication |
| 6 | sll | R[rd] = R[rs] << R[rt] | Shift left logical |
| 7 | sra | R[rd] = R[rs] >> R[rt] | Shift right arithmetic (sign extend) |
| 8 | srl | R[rd] = R[rs] >> R[rt] | Shift right logical |
| 9 | beq | if (R[rs] == R[rt]) pc = R[rd][9:0] | Branch if equal |
| 10 | bne | if (R[rs] != R[rt]) pc = R[rd][9:0] | Branch if not equal |
| 11 | blt | if (R[rs] < R[rt]) pc = R[rd][9:0] | Branch if less than |
| 12 | bgt | if (R[rs] > R[rt]) pc = R[rd][9:0] | Branch if greater than |
| 13 | ble | if (R[rs] <= R[rt]) pc = R[rd][9:0] | Branch if less or equal |
| 14 | bge | if (R[rs] >= R[rt]) pc = R[rd][9:0] | Branch if greater or equal |
| 15 | jal | R[15] = PC + 1, pc = R[rd][9:0] | Jump and link |
| 16 | lw | R[rd] = MEM[R[rs] + R[rt]] | Load word |
| 17 | sw | MEM[R[rs] + R[rt]] = R[rd] | Store word |
| 20 | halt | Halt this core | Halt instruction |

**Note**: Branches use low 10 bits of R[rd] as target PC

### Branch Behavior and Delay Slot

This architecture implements a delay slot mechanism for branch instructions:
- The instruction immediately following a branch is **always executed**, regardless of whether the branch is taken
- Branch resolution occurs in the **Decode stage**
- If a branch instruction appears in the delay slot, it executes normally (compiler responsibility to avoid issues)
- Branch target is taken from the lower 10 bits of R[rd]

#### Branch with Data Hazard and Delay Slot

When a branch instruction has a data dependency:
- The branch **stalls in DECODE stage** waiting for the dependency to resolve
- While the branch stalls, **FETCH also stalls** (stall propagation rule)
- The delay slot instruction is **not yet fetched** while the branch is stalling
- Once the branch unstalls and moves to EX, the delay slot instruction is fetched
- The delay slot executes regardless of branch outcome

### HALT Instruction Behavior

**Important:** HALT does NOT have a delay slot. Its behavior is different from branch instructions:
- HALT is detected in the **DECODE stage**
- The instruction currently in FETCH (PC+1) is **cancelled/discarded** - it will NOT execute
- HALT continues flowing through the pipeline: DECODE → EX → MEM → WB
- The pipeline drains with HALT being the last instruction
- After HALT completes WB, the core stops executing new instructions
- The core **continues snooping the bus** for cache coherence even after halting

**Example pipeline trace showing HALT at PC=008:**
```
Cycle 801: IF=009 ID=008(HALT) EX=007 MEM=006 WB=005
Cycle 802: IF=--- ID=---      EX=008 MEM=007 WB=006  (009 cancelled)
Cycle 803: IF=--- ID=---      EX=--- MEM=008 WB=007
Cycle 804: IF=--- ID=---      EX=--- MEM=--- WB=008  (HALT completes)
```

---

## 8. SIMULATOR IMPLEMENTATION

### Language and Environment
- **Language**: C
- **Environment**: Windows, Visual Studio Community Edition
- Must compile via **Build Solution** in Visual Studio

### Command Line Interface

```bash
sim.exe imem0.txt imem1.txt imem2.txt imem3.txt memin.txt memout.txt \
        regout0.txt regout1.txt regout2.txt regout3.txt \
        core0trace.txt core1trace.txt core2trace.txt core3trace.txt \
        bustrace.txt \
        dsram0.txt dsram1.txt dsram2.txt dsram3.txt \
        tsram0.txt tsram1.txt tsram2.txt tsram3.txt \
        stats0.txt stats1.txt stats2.txt stats3.txt
```

**Total: 27 arguments**

Alternative: Run with **no arguments** uses default filenames from same directory.

### Simulator Execution
- All cores start at **PC = 0**
- All pipeline stages are empty (---) at start
- All DSRAM and TSRAM are zeroed at start
- First Fetch occurs at cycle 1
- All four cores run simultaneously and independently
- When HALT reaches DECODE, the instruction in FETCH is cancelled
- HALT flows through to WB, then core stops
- Core continues snooping after halt
- Simulator exits when:
  - **All cores** have executed **HALT**, AND
  - **All pipelines are empty**

### Memory Output Behavior

**Important:** The simulator does **NOT** automatically flush Modified cache blocks at the end of simulation.

- `memout.txt` reflects the actual state of main memory
- If a block is Modified in cache but never written back, it will **NOT** appear in `memout.txt`
- The assembly program is **responsible** for forcing write-backs (e.g., via conflict misses)
- **This will be tested** - do not auto-flush at simulation end!

---

## 9. FILE FORMATS

### INPUT FILES

#### imem0.txt - imem3.txt (Instruction Memory)
- **Format**: Text file, one instruction per line
- **Encoding**: 8 hexadecimal digits (32-bit instruction)
- **Lines**: Up to 1024 lines (addresses 0-1023)
- **Unspecified addresses**: Filled with zeros
- **Example**:
```
00221001
00331001
0B021064
14000000
```

#### memin.txt (Main Memory Initial Contents)
- **Format**: Text file, one word per line
- **Encoding**: 8 hexadecimal digits (32-bit word)
- **Lines**: Up to 2^21 lines
- **Unspecified addresses**: Filled with zeros
- **Example**:
```
00000000
00000064
00000010
```

### OUTPUT FILES

#### memout.txt (Main Memory Final Contents)
- Same format as memin.txt
- Contains main memory state at end of simulation

#### regout0.txt - regout3.txt (Register File Final State)
- **Format**: 14 lines (R2 through R15 only - don't print R0 or R1)
- **Encoding**: 8 hexadecimal digits per line
- **Example**:
```
00000064
00000064
00000064
...
```

#### core0trace.txt - core3trace.txt (Pipeline Trace)
- **Format**: One line per clock cycle (only cycles where at least one stage is active)
- **Columns**: CYCLE FETCH DECODE EXEC MEM WB R2 R3 R4 ... R15

**Field Formats:**
- **CYCLE**: Decimal cycle number
- **FETCH-WB**: PC in 3 hex digits, or `---` if stage empty
- **R2-R15**: Register values at **start of cycle** (Q of flip-flops), 8 hex digits each

**Example**:
```
0 000 --- --- --- --- 00000000 00000000 00000000 ...
1 001 000 --- --- --- 00000000 00000000 00000000 ...
5 005 004 003 002 001 00000001 00000001 00000000 ...
```

#### bustrace.txt (Bus Transaction Trace)
- **Format**: One line per cycle where `bus_cmd != 0`
- **Columns**: CYCLE bus_origid bus_cmd bus_addr bus_data bus_shared

**Field Formats:**
- **CYCLE**: Decimal
- **bus_origid, bus_cmd, bus_shared**: 1 hex digit each
- **bus_addr**: 6 hex digits
- **bus_data**: 8 hex digits

**Example**:
```
5 2 1 00000F 00000000 0
21 4 3 000008 00000000 0
```

#### dsram0.txt - dsram3.txt (Cache Data Contents)
- **Format**: 512 lines (one per cache word)
- **Encoding**: 8 hexadecimal digits per line
- **Contains**: Final DSRAM contents

#### tsram0.txt - tsram3.txt (Cache Tag/State Contents)
- **Format**: 64 lines (one per cache block)
- **Encoding**: 8 hexadecimal digits per line (only lower 14 bits used)
- **Contains**: Final TSRAM contents (tag + MESI state)

**TSRAM Format** (per line):
```
Bits 13:12 = MESI state (0-3)
Bits 11:0 = Tag
Bits 31:14 = Unused (zeros)
```

#### stats0.txt - stats3.txt (Statistics)
- **Format**: Multiple lines with `name value` pairs
- **Value**: Decimal number

**Required Statistics:**
```
cycles X
instructions X
read_hit X
write_hit X
read_miss X
write_miss X
decode_stall X
mem_stall X
```

**Definitions:**
- **cycles**: Total clock cycles core was running until halt
- **instructions**: Total instructions executed (completed writeback)
- **read_hit**: Cache read hits
- **write_hit**: Cache write hits
- **read_miss**: Cache read misses
- **write_miss**: Cache write misses
- **decode_stall**: Cycles a stall was inserted in decode stage
- **mem_stall**: Cycles a stall was inserted in mem stage

---

## 10. DELIVERABLES

### Required Submissions

1. **Documentation PDF**: `project1_id1_id2_id3.pdf`
   - External to code
   - Explains project design and implementation

2. **Source Code**
   - Written in C
   - Well-commented explaining operation
   - Visual Studio Community Edition solution
   - Must build via "Build Solution"
   - Include build directory with executable

3. **Three Test Programs** (in assembly)

#### Test A: Counter Synchronization
- **Goal**: 4 cores increment shared counter in order
- **Location**: Memory address 0 (initialized to 0)
- **Each core**: Increments counter 128 times
- **Order**: Core 0, Core 1, Core 2, Core 3, repeat
- **Expected result**: Address 0 contains **512** (0x200)
- **Verification**: Force conflict miss to write final value back to memory

**Directory**: `counter/`
**Files**: All 28 files (sim.exe + input/output files)

#### Test B: Serial Matrix Multiplication
- **Goal**: 16×16 matrix multiplication on single core (core 0)
- **Matrix A**: Addresses 0x000 - 0x0FF (256 words)
- **Matrix B**: Addresses 0x100 - 0x1FF
- **Result C**: Addresses 0x200 - 0x2FF
- **Formula**: C[i][j] = Σ(A[i][k] * B[k][j]) for k=0 to 15
- **Memory layout**: Row-major order (like C language)
- **Assumption**: No overflow in multiplication

**Directory**: `mulserial/`
**Files**: All 28 files (sim.exe + input/output files)

#### Test C: Parallel Matrix Multiplication
- **Goal**: Same 16×16 matrix multiplication, but parallelized across 4 cores
- **Same matrices** as Test B
- **Distribution**: Each core computes part of result
- **Scoring**: Lower total execution time = higher grade

**Directory**: `mulparallel/`
**Files**: All 28 files (sim.exe + input/output files)

---

## 11. IMPORTANT IMPLEMENTATION NOTES

### Critical Requirements

1. **No bypassing in pipeline** - all hazards solved by stalling
2. **Branch delay slot** - instruction after branch always executes
3. **Register R0 always zero** - writes ignored
4. **Register R1 auto-updated** with sign-extended immediate each instruction
5. **Register writes visible next cycle** (not same cycle)
6. **Cache: write-back, write-allocate**
7. **MESI protocol must be correct** - critical for multi-core
8. **Round-robin bus arbitration** - fair scheduling
9. **Main memory latency**: 16 cycles first word, +1 per word after
10. **Halt only exits when all cores halted AND pipelines empty**

### Testing Strategy
- Test incrementally: single core → memory access → multi-core
- Verify against provided example files
- Test MESI transitions carefully
- Verify synchronization in counter test
- Check matrix multiplication correctness

### Common Pitfalls

**Critical mistakes to avoid:**

1. **Forgetting that R0 is always 0 and R1 is the sign-extended immediate**
2. **Auto-flushing Modified blocks at simulation end** (DO NOT do this)
3. **Incorrect MESI transitions:** only Modified supplies data, S/E do not
4. **Not properly implementing round-robin arbitration**
5. **Miscounting stall cycles vs. stall events** (should count cycles)
6. **Forgetting that halted cores must still snoop the bus**
7. **BusRdX from Shared:** requires only 1 cycle (no Flush), not 16+
8. **Counting cancelled instruction after HALT as executed**
9. **Not fetching delay slot when branch stalls on data hazard**
10. **Register writes take effect next cycle** (write in N → readable in N+1)
11. **Branch resolution in Decode with delay slot always executed**
12. **HALT has NO delay slot** - next instruction is cancelled

---

## 12. GRADING CRITERIA (Implied)

Based on requirements, grading likely considers:
- ✅ Correctness of simulator (matches expected outputs)
- ✅ Proper MESI protocol implementation
- ✅ Code quality and comments
- ✅ Documentation clarity
- ✅ Test program correctness:
  - Counter test: Must produce 512
  - Serial matrix multiply: Correct result
  - Parallel matrix multiply: Correct result + performance
- ✅ Performance of parallel implementation

---

## APPENDIX: Quick Reference

### Memory Address Breakdown (21 bits, for cache)
```
Assuming 64 sets, 8 words/block:
- Tag: bits [20:9] (12 bits)
- Set index: bits [8:3] (6 bits)
- Block offset: bits [2:0] (3 bits)
```

### Pipeline Stages Summary
1. **Fetch**: Read instruction from IMEM
2. **Decode**: Decode instruction, read registers, resolve branches, detect hazards
3. **Execute**: ALU operations
4. **Mem**: Cache access (lw/sw)
5. **WriteBack**: Write result to register file

### Key Timing Constraints
- Instruction fetch: **1 cycle**
- Cache hit: **1 cycle**
- Cache miss: **Variable** (depends on bus + memory latency)
- Register write-to-read: **1 cycle** delay
- Branch resolution: **In decode stage** (with delay slot)

---

**End of Requirements Document**

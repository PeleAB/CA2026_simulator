
import os

def write_hex(filename, lines):
    with open(filename, 'w') as f:
        for line in lines:
            f.write(line + '\n')

# Core 0: Backward Loop Test (Reproduce Failure)
# 0: ADDI R2, R2, 1  -> 00221001
# 1: SUBI R2, R2, 1  -> 01221001 (R2=0)
# 2: BEQ R2, R0, 0   -> 09200000 (Target=0. Rs=2. Rt=0. Imm=0).
#    *Decode: Inst.Rd=0. Target=0.* 
# 3: ADDI R3, R3, 1  -> 00331001 (Delay Slot).
# 4: HALT            -> 14000000
#
# Expected:
# Cycle 0: PC 0 (ADDI)
# Cycle 1: PC 1 (SUBI)
# Cycle 2: PC 2 (BEQ). Taken. Target=0.
# Cycle 3: PC 3 (Delay). Executed.
# Cycle 4: PC 0 (ADDI). (Back to start).
# We only want to run a few cycles to see the jump.

imem0 = [
    "00221001", 
    "01221001",
    "09200000",
    "00331001", 
    "14000000"
]

imem_halt = ["14000000"]
memin = ["00000000"] * 10
base_dir = "inputs/test_simple"

write_hex(os.path.join(base_dir, "imem0.txt"), imem0)
write_hex(os.path.join(base_dir, "imem1.txt"), imem_halt)
write_hex(os.path.join(base_dir, "imem2.txt"), imem_halt)
write_hex(os.path.join(base_dir, "imem3.txt"), imem_halt)
write_hex(os.path.join(base_dir, "memin.txt"), memin)
print("Test files created.")

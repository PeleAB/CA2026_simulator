
import os

def write_hex(filename, lines):
    with open(filename, 'w') as f:
        for line in lines:
            f.write(line + '\n')

# Core 0: Forward Branch Test
# 0: ADDI R3, R3, 2  -> 00331002 (Init R3=2)
# 1: ADDI R2, R2, 1  -> 00221001 (Init R2=1)
# 2: BLT  R2, R3, 5  -> 0B025000 ??? No.
#    Structure: [Op:8][Rd:4][Rs:4][Rt:4][Imm:12]
#    BLT: Op=11 (0B).
#    Rd: Target? Or Imm for target.
#    My FIX uses inst.imm.
#    Standard format: Imm is in last 12 bits.
#    Rs=2. Rt=3.
#    0B [Target?] 2 3 [Imm].
#    If I use inst.imm for Target.
#    Then Target must be in Imm.
#    Imm = 5.
#    0B 0 2 3 005.
#    0B023005.
# 3: ADDI R4, R4, 1  -> 00441001 (Delay Slot - Should Execute)
# 4: ADDI R6, R6, 1  -> 00661001 (Skipped)
# 5: ADDI R5, R5, 1  -> 00551001 (Target - Should Execute)
# 6: HALT            -> 14000000

imem0 = [
    "00331002",
    "00221001",
    "0B023005", # BLT R2, R3, Target=5
    "00441001", # Delay Slot
    "00661001", # Skipped
    "00551001", # Target
    "14000000"
]

imem_halt = ["14000000"]
memin = ["00000000"] * 10
base_dir = "inputs/test_forward"
if not os.path.exists(base_dir):
    os.makedirs(base_dir)

write_hex(os.path.join(base_dir, "imem0.txt"), imem0)
write_hex(os.path.join(base_dir, "imem1.txt"), imem_halt)
write_hex(os.path.join(base_dir, "imem2.txt"), imem_halt)
write_hex(os.path.join(base_dir, "imem3.txt"), imem_halt)
write_hex(os.path.join(base_dir, "memin.txt"), memin)
print("Test files created.")

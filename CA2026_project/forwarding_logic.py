# Helper function to get forwarded value
# This logic will be inlined or helper-ized in core.c

def get_forwarded_value(reg_num, core):
    p = core.pipeline
    
    # Priority: EX > MEM > WB (Most recent)
    
    # Check EX
    if p.execute.valid and p.execute.reg_write:
        dest = p.execute.inst.rd
        if p.execute.inst.opcode == OP_JAL: dest = 15
        
        if dest == reg_num and dest != 0:
            # Check Load-Use Hazard
            if p.execute.inst.opcode == OP_LW:
                return None # Stall needed (Data not ready until MEM)
            return p.execute.alu_result

    # Check MEM
    if p.mem.valid and p.mem.reg_write:
        dest = p.mem.inst.rd
        if p.mem.inst.opcode == OP_JAL: dest = 15
        
        if dest == reg_num and dest != 0:
            if p.mem.inst.opcode == OP_LW:
                return p.mem.mem_data # Loaded data available? YES (stage_memory runs before decode/exec)
            else:
                return p.mem.alu_result

    # Check WB (Current cycle write)
    if p.writeback.valid and p.writeback.reg_write:
        dest = core.wb_reg_written # Should match p.writeback.inst.rd/15
        if dest == reg_num and dest != 0:
             # Data is in p.writeback.alu_result or mem_data
             if p.writeback.inst.opcode == OP_LW:
                 return p.writeback.mem_data
             else:
                 return p.writeback.alu_result
                 
    return None # No forwarding found

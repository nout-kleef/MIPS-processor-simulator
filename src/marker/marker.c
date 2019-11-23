#include "../mipssim.h"
#include "../parser.h"
#include "../mipssim.c"
#include "../memory_hierarchy.c"

#define STATE_DUMP_FILE "states.txt"

FILE *fp = NULL;
bool first_dump = true;
int prev_cycle_regs[32];

char *get_state_name(const int state)
{
    switch (state)
    {
    case 0:
        return "INSTR_FETCH";
    case 1:
        return "DECODE";
    case 2:
        return "MEM_ADDR_COMP";
    case 3:
        return "MEM_ACCESS_LD";
    case 4:
        return "WB_STEP";
    case 5:
        return "MEM_ACCESS_ST";
    case 6:
        return "EXEC";
    case 7:
        return "R_TYPE_COMPL";
    case 8:
        return "BRANCH_COMPL";
    case 9:
        return "JUMP_COMPL";
    case 10:
        return "EXIT_STATE";
    case 11:
        return "I_TYPE_EXEC";
    case 12:
        return "I_TYPE_COMPL";
    default:
        assert(false);
    }
}

void dump_arch_state()
{
    fprintf(fp, "\n*** CYCLE #%u START ***\n", (unsigned int)arch_state.clock_cycle);
    fprintf(fp,
            "state:\t\t\t\t\t%s\n"
            "clock cycle\t\t\t\t%u\n"
            "instruction\t\t\t\t%u\n"
            "|\topcode (31-26)\t\t%d\n"
            "|\tjmp (25-0)\t\t\t%d\n"
            "|\t|\t$rs (25-21)\t\t%d\n"
            "|\t|\t$rt (20-16)\t\t%d\n"
            "|\t|\timm (15-0)\t\t%d\n"
            "|\t|\t|\t$rd (15-11)\t%d\n"
            "|\t|\t|\tfunct (5-0)\t%d\n",
            get_state_name(arch_state.state),
            (unsigned int)arch_state.clock_cycle,
            (unsigned int)arch_state.IR_meta.instr,
            arch_state.IR_meta.opcode,
            arch_state.IR_meta.jmp_offset,
            arch_state.IR_meta.reg_21_25,
            arch_state.IR_meta.reg_16_20,
            arch_state.IR_meta.immediate,
            arch_state.IR_meta.reg_11_15,
            arch_state.IR_meta.function);
    // registers
    fprintf(fp, "register file (updated only)\n");
    for (int i = 0; i < 32; ++i)
    {
        if (first_dump || arch_state.registers[i] != prev_cycle_regs[i])
        {
            fprintf(fp,
                    "|\treg %d:\t\t\t\t%d\n",
                    i,
                    arch_state.registers[i]);
        }
        prev_cycle_regs[i] = arch_state.registers[i];
    }
    // pipeline registers
    fprintf(fp,
            "pipe registers (current)\n"
            "|\tPC\t\t\t\t\t%d\n"
            "|\tIR\t\t\t\t\t%d\n"
            "|\tA\t\t\t\t\t%d\n"
            "|\tB\t\t\t\t\t%d\n"
            "|\tALUOut\t\t\t\t%d\n"
            "|\tMDR\t\t\t\t\t%d\n",
            arch_state.curr_pipe_regs.pc,
            arch_state.curr_pipe_regs.IR,
            arch_state.curr_pipe_regs.A,
            arch_state.curr_pipe_regs.B,
            arch_state.curr_pipe_regs.ALUOut,
            arch_state.curr_pipe_regs.MDR);
    fprintf(fp, "*** CYCLE ENDED ***\n\n");
}

static inline void marking_after_clock_cycle()
{
    if (fp == NULL)
    {
        fp = fopen(STATE_DUMP_FILE, "w");
        assert(fp != NULL);
        fprintf(fp, "file opened. starting marking process..\n"
                    "#######################################\n\n");
    }
    dump_arch_state();
    first_dump = false;
}

static inline void marking_at_the_end()
{
    dump_arch_state();
    fclose(fp);
}

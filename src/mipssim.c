/*************************************************************************************|
|   1. YOU ARE NOT ALLOWED TO SHARE/PUBLISH YOUR CODE (e.g., post on piazza or online)|
|   2. Fill main.c and memory_hierarchy.c files                                       |
|   3. Do not use any other .c files neither alter main.h or parser.h                 |
|   4. Do not include any other library files                                         |
|*************************************************************************************/

#include "mipssim.h"

// extend instruction types specified in mipssim.h
#define I_TYPE 2
#define J_TYPE 3

#define BREAK_POINT 200000 // exit after so many cycles -- useful for debugging

// Global variables
char mem_init_path[1000];
char reg_init_path[1000];

uint32_t cache_size = 0;
struct architectural_state arch_state;

static inline uint8_t get_instruction_type(int opcode)
{
    switch (opcode)
    {
    /// opcodes are defined in mipssim.h
    case SPECIAL: // ADD, SLT
        return R_TYPE;
    case ADDI:
        return I_TYPE;
    case LW:
        return I_TYPE;
    case SW:
        return I_TYPE;
    case BEQ:
        return I_TYPE;
    case J:
        return J_TYPE;
    case SLT:
        return R_TYPE;
    case EOP:
        return EOP_TYPE;
    default:
        assert(false && "invalid opcode in 'get_instruction_type'");
    }
    // assert(false);
}

void FSM()
{
    struct ctrl_signals *control = &arch_state.control;
    struct instr_meta *IR_meta = &arch_state.IR_meta;

    //reset control signals
    memset(control, 0, (sizeof(struct ctrl_signals)));

    int opcode = IR_meta->opcode;
    int state = arch_state.state;

    switch (state)
    {
    case INSTR_FETCH: // get the instruction from memory
        // update control
        arch_state.control.PCWrite = 1;  // prepare for PC+4
        arch_state.control.PCSource = 0; // ALU result
        arch_state.control.MemRead = 1;
        arch_state.control.IorD = 0; // PC
        arch_state.control.IRWrite = 1;
        arch_state.control.ALUOp = 0;   // add - TODO ???
        arch_state.control.ALUSrcA = 0; // PC
        arch_state.control.ALUSrcB = 1; // WORD_SIZE (4)
        // update state
        state = DECODE;
        break;
    case DECODE:
        // update control
        arch_state.control.ALUSrcA = 0; // PC+4
        arch_state.control.ALUSrcB = 3; // shifted (sign extended) immediate
        arch_state.control.ALUOp = 0;   // add - TODO ??? optimistically compute branch addr
        // update state
        switch (get_instruction_type(opcode))
        {
        case R_TYPE:
            state = EXEC;
            break;
        case I_TYPE:
            if (opcode == LW || opcode == SW)
            {
                state = MEM_ADDR_COMP;
            }
            else if (opcode == ADDI)
            {
                state = I_TYPE_EXEC;
            }
            else if (opcode == BEQ)
            {
                state = BRANCH_COMPL;
            }
            else
            {
                assert(false && "invalid opcode for I_TYPE in DECODE state");
            }
            break;
        case J_TYPE:
            state = JUMP_COMPL;
            break;
        case EOP_TYPE:
            state = EXIT_STATE;
            break;
        default:
            assert(false && "invalid instruction type");
        }
        break;
    case EXIT_STATE:
        // TODO ???
        break;
    case MEM_ADDR_COMP:
        // update control
        arch_state.control.ALUSrcA = 1; // pipe reg A ($rs)
        arch_state.control.ALUSrcB = 2; // (sign extended) immediate
        arch_state.control.ALUOp = 0;   // add - TODO ???
        // update state
        switch (opcode)
        {
        case LW:
            state = MEM_ACCESS_LD;
            break;
        case SW:
            state = MEM_ACCESS_ST;
            break;
        default:
            assert(false && "invalid opcode in MEM_ADDR_COMP");
        }
        break;
    case MEM_ACCESS_LD:
        // update control
        arch_state.control.MemRead = 1;
        arch_state.control.IorD = 1; // ALUout ($rs + immediate)
        // update state
        state = WB_STEP;
        break;
    case MEM_ACCESS_ST:
        // update control
        arch_state.control.MemWrite = 1;
        arch_state.control.IorD = 1; // ALUout ($rs + immediate)
        // update state
        state = INSTR_FETCH;
        break;
    case WB_STEP:
        // update control
        arch_state.control.RegDst = 0; // IR[16-20] ($rt)
        arch_state.control.RegWrite = 1;
        arch_state.control.MemtoReg = 1; // pipe reg MDR (mem[$rs + immediate])
        // update state
        state = INSTR_FETCH;
        break;
    case EXEC:
        // update control
        arch_state.control.ALUSrcA = 1; // pipe reg A ($rs)
        arch_state.control.ALUSrcB = 0; // pipe reg B ($rt)
        // TODO switch function
        arch_state.control.ALUOp = 2; // add - TODO ???
        // update state
        state = R_TYPE_COMPL;
        break;
    case R_TYPE_COMPL:
        // update control
        arch_state.control.RegDst = 1; // IR[11-15] ($rd)
        arch_state.control.RegWrite = 1;
        arch_state.control.MemtoReg = 0; // ALUOut (ADD: $rs + $rt, SLT: ???)
        // update state
        state = INSTR_FETCH;
        break;
    case I_TYPE_EXEC:
        // update control
        arch_state.control.ALUSrcA = 1; // pipe reg A ($rs)
        arch_state.control.ALUSrcB = 2; // (sign extended) immediate
        arch_state.control.ALUOp = 0;   // add - TODO ???
        // update state
        state = I_TYPE_COMPL;
        break;
    case I_TYPE_COMPL:
        // update control
        arch_state.control.RegDst = 0; // IR[16-20] ($rt)
        arch_state.control.RegWrite = 1;
        arch_state.control.MemtoReg = 0; // pipe reg ALUOut
        // update state
        state = INSTR_FETCH;
        break;
    case BRANCH_COMPL:
        // update control
        arch_state.control.PCWriteCond = 1; // AND this with "zero bit"
        arch_state.control.ALUSrcA = 1;     // pipe reg A ($rs)
        arch_state.control.ALUSrcB = 0;     // pipe reg B ($rt)
        arch_state.control.ALUOp = 1;       // subtract - TODO ???
        arch_state.control.PCSource = 1;    // pipe reg ALUOut (PC+4 + imm<<2)
        // update state
        state = INSTR_FETCH;
        break;
    case JUMP_COMPL:
        // update control
        arch_state.control.PCSource = 2; // PC+4[31-28] (+) target<<2
        arch_state.control.PCWrite = 1;
        // update state
        state = INSTR_FETCH;
        break;
    default:
        assert(false && "invalid state (current)");
    }

    arch_state.state = state;
}

void instruction_fetch()
{
    if (arch_state.control.MemRead)
    {
        assert(arch_state.control.IorD == 0 && "IorD should be 0");
        int address = arch_state.curr_pipe_regs.pc;
        arch_state.next_pipe_regs.IR = memory_read(address);
    }
}

void decode_and_read_RF()
{
    int read_register_1 = arch_state.IR_meta.reg_21_25;
    int read_register_2 = arch_state.IR_meta.reg_16_20;
    check_is_valid_reg_id(read_register_1);
    check_is_valid_reg_id(read_register_2);
    arch_state.next_pipe_regs.A = arch_state.registers[read_register_1];
    arch_state.next_pipe_regs.B = arch_state.registers[read_register_2];
}

void execute()
{
    struct ctrl_signals *control = &arch_state.control;
    struct instr_meta *IR_meta = &arch_state.IR_meta;
    struct pipe_regs *curr_pipe_regs = &arch_state.curr_pipe_regs;
    struct pipe_regs *next_pipe_regs = &arch_state.next_pipe_regs;

    int alu_opA = 0;
    int alu_opB = 0;
    int immediate = IR_meta->immediate;
    int shifted_immediate = (immediate) << 2;
    // ALUSrcA
    switch (control->ALUSrcA)
    {
    case 0:
        alu_opA = curr_pipe_regs->pc;
        break;
    case 1:
        alu_opA = curr_pipe_regs->A;
        break;
    default:
        assert(false && "invalid ALUSrcA");
    }
    // ALUSrcB
    switch (control->ALUSrcB)
    {
    case 0:
        alu_opB = curr_pipe_regs->B;
        break;
    case 1:
        alu_opB = WORD_SIZE;
        break;
    case 2:
        alu_opB = immediate;
        break;
    case 3:
        alu_opB = shifted_immediate;
        break;
    default:
        assert(false && "invalid ALUSrcB");
    }
    // ALUOp
    switch (control->ALUOp)
    {
    case 0: // addition
        next_pipe_regs->ALUOut = alu_opA + alu_opB;
        break;
    case 1: // subtraction
        next_pipe_regs->ALUOut = alu_opA - alu_opB;
    case 2: // R_TYPE ONLY!!
        switch (IR_meta->function)
        {
        case ADD:
            next_pipe_regs->ALUOut = alu_opA + alu_opB;
            break;
        case SLT:
            next_pipe_regs->ALUOut = alu_opA < alu_opB ? 1 : 0; // TODO check this
            break;
        default:
            assert(false && "invalid function-field for R_TYPE ALU execution");
        }
        break;
    default:
        assert(false && "invalid ALUOp");
    }

    // PC calculation
    const int jmp = ((curr_pipe_regs->pc >> 28) << 28) + (IR_meta->jmp_offset << 2);
    switch (control->PCSource)
    {
    case 0: // INSTRUCTION_FETCH
        next_pipe_regs->pc = next_pipe_regs->ALUOut;
        break;
    case 1:                                          // BRANCH_COMPL
        next_pipe_regs->pc = curr_pipe_regs->ALUOut; // optimistically computed in DECODE
        break;
    case 2: // JUMP_COMPL
        next_pipe_regs->pc = jmp;
        break;
    default:
        assert(false && "invalid PCSource");
    }
}

void memory_access() {
  // assert(arch_state.control.IorD == 1 && "IorD should be 1 for LW/SW");
    // read
  if (arch_state.control.MemRead) {
    assert(!arch_state.control.MemWrite &&
           "don't read and write simultaneously");
    switch (arch_state.control.IorD) {
      case 0:
        // useless, but whatever
        arch_state.next_pipe_regs.MDR =
            memory_read(arch_state.curr_pipe_regs.pc);
        break;
      case 1:
        arch_state.next_pipe_regs.MDR =
            memory_read(arch_state.curr_pipe_regs.ALUOut);
        break;
      default:
        assert(false && "invalid IorD control line during MemRead");
    }
    }
    // write
  if (arch_state.control.MemWrite) {
    assert(!arch_state.control.MemRead &&
           "don't read and write simultaneously");
    switch (arch_state.control.IorD) {
      case 0:
        // useless, but whatever
        printf("Writing to memory while IorD == 0... You sure?\n");
        memory_write(arch_state.curr_pipe_regs.pc, arch_state.curr_pipe_regs.B);
        break;
      case 1:
        memory_write(arch_state.curr_pipe_regs.ALUOut,
                     arch_state.curr_pipe_regs.B);
        break;
      default:
        assert(false && "invalid IorD control line during MemWrite");
    }
        memory_write(arch_state.curr_pipe_regs.ALUOut, arch_state.curr_pipe_regs.B);
    }
}

void write_back()
{
    if (arch_state.control.RegWrite)
    {
        int write_reg_id = 0;
        int write_data = 0;
        switch (arch_state.control.RegDst)
        {
        case 0:
            write_reg_id = arch_state.IR_meta.reg_16_20;
            break;
        case 1:
            write_reg_id = arch_state.IR_meta.reg_11_15;
            break;
        default:
            assert(false && "invalid RegDst control line");
        }
        switch (arch_state.control.MemtoReg)
        {
        case 0: // R_TYPE or ADDI completion
            write_data = arch_state.curr_pipe_regs.ALUOut;
            break;
        case 1: // LW or SW completion
            write_data = arch_state.curr_pipe_regs.MDR;
            break;
        default:
            assert(false && "invalid MemtoReg control line");
        }
        check_is_valid_reg_id(write_reg_id);
        if (write_reg_id > 0)
        {
            arch_state.registers[write_reg_id] = write_data;
            printf("Reg $%u = %d (from current %s)\n",
                   write_reg_id, write_data, arch_state.control.MemtoReg ? "MDR" : "ALUOut");
        }
        else
        {
            printf("Attempting to write reg_0. That is likely a mistake \n");
        }
    }
}

void set_up_IR_meta(int IR, struct instr_meta *IR_meta)
{
    IR_meta->opcode = get_piece_of_a_word(IR, OPCODE_OFFSET, OPCODE_SIZE);
    IR_meta->immediate = get_sign_extended_imm_id(IR, IMMEDIATE_OFFSET);
    IR_meta->function = get_piece_of_a_word(IR, 0, 6);
    IR_meta->jmp_offset = get_piece_of_a_word(IR, 0, 26);
    IR_meta->reg_11_15 = (uint8_t)get_piece_of_a_word(IR, 11, REGISTER_ID_SIZE);
    IR_meta->reg_16_20 = (uint8_t)get_piece_of_a_word(IR, 16, REGISTER_ID_SIZE);
    IR_meta->reg_21_25 = (uint8_t)get_piece_of_a_word(IR, 21, REGISTER_ID_SIZE);
    IR_meta->type = get_instruction_type(IR_meta->opcode);

    switch (IR_meta->opcode)
    {
    case SPECIAL:
        switch (IR_meta->function)
        {
        case ADD:
            printf("Executing ADD(%d), $%u = $%u + $%u (function: %u) \n",
                   IR_meta->opcode, IR_meta->reg_11_15, IR_meta->reg_21_25, IR_meta->reg_16_20, IR_meta->function);
            break;
        case SLT:
            printf("Executing SLT(%d), $%u = $%u + $%u (function: %u) \n",
                   IR_meta->opcode, IR_meta->reg_11_15, IR_meta->reg_21_25, IR_meta->reg_16_20, IR_meta->function);
            break;
        default:
            assert(false);
        }
        break;
    case ADDI:
        printf("Executing ADDI(%d), $%u = $%u + %d\n",
               IR_meta->opcode, IR_meta->reg_16_20, IR_meta->reg_21_25, IR_meta->immediate);
        break;
    case LW:
        printf("Executing LW(%d), $%u = %d($%u) (%u) \n",
               IR_meta->opcode, IR_meta->reg_16_20, IR_meta->immediate, IR_meta->reg_21_25, IR_meta->immediate + IR_meta->reg_21_25);
        break;
    case SW:
        printf("Executing SW(%d), %d($%u) (%u) = $%u \n",
               IR_meta->opcode, IR_meta->immediate, IR_meta->reg_21_25, IR_meta->immediate + IR_meta->reg_21_25, IR_meta->reg_16_20);
        break;
    case BEQ:
        printf("Executing BEQ(%d), $%u == $%u \n--> %d\nelse -->  %d\n",
               IR_meta->opcode, IR_meta->reg_21_25, IR_meta->reg_16_20, arch_state.curr_pipe_regs.pc + (IR_meta->immediate << 2), arch_state.next_pipe_regs.pc);
        break;
    case J:
        printf("Executing J(%d), $pc = %u", IR_meta->opcode, ((arch_state.curr_pipe_regs.pc >> 28) << 28) + (IR_meta->jmp_offset << 2));
        break;
    case EOP:
        printf("Executing EOP(%d) \n", IR_meta->opcode);
        break;
    default:
        assert(false);
    }
}

void assign_pipeline_registers_for_the_next_cycle()
{
    struct ctrl_signals *control = &arch_state.control;
    struct instr_meta *IR_meta = &arch_state.IR_meta;
    struct pipe_regs *curr_pipe_regs = &arch_state.curr_pipe_regs;
    struct pipe_regs *next_pipe_regs = &arch_state.next_pipe_regs;

    if (control->IRWrite)
    {
        curr_pipe_regs->IR = next_pipe_regs->IR;
        printf("PC %d: ", curr_pipe_regs->pc / 4);
        set_up_IR_meta(curr_pipe_regs->IR, IR_meta);
    }
    curr_pipe_regs->ALUOut = next_pipe_regs->ALUOut;
    curr_pipe_regs->A = next_pipe_regs->A;
    curr_pipe_regs->B = next_pipe_regs->B;
    curr_pipe_regs->MDR = next_pipe_regs->MDR;
    if (control->PCWrite)
    {
        check_address_is_word_aligned(next_pipe_regs->pc);
        curr_pipe_regs->pc = next_pipe_regs->pc;
    }
}

int main(int argc, const char *argv[])
{
    /*--------------------------------------
    /------- Global Variable Init ----------
    /--------------------------------------*/
    parse_arguments(argc, argv);
    arch_state_init(&arch_state);
    ///@students WARNING: Do NOT change/move/remove main's code above this point!
    while (true)
    {

        ///@students: Fill/modify the function bodies of the 7 functions below,
        /// Do NOT modify the main() itself, you only need to
        /// write code inside the definitions of the functions called below.

        FSM();

        instruction_fetch();

        decode_and_read_RF();

        execute();

        memory_access();

        write_back();

        assign_pipeline_registers_for_the_next_cycle();

        ///@students WARNING: Do NOT change/move/remove code below this point!
        marking_after_clock_cycle();
        arch_state.clock_cycle++;
        // Check exit statements
        if (arch_state.state == EXIT_STATE)
        { // I.E. EOP instruction!
            printf("Exiting because the exit state was reached \n");
            break;
        }
        if (arch_state.clock_cycle == BREAK_POINT)
        {
            printf("Exiting because the break point (%u) was reached \n", BREAK_POINT);
            break;
        }
    }
    marking_at_the_end();
}

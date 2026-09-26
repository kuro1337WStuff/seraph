/***************************************************************************************************

  Zyan Disassembler Library (Zydis)

  Original Author : Florian Bernd

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.

***************************************************************************************************/

#include <Zycore/LibC.h>
#include <Zydis/InstructionInfo.h>
#include <Zydis/Register.h>

/* ============================================================================================== */
/* Helpers                                                                                        */
/* ============================================================================================== */

static ZyanStatus ZydisInfoAddRegister(ZydisInstructionInfo* info, ZydisRegister reg,
    ZydisOperandActions action)
{
    ZyanU8 i;

    if ((reg == ZYDIS_REGISTER_NONE) || !action)
    {
        return ZYAN_STATUS_SUCCESS;
    }

    for (i = 0; i < info->register_count; ++i)
    {
        if (info->registers[i].reg == reg)
        {
            info->registers[i].action =
                (ZydisOperandActions)(info->registers[i].action | action);
            return ZYAN_STATUS_SUCCESS;
        }
    }

    if (info->register_count >= ZYDIS_INSTRUCTION_INFO_MAX_REGISTERS)
    {
        return ZYAN_STATUS_INSUFFICIENT_BUFFER_SIZE;
    }

    info->registers[info->register_count].reg = reg;
    info->registers[info->register_count].action = action;
    ++info->register_count;
    return ZYAN_STATUS_SUCCESS;
}

static ZyanU8 ZydisInfoModeBits(ZydisMachineMode mode)
{
    switch (mode)
    {
    case ZYDIS_MACHINE_MODE_LONG_64:
        return 64;
    case ZYDIS_MACHINE_MODE_LONG_COMPAT_32:
    case ZYDIS_MACHINE_MODE_LEGACY_32:
        return 32;
    default:
        return 16;
    }
}

static ZyanBool ZydisInfoGprFamily(ZydisRegister reg, ZyanU8* family, ZyanU8* width,
    ZyanBool* high_byte)
{
    static const ZyanU8 GPR8_FAMILY[36] =
    {
        0, 1, 2, 3, 0, 1, 2, 3,
        4, 5, 6, 7,
        8, 9, 10, 11, 12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23,
        24, 25, 26, 27, 28, 29, 30, 31
    };
    ZydisRegisterClass reg_class = ZydisRegisterGetClass(reg);
    ZyanI8 id = ZydisRegisterGetId(reg);

    *high_byte = ZYAN_FALSE;
    if (id < 0)
    {
        return ZYAN_FALSE;
    }

    switch (reg_class)
    {
    case ZYDIS_REGCLASS_GPR8:
        if ((ZyanU8)id >= ZYAN_ARRAY_LENGTH(GPR8_FAMILY))
        {
            return ZYAN_FALSE;
        }
        *family = GPR8_FAMILY[id];
        *width = 8;
        *high_byte = (id >= 4) && (id <= 7);
        return ZYAN_TRUE;
    case ZYDIS_REGCLASS_GPR16:
        *family = (ZyanU8)id;
        *width = 16;
        return ZYAN_TRUE;
    case ZYDIS_REGCLASS_GPR32:
        *family = (ZyanU8)id;
        *width = 32;
        return ZYAN_TRUE;
    case ZYDIS_REGCLASS_GPR64:
        *family = (ZyanU8)id;
        *width = 64;
        return ZYAN_TRUE;
    default:
        return ZYAN_FALSE;
    }
}

static ZydisRegister ZydisInfoGprView(ZyanU8 family, ZyanU8 width, ZyanBool high_byte)
{
    if (width == 64)
    {
        return (ZydisRegister)(ZYDIS_REGISTER_RAX + family);
    }
    if (width == 32)
    {
        return (ZydisRegister)(ZYDIS_REGISTER_EAX + family);
    }
    if (width == 16)
    {
        return (ZydisRegister)(ZYDIS_REGISTER_AX + family);
    }
    if (high_byte)
    {
        return (ZydisRegister)(ZYDIS_REGISTER_AH + family);
    }
    if (family < 4)
    {
        return (ZydisRegister)(ZYDIS_REGISTER_AL + family);
    }
    if (family < 8)
    {
        return (ZydisRegister)(ZYDIS_REGISTER_SPL + (family - 4));
    }
    return (ZydisRegister)(ZYDIS_REGISTER_R8B + (family - 8));
}

static ZyanU64 ZydisInfoGprMask(ZyanU8 width, ZyanBool high_byte)
{
    if ((width == 8) && high_byte)
    {
        return 0xFF00ULL;
    }
    if (width == 8)
    {
        return 0xFFULL;
    }
    if (width == 16)
    {
        return 0xFFFFULL;
    }
    if (width == 32)
    {
        return 0xFFFFFFFFULL;
    }
    return 0xFFFFFFFFFFFFFFFFULL;
}

static ZyanBool ZydisInfoVec(ZydisRegister reg, ZyanU8* index, ZyanU16* bits)
{
    if ((reg >= ZYDIS_REGISTER_XMM0) && (reg <= ZYDIS_REGISTER_XMM31))
    {
        *index = (ZyanU8)(reg - ZYDIS_REGISTER_XMM0);
        *bits = 128;
        return ZYAN_TRUE;
    }
    if ((reg >= ZYDIS_REGISTER_YMM0) && (reg <= ZYDIS_REGISTER_YMM31))
    {
        *index = (ZyanU8)(reg - ZYDIS_REGISTER_YMM0);
        *bits = 256;
        return ZYAN_TRUE;
    }
    if ((reg >= ZYDIS_REGISTER_ZMM0) && (reg <= ZYDIS_REGISTER_ZMM31))
    {
        *index = (ZyanU8)(reg - ZYDIS_REGISTER_ZMM0);
        *bits = 512;
        return ZYAN_TRUE;
    }
    return ZYAN_FALSE;
}

static ZydisRegister ZydisInfoVecReg(ZyanU8 index, ZyanU16 bits)
{
    if (bits == 128)
    {
        return (ZydisRegister)(ZYDIS_REGISTER_XMM0 + index);
    }
    if (bits == 256)
    {
        return (ZydisRegister)(ZYDIS_REGISTER_YMM0 + index);
    }
    return (ZydisRegister)(ZYDIS_REGISTER_ZMM0 + index);
}

static ZyanStatus ZydisInfoAddVector(ZydisInstructionInfo* info, ZydisRegister reg,
    ZydisOperandActions action, ZyanU16 op_size)
{
    ZyanU8 index;
    ZyanU16 bits;
    ZyanU8 i;
    static const ZyanU16 WIDTHS[3] = { 128, 256, 512 };
    ZydisOperandActions write_bits = (ZydisOperandActions)(action &
        (ZYDIS_OPERAND_ACTION_WRITE | ZYDIS_OPERAND_ACTION_CONDWRITE));
    ZydisOperandActions read_bits = (ZydisOperandActions)(action &
        (ZYDIS_OPERAND_ACTION_READ | ZYDIS_OPERAND_ACTION_CONDREAD));
    ZyanStatus status;

    if (!ZydisInfoVec(reg, &index, &bits))
    {
        return ZYAN_STATUS_SUCCESS;
    }

    if (write_bits && (op_size != 0) && (op_size < bits) &&
        !(action & (ZYDIS_OPERAND_ACTION_READ | ZYDIS_OPERAND_ACTION_CONDREAD)))
    {
        /* movss/movsd keep the untouched lanes of this register. */
        status = ZydisInfoAddRegister(info, reg, ZYDIS_OPERAND_ACTION_READ);
        if (ZYAN_FAILED(status))
        {
            return status;
        }
    }

    for (i = 0; i < 3; ++i)
    {
        ZyanU16 view_bits = WIDTHS[i];
        ZydisOperandActions derived;

        if (view_bits == bits)
        {
            continue;
        }
        if (view_bits < bits)
        {
            derived = (ZydisOperandActions)(read_bits | write_bits);
        }
        else if (write_bits && (op_size != 0) && (op_size < bits))
        {
            /* Scalar merge (movss/movsd): the untouched lanes are read, and
             * the bits above this vector register are still zeroed. */
            derived = (ZydisOperandActions)(ZYDIS_OPERAND_ACTION_READ | write_bits);
            if (derived & ZYDIS_OPERAND_ACTION_WRITE)
            {
                derived = (ZydisOperandActions)(derived & ~ZYDIS_OPERAND_ACTION_CONDWRITE);
            }
        }
        else if (write_bits)
        {
            derived = write_bits;
        }
        else
        {
            derived = read_bits;
        }

        status = ZydisInfoAddRegister(info, ZydisInfoVecReg(index, view_bits), derived);
        if (ZYAN_FAILED(status))
        {
            return status;
        }
    }
    return ZYAN_STATUS_SUCCESS;
}

static ZyanStatus ZydisInfoAddMmxX87(ZydisInstructionInfo* info, ZydisRegister reg,
    ZydisOperandActions action)
{
    ZydisOperandActions write_bits = (ZydisOperandActions)(action &
        (ZYDIS_OPERAND_ACTION_WRITE | ZYDIS_OPERAND_ACTION_CONDWRITE));
    ZydisOperandActions read_bits = (ZydisOperandActions)(action &
        (ZYDIS_OPERAND_ACTION_READ | ZYDIS_OPERAND_ACTION_CONDREAD));
    ZydisRegister other;
    ZydisOperandActions derived;

    if ((reg >= ZYDIS_REGISTER_MM0) && (reg <= ZYDIS_REGISTER_MM7))
    {
        other = (ZydisRegister)(ZYDIS_REGISTER_ST0 + (reg - ZYDIS_REGISTER_MM0));
        /* An MMX write forces the x87 exponent and tag, so the ST slot is defined. */
        derived = write_bits ? (ZydisOperandActions)(write_bits | (read_bits &
            ZYDIS_OPERAND_ACTION_READ)) : read_bits;
    }
    else if ((reg >= ZYDIS_REGISTER_ST0) && (reg <= ZYDIS_REGISTER_ST7))
    {
        other = (ZydisRegister)(ZYDIS_REGISTER_MM0 + (reg - ZYDIS_REGISTER_ST0));
        derived = (ZydisOperandActions)(read_bits | write_bits);
    }
    else
    {
        return ZYAN_STATUS_SUCCESS;
    }

    if (derived & ZYDIS_OPERAND_ACTION_READ)
    {
        derived = (ZydisOperandActions)(derived & ~ZYDIS_OPERAND_ACTION_CONDREAD);
    }
    if (derived & ZYDIS_OPERAND_ACTION_WRITE)
    {
        derived = (ZydisOperandActions)(derived & ~ZYDIS_OPERAND_ACTION_CONDWRITE);
    }
    return ZydisInfoAddRegister(info, other, derived);
}

static ZyanStatus ZydisInfoAddLadder(ZydisInstructionInfo* info, ZydisMachineMode mode,
    ZydisRegister reg, ZydisOperandActions action, ZydisRegister base)
{
    ZyanU8 mode_bits = ZydisInfoModeBits(mode);
    ZyanU8 src = (ZyanU8)(reg - base);
    ZyanU8 src_bits = (ZyanU8)(16U << src);
    ZyanU8 i;
    ZydisOperandActions write_bits = (ZydisOperandActions)(action &
        (ZYDIS_OPERAND_ACTION_WRITE | ZYDIS_OPERAND_ACTION_CONDWRITE));
    ZydisOperandActions read_bits = (ZydisOperandActions)(action &
        (ZYDIS_OPERAND_ACTION_READ | ZYDIS_OPERAND_ACTION_CONDREAD));

    if (src > 2)
    {
        return ZYAN_STATUS_SUCCESS;
    }

    for (i = 0; i < 3; ++i)
    {
        ZyanU8 bits = (ZyanU8)(16U << i);
        ZydisOperandActions derived;
        ZyanStatus status;

        if (i == src)
        {
            continue;
        }
        if ((bits > mode_bits) && (bits > src_bits))
        {
            continue;
        }
        if (bits < src_bits)
        {
            derived = (ZydisOperandActions)(read_bits | write_bits);
        }
        else if (write_bits && (src_bits == 32) && (bits == 64) &&
            (mode == ZYDIS_MACHINE_MODE_LONG_64))
        {
            derived = write_bits;
        }
        else if (write_bits)
        {
            derived = (ZydisOperandActions)(ZYDIS_OPERAND_ACTION_READ | write_bits);
            if (derived & ZYDIS_OPERAND_ACTION_WRITE)
            {
                derived = (ZydisOperandActions)(derived & ~ZYDIS_OPERAND_ACTION_CONDWRITE);
            }
        }
        else
        {
            derived = read_bits;
        }
        status = ZydisInfoAddRegister(info, (ZydisRegister)(base + i), derived);
        if (ZYAN_FAILED(status))
        {
            return status;
        }
    }
    return ZYAN_STATUS_SUCCESS;
}

static ZyanStatus ZydisInfoAddOther(ZydisInstructionInfo* info, ZydisMachineMode mode,
    ZydisRegister reg, ZydisOperandActions action, ZyanU16 op_size)
{
    ZyanStatus status;

    status = ZydisInfoAddVector(info, reg, action, op_size);
    if (ZYAN_FAILED(status))
    {
        return status;
    }
    status = ZydisInfoAddMmxX87(info, reg, action);
    if (ZYAN_FAILED(status))
    {
        return status;
    }
    if ((reg >= ZYDIS_REGISTER_FLAGS) && (reg <= ZYDIS_REGISTER_RFLAGS))
    {
        return ZydisInfoAddLadder(info, mode, reg, action, ZYDIS_REGISTER_FLAGS);
    }
    if ((reg >= ZYDIS_REGISTER_IP) && (reg <= ZYDIS_REGISTER_RIP))
    {
        return ZydisInfoAddLadder(info, mode, reg, action, ZYDIS_REGISTER_IP);
    }
    return ZYAN_STATUS_SUCCESS;
}

static ZyanStatus ZydisInfoAddGpr(ZydisInstructionInfo* info, ZydisMachineMode mode,
    ZydisMnemonic mnemonic, ZydisRegister reg, ZydisOperandActions action, ZyanU16 op_size)
{
    ZyanU8 family;
    ZyanU8 width;
    ZyanBool high_byte;
    ZyanU8 mode_bits;
    ZyanU8 view_width;
    ZyanStatus status;
    static const ZyanU8 WIDTHS[4] = { 8, 16, 32, 64 };

    status = ZydisInfoAddRegister(info, reg, action);
    if (ZYAN_FAILED(status) || !action)
    {
        return status;
    }
    if (!ZydisInfoGprFamily(reg, &family, &width, &high_byte))
    {
        return ZydisInfoAddOther(info, mode, reg, action, op_size);
    }

    mode_bits = ZydisInfoModeBits(mode);
    for (view_width = 0; view_width < 4; ++view_width)
    {
        ZyanU8 bits = WIDTHS[view_width];
        ZyanU8 pass;

        if ((bits > mode_bits) && (bits > width))
        {
            continue;
        }
        for (pass = 0; pass < 2; ++pass)
        {
            ZyanBool view_high = (pass == 1);
            ZydisRegister view;
            ZyanU64 src_mask;
            ZyanU64 dst_mask;
            ZydisOperandActions derived;
            ZydisOperandActions write_bits;
            ZydisOperandActions read_bits;

            if (view_high && (family >= 4))
            {
                continue;
            }
            if (view_high && (bits != 8))
            {
                continue;
            }
            view = ZydisInfoGprView(family, bits, view_high);
            if (view == reg)
            {
                continue;
            }

            src_mask = ZydisInfoGprMask(width, high_byte);
            dst_mask = ZydisInfoGprMask(bits, view_high);
            if ((src_mask & dst_mask) == 0)
            {
                continue;
            }

            write_bits = (ZydisOperandActions)(action &
                (ZYDIS_OPERAND_ACTION_WRITE | ZYDIS_OPERAND_ACTION_CONDWRITE));
            read_bits = (ZydisOperandActions)(action &
                (ZYDIS_OPERAND_ACTION_READ | ZYDIS_OPERAND_ACTION_CONDREAD));
            derived = 0;

            if ((dst_mask & ~src_mask) == 0)
            {
                derived = (ZydisOperandActions)(read_bits | write_bits);
            }
            else if (write_bits && (width == 32) && (bits == 64) &&
                (mode == ZYDIS_MACHINE_MODE_LONG_64))
            {
                if (write_bits & ZYDIS_OPERAND_ACTION_WRITE)
                {
                    derived = write_bits;
                }
                else if ((mnemonic >= ZYDIS_MNEMONIC_CMOVB) &&
                    (mnemonic <= ZYDIS_MNEMONIC_CMOVZ))
                {
                    /* The upper half is cleared even when the move is not taken. */
                    derived = (ZydisOperandActions)(ZYDIS_OPERAND_ACTION_WRITE |
                        ZYDIS_OPERAND_ACTION_CONDREAD);
                }
                else
                {
                    derived = (ZydisOperandActions)(read_bits | ZYDIS_OPERAND_ACTION_CONDWRITE);
                    if (derived & ZYDIS_OPERAND_ACTION_READ)
                    {
                        derived = (ZydisOperandActions)(derived & ~ZYDIS_OPERAND_ACTION_CONDREAD);
                    }
                }
            }
            else if (write_bits)
            {
                /*
                 * A partial update reads the rest of the register on every path:
                 * the other bits are merged in if the write happens, and the old
                 * value is kept if it does not.
                 */
                derived = (ZydisOperandActions)(read_bits | write_bits |
                    ZYDIS_OPERAND_ACTION_READ);
                if (derived & ZYDIS_OPERAND_ACTION_READ)
                {
                    derived = (ZydisOperandActions)(derived & ~ZYDIS_OPERAND_ACTION_CONDREAD);
                }
                if (derived & ZYDIS_OPERAND_ACTION_WRITE)
                {
                    derived = (ZydisOperandActions)(derived & ~ZYDIS_OPERAND_ACTION_CONDWRITE);
                }
            }
            else
            {
                derived = read_bits;
            }

            status = ZydisInfoAddRegister(info, view, derived);
            if (ZYAN_FAILED(status))
            {
                return status;
            }
        }
    }
    return ZYAN_STATUS_SUCCESS;
}

static ZyanBool ZydisInfoHasExplicitRegOrMem(const ZydisDecodedOperand* operands,
    ZyanU8 operand_count)
{
    ZyanU8 i;

    for (i = 0; i < operand_count; ++i)
    {
        if (operands[i].visibility != ZYDIS_OPERAND_VISIBILITY_EXPLICIT)
        {
            continue;
        }
        if ((operands[i].type == ZYDIS_OPERAND_TYPE_REGISTER) ||
            (operands[i].type == ZYDIS_OPERAND_TYPE_MEMORY))
        {
            return ZYAN_TRUE;
        }
    }
    return ZYAN_FALSE;
}

static ZyanI32 ZydisInfoExplicitImmediate(const ZydisDecodedOperand* operands,
    ZyanU8 operand_count)
{
    ZyanU8 i;

    for (i = 0; i < operand_count; ++i)
    {
        if ((operands[i].visibility == ZYDIS_OPERAND_VISIBILITY_EXPLICIT) &&
            (operands[i].type == ZYDIS_OPERAND_TYPE_IMMEDIATE))
        {
            return (ZyanI32)operands[i].imm.value.u;
        }
    }
    return 0;
}

static ZyanBool ZydisInfoMemorySize(const ZydisDecodedOperand* operands, ZyanU8 operand_count,
    ZydisOperandActions required, ZyanI32* size_bytes)
{
    ZyanU8 i;

    for (i = 0; i < operand_count; ++i)
    {
        if (operands[i].type != ZYDIS_OPERAND_TYPE_MEMORY)
        {
            continue;
        }
        if ((operands[i].actions & required) != required)
        {
            continue;
        }
        if ((operands[i].size % 8) != 0)
        {
            return ZYAN_FALSE;
        }
        *size_bytes = (ZyanI32)(operands[i].size / 8);
        return ZYAN_TRUE;
    }
    return ZYAN_FALSE;
}

static void ZydisInfoSetFlow(const ZydisDecodedInstruction* instruction,
    const ZydisDecodedOperand* operands, ZyanU8 operand_count, ZydisInstructionInfo* info)
{
    const ZyanBool indirect = ZydisInfoHasExplicitRegOrMem(operands, operand_count);

    if (instruction->mnemonic == ZYDIS_MNEMONIC_XBEGIN)
    {
        info->flow = ZYDIS_INSTRUCTION_FLOW_XBEGIN;
        return;
    }
    if ((instruction->mnemonic == ZYDIS_MNEMONIC_UD0) ||
        (instruction->mnemonic == ZYDIS_MNEMONIC_UD1) ||
        (instruction->mnemonic == ZYDIS_MNEMONIC_UD2))
    {
        info->flow = ZYDIS_INSTRUCTION_FLOW_EXCEPTION;
        return;
    }

    switch (instruction->meta.category)
    {
    case ZYDIS_CATEGORY_CALL:
        info->flow = indirect ? ZYDIS_INSTRUCTION_FLOW_INDIRECT_CALL
                              : ZYDIS_INSTRUCTION_FLOW_CALL;
        break;
    case ZYDIS_CATEGORY_RET:
        info->flow = ZYDIS_INSTRUCTION_FLOW_RETURN;
        break;
    case ZYDIS_CATEGORY_COND_BR:
        info->flow = ZYDIS_INSTRUCTION_FLOW_CONDITIONAL_BRANCH;
        break;
    case ZYDIS_CATEGORY_UNCOND_BR:
        info->flow = indirect ? ZYDIS_INSTRUCTION_FLOW_INDIRECT_BRANCH
                              : ZYDIS_INSTRUCTION_FLOW_UNCONDITIONAL_BRANCH;
        break;
    case ZYDIS_CATEGORY_INTERRUPT:
        info->flow = ZYDIS_INSTRUCTION_FLOW_INTERRUPT;
        break;
    case ZYDIS_CATEGORY_SYSCALL:
    case ZYDIS_CATEGORY_SYSRET:
        info->flow = ZYDIS_INSTRUCTION_FLOW_SYSCALL;
        break;
    case ZYDIS_CATEGORY_SYSTEM:
    case ZYDIS_CATEGORY_IO:
    case ZYDIS_CATEGORY_VTX:
        info->flow = ZYDIS_INSTRUCTION_FLOW_PRIVILEGED;
        break;
    default:
        info->flow = ZYDIS_INSTRUCTION_FLOW_NEXT;
        break;
    }
}

static ZydisInstructionIntercept ZydisInfoIntercept(const ZydisDecodedInstruction* instruction)
{
    switch (instruction->meta.category)
    {
    case ZYDIS_CATEGORY_IO:
    case ZYDIS_CATEGORY_IOSTRINGOP:
        return ZYDIS_INSTRUCTION_INTERCEPT_IO;
    case ZYDIS_CATEGORY_VTX:
        return ZYDIS_INSTRUCTION_INTERCEPT_VMX;
    case ZYDIS_CATEGORY_MSRLIST:
    case ZYDIS_CATEGORY_WRMSRNS:
        return ZYDIS_INSTRUCTION_INTERCEPT_MSR;
    default:
        break;
    }

    if (instruction->meta.isa_ext == ZYDIS_ISA_EXT_SVM)
    {
        return ZYDIS_INSTRUCTION_INTERCEPT_SVM;
    }

    switch (instruction->mnemonic)
    {
    case ZYDIS_MNEMONIC_RDMSR:
    case ZYDIS_MNEMONIC_WRMSR:
        return ZYDIS_INSTRUCTION_INTERCEPT_MSR;
    case ZYDIS_MNEMONIC_LGDT:
    case ZYDIS_MNEMONIC_SGDT:
    case ZYDIS_MNEMONIC_LIDT:
    case ZYDIS_MNEMONIC_SIDT:
    case ZYDIS_MNEMONIC_LLDT:
    case ZYDIS_MNEMONIC_SLDT:
    case ZYDIS_MNEMONIC_LTR:
    case ZYDIS_MNEMONIC_STR:
        return ZYDIS_INSTRUCTION_INTERCEPT_DESCRIPTOR;
    default:
        return ZYDIS_INSTRUCTION_INTERCEPT_NONE;
    }
}

static void ZydisInfoSetStack(const ZydisDecodedInstruction* instruction,
    const ZydisDecodedOperand* operands, ZyanU8 operand_count, ZydisInstructionInfo* info)
{
    ZyanI32 size_bytes = 0;

    info->stack_delta_known = ZYAN_FALSE;
    info->stack_delta = 0;

    switch (instruction->meta.category)
    {
    case ZYDIS_CATEGORY_PUSH:
        if (ZydisInfoMemorySize(operands, operand_count, ZYDIS_OPERAND_ACTION_WRITE, &size_bytes))
        {
            info->stack_delta_known = ZYAN_TRUE;
            info->stack_delta = -size_bytes;
        }
        break;
    case ZYDIS_CATEGORY_POP:
        if (ZydisInfoMemorySize(operands, operand_count, ZYDIS_OPERAND_ACTION_READ, &size_bytes))
        {
            info->stack_delta_known = ZYAN_TRUE;
            info->stack_delta = size_bytes;
        }
        break;
    case ZYDIS_CATEGORY_CALL:
        if ((instruction->meta.branch_type != ZYDIS_BRANCH_TYPE_FAR) &&
            ZydisInfoMemorySize(operands, operand_count, ZYDIS_OPERAND_ACTION_WRITE, &size_bytes))
        {
            info->stack_delta_known = ZYAN_TRUE;
            info->stack_delta = -size_bytes;
        }
        break;
    case ZYDIS_CATEGORY_RET:
        if ((instruction->meta.branch_type != ZYDIS_BRANCH_TYPE_FAR) &&
            ZydisInfoMemorySize(operands, operand_count, ZYDIS_OPERAND_ACTION_READ, &size_bytes))
        {
            info->stack_delta_known = ZYAN_TRUE;
            info->stack_delta = size_bytes +
                ZydisInfoExplicitImmediate(operands, operand_count);
        }
        break;
    default:
        break;
    }
}

static ZyanI8 ZydisInfoX87Kind(ZydisMnemonic mnemonic, ZyanBool* conditional)
{
    *conditional = ZYAN_FALSE;

    switch (mnemonic)
    {
    case ZYDIS_MNEMONIC_FLD:
    case ZYDIS_MNEMONIC_FLD1:
    case ZYDIS_MNEMONIC_FLDL2E:
    case ZYDIS_MNEMONIC_FLDL2T:
    case ZYDIS_MNEMONIC_FLDLG2:
    case ZYDIS_MNEMONIC_FLDLN2:
    case ZYDIS_MNEMONIC_FLDPI:
    case ZYDIS_MNEMONIC_FLDZ:
    case ZYDIS_MNEMONIC_FXTRACT:
    case ZYDIS_MNEMONIC_FILD:
    case ZYDIS_MNEMONIC_FBLD:
        return 1;
    case ZYDIS_MNEMONIC_FDECSTP:
        return 2;
    case ZYDIS_MNEMONIC_FPTAN:
    case ZYDIS_MNEMONIC_FSINCOS:
        *conditional = ZYAN_TRUE;
        return 1;
    case ZYDIS_MNEMONIC_FINCSTP:
        return -2;
    case ZYDIS_MNEMONIC_FSTP:
    case ZYDIS_MNEMONIC_FSTPNCE:
    case ZYDIS_MNEMONIC_FCOMP:
    case ZYDIS_MNEMONIC_FUCOMP:
    case ZYDIS_MNEMONIC_FICOMP:
    case ZYDIS_MNEMONIC_FISTP:
    case ZYDIS_MNEMONIC_FISTTP:
    case ZYDIS_MNEMONIC_FADDP:
    case ZYDIS_MNEMONIC_FMULP:
    case ZYDIS_MNEMONIC_FSUBP:
    case ZYDIS_MNEMONIC_FSUBRP:
    case ZYDIS_MNEMONIC_FDIVP:
    case ZYDIS_MNEMONIC_FDIVRP:
    case ZYDIS_MNEMONIC_FBSTP:
    case ZYDIS_MNEMONIC_FFREEP:
    case ZYDIS_MNEMONIC_FYL2X:
    case ZYDIS_MNEMONIC_FYL2XP1:
    case ZYDIS_MNEMONIC_FPATAN:
    case ZYDIS_MNEMONIC_FCOMIP:
    case ZYDIS_MNEMONIC_FUCOMIP:
        return -1;
    case ZYDIS_MNEMONIC_FCOMPP:
    case ZYDIS_MNEMONIC_FUCOMPP:
        return -3;
    case ZYDIS_MNEMONIC_FNINIT:
    case ZYDIS_MNEMONIC_FRSTOR:
    case ZYDIS_MNEMONIC_EMMS:
    case ZYDIS_MNEMONIC_FEMMS:
        return 4;
    case ZYDIS_MNEMONIC_FNSAVE:
        return 5;
    default:
        return 0;
    }
}

static void ZydisInfoSetFpu(const ZydisDecodedInstruction* instruction, ZydisInstructionInfo* info)
{
    ZyanBool conditional = ZYAN_FALSE;
    ZyanI8 kind = ZydisInfoX87Kind(instruction->mnemonic, &conditional);

    info->fpu_delta_known = ZYAN_FALSE;
    info->fpu_delta = 0;
    info->fpu_top_written = ZYAN_FALSE;

    /*
     * emms and femms empty the tag word and leave TOP unchanged. Every other
     * counted stack op writes TOP. fldenv and fxrstor load a new TOP without
     * a constant push or pop, so the delta stays unknown.
     */
    if ((instruction->mnemonic != ZYDIS_MNEMONIC_EMMS) &&
        (instruction->mnemonic != ZYDIS_MNEMONIC_FEMMS) &&
        ((kind != 0) ||
            (instruction->mnemonic == ZYDIS_MNEMONIC_FLDENV) ||
            (instruction->mnemonic == ZYDIS_MNEMONIC_FXRSTOR) ||
            (instruction->mnemonic == ZYDIS_MNEMONIC_FXRSTOR64)))
    {
        info->fpu_top_written = ZYAN_TRUE;
    }

    if (conditional || (kind == 0) || (kind == 4) || (kind == 5))
    {
        return;
    }
    info->fpu_delta_known = ZYAN_TRUE;
    if (kind == 1)
    {
        info->fpu_delta = 1;
    }
    else if (kind == 2)
    {
        info->fpu_delta = 1;
    }
    else if (kind == -2)
    {
        info->fpu_delta = -1;
    }
    else if (kind == -1)
    {
        info->fpu_delta = -1;
    }
    else if (kind == -3)
    {
        info->fpu_delta = -2;
    }
}

static void ZydisInfoDropMustWrite(ZydisInstructionInfo* info, ZydisRegister reg)
{
    ZyanU8 i;

    for (i = 0; i < info->register_count; ++i)
    {
        ZydisOperandActions kept;

        if (info->registers[i].reg != reg)
        {
            continue;
        }
        if ((info->registers[i].action & ZYDIS_OPERAND_ACTION_WRITE) == 0)
        {
            continue;
        }
        kept = (ZydisOperandActions)(info->registers[i].action &
            (ZYDIS_OPERAND_ACTION_READ | ZYDIS_OPERAND_ACTION_CONDREAD |
                ZYDIS_OPERAND_ACTION_CONDWRITE));
        info->registers[i].action = (ZydisOperandActions)(kept |
            ZYDIS_OPERAND_ACTION_CONDWRITE);
    }
}

static ZyanStatus ZydisInfoAddX87Stack(ZydisInstructionInfo* info, ZydisMnemonic mnemonic)
{
    ZyanBool conditional = ZYAN_FALSE;
    ZyanI8 kind = ZydisInfoX87Kind(mnemonic, &conditional);
    ZydisOperandActions move_action;
    ZydisOperandActions st7_action;
    ZyanU8 i;

    /*
     * fsin and fcos replace st0 only when C2 stays clear. Zydis marks that
     * write as unconditional. The out-of-range path leaves st0 unchanged.
     */
    if ((mnemonic == ZYDIS_MNEMONIC_FSIN) || (mnemonic == ZYDIS_MNEMONIC_FCOS))
    {
        ZydisInfoDropMustWrite(info, ZYDIS_REGISTER_ST0);
        ZydisInfoDropMustWrite(info, ZYDIS_REGISTER_MM0);
        return ZYAN_STATUS_SUCCESS;
    }

    if (kind == 0)
    {
        return ZYAN_STATUS_SUCCESS;
    }

    if (conditional)
    {
        move_action = (ZydisOperandActions)(ZYDIS_OPERAND_ACTION_CONDREAD |
            ZYDIS_OPERAND_ACTION_CONDWRITE);
        st7_action = ZYDIS_OPERAND_ACTION_CONDWRITE;
    }
    else if (kind == 1)
    {
        move_action = ZYDIS_OPERAND_ACTION_READWRITE;
        st7_action = ZYDIS_OPERAND_ACTION_WRITE;
    }
    else if (kind == 4)
    {
        move_action = ZYDIS_OPERAND_ACTION_WRITE;
        st7_action = ZYDIS_OPERAND_ACTION_WRITE;
    }
    else
    {
        move_action = ZYDIS_OPERAND_ACTION_READWRITE;
        st7_action = ZYDIS_OPERAND_ACTION_READWRITE;
    }

    for (i = 0; i < 8; ++i)
    {
        ZyanStatus status = ZydisInfoAddGpr(info, ZYDIS_MACHINE_MODE_LONG_64, mnemonic,
            (ZydisRegister)(ZYDIS_REGISTER_ST0 + i),
            (i == 7) ? st7_action : move_action, 80);

        if (ZYAN_FAILED(status))
        {
            return status;
        }
    }

    /*
     * fptan and fsincos push only when C2 stays clear. Zydis still marks st0
     * and st1 as unconditional writes for that result. Those writes are the
     * conditional push, so the must-write bit does not stay. st0 keeps its read.
     */
    if (conditional)
    {
        for (i = 0; i < 8; ++i)
        {
            ZydisInfoDropMustWrite(info, (ZydisRegister)(ZYDIS_REGISTER_ST0 + i));
            ZydisInfoDropMustWrite(info, (ZydisRegister)(ZYDIS_REGISTER_MM0 + i));
        }
    }
    return ZYAN_STATUS_SUCCESS;
}

/* ============================================================================================== */
/* Exported functions                                                                             */
/* ============================================================================================== */

ZyanStatus ZydisGetInstructionInfo(const ZydisDecodedInstruction* instruction,
    const ZydisDecodedOperand* operands, ZyanU8 operand_count, ZydisInstructionInfo* info)
{
    ZyanU8 i;
    ZyanStatus status;

    if (!instruction || !info || (operand_count && !operands) ||
        (operand_count > ZYDIS_MAX_OPERAND_COUNT))
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }

    /*
     * The summary needs every operand, including the implicit ones. `mul rcx`
     * has one visible operand but four in total (RCX, RAX, RDX, RFLAGS). A
     * caller that passes `operand_count_visible` would otherwise get a SUCCESS
     * result that silently drops the implicit registers. Require the full
     * count so a short array is a loud error, not wrong data.
     */
    if (operand_count != instruction->operand_count)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }

    ZYAN_MEMSET(info, 0, sizeof(*info));
    ZydisInfoSetFlow(instruction, operands, operand_count, info);
    info->intercept = ZydisInfoIntercept(instruction);
    ZydisInfoSetStack(instruction, operands, operand_count, info);
    ZydisInfoSetFpu(instruction, info);

    if (instruction->cpu_flags)
    {
        info->flags_tested = instruction->cpu_flags->tested;
        info->flags_modified = instruction->cpu_flags->modified;
        info->flags_set_0 = instruction->cpu_flags->set_0;
        info->flags_set_1 = instruction->cpu_flags->set_1;
        info->flags_undefined = instruction->cpu_flags->undefined;
    }

    for (i = 0; i < operand_count; ++i)
    {
        const ZydisDecodedOperand* operand = &operands[i];

        if (operand->type == ZYDIS_OPERAND_TYPE_REGISTER)
        {
            status = ZydisInfoAddGpr(info, instruction->machine_mode, instruction->mnemonic,
                operand->reg.value, operand->actions, operand->size);
            if (ZYAN_FAILED(status))
            {
                return status;
            }
        }
        else if (operand->type == ZYDIS_OPERAND_TYPE_MEMORY)
        {
            if (info->memory_count >= ZYDIS_MAX_OPERAND_COUNT)
            {
                return ZYAN_STATUS_INSUFFICIENT_BUFFER_SIZE;
            }
            if ((operand->size % 8) != 0)
            {
                return ZYAN_STATUS_INVALID_ARGUMENT;
            }

            info->memory[info->memory_count].segment = operand->mem.segment;
            info->memory[info->memory_count].base = operand->mem.base;
            info->memory[info->memory_count].index = operand->mem.index;
            info->memory[info->memory_count].scale = operand->mem.scale;
            info->memory[info->memory_count].disp = operand->mem.disp.value;
            info->memory[info->memory_count].size = (ZyanU32)(operand->size / 8);
            info->memory[info->memory_count].action = operand->actions;
            ++info->memory_count;

            status = ZydisInfoAddGpr(info, instruction->machine_mode, instruction->mnemonic,
                operand->mem.base, ZYDIS_OPERAND_ACTION_READ, 0);
            if (ZYAN_FAILED(status))
            {
                return status;
            }
            status = ZydisInfoAddGpr(info, instruction->machine_mode, instruction->mnemonic,
                operand->mem.index, ZYDIS_OPERAND_ACTION_READ, 0);
            if (ZYAN_FAILED(status))
            {
                return status;
            }
            status = ZydisInfoAddRegister(info, operand->mem.segment, ZYDIS_OPERAND_ACTION_READ);
            if (ZYAN_FAILED(status))
            {
                return status;
            }
        }
    }

    /*
     * A read-write of the 32-bit register itself must not stick READ onto the
     * 64-bit register: that write zero-extends and the upper half is not read.
     * A separate use of the value as an address does read it, whether the
     * base or index is the 32-bit name or the 64-bit name.
     */
    if (instruction->machine_mode == ZYDIS_MACHINE_MODE_LONG_64)
    {
        for (i = 0; i < info->register_count; ++i)
        {
            ZyanU8 family;
            ZyanU8 width;
            ZyanBool high_byte;
            ZydisRegister gpr32;
            ZydisOperandActions write_bits;
            ZyanU8 j;
            ZyanBool direct_read;

            if (!ZydisInfoGprFamily(info->registers[i].reg, &family, &width, &high_byte) ||
                (width != 64))
            {
                continue;
            }

            gpr32 = ZydisInfoGprView(family, 32, ZYAN_FALSE);
            write_bits = 0;
            for (j = 0; j < operand_count; ++j)
            {
                if ((operands[j].type == ZYDIS_OPERAND_TYPE_REGISTER) &&
                    (operands[j].reg.value == gpr32))
                {
                    write_bits = (ZydisOperandActions)(operands[j].actions &
                        (ZYDIS_OPERAND_ACTION_WRITE | ZYDIS_OPERAND_ACTION_CONDWRITE));
                    if (write_bits)
                    {
                        break;
                    }
                }
            }
            if (!write_bits)
            {
                continue;
            }

            direct_read = ZYAN_FALSE;
            for (j = 0; j < operand_count; ++j)
            {
                if ((operands[j].type == ZYDIS_OPERAND_TYPE_REGISTER) &&
                    (operands[j].reg.value == info->registers[i].reg) &&
                    (operands[j].actions &
                        (ZYDIS_OPERAND_ACTION_READ | ZYDIS_OPERAND_ACTION_CONDREAD)))
                {
                    direct_read = ZYAN_TRUE;
                }
                if ((operands[j].type == ZYDIS_OPERAND_TYPE_MEMORY) &&
                    ((operands[j].mem.base == info->registers[i].reg) ||
                        (operands[j].mem.index == info->registers[i].reg) ||
                        (operands[j].mem.base == gpr32) ||
                        (operands[j].mem.index == gpr32)))
                {
                    direct_read = ZYAN_TRUE;
                }
            }
            if (!direct_read)
            {
                if (write_bits & ZYDIS_OPERAND_ACTION_WRITE)
                {
                    info->registers[i].action = write_bits;
                }
                else if ((instruction->mnemonic >= ZYDIS_MNEMONIC_CMOVB) &&
                    (instruction->mnemonic <= ZYDIS_MNEMONIC_CMOVZ))
                {
                    info->registers[i].action = (ZydisOperandActions)(ZYDIS_OPERAND_ACTION_WRITE |
                        ZYDIS_OPERAND_ACTION_CONDREAD);
                }
            }
        }
    }

    /*
     * A full-width XMM/YMM write zeroes the bits above that width. A second
     * read of the same narrow register must not turn ymm/zmm into a read.
     * A scalar merge (movss) is smaller than the XMM register and keeps the read.
     */
    for (i = 0; i < info->register_count; ++i)
    {
        ZyanU8 index;
        ZyanU16 bits;
        ZyanU8 j;
        ZydisOperandActions write_bits = 0;
        ZyanBool partial = ZYAN_FALSE;
        ZyanBool direct_read = ZYAN_FALSE;

        if (!ZydisInfoVec(info->registers[i].reg, &index, &bits) || (bits == 128))
        {
            continue;
        }
        for (j = 0; j < operand_count; ++j)
        {
            ZyanU8 op_index;
            ZyanU16 op_bits;

            if ((operands[j].type != ZYDIS_OPERAND_TYPE_REGISTER) ||
                !ZydisInfoVec(operands[j].reg.value, &op_index, &op_bits) ||
                (op_index != index))
            {
                continue;
            }
            if ((operands[j].actions &
                    (ZYDIS_OPERAND_ACTION_READ | ZYDIS_OPERAND_ACTION_CONDREAD)) &&
                (op_bits == bits))
            {
                direct_read = ZYAN_TRUE;
            }
            if ((op_bits < bits) &&
                (operands[j].actions & ZYDIS_OPERAND_ACTION_WRITE))
            {
                if ((operands[j].size != 0) && (operands[j].size < op_bits))
                {
                    partial = ZYAN_TRUE;
                }
                else
                {
                    write_bits = (ZydisOperandActions)(write_bits | ZYDIS_OPERAND_ACTION_WRITE);
                }
            }
        }
        if (write_bits && !partial && !direct_read)
        {
            info->registers[i].action = write_bits;
        }
    }

    return ZydisInfoAddX87Stack(info, instruction->mnemonic);
}

ZyanStatus ZydisGetInstructionInfoInsn(const ZydisDecodedInstruction* instruction,
    const ZydisDecodedOperand* operands, ZydisInstructionInfo* info)
{
    if (!instruction)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }
    return ZydisGetInstructionInfo(instruction, operands, instruction->operand_count, info);
}

ZydisInstructionIntercept ZydisGetInterceptClass(const ZydisDecodedInstruction* instruction)
{
    if (!instruction)
    {
        return ZYDIS_INSTRUCTION_INTERCEPT_NONE;
    }
    return ZydisInfoIntercept(instruction);
}

static ZyanBool ZydisMmioIsGpr(ZydisRegister reg)
{
    ZydisRegisterClass reg_class = ZydisRegisterGetClass(reg);
    return (reg_class == ZYDIS_REGCLASS_GPR8) || (reg_class == ZYDIS_REGCLASS_GPR16) ||
        (reg_class == ZYDIS_REGCLASS_GPR32) || (reg_class == ZYDIS_REGCLASS_GPR64);
}

ZyanStatus ZydisGetMmioAccess(const ZydisDecodedInstruction* instruction,
    const ZydisDecodedOperand* operands, ZyanU8 operand_count, ZydisMmioAccess* access)
{
    ZyanU8 i;
    ZyanBool have_mem = ZYAN_FALSE;
    ZyanBool have_mem2 = ZYAN_FALSE;
    ZydisOperandActions direction;

    if (!instruction || !access || (operand_count && !operands))
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }

    ZYAN_MEMSET(access, 0, sizeof(*access));
    access->segment = ZYDIS_REGISTER_NONE;
    access->gpr = ZYDIS_REGISTER_NONE;

    for (i = 0; i < operand_count; ++i)
    {
        const ZydisDecodedOperand* op = &operands[i];

        if ((op->type == ZYDIS_OPERAND_TYPE_MEMORY) && (op->mem.type == ZYDIS_MEMOP_TYPE_MEM))
        {
            ZydisInstructionMemoryUse* slot;
            if (have_mem && have_mem2)
            {
                continue;
            }
            slot = have_mem ? &access->mem2 : &access->mem;
            slot->segment = op->mem.segment;
            slot->base = op->mem.base;
            slot->index = op->mem.index;
            slot->scale = op->mem.scale;
            slot->disp = op->mem.disp.value;
            slot->size = (ZyanU32)(op->size / 8);
            slot->action = op->actions;
            if (have_mem)
            {
                have_mem2 = ZYAN_TRUE;
            }
            else
            {
                have_mem = ZYAN_TRUE;
            }
        }
        else if (i < instruction->operand_count_visible)
        {
            /* The explicit non-memory side is the paired register or immediate. Implicit
               operands (a string op's RSI/RDI/RCX) live past the visible count and are skipped. */
            if ((op->type == ZYDIS_OPERAND_TYPE_REGISTER) &&
                (access->gpr == ZYDIS_REGISTER_NONE) && ZydisMmioIsGpr(op->reg.value))
            {
                access->gpr = op->reg.value;
            }
            else if ((op->type == ZYDIS_OPERAND_TYPE_IMMEDIATE) && !access->has_immediate)
            {
                access->has_immediate = ZYAN_TRUE;
                access->immediate = op->imm.value.u;
            }
        }
    }

    if (!have_mem)
    {
        return ZYAN_STATUS_NOT_FOUND;
    }

    direction = 0;
    if (access->mem.action & ZYDIS_OPERAND_ACTION_MASK_READ)
    {
        direction = (ZydisOperandActions)(direction | ZYDIS_OPERAND_ACTION_READ);
    }
    if (access->mem.action & ZYDIS_OPERAND_ACTION_MASK_WRITE)
    {
        direction = (ZydisOperandActions)(direction | ZYDIS_OPERAND_ACTION_WRITE);
    }
    access->direction = direction;
    access->size = access->mem.size;
    access->segment = access->mem.segment;
    access->is_string_op = have_mem2;
    access->rep_prefixed = (instruction->attributes &
        (ZYDIS_ATTRIB_HAS_REP | ZYDIS_ATTRIB_HAS_REPE | ZYDIS_ATTRIB_HAS_REPNE)) ?
        ZYAN_TRUE : ZYAN_FALSE;

    switch (instruction->mnemonic)
    {
    case ZYDIS_MNEMONIC_MOVSX:
    case ZYDIS_MNEMONIC_MOVSXD:
        access->sign_extend = ZYAN_TRUE;
        break;
    case ZYDIS_MNEMONIC_MOVZX:
        access->zero_extend = ZYAN_TRUE;
        break;
    default:
        break;
    }

    return ZYAN_STATUS_SUCCESS;
}

/* ============================================================================================== */
/* Enum strings                                                                                   */
/* ============================================================================================== */

static const char* const ZYDIS_FLOW_NAMES[] =
{
    "next",
    "conditional-branch",
    "unconditional-branch",
    "indirect-branch",
    "call",
    "indirect-call",
    "return",
    "interrupt",
    "syscall",
    "xbegin",
    "exception",
    "privileged"
};

static const char* const ZYDIS_INTERCEPT_NAMES[] =
{
    "none",
    "io",
    "msr",
    "descriptor",
    "vmx",
    "svm"
};

ZYAN_STATIC_ASSERT((sizeof(ZYDIS_FLOW_NAMES) / sizeof(ZYDIS_FLOW_NAMES[0])) ==
    (ZYDIS_INSTRUCTION_FLOW_MAX_VALUE + 1));
ZYAN_STATIC_ASSERT((sizeof(ZYDIS_INTERCEPT_NAMES) / sizeof(ZYDIS_INTERCEPT_NAMES[0])) ==
    (ZYDIS_INSTRUCTION_INTERCEPT_MAX_VALUE + 1));

const char* ZydisInstructionFlowGetString(ZydisInstructionFlow flow)
{
    if ((ZyanUSize)flow > ZYDIS_INSTRUCTION_FLOW_MAX_VALUE)
    {
        return ZYAN_NULL;
    }
    return ZYDIS_FLOW_NAMES[flow];
}

const char* ZydisInstructionInterceptGetString(ZydisInstructionIntercept intercept)
{
    if ((ZyanUSize)intercept > ZYDIS_INSTRUCTION_INTERCEPT_MAX_VALUE)
    {
        return ZYAN_NULL;
    }
    return ZYDIS_INTERCEPT_NAMES[intercept];
}

/* ============================================================================================== */

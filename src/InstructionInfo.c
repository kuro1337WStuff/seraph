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

static ZyanStatus ZydisInfoAddGpr(ZydisInstructionInfo* info, ZydisMachineMode mode,
    ZydisRegister reg, ZydisOperandActions action)
{
    ZyanU8 family;
    ZyanU8 width;
    ZyanBool high_byte;
    ZyanU8 mode_bits;
    ZyanU8 view_width;
    ZyanStatus status;
    static const ZyanU8 WIDTHS[4] = { 8, 16, 32, 64 };

    status = ZydisInfoAddRegister(info, reg, action);
    if (ZYAN_FAILED(status) || !action || !ZydisInfoGprFamily(reg, &family, &width, &high_byte))
    {
        return status;
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
                derived = write_bits;
            }
            else if (write_bits)
            {
                derived = (write_bits & ZYDIS_OPERAND_ACTION_WRITE)
                    ? (ZydisOperandActions)(ZYDIS_OPERAND_ACTION_READ | write_bits)
                    : (ZydisOperandActions)(ZYDIS_OPERAND_ACTION_CONDREAD | write_bits);
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

static void ZydisInfoSetFpu(const ZydisDecodedInstruction* instruction, ZydisInstructionInfo* info)
{
    info->fpu_delta_known = ZYAN_FALSE;
    info->fpu_delta = 0;

    switch (instruction->mnemonic)
    {
    case ZYDIS_MNEMONIC_FLD:
    case ZYDIS_MNEMONIC_FLD1:
    case ZYDIS_MNEMONIC_FLDL2E:
    case ZYDIS_MNEMONIC_FLDL2T:
    case ZYDIS_MNEMONIC_FLDLG2:
    case ZYDIS_MNEMONIC_FLDLN2:
    case ZYDIS_MNEMONIC_FLDPI:
    case ZYDIS_MNEMONIC_FLDZ:
        info->fpu_delta_known = ZYAN_TRUE;
        info->fpu_delta = 1;
        break;
    case ZYDIS_MNEMONIC_FSTP:
        info->fpu_delta_known = ZYAN_TRUE;
        info->fpu_delta = -1;
        break;
    default:
        break;
    }
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

    ZYAN_MEMSET(info, 0, sizeof(*info));
    ZydisInfoSetFlow(instruction, operands, operand_count, info);
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
            status = ZydisInfoAddGpr(info, instruction->machine_mode, operand->reg.value,
                operand->actions);
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

            status = ZydisInfoAddGpr(info, instruction->machine_mode, operand->mem.base,
                ZYDIS_OPERAND_ACTION_READ);
            if (ZYAN_FAILED(status))
            {
                return status;
            }
            status = ZydisInfoAddGpr(info, instruction->machine_mode, operand->mem.index,
                ZYDIS_OPERAND_ACTION_READ);
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
     * A later read of the same 32-bit register must not stick READ onto the
     * 64-bit register that a zero-extending write already defined. A direct
     * read of that 64-bit register, including as a memory base or index, stays.
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
                        (operands[j].mem.index == info->registers[i].reg)))
                {
                    direct_read = ZYAN_TRUE;
                }
            }
            if (!direct_read)
            {
                info->registers[i].action = write_bits;
            }
        }
    }

    return ZYAN_STATUS_SUCCESS;
}

/* ============================================================================================== */

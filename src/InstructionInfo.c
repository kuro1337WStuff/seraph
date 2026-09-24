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
            status = ZydisInfoAddRegister(info, operand->reg.value, operand->actions);
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

            status = ZydisInfoAddRegister(info, operand->mem.base, ZYDIS_OPERAND_ACTION_READ);
            if (ZYAN_FAILED(status))
            {
                return status;
            }
            status = ZydisInfoAddRegister(info, operand->mem.index, ZYDIS_OPERAND_ACTION_READ);
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

    return ZYAN_STATUS_SUCCESS;
}

/* ============================================================================================== */

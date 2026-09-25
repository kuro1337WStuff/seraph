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

/**
 * @file
 * Checks ZydisGetInstructionInfo for control flow, registers, flags, and
 * stack or x87 deltas.
 */

#include <stdio.h>
#include <string.h>

#include <Zydis/Zydis.h>

/* ============================================================================================== */
/* Helpers                                                                                        */
/* ============================================================================================== */

static int g_failures = 0;

static void Fail(const char* label, const char* what)
{
    printf("FAIL %s: %s\n", label, what);
    ++g_failures;
}

static int DecodeMode(const char* label, ZydisMachineMode mode, ZydisStackWidth stack,
    const ZyanU8* bytes, ZyanUSize length, ZydisDecodedInstruction* instruction,
    ZydisDecodedOperand* operands, ZydisInstructionInfo* info)
{
    ZydisDecoder decoder;

    if (ZYAN_FAILED(ZydisDecoderInit(&decoder, mode, stack)) ||
        ZYAN_FAILED(ZydisDecoderDecodeFull(&decoder, bytes, length, instruction, operands)) ||
        (instruction->length != length) ||
        ZYAN_FAILED(ZydisGetInstructionInfo(instruction, operands, instruction->operand_count, info)))
    {
        Fail(label, "decode");
        return 0;
    }
    return 1;
}

static int Decode(const char* label, const ZyanU8* bytes, ZyanUSize length,
    ZydisDecodedInstruction* instruction, ZydisDecodedOperand* operands,
    ZydisInstructionInfo* info)
{
    return DecodeMode(label, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64, bytes, length,
        instruction, operands, info);
}

static int FindReg(const ZydisInstructionInfo* info, ZydisRegister reg, ZydisOperandActions* action)
{
    ZyanU8 i;

    for (i = 0; i < info->register_count; ++i)
    {
        if (info->registers[i].reg == reg)
        {
            *action = info->registers[i].action;
            return 1;
        }
    }
    return 0;
}

static void ExpectAbsent(const char* label, const ZydisInstructionInfo* info, ZydisRegister reg)
{
    ZydisOperandActions got = 0;

    if (FindReg(info, reg, &got))
    {
        printf("FAIL %s: unexpected %s action 0x%x\n", label, ZydisRegisterGetString(reg),
            (unsigned)got);
        ++g_failures;
    }
}

static void ExpectReg(const char* label, const ZydisInstructionInfo* info, ZydisRegister reg,
    ZydisOperandActions action)
{
    ZydisOperandActions got = 0;

    if (!FindReg(info, reg, &got))
    {
        printf("FAIL %s: missing %s\n", label, ZydisRegisterGetString(reg));
        ++g_failures;
        return;
    }
    if (got != action)
    {
        printf("FAIL %s: %s action got 0x%x want 0x%x\n", label, ZydisRegisterGetString(reg),
            (unsigned)got, (unsigned)action);
        ++g_failures;
    }
}

static void ExpectFlow(const char* label, const ZydisInstructionInfo* info, ZydisInstructionFlow flow)
{
    if (info->flow != flow)
    {
        printf("FAIL %s: flow got %d want %d\n", label, (int)info->flow, (int)flow);
        ++g_failures;
    }
}

static void ExpectStack(const char* label, const ZydisInstructionInfo* info, ZyanI32 delta)
{
    if (!info->stack_delta_known || (info->stack_delta != delta))
    {
        printf("FAIL %s: stack got known=%d delta=%d want %d\n", label,
            (int)info->stack_delta_known, (int)info->stack_delta, (int)delta);
        ++g_failures;
    }
}

static void ExpectFpu(const char* label, const ZydisInstructionInfo* info, ZyanI8 delta)
{
    if (!info->fpu_delta_known || (info->fpu_delta != delta))
    {
        printf("FAIL %s: fpu got known=%d delta=%d want %d\n", label,
            (int)info->fpu_delta_known, (int)info->fpu_delta, (int)delta);
        ++g_failures;
    }
}

/* ============================================================================================== */
/* Cases                                                                                          */
/* ============================================================================================== */

int main(void)
{
    ZydisDecodedInstruction instruction;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
    ZydisInstructionInfo info;

    memset(&instruction, 0, sizeof(instruction));
    memset(operands, 0, sizeof(operands));

    if (!ZYAN_FAILED(ZydisGetInstructionInfo(ZYAN_NULL, operands, 1, &info)) ||
        !ZYAN_FAILED(ZydisGetInstructionInfo(&instruction, operands, 1, ZYAN_NULL)))
    {
        Fail("null", "accepted");
    }

    {
        static const ZyanU8 bytes[] = { 0x50 };
        if (Decode("push rax", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectFlow("push rax", &info, ZYDIS_INSTRUCTION_FLOW_NEXT);
            ExpectStack("push rax", &info, -8);
            ExpectReg("push rax", &info, ZYDIS_REGISTER_RAX, ZYDIS_OPERAND_ACTION_READ);
            ExpectReg("push rax", &info, ZYDIS_REGISTER_RSP, ZYDIS_OPERAND_ACTION_READWRITE);
            if ((info.memory_count != 1) || (info.memory[0].size != 8) ||
                (info.memory[0].action != ZYDIS_OPERAND_ACTION_WRITE))
            {
                Fail("push rax", "memory");
            }
        }
    }

    {
        static const ZyanU8 bytes[] = { 0x0F, 0xA2 };
        if (Decode("cpuid", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectFlow("cpuid", &info, ZYDIS_INSTRUCTION_FLOW_NEXT);
            ExpectReg("cpuid", &info, ZYDIS_REGISTER_EAX, ZYDIS_OPERAND_ACTION_READWRITE);
            ExpectReg("cpuid", &info, ZYDIS_REGISTER_EBX, ZYDIS_OPERAND_ACTION_WRITE);
            ExpectReg("cpuid", &info, ZYDIS_REGISTER_ECX, ZYDIS_OPERAND_ACTION_CONDREAD_WRITE);
            ExpectReg("cpuid", &info, ZYDIS_REGISTER_EDX, ZYDIS_OPERAND_ACTION_WRITE);
            if (info.stack_delta_known)
            {
                Fail("cpuid", "stack should be unknown");
            }
        }
    }

    {
        static const ZyanU8 bytes[] = { 0x0F, 0xAF, 0xC1 };
        if (Decode("imul", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectFlow("imul", &info, ZYDIS_INSTRUCTION_FLOW_NEXT);
            if ((info.flags_modified & ZYDIS_CPUFLAG_CF) == 0 ||
                (info.flags_modified & ZYDIS_CPUFLAG_OF) == 0)
            {
                Fail("imul", "modified flags");
            }
            if ((info.flags_undefined & ZYDIS_CPUFLAG_ZF) == 0)
            {
                Fail("imul", "undefined ZF");
            }
        }
    }

    {
        static const ZyanU8 bytes[] = { 0xE8, 0x00, 0x00, 0x00, 0x00 };
        if (Decode("call", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectFlow("call", &info, ZYDIS_INSTRUCTION_FLOW_CALL);
            ExpectStack("call", &info, -8);
        }
    }

    {
        static const ZyanU8 bytes[] = { 0xC3 };
        if (Decode("ret", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectFlow("ret", &info, ZYDIS_INSTRUCTION_FLOW_RETURN);
            ExpectStack("ret", &info, 8);
        }
    }

    {
        static const ZyanU8 bytes[] = { 0xC2, 0x08, 0x00 };
        if (Decode("ret 8", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectFlow("ret 8", &info, ZYDIS_INSTRUCTION_FLOW_RETURN);
            ExpectStack("ret 8", &info, 16);
        }
    }

    {
        static const ZyanU8 bytes[] = { 0xEB, 0x00 };
        if (Decode("jmp", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectFlow("jmp", &info, ZYDIS_INSTRUCTION_FLOW_UNCONDITIONAL_BRANCH);
        }
    }

    {
        static const ZyanU8 bytes[] = { 0xFF, 0xE0 };
        if (Decode("jmp rax", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectFlow("jmp rax", &info, ZYDIS_INSTRUCTION_FLOW_INDIRECT_BRANCH);
            ExpectReg("jmp rax", &info, ZYDIS_REGISTER_RAX, ZYDIS_OPERAND_ACTION_READ);
        }
    }

    {
        static const ZyanU8 bytes[] = { 0x75, 0x00 };
        if (Decode("jnz", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectFlow("jnz", &info, ZYDIS_INSTRUCTION_FLOW_CONDITIONAL_BRANCH);
            if ((info.flags_tested & ZYDIS_CPUFLAG_ZF) == 0)
            {
                Fail("jnz", "tested ZF");
            }
        }
    }

    {
        static const ZyanU8 bytes[] = { 0xCC };
        if (Decode("int3", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectFlow("int3", &info, ZYDIS_INSTRUCTION_FLOW_INTERRUPT);
        }
    }

    {
        static const ZyanU8 bytes[] = { 0x0F, 0x05 };
        if (Decode("syscall", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectFlow("syscall", &info, ZYDIS_INSTRUCTION_FLOW_SYSCALL);
        }
    }

    {
        static const ZyanU8 bytes[] = { 0xC7, 0xF8, 0x00, 0x00, 0x00, 0x00 };
        if (Decode("xbegin", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectFlow("xbegin", &info, ZYDIS_INSTRUCTION_FLOW_XBEGIN);
        }
    }

    {
        static const ZyanU8 bytes[] = { 0xD9, 0x00 };
        if (Decode("fld", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectFpu("fld", &info, 1);
            if ((info.memory_count != 1) || (info.memory[0].action != ZYDIS_OPERAND_ACTION_READ))
            {
                Fail("fld", "memory");
            }
        }
    }

    {
        static const ZyanU8 bytes[] = { 0xD9, 0x18 };
        if (Decode("fstp", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectFpu("fstp", &info, -1);
            if ((info.memory_count != 1) || (info.memory[0].action != ZYDIS_OPERAND_ACTION_WRITE))
            {
                Fail("fstp", "memory");
            }
        }
    }

    {
        static const ZyanU8 bytes[] = { 0x0F, 0xAE, 0x20 };
        if (Decode("xsave", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectReg("xsave", &info, ZYDIS_REGISTER_EAX, ZYDIS_OPERAND_ACTION_READ);
            ExpectReg("xsave", &info, ZYDIS_REGISTER_EDX, ZYDIS_OPERAND_ACTION_READ);
            if ((info.memory_count != 1) || (info.memory[0].size != 576) ||
                (info.memory[0].action != ZYDIS_OPERAND_ACTION_READWRITE))
            {
                Fail("xsave", "memory");
            }
        }
    }

    {
        static const ZyanU8 bytes[] = { 0x0F, 0x0B };
        if (Decode("ud2", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectFlow("ud2", &info, ZYDIS_INSTRUCTION_FLOW_EXCEPTION);
        }
    }

    {
        static const ZyanU8 bytes[] = { 0xF4 };
        if (Decode("hlt", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectFlow("hlt", &info, ZYDIS_INSTRUCTION_FLOW_PRIVILEGED);
        }
    }

    {
        static const ZyanU8 bytes[] = { 0x88, 0xC8 };
        if (Decode("mov al, cl", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectReg("mov al, cl", &info, ZYDIS_REGISTER_AL, ZYDIS_OPERAND_ACTION_WRITE);
            ExpectReg("mov al, cl", &info, ZYDIS_REGISTER_AX, ZYDIS_OPERAND_ACTION_READWRITE);
            ExpectReg("mov al, cl", &info, ZYDIS_REGISTER_EAX, ZYDIS_OPERAND_ACTION_READWRITE);
            ExpectReg("mov al, cl", &info, ZYDIS_REGISTER_RAX, ZYDIS_OPERAND_ACTION_READWRITE);
            ExpectAbsent("mov al, cl", &info, ZYDIS_REGISTER_AH);
            ExpectReg("mov al, cl", &info, ZYDIS_REGISTER_CL, ZYDIS_OPERAND_ACTION_READ);
            ExpectReg("mov al, cl", &info, ZYDIS_REGISTER_CX, ZYDIS_OPERAND_ACTION_READ);
            ExpectReg("mov al, cl", &info, ZYDIS_REGISTER_ECX, ZYDIS_OPERAND_ACTION_READ);
            ExpectReg("mov al, cl", &info, ZYDIS_REGISTER_RCX, ZYDIS_OPERAND_ACTION_READ);
            ExpectAbsent("mov al, cl", &info, ZYDIS_REGISTER_CH);
        }
    }

    {
        static const ZyanU8 bytes[] = { 0x88, 0xE8 };
        if (Decode("mov al, ch", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectReg("mov al, ch", &info, ZYDIS_REGISTER_CH, ZYDIS_OPERAND_ACTION_READ);
            ExpectReg("mov al, ch", &info, ZYDIS_REGISTER_CX, ZYDIS_OPERAND_ACTION_READ);
            ExpectReg("mov al, ch", &info, ZYDIS_REGISTER_RCX, ZYDIS_OPERAND_ACTION_READ);
            ExpectAbsent("mov al, ch", &info, ZYDIS_REGISTER_CL);
        }
    }

    {
        static const ZyanU8 bytes[] = { 0x31, 0xC0 };
        if (Decode("xor eax, eax", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectReg("xor eax, eax", &info, ZYDIS_REGISTER_EAX, ZYDIS_OPERAND_ACTION_READWRITE);
            ExpectReg("xor eax, eax", &info, ZYDIS_REGISTER_RAX, ZYDIS_OPERAND_ACTION_WRITE);
        }
    }

    {
        static const ZyanU8 bytes[] = { 0x01, 0xC8 };
        if (Decode("add eax, ecx", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectReg("add eax, ecx", &info, ZYDIS_REGISTER_EAX, ZYDIS_OPERAND_ACTION_READWRITE);
            ExpectReg("add eax, ecx", &info, ZYDIS_REGISTER_RAX, ZYDIS_OPERAND_ACTION_WRITE);
            ExpectReg("add eax, ecx", &info, ZYDIS_REGISTER_AX, ZYDIS_OPERAND_ACTION_READWRITE);
            ExpectReg("add eax, ecx", &info, ZYDIS_REGISTER_AH, ZYDIS_OPERAND_ACTION_READWRITE);
            ExpectReg("add eax, ecx", &info, ZYDIS_REGISTER_ECX, ZYDIS_OPERAND_ACTION_READ);
            ExpectReg("add eax, ecx", &info, ZYDIS_REGISTER_RCX, ZYDIS_OPERAND_ACTION_READ);
            ExpectReg("add eax, ecx", &info, ZYDIS_REGISTER_CH, ZYDIS_OPERAND_ACTION_READ);
        }
    }

    {
        static const ZyanU8 bytes[] = { 0x48, 0x89, 0xC8 };
        if (Decode("mov rax, rcx", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectReg("mov rax, rcx", &info, ZYDIS_REGISTER_RAX, ZYDIS_OPERAND_ACTION_WRITE);
            ExpectReg("mov rax, rcx", &info, ZYDIS_REGISTER_EAX, ZYDIS_OPERAND_ACTION_WRITE);
            ExpectReg("mov rax, rcx", &info, ZYDIS_REGISTER_AH, ZYDIS_OPERAND_ACTION_WRITE);
            ExpectReg("mov rax, rcx", &info, ZYDIS_REGISTER_RCX, ZYDIS_OPERAND_ACTION_READ);
            ExpectReg("mov rax, rcx", &info, ZYDIS_REGISTER_CL, ZYDIS_OPERAND_ACTION_READ);
        }
    }

    {
        static const ZyanU8 bytes[] = { 0x66, 0x01, 0xC8 };
        if (DecodeMode("add ax, cx", ZYDIS_MACHINE_MODE_LEGACY_32, ZYDIS_STACK_WIDTH_32, bytes,
                sizeof(bytes), &instruction, operands, &info))
        {
            ExpectReg("add ax, cx", &info, ZYDIS_REGISTER_AX, ZYDIS_OPERAND_ACTION_READWRITE);
            ExpectReg("add ax, cx", &info, ZYDIS_REGISTER_EAX, ZYDIS_OPERAND_ACTION_READWRITE);
            ExpectReg("add ax, cx", &info, ZYDIS_REGISTER_AL, ZYDIS_OPERAND_ACTION_READWRITE);
            ExpectAbsent("add ax, cx", &info, ZYDIS_REGISTER_RAX);
        }
    }

    {
        static const ZyanU8 bytes[] = { 0x00, 0xC8 };
        if (DecodeMode("add al, cl 16", ZYDIS_MACHINE_MODE_REAL_16, ZYDIS_STACK_WIDTH_16, bytes,
                sizeof(bytes), &instruction, operands, &info))
        {
            ExpectReg("add al, cl 16", &info, ZYDIS_REGISTER_AL, ZYDIS_OPERAND_ACTION_READWRITE);
            ExpectReg("add al, cl 16", &info, ZYDIS_REGISTER_AX, ZYDIS_OPERAND_ACTION_READWRITE);
            ExpectAbsent("add al, cl 16", &info, ZYDIS_REGISTER_EAX);
            ExpectAbsent("add al, cl 16", &info, ZYDIS_REGISTER_RAX);
        }
    }

    if (g_failures)
    {
        printf("%d failure(s)\n", g_failures);
        return 1;
    }
    printf("SeraphInstructionInfo: ok\n");
    return 0;
}

/* ============================================================================================== */

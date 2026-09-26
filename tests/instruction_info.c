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
    ZydisInstructionInfo* info);

static int EncodeRegs(const char* label, ZydisMnemonic mnemonic, ZydisRegister dest,
    ZydisRegister src, ZydisDecodedInstruction* instruction, ZydisDecodedOperand* operands,
    ZydisInstructionInfo* info)
{
    ZydisEncoderRequest request;
    ZyanU8 bytes[ZYDIS_MAX_INSTRUCTION_LENGTH];
    ZyanUSize length = sizeof(bytes);

    memset(&request, 0, sizeof(request));
    request.machine_mode = ZYDIS_MACHINE_MODE_LONG_64;
    request.mnemonic = mnemonic;
    request.operand_count = 2;
    request.operands[0].type = ZYDIS_OPERAND_TYPE_REGISTER;
    request.operands[0].reg.value = dest;
    request.operands[1].type = ZYDIS_OPERAND_TYPE_REGISTER;
    request.operands[1].reg.value = src;
    if (ZYAN_FAILED(ZydisEncoderEncodeInstruction(&request, bytes, &length)))
    {
        Fail(label, "encode");
        return 0;
    }
    return Decode(label, bytes, length, instruction, operands, info);
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

static void ExpectTop(const char* label, const ZydisInstructionInfo* info, ZyanBool written)
{
    if (info->fpu_top_written != written)
    {
        printf("FAIL %s: fpu_top_written got %d want %d\n", label,
            (int)info->fpu_top_written, (int)written);
        ++g_failures;
    }
}

static void ExpectIntercept(const char* label, const ZydisInstructionInfo* info,
    ZydisInstructionIntercept intercept)
{
    if (info->intercept != intercept)
    {
        printf("FAIL %s: intercept got %d want %d\n", label,
            (int)info->intercept, (int)intercept);
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
            ExpectIntercept("cpuid", &info, ZYDIS_INSTRUCTION_INTERCEPT_NONE);
            ExpectTop("cpuid", &info, ZYAN_FALSE);
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
            ExpectTop("fld", &info, ZYAN_TRUE);
            ExpectReg("fld", &info, ZYDIS_REGISTER_ST0, ZYDIS_OPERAND_ACTION_READWRITE);
            ExpectReg("fld", &info, ZYDIS_REGISTER_ST1, ZYDIS_OPERAND_ACTION_READWRITE);
            ExpectReg("fld", &info, ZYDIS_REGISTER_ST6, ZYDIS_OPERAND_ACTION_READWRITE);
            ExpectReg("fld", &info, ZYDIS_REGISTER_ST7, ZYDIS_OPERAND_ACTION_WRITE);
            ExpectReg("fld", &info, ZYDIS_REGISTER_MM0, ZYDIS_OPERAND_ACTION_READWRITE);
            ExpectReg("fld", &info, ZYDIS_REGISTER_MM7, ZYDIS_OPERAND_ACTION_WRITE);
            if ((info.memory_count != 1) || (info.memory[0].action != ZYDIS_OPERAND_ACTION_READ))
            {
                Fail("fld", "memory");
            }
        }
    }

    {
        static const ZyanU8 bytes[] = { 0xDE, 0xD9 };
        if (Decode("fcompp", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectFpu("fcompp", &info, -2);
            ExpectReg("fcompp", &info, ZYDIS_REGISTER_ST0, ZYDIS_OPERAND_ACTION_READWRITE);
            ExpectReg("fcompp", &info, ZYDIS_REGISTER_ST7, ZYDIS_OPERAND_ACTION_READWRITE);
            ExpectReg("fcompp", &info, ZYDIS_REGISTER_MM1, ZYDIS_OPERAND_ACTION_READWRITE);
        }
    }

    {
        static const ZyanU8 bytes[] = { 0xD9, 0xF6 };
        if (Decode("fdecstp", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectFpu("fdecstp", &info, 1);
            ExpectReg("fdecstp", &info, ZYDIS_REGISTER_ST0, ZYDIS_OPERAND_ACTION_READWRITE);
            ExpectReg("fdecstp", &info, ZYDIS_REGISTER_ST7, ZYDIS_OPERAND_ACTION_READWRITE);
        }
    }

    {
        static const ZyanU8 bytes[] = { 0xDB, 0xE3 };
        if (Decode("fninit", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            if (info.fpu_delta_known)
            {
                Fail("fninit", "delta should be unknown");
            }
            ExpectTop("fninit", &info, ZYAN_TRUE);
            ExpectReg("fninit", &info, ZYDIS_REGISTER_ST0, ZYDIS_OPERAND_ACTION_WRITE);
            ExpectReg("fninit", &info, ZYDIS_REGISTER_ST7, ZYDIS_OPERAND_ACTION_WRITE);
            ExpectReg("fninit", &info, ZYDIS_REGISTER_MM0, ZYDIS_OPERAND_ACTION_WRITE);
        }
    }

    {
        static const ZyanU8 bytes[] = { 0xD9, 0xF2 };
        const ZydisOperandActions st0_action = (ZydisOperandActions)(
            ZYDIS_OPERAND_ACTION_READ | ZYDIS_OPERAND_ACTION_CONDREAD |
            ZYDIS_OPERAND_ACTION_CONDWRITE);

        if (Decode("fptan", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            if (info.fpu_delta_known)
            {
                Fail("fptan", "delta should be unknown");
            }
            ExpectTop("fptan", &info, ZYAN_TRUE);
            ExpectReg("fptan", &info, ZYDIS_REGISTER_ST0, st0_action);
            ExpectReg("fptan", &info, ZYDIS_REGISTER_ST1,
                ZYDIS_OPERAND_ACTION_CONDREAD_CONDWRITE);
            ExpectReg("fptan", &info, ZYDIS_REGISTER_ST6,
                ZYDIS_OPERAND_ACTION_CONDREAD_CONDWRITE);
            ExpectReg("fptan", &info, ZYDIS_REGISTER_ST7, ZYDIS_OPERAND_ACTION_CONDWRITE);
            ExpectReg("fptan", &info, ZYDIS_REGISTER_MM0, st0_action);
            ExpectReg("fptan", &info, ZYDIS_REGISTER_MM1,
                ZYDIS_OPERAND_ACTION_CONDREAD_CONDWRITE);
            ExpectReg("fptan", &info, ZYDIS_REGISTER_MM7, ZYDIS_OPERAND_ACTION_CONDWRITE);
        }
    }

    {
        static const ZyanU8 bytes[] = { 0xD9, 0xFB };
        const ZydisOperandActions st0_action = (ZydisOperandActions)(
            ZYDIS_OPERAND_ACTION_READ | ZYDIS_OPERAND_ACTION_CONDREAD |
            ZYDIS_OPERAND_ACTION_CONDWRITE);

        if (Decode("fsincos", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            if (info.fpu_delta_known)
            {
                Fail("fsincos", "delta should be unknown");
            }
            ExpectReg("fsincos", &info, ZYDIS_REGISTER_ST0, st0_action);
            ExpectReg("fsincos", &info, ZYDIS_REGISTER_ST1,
                ZYDIS_OPERAND_ACTION_CONDREAD_CONDWRITE);
            ExpectReg("fsincos", &info, ZYDIS_REGISTER_ST7, ZYDIS_OPERAND_ACTION_CONDWRITE);
            ExpectReg("fsincos", &info, ZYDIS_REGISTER_MM0, st0_action);
            ExpectReg("fsincos", &info, ZYDIS_REGISTER_MM7, ZYDIS_OPERAND_ACTION_CONDWRITE);
        }
    }

    {
        static const ZyanU8 bytes[] = { 0xD9, 0xFE };

        if (Decode("fsin", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            if (info.fpu_delta_known)
            {
                Fail("fsin", "delta should be unknown");
            }
            ExpectReg("fsin", &info, ZYDIS_REGISTER_ST0, ZYDIS_OPERAND_ACTION_READ_CONDWRITE);
            ExpectReg("fsin", &info, ZYDIS_REGISTER_MM0, ZYDIS_OPERAND_ACTION_READ_CONDWRITE);
            ExpectAbsent("fsin", &info, ZYDIS_REGISTER_ST1);
            ExpectAbsent("fsin", &info, ZYDIS_REGISTER_ST7);
            ExpectTop("fsin", &info, ZYAN_FALSE);
        }
    }

    {
        static const ZyanU8 bytes[] = { 0xD9, 0xFF };

        if (Decode("fcos", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            if (info.fpu_delta_known)
            {
                Fail("fcos", "delta should be unknown");
            }
            ExpectReg("fcos", &info, ZYDIS_REGISTER_ST0, ZYDIS_OPERAND_ACTION_READ_CONDWRITE);
            ExpectReg("fcos", &info, ZYDIS_REGISTER_MM0, ZYDIS_OPERAND_ACTION_READ_CONDWRITE);
            ExpectAbsent("fcos", &info, ZYDIS_REGISTER_ST1);
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
            ExpectIntercept("hlt", &info, ZYDIS_INSTRUCTION_INTERCEPT_NONE);
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
        static const ZyanU8 bytes[] = { 0x67, 0x8B, 0x00 };
        if (Decode("mov eax, [eax]", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectReg("mov eax, [eax]", &info, ZYDIS_REGISTER_EAX, ZYDIS_OPERAND_ACTION_READWRITE);
            ExpectReg("mov eax, [eax]", &info, ZYDIS_REGISTER_RAX,
                (ZydisOperandActions)(ZYDIS_OPERAND_ACTION_READ | ZYDIS_OPERAND_ACTION_WRITE));
        }
    }

    {
        static const ZyanU8 bytes[] = { 0x67, 0x0F, 0x44, 0x00 };
        if (Decode("cmovz eax, [eax]", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectReg("cmovz eax, [eax]", &info, ZYDIS_REGISTER_RAX,
                (ZydisOperandActions)(ZYDIS_OPERAND_ACTION_READ | ZYDIS_OPERAND_ACTION_WRITE |
                    ZYDIS_OPERAND_ACTION_CONDREAD));
        }
    }

    {
        static const ZyanU8 bytes[] = { 0x0F, 0xB0, 0xC8 };
        if (Decode("cmpxchg al, cl", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectReg("cmpxchg al, cl", &info, ZYDIS_REGISTER_AX,
                (ZydisOperandActions)(ZYDIS_OPERAND_ACTION_READ | ZYDIS_OPERAND_ACTION_CONDWRITE));
            ExpectReg("cmpxchg al, cl", &info, ZYDIS_REGISTER_RAX,
                (ZydisOperandActions)(ZYDIS_OPERAND_ACTION_READ | ZYDIS_OPERAND_ACTION_CONDWRITE));
            ExpectAbsent("cmpxchg al, cl", &info, ZYDIS_REGISTER_AH);
        }
    }

    {
        static const ZyanU8 bytes[] = { 0x0F, 0x44, 0xC1 };
        if (Decode("cmovz eax, ecx", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectReg("cmovz eax, ecx", &info, ZYDIS_REGISTER_RAX,
                (ZydisOperandActions)(ZYDIS_OPERAND_ACTION_WRITE | ZYDIS_OPERAND_ACTION_CONDREAD));
        }
    }

    if (EncodeRegs("movaps", ZYDIS_MNEMONIC_MOVAPS, ZYDIS_REGISTER_XMM0, ZYDIS_REGISTER_XMM1,
            &instruction, operands, &info))
    {
        ExpectReg("movaps", &info, ZYDIS_REGISTER_XMM0, ZYDIS_OPERAND_ACTION_WRITE);
        ExpectReg("movaps", &info, ZYDIS_REGISTER_YMM0, ZYDIS_OPERAND_ACTION_WRITE);
        ExpectReg("movaps", &info, ZYDIS_REGISTER_ZMM0, ZYDIS_OPERAND_ACTION_WRITE);
        ExpectReg("movaps", &info, ZYDIS_REGISTER_XMM1, ZYDIS_OPERAND_ACTION_READ);
        ExpectReg("movaps", &info, ZYDIS_REGISTER_YMM1, ZYDIS_OPERAND_ACTION_READ);
        ExpectReg("movaps", &info, ZYDIS_REGISTER_ZMM1, ZYDIS_OPERAND_ACTION_READ);
    }

    if (EncodeRegs("addps", ZYDIS_MNEMONIC_ADDPS, ZYDIS_REGISTER_XMM0, ZYDIS_REGISTER_XMM0,
            &instruction, operands, &info))
    {
        ExpectReg("addps", &info, ZYDIS_REGISTER_XMM0, ZYDIS_OPERAND_ACTION_READWRITE);
        ExpectReg("addps", &info, ZYDIS_REGISTER_YMM0, ZYDIS_OPERAND_ACTION_WRITE);
        ExpectReg("addps", &info, ZYDIS_REGISTER_ZMM0, ZYDIS_OPERAND_ACTION_WRITE);
    }

    if (EncodeRegs("movss", ZYDIS_MNEMONIC_MOVSS, ZYDIS_REGISTER_XMM0, ZYDIS_REGISTER_XMM1,
            &instruction, operands, &info))
    {
        ExpectReg("movss", &info, ZYDIS_REGISTER_XMM0, ZYDIS_OPERAND_ACTION_READWRITE);
        ExpectReg("movss", &info, ZYDIS_REGISTER_YMM0, ZYDIS_OPERAND_ACTION_READWRITE);
        ExpectReg("movss", &info, ZYDIS_REGISTER_ZMM0, ZYDIS_OPERAND_ACTION_READWRITE);
    }

    if (EncodeRegs("movq mm", ZYDIS_MNEMONIC_MOVQ, ZYDIS_REGISTER_MM0, ZYDIS_REGISTER_RAX,
            &instruction, operands, &info))
    {
        ExpectReg("movq mm", &info, ZYDIS_REGISTER_MM0, ZYDIS_OPERAND_ACTION_WRITE);
        ExpectReg("movq mm", &info, ZYDIS_REGISTER_ST0, ZYDIS_OPERAND_ACTION_WRITE);
    }

    {
        ZydisEncoderRequest request;
        ZyanU8 bytes[ZYDIS_MAX_INSTRUCTION_LENGTH];
        ZyanUSize length = sizeof(bytes);

        memset(&request, 0, sizeof(request));
        request.machine_mode = ZYDIS_MACHINE_MODE_LONG_64;
        request.mnemonic = ZYDIS_MNEMONIC_VADDPS;
        request.operand_count = 3;
        request.operands[0].type = ZYDIS_OPERAND_TYPE_REGISTER;
        request.operands[0].reg.value = ZYDIS_REGISTER_YMM0;
        request.operands[1].type = ZYDIS_OPERAND_TYPE_REGISTER;
        request.operands[1].reg.value = ZYDIS_REGISTER_YMM0;
        request.operands[2].type = ZYDIS_OPERAND_TYPE_REGISTER;
        request.operands[2].reg.value = ZYDIS_REGISTER_YMM1;
        if (ZYAN_FAILED(ZydisEncoderEncodeInstruction(&request, bytes, &length)) ||
            !Decode("vaddps", bytes, length, &instruction, operands, &info))
        {
            Fail("vaddps", "encode");
        }
        else
        {
            ExpectReg("vaddps", &info, ZYDIS_REGISTER_YMM0, ZYDIS_OPERAND_ACTION_READWRITE);
            ExpectReg("vaddps", &info, ZYDIS_REGISTER_XMM0, ZYDIS_OPERAND_ACTION_READWRITE);
            ExpectReg("vaddps", &info, ZYDIS_REGISTER_ZMM0, ZYDIS_OPERAND_ACTION_WRITE);
            ExpectReg("vaddps", &info, ZYDIS_REGISTER_YMM1, ZYDIS_OPERAND_ACTION_READ);
            ExpectReg("vaddps", &info, ZYDIS_REGISTER_ZMM1, ZYDIS_OPERAND_ACTION_READ);
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
            ExpectReg("add eax, ecx", &info, ZYDIS_REGISTER_RFLAGS, ZYDIS_OPERAND_ACTION_WRITE);
            ExpectReg("add eax, ecx", &info, ZYDIS_REGISTER_EFLAGS, ZYDIS_OPERAND_ACTION_WRITE);
            ExpectReg("add eax, ecx", &info, ZYDIS_REGISTER_FLAGS, ZYDIS_OPERAND_ACTION_WRITE);
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

    {
        static const ZyanU8 bytes[] = { 0x0F, 0x77 };
        if (Decode("emms", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            if (info.fpu_delta_known)
            {
                Fail("emms", "delta should be unknown");
            }
            ExpectTop("emms", &info, ZYAN_FALSE);
            ExpectReg("emms", &info, ZYDIS_REGISTER_ST0, ZYDIS_OPERAND_ACTION_WRITE);
        }
    }

    {
        static const ZyanU8 bytes[] = { 0xD9, 0x20 };
        if (Decode("fldenv", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            if (info.fpu_delta_known)
            {
                Fail("fldenv", "delta should be unknown");
            }
            ExpectTop("fldenv", &info, ZYAN_TRUE);
            ExpectAbsent("fldenv", &info, ZYDIS_REGISTER_ST0);
        }
    }

    {
        static const ZyanU8 bytes[] = { 0xDD, 0x30 };
        if (Decode("fnsave", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            if (info.fpu_delta_known)
            {
                Fail("fnsave", "delta should be unknown");
            }
            ExpectTop("fnsave", &info, ZYAN_TRUE);
            ExpectReg("fnsave", &info, ZYDIS_REGISTER_ST0, ZYDIS_OPERAND_ACTION_READWRITE);
        }
    }

    {
        static const ZyanU8 bytes[] = { 0x0F, 0xAE, 0x08 };
        if (Decode("fxrstor", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectTop("fxrstor", &info, ZYAN_TRUE);
            if (info.fpu_delta_known)
            {
                Fail("fxrstor", "delta should be unknown");
            }
        }
    }

    {
        static const ZyanU8 bytes[] = { 0xE4, 0x00 };
        if (Decode("in al, 0", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectFlow("in al, 0", &info, ZYDIS_INSTRUCTION_FLOW_PRIVILEGED);
            ExpectIntercept("in al, 0", &info, ZYDIS_INSTRUCTION_INTERCEPT_IO);
        }
    }

    {
        static const ZyanU8 bytes[] = { 0x0F, 0x32 };
        if (Decode("rdmsr", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectFlow("rdmsr", &info, ZYDIS_INSTRUCTION_FLOW_PRIVILEGED);
            ExpectIntercept("rdmsr", &info, ZYDIS_INSTRUCTION_INTERCEPT_MSR);
        }
    }

    {
        static const ZyanU8 bytes[] = { 0x0F, 0x01, 0x10 };
        if (Decode("lgdt", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectFlow("lgdt", &info, ZYDIS_INSTRUCTION_FLOW_PRIVILEGED);
            ExpectIntercept("lgdt", &info, ZYDIS_INSTRUCTION_INTERCEPT_DESCRIPTOR);
        }
    }

    {
        static const ZyanU8 bytes[] = { 0x0F, 0x78, 0xC0 };
        if (Decode("vmread", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectFlow("vmread", &info, ZYDIS_INSTRUCTION_FLOW_PRIVILEGED);
            ExpectIntercept("vmread", &info, ZYDIS_INSTRUCTION_INTERCEPT_VMX);
        }
    }

    {
        static const ZyanU8 bytes[] = { 0x0F, 0x01, 0xD8 };
        if (Decode("vmrun", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectFlow("vmrun", &info, ZYDIS_INSTRUCTION_FLOW_PRIVILEGED);
            ExpectIntercept("vmrun", &info, ZYDIS_INSTRUCTION_INTERCEPT_SVM);
        }
    }

    {
        static const ZyanU8 bytes[] = { 0x0F, 0x20, 0xC0 };
        if (Decode("mov rax, cr0", bytes, sizeof(bytes), &instruction, operands, &info))
        {
            ExpectFlow("mov rax, cr0", &info, ZYDIS_INSTRUCTION_FLOW_NEXT);
            ExpectIntercept("mov rax, cr0", &info, ZYDIS_INSTRUCTION_INTERCEPT_NONE);
        }
    }

    /* Implicit-operand count footgun: a short count must fail, not truncate. */
    {
        static const ZyanU8 bytes[] = { 0x48, 0xF7, 0xE1 }; /* mul rcx */
        ZydisDecoder decoder;
        if (ZYAN_FAILED(ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64,
                ZYDIS_STACK_WIDTH_64)) ||
            ZYAN_FAILED(ZydisDecoderDecodeFull(&decoder, bytes, sizeof(bytes), &instruction,
                operands)))
        {
            Fail("mul rcx", "decode");
        }
        else
        {
            if (instruction.operand_count_visible >= instruction.operand_count)
            {
                Fail("mul rcx", "expected implicit operands");
            }
            if (!ZYAN_FAILED(ZydisGetInstructionInfo(&instruction, operands,
                    instruction.operand_count_visible, &info)))
            {
                Fail("mul rcx", "short count accepted");
            }
            if (ZYAN_FAILED(ZydisGetInstructionInfoInsn(&instruction, operands, &info)))
            {
                Fail("mul rcx", "insn wrapper failed");
            }
            else
            {
                ExpectReg("mul rcx", &info, ZYDIS_REGISTER_RAX, ZYDIS_OPERAND_ACTION_READWRITE);
                ExpectReg("mul rcx", &info, ZYDIS_REGISTER_RDX, ZYDIS_OPERAND_ACTION_WRITE);
            }
        }
    }

    /* Flow and intercept enum-string accessors. */
    {
        const char* s = ZydisInstructionFlowGetString(ZYDIS_INSTRUCTION_FLOW_CALL);
        if (!s || strcmp(s, "call"))
        {
            Fail("flow string", "call");
        }
        s = ZydisInstructionFlowGetString(ZYDIS_INSTRUCTION_FLOW_PRIVILEGED);
        if (!s || strcmp(s, "privileged"))
        {
            Fail("flow string", "privileged");
        }
        if (ZydisInstructionFlowGetString(
                (ZydisInstructionFlow)(ZYDIS_INSTRUCTION_FLOW_MAX_VALUE + 1)))
        {
            Fail("flow string", "out-of-range accepted");
        }
        s = ZydisInstructionInterceptGetString(ZYDIS_INSTRUCTION_INTERCEPT_IO);
        if (!s || strcmp(s, "io"))
        {
            Fail("intercept string", "io");
        }
        s = ZydisInstructionInterceptGetString(ZYDIS_INSTRUCTION_INTERCEPT_SVM);
        if (!s || strcmp(s, "svm"))
        {
            Fail("intercept string", "svm");
        }
        if (ZydisInstructionInterceptGetString(
                (ZydisInstructionIntercept)(ZYDIS_INSTRUCTION_INTERCEPT_MAX_VALUE + 1)))
        {
            Fail("intercept string", "out-of-range accepted");
        }
    }

    /* Operand-free intercept classifier matches the full-info field. */
    {
        static const ZyanU8 rdmsr_b[] = { 0x0F, 0x32 };
        static const ZyanU8 lgdt_b[]  = { 0x0F, 0x01, 0x10 };
        static const ZyanU8 vmrun_b[] = { 0x0F, 0x01, 0xD8 };
        static const ZyanU8 cpuid_b[] = { 0x0F, 0xA2 };
        struct { const char* label; const ZyanU8* b; ZyanUSize n;
            ZydisInstructionIntercept want; } t[4] =
        {
            { "rdmsr", rdmsr_b, sizeof(rdmsr_b), ZYDIS_INSTRUCTION_INTERCEPT_MSR },
            { "lgdt",  lgdt_b,  sizeof(lgdt_b),  ZYDIS_INSTRUCTION_INTERCEPT_DESCRIPTOR },
            { "vmrun", vmrun_b, sizeof(vmrun_b), ZYDIS_INSTRUCTION_INTERCEPT_SVM },
            { "cpuid", cpuid_b, sizeof(cpuid_b), ZYDIS_INSTRUCTION_INTERCEPT_NONE }
        };
        int k;

        for (k = 0; k < 4; ++k)
        {
            if (Decode(t[k].label, t[k].b, t[k].n, &instruction, operands, &info))
            {
                if ((ZydisGetInterceptClass(&instruction) != t[k].want) ||
                    (ZydisGetInterceptClass(&instruction) != info.intercept))
                {
                    Fail(t[k].label, "intercept class");
                }
            }
        }
        if (ZydisGetInterceptClass(ZYAN_NULL) != ZYDIS_INSTRUCTION_INTERCEPT_NONE)
        {
            Fail("intercept class", "null");
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

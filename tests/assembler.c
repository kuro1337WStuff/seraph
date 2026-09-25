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

#include <stdio.h>
#include <string.h>

#include <Zydis/Zydis.h>

static int g_failures = 0;

int main(void)
{
    ZydisAsm assembler;
    ZyanU8 out[64];
    ZyanUSize written = 0;
    ZydisDecoder decoder;
    ZydisDecodedInstruction instruction;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
    ZyanUSize offset = 0;
    ZydisMnemonic expect[] = {
        ZYDIS_MNEMONIC_MOV,
        ZYDIS_MNEMONIC_MOV,
        ZYDIS_MNEMONIC_MOV,
        ZYDIS_MNEMONIC_ADD,
        ZYDIS_MNEMONIC_JMP,
        ZYDIS_MNEMONIC_RET
    };
    int index = 0;

    ZydisAsmInit(&assembler, ZYDIS_MACHINE_MODE_LONG_64);
    ZydisAsmMovRegReg(&assembler, ZYDIS_REGISTER_RAX, ZYDIS_REGISTER_RCX);
    ZydisAsmMovRegImm(&assembler, ZYDIS_REGISTER_RAX, 0x1337);
    ZydisAsmMovRegMem(&assembler, ZYDIS_REGISTER_RAX, ZYDIS_REGISTER_RBX, ZYDIS_REGISTER_RCX, 8,
        0x10);
    ZydisAsmLock(&assembler);
    ZydisAsmAddMemReg(&assembler, ZYDIS_REGISTER_RAX, ZYDIS_REGISTER_RCX);
    ZydisAsmJmpLabel(&assembler, 1);
    ZydisAsmLabel(&assembler, 1);
    ZydisAsmRet(&assembler);
    if (ZYAN_FAILED(ZydisAsmEncode(&assembler, 0x1000, out, sizeof(out), &written)))
    {
        printf("FAIL encode\n");
        return 1;
    }

    ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
    while (offset < written)
    {
        if (ZYAN_FAILED(ZydisDecoderDecodeFull(&decoder, out + offset, written - offset,
                &instruction, operands)))
        {
            printf("FAIL decode at %u\n", (unsigned)offset);
            return 1;
        }
        if ((index >= 6) || (instruction.mnemonic != expect[index]))
        {
            printf("FAIL mnemonic %d at %d\n", (int)instruction.mnemonic, index);
            ++g_failures;
        }
        if (index == 2)
        {
            if ((operands[1].mem.base != ZYDIS_REGISTER_RBX) ||
                (operands[1].mem.index != ZYDIS_REGISTER_RCX) ||
                (operands[1].mem.scale != 8) ||
                (operands[1].mem.disp.value != 0x10))
            {
                printf("FAIL mem operand\n");
                ++g_failures;
            }
        }
        if ((index == 3) && !(instruction.attributes & ZYDIS_ATTRIB_HAS_LOCK))
        {
            printf("FAIL lock\n");
            ++g_failures;
        }
        offset += instruction.length;
        ++index;
    }
    if (index != 6)
    {
        printf("FAIL count %d\n", index);
        ++g_failures;
    }
    ZydisAsmInit(&assembler, ZYDIS_MACHINE_MODE_LONG_64);
    {
        ZydisAsmOp cmp_ops[2];
        ZydisAsmOp lea_ops[2];
        ZyanU64 target = 0;

        memset(cmp_ops, 0, sizeof(cmp_ops));
        cmp_ops[0].kind = ZYDIS_OPERAND_TYPE_REGISTER;
        cmp_ops[0].reg = ZYDIS_REGISTER_RAX;
        cmp_ops[1].kind = ZYDIS_OPERAND_TYPE_REGISTER;
        cmp_ops[1].reg = ZYDIS_REGISTER_RCX;
        memset(lea_ops, 0, sizeof(lea_ops));
        lea_ops[0].kind = ZYDIS_OPERAND_TYPE_REGISTER;
        lea_ops[0].reg = ZYDIS_REGISTER_RAX;
        lea_ops[1].kind = ZYDIS_OPERAND_TYPE_MEMORY;
        lea_ops[1].base = ZYDIS_REGISTER_RBX;
        lea_ops[1].index = ZYDIS_REGISTER_RCX;
        lea_ops[1].scale = 4;
        lea_ops[1].disp = 8;
        if (ZYAN_FAILED(ZydisAsmInsn(&assembler, ZYDIS_MNEMONIC_CMP, cmp_ops, 2)) ||
            ZYAN_FAILED(ZydisAsmInsn(&assembler, ZYDIS_MNEMONIC_LEA, lea_ops, 2)) ||
            ZYAN_FAILED(ZydisAsmBranch(&assembler, ZYDIS_MNEMONIC_JZ, 7)) ||
            ZYAN_FAILED(ZydisAsmInsn(&assembler, ZYDIS_MNEMONIC_NOP, ZYAN_NULL, 0)) ||
            ZYAN_FAILED(ZydisAsmLabel(&assembler, 7)) ||
            ZYAN_FAILED(ZydisAsmBranch(&assembler, ZYDIS_MNEMONIC_CALL, 8)) ||
            ZYAN_FAILED(ZydisAsmLabel(&assembler, 8)) ||
            ZYAN_FAILED(ZydisAsmRet(&assembler)) ||
            ZYAN_FAILED(ZydisAsmEncode(&assembler, 0x2000, out, sizeof(out), &written)))
        {
            printf("FAIL general encode\n");
            return 1;
        }
        offset = 0;
        index = 0;
        while (offset < written)
        {
            ZyanU64 ip = 0x2000 + offset;

            if (ZYAN_FAILED(ZydisDecoderDecodeFull(&decoder, out + offset, written - offset,
                    &instruction, operands)))
            {
                printf("FAIL general decode\n");
                return 1;
            }
            if (index == 0 && instruction.mnemonic != ZYDIS_MNEMONIC_CMP)
            {
                printf("FAIL cmp\n");
                ++g_failures;
            }
            if (index == 1)
            {
                if ((instruction.mnemonic != ZYDIS_MNEMONIC_LEA) ||
                    (operands[1].mem.base != ZYDIS_REGISTER_RBX) ||
                    (operands[1].mem.index != ZYDIS_REGISTER_RCX) ||
                    (operands[1].mem.scale != 4) ||
                    (operands[1].mem.disp.value != 8))
                {
                    printf("FAIL lea\n");
                    ++g_failures;
                }
            }
            if (index == 2)
            {
                if ((instruction.mnemonic != ZYDIS_MNEMONIC_JZ) ||
                    ZYAN_FAILED(ZydisCalcAbsoluteAddress(&instruction, &operands[0], ip, &target)) ||
                    (target != 0x2000 + offset + instruction.length + 1))
                {
                    printf("FAIL jz target 0x%llx\n", (unsigned long long)target);
                    ++g_failures;
                }
            }
            if ((index == 4) && (instruction.mnemonic != ZYDIS_MNEMONIC_CALL))
            {
                printf("FAIL call\n");
                ++g_failures;
            }
            offset += instruction.length;
            ++index;
        }
        if (index != 6)
        {
            printf("FAIL general count %d\n", index);
            ++g_failures;
        }
    }
    if (g_failures)
    {
        printf("%d failure(s)\n", g_failures);
        return 1;
    }
    printf("SeraphAssembler: ok\n");
    return 0;
}

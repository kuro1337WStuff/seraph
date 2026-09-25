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

static void Jmp(ZydisBlockSlot* slot, ZyanU32 label)
{
    memset(slot, 0, sizeof(*slot));
    slot->kind = ZYDIS_BLOCK_SLOT_INSTR;
    slot->branch_label = label;
    slot->branch_operand = 0;
    slot->request.machine_mode = ZYDIS_MACHINE_MODE_LONG_64;
    slot->request.mnemonic = ZYDIS_MNEMONIC_JMP;
    slot->request.operand_count = 1;
    slot->request.operands[0].type = ZYDIS_OPERAND_TYPE_IMMEDIATE;
}

static void Ret(ZydisBlockSlot* slot, ZyanU32 label)
{
    memset(slot, 0, sizeof(*slot));
    slot->kind = ZYDIS_BLOCK_SLOT_INSTR;
    slot->label_id = label;
    slot->request.machine_mode = ZYDIS_MACHINE_MODE_LONG_64;
    slot->request.mnemonic = ZYDIS_MNEMONIC_RET;
}

int main(void)
{
    ZyanU8 out[256];
    ZyanUSize written = 0;
    ZydisBlockSlot slots[5];
    ZyanU8 gap[130];
    ZyanU8 data = 0x11;

    memset(gap, 0, sizeof(gap));
    memset(slots, 0, sizeof(slots));
    Jmp(&slots[0], 1);
    Ret(&slots[1], 1);
    if (ZYAN_FAILED(ZydisBlockEncode(ZYDIS_MACHINE_MODE_LONG_64, 0x1000, slots, 2, out,
            sizeof(out), &written, 0)) ||
        (written != 3) || (out[0] != 0xEB) || (out[1] != 0x00) || (out[2] != 0xC3))
    {
        printf("FAIL short jmp %u bytes %02X %02X %02X\n", (unsigned)written, out[0], out[1],
            out[2]);
        ++g_failures;
    }

    memset(slots, 0, sizeof(slots));
    Jmp(&slots[0], 1);
    slots[1].kind = ZYDIS_BLOCK_SLOT_BYTES;
    slots[1].bytes = gap;
    slots[1].byte_count = (ZyanU8)sizeof(gap);
    Ret(&slots[2], 1);
    if (!ZYAN_FAILED(ZydisBlockEncode(ZYDIS_MACHINE_MODE_LONG_64, 0x1000, slots, 3, out,
            sizeof(out), &written, ZYDIS_BLOCK_ENCODER_DONT_FIX_BRANCHES)))
    {
        printf("FAIL dont-fix should fail\n");
        ++g_failures;
    }
    if (ZYAN_FAILED(ZydisBlockEncode(ZYDIS_MACHINE_MODE_LONG_64, 0x1000, slots, 3, out,
            sizeof(out), &written, 0)) ||
        (out[0] != 0xE9))
    {
        printf("FAIL widen got %02X\n", out[0]);
        ++g_failures;
    }

    memset(slots, 0, sizeof(slots));
    slots[0].kind = ZYDIS_BLOCK_SLOT_INSTR;
    slots[0].branch_label = 1;
    slots[0].branch_operand = 1;
    slots[0].request.machine_mode = ZYDIS_MACHINE_MODE_LONG_64;
    slots[0].request.mnemonic = ZYDIS_MNEMONIC_MOV;
    slots[0].request.operand_count = 2;
    slots[0].request.operands[0].type = ZYDIS_OPERAND_TYPE_REGISTER;
    slots[0].request.operands[0].reg.value = ZYDIS_REGISTER_RAX;
    slots[0].request.operands[1].type = ZYDIS_OPERAND_TYPE_MEMORY;
    slots[0].request.operands[1].mem.base = ZYDIS_REGISTER_RIP;
    slots[0].request.operands[1].mem.size = 8;
    Jmp(&slots[1], 2);
    slots[2].kind = ZYDIS_BLOCK_SLOT_BYTES;
    slots[2].bytes = gap;
    slots[2].byte_count = (ZyanU8)sizeof(gap);
    Ret(&slots[3], 2);
    slots[4].kind = ZYDIS_BLOCK_SLOT_BYTES;
    slots[4].label_id = 1;
    slots[4].bytes = &data;
    slots[4].byte_count = 1;
    if (ZYAN_FAILED(ZydisBlockEncode(ZYDIS_MACHINE_MODE_LONG_64, 0x1000, slots, 5, out,
            sizeof(out), &written, 0)))
    {
        printf("FAIL rip encode\n");
        ++g_failures;
    }
    else
    {
        ZydisDecoder decoder;
        ZydisDecodedInstruction instruction;
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];

        ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
        if (ZYAN_FAILED(ZydisDecoderDecodeFull(&decoder, out, written, &instruction, operands)) ||
            (operands[1].mem.disp.value != 136))
        {
            printf("FAIL rip disp %lld\n", (long long)operands[1].mem.disp.value);
            ++g_failures;
        }
    }

    if (g_failures)
    {
        printf("%d failure(s)\n", g_failures);
        return 1;
    }
    printf("SeraphBlockEncoder: ok\n");
    return 0;
}

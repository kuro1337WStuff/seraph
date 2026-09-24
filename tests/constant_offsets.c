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
 * Checks ZydisGetConstantOffsets against instructions whose displacement and
 * immediate bytes are known, then patches those bytes and decodes again.
 */

#include <stdio.h>
#include <string.h>

#include <Zydis/Zydis.h>

/* ============================================================================================== */
/* Helpers                                                                                        */
/* ============================================================================================== */

static int g_failures = 0;

static void ExpectU8(const char* label, const char* field, ZyanU8 got, ZyanU8 want)
{
    if (got != want)
    {
        printf("FAIL %s %s: got %u want %u\n", label, field, (unsigned)got, (unsigned)want);
        ++g_failures;
    }
}

static void ExpectU64(const char* label, const char* field, ZyanU64 got, ZyanU64 want)
{
    if (got != want)
    {
        printf("FAIL %s %s: got 0x%llx want 0x%llx\n", label, field,
            (unsigned long long)got, (unsigned long long)want);
        ++g_failures;
    }
}

static ZyanBool Decode(const char* label, ZyanU8* bytes, ZyanUSize length,
    ZydisDecodedInstruction* instruction, ZydisDecodedOperand* operands)
{
    ZydisDecoder decoder;

    if (ZYAN_FAILED(ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64)) ||
        ZYAN_FAILED(ZydisDecoderDecodeFull(&decoder, bytes, length, instruction, operands)) ||
        (instruction->length != length))
    {
        printf("FAIL %s: decode\n", label);
        ++g_failures;
        return ZYAN_FALSE;
    }
    return ZYAN_TRUE;
}

static void CheckOffsets(const char* label, const ZyanU8* bytes, ZyanUSize length,
    ZyanU8 displacement_offset, ZyanU8 displacement_size, ZyanU8 immediate_offset,
    ZyanU8 immediate_size, ZyanU8 immediate_offset2, ZyanU8 immediate_size2)
{
    ZydisDecodedInstruction instruction;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
    ZydisConstantOffsets offsets;
    ZyanU8 mutable_bytes[ZYDIS_MAX_INSTRUCTION_LENGTH];

    memcpy(mutable_bytes, bytes, length);
    if (!Decode(label, mutable_bytes, length, &instruction, operands))
    {
        return;
    }
    if (ZYAN_FAILED(ZydisGetConstantOffsets(&instruction, &offsets)))
    {
        printf("FAIL %s: ZydisGetConstantOffsets\n", label);
        ++g_failures;
        return;
    }

    ExpectU8(label, "displacement_offset", offsets.displacement_offset, displacement_offset);
    ExpectU8(label, "displacement_size", offsets.displacement_size, displacement_size);
    ExpectU8(label, "immediate_offset", offsets.immediate_offset, immediate_offset);
    ExpectU8(label, "immediate_size", offsets.immediate_size, immediate_size);
    ExpectU8(label, "immediate_offset2", offsets.immediate_offset2, immediate_offset2);
    ExpectU8(label, "immediate_size2", offsets.immediate_size2, immediate_size2);
}

/* ============================================================================================== */
/* Cases                                                                                          */
/* ============================================================================================== */

int main(void)
{
    ZydisDecodedInstruction instruction;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
    ZydisConstantOffsets offsets;

    /* mov rax, 0x1337 */
    static const ZyanU8 mov_imm[] = { 0x48, 0xC7, 0xC0, 0x37, 0x13, 0x00, 0x00 };
    /* mov rax, qword ptr [rip+0x12345678] */
    static const ZyanU8 mov_rip[] = { 0x48, 0x8B, 0x05, 0x78, 0x56, 0x34, 0x12 };
    /* enter 0x1234, 0x56 */
    static const ZyanU8 enter_bytes[] = { 0xC8, 0x34, 0x12, 0x56 };
    /* shl al, 1 — the 1 is implicit and is not in the byte stream */
    static const ZyanU8 shl_al[] = { 0xD0, 0xE0 };

    if (!ZYAN_FAILED(ZydisGetConstantOffsets(ZYAN_NULL, &offsets)) ||
        !ZYAN_FAILED(ZydisGetConstantOffsets(&instruction, ZYAN_NULL)))
    {
        printf("FAIL null arguments were accepted\n");
        ++g_failures;
    }

    CheckOffsets("mov rax, 0x1337", mov_imm, sizeof(mov_imm), 0, 0, 3, 4, 0, 0);
    CheckOffsets("mov rax, [rip+disp]", mov_rip, sizeof(mov_rip), 3, 4, 0, 0, 0, 0);
    CheckOffsets("enter 0x1234, 0x56", enter_bytes, sizeof(enter_bytes), 0, 0, 1, 2, 3, 1);
    CheckOffsets("shl al, 1", shl_al, sizeof(shl_al), 0, 0, 0, 0, 0, 0);

    {
        ZyanU8 bytes[] = { 0x48, 0xC7, 0xC0, 0x37, 0x13, 0x00, 0x00 };
        if (Decode("patch imm", bytes, sizeof(bytes), &instruction, operands) &&
            ZYAN_SUCCESS(ZydisGetConstantOffsets(&instruction, &offsets)) &&
            (offsets.immediate_size == 4))
        {
            bytes[offsets.immediate_offset + 0] = 0x39;
            bytes[offsets.immediate_offset + 1] = 0x05;
            bytes[offsets.immediate_offset + 2] = 0x00;
            bytes[offsets.immediate_offset + 3] = 0x00;
            if (Decode("patched imm", bytes, sizeof(bytes), &instruction, operands))
            {
                ExpectU64("patched imm", "value", operands[1].imm.value.u, 0x539);
            }
        }
        else
        {
            printf("FAIL patch imm setup\n");
            ++g_failures;
        }
    }

    {
        ZyanU8 bytes[] = { 0x48, 0x8B, 0x05, 0x78, 0x56, 0x34, 0x12 };
        if (Decode("patch disp", bytes, sizeof(bytes), &instruction, operands) &&
            ZYAN_SUCCESS(ZydisGetConstantOffsets(&instruction, &offsets)) &&
            (offsets.displacement_size == 4))
        {
            bytes[offsets.displacement_offset + 0] = 0x10;
            bytes[offsets.displacement_offset + 1] = 0x00;
            bytes[offsets.displacement_offset + 2] = 0x00;
            bytes[offsets.displacement_offset + 3] = 0x00;
            if (Decode("patched disp", bytes, sizeof(bytes), &instruction, operands))
            {
                ExpectU64("patched disp", "value", (ZyanU64)operands[1].mem.disp.value, 0x10);
            }
        }
        else
        {
            printf("FAIL patch disp setup\n");
            ++g_failures;
        }
    }

    if (g_failures)
    {
        printf("%d failure(s)\n", g_failures);
        return 1;
    }
    printf("SeraphConstantOffsets: ok\n");
    return 0;
}

/* ============================================================================================== */

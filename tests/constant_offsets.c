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

static void Fail(const char* label, const char* what)
{
    printf("FAIL %s: %s\n", label, what);
    ++g_failures;
}

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

    memset(&instruction, 0, sizeof(instruction));

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

    /* Guest linear address: ZydisCalcAbsoluteAddressSeg folds in the segment base. */
    {
        ZyanU8 fs_base_disp[] = { 0x64, 0x8B, 0x40, 0x10 };            /* mov eax, fs:[rax+0x10] */
        ZyanU8 fs_disp_only[] = { 0x64, 0x8B, 0x04, 0x25, 0x10, 0x00, 0x00, 0x00 }; /* fs:[0x10] */
        ZydisRegisterContext ctx;
        ZyanU64 addr;
        ZyanU8 i;
        int mem;

        memset(&ctx, 0, sizeof(ctx));
        ctx.values[ZYDIS_REGISTER_RAX] = 0x1000;

        if (Decode("fs:[rax+0x10]", fs_base_disp, sizeof(fs_base_disp), &instruction, operands))
        {
            mem = -1;
            for (i = 0; i < instruction.operand_count; ++i)
            {
                if (operands[i].type == ZYDIS_OPERAND_TYPE_MEMORY)
                {
                    mem = (int)i;
                    break;
                }
            }
            if (mem < 0)
            {
                Fail("fs:[rax+0x10]", "no memory operand");
            }
            else
            {
                /* Effective address only (no segment base). */
                if (ZYAN_FAILED(ZydisCalcAbsoluteAddressEx(&instruction, &operands[mem], 0, &ctx,
                        &addr)))
                {
                    Fail("fs:[rax+0x10]", "ex status");
                }
                ExpectU64("fs:[rax+0x10]", "effective", addr, 0x1010);

                /* segment_base 0 reproduces the effective address. */
                if (ZYAN_FAILED(ZydisCalcAbsoluteAddressSeg(&instruction, &operands[mem], 0, &ctx,
                        0, &addr)))
                {
                    Fail("fs:[rax+0x10]", "seg flat status");
                }
                ExpectU64("fs:[rax+0x10]", "seg flat", addr, 0x1010);

                /* A real FS base is folded into the linear address. */
                if (ZYAN_FAILED(ZydisCalcAbsoluteAddressSeg(&instruction, &operands[mem], 0, &ctx,
                        0x00007FFE00000000ULL, &addr)))
                {
                    Fail("fs:[rax+0x10]", "seg base status");
                }
                ExpectU64("fs:[rax+0x10]", "seg base", addr, 0x00007FFE00001010ULL);
            }
        }

        if (Decode("fs:[0x10]", fs_disp_only, sizeof(fs_disp_only), &instruction, operands))
        {
            mem = -1;
            for (i = 0; i < instruction.operand_count; ++i)
            {
                if (operands[i].type == ZYDIS_OPERAND_TYPE_MEMORY)
                {
                    mem = (int)i;
                    break;
                }
            }
            if (mem >= 0)
            {
                if (ZYAN_FAILED(ZydisCalcAbsoluteAddressSeg(&instruction, &operands[mem], 0, &ctx,
                        0x1000, &addr)))
                {
                    Fail("fs:[0x10]", "seg status");
                }
                ExpectU64("fs:[0x10]", "seg base", addr, 0x1010);
            }
        }

        /* A null result pointer is rejected. */
        if (ZydisCalcAbsoluteAddressSeg(&instruction, &operands[0], 0, &ctx, 0, ZYAN_NULL) !=
                ZYAN_STATUS_INVALID_ARGUMENT)
        {
            Fail("seg null", "not rejected");
        }
    }

    /* Signature mask: wildcard only address-bearing bytes; keep constants and struct offsets. */
    {
        struct { const char* label; ZyanU8 bytes[16]; ZyanU8 len; const char* sig; } cases[] =
        {
            { "call rel32",      { 0xE8, 0x11, 0x22, 0x33, 0x44 }, 5, "E8 ?? ?? ?? ??" },
            { "mov rax,[rip+d]", { 0x48, 0x8B, 0x05, 0x78, 0x56, 0x34, 0x12 }, 7,
                "48 8B 05 ?? ?? ?? ??" },
            { "add rax,8",       { 0x48, 0x83, 0xC0, 0x08 }, 4, "48 83 C0 08" },
            { "add [rax+d],5",   { 0x83, 0x80, 0x78, 0x56, 0x34, 0x12, 0x05 }, 7,
                "83 80 78 56 34 12 05" },
            { "mov rax,[abs64]", { 0x48, 0xA1, 0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70 }, 10,
                "48 A1 ?? ?? ?? ?? ?? ?? ?? ??" },
            { "jz rel8",         { 0x74, 0x10 }, 2, "74 ??" }
        };
        char sig[64];
        ZyanU8 buf[16];
        int c;

        for (c = 0; c < (int)(sizeof(cases) / sizeof(cases[0])); ++c)
        {
            memcpy(buf, cases[c].bytes, cases[c].len);
            if (Decode(cases[c].label, buf, cases[c].len, &instruction, operands))
            {
                if (ZYAN_FAILED(ZydisFormatSignature(&instruction, operands,
                        instruction.operand_count, buf, sig, sizeof(sig))) ||
                    strcmp(sig, cases[c].sig))
                {
                    printf("FAIL %s: got '%s' want '%s'\n", cases[c].label, sig, cases[c].sig);
                    ++g_failures;
                }
            }
        }

        /* Direct mask, small-buffer, and null checks. */
        {
            ZyanU8 addc[] = { 0x48, 0x83, 0xC0, 0x08 }; /* add rax, 8 : nothing wildcarded */
            ZyanU8 mask[16];

            if (Decode("sig mask", addc, sizeof(addc), &instruction, operands))
            {
                if (ZYAN_FAILED(ZydisGetSignatureMask(&instruction, operands,
                        instruction.operand_count, mask, sizeof(mask))) ||
                    mask[0] || mask[1] || mask[2] || mask[3])
                {
                    Fail("sig mask", "add rax,8 wildcarded");
                }
                if (ZydisGetSignatureMask(&instruction, operands, instruction.operand_count,
                        mask, 1) != ZYAN_STATUS_INSUFFICIENT_BUFFER_SIZE)
                {
                    Fail("sig mask", "small buffer");
                }
                if (ZydisFormatSignature(&instruction, operands, instruction.operand_count,
                        addc, sig, 3) != ZYAN_STATUS_INSUFFICIENT_BUFFER_SIZE)
                {
                    Fail("sig fmt", "small buffer");
                }
                if (ZydisGetSignatureMask(ZYAN_NULL, operands, 0, mask, sizeof(mask)) !=
                        ZYAN_STATUS_INVALID_ARGUMENT)
                {
                    Fail("sig mask", "null");
                }
            }
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

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
 * Checks ZydisGetCpuidFlags against ISA sets Zydis already stores.
 * Leaf numbers are not part of the result.
 */

#include <stdio.h>
#include <string.h>

#include <Zydis/Zydis.h>

static int g_failures = 0;

static void Fail(const char* label, const char* what)
{
    printf("FAIL %s: %s\n", label, what);
    ++g_failures;
}

static int HasFlag(const ZydisCpuidFlag* flags, ZyanU8 count, ZydisCpuidFlag flag)
{
    ZyanU8 i;

    for (i = 0; i < count; ++i)
    {
        if (flags[i] == flag)
        {
            return 1;
        }
    }
    return 0;
}

static int DecodeFlags(const char* label, const ZyanU8* bytes, ZyanUSize length,
    ZydisCpuidFlag* flags, ZyanU8 capacity, ZyanU8* count)
{
    ZydisDecoder decoder;
    ZydisDecodedInstruction instruction;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];

    if (ZYAN_FAILED(ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64)) ||
        ZYAN_FAILED(ZydisDecoderDecodeFull(&decoder, bytes, length, &instruction, operands)) ||
        (instruction.length != length) ||
        ZYAN_FAILED(ZydisGetCpuidFlags(&instruction, flags, capacity, count)))
    {
        Fail(label, "decode");
        return 0;
    }
    return 1;
}

int main(void)
{
    ZydisCpuidFlag flags[ZYDIS_CPUID_FLAG_MAX_COUNT];
    ZyanU8 count = 0;
    ZydisDecodedInstruction blank;

    memset(&blank, 0, sizeof(blank));
    if (!ZYAN_FAILED(ZydisGetCpuidFlags(ZYAN_NULL, flags, 2, &count)) ||
        !ZYAN_FAILED(ZydisGetCpuidFlags(&blank, flags, 2, ZYAN_NULL)))
    {
        Fail("null", "arguments");
    }

    {
        static const ZyanU8 bytes[] = { 0xC5, 0xF8, 0x28, 0xC1 };
        if (DecodeFlags("vmovaps vex", bytes, sizeof(bytes), flags, 2, &count))
        {
            if ((count != 1) || (flags[0] != ZYDIS_CPUID_FLAG_AVX))
            {
                Fail("vmovaps vex", "flags");
            }
        }
    }

    {
        static const ZyanU8 bytes[] = { 0x66, 0x0F, 0x38, 0xDC, 0xC1 };
        if (DecodeFlags("aesenc", bytes, sizeof(bytes), flags, 2, &count))
        {
            if ((count != 1) || (flags[0] != ZYDIS_CPUID_FLAG_AES))
            {
                Fail("aesenc", "flags");
            }
        }
    }

    {
        static const ZyanU8 bytes[] = { 0x0F, 0x01, 0xC1 };
        if (DecodeFlags("vmcall", bytes, sizeof(bytes), flags, 2, &count))
        {
            if ((count != 1) || (flags[0] != ZYDIS_CPUID_FLAG_VTX))
            {
                Fail("vmcall", "flags");
            }
        }
    }

    {
        static const ZyanU8 bytes[] = { 0x48, 0x89, 0xC8 };
        ZydisDecoder decoder;
        ZydisDecodedInstruction instruction;
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
        ZydisCpuidFlag sentinel = ZYDIS_CPUID_FLAG_AVX;

        ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
        if (ZYAN_FAILED(ZydisDecoderDecodeFull(&decoder, bytes, sizeof(bytes), &instruction,
                operands)) ||
            !ZYAN_FAILED(ZydisGetCpuidFlags(&instruction, ZYAN_NULL, 0, &count)) ||
            (count == 0))
        {
            Fail("mov", "count");
        }
        else if (count > 1)
        {
            if (!ZYAN_FAILED(ZydisGetCpuidFlags(&instruction, &sentinel, 1, &count)) ||
                (sentinel != ZYDIS_CPUID_FLAG_AVX))
            {
                Fail("mov", "small buffer");
            }
        }
    }

    {
        static const ZyanU8 bytes[] = { 0x62, 0xF1, 0x7C, 0x08, 0x58, 0xC1 };
        if (DecodeFlags("vaddps xmm", bytes, sizeof(bytes), flags, 2, &count))
        {
            ZydisCpuidFlag sentinel = ZYDIS_CPUID_FLAG_AVX;
            ZydisDecoder decoder;
            ZydisDecodedInstruction instruction;
            ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];

            if ((count != 2) ||
                !HasFlag(flags, count, ZYDIS_CPUID_FLAG_AVX512F) ||
                !HasFlag(flags, count, ZYDIS_CPUID_FLAG_AVX512VL))
            {
                Fail("vaddps xmm", "flags");
            }
            ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
            if (ZYAN_FAILED(ZydisDecoderDecodeFull(&decoder, bytes, sizeof(bytes), &instruction,
                    operands)) ||
                !ZYAN_FAILED(ZydisGetCpuidFlags(&instruction, &sentinel, 1, &count)) ||
                (count != 2) ||
                (sentinel != ZYDIS_CPUID_FLAG_AVX))
            {
                Fail("vaddps xmm", "small buffer");
            }
        }
    }

    {
        static const ZyanU8 bytes[] = { 0x62, 0xF1, 0x7C, 0x48, 0x58, 0xC1 };
        if (DecodeFlags("vaddps zmm", bytes, sizeof(bytes), flags, 2, &count))
        {
            if ((count != 1) || (flags[0] != ZYDIS_CPUID_FLAG_AVX512F) ||
                HasFlag(flags, count, ZYDIS_CPUID_FLAG_AVX512VL))
            {
                Fail("vaddps zmm", "flags");
            }
        }
    }

    if (g_failures)
    {
        printf("%d failure(s)\n", g_failures);
        return 1;
    }
    printf("SeraphCpuidFlags: ok\n");
    return 0;
}

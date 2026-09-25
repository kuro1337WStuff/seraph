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
 * Checks NASM text and the MASM far-return and x87 stack spellings.
 */

#include <stdio.h>
#include <string.h>

#include <Zydis/Zydis.h>

/* ============================================================================================== */
/* Helpers                                                                                        */
/* ============================================================================================== */

static int g_failures = 0;

static void ExpectText(const char* label, const char* got, const char* want)
{
    if (strcmp(got, want) != 0)
    {
        printf("FAIL %s: got \"%s\" want \"%s\"\n", label, got, want);
        ++g_failures;
    }
}

static int DecodeFormat(const char* label, ZydisFormatterStyle style, const ZyanU8* bytes,
    ZyanUSize length, char* buffer, ZyanUSize buffer_length, ZydisDecodedInstruction* instruction,
    ZydisDecodedOperand* operands)
{
    ZydisDecoder decoder;
    ZydisFormatter formatter;

    if (ZYAN_FAILED(ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64)) ||
        ZYAN_FAILED(ZydisDecoderDecodeFull(&decoder, bytes, length, instruction, operands)) ||
        (instruction->length != length) ||
        ZYAN_FAILED(ZydisFormatterInit(&formatter, style)) ||
        ZYAN_FAILED(ZydisFormatterFormatInstruction(&formatter, instruction, operands,
            instruction->operand_count_visible, buffer, buffer_length,
            ZYDIS_RUNTIME_ADDRESS_NONE, ZYAN_NULL)))
    {
        printf("FAIL %s: format\n", label);
        ++g_failures;
        return 0;
    }
    return 1;
}

static void ExpectMemoryOperand(const ZydisFormatter* formatter,
    const ZydisDecodedInstruction* instruction, const ZydisDecodedOperand* operands)
{
    ZyanU8 i;
    char buffer[128];
    const ZydisDecodedOperand* mem = ZYAN_NULL;

    for (i = 0; i < instruction->operand_count_visible; ++i)
    {
        if (operands[i].type == ZYDIS_OPERAND_TYPE_MEMORY)
        {
            mem = &operands[i];
            break;
        }
    }
    if (!mem)
    {
        printf("FAIL mem: no memory operand\n");
        ++g_failures;
        return;
    }
    if (ZYAN_FAILED(ZydisFormatterFormatOperand(formatter, instruction, mem, buffer, sizeof(buffer),
        ZYDIS_RUNTIME_ADDRESS_NONE, ZYAN_NULL)))
    {
        printf("FAIL mem: format operand\n");
        ++g_failures;
        return;
    }
    if (!strstr(buffer, "[rax]") || strstr(buffer, "ptr"))
    {
        printf("FAIL mem: got \"%s\"\n", buffer);
        ++g_failures;
    }
}

/* ============================================================================================== */
/* Entry point                                                                                    */
/* ============================================================================================== */

int main(void)
{
    char text[256];
    ZydisDecodedInstruction instruction;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];

    {
        static const ZyanU8 bytes[] = { 0x90 };

        if (DecodeFormat("nop", ZYDIS_FORMATTER_STYLE_NASM, bytes, sizeof(bytes), text,
            sizeof(text), &instruction, operands))
        {
            ExpectText("nop", text, "nop");
        }
    }

    {
        static const ZyanU8 bytes[] = { 0x48, 0xC7, 0xC0, 0x37, 0x13, 0x00, 0x00 };

        if (DecodeFormat("mov imm", ZYDIS_FORMATTER_STYLE_NASM, bytes, sizeof(bytes), text,
            sizeof(text), &instruction, operands))
        {
            ExpectText("mov imm", text, "mov rax, 0x1337");
        }
    }

    {
        static const ZyanU8 bytes[] = { 0x48, 0x8B, 0x00 };
        ZydisFormatter formatter;

        if (DecodeFormat("mov mem", ZYDIS_FORMATTER_STYLE_NASM, bytes, sizeof(bytes), text,
            sizeof(text), &instruction, operands))
        {
            if (!strstr(text, "[rax]") || strstr(text, "ptr"))
            {
                printf("FAIL mov mem: got \"%s\"\n", text);
                ++g_failures;
            }
            if (ZYAN_FAILED(ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_NASM)))
            {
                printf("FAIL mov mem: formatter\n");
                ++g_failures;
            }
            else
            {
                ExpectMemoryOperand(&formatter, &instruction, operands);
            }
        }
    }

    {
        static const ZyanU8 bytes[] = { 0xCB };

        if (DecodeFormat("nasm retf", ZYDIS_FORMATTER_STYLE_NASM, bytes, sizeof(bytes), text,
            sizeof(text), &instruction, operands))
        {
            ExpectText("nasm retf", text, "retf");
        }
        if (DecodeFormat("masm retf", ZYDIS_FORMATTER_STYLE_INTEL_MASM, bytes, sizeof(bytes), text,
            sizeof(text), &instruction, operands))
        {
            ExpectText("masm retf", text, "retf");
        }
        if (DecodeFormat("intel ret", ZYDIS_FORMATTER_STYLE_INTEL, bytes, sizeof(bytes), text,
            sizeof(text), &instruction, operands))
        {
            ExpectText("intel ret", text, "ret far");
        }
    }

    {
        static const ZyanU8 bytes[] = { 0xC7, 0x00, 0x37, 0x13, 0x00, 0x00 };

        if (DecodeFormat("dword mem", ZYDIS_FORMATTER_STYLE_NASM, bytes, sizeof(bytes), text,
            sizeof(text), &instruction, operands))
        {
            ExpectText("dword mem", text, "mov dword [rax], 0x1337");
        }
    }

    {
        static const ZyanU8 bytes[] = { 0xD9, 0xC1 };

        if (DecodeFormat("nasm st1", ZYDIS_FORMATTER_STYLE_NASM, bytes, sizeof(bytes), text,
            sizeof(text), &instruction, operands))
        {
            if (!strstr(text, "st1") || strstr(text, "st("))
            {
                printf("FAIL nasm st1: got \"%s\"\n", text);
                ++g_failures;
            }
        }
        if (DecodeFormat("masm st1", ZYDIS_FORMATTER_STYLE_INTEL_MASM, bytes, sizeof(bytes), text,
            sizeof(text), &instruction, operands))
        {
            if (!strstr(text, "st(1)"))
            {
                printf("FAIL masm st1: got \"%s\"\n", text);
                ++g_failures;
            }
        }
    }

    if (g_failures)
    {
        printf("%d failure(s)\n", g_failures);
        return 1;
    }
    printf("SeraphNasmFormatter: ok\n");
    return 0;
}

/* ============================================================================================== */

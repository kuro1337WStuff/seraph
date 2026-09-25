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
 * Checks ZydisGetEncodingInfo against the opcode, ModRM, and ISA fields already
 * stored on a decoded instruction.
 */

#include <stdio.h>
#include <string.h>

#include <Zydis/Zydis.h>
#include <Zydis/OpcodeInfo.h>

/* ============================================================================================== */
/* Helpers                                                                                        */
/* ============================================================================================== */

static int g_failures = 0;

static void Fail(const char* label, const char* what)
{
    printf("FAIL %s: %s\n", label, what);
    ++g_failures;
}

static int Decode(const char* label, const ZyanU8* bytes, ZyanUSize length,
    ZydisDecodedInstruction* instruction, ZydisEncodingInfo* info)
{
    ZydisDecoder decoder;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];

    if (ZYAN_FAILED(ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64)) ||
        ZYAN_FAILED(ZydisDecoderDecodeFull(&decoder, bytes, length, instruction, operands)) ||
        (instruction->length != length) ||
        ZYAN_FAILED(ZydisGetEncodingInfo(instruction, info)))
    {
        Fail(label, "decode");
        return 0;
    }
    return 1;
}

/* ============================================================================================== */
/* Cases                                                                                          */
/* ============================================================================================== */

int main(void)
{
    ZydisDecodedInstruction instruction;
    ZydisEncodingInfo info;

    memset(&instruction, 0, sizeof(instruction));
    memset(&info, 0, sizeof(info));

    if (!ZYAN_FAILED(ZydisGetEncodingInfo(ZYAN_NULL, &info)) ||
        !ZYAN_FAILED(ZydisGetEncodingInfo(&instruction, ZYAN_NULL)))
    {
        Fail("null", "accepted");
    }

    {
        static const ZyanU8 bytes[] = { 0x48, 0xC7, 0xC0, 0x37, 0x13, 0x00, 0x00 };
        if (Decode("mov rax, 0x1337", bytes, sizeof(bytes), &instruction, &info))
        {
            if (info.encoding != ZYDIS_INSTRUCTION_ENCODING_LEGACY)
            {
                Fail("mov rax, 0x1337", "encoding");
            }
            if (info.opcode_map != ZYDIS_OPCODE_MAP_DEFAULT)
            {
                Fail("mov rax, 0x1337", "opcode_map");
            }
            if (info.opcode != 0xC7)
            {
                Fail("mov rax, 0x1337", "opcode");
            }
            if (!info.has_modrm)
            {
                Fail("mov rax, 0x1337", "has_modrm");
            }
            if (info.modrm != 0xC0)
            {
                Fail("mov rax, 0x1337", "modrm");
            }
            if ((info.isa_set == ZYDIS_ISA_SET_INVALID) &&
                (info.isa_ext == ZYDIS_ISA_EXT_INVALID))
            {
                Fail("mov rax, 0x1337", "isa");
            }
        }
    }

    {
        static const ZyanU8 bytes[] = { 0x90 };
        if (Decode("nop", bytes, sizeof(bytes), &instruction, &info))
        {
            if (info.opcode != 0x90)
            {
                Fail("nop", "opcode");
            }
            if (info.has_modrm)
            {
                Fail("nop", "has_modrm");
            }
            if (info.modrm != 0)
            {
                Fail("nop", "modrm");
            }
        }
    }

    if (g_failures)
    {
        printf("%d failure(s)\n", g_failures);
        return 1;
    }
    printf("SeraphOpcodeInfo: ok\n");
    return 0;
}

/* ============================================================================================== */

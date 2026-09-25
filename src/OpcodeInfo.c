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
#include <Zydis/OpcodeInfo.h>

/* ============================================================================================== */
/* Exported functions                                                                             */
/* ============================================================================================== */

ZyanStatus ZydisGetEncodingInfo(const ZydisDecodedInstruction* instruction,
    ZydisEncodingInfo* info)
{
    if (!instruction || !info)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }

    ZYAN_MEMSET(info, 0, sizeof(*info));
    info->encoding = instruction->encoding;
    info->opcode_map = instruction->opcode_map;
    info->opcode = instruction->opcode;
    info->has_modrm = (instruction->attributes & ZYDIS_ATTRIB_HAS_MODRM) ? ZYAN_TRUE : ZYAN_FALSE;
    if (info->has_modrm)
    {
        info->modrm = (ZyanU8)((instruction->raw.modrm.mod << 6) |
            (instruction->raw.modrm.reg << 3) | instruction->raw.modrm.rm);
    }
    info->isa_set = instruction->meta.isa_set;
    info->isa_ext = instruction->meta.isa_ext;
    return ZYAN_STATUS_SUCCESS;
}

/* ============================================================================================== */

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
 * Encoding, opcode, ModRM, and ISA fields from a decoded instruction.
 */

#ifndef ZYDIS_OPCODE_INFO_H
#define ZYDIS_OPCODE_INFO_H

#include <Zycore/Types.h>
#include <Zydis/DecoderTypes.h>
#include <Zydis/Status.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================================== */
/* Enums and types                                                                                */
/* ============================================================================================== */

/**
 * Encoding fields copied from a decoded instruction.
 *
 * `modrm` is the encoded ModRM byte, rebuilt from `raw.modrm.mod`, `raw.modrm.reg`, and
 * `raw.modrm.rm`. It is zero when `has_modrm` is false.
 */
typedef struct ZydisEncodingInfo_
{
    /**
     * The instruction-encoding (`LEGACY`, `3DNOW`, `VEX`, `EVEX`, `XOP`, `MVEX`, `REX2`).
     */
    ZydisInstructionEncoding encoding;
    /**
     * The opcode-map.
     */
    ZydisOpcodeMap opcode_map;
    /**
     * The instruction-opcode.
     */
    ZyanU8 opcode;
    /**
     * Signals, if the instruction has a ModRM byte.
     */
    ZyanBool has_modrm;
    /**
     * The ModRM byte, or zero when `has_modrm` is false.
     */
    ZyanU8 modrm;
    /**
     * The ISA-set.
     */
    ZydisISASet isa_set;
    /**
     * The ISA-set extension.
     */
    ZydisISAExt isa_ext;
} ZydisEncodingInfo;

/* ============================================================================================== */
/* Exported functions                                                                             */
/* ============================================================================================== */

/**
 * Copies encoding, opcode, ModRM, and ISA fields from a decoded instruction.
 *
 * @param   instruction A pointer to the `ZydisDecodedInstruction` struct.
 * @param   info        A pointer to the `ZydisEncodingInfo` struct that receives the result.
 *
 * @return  A zyan status code.
 */
ZYDIS_EXPORT ZyanStatus ZydisGetEncodingInfo(const ZydisDecodedInstruction* instruction,
    ZydisEncodingInfo* info);

/* ============================================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* ZYDIS_OPCODE_INFO_H */

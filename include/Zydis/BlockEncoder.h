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
 * Encode a block of instructions and raw bytes, widening short branches when needed.
 */

#ifndef ZYDIS_BLOCK_ENCODER_H
#define ZYDIS_BLOCK_ENCODER_H

#include <Zycore/Defines.h>
#include <Zycore/Types.h>
#include <Zydis/Encoder.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================================== */
/* Constants                                                                                      */
/* ============================================================================================== */

#define ZYDIS_BLOCK_MAX_SLOTS 32

#define ZYDIS_BLOCK_SLOT_INSTR 0
#define ZYDIS_BLOCK_SLOT_BYTES 1

/**
 * Leave a short branch unchanged when its target is out of range.
 */
#define ZYDIS_BLOCK_ENCODER_DONT_FIX_BRANCHES 0x00000001u

/* ============================================================================================== */
/* Enums and types                                                                                */
/* ============================================================================================== */

/**
 * One instruction or one raw byte run inside a block.
 *
 * `label_id` 0 means this slot defines no label. `branch_label` 0 means the request already
 * carries its own operands. A non-zero `branch_label` is resolved to an absolute address at
 * `base_ip + offset` and written into `operands[branch_operand]` before encoding. An immediate
 * operand is a branch and may be widened from rel8 to rel32. A memory operand is RIP-relative.
 */
typedef struct ZydisBlockSlot_
{
    ZyanU8 kind;
    ZyanU32 label_id;
    ZyanU32 branch_label;
    ZyanU8 branch_operand;
    ZydisEncoderRequest request;
    const ZyanU8* bytes;
    ZyanU8 byte_count;
} ZydisBlockSlot;

/* ============================================================================================== */
/* Exported functions                                                                             */
/* ============================================================================================== */

/**
 * Encodes `slot_count` slots at `base_ip` into a caller buffer.
 *
 * @return  `ZYAN_STATUS_FAILED` when a short branch cannot reach and
 *          `ZYDIS_BLOCK_ENCODER_DONT_FIX_BRANCHES` is set.
 */
ZYDIS_EXPORT ZyanStatus ZydisBlockEncode(ZydisMachineMode mode, ZyanU64 base_ip,
    const ZydisBlockSlot* slots, ZyanU8 slot_count, ZyanU8* out, ZyanUSize out_cap,
    ZyanUSize* written, ZyanU32 options);

/* ============================================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* ZYDIS_BLOCK_ENCODER_H */

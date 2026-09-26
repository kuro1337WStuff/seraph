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
 * Other utility functions.
 */

#ifndef ZYDIS_UTILS_H
#define ZYDIS_UTILS_H

#include <Zycore/Defines.h>
#include <Zydis/DecoderTypes.h>
#include <Zydis/Status.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================================== */
/* Exported functions                                                                             */
/* ============================================================================================== */

/**
 * @addtogroup utils Utils
 * Miscellaneous utility functions. Address translation and other helpers.
 * @{
 */

/* ---------------------------------------------------------------------------------------------- */
/* Address calculation                                                                            */
/* ---------------------------------------------------------------------------------------------- */

// TODO: Provide a function that works in minimal-mode and does not require a operand parameter

/**
 * Calculates the absolute address value for the given instruction operand.
 *
 * @param   instruction     A pointer to the `ZydisDecodedInstruction` struct.
 * @param   operand         A pointer to the `ZydisDecodedOperand` struct.
 * @param   runtime_address The runtime address of the instruction.
 * @param   result_address  A pointer to the memory that receives the absolute address.
 *
 * @return  A zyan status code.
 *
 * You should use this function in the following cases:
 * - `IMM` operands with relative address (e.g. `JMP`, `CALL`, ...)
 * - `MEM` operands with `RIP`/`EIP`-relative address (e.g. `MOV RAX, [RIP+0x12345678]`)
 * - `MEM` operands with absolute address (e.g. `MOV RAX, [0x12345678]`)
 *   - The displacement needs to get truncated and zero extended
 */
ZYDIS_EXPORT ZyanStatus ZydisCalcAbsoluteAddress(const ZydisDecodedInstruction* instruction,
    const ZydisDecodedOperand* operand, ZyanU64 runtime_address, ZyanU64* result_address);

/**
 * Calculates the absolute address value for the given instruction operand.
 *
 * @param   instruction         A pointer to the `ZydisDecodedInstruction` struct.
 * @param   operand             A pointer to the `ZydisDecodedOperand` struct.
 * @param   runtime_address     The runtime address of the instruction.
 * @param   register_context    A pointer to the `ZydisRegisterContext` struct.
 * @param   result_address      A pointer to the memory that receives the absolute target-address.
 *
 * @return  A zyan status code.
 *
 * This function behaves like `ZydisCalcAbsoluteAddress` but takes an additional register-context
 * argument to allow calculation of addresses depending on runtime register values.
 *
 * Note that `IP/EIP/RIP` from the register-context will be ignored in favor of the passed
 * runtime-address.
 */
ZYDIS_EXPORT ZyanStatus ZydisCalcAbsoluteAddressEx(const ZydisDecodedInstruction* instruction,
    const ZydisDecodedOperand* operand, ZyanU64 runtime_address,
    const ZydisRegisterContext* register_context, ZyanU64* result_address);

/**
 * Calculates the guest linear address for a memory operand, including the segment base.
 *
 * `ZydisCalcAbsoluteAddressEx` returns the effective address (base + index*scale +
 * displacement) inside the segment and never adds a segment base, so it is a flat address.
 * A hypervisor or emulator resolving a non-flat access (`fs:`/`gs:`, real-mode, or V8086)
 * needs the effective address plus the base of `operand->mem.segment`. This wrapper computes
 * the effective address with `ZydisCalcAbsoluteAddressEx` and adds `segment_base`, which the
 * caller looks up for that segment from its own guest state.
 *
 * @param   instruction         A pointer to the `ZydisDecodedInstruction` struct.
 * @param   operand             A pointer to the `ZydisDecodedOperand` struct.
 * @param   runtime_address     The runtime address of the instruction.
 * @param   register_context    A pointer to the `ZydisRegisterContext` struct.
 * @param   segment_base        The base address of `operand->mem.segment`; pass 0 for a flat
 *                              segment to reproduce `ZydisCalcAbsoluteAddressEx`.
 * @param   result_address      A pointer to the memory that receives the linear target-address.
 *
 * @return  A zyan status code.
 */
ZYDIS_EXPORT ZyanStatus ZydisCalcAbsoluteAddressSeg(const ZydisDecodedInstruction* instruction,
    const ZydisDecodedOperand* operand, ZyanU64 runtime_address,
    const ZydisRegisterContext* register_context, ZyanU64 segment_base, ZyanU64* result_address);

/* ---------------------------------------------------------------------------------------------- */
/* Signature mask                                                                                 */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Builds a byte-match mask for turning an instruction into a search signature.
 *
 * Writes `instruction->length` entries into `mask`: 0 for a byte that stays part of the pattern
 * and 1 for a byte to wildcard. Only address-bearing bytes are wildcarded — a relative branch
 * target, a RIP/EIP-relative displacement, an absolute `[disp]` address, and an address
 * immediate — so that a genuine constant (`add rax, 8`) and a struct/array offset
 * (`mov eax, [rbx+8]`) stay in the pattern and a signature survives a rebuild that only moves
 * code and data around. The displacement policy needs the operands to tell a struct offset from
 * an absolute address; when `operand_count` is 0 the displacement is kept.
 *
 * @param   instruction     Decoded instruction.
 * @param   operands        Operand array from the decoder (used for the displacement policy).
 * @param   operand_count   Number of valid entries in `operands`.
 * @param   mask            Receives one byte per instruction byte (0 = keep, 1 = wildcard).
 * @param   mask_capacity   Number of entries `mask` can hold; must be at least `instruction->length`.
 *
 * @return  A zyan status code.
 */
ZYDIS_EXPORT ZyanStatus ZydisGetSignatureMask(const ZydisDecodedInstruction* instruction,
    const ZydisDecodedOperand* operands, ZyanU8 operand_count, ZyanU8* mask,
    ZyanUSize mask_capacity);

/**
 * Formats an instruction as an IDA-style signature string, e.g. `E8 ?? ?? ?? ??`.
 *
 * Applies `ZydisGetSignatureMask` and writes space-separated upper-case hex bytes, with `??`
 * for each wildcarded byte, into the caller buffer. No libc is used.
 *
 * @param   instruction     Decoded instruction.
 * @param   operands        Operand array from the decoder.
 * @param   operand_count   Number of valid entries in `operands`.
 * @param   bytes           The instruction's own bytes (`instruction->length` of them).
 * @param   buffer          Receives the null-terminated signature text.
 * @param   capacity        Size of `buffer` in bytes.
 *
 * @return  A zyan status code; `ZYAN_STATUS_INSUFFICIENT_BUFFER_SIZE` when `buffer` is too small.
 */
ZYDIS_EXPORT ZyanStatus ZydisFormatSignature(const ZydisDecodedInstruction* instruction,
    const ZydisDecodedOperand* operands, ZyanU8 operand_count, const ZyanU8* bytes,
    char* buffer, ZyanUSize capacity);

/* ---------------------------------------------------------------------------------------------- */
/* Constant offsets                                                                               */
/* ---------------------------------------------------------------------------------------------- */

/**
 * Byte offsets of the displacement and immediates inside an instruction.
 *
 * A size of zero means that field is not present in the instruction bytes. Offsets are
 * relative to the first byte of the instruction. Implicit immediates that are not encoded
 * in the byte stream (for example `shl al, 1`) report a size of zero.
 *
 * Sizes are in bytes. `ZydisDecodedInstructionRaw` stores the same locations with sizes
 * in bits.
 */
typedef struct ZydisConstantOffsets_
{
    /**
     * Offset of the displacement, in bytes.
     */
    ZyanU8 displacement_offset;
    /**
     * Size of the displacement, in bytes, or zero when the instruction has none.
     */
    ZyanU8 displacement_size;
    /**
     * Offset of the first immediate, in bytes.
     */
    ZyanU8 immediate_offset;
    /**
     * Size of the first immediate, in bytes, or zero when the instruction has none.
     */
    ZyanU8 immediate_size;
    /**
     * Offset of the second immediate, in bytes.
     */
    ZyanU8 immediate_offset2;
    /**
     * Size of the second immediate, in bytes, or zero when the instruction has none.
     */
    ZyanU8 immediate_size2;
} ZydisConstantOffsets;

/**
 * Reports the displacement and immediate locations inside a decoded instruction.
 *
 * @param   instruction A pointer to the `ZydisDecodedInstruction` struct.
 * @param   offsets     A pointer to the `ZydisConstantOffsets` struct that receives the result.
 *
 * @return  A zyan status code.
 */
ZYDIS_EXPORT ZyanStatus ZydisGetConstantOffsets(const ZydisDecodedInstruction* instruction,
    ZydisConstantOffsets* offsets);

/* ---------------------------------------------------------------------------------------------- */

/**
 * @}
 */

/* ============================================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* ZYDIS_UTILS_H */
